#ifndef int64_t
typedef signed long long int 	int64_t;
#endif

#ifndef __LIBFFPLAY_H__
#define __LIBFFPLAY_H__

#include "eventqueue.h"
#include "libxplayer.h"
//#include "pthread.h"
#include "af.h"

#define LOGSNAME                "libffplay.log"

#define MAX_IMAGES              256
#define MAX_GROUPS              512

#define AUDIO_PREFILL           ((double)0.02)
#define AUDIO_PRELOAD           ((double)0.02)
#define AUDIO_BUFFER_SIZE       ((double)0.04)
#define AUDIO_ITERTIME          ((double)0.01)

#define AUDIO_RATE              48000
#define AUDIO_CHANNELS          2
#define AUDIO_FORMAT            AF_FORMAT_S16_LE

#define BUFFERED_PACKETS        0

#define FF_ALLOC_EVENT          1000
#define FF_REFRESH_EVENT        1001
#define FF_STOP_EVENT           1002
#define FF_QUIT_EVENT           1003
#define TOGGLE_PAUSE_EVENT      1004
#define PAUSE_SET_EVENT         1005
#define PAUSE_CLEAR_EVENT       1006
#define PAUSE_STEP_EVENT        1007
#define SEEK_EVENT              1008
#define SEEK_REL_EVENT          1009
#define QUEUE_FLUSH_EVENT       1010
#define GROUP_PAUSE_CLEAR_EVENT 1011
#define GROUP_PAUSE_SET_EVENT   1012
#define GROUP_SEEK_EVENT        1013
#define STEP_FRAME              1014
#define FF_CLOSE_EVENT          1015

#define INVALID_FRAME           0
#define INVALID_DELAY           1
#define VALID_DELAY             2

#define STATUSLINE_SIZE         1024

#define DEBUGFLAG_STATUS        0x01
#define DEBUGFLAG_READ          0x02
#define DEBUGFLAG_AUDIO_DIFF    0x04
#define DEBUGFLAG_AV            0x08
#define DEBUGFLAG_DELAY         0x10
#define DEBUGFLAG_GROUP         0x20

#define DEFAULT_BUFFER_TIME     0.05
enum {
    AV_SYNC_AUDIO_MASTER = 0, /* default choice */
    AV_SYNC_VIDEO_MASTER,
    AV_SYNC_EXTERNAL_CLOCK, /* synchronize to an external clock */
};
typedef enum {
    SHOW_MODE_NONE = -1, SHOW_MODE_VIDEO = 0, SHOW_MODE_WAVES, SHOW_MODE_RDFT, SHOW_MODE_NB
} ShowMode;

typedef struct slotinfo_st slotinfo_t;

struct slotinfo_st {
    void*               (*fn)(void *);
    void                *data;
    int                 slotid;
    int                 freeable;
    int                 status;
    int                 loglevel;
    int                 debugflag;
    void*               playerpriv;
    void*               streampriv;
    char*               url;
    char*               currenturl;

    int                 groupid;
    double              grouptime;

    void*               eventqueue;

    int                 playflag;
    int                 stopflag;
    int                 pauseflag;
    int                 reopen;
    double              currentpos;
    double              realpos;
    double              length;
    double              fps;
    double              seekpos;
    int                 seekflag;
    int                 volume;
    int                 setvolume;
    int                 mute;
    int                 setmute;
    int                 doneflag;
    int                 pauseafterload;
    int                 pausereq;
    int                 pauseseekreq;

    int                 w;
    int                 h;
    int                 newimageflag;
    int                 imgfmt;

    mp_image_t *        img;
    mp_image_t *        lockimg;
    int                 freeablelockimg;


    video_info_t        video_info;
    audio_info_t        audio_info;

    int                 threadinited;
    pthread_t           thread;
    pthread_mutex_t     mutex;

    void*               af;
    af_data_t*          audio;
    pthread_mutex_t     audiomutex;
    int                 audiolock;

    slotinfo_t*         next;
    slotinfo_t*         prev;

    mp_image_t*         freeable_images[MAX_IMAGES];

    pthread_cond_t      freeable_cond;

    double              buffer_time;

    char*               audio_codec_name;
    char*               subtitle_codec_name;
    char*               video_codec_name;

    int                 codec_imgfmt;
    int                 codec_w;
    int                 codec_h;

    int                 loop;
    int                 display_disable;
    int                 show_status;
    int                 audio_disable;
    int                 video_disable;
    int                 fast;
    int                 genpts;
    int                 lowres;
    int                 av_sync_type;
    int                 workaround_bugs;
    ShowMode            show_mode;
    int                 autoexit;
    int                 rdftspeed;
    int                 framedrop;
    int64_t             start_time;
    int64_t             duration;
    int                 idct;
    int                 decoder_reorder_pts;
    int64_t             audio_callback_time;
    int                 error_concealment;
    int                 error_recognition;
    enum AVDiscard      skip_frame;
    enum AVDiscard      skip_idct;
    enum AVDiscard      skip_loop_filter;
    int                 wanted_stream[AVMEDIA_TYPE_NB];
    int                 audio_hw_buf_size;

/// options
    AVInputFormat*      file_iformat;
    int                 seek_by_bytes;
    int                 set_rtsp_transport;


    struct SwsContext*  sws_opts;
    AVDictionary*       format_opts;
    AVDictionary*       codec_opts;
    void*               optctx;
    void*               opt_def;

    char                statusline[STATUSLINE_SIZE];

    int                 callplayerstatus;
	int                 buffering;
	int                 tbden;
    int64_t             pts;
    double              start_pts;

};

typedef struct {
    int                 used;
    double              master_clock;
    int                 master_slot;
    int                 paused;
    int                 pausestatus;
    pthread_mutex_t     pausemutex;
} group_status_t;

typedef struct {
    slotinfo_t*         slotinfo;
    slotinfo_t*         slotinfo_chache[SLOT_MAX_CACHE];
    group_status_t      groups[MAX_GROUPS];
    pthread_mutex_t     mutex;
    void*               af;
    af_data_t*          audio;
    double              audiopreload;
    double              audioprefill;
    double              audiobuffersize;
    double              itertime;
    int                 run;
    int                 forcevolume;

    pthread_mutex_t     audiomutex;
    int                 threadinited;
    pthread_t           thread;
} xplayer_global_status_t;

typedef struct {
    int format;
    int freq;
    int channels_layout;
    int channels;
} audio_spec_t;



#endif
