
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#if !defined(__APPLE__)
  #include <malloc.h>
#endif

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

int audio_ffmpeg_format(int sample_fmt) {
    int format = 0;
    switch (sample_fmt) {
        case AV_SAMPLE_FMT_U8:
            format = AF_FORMAT_U8;
            break;
        case AV_SAMPLE_FMT_S16:
            format = AF_FORMAT_S16_NE;
            break;
        case AV_SAMPLE_FMT_S32:
            format = AF_FORMAT_S32_NE;
            break;
        case AV_SAMPLE_FMT_FLT:
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

double xplayer_clock() {
    int64_t ti;
    ti = av_gettime();
    return (double)ti/1e6;
}
