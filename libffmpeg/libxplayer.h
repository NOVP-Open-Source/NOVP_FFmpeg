#ifndef int64_t
typedef signed long long int 	int64_t;
#endif

#ifndef __LIBXPALAYER_H_
#define __LIBXPALAYER_H_

#ifdef __cplusplus
extern "C" {
#endif

#ifndef MPLAYER_MP_IMAGE_H
#define MPLAYER_MP_IMAGE_H
/*
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

extern int verbose;

//--- buffer content restrictions:
// set if buffer content shouldn't be modified:
#define MP_IMGFLAG_PRESERVE 0x01
// set if buffer content will be READ for next frame's MC: (I/P mpeg frames)
#define MP_IMGFLAG_READABLE 0x02

//--- buffer width/stride/plane restrictions: (used for direct rendering)
// stride _have_to_ be aligned to MB boundary:  [for DR restrictions]
#define MP_IMGFLAG_ACCEPT_ALIGNED_STRIDE 0x4
// stride should be aligned to MB boundary:     [for buffer allocation]
#define MP_IMGFLAG_PREFER_ALIGNED_STRIDE 0x8
// codec accept any stride (>=width):
#define MP_IMGFLAG_ACCEPT_STRIDE 0x10
// codec accept any width (width*bpp=stride -> stride%bpp==0) (>=width):
#define MP_IMGFLAG_ACCEPT_WIDTH 0x20
//--- for planar formats only:
// uses only stride[0], and stride[1]=stride[2]=stride[0]>>mpi->chroma_x_shift
#define MP_IMGFLAG_COMMON_STRIDE 0x40
// uses only planes[0], and calculates planes[1,2] from width,height,imgfmt
#define MP_IMGFLAG_COMMON_PLANE 0x80

#define MP_IMGFLAGMASK_RESTRICTIONS 0xFF

//--------- color info (filled by mp_image_setfmt() ) -----------
// set if number of planes > 1
#define MP_IMGFLAG_PLANAR 0x100
// set if it's YUV colorspace
#define MP_IMGFLAG_YUV 0x200
// set if it's swapped (BGR or YVU) plane/byteorder
#define MP_IMGFLAG_SWAPPED 0x400
// set if you want memory for palette allocated and managed by vf_get_image etc.
#define MP_IMGFLAG_RGB_PALETTE 0x800

#define MP_IMGFLAGMASK_COLORS 0xF00

// codec uses drawing/rendering callbacks (draw_slice()-like thing, DR method 2)
// [the codec will set this flag if it supports callbacks, and the vo _may_
//  clear it in get_image() if draw_slice() not implemented]
#define MP_IMGFLAG_DRAW_CALLBACK 0x1000
// set if it's in video buffer/memory: [set by vo/vf's get_image() !!!]
#define MP_IMGFLAG_DIRECT 0x2000
// set if buffer is allocated (used in destination images):
#define MP_IMGFLAG_ALLOCATED 0x4000

// buffer type was printed (do NOT set this flag - it's for INTERNAL USE!!!)
#define MP_IMGFLAG_TYPE_DISPLAYED 0x8000

// codec doesn't support any form of direct rendering - it has own buffer
// allocation. so we just export its buffer pointers:
#define MP_IMGTYPE_EXPORT 0
// codec requires a static WO buffer, but it does only partial updates later:
#define MP_IMGTYPE_STATIC 1
// codec just needs some WO memory, where it writes/copies the whole frame to:
#define MP_IMGTYPE_TEMP 2
// I+P type, requires 2+ independent static R/W buffers
#define MP_IMGTYPE_IP 3
// I+P+B type, requires 2+ independent static R/W and 1+ temp WO buffers
#define MP_IMGTYPE_IPB 4
// Upper 16 bits give desired buffer number, -1 means get next available
#define MP_IMGTYPE_NUMBERED 5

#define MP_MAX_PLANES	4

#define MP_IMGFIELD_ORDERED 0x01
#define MP_IMGFIELD_TOP_FIRST 0x02
#define MP_IMGFIELD_REPEAT_FIRST 0x04
#define MP_IMGFIELD_TOP 0x08
#define MP_IMGFIELD_BOTTOM 0x10
#define MP_IMGFIELD_INTERLACED 0x20

typedef struct mp_image_s {
    unsigned int flags;
    unsigned char type;
    int number;
    unsigned char bpp;  // bits/pixel. NOT depth! for RGB it will be n*8
    unsigned int imgfmt;
    int width,height;  // stored dimensions
    int x,y,w,h;  // visible dimensions
    unsigned char* planes[MP_MAX_PLANES];
    int stride[MP_MAX_PLANES];
    char * qscale;
    int qstride;
    int pict_type; // 0->unknown, 1->I, 2->P, 3->B
    int fields;
    int qscale_type; // 0->mpeg1/4/h263, 1->mpeg2
    int num_planes;
    /* these are only used by planar formats Y,U(Cb),V(Cr) */
    int chroma_width;
    int chroma_height;
    int chroma_x_shift; // horizontal
    int chroma_y_shift; // vertical
    int usage_count;
    /* for private use by filter or vo driver (to store buffer id or dmpi) */
    void* priv;
} mp_image_t;

#define VOFLAG_HIDDEN 1
#define VOFLAG_FULLSCREEN 2

#define vo_wm_LAYER 1
#define vo_wm_FULLSCREEN 2
#define vo_wm_STAYS_ON_TOP 4
#define vo_wm_ABOVE 8
#define vo_wm_BELOW 16
#define vo_wm_NETWM (vo_wm_FULLSCREEN | vo_wm_STAYS_ON_TOP | vo_wm_ABOVE | vo_wm_BELOW)

mp_image_t* alloc_mpi(int w, int h, unsigned long int fmt);

/*
 *********************  End of MPlayer code. ****************************
 */
#endif  //#ifndef MPLAYER_MP_IMAGE_H

#ifndef MPLAYER_IMG_FORMAT_H
#define MPLAYER_IMG_FORMAT_H

/* RGB/BGR Formats */

#define IMGFMT_RGB_MASK 0xFFFFFF00
#define IMGFMT_RGB (('R'<<24)|('G'<<16)|('B'<<8))
#define IMGFMT_RGB1  (IMGFMT_RGB|1)
#define IMGFMT_RGB4  (IMGFMT_RGB|4)
#define IMGFMT_RGB4_CHAR  (IMGFMT_RGB|4|128) // RGB4 with 1 pixel per byte
#define IMGFMT_RGB8  (IMGFMT_RGB|8)
#define IMGFMT_RGB15 (IMGFMT_RGB|15)
#define IMGFMT_RGB16 (IMGFMT_RGB|16)
#define IMGFMT_RGB24 (IMGFMT_RGB|24)
#define IMGFMT_RGB32 (IMGFMT_RGB|32)
#define IMGFMT_RGB48LE (IMGFMT_RGB|48)
#define IMGFMT_RGB48BE (IMGFMT_RGB|48|128)

#define IMGFMT_BGR_MASK 0xFFFFFF00
#define IMGFMT_BGR (('B'<<24)|('G'<<16)|('R'<<8))
#define IMGFMT_BGR1 (IMGFMT_BGR|1)
#define IMGFMT_BGR4 (IMGFMT_BGR|4)
#define IMGFMT_BGR4_CHAR (IMGFMT_BGR|4|128) // BGR4 with 1 pixel per byte
#define IMGFMT_BGR8 (IMGFMT_BGR|8)
#define IMGFMT_BGR15 (IMGFMT_BGR|15)
#define IMGFMT_BGR16 (IMGFMT_BGR|16)
#define IMGFMT_BGR24 (IMGFMT_BGR|24)
#define IMGFMT_BGR32 (IMGFMT_BGR|32)

#if HAVE_BIGENDIAN
#define IMGFMT_ABGR IMGFMT_RGB32
#define IMGFMT_BGRA (IMGFMT_RGB32|64)
#define IMGFMT_ARGB IMGFMT_BGR32
#define IMGFMT_RGBA (IMGFMT_BGR32|64)
#define IMGFMT_RGB48NE IMGFMT_RGB48BE
#define IMGFMT_RGB15BE IMGFMT_RGB15
#define IMGFMT_RGB15LE (IMGFMT_RGB15|64)
#define IMGFMT_RGB16BE IMGFMT_RGB16
#define IMGFMT_RGB16LE (IMGFMT_RGB16|64)
#define IMGFMT_BGR15BE IMGFMT_BGR15
#define IMGFMT_BGR15LE (IMGFMT_BGR15|64)
#define IMGFMT_BGR16BE IMGFMT_BGR16
#define IMGFMT_BGR16LE (IMGFMT_BGR16|64)
#else
#define IMGFMT_ABGR (IMGFMT_BGR32|64)
#define IMGFMT_BGRA IMGFMT_BGR32
#define IMGFMT_ARGB (IMGFMT_RGB32|64)
#define IMGFMT_RGBA IMGFMT_RGB32
#define IMGFMT_RGB48NE IMGFMT_RGB48LE
#define IMGFMT_RGB15BE (IMGFMT_RGB15|64)
#define IMGFMT_RGB15LE IMGFMT_RGB15
#define IMGFMT_RGB16BE (IMGFMT_RGB16|64)
#define IMGFMT_RGB16LE IMGFMT_RGB16
#define IMGFMT_BGR15BE (IMGFMT_BGR15|64)
#define IMGFMT_BGR15LE IMGFMT_BGR15
#define IMGFMT_BGR16BE (IMGFMT_BGR16|64)
#define IMGFMT_BGR16LE IMGFMT_BGR16
#endif

/* old names for compatibility */
#define IMGFMT_RG4B  IMGFMT_RGB4_CHAR
#define IMGFMT_BG4B  IMGFMT_BGR4_CHAR

#define IMGFMT_IS_RGB(fmt) (((fmt)&IMGFMT_RGB_MASK)==IMGFMT_RGB)
#define IMGFMT_IS_BGR(fmt) (((fmt)&IMGFMT_BGR_MASK)==IMGFMT_BGR)

#define IMGFMT_RGB_DEPTH(fmt) ((fmt)&0x3F)
#define IMGFMT_BGR_DEPTH(fmt) ((fmt)&0x3F)


/* Planar YUV Formats */

#define IMGFMT_YVU9 0x39555659
#define IMGFMT_IF09 0x39304649
#define IMGFMT_YV12 0x32315659
#define IMGFMT_I420 0x30323449
#define IMGFMT_IYUV 0x56555949
#define IMGFMT_CLPL 0x4C504C43
#define IMGFMT_Y800 0x30303859
#define IMGFMT_Y8   0x20203859
#define IMGFMT_NV12 0x3231564E
#define IMGFMT_NV21 0x3132564E

// FIXME!!!
#define IMGFMT_Y16LE 0x20363159
#define IMGFMT_Y16   IMGFMT_Y16LE

/* unofficial Planar Formats, FIXME if official 4CC exists */
#define IMGFMT_444P 0x50343434
#define IMGFMT_422P 0x50323234
#define IMGFMT_411P 0x50313134
#define IMGFMT_440P 0x50303434
#define IMGFMT_HM12 0x32314D48

// 4:2:0 planar with alpha
#define IMGFMT_420A 0x41303234

#define IMGFMT_444P16_LE 0x51343434
#define IMGFMT_444P16_BE 0x34343451
#define IMGFMT_422P16_LE 0x51323234
#define IMGFMT_422P16_BE 0x34323251
#define IMGFMT_420P16_LE 0x51303234
#define IMGFMT_420P16_BE 0x34323051
#if HAVE_BIGENDIAN
#define IMGFMT_444P16 IMGFMT_444P16_BE
#define IMGFMT_422P16 IMGFMT_422P16_BE
#define IMGFMT_420P16 IMGFMT_420P16_BE
#else
#define IMGFMT_444P16 IMGFMT_444P16_LE
#define IMGFMT_422P16 IMGFMT_422P16_LE
#define IMGFMT_420P16 IMGFMT_420P16_LE
#endif

#define IMGFMT_IS_YUVP16_LE(fmt) (((fmt  ^ IMGFMT_420P16_LE) & 0xff0000ff) == 0)
#define IMGFMT_IS_YUVP16_BE(fmt) (((fmt  ^ IMGFMT_420P16_BE) & 0xff0000ff) == 0)
#define IMGFMT_IS_YUVP16_NE(fmt) (((fmt  ^ IMGFMT_420P16   ) & 0xff0000ff) == 0)
#define IMGFMT_IS_YUVP16(fmt)    (IMGFMT_IS_YUVP16_LE(fmt) || IMGFMT_IS_YUVP16_BE(fmt))

/* Packed YUV Formats */

#define IMGFMT_IUYV 0x56595549
#define IMGFMT_IY41 0x31435949
#define IMGFMT_IYU1 0x31555949
#define IMGFMT_IYU2 0x32555949
#define IMGFMT_UYVY 0x59565955
#define IMGFMT_UYNV 0x564E5955
#define IMGFMT_cyuv 0x76757963
#define IMGFMT_Y422 0x32323459
#define IMGFMT_YUY2 0x32595559
#define IMGFMT_YUNV 0x564E5559
#define IMGFMT_YVYU 0x55595659
#define IMGFMT_Y41P 0x50313459
#define IMGFMT_Y211 0x31313259
#define IMGFMT_Y41T 0x54313459
#define IMGFMT_Y42T 0x54323459
#define IMGFMT_V422 0x32323456
#define IMGFMT_V655 0x35353656
#define IMGFMT_CLJR 0x524A4C43
#define IMGFMT_YUVP 0x50565559
#define IMGFMT_UYVP 0x50565955

/* Compressed Formats */
#define IMGFMT_MPEGPES (('M'<<24)|('P'<<16)|('E'<<8)|('S'))
#define IMGFMT_MJPEG (('M')|('J'<<8)|('P'<<16)|('G'<<24))
/* Formats that are understood by zoran chips, we include
 * non-interlaced, interlaced top-first, interlaced bottom-first */
#define IMGFMT_ZRMJPEGNI  (('Z'<<24)|('R'<<16)|('N'<<8)|('I'))
#define IMGFMT_ZRMJPEGIT (('Z'<<24)|('R'<<16)|('I'<<8)|('T'))
#define IMGFMT_ZRMJPEGIB (('Z'<<24)|('R'<<16)|('I'<<8)|('B'))

// I think that this code could not be used by any other codec/format
#define IMGFMT_XVMC 0x1DC70000
#define IMGFMT_XVMC_MASK 0xFFFF0000
#define IMGFMT_IS_XVMC(fmt) (((fmt)&IMGFMT_XVMC_MASK)==IMGFMT_XVMC)
//these are chroma420
#define IMGFMT_XVMC_MOCO_MPEG2 (IMGFMT_XVMC|0x02)
#define IMGFMT_XVMC_IDCT_MPEG2 (IMGFMT_XVMC|0x82)

// VDPAU specific format.
#define IMGFMT_VDPAU               0x1DC80000
#define IMGFMT_VDPAU_MASK          0xFFFF0000
#define IMGFMT_IS_VDPAU(fmt)       (((fmt)&IMGFMT_VDPAU_MASK)==IMGFMT_VDPAU)
#define IMGFMT_VDPAU_MPEG1         (IMGFMT_VDPAU|0x01)
#define IMGFMT_VDPAU_MPEG2         (IMGFMT_VDPAU|0x02)
#define IMGFMT_VDPAU_H264          (IMGFMT_VDPAU|0x03)
#define IMGFMT_VDPAU_WMV3          (IMGFMT_VDPAU|0x04)
#define IMGFMT_VDPAU_VC1           (IMGFMT_VDPAU|0x05)
#define IMGFMT_VDPAU_MPEG4         (IMGFMT_VDPAU|0x06)

#define IMGFMT_VDA_VLD             (('V'<<24)|('D'<<16)|('A'<<8)|('0'))

typedef struct {
    void* data;
    int size;
    int id;        // stream id. usually 0x1E0
    int timestamp; // pts, 90000 Hz counter based
} vo_mpegpes_t;

const char *vo_format_name(int format);
void mp_image_setfmt(mp_image_t* mpi,unsigned int out_fmt);
mp_image_t* new_mp_image(int w,int h);
void free_mp_image(mp_image_t* mpi);
mp_image_t* alloc_mpi(int w, int h, unsigned long int fmt);
void mp_image_alloc_planes(mp_image_t *mpi);
void copy_mpi(mp_image_t *dmpi, mp_image_t *mpi);
int planes_size_mpi(mp_image_t *mpi);

#endif

int audio_ffmpeg_format(int sample_fmt);
char* audio_format_name(int format, char* str, int size);
const char* audio_format_name_short(int format);

typedef struct {
    int valid;
    int streamid;
    int width;
    int height;
    int format;
    double fps;
} video_info_t;

typedef struct {
    int valid;
    int streamid;
    int channels;
    int rate;
    int format;
} audio_info_t;

double xplayer_clock();
void threadtime(int slotid, int tid, double proc, double run);
void plugincall(int slotid, int callid);

/// API

#define STATUS_PLAYER_STARTED           0x0001
#define STATUS_PLAYER_INITED            0x0002
#define STATUS_PLAYER_CONNECT           0x0004
#define STATUS_PLAYER_OPENED            0x0008
#define STATUS_PLAYER_ERROR             0x0010
#define STATUS_WAIT_FOR_FREE_IMAGE      0x0020
#define STATUS_PLAYER_IMAGE             0x0040
#define STATUS_PLAYER_PAUSE             0x0080
#define STATUS_PLAYER_PAUSE_IMG         0x0100
#define STATUS_PLAYER_SEEK              0x0200

#define SLOT_MAX_CACHE                  256

#define SYNC_TYPE_AUDIO                 0
#define SYNC_TYPE_VIDEO                 1
#define SYNC_TYPE_CLOCK                 2

void            xplayer_API_init(int log_level, const char* logfile);
void            xplayer_API_done();
void            xplayer_API_setlogfile(const char* logfile);

void            xplayer_API_setgroup(int slot, int group);
int             xplayer_API_getgroup(int slot);
void            xplayer_API_settimeshift(int slot, double time);
int             xplayer_API_groupplay(int slot);
int             xplayer_API_groupstop(int slot);
int             xplayer_API_seekfinished(int slot);
int             xplayer_API_stepframe(int slot, int frame);
int             xplayer_API_groupseekpos(int slot, double pos);

int             xplayer_API_setimage(int slot, int w, int h, unsigned int fmt);
video_info_t*   xplayer_API_getvideoformat(int slot);
audio_info_t*   xplayer_API_getaudioformat(int slot);
double          xplayer_API_getmovielength(int slot);

int             xplayer_API_newfreeslot();

void            xplayer_API_setloglevel(int slot, int loglevel);
void            xplayer_API_setdebug(int slot, int flag);

void            xplayer_API_slotfree(int slot);
int             xplayer_API_loadurl(int slot, char* url);
char*           xplayer_API_geturl(int slot);
int             xplayer_API_unloadurl(int slot);
int             xplayer_API_enableaudio(int slot, int enable);
int             xplayer_API_play(int slot);
int             xplayer_API_pause(int slot);
int             xplayer_API_pause_step(int slot);
int             xplayer_API_stepnumber(int slot, int step);
int             xplayer_API_stop(int slot);
int             xplayer_API_seek(int slot, double pos);
int             xplayer_API_seekpos(int slot, double pos);
int             xplayer_API_seekrel(int slot, double pos);
int             xplayer_API_flush(int slot);

int             xplayer_API_volume(int slot, int volume);
int             xplayer_API_getvolume(int slot);
int             xplayer_API_mute(int slot, int mute);
int             xplayer_API_getmute(int slot);

int             xplayer_API_getstatus(int slot);
double          xplayer_API_getcurrentpts(int slot);
double          xplayer_API_getrealpts(int slot);
double          xplayer_API_getfps(int slot);

int             xplayer_API_setbuffertime(int slot, double sec);

int             xplayer_API_isnewimage(int slot);
int             xplayer_API_getimage(int slot, mp_image_t** img);
int             xplayer_API_imagedone(int slot);

int             xplayer_API_freeableimage(int slot, mp_image_t** img);
int             xplayer_API_freeimage(int slot, mp_image_t* img);
int             xplayer_API_videoprocessdone(int slot);

int             xplayer_API_isvda(int slot);
int             xplayer_API_getcallplayerstatus(int slot);

int             xplayer_API_getvdaframe(int slot, void** vdaframe);
int             xplayer_API_vdaframedone(int slot);
int             xplayer_API_freeablevdaframe(int slot, void** vdaframe);
int             xplayer_API_freevdaframe(int slot, void* vdaframe);
void*           xplayer_API_vdaframe2cvbuffer(int slot, void* vdaframe);

int             xplayer_API_setvideocodec(int slot, char* name);
int             xplayer_API_setaudiocodec(int slot, char* name);
int             xplayer_API_setsubtitlecodec(int slot, char* name);
int             xplayer_API_setsynctype(int slot, int synctype);
#define FLAG_VDA_NONE        0
#define FLAG_VDA_DECODE      1
#define FLAG_VDA_FRAME       2
int             xplayer_API_setvda(int slot, int vda);
int             xplayer_API_sethwbuffersize(int slot, int size);

int             xplayer_API_setoptions(int slot, const char *opt, const char *arg);

int             xplayer_API_prefillaudio(char* buffer, int bufferlen, int playlen);
int             xplayer_API_prefilllen(void);
int             xplayer_API_getaudio(char* buffer, int bufferlen);
int             xplayer_API_getaudio_rate();
int             xplayer_API_getaudio_channels();
int             xplayer_API_getaudio_format();

char*           xplayer_API_getstatusline(int slot);
void            xplayer_API_freestatusline(char* line);
int             xplayer_API_isbuffering(int slot);
int64_t         xplayer_API_getcurrentframe(int slot);
#ifdef __cplusplus
};
#endif

#endif
