
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <malloc.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>

#include "libavutil/mem.h"
#include "fmt-conversion.h"

#include "afilter/format.h"
#include "libxplayer.h"

#include "libavutil/log.h"

#define USE_CACHE 1

#define RING_BUFFER_SIZE        8192

#define MASTER_BUFFER           0
#define VIDEO_BUFFER            1
#define AUDIO_BUFFER            2

static unsigned char inited = 0;

typedef struct {
    char*               filename;

    int                 w;
    int                 h;
    int                 imgw;
    int                 imgh;
    unsigned int        fmt;
    unsigned int        pixfmt;
    double              fps;
    unsigned long int   waittime;

    int                 origw;
    int                 origh;
    unsigned int        origpixfmt;

    double              pts;
    double              rpts;
    double              vpts;
    double              apts;
    double              rapts;
    double              rvpts;

    mp_image_t*         img;
    int                 newimg;

    char*               audio_buf;
    int                 audio_buf_size;
    int                 audio_buf_len;
    char*               buf;
    int                 buf_size;

    AVFormatContext*    pFormatCtx;
    int                 videoStream;
    int                 audioStream;
    AVCodecContext*     vCodecCtx;
    AVCodec*            vCodec;
    AVCodecContext*     aCodecCtx;
    AVCodec*            aCodec;
    AVFrame*            vFrame;
    AVFrame*            aFrame;
    AVPicture           pict;
    int                 frameFinished;
    struct SwsContext*  img_convert_ctx;

    int                 buffered_packet;
    AVPacket            packets[RING_BUFFER_SIZE];
    int                 packets_read;
    int                 packets_write;

    AVPacket            vpackets[RING_BUFFER_SIZE];
    int                 vpackets_read;
    int                 vpackets_write;

    AVPacket            apackets[RING_BUFFER_SIZE];
    int                 apackets_read;
    int                 apackets_write;
} xplayer_t;


void *memalign(size_t boundary, size_t size) {
    return malloc(size);
}

void * fast_memcpy(void * to, const void * from, size_t len);
void * mem2agpcpy(void * to, const void * from, size_t len);

#if ! defined(CONFIG_FASTMEMCPY) && ! (HAVE_MMX || HAVE_MMX2 || HAVE_AMD3DNOW /* || HAVE_SSE || HAVE_SSE2 */)
#define mem2agpcpy(a,b,c) memcpy(a,b,c)
#define fast_memcpy(a,b,c) memcpy(a,b,c)
#endif

static inline void * mem2agpcpy_pic(void * dst, const void * src, int bytesPerLine, int height, int dstStride, int srcStride)
{
	int i;
	void *retval=dst;

	if(dstStride == srcStride)
	{
		if (srcStride < 0) {
	    		src = (uint8_t*)src + (height-1)*srcStride;
	    		dst = (uint8_t*)dst + (height-1)*dstStride;
	    		srcStride = -srcStride;
		}

		mem2agpcpy(dst, src, srcStride*height);
	}
	else
	{
		for(i=0; i<height; i++)
		{
			mem2agpcpy(dst, src, bytesPerLine);
			src = (uint8_t*)src + srcStride;
			dst = (uint8_t*)dst + dstStride;
		}
	}

	return retval;
}

#define memcpy_pic(d, s, b, h, ds, ss) memcpy_pic2(d, s, b, h, ds, ss, 0)
#define my_memcpy_pic(d, s, b, h, ds, ss) memcpy_pic2(d, s, b, h, ds, ss, 1)

/**
 * \param limit2width always skip data between end of line and start of next
 *                    instead of copying the full block when strides are the same
 */
static inline void * memcpy_pic2(void * dst, const void * src,
                                 int bytesPerLine, int height,
                                 int dstStride, int srcStride, int limit2width)
{
	int i;
	void *retval=dst;

	if(!limit2width && dstStride == srcStride)
	{
		if (srcStride < 0) {
	    		src = (uint8_t*)src + (height-1)*srcStride;
	    		dst = (uint8_t*)dst + (height-1)*dstStride;
	    		srcStride = -srcStride;
		}

		fast_memcpy(dst, src, srcStride*height);
	}
	else
	{
		for(i=0; i<height; i++)
		{
			fast_memcpy(dst, src, bytesPerLine);
			src = (uint8_t*)src + srcStride;
			dst = (uint8_t*)dst + dstStride;
		}
	}

	return retval;
}

const char *vo_format_name(int format)
{
    static char unknown_format[20];
    switch(format)
    {
	case IMGFMT_RGB1: return "RGB 1-bit";
	case IMGFMT_RGB4: return "RGB 4-bit";
	case IMGFMT_RG4B: return "RGB 4-bit per byte";
	case IMGFMT_RGB8: return "RGB 8-bit";
	case IMGFMT_RGB15: return "RGB 15-bit";
	case IMGFMT_RGB16: return "RGB 16-bit";
	case IMGFMT_RGB24: return "RGB 24-bit";
//	case IMGFMT_RGB32: return "RGB 32-bit";
	case IMGFMT_RGB48LE: return "RGB 48-bit LE";
	case IMGFMT_RGB48BE: return "RGB 48-bit BE";
	case IMGFMT_BGR1: return "BGR 1-bit";
	case IMGFMT_BGR4: return "BGR 4-bit";
	case IMGFMT_BG4B: return "BGR 4-bit per byte";
	case IMGFMT_BGR8: return "BGR 8-bit";
	case IMGFMT_BGR15: return "BGR 15-bit";
	case IMGFMT_BGR16: return "BGR 16-bit";
	case IMGFMT_BGR24: return "BGR 24-bit";
//	case IMGFMT_BGR32: return "BGR 32-bit";
	case IMGFMT_ABGR: return "ABGR";
	case IMGFMT_BGRA: return "BGRA";
	case IMGFMT_ARGB: return "ARGB";
	case IMGFMT_RGBA: return "RGBA";
	case IMGFMT_YVU9: return "Planar YVU9";
	case IMGFMT_IF09: return "Planar IF09";
	case IMGFMT_YV12: return "Planar YV12";
	case IMGFMT_I420: return "Planar I420";
	case IMGFMT_IYUV: return "Planar IYUV";
	case IMGFMT_CLPL: return "Planar CLPL";
	case IMGFMT_Y800: return "Planar Y800";
	case IMGFMT_Y8: return "Planar Y8";
	case IMGFMT_Y16: return "Planar Y16";
	case IMGFMT_420P16_LE: return "Planar 420P 16-bit little-endian";
	case IMGFMT_420P16_BE: return "Planar 420P 16-bit big-endian";
	case IMGFMT_422P16_LE: return "Planar 422P 16-bit little-endian";
	case IMGFMT_422P16_BE: return "Planar 422P 16-bit big-endian";
	case IMGFMT_444P16_LE: return "Planar 444P 16-bit little-endian";
	case IMGFMT_444P16_BE: return "Planar 444P 16-bit big-endian";
	case IMGFMT_420A: return "Planar 420P with alpha";
	case IMGFMT_444P: return "Planar 444P";
	case IMGFMT_422P: return "Planar 422P";
	case IMGFMT_411P: return "Planar 411P";
	case IMGFMT_NV12: return "Planar NV12";
	case IMGFMT_NV21: return "Planar NV21";
        case IMGFMT_HM12: return "Planar NV12 Macroblock";
	case IMGFMT_IUYV: return "Packed IUYV";
	case IMGFMT_IY41: return "Packed IY41";
	case IMGFMT_IYU1: return "Packed IYU1";
	case IMGFMT_IYU2: return "Packed IYU2";
	case IMGFMT_UYVY: return "Packed UYVY";
	case IMGFMT_UYNV: return "Packed UYNV";
	case IMGFMT_cyuv: return "Packed CYUV";
	case IMGFMT_Y422: return "Packed Y422";
	case IMGFMT_YUY2: return "Packed YUY2";
	case IMGFMT_YUNV: return "Packed YUNV";
	case IMGFMT_YVYU: return "Packed YVYU";
	case IMGFMT_Y41P: return "Packed Y41P";
	case IMGFMT_Y211: return "Packed Y211";
	case IMGFMT_Y41T: return "Packed Y41T";
	case IMGFMT_Y42T: return "Packed Y42T";
	case IMGFMT_V422: return "Packed V422";
	case IMGFMT_V655: return "Packed V655";
	case IMGFMT_CLJR: return "Packed CLJR";
	case IMGFMT_YUVP: return "Packed YUVP";
	case IMGFMT_UYVP: return "Packed UYVP";
	case IMGFMT_MPEGPES: return "Mpeg PES";
	case IMGFMT_ZRMJPEGNI: return "Zoran MJPEG non-interlaced";
	case IMGFMT_ZRMJPEGIT: return "Zoran MJPEG top field first";
	case IMGFMT_ZRMJPEGIB: return "Zoran MJPEG bottom field first";
	case IMGFMT_XVMC_MOCO_MPEG2: return "MPEG1/2 Motion Compensation";
	case IMGFMT_XVMC_IDCT_MPEG2: return "MPEG1/2 Motion Compensation and IDCT";
	case IMGFMT_VDPAU_MPEG1: return "MPEG1 VDPAU acceleration";
	case IMGFMT_VDPAU_MPEG2: return "MPEG2 VDPAU acceleration";
	case IMGFMT_VDPAU_H264: return "H.264 VDPAU acceleration";
	case IMGFMT_VDPAU_MPEG4: return "MPEG-4 Part 2 VDPAU acceleration";
	case IMGFMT_VDPAU_WMV3: return "WMV3 VDPAU acceleration";
	case IMGFMT_VDPAU_VC1: return "VC1 VDPAU acceleration";
    }
    snprintf(unknown_format,20,"Unknown 0x%04x",format);
    return unknown_format;
}

int mp_get_chroma_shift(int format, int *x_shift, int *y_shift)
{
    int xs = 0, ys = 0;
    int bpp;
    int bpp_factor = 1;
    int err = 0;
    switch (format) {
    case IMGFMT_420P16_LE:
    case IMGFMT_420P16_BE:
        bpp_factor = 2;
    case IMGFMT_420A:
    case IMGFMT_I420:
    case IMGFMT_IYUV:
    case IMGFMT_YV12:
        xs = 1;
        ys = 1;
        break;
    case IMGFMT_IF09:
    case IMGFMT_YVU9:
        xs = 2;
        ys = 2;
        break;
    case IMGFMT_444P16_LE:
    case IMGFMT_444P16_BE:
        bpp_factor = 2;
    case IMGFMT_444P:
        xs = 0;
        ys = 0;
        break;
    case IMGFMT_422P16_LE:
    case IMGFMT_422P16_BE:
        bpp_factor = 2;
    case IMGFMT_422P:
        xs = 1;
        ys = 0;
        break;
    case IMGFMT_411P:
        xs = 2;
        ys = 0;
        break;
    case IMGFMT_440P:
        xs = 0;
        ys = 1;
        break;
    default:
        err = 1;
        break;
    }
    if (x_shift) *x_shift = xs;
    if (y_shift) *y_shift = ys;
    bpp = 8 + (16 >> (xs + ys));
    if (format == IMGFMT_420A)
        bpp += 8;
    bpp *= bpp_factor;
    return err ? 0 : bpp;
}

void mp_image_alloc_planes(mp_image_t *mpi) {
  // IF09 - allocate space for 4. plane delta info - unused
  if (mpi->imgfmt == IMGFMT_IF09) {
    mpi->planes[0]=memalign(64, mpi->bpp*mpi->width*(mpi->height+2)/8+
                            mpi->chroma_width*mpi->chroma_height);
  } else
    mpi->planes[0]=memalign(64, mpi->bpp*mpi->width*(mpi->height+2)/8);
  if (mpi->flags&MP_IMGFLAG_PLANAR) {
    int bpp = IMGFMT_IS_YUVP16(mpi->imgfmt)? 2 : 1;
    // YV12/I420/YVU9/IF09. feel free to add other planar formats here...
    mpi->stride[0]=mpi->stride[3]=bpp*mpi->width;
    if(mpi->num_planes > 2){
      mpi->stride[1]=mpi->stride[2]=bpp*mpi->chroma_width;
      if(mpi->flags&MP_IMGFLAG_SWAPPED){
        // I420/IYUV  (Y,U,V)
        mpi->planes[1]=mpi->planes[0]+mpi->stride[0]*mpi->height;
        mpi->planes[2]=mpi->planes[1]+mpi->stride[1]*mpi->chroma_height;
        if (mpi->num_planes > 3)
            mpi->planes[3]=mpi->planes[2]+mpi->stride[2]*mpi->chroma_height;
      } else {
        // YV12,YVU9,IF09  (Y,V,U)
        mpi->planes[2]=mpi->planes[0]+mpi->stride[0]*mpi->height;
        mpi->planes[1]=mpi->planes[2]+mpi->stride[1]*mpi->chroma_height;
        if (mpi->num_planes > 3)
            mpi->planes[3]=mpi->planes[1]+mpi->stride[1]*mpi->chroma_height;
      }
    } else {
      // NV12/NV21
      mpi->stride[1]=mpi->chroma_width;
      mpi->planes[1]=mpi->planes[0]+mpi->stride[0]*mpi->height;
    }
  } else {
    mpi->stride[0]=mpi->width*mpi->bpp/8;
    if (mpi->flags & MP_IMGFLAG_RGB_PALETTE)
      mpi->planes[1] = memalign(64, 1024);
  }
  mpi->flags|=MP_IMGFLAG_ALLOCATED;
}

int planes_size_mpi(mp_image_t *mpi) {
    if (mpi->imgfmt == IMGFMT_IF09)
        return mpi->bpp*mpi->width*(mpi->height+2)/8+mpi->chroma_width*mpi->chroma_height;
    return mpi->bpp*mpi->width*(mpi->height+2)/8;
}

mp_image_t* alloc_mpi(int w, int h, unsigned long int fmt) {
  mp_image_t* mpi = new_mp_image(w,h);

  mp_image_setfmt(mpi,fmt);
  mp_image_alloc_planes(mpi);

  return mpi;
}

void copy_mpi(mp_image_t *dmpi, mp_image_t *mpi) {
  if(mpi->flags&MP_IMGFLAG_PLANAR){
    memcpy_pic(dmpi->planes[0],mpi->planes[0], mpi->w, mpi->h,
	       dmpi->stride[0],mpi->stride[0]);
    memcpy_pic(dmpi->planes[1],mpi->planes[1], mpi->chroma_width, mpi->chroma_height,
	       dmpi->stride[1],mpi->stride[1]);
    memcpy_pic(dmpi->planes[2], mpi->planes[2], mpi->chroma_width, mpi->chroma_height,
	       dmpi->stride[2],mpi->stride[2]);
  } else {
    memcpy_pic(dmpi->planes[0],mpi->planes[0],
	       mpi->w*(dmpi->bpp/8), mpi->h,
	       dmpi->stride[0],mpi->stride[0]);
  }
}

mp_image_t* new_mp_image(int w,int h){
    mp_image_t* mpi = malloc(sizeof(mp_image_t));
    if(!mpi) return NULL; // error!
    memset(mpi,0,sizeof(mp_image_t));
    mpi->width=mpi->w=w;
    mpi->height=mpi->h=h;
    return mpi;
}

void free_mp_image(mp_image_t* mpi){
    if(!mpi) return;
    if(mpi->flags&MP_IMGFLAG_ALLOCATED){
        /* becouse we allocate the whole image in once */
        if(mpi->planes[0]) free(mpi->planes[0]);
        if (mpi->flags & MP_IMGFLAG_RGB_PALETTE)
            free(mpi->planes[1]);
    }
    free(mpi);
}

void mp_image_setfmt(mp_image_t* mpi,unsigned int out_fmt){
    mpi->flags&=~(MP_IMGFLAG_PLANAR|MP_IMGFLAG_YUV|MP_IMGFLAG_SWAPPED);
    mpi->imgfmt=out_fmt;
    // compressed formats
    if(out_fmt == IMGFMT_MPEGPES ||
       out_fmt == IMGFMT_ZRMJPEGNI || out_fmt == IMGFMT_ZRMJPEGIT || out_fmt == IMGFMT_ZRMJPEGIB ||
       IMGFMT_IS_VDPAU(out_fmt) || IMGFMT_IS_XVMC(out_fmt)){
	mpi->bpp=0;
	return;
    }
    mpi->num_planes=1;
    if (IMGFMT_IS_RGB(out_fmt)) {
	if (IMGFMT_RGB_DEPTH(out_fmt) < 8 && !(out_fmt&128))
	    mpi->bpp = IMGFMT_RGB_DEPTH(out_fmt);
	else
	    mpi->bpp=(IMGFMT_RGB_DEPTH(out_fmt)+7)&(~7);
	return;
    }
    if (IMGFMT_IS_BGR(out_fmt)) {
	if (IMGFMT_BGR_DEPTH(out_fmt) < 8 && !(out_fmt&128))
	    mpi->bpp = IMGFMT_BGR_DEPTH(out_fmt);
	else
	    mpi->bpp=(IMGFMT_BGR_DEPTH(out_fmt)+7)&(~7);
	mpi->flags|=MP_IMGFLAG_SWAPPED;
	return;
    }
    mpi->flags|=MP_IMGFLAG_YUV;
    mpi->num_planes=3;
    if (mp_get_chroma_shift(out_fmt, NULL, NULL)) {
        mpi->flags|=MP_IMGFLAG_PLANAR;
        mpi->bpp = mp_get_chroma_shift(out_fmt, &mpi->chroma_x_shift, &mpi->chroma_y_shift);
        mpi->chroma_width  = mpi->width  >> mpi->chroma_x_shift;
        mpi->chroma_height = mpi->height >> mpi->chroma_y_shift;
    }
    switch(out_fmt){
    case IMGFMT_I420:
    case IMGFMT_IYUV:
	mpi->flags|=MP_IMGFLAG_SWAPPED;
    case IMGFMT_YV12:
	return;
    case IMGFMT_420A:
    case IMGFMT_IF09:
	mpi->num_planes=4;
    case IMGFMT_YVU9:
    case IMGFMT_444P:
    case IMGFMT_422P:
    case IMGFMT_411P:
    case IMGFMT_440P:
    case IMGFMT_444P16_LE:
    case IMGFMT_444P16_BE:
    case IMGFMT_422P16_LE:
    case IMGFMT_422P16_BE:
    case IMGFMT_420P16_LE:
    case IMGFMT_420P16_BE:
	return;
    case IMGFMT_Y800:
    case IMGFMT_Y8:
	/* they're planar ones, but for easier handling use them as packed */
//	mpi->flags|=MP_IMGFLAG_PLANAR;
	mpi->bpp=8;
	mpi->num_planes=1;
	return;
    case IMGFMT_UYVY:
	mpi->flags|=MP_IMGFLAG_SWAPPED;
    case IMGFMT_YUY2:
	mpi->bpp=16;
	mpi->num_planes=1;
	return;
    case IMGFMT_NV12:
	mpi->flags|=MP_IMGFLAG_SWAPPED;
    case IMGFMT_NV21:
	mpi->flags|=MP_IMGFLAG_PLANAR;
	mpi->bpp=12;
	mpi->num_planes=2;
	mpi->chroma_width=(mpi->width>>0);
	mpi->chroma_height=(mpi->height>>1);
	mpi->chroma_x_shift=0;
	mpi->chroma_y_shift=1;
	return;
    }
    mpi->bpp=0;
}

/*
 *****************************************************************************************
*/

static int pcache_full(xplayer_t* xplayer, int ptype) {
    int write_ptr;
    switch(ptype) {
        case VIDEO_BUFFER:
            write_ptr = xplayer->vpackets_write+1;
            if(write_ptr==RING_BUFFER_SIZE)
                write_ptr=0;
            return (write_ptr==xplayer->vpackets_read);
        case AUDIO_BUFFER:
            write_ptr = xplayer->apackets_write+1;
            if(write_ptr==RING_BUFFER_SIZE)
                write_ptr=0;
                return (write_ptr==xplayer->apackets_read);
        default:
            break;
    }
    write_ptr = xplayer->packets_write+1;
    if(write_ptr==RING_BUFFER_SIZE)
        write_ptr=0;
    return (write_ptr==xplayer->packets_read);
}

static void pcache_push(xplayer_t* xplayer, AVPacket packet, int ptype) {
    int write_ptr;

    switch(ptype) {
        case VIDEO_BUFFER:
            write_ptr = xplayer->vpackets_write+1;
            if(write_ptr==RING_BUFFER_SIZE)
                write_ptr=0;
            if(write_ptr==xplayer->vpackets_read) {
                break;
            }
            xplayer->vpackets[xplayer->vpackets_write]=packet;
            xplayer->vpackets_write=write_ptr;
            break;
        case AUDIO_BUFFER:
            write_ptr = xplayer->apackets_write+1;
            if(write_ptr==RING_BUFFER_SIZE)
                write_ptr=0;
            if(write_ptr==xplayer->apackets_read) {
                break;
            }
            xplayer->apackets[xplayer->apackets_write]=packet;
            xplayer->apackets_write=write_ptr;
            break;
        default:
            write_ptr = xplayer->packets_write+1;
            if(write_ptr==RING_BUFFER_SIZE)
                write_ptr=0;
            if(write_ptr==xplayer->packets_read) {
                break;
            }
            xplayer->packets[xplayer->packets_write]=packet;
            xplayer->packets_write=write_ptr;
            break;
    }
}

static AVPacket* pcache_pop(xplayer_t* xplayer, int ptype) {
    AVPacket* packet = NULL;
    int pno;

    switch(ptype) {
        case VIDEO_BUFFER:
            if(xplayer->vpackets_read==xplayer->vpackets_write) {
                return packet;
            }
            packet = &xplayer->vpackets[xplayer->vpackets_read];
            xplayer->vpackets_read++;
            if(xplayer->vpackets_read==RING_BUFFER_SIZE)
                xplayer->vpackets_read=0;
            return packet;
        case AUDIO_BUFFER:
            if(xplayer->apackets_read==xplayer->apackets_write) {
                return packet;
            }
            packet = &xplayer->apackets[xplayer->apackets_read];
            xplayer->apackets_read++;
            if(xplayer->apackets_read==RING_BUFFER_SIZE)
                xplayer->apackets_read=0;
            return packet;
        default:
            break;
    }
    pno=xplayer->packets_write-xplayer->packets_read;
    if(pno<0)
        pno+=RING_BUFFER_SIZE;
    if(pno<xplayer->buffered_packet) {
        return packet;
    }
    if(xplayer->packets_read==xplayer->packets_write) {
        return packet;
    }
    packet = &xplayer->packets[xplayer->packets_read];
    xplayer->packets_read++;
    if(xplayer->packets_read==RING_BUFFER_SIZE)
        xplayer->packets_read=0;
    return packet;
}

static void pcache_free(xplayer_t* xplayer, int ptype) {
    switch(ptype) {
        case VIDEO_BUFFER:
            xplayer->vpackets_write=0;
            xplayer->vpackets_read=0;
            break;
        case AUDIO_BUFFER:
            xplayer->apackets_write=0;
            xplayer->apackets_read=0;
            break;
        default:
            xplayer->packets_write=0;
            xplayer->packets_read=0;
            break;
    }
}

/*
 *****************************************************************************************
*/

int audio_ffmpeg_format(int sample_fmt) {
    int format = 0;
    switch (sample_fmt) {
        case SAMPLE_FMT_U8:
            format = AF_FORMAT_U8;
            break;
        case SAMPLE_FMT_S16: 
            format = AF_FORMAT_S16_NE;
            break;
        case SAMPLE_FMT_S32: 
            format = AF_FORMAT_S32_NE;
            break;
        case SAMPLE_FMT_FLT: 
            format = AF_FORMAT_FLOAT_NE;
            break;
    }
    return format;
}

char* audio_format_name(int format, char* str, int size)
{
    return af_fmt2str(format, str, size);
}

const char* audio_format_name_short(int format)
{
    return af_fmt2str_short(format);
}

/*
 *****************************************************************************************
*/

void* xplayer_init(int loglevel) {
    xplayer_t* xplayer = (xplayer_t*)calloc(1,sizeof(xplayer_t));

    if(!inited) {
        av_log_set_level(loglevel);
        avcodec_register_all();
//#if CONFIG_AVFILTER
        avfilter_register_all();
//#endif
        av_register_all();
        avformat_network_init();
        inited=1;
    }
    xplayer->buf_size=0x10000;
    return xplayer;
}

int xplayer_open(void* priv, const char* filename, int noaudio) {
    xplayer_t* xplayer = (xplayer_t*)priv;
    int i;

    if(xplayer->filename)
        xplayer_close(xplayer);
    xplayer->filename=strdup(filename);
    if(av_open_input_file(&xplayer->pFormatCtx, xplayer->filename, NULL, 0, NULL)!=0) {
        free(xplayer->filename);
        xplayer->filename=NULL;
        return -1;
    }
    // Retrieve stream information
    if(av_find_stream_info(xplayer->pFormatCtx)<0) {
        free(xplayer->filename);
        xplayer->filename=NULL;
        return -1;
    }
    // Find the first video stream
    xplayer->videoStream=-1;
    xplayer->audioStream=-1;
    for(i=0; i<xplayer->pFormatCtx->nb_streams; i++) {
        if(xplayer->pFormatCtx->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO && xplayer->videoStream==-1) {
            xplayer->videoStream=i;
        }
        if(xplayer->pFormatCtx->streams[i]->codec->codec_type==AVMEDIA_TYPE_AUDIO && xplayer->audioStream==-1) {
            xplayer->audioStream=i;
        }
    }
    if(xplayer->videoStream==-1) {
        free(xplayer->filename);
        xplayer->filename=NULL;
        return -1;
    }
    // Get a pointer to the codec context for the video stream
    xplayer->vCodecCtx=xplayer->pFormatCtx->streams[xplayer->videoStream]->codec;
    // Find the decoder for the video stream
    xplayer->vCodec=avcodec_find_decoder(xplayer->vCodecCtx->codec_id);
    if(xplayer->vCodec==NULL) {
        free(xplayer->filename);
        xplayer->filename=NULL;
        return -1;
    }
    // Open codec
    if(avcodec_open(xplayer->vCodecCtx, xplayer->vCodec)<0) {
        free(xplayer->filename);
        xplayer->filename=NULL;
        return -1;
    }
    // Allocate video frame
    xplayer->vFrame=avcodec_alloc_frame();
    // Audio
    xplayer->aCodecCtx = NULL;
    xplayer->aCodec = NULL;
    if(xplayer->audioStream!=-1 && !noaudio) {
        xplayer->aCodecCtx=xplayer->pFormatCtx->streams[xplayer->audioStream]->codec;
        xplayer->aCodec = avcodec_find_decoder(xplayer->aCodecCtx->codec_id);
        if(!xplayer->aCodec) {
            free(xplayer->filename);
            xplayer->filename=NULL;
            return -1;
        }
        avcodec_open(xplayer->aCodecCtx, xplayer->aCodec);
        xplayer->aFrame=avcodec_alloc_frame();
    }
    if(xplayer->fps<=0.0)
        xplayer->fps=25.00;
    xplayer->waittime=1000000/xplayer->fps;
    xplayer->fmt = IMGFMT_BGR32;
    return 0;
}

void xplayer_setimage(void* priv, int w, int h, unsigned int fmt) {
    xplayer_t* xplayer = (xplayer_t*)priv;

    xplayer->imgw = w;
    xplayer->imgh = h;
    xplayer->fmt = fmt;
}

int xplayer_read(void* priv) {
    xplayer_t* xplayer = (xplayer_t*)priv;
    AVPacket packet;

    if(!xplayer->filename) {
        av_log(NULL, AV_LOG_ERROR,"File name is null!\n");
        return -1;
    }
    if(!xplayer->vCodecCtx && !xplayer->aCodecCtx) {
        av_log(NULL, AV_LOG_ERROR,"Video and audio codec is null!\n");
        return -1;
    }
#ifdef USE_CACHE
    if(pcache_full(xplayer, MASTER_BUFFER))
        return 0;
    if(av_read_frame(xplayer->pFormatCtx, &packet)<0) {
        av_log(NULL, AV_LOG_ERROR,"av_read_frame fail!\n");
        return -1;
    }
    xplayer->rpts=av_rescale_q(packet.pts, xplayer->pFormatCtx->streams[xplayer->videoStream]->time_base, AV_TIME_BASE_Q) / 1e6;
    pcache_push(xplayer, packet, MASTER_BUFFER);
//    av_log(NULL, AV_LOG_DEBUG,"readed packet: %12.6f\n",xplayer->rpts);
#endif
    return 0;
}

int xplayer_cache_read(void* priv) {
    xplayer_t* xplayer = (xplayer_t*)priv;
    AVPacket* packet = NULL;
    double pts;

    if(!xplayer->filename)
        return -1;
    if(!xplayer->vCodecCtx && !xplayer->aCodecCtx)
        return -1;
#ifdef USE_CACHE
    if(pcache_full(xplayer, VIDEO_BUFFER) || pcache_full(xplayer, AUDIO_BUFFER)) {
        return 0;
    }
    if(!(packet=pcache_pop(xplayer, MASTER_BUFFER)))
    {
        return 0;
    }
    pts=av_rescale_q(packet->pts, xplayer->pFormatCtx->streams[xplayer->videoStream]->time_base, AV_TIME_BASE_Q) / 1e6;
    if(pts>0.0) {
        xplayer->pts=pts;
    }
    if(packet->stream_index==xplayer->videoStream) {
        if(pts>0.0)
            xplayer->rvpts=pts;
        pcache_push(xplayer, *packet, VIDEO_BUFFER);
    } else {
        if(pts>0.0)
            xplayer->rapts=pts;
        pcache_push(xplayer, *packet, AUDIO_BUFFER);
    }
#endif
    return 0;
}

int xplayer_loop(void* priv) {
    xplayer_t* xplayer = (xplayer_t*)priv;
    int ret = 0;
    int got_frame = 0;
    int data_size = 0;
    int alen = 0;
    AVPacket* packet = NULL;
#ifndef USE_CACHE
    AVPacket  packet_s;
#endif

    if(!xplayer->filename)
        return -1;
    if(!xplayer->vCodecCtx && !xplayer->aCodecCtx)
        return -1;
#ifdef USE_CACHE
#warning FIXME!!!
    if(!(packet=pcache_pop(xplayer, 1)))
        return ret;
#else
    packet=&packet_s;
    if(av_read_frame(xplayer->pFormatCtx, packet)<0)
        return -1;
#endif
    if(packet->stream_index==xplayer->videoStream && xplayer->vCodecCtx) {
        avcodec_decode_video2(xplayer->vCodecCtx, xplayer->vFrame, &xplayer->frameFinished, packet);
        if(xplayer->frameFinished) {
            if(xplayer->origw != xplayer->vCodecCtx->width || xplayer->origh != xplayer->vCodecCtx->height || xplayer->origpixfmt != xplayer->vCodecCtx->pix_fmt) {
                if(xplayer->img_convert_ctx)
                    sws_freeContext(xplayer->img_convert_ctx);
                xplayer->img_convert_ctx=NULL;
                if(xplayer->img)
                    free_mp_image(xplayer->img);
                xplayer->img=NULL;
            }
            if(xplayer->img_convert_ctx == NULL) {
                xplayer->origw=xplayer->vCodecCtx->width;
                xplayer->origh=xplayer->vCodecCtx->height;
                xplayer->origpixfmt=xplayer->vCodecCtx->pix_fmt;
                if(!xplayer->imgw || !xplayer->imgh) {
                    xplayer->w = xplayer->vCodecCtx->width;
                    xplayer->h = xplayer->vCodecCtx->height;
                } else {
                    xplayer->w = xplayer->imgw;
                    xplayer->h = xplayer->imgh;
                }
                xplayer->pixfmt = imgfmt2pixfmt(xplayer->fmt);
                if(!xplayer->pixfmt)
                    xplayer->pixfmt = PIX_FMT_RGB32;

                xplayer->img_convert_ctx = sws_getContext(xplayer->vCodecCtx->width, xplayer->vCodecCtx->height, xplayer->vCodecCtx->pix_fmt, xplayer->w, xplayer->h, xplayer->pixfmt, SWS_BICUBIC, NULL, NULL, NULL);
                if(!xplayer->img_convert_ctx) {
                    av_log(NULL, AV_LOG_ERROR,"sws_getContext() error.\n");
                    ret=-1;
                }
            }
            if(xplayer->img_convert_ctx) {
                if(!xplayer->img) {
                    unsigned int fmt = pixfmt2imgfmt(xplayer->pixfmt);
                    if(!fmt)
                        fmt=IMGFMT_BGR32;
                    xplayer->img=alloc_mpi(xplayer->w, xplayer->h, fmt);
                }
                sws_scale(xplayer->img_convert_ctx, (const uint8_t * const*)xplayer->vFrame->data, xplayer->vFrame->linesize, 0, xplayer->vCodecCtx->height, xplayer->img->planes, xplayer->img->stride);
                xplayer->newimg=1;
                xplayer->vpts = av_rescale_q(xplayer->vFrame->pkt_pts, xplayer->pFormatCtx->streams[xplayer->videoStream]->time_base, AV_TIME_BASE_Q) / 1e6;
//                usleep(xplayer->waittime);
            }
        }
    }
    if(packet->stream_index==xplayer->audioStream && xplayer->aCodecCtx) {
        data_size = xplayer->buf_size;
        if (xplayer->aCodecCtx->get_buffer != avcodec_default_get_buffer) {
        }
        if(!xplayer->buf)
            xplayer->buf=calloc(1,xplayer->buf_size);
        alen = avcodec_decode_audio4(xplayer->aCodecCtx, xplayer->aFrame, &got_frame, packet);
        if (alen >= 0 && got_frame) {
            int ch, plane_size;
            int planar = av_sample_fmt_is_planar(xplayer->aCodecCtx->sample_fmt);
            int sample_data_size = av_samples_get_buffer_size(&plane_size, xplayer->aCodecCtx->channels, xplayer->aFrame->nb_samples, xplayer->aCodecCtx->sample_fmt, 1);
            if (data_size < sample_data_size) {
                data_size=0;
            }
            data_size=plane_size;
            memcpy(xplayer->buf, xplayer->aFrame->extended_data[0], plane_size);
            if (planar && xplayer->aCodecCtx->channels > 1) {
                uint8_t *out = ((uint8_t *)xplayer->buf) + plane_size;
                for (ch = 1; ch < xplayer->aCodecCtx->channels; ch++) {
                    memcpy(out, xplayer->aFrame->extended_data[ch], plane_size);
                    out += plane_size;
                    data_size+=plane_size;
                }
            }
            if(data_size) {
                if(!xplayer->audio_buf) {
                    xplayer->audio_buf=calloc(1,xplayer->buf_size);
                    xplayer->audio_buf_size=xplayer->buf_size;
                }
                if(data_size+xplayer->audio_buf_len>xplayer->audio_buf_size) {
                    xplayer->audio_buf_size*=2;
                    char* tmp = calloc(1,xplayer->audio_buf_size);
                    memcpy(tmp,xplayer->audio_buf,xplayer->audio_buf_len);
                    free(xplayer->audio_buf);
                    xplayer->audio_buf=tmp;
                }
                if(!xplayer->audio_buf_len) {
                    xplayer->apts = av_rescale_q(xplayer->aFrame->pkt_pts, xplayer->pFormatCtx->streams[xplayer->audioStream]->time_base, AV_TIME_BASE_Q) / 1e6;
                }
                memcpy(xplayer->audio_buf+xplayer->audio_buf_len,xplayer->buf,data_size);
                xplayer->audio_buf_len+=data_size;
            }
        } else {
            data_size = 0;
        }

    }
    // Free the packet that was allocated by av_read_frame
    av_free_packet(packet);
    return ret;
}

int xplayer_vloop(void* priv) {
    xplayer_t* xplayer = (xplayer_t*)priv;
    int ret = 0;
    AVPacket* packet = NULL;

#ifndef USE_CACHE
    return xplayer_loop(priv);
#endif
    if(!xplayer->filename)
        return -1;
    if(!xplayer->vCodecCtx && !xplayer->aCodecCtx)
        return -1;
    if(!(packet=pcache_pop(xplayer, VIDEO_BUFFER)))
        return ret;
    if(packet->stream_index==xplayer->videoStream && xplayer->vCodecCtx) {
        avcodec_decode_video2(xplayer->vCodecCtx, xplayer->vFrame, &xplayer->frameFinished, packet);
        if(xplayer->frameFinished) {
            if(xplayer->origw != xplayer->vCodecCtx->width || xplayer->origh != xplayer->vCodecCtx->height || xplayer->origpixfmt != xplayer->vCodecCtx->pix_fmt) {
                if(xplayer->img_convert_ctx)
                    sws_freeContext(xplayer->img_convert_ctx);
                xplayer->img_convert_ctx=NULL;
                if(xplayer->img)
                    free_mp_image(xplayer->img);
                xplayer->img=NULL;
            }
            if(xplayer->img_convert_ctx == NULL) {
                xplayer->origw=xplayer->vCodecCtx->width;
                xplayer->origh=xplayer->vCodecCtx->height;
                xplayer->origpixfmt=xplayer->vCodecCtx->pix_fmt;
                if(!xplayer->imgw || !xplayer->imgh) {
                    xplayer->w = xplayer->vCodecCtx->width;
                    xplayer->h = xplayer->vCodecCtx->height;
                } else {
                    xplayer->w = xplayer->imgw;
                    xplayer->h = xplayer->imgh;
                }
                xplayer->pixfmt = imgfmt2pixfmt(xplayer->fmt);
                if(!xplayer->pixfmt)
                    xplayer->pixfmt = PIX_FMT_RGB32;

                xplayer->img_convert_ctx = sws_getContext(xplayer->vCodecCtx->width, xplayer->vCodecCtx->height, xplayer->vCodecCtx->pix_fmt, xplayer->w, xplayer->h, xplayer->pixfmt, SWS_BICUBIC, NULL, NULL, NULL);
                if(!xplayer->img_convert_ctx) {
                    av_log(NULL, AV_LOG_ERROR,"sws_getContext() error.\n");
                    ret=-1;
                }
            }
            if(xplayer->img_convert_ctx) {
                if(!xplayer->img) {
                    unsigned int fmt = pixfmt2imgfmt(xplayer->pixfmt);
                    if(!fmt)
                        fmt=IMGFMT_BGR32;
                    xplayer->img=alloc_mpi(xplayer->w, xplayer->h, fmt);
                }
                sws_scale(xplayer->img_convert_ctx, (const uint8_t * const*)xplayer->vFrame->data, xplayer->vFrame->linesize, 0, xplayer->vCodecCtx->height, xplayer->img->planes, xplayer->img->stride);
                xplayer->newimg=1;
                xplayer->vpts = av_rescale_q(xplayer->vFrame->pkt_pts, xplayer->pFormatCtx->streams[xplayer->videoStream]->time_base, AV_TIME_BASE_Q) / 1e6;
//                usleep(xplayer->waittime);
            }
        }
    }
    // Free the packet that was allocated by av_read_frame
    av_free_packet(packet);
    return ret;
}

int xplayer_aloop(void* priv) {
    xplayer_t* xplayer = (xplayer_t*)priv;
    int ret = 0;
    int got_frame = 0;
    int data_size = 0;
    int alen = 0;
    AVPacket* packet = NULL;

#ifndef USE_CACHE
    return xplayer_loop(priv);
#endif
    if(!xplayer->filename)
        return -1;
    if(!xplayer->vCodecCtx && !xplayer->aCodecCtx)
        return -1;
    if(!(packet=pcache_pop(xplayer, AUDIO_BUFFER)))
        return ret;
    if(packet->stream_index==xplayer->audioStream && xplayer->aCodecCtx) {
        data_size = xplayer->buf_size;
        if (xplayer->aCodecCtx->get_buffer != avcodec_default_get_buffer) {
        }
        if(!xplayer->buf)
            xplayer->buf=calloc(1,xplayer->buf_size);
        alen = avcodec_decode_audio4(xplayer->aCodecCtx, xplayer->aFrame, &got_frame, packet);
        if (alen >= 0 && got_frame) {
            int ch, plane_size;
            int planar = av_sample_fmt_is_planar(xplayer->aCodecCtx->sample_fmt);
            int sample_data_size = av_samples_get_buffer_size(&plane_size, xplayer->aCodecCtx->channels, xplayer->aFrame->nb_samples, xplayer->aCodecCtx->sample_fmt, 1);
            if (data_size < sample_data_size) {
                data_size=0;
            }
            data_size=plane_size;
            memcpy(xplayer->buf, xplayer->aFrame->extended_data[0], plane_size);
            if (planar && xplayer->aCodecCtx->channels > 1) {
                uint8_t *out = ((uint8_t *)xplayer->buf) + plane_size;
                for (ch = 1; ch < xplayer->aCodecCtx->channels; ch++) {
                    memcpy(out, xplayer->aFrame->extended_data[ch], plane_size);
                    out += plane_size;
                    data_size+=plane_size;
                }
            }
            if(data_size) {
                if(!xplayer->audio_buf) {
                    xplayer->audio_buf=calloc(1,xplayer->buf_size);
                    xplayer->audio_buf_size=xplayer->buf_size;
                }
                if(data_size+xplayer->audio_buf_len>xplayer->audio_buf_size) {
                    xplayer->audio_buf_size*=2;
                    char* tmp = calloc(1,xplayer->audio_buf_size);
                    memcpy(tmp,xplayer->audio_buf,xplayer->audio_buf_len);
                    free(xplayer->audio_buf);
                    xplayer->audio_buf=tmp;
                }
                if(!xplayer->audio_buf_len) {
                    xplayer->apts = av_rescale_q(xplayer->aFrame->pkt_pts, xplayer->pFormatCtx->streams[xplayer->audioStream]->time_base, AV_TIME_BASE_Q) / 1e6;
                }
                memcpy(xplayer->audio_buf+xplayer->audio_buf_len,xplayer->buf,data_size);
                xplayer->audio_buf_len+=data_size;
            }
        } else {
            data_size = 0;
        }

    }
    // Free the packet that was allocated by av_read_frame
    av_free_packet(packet);
    return ret;
}

int xplayer_close(void* priv) {
    xplayer_t* xplayer = (xplayer_t*)priv;

    pcache_free(xplayer, VIDEO_BUFFER);
    pcache_free(xplayer, AUDIO_BUFFER);
    pcache_free(xplayer, MASTER_BUFFER);
    if(xplayer->filename)
        free(xplayer->filename);
    xplayer->filename=NULL;
        // Free the YUV frame
    if(xplayer->vFrame)
        av_free(xplayer->vFrame);
    xplayer->vFrame=NULL;
    if(xplayer->aFrame)
        av_free(xplayer->aFrame);
    xplayer->aFrame=NULL;
    // Close the codec
    if(xplayer->vCodecCtx)
        avcodec_close(xplayer->vCodecCtx);
    xplayer->vCodecCtx=NULL;
    if(xplayer->aCodecCtx)
        avcodec_close(xplayer->aCodecCtx);
    xplayer->aCodecCtx=NULL;
    // Close the video file
    if(xplayer->pFormatCtx)
        av_close_input_file(xplayer->pFormatCtx);
    xplayer->pFormatCtx=NULL;
    if(xplayer->buf)
        free(xplayer->buf);
    xplayer->buf=NULL;
    if(xplayer->img_convert_ctx)
        sws_freeContext(xplayer->img_convert_ctx);
    xplayer->img_convert_ctx=NULL;
    xplayer->origw=0;
    xplayer->origh=0;
    xplayer->origpixfmt=0;
    xplayer->vpts=0.0;
    xplayer->apts=0.0;
    return 0;
}

void xplayer_uninit(void* priv) {
    xplayer_t* xplayer = (xplayer_t*)priv;
    if(xplayer->filename)
        xplayer_close(xplayer);
    free(xplayer);
}

int xplayer_isnewimage(void* priv) {
    xplayer_t* xplayer = (xplayer_t*)priv;

    return (xplayer->newimg && xplayer->img);
}

mp_image_t* xplayer_getimage(void* priv) {
    xplayer_t* xplayer = (xplayer_t*)priv;

    xplayer->newimg=0;
    return xplayer->img;
}

int xplayer_audiorate(void* priv) {
    xplayer_t* xplayer = (xplayer_t*)priv;

    if(xplayer->aCodecCtx)
        return xplayer->aCodecCtx->sample_rate;
    return 0;
}

int xplayer_audioch(void* priv) {
    xplayer_t* xplayer = (xplayer_t*)priv;

    if(xplayer->aCodecCtx)
        return xplayer->aCodecCtx->channels;
    return 0;
}

int xplayer_audiofmt(void* priv) {
    xplayer_t* xplayer = (xplayer_t*)priv;

    if(xplayer->aCodecCtx)
        return xplayer->aCodecCtx->sample_fmt;
    return 0;
}

int xplayer_audiolen(void* priv) {
    xplayer_t* xplayer = (xplayer_t*)priv;

    return xplayer->audio_buf_len;
}

char* xplayer_audio(void* priv) {
    xplayer_t* xplayer = (xplayer_t*)priv;
    char* ret = NULL;

    if(xplayer->audio_buf_len) {
        ret = malloc(xplayer->audio_buf_len);
        memcpy(ret,xplayer->audio_buf,xplayer->audio_buf_len);
        xplayer->audio_buf_len=0;
    }
    return ret;
}

int xplayer_seek(void* priv, double timestamp) {
    xplayer_t* xplayer = (xplayer_t*)priv;

//    return avformat_seek_file(xplayer->pFormatCtx, -1, INT64_MIN, timestamp, INT64_MAX, 0);
    pcache_free(xplayer, VIDEO_BUFFER);
    pcache_free(xplayer, AUDIO_BUFFER);
    pcache_free(xplayer, MASTER_BUFFER);
    return av_seek_frame(xplayer->pFormatCtx, -1, timestamp* AV_TIME_BASE, 0);
}

double xplayer_duration(void* priv) {
    xplayer_t* xplayer = (xplayer_t*)priv;
    double ret = 0.0;

    if(xplayer->pFormatCtx) {
        ret = xplayer->pFormatCtx->duration/1000000LL;
    }
    return ret;
}

double xplayer_pts(void* priv) {
    xplayer_t* xplayer = (xplayer_t*)priv;

    return xplayer->pts;
}

double xplayer_vpts(void* priv) {
    xplayer_t* xplayer = (xplayer_t*)priv;

    return xplayer->vpts;
}

double xplayer_rpts(void* priv) {
    xplayer_t* xplayer = (xplayer_t*)priv;

    return xplayer->rpts;
}

double xplayer_rapts(void* priv) {
    xplayer_t* xplayer = (xplayer_t*)priv;

    return xplayer->rapts;
}

double xplayer_rvpts(void* priv) {
    xplayer_t* xplayer = (xplayer_t*)priv;

    return xplayer->rvpts;
}

double xplayer_apts(void* priv) {
    xplayer_t* xplayer = (xplayer_t*)priv;

    return xplayer->apts;
}

double xplayer_clock(void* priv) {
    int64_t ti;
    ti = av_gettime();
    return (double)ti/1e6;
}

double xplayer_framerate(void* priv) {
    xplayer_t* xplayer = (xplayer_t*)priv;
    double ret = 0.0;

    if(xplayer->pFormatCtx) {
        ret = av_q2d(xplayer->pFormatCtx->streams[xplayer->videoStream]->r_frame_rate);
    }
    return ret;
}

int xplayer_video_info(void* priv, video_info_t* video_info) {
    xplayer_t* xplayer = (xplayer_t*)priv;
    if(!priv || !video_info || !xplayer->vCodecCtx || xplayer->videoStream==-1) {
        return 1;
    }
    video_info->valid=1;
    video_info->streamid=xplayer->videoStream;
    video_info->width=xplayer->vCodecCtx->width;
    video_info->height=xplayer->vCodecCtx->height;
    video_info->format=pixfmt2imgfmt(xplayer->vCodecCtx->pix_fmt);
    video_info->fps=av_q2d(xplayer->pFormatCtx->streams[xplayer->videoStream]->r_frame_rate);
    return 0;
}

int xplayer_audio_info(void* priv, audio_info_t* audio_info) {
    xplayer_t* xplayer = (xplayer_t*)priv;
    if(!priv || !audio_info || !xplayer->aCodecCtx || xplayer->audioStream==-1) {
        return 1;
    }
    audio_info->valid=1;
    audio_info->streamid=xplayer->audioStream;
    audio_info->channels=xplayer->aCodecCtx->channels;
    audio_info->rate=xplayer->aCodecCtx->sample_rate;
    audio_info->format=audio_ffmpeg_format(xplayer->aCodecCtx->sample_fmt);
    return 0;
}

int xplayer_buffer_size(void* priv)
{
    return RING_BUFFER_SIZE;
}

int xplayer_get_buffered_packet(void* priv)
{
    xplayer_t* xplayer = (xplayer_t*)priv;
    if(!priv)
    {
        return -1;
    }
    return xplayer->buffered_packet;
}

int xplayer_set_buffered_packet(void* priv, int pno)
{
    xplayer_t* xplayer = (xplayer_t*)priv;
    if(!priv || pno<0 || pno>RING_BUFFER_SIZE)
    {
        return -1;
    }
    xplayer->buffered_packet=pno;
    return 0;
}

