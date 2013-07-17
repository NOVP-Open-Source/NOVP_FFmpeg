/*
 * ffplay : Simple Media Player based on the FFmpeg libraries
 * Copyright (c) 2003 Fabrice Bellard
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */


#define THREAD_DEBUG 1
#define DEBUG_DIVIMGSIZE 1
#define NOWAIT_ENDOF_SLOTPROCESS 1
//#define DEBUG_AUDIOPROC 1
#define USE_LOCKIMAGE 1
//#define USE_WAV_DUMP 1
#define NOVOLUMECONTROL 1
#define USE_AUDIOCORR_ZERO 0
#define USE_AUDIOCORR_INETRNPOL 0
//#define DEBUG_AUDIOCORR 1
#define FORCE_ACCURACY 1

#define USES_PAUSESEEK 1

#define DEBUG_FLAG_LOG_LEVEL    32
#define GLOBAL_PAUSE_LIMIT      0.4
#define SEEK_PRECISION          0.04

#include "config.h"
#include "ffmpeg_config.h"
#undef CONFIG_AVFILTER

#include <inttypes.h>
#include <math.h>
#include <limits.h>
#include "libavutil/avstring.h"
#include "libavutil/colorspace.h"
#include "libavutil/mathematics.h"
#include "libavutil/pixdesc.h"
#include "libavutil/imgutils.h"
#include "libavutil/dict.h"
#include "libavutil/parseutils.h"
#include "libavutil/samplefmt.h"
#include "libavutil/avassert.h"
#include "libavformat/avformat.h"
#include "libavdevice/avdevice.h"
#include "libswscale/swscale.h"
#include "libavcodec/audioconvert.h"
#include "libavutil/opt.h"
#include "libavcodec/avfft.h"
#include "libswresample/swresample.h"

#if CONFIG_AVFILTER
# include "libavfilter/avcodec.h"
# include "libavfilter/avfilter.h"
# include "libavfilter/avfiltergraph.h"
# include "libavfilter/buffersink.h"
#endif

#if defined(__APPLE__)
#include "../ffmpeg/libavcodec/vda.h"
static int vdaframeno = 0;
static unsigned int vdaframes_pop = 0;
static unsigned int vdaframes_release = 0;
#endif
static unsigned int mpi_alloc = 0;
static unsigned int mpi_free = 0;

#include <unistd.h>
#include <assert.h>

//#include <pthread.h>

#include "af.h"
#include "afilter/format.h"
#include "afilter/util.h"
#include "libxplayer.h"
#include "afilter/format.h"
#include "fmt-conversion.h"
#include "libavutil/log.h"

#include "debug.h"

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#if HAVE_PTHREADS
#include <pthread.h>
#elif HAVE_W32THREADS
#include "libavcodec/w32pthreads.h"
#elif HAVE_OS2THREADS
#include "libavcodec/os2threads.h"
#endif 

#include "libffplayopts.h"
#include "libffplay.h"

#ifdef __MINGW32__
#undef THREAD_DEBUG
#endif

#ifdef USE_WAV_DUMP
#include "wav.h"
#endif

int loglevel = 0;
char* logfname = NULL;
FILE* logfh = NULL;
FILE* logsfh = NULL;

typedef struct {
    int x;
    int y;
    int w;
    int h;
} rect_t;

int triggermeh = 0;
int dont = 0;
int laserbeams = 0;
int lastret = 0;

static pthread_mutex_t xplayer_global_mutex;
static xplayer_global_status_t * xplayer_global_status = NULL;
void* dmem = NULL;

/// ---------------------------------------------------------------------------
/// ---------------------------------------------------------------------------
/// ---------------------------------------------------------------------------
/// ---------------------------------------------------------------------------

const uint8_t extra_data[] = {
	0x01, 0x42 ,0x00 ,0x29 ,0xff ,0xe1 ,0x00 ,0x14 ,0x67 ,0x42 ,0x00 ,0x29 ,0xe2 ,0x90 ,0x0f ,0x00 ,0x44 ,0xfc ,0xb8 ,0x0b ,0x70 ,0x10 ,0x10 ,0x1a ,0x41 ,0xe2 ,0x44 ,0x54 ,0x01 ,0x00 ,0x04 ,0x68 ,0xce ,0x3c ,0x80};

#undef fprintf
void slog(char *format_str, ...) {
    va_list ap;
    char line[8192];

    if(!logsfh && !logfh) {
        if(!(logsfh=fopen(LOGSNAME,"w+")))
            return;
    }
    va_start(ap,format_str);
    (void)vsnprintf(line,sizeof(line),format_str,ap);
    va_end(ap);
    if(logfh) {
        fprintf(logfh,"%s",line);
        fflush(logfh);
    } else if(logsfh) {
        fprintf(logsfh,"%s",line);
        fflush(logsfh);
    }
}

static void sanitize(uint8_t *line){
    while(*line){
        if(*line < 0x08 || (*line > 0x0D && *line < 0x20))
            *line='?';
        line++;
    }
}

void av_log_callback(void* ptr, int level, const char* fmt, va_list vl)
{

    static int print_prefix = 1;
    static int count;
    static char prev[1024];
    char line[1024];
    char buf[1024];
    time_t ttime;
    struct tm *ptm;

    AVClass* avc = ptr ? *(AVClass **) ptr : NULL;
//    if (level > av_log_get_level())
//        return;
    line[0] = 0;
#undef fprintf
#undef printf
#undef time
    if (print_prefix && avc) {
        if (avc->parent_log_context_offset) {
            AVClass** parent = *(AVClass ***) (((uint8_t *) ptr) +
                                   avc->parent_log_context_offset);
            if (parent && *parent) {
                snprintf(line, sizeof(line), "[%s @ %p] ",
                         (*parent)->item_name(parent), parent);
            }
        }
        snprintf(line + strlen(line), sizeof(line) - strlen(line), "[%s @ %p] ",
                 avc->item_name(ptr), ptr);
    }

    vsnprintf(line + strlen(line), sizeof(line) - strlen(line), fmt, vl);

    print_prefix = strlen(line) && line[strlen(line) - 1] == '\n';

    if (count > 0) {
        fprintf(stderr, "    Last message repeated %d times\n", count);
        count = 0;
    }
    strcpy(prev, line);
    sanitize((uint8_t*)line);
    if(logfh)
    {
        ttime=time(NULL);
        ptm=localtime(&ttime);
        strftime(buf,sizeof(buf),"%Y-%m-%d %H:%M:%S",ptm);
        fprintf(logfh,"%s %s",buf,line);
        fflush(logfh);
    }
    else
    {
        printf("%s",line);
    }
}

void printdebugbuffer(int slotid, char *format_str, ...) {
#ifdef USE_DEBUG_LIB
    va_list ap;
    char msg[512];

    if(dmem && slotid<MAX_DEBUG_SLOT) {
        slotdebug_t* slotdebugs = debugmem_getslot(dmem);
        if(slotdebugs) {
            memset(msg,0,sizeof(msg));
            va_start(ap,format_str);
            (void)vsnprintf(msg,sizeof(msg),format_str,ap);
            va_end(ap);
            snprintf(slotdebugs[slotid].msg,sizeof(slotdebugs[slotid].msg),"%s",msg);
        }
    }
#endif
}

void apicall(int slotid, int callid) {
#ifdef USE_DEBUG_LIB
    if(dmem && slotid<MAX_DEBUG_SLOT) {
        slotdebug_t* slotdebugs = debugmem_getslot(dmem);
        if(slotdebugs) {
            slotdebugs[slotid].apicall=callid;
            if(callid && callid<MAX_DEBUG_APINUM)
            {
                slotdebugs[slotid].proccount[callid]++;
            }
        }
    }
#endif
}

void plugincall(int slotid, int callid) {
#ifdef USE_DEBUG_LIB
    if(dmem && slotid<MAX_DEBUG_SLOT && callid && callid<MAX_DEBUG_APINUM) {
        slotdebug_t* slotdebugs = debugmem_getslot(dmem);
        if(slotdebugs) {
            slotdebugs[slotid].plugincount[callid]++;
        }
    }
#endif
}

void threadtime(int slotid, int tid, double proc, double run) {
#ifdef USE_DEBUG_LIB
    if(dmem && slotid<MAX_DEBUG_SLOT && tid<MAX_DEBUG_THREADS) {
        slotdebug_t* slotdebugs = debugmem_getslot(dmem);
        if(slotdebugs) {
            slotdebugs[slotid].thread_time[tid].proc+=proc;
            slotdebugs[slotid].thread_time[tid].run+=run;
        }
    }
#endif
}

int envloglevel(int loglevel)
{
    char * env;
    if((env=getenv("SPLAYER_LOGLEVEL"))) {
        if(strlen(env)) {
            return atoi(env);
        }
    }
    return loglevel;
}

int envdebugflag(int debugflag)
{
    char * env;
    if((env=getenv("SPLAYER_DEBUGFLAG"))) {
        if(strlen(env)) {
            return atoi(env);
        }
    }
    return debugflag;
}

const char* envtransport(const char* mode)
{
    char * env;
    if((env=getenv("SPLAYER_TRANSPORT"))) {
        if(strlen(env)) {
            return env;
        }
    }
    return mode;
}


static void *player_process(void *data);
static void *audio_process(void *data);
static void global_toggle_pause(slotinfo_t* slotinfo);
static slotinfo_t* get_slot_info(int slot, int alloc, int lock);

static int lockmgr(void **mtx, enum AVLockOp op)
{
   switch(op) {
      case AV_LOCK_CREATE:
          (*mtx) = av_malloc(sizeof(pthread_mutex_t));
          pthread_mutex_init(*mtx, NULL);
          if(!(*mtx)) {
              return 1;
          }
          return 0;
      case AV_LOCK_OBTAIN:
          return !!pthread_mutex_lock(*mtx);;
      case AV_LOCK_RELEASE:
          return !!pthread_mutex_unlock(*mtx);
      case AV_LOCK_DESTROY:
          if(*mtx)
          {
                pthread_mutex_destroy(*mtx);
              av_free(*mtx);
          }
          return 0;
   }
   return 1;
}


static void master_init(int callerid) {
    char* env = NULL;
    int n;

    if(xplayer_global_status) {
        return;
    }
    av_log(NULL, AV_LOG_DEBUG,"[debug] master_init(): callerid=%d \n",callerid);
#ifdef USE_DEBUG_LIB
    dmem=debugmem_open(0);
#endif
    xplayer_global_status = av_malloc(sizeof(xplayer_global_status_t));
    memset(xplayer_global_status,0,sizeof(xplayer_global_status_t));
    pthread_mutex_init(&xplayer_global_mutex, NULL);
    pthread_mutex_init(&xplayer_global_status->mutex, NULL);
    pthread_mutex_init(&xplayer_global_status->audiomutex, NULL);
    xplayer_global_status->audio=af_empty(AUDIO_RATE, AUDIO_CHANNELS, AUDIO_FORMAT, 0, 0);
    xplayer_global_status->audiopreload=AUDIO_PRELOAD;
    xplayer_global_status->audioprefill=AUDIO_PREFILL;
    xplayer_global_status->audiobuffersize=AUDIO_BUFFER_SIZE;
    xplayer_global_status->itertime=AUDIO_ITERTIME;
    if((env=getenv("SPLAYER_FORCEVOLUME")))
    {
        xplayer_global_status->forcevolume=atoi(env);
    }
    for(n=0;n<MAX_GROUPS;n++) {
        xplayer_global_status->groups[n].master_slot=-1;
        pthread_mutex_init(&xplayer_global_status->groups[n].pausemutex, NULL);
    }
    av_log_set_level(loglevel);
    avcodec_register_all();
//#if CONFIG_AVFILTER
    avfilter_register_all();
//#endif
    av_register_all();
    avformat_network_init();
    if (av_lockmgr_register(lockmgr)) {
        av_log(NULL, AV_LOG_ERROR, "[error] master_init(): Could not initialize lock manager!\n");
//        do_exit(NULL);
    }
    pthread_create(&xplayer_global_status->thread, NULL, audio_process, xplayer_global_status);
    xplayer_global_status->threadinited=1;
#ifdef THREAD_DEBUG
    av_log(NULL, AV_LOG_DEBUG,"[debug] master_init(): pthread_create: #159 master 0x%x \n",(unsigned int)&xplayer_global_status->thread);
#endif
}

static void group_update_master(int group)
{
    slotinfo_t* slotinfo;
    slotinfo_t* masterslotinfo = NULL;

    if(!group)
        return;
    if(xplayer_global_status->groups[group].master_slot!=-1) {
        masterslotinfo = get_slot_info(xplayer_global_status->groups[group].master_slot,0,0);
    }
    slotinfo=xplayer_global_status->slotinfo;
    while(slotinfo) {
        if(slotinfo->groupid==group) {
            if(!masterslotinfo) {
                masterslotinfo=slotinfo;
                xplayer_global_status->groups[group].master_slot=slotinfo->slotid;
            } else if(slotinfo->grouptime<masterslotinfo->grouptime) {
                masterslotinfo=slotinfo;
                xplayer_global_status->groups[group].master_slot=slotinfo->slotid;
            }
        }
        slotinfo=(slotinfo_t*)slotinfo->next;
    }
    if(!masterslotinfo) {
        xplayer_global_status->groups[group].master_slot=-1;
    }
    if(masterslotinfo && (masterslotinfo->debugflag & DEBUGFLAG_GROUP)) {
        av_log(NULL,DEBUG_FLAG_LOG_LEVEL,"Group Master: %d \n",xplayer_global_status->groups[group].master_slot);
    }
}

static slotinfo_t* init_slotinfo(int slot) {
    slotinfo_t* slotinfo;

    av_log(NULL, AV_LOG_DEBUG,"[debug] init_slotinfo(): alloc slotinfo: %d\n",slot);
    slotinfo=av_malloc(sizeof(slotinfo_t));
    memset(slotinfo,0,sizeof(slotinfo_t));
    pthread_mutex_init(&slotinfo->mutex, NULL);
    pthread_mutex_init(&slotinfo->audiomutex, NULL);
    pthread_cond_init(&slotinfo->freeable_cond, NULL);
    if(xplayer_global_status->forcevolume)
        slotinfo->volume=xplayer_global_status->forcevolume;
    else
        slotinfo->volume=80;
    slotinfo->w=0;
    slotinfo->h=0;
    slotinfo->imgfmt=0;
    slotinfo->vdaframe=0;
    slotinfo->av_sync_type=AV_SYNC_AUDIO_MASTER;
//    slotinfo->av_sync_type=AV_SYNC_EXTERNAL_CLOCK;
    slotinfo->workaround_bugs=1;
    slotinfo->show_status=1;
    slotinfo->show_mode = SHOW_MODE_NONE;
    slotinfo->rdftspeed=20;
    slotinfo->framedrop=-1;
    slotinfo->start_time = AV_NOPTS_VALUE;
    slotinfo->duration = AV_NOPTS_VALUE;
    slotinfo->idct = FF_IDCT_AUTO;
    slotinfo->decoder_reorder_pts= -1;
    slotinfo->error_concealment = 3;
    slotinfo->skip_frame= AVDISCARD_DEFAULT;
    slotinfo->skip_idct= AVDISCARD_DEFAULT;
    slotinfo->skip_loop_filter= AVDISCARD_DEFAULT;
    slotinfo->wanted_stream[AVMEDIA_TYPE_AUDIO]=-1;
    slotinfo->wanted_stream[AVMEDIA_TYPE_VIDEO]=-1;
    slotinfo->wanted_stream[AVMEDIA_TYPE_SUBTITLE]=-1;
    slotinfo->buffer_time=DEFAULT_BUFFER_TIME;
    memset(slotinfo->statusline,0,STATUSLINE_SIZE);
    slotinfo->loglevel=envloglevel(0);
    slotinfo->debugflag=envdebugflag(0);
    init_options(slotinfo);
    if(slot>=0 && slot<SLOT_MAX_CACHE) {
        xplayer_global_status->slotinfo_chache[slot]=slotinfo;
    }
    slotinfo->slotid=slot;
    xplayer_global_status->groups[0].used++;
    slotinfo->next=xplayer_global_status->slotinfo;
    if(slotinfo->next) {
        slotinfo->next->prev=slotinfo;
    }
    slotinfo->prev=NULL;
    xplayer_global_status->slotinfo=slotinfo;
    group_update_master(0);
    return slotinfo;
}

static slotinfo_t* get_slot_info(int slot, int alloc, int lock) {
    slotinfo_t* slotinfo;

    master_init(1);
    if(lock)
        pthread_mutex_lock(&xplayer_global_status->mutex);
    if(slot>=0 && slot<SLOT_MAX_CACHE) {
        slotinfo = xplayer_global_status->slotinfo_chache[slot];
        if(slotinfo) {
            if(lock)
                pthread_mutex_unlock(&xplayer_global_status->mutex);
            return slotinfo;
        }
    }

    slotinfo=xplayer_global_status->slotinfo;
    while(slotinfo) {
        if(slotinfo->slotid==slot) {
            break;
        }
        slotinfo=(slotinfo_t*)slotinfo->next;
    }
    if(!slotinfo && alloc) {
        slotinfo=init_slotinfo(slot);
    }
    if(lock)
        pthread_mutex_unlock(&xplayer_global_status->mutex);
    return slotinfo;
}

static void updateloglevel(int nolock) {
    slotinfo_t* slotinfo;
    int _loglevel = loglevel;

    master_init(2);
    if(!nolock) {
        pthread_mutex_lock(&xplayer_global_status->mutex);
    }
    slotinfo=xplayer_global_status->slotinfo;
    while(slotinfo) {
        if(slotinfo->loglevel>_loglevel) {
            _loglevel=slotinfo->loglevel;
        }
        slotinfo=(slotinfo_t*)slotinfo->next;
    }
    if(!nolock) {
        pthread_mutex_unlock(&xplayer_global_status->mutex);
    }
    av_log_set_level(_loglevel);
}

static int safestrcmp(char* txt1, char* txt2) {
    if(!txt1 && !txt2)
        return 0;
    if(txt1 && !txt2)
        return 1;
    if(!txt1 && txt2)
        return 1;
    return strcmp(txt1,txt2);
}

//static void audio_append(af_data_t* dest ,unsigned char * audio, int len) {
static void audio_append(af_data_t* dest, af_data_t* src) {
    unsigned char* tempdata;
    int oldlen;
    int oldoffset;
    int srclen;
    int srcoffset;
    int newlen;
    int maxlen;

    if(!dest || !src || !src->audio || !src->len) {
        return;
    }
    maxlen=af_time2len(dest, xplayer_global_status->audiobuffersize);
    srclen=src->len;
    newlen=dest->len+srclen;
    if(newlen>maxlen) {
        newlen=maxlen;
    }
    oldlen=newlen-srclen;
    if(oldlen<0)
        oldlen=0;
    if(srclen>maxlen) {
        srclen=maxlen;
    }
    oldoffset=dest->len-oldlen;
    srcoffset=srclen-src->len;
#ifdef DEBUG_AUDIOPROC
    av_log(NULL, AV_LOG_DEBUG,"[debug] audio_append(): src: %d dst: %d max: %d   old: %d %d + new off: %d new len: %d\n",src->len,dest->len,maxlen,oldoffset,oldlen,srcoffset,srclen);
#endif
    if(srcoffset<0) {
        srcoffset=0;
    }
    tempdata=av_malloc(newlen);
    if(oldlen) {
        memcpy(tempdata,dest->audio+oldoffset,oldlen);
    }
    memcpy(tempdata+oldlen,src->audio+srcoffset,srclen);
    av_free(dest->audio);
    dest->audio=tempdata;
    dest->len=oldlen+srclen;
}

static af_data_t* read_audio_data(void *opaque, af_data_t* basedata, int len, double ctime);
static void free_freeable_slot();

void audiodebugpass(int pass) {
#ifdef USE_DEBUG_LIB
    if(dmem) {
        maindebug_t* maindebug = debugmem_getmain(dmem);
        if(maindebug) {
            maindebug->pass=pass;
        }
    }
#endif
}

void audiodebugbuffer(unsigned int len) {
#ifdef USE_DEBUG_LIB
    if(dmem) {
        maindebug_t* maindebug = debugmem_getmain(dmem);
        if(maindebug) {
            maindebug->audio_buffer=len;
        }
    }
#endif
}

void audiodebugslot(int slotid) {
#ifdef USE_DEBUG_LIB
    if(dmem) {
        maindebug_t* maindebug = debugmem_getmain(dmem);
        if(maindebug) {
            maindebug->audio_slot=slotid;
        }
    }
#endif
}

int globalpause(slotinfo_t* slotinfo) {
    return xplayer_global_status->groups[slotinfo->groupid].paused;
}

static void *audio_process(void *data) {
    void* af = NULL;
    void* afnorm = NULL;
    slotinfo_t* slotinfo;
    af_data_t* af_data;
    af_data_t* anormdata;
    af_data_t* adata;
    double systime;
    double systimenext;
    double systimediff=0.0;
    double ftime;
    double ptimes=0.0;
    int ml,alen;
    int iterbytes;
    int len;
    int n;
#ifdef USE_WAV_DUMP
    unsigned int wavsize = 0;
    FILE* fh = NULL;
#endif

#ifdef USE_DEBUG_LIB
    double stime = 0.0;
    double etime = 0.0;
    double ltime = 0.0;
#endif

    xplayer_global_status->run=1;
    systime=xplayer_clock();
    alen=xplayer_global_status->audio->nch*format_2_bps(xplayer_global_status->audio->format)*xplayer_global_status->audio->rate;
    iterbytes=af_time2len(xplayer_global_status->audio,xplayer_global_status->itertime);

#ifdef USE_WAV_DUMP
    fh = fopen("/tmp/audiodump.wav","w+");
    if(fh) {
        write_wav_header(fh, xplayer_global_status->audio->rate, xplayer_global_status->audio->nch, 16, 0);
    }
#endif
    af=af_init(xplayer_global_status->audio->rate, xplayer_global_status->audio->nch, xplayer_global_status->audio->format, xplayer_global_status->audio->bps, 0.0);
    afnorm=af_init(xplayer_global_status->audio->rate, xplayer_global_status->audio->nch, AF_FORMAT_FLOAT_NE, xplayer_global_status->audio->bps, 0.0);
    while (xplayer_global_status->run) {
#ifdef USE_DEBUG_LIB
        if(dmem) {
            maindebug_t* maindebug = debugmem_getmain(dmem);
            if(maindebug) {
                maindebug->audio_proc++;
                maindebug->audio_slot=-1;
                maindebug->mpi_alloc=mpi_alloc;
                maindebug->mpi_free=mpi_free;
#if defined(__APPLE__)
                maindebug->vdaframes_pop=vdaframes_pop;
                maindebug->vdaframes_release=vdaframes_release;
#endif
            }
        }
#endif
        audiodebugslot(-1);

        systimenext=xplayer_clock();
        ftime=ml=af_time2len(xplayer_global_status->audio, systimenext-systime+systimediff);
        systimediff+=(systimenext-systime)-af_len2time(xplayer_global_status->audio,ml);

        ftime/=alen;
        ml=af_round_len(xplayer_global_status->audio,ml);
        if(ml>=iterbytes) {
            ptimes=xplayer_clock();
            audiodebugpass(1);
//            af_data=af_emptyfromdata(xplayer_global_status->audio, ml);
//            af_data=af_empty(xplayer_global_status->audio->rate, xplayer_global_status->audio->nch, AUDIO_FORMAT, 0.0, ml);
            af_data=af_empty(xplayer_global_status->audio->rate, xplayer_global_status->audio->nch, AF_FORMAT_FLOAT_NE, 0, ml*2);
            pthread_mutex_lock(&xplayer_global_status->audiomutex);
            slotinfo=xplayer_global_status->slotinfo;
            while(slotinfo) {
                audiodebugslot(slotinfo->slotid);
                audiodebugpass(2);
                pthread_mutex_lock(&slotinfo->audiomutex);
                if(!slotinfo->audiolock)
                {
                    len=ml;
                    if(slotinfo->streampriv)
                    {
                        audiodebugpass(3);
                        af_data=read_audio_data(slotinfo->streampriv, af_data, len, ptimes);
                        audiodebugpass(4);
                    }
                }
                audiodebugpass(5);
                pthread_mutex_unlock(&slotinfo->audiomutex);
                audiodebugpass(6);
                slotinfo=(slotinfo_t*)slotinfo->next;
            }
            if(afnorm) {
                anormdata=af_play(afnorm, af_data);
            } else
                anormdata=af_data;
            if(af)
                adata=af_play(af, anormdata);
            else
                adata=anormdata;
#ifdef USE_WAV_DUMP
            if(fh) {
                fwrite(adata->audio,1,adata->len,fh);
                wavsize+=adata->len;
            }
#endif
            audio_append(xplayer_global_status->audio, adata);
            af_data_free(af_data);
            pthread_mutex_unlock(&xplayer_global_status->audiomutex);
            audiodebugbuffer(xplayer_global_status->audio->len);
            audiodebugpass(7);
#ifdef DEBUG_AUDIOPROC
            av_log(NULL, AV_LOG_DEBUG,"[debug] audio_process(): %d bytes buffer size: %d\n",ml,xplayer_global_status->audio->len);
#endif
#ifdef NOWAIT_ENDOF_SLOTPROCESS
            free_freeable_slot();
#endif
            for(n=0;n<MAX_GROUPS;n++) {
                xplayer_global_status->groups[n].pausestatus=0;
            }
            slotinfo=xplayer_global_status->slotinfo;
            while(slotinfo) {
                if(slotinfo->pausereq || slotinfo->pauseseekreq) {
                    xplayer_global_status->groups[slotinfo->groupid].pausestatus=1;
                }
#ifdef USE_DEBUG_LIB
                if(dmem && slotinfo->slotid<MAX_DEBUG_SLOT) {
                    slotdebug_t* slotdebugs = debugmem_getslot(dmem);
                    if(slotdebugs) {
                        slotdebugs[slotinfo->slotid].pausereq=slotinfo->pausereq|(slotinfo->pausereq<<1)|(slotdebugs[slotinfo->slotid].pausereq & 0x04);
                    }
                }
#endif
                slotinfo=(slotinfo_t*)slotinfo->next;
            }
            slotinfo=xplayer_global_status->slotinfo;
            while(slotinfo) {
                if(xplayer_global_status->groups[slotinfo->groupid].pausestatus!=xplayer_global_status->groups[slotinfo->groupid].paused) {
                    xplayer_global_status->groups[slotinfo->groupid].paused=xplayer_global_status->groups[slotinfo->groupid].pausestatus;
                    global_toggle_pause(slotinfo);
                }
#ifdef USE_DEBUG_LIB
                if(dmem && slotinfo->slotid<MAX_DEBUG_SLOT) {
                    slotdebug_t* slotdebugs = debugmem_getslot(dmem);
                    if(slotdebugs) {
                        slotdebugs[slotinfo->slotid].groupid=slotinfo->groupid;
                        slotdebugs[slotinfo->slotid].group_clock=xplayer_global_status->groups[slotinfo->groupid].master_clock;
                        slotdebugs[slotinfo->slotid].group_paused=xplayer_global_status->groups[slotinfo->groupid].paused;
                    }
                }
#endif
                slotinfo=(slotinfo_t*)slotinfo->next;
            }
            systime=systimenext;
        } else {
            ml=(xplayer_global_status->itertime-ptimes+xplayer_clock())*1000000.0;
#ifdef USE_DEBUG_LIB
            if(etime>0.0 && stime>0.0) {
                if(dmem) {
                    maindebug_t* maindebug = debugmem_getmain(dmem);
                    if(maindebug) {
                        maindebug->thread_time.proc+=etime-stime;
                        maindebug->thread_time.run+=etime-ltime;
                    }
                }
            }
#endif
            if(ml<30000 && ml>100)
                usleep(ml);
            else
                usleep(100);
#ifdef USE_DEBUG_LIB
            stime=xplayer_clock();
#endif
        }
        if(!xplayer_global_status->run)
        {
            break;
        }
    }
    if(af) {
        af_uninit(af);
    }
    if(afnorm) {
        af_uninit(afnorm);
    }
#ifdef USE_WAV_DUMP
    if(fh) {
        update_wav_header(fh, wavsize);
        fclose(fh);
    }
#endif
    return NULL;
}

static void add_freeable_image(slotinfo_t* slotinfo, mp_image_t* img) {
#ifndef USE_LOCKIMAGE
    int exist = 0;
#endif

    pthread_mutex_lock(&slotinfo->mutex);
#ifdef USE_LOCKIMAGE
    if(img) {
        if(slotinfo->lockimg==img) {
            slotinfo->freeablelockimg=1;
        } else {
            if(slotinfo->img==img)
                slotinfo->img=NULL;
            free_mp_image(img);
            mpi_free++;
        }
    }
#else
    for(i=0;i<MAX_IMAGES;i++) {
        if(!slotinfo->freeable_images[i] && (empty == -1) ) {
            empty=i;
        }
        if(slotinfo->freeable_images[i]==img) {
            exist = 1;
            break;
        }
    }
    if(!exist && empty!=-1) {
        slotinfo->freeable_images[empty]=img;
    }
#endif
    pthread_mutex_unlock(&slotinfo->mutex);
}

static void wait_for_free_image(slotinfo_t* slotinfo) {
    int i;
    int empty = 1;

    pthread_mutex_lock(&slotinfo->mutex);
    av_log(NULL, AV_LOG_DEBUG,"[debug] wait_for_free_image(): slot: %d ****************************\n\n",slotinfo->slotid);
    while(1) {
        empty=1;
        for(i=0;i<MAX_IMAGES;i++) {
            if(slotinfo->freeable_images[i]) {
                empty=0;
                break;
            }
            if(slotinfo->freeable_vdaframe[i]) {
                empty=0;
                break;
            }
        }
        if(empty)
            break;
        av_log(NULL, AV_LOG_DEBUG,"[debug] wait_for_free_image(): slot: %d Wait ****************************\n\n",slotinfo->slotid);
        pthread_cond_wait(&slotinfo->freeable_cond, &slotinfo->mutex);
    }
    av_log(NULL, AV_LOG_DEBUG,"[debug] wait_for_free_image(): slot: %d Ok ****************************\n\n",slotinfo->slotid);
    pthread_mutex_unlock(&slotinfo->mutex);
}

static void add_freeable_vdaframe(slotinfo_t* slotinfo, void* vdaframe, int nolock) {
#ifndef USE_LOCKIMAGE
    int exist = 0;
    int empty = -1;
    int i;
#endif

    if(!nolock)
        pthread_mutex_lock(&slotinfo->mutex);
#ifdef USE_LOCKIMAGE
    if(vdaframe) {
        if(slotinfo->lockvdaframe!=vdaframe) {
            if(slotinfo->vdaframe==vdaframe)
                slotinfo->vdaframe=NULL;
#if defined(__APPLE__)
            vdaframeno--;
            vdaframes_release++;
            ff_vda_release_vda_frame(vdaframe);
#endif
        }
    }
#else
    for(i=0;i<MAX_IMAGES;i++) {
        if(!slotinfo->freeable_vdaframe[i] && (empty == -1) ) {
            empty=i;
        }
        if(slotinfo->freeable_vdaframe[i]==vdaframe) {
            exist = 1;
            break;
        }
    }
    if(!exist && empty!=-1) {
        slotinfo->freeable_vdaframe[empty]=vdaframe;
    }
#endif
    if(!nolock)
        pthread_mutex_unlock(&slotinfo->mutex);
}

void xplayer_API_init(int log_level, const char* logfile) {

    xplayer_API_setlogfile(logfile);
    loglevel = envloglevel(loglevel);
    av_log_set_level(loglevel);
    master_init(3);
}

int64_t xplayer_API_getcurrentframe(int slot) {
    slotinfo_t* slotinfo = get_slot_info(slot,1,1);

    int64_t currentframe;
    double tmp;

    apicall(slot, 8);
    pthread_mutex_lock(&slotinfo->mutex);
    tmp = (double)slotinfo->fps;
    tmp/= (double)slotinfo->tbden;
    tmp*= (double)(slotinfo->pts - slotinfo->start_pts);
    currentframe = (int64_t)tmp;

    pthread_mutex_unlock(&slotinfo->mutex);
    apicall(slot, 0);
    return currentframe;

}

void xplayer_API_done() {
    slotinfo_t* slotinfo;
    slotinfo_t* nextslotinfo;
    event_t event;
    int n, oldgroup;

#ifdef DEBUG_DONE
    av_log(NULL, AV_LOG_DEBUG,"[debug] xplayer_API_done(): start ...\n");
#endif
    loglevel = 99;
    av_log_set_level(loglevel);

    if(xplayer_global_status) {
        pthread_mutex_lock(&xplayer_global_mutex);
        if(!xplayer_global_status) {
            return;
        }
        pthread_mutex_lock(&xplayer_global_status->mutex);
        slotinfo=xplayer_global_status->slotinfo;
        xplayer_global_status->run=0;
        while(slotinfo) {
            slotinfo->doneflag=1;
            event.type = FF_STOP_EVENT;
            event.data = slotinfo->streampriv;
            push_event(slotinfo->eventqueue, &event);
#ifdef DEBUG_DONE
            av_log(NULL, AV_LOG_DEBUG,"[debug] xplayer_API_done(): Wait for done slot: %d\n",slotinfo->slotid);
#endif
            if(slotinfo->threadinited)
                pthread_join(slotinfo->thread, NULL);
            slotinfo->threadinited=0;
#ifdef DEBUG_DONE
            av_log(NULL, AV_LOG_DEBUG,"[debug] xplayer_API_done(): Done slot: %d\n",slotinfo->slotid);
#endif
            nextslotinfo=slotinfo->next;
#ifdef USE_DEBUG_LIB
            if(dmem && slotinfo->slotid<MAX_DEBUG_SLOT) {
                slotdebug_t* slotdebugs = debugmem_getslot(dmem);
                if(slotdebugs) {
                    slotdebugs[slotinfo->slotid].uses=0;
                    memset(&slotdebugs[slotinfo->slotid],0,sizeof(slotdebug_t));
                }
            }
#endif
            uninit_eventqueue(slotinfo->eventqueue);
            if(slotinfo->af)
                af_uninit(slotinfo->af);
            slotinfo->af=NULL;
#ifdef DEBUG_DONE
            av_log(NULL, AV_LOG_DEBUG,"[debug] xplayer_API_done(): av_free slot: %d\n",slotinfo->slotid);
#endif
            oldgroup=slotinfo->groupid;
            xplayer_global_status->groups[oldgroup].used--;
            slotinfo->groupid=-1;
            if(xplayer_global_status->groups[oldgroup].master_slot==slotinfo->slotid)
                group_update_master(oldgroup);
            av_free(slotinfo);
            slotinfo=nextslotinfo;
        }
        pthread_mutex_unlock(&xplayer_global_status->mutex);
        pthread_mutex_destroy(&xplayer_global_status->mutex);
#ifdef DEBUG_DONE
        av_log(NULL, AV_LOG_DEBUG,"[debug] xplayer_API_done(): Wait for global thread...\n");
#endif
        if(xplayer_global_status->threadinited)
             pthread_join(xplayer_global_status->thread, NULL);
         xplayer_global_status->threadinited=0;
#ifdef DEBUG_DONE
        av_log(NULL, AV_LOG_DEBUG,"[debug] xplayer_API_done(): Done global thread...\n");
#endif
        xplayer_global_status->audio=af_data_free(xplayer_global_status->audio);
        pthread_mutex_destroy(&xplayer_global_status->audiomutex);
        for(n=0;n<MAX_GROUPS;n++) {
            pthread_mutex_destroy(&xplayer_global_status->groups[n].pausemutex);
        }
        av_free(xplayer_global_status);
        xplayer_global_status=NULL;

#ifdef DEBUG_DONE
        av_log(NULL, AV_LOG_DEBUG,"[debug] xplayer_API_done(): Call av_lockmgr_register()...\n");
#endif
        av_lockmgr_register(NULL);
#if CONFIG_AVFILTER
        avfilter_uninit();
#endif
#ifdef DEBUG_DONE
        av_log(NULL, AV_LOG_DEBUG,"[debug] xplayer_API_done(): Call avformat_network_deinit()...\n");
#endif
        avformat_network_deinit();
        pthread_mutex_unlock(&xplayer_global_mutex);
        pthread_mutex_destroy(&xplayer_global_mutex);
    }
#ifdef USE_DEBUG_LIB
    if(dmem) {
        debugmem_close(dmem);
    }
    dmem=NULL;
#endif
#ifdef DEBUG_DONE
    av_log(NULL, AV_LOG_DEBUG,"[debug] xplayer_API_done(): end.\n");
#endif
    logfname=NULL;
    if(logfh)
        fclose(logfh);
    logfh=NULL;
    if(logsfh)
        fclose(logsfh);
    logsfh=NULL;
}

void xplayer_API_setlogfile(const char* logfile)
{
    char fnamenew[8192];
    char fnameold[8192];
    int i;
    struct stat st;

//    if(!logfile || !strlen(logfile))
//        return;
    if(!logfile || !strlen(logfile))
    {
        if(!logfname)
        {
            return;
        }
        logfname=NULL;
        if(logfh)
            fclose(logfh);
        logfh=NULL;
        av_log_set_callback(av_log_default_callback);
        return;
    }
    if(logfname && !strcmp(logfname,logfile))
    {
        return;
    }
    logfname=av_strdup(logfile);
    if(stat(logfname,&st)!=-1 && S_ISREG(st.st_mode)) {
        snprintf(fnameold,sizeof(fnameold),"%s.%d",logfname,5);
        unlink(fnameold);
        for(i=4;i>=0;i--) {
            if(i)
                snprintf(fnameold,sizeof(fnameold),"%s.%d",logfname,i);
            else
                snprintf(fnameold,sizeof(fnameold),"%s",logfname);
            snprintf(fnamenew,sizeof(fnamenew),"%s.%d",logfname,i+1);
            rename(fnameold,fnamenew);
        }
    }
    logfh=fopen(logfname,"w+");
    av_log_set_callback(av_log_callback);
}

static void free_freeable_slot() {
    int slot;
    slotinfo_t* slotinfo;
    slotinfo_t* nextslotinfo;
    slotinfo_t* next;
    slotinfo_t* prev;
    int freeslot = 0;
    int oldgroup;

    if(xplayer_global_status) {
        pthread_mutex_lock(&xplayer_global_status->mutex);
        slotinfo=xplayer_global_status->slotinfo;
        while(slotinfo) {
            nextslotinfo=slotinfo->next;
            slot=slotinfo->slotid;
            if(slotinfo->freeable)
            {
#ifdef USE_DEBUG_LIB
                if(dmem && slotinfo->slotid<MAX_DEBUG_SLOT) {
                    slotdebug_t* slotdebugs = debugmem_getslot(dmem);
                    if(slotdebugs) {
                        slotdebugs[slotinfo->slotid].uses=0;
                        memset(&slotdebugs[slotinfo->slotid],0,sizeof(slotdebug_t));
                    }
                }
#endif
                next=slotinfo->next;
                prev=slotinfo->prev;
                if(next) {
                    next->prev=prev;
                }
                if(prev) {
                    prev->next=next;
                }
                if(xplayer_global_status->slotinfo==slotinfo)
                    xplayer_global_status->slotinfo=next;
                if(slot>=0 && slot<SLOT_MAX_CACHE) {
                    xplayer_global_status->slotinfo_chache[slot]=NULL;
                }
                uninit_eventqueue(slotinfo->eventqueue);
                if(slotinfo->af)
                    af_uninit(slotinfo->af);
                slotinfo->af=NULL;
                av_log(NULL, AV_LOG_DEBUG,"[debug] free_freeable_slot(): slot: %d\n",slot);
                oldgroup=slotinfo->groupid;
                xplayer_global_status->groups[oldgroup].used--;
                slotinfo->groupid=-1;
                if(xplayer_global_status->groups[oldgroup].master_slot==slotinfo->slotid)
                    group_update_master(oldgroup);
                pthread_mutex_destroy(&slotinfo->mutex);
                pthread_cond_destroy(&slotinfo->freeable_cond);
                av_free(slotinfo);
                freeslot=1;
            }
            slotinfo=nextslotinfo;
        }
        if(freeslot) {
            updateloglevel(1);
        }
        pthread_mutex_unlock(&xplayer_global_status->mutex);
    }
}

int xplayer_API_newfreeslot()
{
    int slot = -1;
    slotinfo_t* slotinfo;
    int i;
    master_init(4);

    pthread_mutex_lock(&xplayer_global_status->mutex);
    for(i=0;i<SLOT_MAX_CACHE;i++) {
         if(!xplayer_global_status->slotinfo_chache[i])
        {
            slot=i;
            av_log(NULL, AV_LOG_DEBUG,"[debug] xplayer_API_newfreeslot(): found free slot: %d\n",slot);
            break;
        }
    }
    if(slot==-1) {
        slotinfo=xplayer_global_status->slotinfo;
        i=-1;
        while(slotinfo) {
            if(slotinfo->slotid>i) {
                i=slotinfo->slotid;
            }
            slotinfo=(slotinfo_t*)slotinfo->next;
        }
        slot=i+1;
        av_log(NULL, AV_LOG_DEBUG,"[debug] xplayer_API_newfreeslot(): found next slot: %d\n",slot);
    }
    init_slotinfo(slot);
    pthread_mutex_unlock(&xplayer_global_status->mutex);
    return slot;
}

void xplayer_API_slotfree(int slot) {
    slotinfo_t* slotinfo = get_slot_info(slot,0,1);
    slotinfo_t* next;
    slotinfo_t* prev;
    int oldgroup;

    if(!slotinfo)
        return;
    pthread_mutex_lock(&slotinfo->mutex);
    if(slotinfo->status)
        slotinfo->status=0;
    pthread_mutex_unlock(&slotinfo->mutex);
#ifdef NOWAIT_ENDOF_SLOTPROCESS
    return;
#endif
//    pthread_mutex_lock(&xplayer_global_status->mutex);
    if(slotinfo->threadinited)
        pthread_join(slotinfo->thread, NULL);
    slotinfo->threadinited=0;
    pthread_mutex_lock(&xplayer_global_status->mutex);
    next=slotinfo->next;
    prev=slotinfo->prev;
    if(next) {
        next->prev=prev;
    }
    if(prev) {
        prev->next=next;
    }
    if(xplayer_global_status->slotinfo==slotinfo)
        xplayer_global_status->slotinfo=next;
    if(slot>=0 && slot<SLOT_MAX_CACHE) {
        xplayer_global_status->slotinfo_chache[slot]=NULL;
    }
#ifdef USE_DEBUG_LIB
    if(dmem && slotinfo->slotid<MAX_DEBUG_SLOT) {
        slotdebug_t* slotdebugs = debugmem_getslot(dmem);
        if(slotdebugs) {
            slotdebugs[slotinfo->slotid].uses=0;
            memset(&slotdebugs[slotinfo->slotid],0,sizeof(slotdebug_t));
        }
    }
#endif
    if(slotinfo->af)
        af_uninit(slotinfo->af);
    slotinfo->af=NULL;
    oldgroup=slotinfo->groupid;
    xplayer_global_status->groups[oldgroup].used--;
    slotinfo->groupid=-1;
    if(xplayer_global_status->groups[oldgroup].master_slot==slotinfo->slotid)
        group_update_master(oldgroup);
    av_free(slotinfo);
    updateloglevel(1);
    pthread_mutex_unlock(&xplayer_global_status->mutex);
}

void xplayer_API_setloglevel(int slot, int loglevel) {
    slotinfo_t* slotinfo = get_slot_info(slot,1,1);

    slotinfo->loglevel = envloglevel(loglevel);
    updateloglevel(0);
}

void xplayer_API_setdebug(int slot, int flag) {
    slotinfo_t* slotinfo = get_slot_info(slot,1,1);

    slotinfo->debugflag = envdebugflag(flag);
}

void xplayer_API_setgroup(int slot, int group) {
    slotinfo_t* slotinfo = get_slot_info(slot,1,1);
    int oldgroup,n;

    if((slotinfo->debugflag & DEBUGFLAG_GROUP)) {
        av_log(NULL,DEBUG_FLAG_LOG_LEVEL,"Slot: %d SetGroupId(%d)\n",slot,group);
    }
    if(group>MAX_GROUPS || slotinfo->groupid==group)
        return;
    oldgroup=slotinfo->groupid;
    pthread_mutex_lock(&xplayer_global_status->mutex);
    if(group<0) {
        for(n=0;n<MAX_GROUPS;n++) {
            if(!xplayer_global_status->groups[n].used) {
                if((slotinfo->debugflag & DEBUGFLAG_GROUP)) {
                    av_log(NULL,DEBUG_FLAG_LOG_LEVEL,"Slot: %d SetGroupId(%d) new group: %d\n",slot,group,n);
                }
                group=n;
                break;
            }
        }
        if(group<0) {
            group=0;
        }
        if(slotinfo->groupid==group) {
            pthread_mutex_unlock(&xplayer_global_status->mutex);
            return;
        }
    }
    xplayer_global_status->groups[oldgroup].used--;
    slotinfo->groupid=group;
    if(xplayer_global_status->groups[oldgroup].master_slot==slot) {
        group_update_master(oldgroup);
    }
    xplayer_global_status->groups[group].used++;
    group_update_master(group);
    pthread_mutex_unlock(&xplayer_global_status->mutex);
}

int xplayer_API_getgroup(int slot) {
    slotinfo_t* slotinfo = get_slot_info(slot,1,1);

    return slotinfo->groupid;
}

void xplayer_API_settimeshift(int slot, double time) {
    slotinfo_t* slotinfo = get_slot_info(slot,1,1);

    if((slotinfo->debugflag & DEBUGFLAG_GROUP)) {
        av_log(NULL,DEBUG_FLAG_LOG_LEVEL,"SetTimeShift: %d %8.3f (%d)\n",slot,time,slotinfo->groupid);
    }
    slotinfo->grouptime=time/1000.0;
    pthread_mutex_lock(&xplayer_global_status->mutex);
    group_update_master(slotinfo->groupid);
    pthread_mutex_unlock(&xplayer_global_status->mutex);
}


video_info_t* xplayer_API_getvideoformat(int slot) {
    slotinfo_t* slotinfo = get_slot_info(slot,1,1);
    if(!slotinfo->status) {
        return NULL;
    }
    video_info_t* ret = av_malloc(sizeof(video_info_t));
    memcpy(ret,(char*)&(slotinfo->video_info),sizeof(video_info_t));
    return ret;
}

audio_info_t* xplayer_API_getaudioformat(int slot) {
    slotinfo_t* slotinfo = get_slot_info(slot,1,1);
    if(!slotinfo->status) {
        return NULL;
    }
    audio_info_t* ret = av_malloc(sizeof(audio_info_t));
    memcpy(ret,(char*)&(slotinfo->audio_info),sizeof(audio_info_t));
    return ret;
}

double xplayer_API_getmovielength(int slot) {
    slotinfo_t* slotinfo = get_slot_info(slot,1,1);
    double length = 0.0;

    if(!slotinfo->status) {
        return length;
    }
    apicall(slot, 1);
    pthread_mutex_lock(&slotinfo->mutex);
    if(slotinfo->url && slotinfo->length>0.0) {
        length = slotinfo->length;
    }
    pthread_mutex_unlock(&slotinfo->mutex);
    apicall(slot, 0);
    return length;
}

int xplayer_API_setimage(int slot, int w, int h, unsigned int fmt) {
    slotinfo_t* slotinfo = get_slot_info(slot,1,1);

    apicall(slot, 2);
    pthread_mutex_lock(&slotinfo->mutex);
    slotinfo->w=w;
    slotinfo->h=h;
    slotinfo->imgfmt=fmt;
    pthread_mutex_unlock(&slotinfo->mutex);
    apicall(slot, 0);
    av_log(NULL, AV_LOG_DEBUG,"[debug] xplayer_API_setimage(): %dx%d %s\n",w,h,vo_format_name(fmt));
    return 0;
}

int xplayer_API_setvda(int slot, int vda) {
    slotinfo_t* slotinfo = get_slot_info(slot,1,1);

    apicall(slot, 3);
    pthread_mutex_lock(&slotinfo->mutex);
    slotinfo->vda=vda;
    pthread_mutex_unlock(&slotinfo->mutex);
    apicall(slot, 0);
    return 0;
}

int xplayer_API_isvda(int slot) {
    slotinfo_t* slotinfo = get_slot_info(slot,1,1);
    int ret = 0;

    apicall(slot, 4);
    pthread_mutex_lock(&slotinfo->mutex);
    ret = slotinfo->usesvda;
    pthread_mutex_unlock(&slotinfo->mutex);
    apicall(slot, 0);
    return ret;
}

int xplayer_API_enableaudio(int slot, int enable) {
    slotinfo_t* slotinfo = get_slot_info(slot,1,1);

    av_log(NULL, AV_LOG_DEBUG,"[debug] xplayer_API_enableaudio(): %d slot: %d\n",enable,slot);
    apicall(slot, 5);
    pthread_mutex_lock(&slotinfo->mutex);
    slotinfo->audio_disable=!enable;
    pthread_mutex_unlock(&slotinfo->mutex);
    apicall(slot, 0);
    return 0;
}

int xplayer_API_setbuffertime(int slot, double sec) {
    slotinfo_t* slotinfo = get_slot_info(slot,1,1);

    apicall(slot, 6);
    pthread_mutex_lock(&slotinfo->mutex);
    slotinfo->buffer_time=sec;
    pthread_mutex_unlock(&slotinfo->mutex);
    apicall(slot, 0);
    return 0;
}

int xplayer_API_loadurl(int slot, char* url) {
    slotinfo_t* slotinfo = get_slot_info(slot,1,1);
    event_t event;

    slotinfo->callplayerstatus = 0;

    if(!url) {
        return -1;
    }
    if(!slotinfo->set_rtsp_transport) {
        xplayer_API_setoptions(slot, "rtsp_transport", envtransport("http"));
        slotinfo->set_rtsp_transport=0;
    }
    apicall(slot, 7);
    pthread_mutex_lock(&slotinfo->mutex);
    if(slotinfo->url && url && strcmp(slotinfo->url,url)) {
        if(slotinfo->status) {
            event.type = FF_STOP_EVENT;
            event.data = slotinfo->streampriv;
            push_event(slotinfo->eventqueue, &event);
        }
    }
    if(slotinfo->url)
        av_free(slotinfo->url);
    slotinfo->url=av_strdup(url);
    if(url) {
        slotinfo->playflag=2;
        slotinfo->pauseafterload=1;
        slotinfo->reopen=0;
    }
    if(!slotinfo->status && !slotinfo->threadinited) {
        pthread_create(&slotinfo->thread, NULL, player_process, slotinfo);
        slotinfo->threadinited=1;
#ifdef THREAD_DEBUG
        av_log(NULL, AV_LOG_DEBUG,"[debug] xplayer_API_loadurl(): pthread_create: #691 slot: %d 0x%x \n",slotinfo->slotid,(unsigned int)slotinfo->thread);
#endif
    }
    pthread_mutex_unlock(&slotinfo->mutex);
    apicall(slot, 0);
    return 0;
}

char* xplayer_API_geturl(int slot) {
    slotinfo_t* slotinfo = get_slot_info(slot,1,1);
    char* url = NULL;

    apicall(slot, 8);
    pthread_mutex_lock(&slotinfo->mutex);
    if(slotinfo->status) {
        url=slotinfo->url;
    }
    pthread_mutex_unlock(&slotinfo->mutex);
    apicall(slot, 0);
    return url;
}

int xplayer_API_unloadurl(int slot) {
    slotinfo_t* slotinfo = get_slot_info(slot,1,1);
    int ret = -1;
    event_t event;

    apicall(slot, 9);
    pthread_mutex_lock(&slotinfo->mutex);
    slotinfo->playflag=0;
    slotinfo->pauseflag=0;
    slotinfo->pauseafterload=0;
    if(slotinfo->url && slotinfo->status) {
        av_free(slotinfo->url);
        slotinfo->url=NULL;
        event.type = FF_STOP_EVENT;
        event.data = slotinfo->streampriv;
        push_event(slotinfo->eventqueue, &event);
        ret=0;
    }
    pthread_mutex_unlock(&slotinfo->mutex);
    apicall(slot, 0);
    return ret;
}

int xplayer_API_play(int slot) {
    slotinfo_t* slotinfo = get_slot_info(slot,1,1);
    int ret = -1;
    event_t event;

    if((slotinfo->debugflag & DEBUGFLAG_GROUP)) {
        av_log(NULL,DEBUG_FLAG_LOG_LEVEL,"API_Play(%d)\n",slot);
    }
    apicall(slot, 10);
    pthread_mutex_lock(&slotinfo->mutex);
    slotinfo->pauseafterload=0;
#ifdef USE_DEBUG_LIB
    if(dmem && slot<MAX_DEBUG_SLOT) {
        slotdebug_t* slotdebugs = debugmem_getslot(dmem);
        if(slotdebugs) {
            slotdebugs[slot].playtime=xplayer_clock();
        }
    }
#endif
    if(slotinfo->url) {
        if(safestrcmp(slotinfo->currenturl,slotinfo->url)) {
            slotinfo->pauseflag=0;
            event.type = FF_STOP_EVENT;
            event.data = slotinfo->streampriv;
            push_event(slotinfo->eventqueue, &event);
        } else if(slotinfo->pauseflag) {
            slotinfo->pauseflag=0;
            event.type = PAUSE_CLEAR_EVENT;
            event.data = slotinfo->streampriv;
            push_event(slotinfo->eventqueue, &event);
        }
        slotinfo->playflag=1;
        ret=0;
    }
    pthread_mutex_unlock(&slotinfo->mutex);
    apicall(slot, 0);
    return ret;
}

int xplayer_API_groupplay(int slot)
{
    slotinfo_t* slotinfo = get_slot_info(slot,1,1);
    int ret = -1;
    event_t event;

    if((slotinfo->debugflag & DEBUGFLAG_GROUP)) {
        av_log(NULL,DEBUG_FLAG_LOG_LEVEL,"API_GroupPlay(%d)\n",slot);
    }
    pthread_mutex_lock(&slotinfo->mutex);
    event.type = GROUP_PAUSE_CLEAR_EVENT;
    event.data = slotinfo->streampriv;
    push_event(slotinfo->eventqueue, &event);
    ret=0;
    pthread_mutex_unlock(&slotinfo->mutex);
    return ret;
}

int xplayer_API_groupstop(int slot)
{
    slotinfo_t* slotinfo = get_slot_info(slot,1,1);
    int ret = -1;
    event_t event;

    if((slotinfo->debugflag & DEBUGFLAG_GROUP)) {
        av_log(NULL,DEBUG_FLAG_LOG_LEVEL,"API_GroupStop(%d)\n",slot);
    }
    pthread_mutex_lock(&slotinfo->mutex);
    event.type = GROUP_PAUSE_SET_EVENT;
    event.data = slotinfo->streampriv;
    push_event(slotinfo->eventqueue, &event);
    ret=0;
    pthread_mutex_unlock(&slotinfo->mutex);
    return ret;
}

int xplayer_API_groupseekpos(int slot, double pos) {
    slotinfo_t* slotinfo = get_slot_info(slot,1,1);
    int ret = -1;
    event_t event;

    pthread_mutex_lock(&slotinfo->mutex);
    if(slotinfo->url && slotinfo->status) {
        event.type = GROUP_SEEK_EVENT;
        event.vdouble = pos;
        event.data = NULL;
        push_event(slotinfo->eventqueue, &event);
        slotinfo->seekpos=pos;
        slotinfo->seekflag=1;
    }
    pthread_mutex_unlock(&slotinfo->mutex);
    return ret;
}

int xplayer_API_pause(int slot) {
    slotinfo_t* slotinfo = get_slot_info(slot,1,1);
    int ret = -1;
    event_t event;

    if((slotinfo->debugflag & DEBUGFLAG_GROUP)) {
        av_log(NULL,DEBUG_FLAG_LOG_LEVEL,"API_Stop(%d)\n",slot);
    }
    apicall(slot, 11);
    pthread_mutex_lock(&slotinfo->mutex);
    slotinfo->pauseafterload=0;
    if(slotinfo->url && slotinfo->status) {
        if(!slotinfo->pauseflag) {
            slotinfo->pauseflag=1;
            event.type = PAUSE_SET_EVENT;
            event.data = slotinfo->streampriv;
            push_event(slotinfo->eventqueue, &event);
        }
        ret=0;
    }
    pthread_mutex_unlock(&slotinfo->mutex);
    apicall(slot, 0);
    return ret;
}

int xplayer_API_flush(int slot) {
    slotinfo_t* slotinfo = get_slot_info(slot,1,1);
    int ret = -1;
    event_t event;

    apicall(slot, 12);
    pthread_mutex_lock(&slotinfo->mutex);
    if(slotinfo->url && slotinfo->status) {
        event.type = QUEUE_FLUSH_EVENT;
        event.data = slotinfo->streampriv;
        push_event(slotinfo->eventqueue, &event);
        ret=0;
    }
    pthread_mutex_unlock(&slotinfo->mutex);
    apicall(slot, 0);
    return ret;
}

int isstep = 0;

int xplayer_API_stepnumber(int slot, int step) {
    isstep = step;
    return xplayer_API_pause_step(slot);
}

int xplayer_API_isbuffering(int slot) {
    slotinfo_t* slotinfo = get_slot_info(slot,1,1);
    return slotinfo->buffering;
}

int xplayer_API_pause_step(int slot) {
    slotinfo_t* slotinfo = get_slot_info(slot,1,1);
    int ret = -1;
    event_t event;

    apicall(slot, 13);
    pthread_mutex_lock(&slotinfo->mutex);
    if(slotinfo->url && slotinfo->status) {
        if(slotinfo->pauseflag) {
            event.type = PAUSE_STEP_EVENT;
            event.data = slotinfo->streampriv;
            push_event(slotinfo->eventqueue, &event);
            ret=0;
        }
    }
    pthread_mutex_unlock(&slotinfo->mutex);
    apicall(slot, 0);
    return ret;
}

int xplayer_API_stop(int slot) {
    slotinfo_t* slotinfo = get_slot_info(slot,1,1);
    int ret = -1;
    event_t event;

    apicall(slot, 14);
    pthread_mutex_lock(&slotinfo->mutex);
    slotinfo->playflag=0;
    slotinfo->pauseflag=0;
    if(slotinfo->url && slotinfo->status) {
        event.type = FF_STOP_EVENT;
        event.data = slotinfo->streampriv;
        push_event(slotinfo->eventqueue, &event);
        ret=0;
    }
    pthread_mutex_unlock(&slotinfo->mutex);
    apicall(slot, 0);
    return ret;
}

int xplayer_API_stepframe(int slot, int frame) {
    slotinfo_t* slotinfo = get_slot_info(slot,1,1);
    int ret = -1;
    event_t event;

    apicall(slot, 15);
    pthread_mutex_lock(&slotinfo->mutex);
    if(slotinfo->url && slotinfo->status) {
        event.type = STEP_FRAME;
        event.vdouble = frame;
        event.data = NULL;
        push_event(slotinfo->eventqueue, &event);
    }
    pthread_mutex_unlock(&slotinfo->mutex);
    apicall(slot, 0);
    return ret;
}

int xplayer_API_seek(int slot, double pos) {
    slotinfo_t* slotinfo = get_slot_info(slot,1,1);
    int ret = -1;
    event_t event;

    apicall(slot, 15);
    pthread_mutex_lock(&slotinfo->mutex);
    if(slotinfo->url && slotinfo->status) {
        event.type = SEEK_EVENT;
        event.vdouble = pos;
        event.data = NULL;
        push_event(slotinfo->eventqueue, &event);
        slotinfo->seekpos=pos;
        slotinfo->seekflag=1;
    }
    pthread_mutex_unlock(&slotinfo->mutex);
    apicall(slot, 0);
    return ret;
}

int xplayer_API_seekpos(int slot, double pos) {
    slotinfo_t* slotinfo = get_slot_info(slot,1,1);
    int ret = -1;
    event_t event;

    apicall(slot, 16);
    pthread_mutex_lock(&slotinfo->mutex);
    if(slotinfo->url && slotinfo->status) {
        event.type = SEEK_EVENT;
        event.vdouble = pos;
        event.data = NULL;
        push_event(slotinfo->eventqueue, &event);
        slotinfo->seekpos=pos;
        slotinfo->seekflag=1;
    }
    pthread_mutex_unlock(&slotinfo->mutex);
    apicall(slot, 0);
    return ret;
}

int xplayer_API_seekrel(int slot, double pos) {
    slotinfo_t* slotinfo = get_slot_info(slot,1,1);
    int ret = -1;
    event_t event;

    apicall(slot, 17);
    pthread_mutex_lock(&slotinfo->mutex);
    if(slotinfo->url && slotinfo->status) {
        event.type = SEEK_REL_EVENT;
        event.vdouble = pos;
        event.data = NULL;
        push_event(slotinfo->eventqueue, &event);
        slotinfo->seekpos=pos;
        slotinfo->seekflag=1;
    }
    pthread_mutex_unlock(&slotinfo->mutex);
    apicall(slot, 0);
    return ret;
}


int xplayer_API_volume(int slot, int volume) {
    slotinfo_t* slotinfo = get_slot_info(slot,1,1);

    if(volume<0 || volume>100) {
        return -1;
    }
    apicall(slot, 18);
    pthread_mutex_lock(&slotinfo->mutex);
    if(xplayer_global_status->forcevolume)
        slotinfo->volume=xplayer_global_status->forcevolume;
    else
        slotinfo->volume=volume;
    slotinfo->setvolume=1;
    pthread_mutex_unlock(&slotinfo->mutex);
    apicall(slot, 0);
    return 0;
}

int xplayer_API_getvolume(int slot) {
    slotinfo_t* slotinfo = get_slot_info(slot,1,1);
    int ret;

    apicall(slot, 19);
    pthread_mutex_lock(&slotinfo->mutex);
    ret=slotinfo->volume;
    pthread_mutex_unlock(&slotinfo->mutex);
    apicall(slot, 0);
    return ret;
}

int xplayer_API_mute(int slot, int mute) {
    slotinfo_t* slotinfo = get_slot_info(slot,1,1);

    apicall(slot, 20);
    pthread_mutex_lock(&slotinfo->mutex);
    if(xplayer_global_status->forcevolume)
        slotinfo->mute=0;
    else
        slotinfo->mute=mute;
    slotinfo->setmute=1;
    pthread_mutex_unlock(&slotinfo->mutex);
    apicall(slot, 0);
    return 0;
}

int xplayer_API_getmute(int slot) {
    slotinfo_t* slotinfo = get_slot_info(slot,1,1);
    int ret;

    apicall(slot, 21);
    pthread_mutex_lock(&slotinfo->mutex);
    ret=slotinfo->mute;
    pthread_mutex_unlock(&slotinfo->mutex);
    apicall(slot, 0);
    return ret;
}

int xplayer_API_getcallplayerstatus(int slot) {
    slotinfo_t* slotinfo = get_slot_info(slot,1,1);
    int ret;

    apicall(slot, 22);
    pthread_mutex_lock(&slotinfo->mutex);
    ret=slotinfo->callplayerstatus;
    pthread_mutex_unlock(&slotinfo->mutex);
    apicall(slot, 0);
    return ret;
}

int xplayer_API_getstatus(int slot) {
    slotinfo_t* slotinfo = get_slot_info(slot,1,1);
    int ret;

    apicall(slot, 22);
    pthread_mutex_lock(&slotinfo->mutex);
    ret=slotinfo->status;
    pthread_mutex_unlock(&slotinfo->mutex);
    apicall(slot, 0);
    return ret;
}

int xplayer_API_seekfinished(int slot) {
    return triggermeh == 0;
}

double xplayer_API_getcurrentpts(int slot) {
    slotinfo_t* slotinfo = get_slot_info(slot,1,1);
    double currentpos = 0.0;

    apicall(slot, 23);
    pthread_mutex_lock(&slotinfo->mutex);
    if(slotinfo->url && slotinfo->status) {
        currentpos = slotinfo->currentpos;
    }
    pthread_mutex_unlock(&slotinfo->mutex);
    apicall(slot, 0);
    return currentpos;
}

double xplayer_API_getrealpts(int slot) {
    slotinfo_t* slotinfo = get_slot_info(slot,1,1);
    double realpos = 0.0;

    apicall(slot, 24);
    pthread_mutex_lock(&slotinfo->mutex);
    if(slotinfo->url && slotinfo->status) {
        realpos = slotinfo->realpos;
    }
    pthread_mutex_unlock(&slotinfo->mutex);
    apicall(slot, 0);
    return realpos;
}

double xplayer_API_getfps(int slot) {
    slotinfo_t* slotinfo = get_slot_info(slot,1,1);
    double fps = 0.0;

    apicall(slot, 25);
    pthread_mutex_lock(&slotinfo->mutex);
    if(slotinfo->url && slotinfo->status) {
        fps = slotinfo->fps;
    }
    pthread_mutex_unlock(&slotinfo->mutex);
    apicall(slot, 0);
    return fps;
}

int xplayer_API_isnewimage(int slot) {
    slotinfo_t* slotinfo = get_slot_info(slot,1,1);
    int ret = -1;

    apicall(slot, 26);
    pthread_mutex_lock(&slotinfo->mutex);
    if(slotinfo->url) {
        ret=0;
        if(!(slotinfo->newimageflag&2)) {
            ret = slotinfo->newimageflag;
            slotinfo->newimageflag=0;
        }
        slotinfo->status&=~(STATUS_PLAYER_IMAGE);
    }
    pthread_mutex_unlock(&slotinfo->mutex);
    apicall(slot, 0);
    return ret;
}

int xplayer_API_getimage(int slot, mp_image_t** img) {
    slotinfo_t* slotinfo = get_slot_info(slot,1,1);
    int ret = -1;

    apicall(slot, 0);
    if(!img) {
        return ret;
    }
    *img=NULL;
    apicall(slot, 27);
    pthread_mutex_lock(&slotinfo->mutex);
    if(slotinfo->doneflag==2)
        slotinfo->doneflag=0;
    if(slotinfo->url && slotinfo->status) {
        if(slotinfo->img) {
#ifdef USE_LOCKIMAGE
            if(slotinfo->lockimg && slotinfo->freeablelockimg) {
                free_mp_image(slotinfo->lockimg);
                mpi_free++;
            }
#endif
            *img = slotinfo->img;
#ifdef USE_LOCKIMAGE
            slotinfo->lockimg=slotinfo->img;
            slotinfo->img=NULL;
            slotinfo->freeablelockimg=0;
#endif
            ret=1;
        } else {
            ret=0;
        }
    }
    pthread_mutex_unlock(&slotinfo->mutex);
    apicall(slot, 0);
    return ret;
}

int xplayer_API_imagedone(int slot) {
    slotinfo_t* slotinfo = get_slot_info(slot,1,1);
    int ret = -1;

#ifdef USE_LOCKIMAGE
    apicall(slot, 28);
    pthread_mutex_lock(&slotinfo->mutex);
    if(slotinfo->lockimg) {
        if(slotinfo->freeablelockimg) {
            free_mp_image(slotinfo->lockimg);
            mpi_free++;
        }
        slotinfo->lockimg=NULL;
        slotinfo->freeablelockimg=0;
        ret=0;
    }
    pthread_mutex_unlock(&slotinfo->mutex);
    apicall(slot, 0);
#endif
    return ret;
}

int xplayer_API_vdaframedone(int slot) {
    int ret = -1;
#ifdef USE_LOCKIMAGE
#if defined(__APPLE__)
    slotinfo_t* slotinfo = get_slot_info(slot,1,1);

    apicall(slot, 29);
    pthread_mutex_lock(&slotinfo->mutex);
    if(slotinfo->lockvdaframe) {
            vdaframeno--;
            vdaframes_release++;
            ff_vda_release_vda_frame(slotinfo->lockvdaframe);
        slotinfo->lockvdaframe=NULL;
        ret=0;
    }
    pthread_mutex_unlock(&slotinfo->mutex);
    apicall(slot, 0);
#endif
#endif
    return ret;
}

int xplayer_API_freeableimage(int slot, mp_image_t** img) {
    slotinfo_t* slotinfo = get_slot_info(slot,1,1);
    int ret = -1;
    int i;

    if(!img) {
        return ret;
    }
    *img = NULL;
    ret = 1;
    apicall(slot, 30);
    pthread_mutex_lock(&slotinfo->mutex);
    for(i=0;i<MAX_IMAGES;i++) {
        if(slotinfo->freeable_images[i]) {
            *img = slotinfo->freeable_images[i];
            ret = 0;
            break;
        }
    }
    pthread_mutex_unlock(&slotinfo->mutex);
    apicall(slot, 0);
    return ret;
}

int xplayer_API_freeimage(int slot, mp_image_t* img) {
    slotinfo_t* slotinfo = get_slot_info(slot,1,1);
    int ret = -1;
    int i;

    if(!img) {
        return ret;
    }
    apicall(slot, 31);
    pthread_mutex_lock(&slotinfo->mutex);
    for(i=0;i<MAX_IMAGES;i++) {
        if(slotinfo->freeable_images[i]==img) {
            free_mp_image(img);
            mpi_free++;
            slotinfo->freeable_images[i]=NULL;
            ret = 0;
            break;
        }
    }
    if(!ret) {
        pthread_cond_signal(&slotinfo->freeable_cond);
    }
    pthread_mutex_unlock(&slotinfo->mutex);
    apicall(slot, 0);
    return ret;
}

int xplayer_API_videoprocessdone(int slot) {
    slotinfo_t* slotinfo = get_slot_info(slot,1,1);
    int ret = 1;
    int i;

    apicall(slot, 32);
    pthread_mutex_lock(&slotinfo->mutex);
    av_log(NULL, AV_LOG_DEBUG,"[debug] xplayer_API_videoprocessdone(): slot: %d \n",slotinfo->slotid);
    slotinfo->doneflag=2;
    for(i=0;i<MAX_IMAGES;i++) {
        if(slotinfo->freeable_images[i]) {
            free_mp_image(slotinfo->freeable_images[i]);
            mpi_free++;
            slotinfo->freeable_images[i]=NULL;
            ret=0;
        }
    }
    for(i=0;i<MAX_IMAGES;i++) {
        if(slotinfo->freeable_vdaframe[i]) {
#if defined(__APPLE__)
            vdaframeno--;
            vdaframes_release++;
            ff_vda_release_vda_frame(slotinfo->freeable_vdaframe[i]);
#endif
            slotinfo->freeable_vdaframe[i]=NULL;
            ret=0;
        }
    }
    pthread_cond_signal(&slotinfo->freeable_cond);
    av_log(NULL, AV_LOG_DEBUG,"[debug] xplayer_API_videoprocessdone(): slot: %d OK \n",slotinfo->slotid);
    pthread_mutex_unlock(&slotinfo->mutex);
    apicall(slot, 0);
    return ret;
}

int xplayer_API_getvdaframe(int slot, void** vdaframe) {
    slotinfo_t* slotinfo = get_slot_info(slot,1,1);
    int ret = -1;

    if(!vdaframe) {
        return ret;
    }
    *vdaframe=NULL;
    apicall(slot, 33);
    pthread_mutex_lock(&slotinfo->mutex);
    if(slotinfo->doneflag==2)
        slotinfo->doneflag=0;
    if(slotinfo->url && slotinfo->status) {
        if(slotinfo->vdaframe) {
#ifdef USE_LOCKIMAGE
            if(slotinfo->lockvdaframe) {
#if defined(__APPLE__)
                vdaframeno--;
                vdaframes_release++;
                ff_vda_release_vda_frame(slotinfo->lockvdaframe);
#endif
            }
#endif
            *vdaframe = slotinfo->vdaframe;
#ifdef USE_LOCKIMAGE
            slotinfo->lockvdaframe=slotinfo->vdaframe;
            slotinfo->vdaframe=NULL;
#endif
//            slotinfo->vdaframe=NULL;
//            add_freeable_vdaframe(slotinfo, *vdaframe, 1);
            ret=1;
        } else {
            ret=0;
        }
    }
    pthread_mutex_unlock(&slotinfo->mutex);
    apicall(slot, 0);
    return ret;
}

void* xplayer_API_vdaframe2cvbuffer(int slot, void* vdaframe) {
    slotinfo_t* slotinfo = get_slot_info(slot,1,1);

    if(!slotinfo || !vdaframe) {
        return NULL;
    }
    if(vdaframe) {
#if defined(__APPLE__)
        vda_frame * ptr_vda_frame = (vda_frame *)vdaframe;
        return ptr_vda_frame->cv_buffer;
#endif
    }
    return NULL;
}

int xplayer_API_freeablevdaframe(int slot, void** vdaframe) {
    slotinfo_t* slotinfo = get_slot_info(slot,1,1);
    int ret = -1;
    int i;

    if(!vdaframe) {
        return ret;
    }
    *vdaframe = NULL;
    ret = 1;
    apicall(slot, 34);
    pthread_mutex_lock(&slotinfo->mutex);
    for(i=0;i<MAX_IMAGES;i++) {
        if(slotinfo->freeable_vdaframe[i]) {
            *vdaframe = slotinfo->freeable_vdaframe[i];
            ret = 0;
            break;
        }
    }
    pthread_mutex_unlock(&slotinfo->mutex);
    apicall(slot, 0);
    return ret;
}

int xplayer_API_freevdaframe(int slot, void* vdaframe) {
    slotinfo_t* slotinfo = get_slot_info(slot,1,1);
    int ret = -1;
    int i;

    if(!vdaframe) {
        return ret;
    }
    apicall(slot, 35);
    pthread_mutex_lock(&slotinfo->mutex);
    for(i=0;i<MAX_IMAGES;i++) {
        if(slotinfo->freeable_vdaframe[i]==vdaframe) {
#if defined(__APPLE__)
            vdaframeno--;
            vdaframes_release++;
            ff_vda_release_vda_frame(vdaframe);
#endif
            slotinfo->freeable_vdaframe[i]=NULL;
            ret = 0;
            break;
        }
    }
    if(!ret) {
        pthread_cond_signal(&slotinfo->freeable_cond);
    }
    pthread_mutex_unlock(&slotinfo->mutex);
    apicall(slot, 0);
    return ret;
}

int xplayer_API_prefillaudio(char* buffer, int bufferlen, int playlen) {
    int len;

    master_init(5);
    len = af_time2len(xplayer_global_status->audio, xplayer_global_status->audioprefill);
    len=len-playlen;
    if(len<=0)
        return 0;
    if(len>bufferlen)
        len=bufferlen;
    if(!len)
        return 0;
    memset(buffer,0,len);
    return len;
}

int xplayer_API_prefilllen(void) {
    int len;

    master_init(6);
    len = af_time2len(xplayer_global_status->audio, xplayer_global_status->audioprefill);
    return len;
}

int xplayer_API_getaudio(char* buffer, int bufferlen) {
    int len;

    master_init(7);
    pthread_mutex_lock(&xplayer_global_status->audiomutex);
    if(xplayer_global_status->run && xplayer_global_status->audio) {
        len=xplayer_global_status->audio->len;
        if(bufferlen<0) {
            if(len)
                af_drop_data(xplayer_global_status->audio, len);
            pthread_mutex_unlock(&xplayer_global_status->audiomutex);
            return 0;
        }
        if(len>bufferlen) {
            len=bufferlen;
        }
        if(len) {
            memcpy(buffer,xplayer_global_status->audio->audio,len);
            af_drop_data(xplayer_global_status->audio, len);
        }
        pthread_mutex_unlock(&xplayer_global_status->audiomutex);
        return len;
    }
    pthread_mutex_unlock(&xplayer_global_status->audiomutex);
    return 0;
}

int xplayer_API_getaudio_rate() {
    master_init(8);
    if(xplayer_global_status->audio)
        return xplayer_global_status->audio->rate;
    return 0;
}

int xplayer_API_getaudio_channels() {
    master_init(9);
    if(xplayer_global_status->audio)
        return xplayer_global_status->audio->nch;
    return 0;
}

int xplayer_API_getaudio_format() {
    master_init(10);
    if(xplayer_global_status->audio)
        return xplayer_global_status->audio->format;
    return 0;
}

int xplayer_API_setvideocodec(int slot, char* name)
{
    slotinfo_t* slotinfo = get_slot_info(slot,1,1);
    int ret = -1;

    apicall(slot, 36);
    pthread_mutex_lock(&slotinfo->mutex);
    ret = 0;
    if(slotinfo->video_codec_name)
        av_free(slotinfo->video_codec_name);
    slotinfo->video_codec_name=NULL;
    if(name)
        slotinfo->video_codec_name=av_strdup(name);
    pthread_mutex_unlock(&slotinfo->mutex);
    apicall(slot, 0);
    return ret;
}

int xplayer_API_setaudiocodec(int slot, char* name)
{
    slotinfo_t* slotinfo = get_slot_info(slot,1,1);
    int ret = -1;

    apicall(slot, 37);
    pthread_mutex_lock(&slotinfo->mutex);
    if(slotinfo->audio_codec_name)
        av_free(slotinfo->audio_codec_name);
    slotinfo->audio_codec_name=NULL;
    if(name)
        slotinfo->audio_codec_name=av_strdup(name);
    ret = 0;
    pthread_mutex_unlock(&slotinfo->mutex);
    apicall(slot, 0);
    return ret;
}

int xplayer_API_setsubtitlecodec(int slot, char* name)
{
    slotinfo_t* slotinfo = get_slot_info(slot,1,1);
    int ret = -1;

    apicall(slot, 38);
    pthread_mutex_lock(&slotinfo->mutex);
    if(slotinfo->subtitle_codec_name)
        av_free(slotinfo->subtitle_codec_name);
    slotinfo->subtitle_codec_name=NULL;
    if(name)
        slotinfo->subtitle_codec_name=av_strdup(name);
    ret = 0;
    pthread_mutex_unlock(&slotinfo->mutex);
    apicall(slot, 0);
    return ret;
}

int xplayer_API_setsynctype(int slot, int synctype)
{
    slotinfo_t* slotinfo = get_slot_info(slot,1,1);
    int ret = -1;

    apicall(slot, 39);
    pthread_mutex_lock(&slotinfo->mutex);
    switch(synctype) {
        case SYNC_TYPE_AUDIO:
            slotinfo->av_sync_type = AV_SYNC_AUDIO_MASTER;
            ret=0;
            break;
        case SYNC_TYPE_VIDEO:
            slotinfo->av_sync_type = AV_SYNC_VIDEO_MASTER;
            ret=0;
            break;
        case SYNC_TYPE_CLOCK:
            slotinfo->av_sync_type = AV_SYNC_EXTERNAL_CLOCK;
            ret=0;
            break;
    }
    pthread_mutex_unlock(&slotinfo->mutex);
    apicall(slot, 0);
    return ret;
}

int xplayer_API_sethwbuffersize(int slot, int size)
{
    slotinfo_t* slotinfo = get_slot_info(slot,1,1);
    int ret = -1;

    apicall(slot, 40);
    pthread_mutex_lock(&slotinfo->mutex);
    slotinfo->audio_hw_buf_size=size;
    ret=0;
    pthread_mutex_unlock(&slotinfo->mutex);
    apicall(slot, 0);
    return ret;
}

int xplayer_API_setoptions(int slot, const char *opt, const char *arg)
{
    slotinfo_t* slotinfo = get_slot_info(slot,1,1);
    int ret = -1;

    if(!strcmp(opt,"rtsp_transport"))
    {
        slotinfo->set_rtsp_transport=1;
    }
    apicall(slot, 41);
    pthread_mutex_lock(&slotinfo->mutex);
    parse_option(slotinfo, opt, "");
    if(!strcmp(opt,"rtsp_transport"))
    {
        ret=parse_option(slotinfo, opt, envtransport(arg));
    }
    else
    {
        ret=parse_option(slotinfo, opt, arg);
    }
    pthread_mutex_unlock(&slotinfo->mutex);
    apicall(slot, 0);
    return ret;
}

char* xplayer_API_getstatusline(int slot)
{
    char* ret = NULL;
    slotinfo_t* slotinfo = get_slot_info(slot,1,1);
    apicall(slot, 42);
    pthread_mutex_lock(&slotinfo->mutex);
    ret = av_strdup(slotinfo->statusline);
    pthread_mutex_unlock(&slotinfo->mutex);
    apicall(slot, 0);
    return ret;
}

void xplayer_API_freestatusline(char* line)
{
    if(line)
        av_free(line);
}

/// ---------------------------------------------------------------------------
/// ---------------------------------------------------------------------------
/// ---------------------------------------------------------------------------
/// ---------------------------------------------------------------------------


void print_error(const char *filename, int err)
{
    char errbuf[128];
    const char *errbuf_ptr = errbuf;

    if (av_strerror(err, errbuf, sizeof(errbuf)) < 0)
        errbuf_ptr = strerror(AVUNERROR(err));
    av_log(NULL, AV_LOG_ERROR, "%s: %s\n", filename, errbuf_ptr);
}

#define MAX_QUEUE_SIZE (15 * 1024 * 1024)
#define MIN_AUDIOQ_SIZE (20 * 16 * 1024)
#define MIN_FRAMES 5

/* SDL audio buffer size, in samples. Should be small to have precise
   A/V sync as SDL does not have hardware buffer fullness info. */
//#define SDL_AUDIO_BUFFER_SIZE 1024
#define SDL_AUDIO_BUFFER_SIZE 1024

/* no AV sync correction is done if below the AV sync threshold */
#define AV_SYNC_THRESHOLD 0.01
/* no AV correction is done if too big error */
#define AV_NOSYNC_THRESHOLD 10.0

/* maximum audio speed change to get correct sync */
#define SAMPLE_CORRECTION_PERCENT_MAX 10

/* we use about AUDIO_DIFF_AVG_NB A-V differences to make the average */
#define AUDIO_DIFF_AVG_NB   20

/* NOTE: the size must be big enough to compensate the hardware audio buffersize size */
#define SAMPLE_ARRAY_SIZE (2*65536)

typedef struct PacketQueue {
    AVPacketList *first_pkt, *last_pkt;
    int nb_packets;
    int size;
    int abort_request;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} PacketQueue;

#define VIDEO_PICTURE_QUEUE_SIZE 2
#define SUBPICTURE_QUEUE_SIZE 4

typedef struct VideoPicture {
    double pts;                                  ///<presentation time stamp for this picture
    double duration;                             ///<expected duration of the frame
    int64_t pos;                                 ///<byte position in file
    int skip;
    mp_image_t* bmp;
    mp_image_t* tempimg;
    int width, height; /* source height & width */
    int owidth, oheight; /* source height & width */
    int allocated;
    int reallocate;
    enum PixelFormat pix_fmt;

#if CONFIG_AVFILTER
    AVFilterBufferRef *picref;
#endif
    int usesvda;
#if defined(__APPLE__)
    vda_frame * vdaframe;
#else
    void * vdaframe;
#endif
} VideoPicture;

typedef struct SubPicture {
    double pts; /* presentation time stamp for this picture */
    AVSubtitle sub;
} SubPicture;

typedef struct VideoState {
    slotinfo_t* slotinfo;
    int run;
    enum PixelFormat pix_fmt;
    int fmt;
    int w;
    int h;

    int read_tidinited;
    pthread_t read_tid;
    pthread_t video_tid;
    int refresh_tidinited;
    pthread_t refresh_tid;
    AVInputFormat *iformat;
    int no_background;
    int abort_request;
    int paused;
    int local_paused;
    int local_nopaused;
    int last_paused;
    int pause_seek;
    int global_paused;
    double pause_seek_pos;
    double pause_seek_curr;
    double vpts;
    int seek_req;
    int seek_flags;
    int64_t seek_pos;
    int64_t seek_rel;
    int read_pause_return;
    AVFormatContext *ic;
    int audio_exit_flag;
    int reading;
    double readingtime;
    int readeof;

    int groupsync;

    int audio_stream;

    int av_sync_type;
    double external_clock; /* external clock base */
    int64_t external_clock_time;

    char*  audio_decode_buffer;
    int    audio_decode_buffer_len;
    int    audio_decode_buffer_size;
    double audio_decode_buffer_clock;
    uint64_t audio_decode_last_pts;
    pthread_mutex_t audio_decode_buffer_mutex;

    double decode_audio_clock;
    double audio_clock;
    double audio_diff_cum; /* used for AV difference average computation */
    double audio_diff_avg_coef;
    double audio_diff_threshold;
    int audio_diff_avg_count;
    AVStream *audio_st;
    PacketQueue audioq;
    int audio_hw_buf_size;
    DECLARE_ALIGNED(16,uint8_t,audio_buf2)[AVCODEC_MAX_AUDIO_FRAME_SIZE * 4];
    uint8_t silence_buf[SDL_AUDIO_BUFFER_SIZE];
    uint8_t *audio_buf;
    uint8_t *audio_buf1;
    unsigned int audio_buf_size; /* in bytes */
    int audio_buf_index; /* in bytes */
    int audio_write_buf_size;
    AVPacket audio_pkt_temp;
    AVPacket audio_pkt;
    enum AVSampleFormat audio_src_fmt;
    enum AVSampleFormat audio_tgt_fmt;
    int audio_src_channels;
    int audio_tgt_channels;
    int64_t audio_src_channel_layout;
    int64_t audio_tgt_channel_layout;
    int audio_src_freq;
    int audio_tgt_freq;
    struct SwrContext *swr_ctx;
    double audio_current_pts;
    double audio_current_pts_drift;
    int frame_drops_early;
    int frame_drops_late;
    AVFrame *frame;

    double last_audio_pts;
    int64_t last_audio_callback_time;
    double first_pts;
    int64_t first_time;
    long int audio_clock_diff_len;
    int audio_diff;
    int audio_buf_len;
    int audio_seek_flag;

    ShowMode show_mode;
    int16_t sample_array[SAMPLE_ARRAY_SIZE];
    int sample_array_index;
    int last_i_start;
    RDFTContext *rdft;
    int rdft_bits;
    FFTSample *rdft_data;
    int xpos;

    pthread_t subtitle_tid;
    int subtitle_stream;
    int subtitle_stream_changed;
    AVStream *subtitle_st;
    PacketQueue subtitleq;
    SubPicture subpq[SUBPICTURE_QUEUE_SIZE];
    int subpq_size, subpq_rindex, subpq_windex;
    pthread_mutex_t subpq_mutex;
    pthread_cond_t subpq_cond;

    double frame_timer;
    double frame_last_pts;
    double frame_last_duration;
    double frame_last_dropped_pts;
    double frame_last_returned_time;
    double frame_last_filter_delay;
    int64_t frame_last_dropped_pos;
    double video_clock;                          ///<pts of last decoded frame / predicted pts of next decoded frame
    int video_stream;
    AVStream *video_st;
    PacketQueue videoq;
    double video_current_pts;                    ///<current displayed pts (different from video_clock if frame fifos are used)
    double video_current_pts_drift;              ///<video_current_pts - time (av_gettime) at which we updated video_current_pts - used to have running video pts
    int64_t video_current_pos;                   ///<current displayed file pos
    VideoPicture pictq[VIDEO_PICTURE_QUEUE_SIZE];
    int pictq_size, pictq_rindex, pictq_windex;
    pthread_mutex_t pictq_mutex;
    pthread_cond_t pictq_cond;
#if !CONFIG_AVFILTER
    struct SwsContext *img_convert_ctx;
    struct SwsContext *img_scale_ctx;
#endif

    char filename[1024];
    int width, height, xleft, ytop;
    int step;
    int64_t last_time;
    int64_t start_time;
    double start_pts;
    double max_diff;

    pthread_mutex_t event_mutex;

#if CONFIG_AVFILTER
    AVFilterContext *out_video_filter;          ///<the last filter in the video chain
#endif

    int refresh;
    AVPacket flush_pkt;
    int screen;
    int sws_flags;
    int usesvda;
#if defined(__APPLE__)
    struct vda_context vdactx;
#endif

    int read_enable;
    double buffer_time;
    int flushflag;
    int realtime;

    int valid_delay;
    double seek_pts;

    int vcnt;

    double videowait;

    unsigned int audio_drop;

    double diffclock;
    double diffdclock;
    double diffd2clock;

    double speed;
    double oldspeed;
    int speedrate;
    unsigned long long int audiocorr;

#ifdef USE_WAV_DUMP
    FILE* fhwav1;
    FILE* fhwav2;
    int wav1size;
    int wav2size;
#endif

} VideoState;

#if CONFIG_AVFILTER
static char *vfilters = NULL;
#endif

static void toggle_pause(VideoState *is);

static int ispaused(VideoState *is)
{
    if((is->paused || is->local_paused) && !is->local_nopaused)
        return 1;
    return 0;
}

static int packet_queue_put(VideoState *is, PacketQueue *q, AVPacket *pkt)
{
    AVPacketList *pkt1;

    /* duplicate the packet */
    if (pkt!=&is->flush_pkt && av_dup_packet(pkt) < 0)
        return -1;

    pkt1 = av_malloc(sizeof(AVPacketList));
    if (!pkt1)
        return -1;
    pkt1->pkt = *pkt;
    pkt1->next = NULL;

    pthread_mutex_lock(&q->mutex);

    if (!q->last_pkt)

        q->first_pkt = pkt1;
    else
        q->last_pkt->next = pkt1;
    q->last_pkt = pkt1;
    q->nb_packets++;
    q->size += pkt1->pkt.size + sizeof(*pkt1);
    /* XXX: should duplicate packet data in DV case */
    pthread_cond_signal(&q->cond);

    pthread_mutex_unlock(&q->mutex);
    return 0;
}

/* packet queue handling */
static void packet_queue_init(VideoState *is, PacketQueue *q)
{
    memset(q, 0, sizeof(PacketQueue));
    pthread_mutex_init(&q->mutex, NULL);
    pthread_cond_init(&q->cond, NULL);
    packet_queue_put(is, q, &is->flush_pkt);
}

static void packet_queue_flush(VideoState *is, PacketQueue *q)
{
    AVPacketList *pkt, *pkt1;

    pthread_mutex_lock(&q->mutex);
    for(pkt = q->first_pkt; pkt != NULL; pkt = pkt1) {
        pkt1 = pkt->next;
        av_free_packet(&pkt->pkt);
        av_freep(&pkt);
    }
    q->last_pkt = NULL;
    q->first_pkt = NULL;
    q->nb_packets = 0;
    q->size = 0;
    pthread_mutex_unlock(&q->mutex);
}

static void packet_queue_end(VideoState *is, PacketQueue *q)
{
    packet_queue_flush(is, q);
    pthread_mutex_destroy(&q->mutex);
    pthread_cond_destroy(&q->cond);
}

static void packet_queue_abort(PacketQueue *q)
{
    pthread_mutex_lock(&q->mutex);

    q->abort_request = 1;

    pthread_cond_signal(&q->cond);

    pthread_mutex_unlock(&q->mutex);
}

static double audio_queue_time(VideoState *is)
{
    double clock;
    double l;

    l=is->audio_tgt_channels * av_get_bytes_per_sample(is->audio_tgt_fmt) * is->audio_tgt_freq;
    if(l==0.0 || !is->audio_decode_buffer_len)
    {
        return -1.0;
    }
    clock=is->audio_decode_buffer_len;
    clock/=l;
    return clock;
}

static double queue_time(VideoState *is, PacketQueue *q, int debug)
{
    double first = -1.0;
    double last = -1.0;

    if(!q)
    {
        first=queue_time(is,&is->videoq, debug);
        if(is->audio_st) {
            last=audio_queue_time(is);
        }
        if(first==-1.0 && last==-1.0)
            return -1.0;
        if(last==-1.0)
            return first;
        if(first<last)
            return first;
        return last;
    }
    if(q==&is->audioq)
    {
        return audio_queue_time(is);
    }
    AVPacketList *pl;
    pl = q->first_pkt;
    while(pl) {
        if(pl->pkt.pts!=AV_NOPTS_VALUE)
        {
            first = av_q2d(is->video_st->time_base) * pl->pkt.pts;
            break;
        }
        pl=pl->next;
    }
    if(first==-1.0)
    {
        return -1.0;
    }
    pl = q->last_pkt;
    while(pl) {
        if(pl->pkt.pts!=AV_NOPTS_VALUE) {
            last = av_q2d(is->video_st->time_base) * pl->pkt.pts;
        }
        pl = pl->next;
    }
    if(last==-1.0)
    {
        return -1.0;
    }
    double droplimit = is->buffer_time;
    if(droplimit<0.05)
        droplimit=0.05;
    if(!is->read_enable && ((last-first)/q->nb_packets<-droplimit || (last-first)/q->nb_packets>droplimit))
    {
        av_log(NULL, AV_LOG_DEBUG,"[debug] queue_time(): slot: %d drop packet \n",is->slotinfo->slotid);
        AVPacketList *pkt1 = q->first_pkt;
        q->first_pkt = pkt1->next;
        if (!q->first_pkt)
            q->last_pkt = NULL;
        q->nb_packets--;
        q->size -= pkt1->pkt.size + sizeof(*pkt1);
        av_free(pkt1);
    }

    return last-first;
}

static void wait_for_fill_queue(VideoState *is) {
    double t;

    if(is->buffer_time==0.0) {
        av_log(NULL, AV_LOG_DEBUG,"[debug] wait_for_fill_queue(): slot: %d read enable: buffer time 0.0 \n",is->slotinfo->slotid);
        is->read_enable=1;
        return;
    }
    if(!strcmp(is->ic->iformat->name, "rtsp") && is->ic->duration==AV_NOPTS_VALUE)
    {
        av_log(NULL, AV_LOG_DEBUG,"[debug] wait_for_fill_queue(): slot: %d  read enable: rtsp + duration N/A \n",is->slotinfo->slotid);
        is->read_enable=1;
        return;
    }
    if(is->read_enable)
        return;
    t = queue_time(is, NULL, 1);
    if(t!=-1.0 && t>=is->buffer_time)
    {
        av_log(NULL, AV_LOG_DEBUG,"[debug] wait_for_fill_queue(): slot: %d  read enable \n",is->slotinfo->slotid);
        is->read_enable=1;
    }
}

#if 0
static int packet_queue_is(VideoState *is, PacketQueue *q)
{
    int ret = 0;
    pthread_mutex_lock(&q->mutex);
    if(is->read_enable==0)
        wait_for_fill_queue(is);
    if(q->first_pkt)
        ret=1;
    pthread_mutex_unlock(&q->mutex);
    return ret;
}
#endif

/* return < 0 if aborted, 0 if no packet and > 0 if packet.  */
static int packet_queue_get(VideoState *is, PacketQueue *q, AVPacket *pkt, int block)
{
    AVPacketList *pkt1 = 0;
    int ret;

    pthread_mutex_lock(&q->mutex);

    for(;;) {
        if (q->abort_request) {
            ret = -1;
            break;
        }

        if(is->read_enable==0)
            wait_for_fill_queue(is);
        if(is->read_enable==1)
            pkt1 = q->first_pkt;
        if (pkt1) {
            q->first_pkt = pkt1->next;
            if (!q->first_pkt)
                q->last_pkt = NULL;
            q->nb_packets--;
            q->size -= pkt1->pkt.size + sizeof(*pkt1);
            *pkt = pkt1->pkt;
            av_free(pkt1);
            ret = 1;
            break;
        } else if (!block) {
            ret = 0;
            break;
        } else {
            pthread_cond_wait(&q->cond, &q->mutex);
        }
    }
    pthread_mutex_unlock(&q->mutex);
    return ret;
}

#define ALPHA_BLEND(a, oldp, newp, s)\
((((oldp << s) * (255 - (a))) + (newp * (a))) / (255 << s))

#define RGBA_IN(r, g, b, a, s)\
{\
    unsigned int v = ((const uint32_t *)(s))[0];\
    a = (v >> 24) & 0xff;\
    r = (v >> 16) & 0xff;\
    g = (v >> 8) & 0xff;\
    b = v & 0xff;\
}

#define YUVA_IN(y, u, v, a, s, pal)\
{\
    unsigned int val = ((const uint32_t *)(pal))[*(const uint8_t*)(s)];\
    a = (val >> 24) & 0xff;\
    y = (val >> 16) & 0xff;\
    u = (val >> 8) & 0xff;\
    v = val & 0xff;\
}

#define YUVA_OUT(d, y, u, v, a)\
{\
    ((uint32_t *)(d))[0] = (a << 24) | (y << 16) | (u << 8) | v;\
}


#define BPP 1

static void blend_subrect(AVPicture *dst, const AVSubtitleRect *rect, int imgw, int imgh)
{
    int wrap, wrap3, width2, skip2;
    int y, u, v, a, u1, v1, a1, w, h;
    uint8_t *lum, *cb, *cr;
    const uint8_t *p;
    const uint32_t *pal;
    int dstx, dsty, dstw, dsth;

    dstw = av_clip(rect->w, 0, imgw);
    dsth = av_clip(rect->h, 0, imgh);
    dstx = av_clip(rect->x, 0, imgw - dstw);
    dsty = av_clip(rect->y, 0, imgh - dsth);
    lum = dst->data[0] + dsty * dst->linesize[0];
    cb = dst->data[1] + (dsty >> 1) * dst->linesize[1];
    cr = dst->data[2] + (dsty >> 1) * dst->linesize[2];

    width2 = ((dstw + 1) >> 1) + (dstx & ~dstw & 1);
    skip2 = dstx >> 1;
    wrap = dst->linesize[0];
    wrap3 = rect->pict.linesize[0];
    p = rect->pict.data[0];
    pal = (const uint32_t *)rect->pict.data[1];  /* Now in YCrCb! */

    if (dsty & 1) {
        lum += dstx;
        cb += skip2;
        cr += skip2;

        if (dstx & 1) {
            YUVA_IN(y, u, v, a, p, pal);
            lum[0] = ALPHA_BLEND(a, lum[0], y, 0);
            cb[0] = ALPHA_BLEND(a >> 2, cb[0], u, 0);
            cr[0] = ALPHA_BLEND(a >> 2, cr[0], v, 0);
            cb++;
            cr++;
            lum++;
            p += BPP;
        }
        for(w = dstw - (dstx & 1); w >= 2; w -= 2) {
            YUVA_IN(y, u, v, a, p, pal);
            u1 = u;
            v1 = v;
            a1 = a;
            lum[0] = ALPHA_BLEND(a, lum[0], y, 0);

            YUVA_IN(y, u, v, a, p + BPP, pal);
            u1 += u;
            v1 += v;
            a1 += a;
            lum[1] = ALPHA_BLEND(a, lum[1], y, 0);
            cb[0] = ALPHA_BLEND(a1 >> 2, cb[0], u1, 1);
            cr[0] = ALPHA_BLEND(a1 >> 2, cr[0], v1, 1);
            cb++;
            cr++;
            p += 2 * BPP;
            lum += 2;
        }
        if (w) {
            YUVA_IN(y, u, v, a, p, pal);
            lum[0] = ALPHA_BLEND(a, lum[0], y, 0);
            cb[0] = ALPHA_BLEND(a >> 2, cb[0], u, 0);
            cr[0] = ALPHA_BLEND(a >> 2, cr[0], v, 0);
            p++;
            lum++;
        }
        p += wrap3 - dstw * BPP;
        lum += wrap - dstw - dstx;
        cb += dst->linesize[1] - width2 - skip2;
        cr += dst->linesize[2] - width2 - skip2;
    }
    for(h = dsth - (dsty & 1); h >= 2; h -= 2) {
        lum += dstx;
        cb += skip2;
        cr += skip2;

        if (dstx & 1) {
            YUVA_IN(y, u, v, a, p, pal);
            u1 = u;
            v1 = v;
            a1 = a;
            lum[0] = ALPHA_BLEND(a, lum[0], y, 0);
            p += wrap3;
            lum += wrap;
            YUVA_IN(y, u, v, a, p, pal);
            u1 += u;
            v1 += v;
            a1 += a;
            lum[0] = ALPHA_BLEND(a, lum[0], y, 0);
            cb[0] = ALPHA_BLEND(a1 >> 2, cb[0], u1, 1);
            cr[0] = ALPHA_BLEND(a1 >> 2, cr[0], v1, 1);
            cb++;
            cr++;
            p += -wrap3 + BPP;
            lum += -wrap + 1;
        }
        for(w = dstw - (dstx & 1); w >= 2; w -= 2) {
            YUVA_IN(y, u, v, a, p, pal);
            u1 = u;
            v1 = v;
            a1 = a;
            lum[0] = ALPHA_BLEND(a, lum[0], y, 0);

            YUVA_IN(y, u, v, a, p + BPP, pal);
            u1 += u;
            v1 += v;
            a1 += a;
            lum[1] = ALPHA_BLEND(a, lum[1], y, 0);
            p += wrap3;
            lum += wrap;

            YUVA_IN(y, u, v, a, p, pal);
            u1 += u;
            v1 += v;
            a1 += a;
            lum[0] = ALPHA_BLEND(a, lum[0], y, 0);

            YUVA_IN(y, u, v, a, p + BPP, pal);
            u1 += u;
            v1 += v;
            a1 += a;
            lum[1] = ALPHA_BLEND(a, lum[1], y, 0);

            cb[0] = ALPHA_BLEND(a1 >> 2, cb[0], u1, 2);
            cr[0] = ALPHA_BLEND(a1 >> 2, cr[0], v1, 2);

            cb++;
            cr++;
            p += -wrap3 + 2 * BPP;
            lum += -wrap + 2;
        }
        if (w) {
            YUVA_IN(y, u, v, a, p, pal);
            u1 = u;
            v1 = v;
            a1 = a;
            lum[0] = ALPHA_BLEND(a, lum[0], y, 0);
            p += wrap3;
            lum += wrap;
            YUVA_IN(y, u, v, a, p, pal);
            u1 += u;
            v1 += v;
            a1 += a;
            lum[0] = ALPHA_BLEND(a, lum[0], y, 0);
            cb[0] = ALPHA_BLEND(a1 >> 2, cb[0], u1, 1);
            cr[0] = ALPHA_BLEND(a1 >> 2, cr[0], v1, 1);
            cb++;
            cr++;
            p += -wrap3 + BPP;
            lum += -wrap + 1;
        }
        p += wrap3 + (wrap3 - dstw * BPP);
        lum += wrap + (wrap - dstw - dstx);
        cb += dst->linesize[1] - width2 - skip2;
        cr += dst->linesize[2] - width2 - skip2;
    }
    /* handle odd height */
    if (h) {
        lum += dstx;
        cb += skip2;
        cr += skip2;

        if (dstx & 1) {
            YUVA_IN(y, u, v, a, p, pal);
            lum[0] = ALPHA_BLEND(a, lum[0], y, 0);
            cb[0] = ALPHA_BLEND(a >> 2, cb[0], u, 0);
            cr[0] = ALPHA_BLEND(a >> 2, cr[0], v, 0);
            cb++;
            cr++;
            lum++;
            p += BPP;
        }
        for(w = dstw - (dstx & 1); w >= 2; w -= 2) {
            YUVA_IN(y, u, v, a, p, pal);
            u1 = u;
            v1 = v;
            a1 = a;
            lum[0] = ALPHA_BLEND(a, lum[0], y, 0);

            YUVA_IN(y, u, v, a, p + BPP, pal);
            u1 += u;
            v1 += v;
            a1 += a;
            lum[1] = ALPHA_BLEND(a, lum[1], y, 0);
            cb[0] = ALPHA_BLEND(a1 >> 2, cb[0], u, 1);
            cr[0] = ALPHA_BLEND(a1 >> 2, cr[0], v, 1);
            cb++;
            cr++;
            p += 2 * BPP;
            lum += 2;
        }
        if (w) {
            YUVA_IN(y, u, v, a, p, pal);
            lum[0] = ALPHA_BLEND(a, lum[0], y, 0);
            cb[0] = ALPHA_BLEND(a >> 2, cb[0], u, 0);
            cr[0] = ALPHA_BLEND(a >> 2, cr[0], v, 0);
        }
    }
}

static void free_subpicture(SubPicture *sp)
{
    avsubtitle_free(&sp->sub);
}

static void video_image_display(VideoState *is)
{
    VideoPicture *vp;
    SubPicture *sp;
    AVPicture pict;
    float aspect_ratio;
    int width, height;
    int i;

    vp = &is->pictq[is->pictq_rindex];
    if (vp->bmp || vp->vdaframe) {
#if CONFIG_AVFILTER
         if (vp->picref->video->sample_aspect_ratio.num == 0)
             aspect_ratio = 0;
         else
             aspect_ratio = av_q2d(vp->picref->video->sample_aspect_ratio);
#else

        /* XXX: use variable in the frame */
        if (is->video_st->sample_aspect_ratio.num)
            aspect_ratio = av_q2d(is->video_st->sample_aspect_ratio);
        else if (is->video_st->codec->sample_aspect_ratio.num)
            aspect_ratio = av_q2d(is->video_st->codec->sample_aspect_ratio);
        else
            aspect_ratio = 0;
#endif
        if (aspect_ratio <= 0.0)
            aspect_ratio = 1.0;
        aspect_ratio *= (float)vp->width / (float)vp->height;

        if (is->subtitle_st && vp->bmp) {
            if (is->subpq_size > 0) {
                sp = &is->subpq[is->subpq_rindex];

                if (vp->pts >= sp->pts + ((float) sp->sub.start_display_time / 1000)) {
                    pthread_mutex_lock(&is->slotinfo->mutex);
                    pict.data[0] = vp->bmp->planes[0];
                    pict.data[1] = vp->bmp->planes[2];
                    pict.data[2] = vp->bmp->planes[1];

                    pict.linesize[0] = vp->bmp->stride[0];
                    pict.linesize[1] = vp->bmp->stride[2];
                    pict.linesize[2] = vp->bmp->stride[1];

                    for (i = 0; i < sp->sub.num_rects; i++)
                        blend_subrect(&pict, sp->sub.rects[i],
                                      vp->bmp->w, vp->bmp->h);

                    pthread_mutex_unlock(&is->slotinfo->mutex);
                }
            }
        }


        /* XXX: we suppose the screen has a 1.0 pixel ratio */
        height = is->height;
        width = ((int)rint(height * aspect_ratio)) & ~1;
        if (width > is->width) {
            width = is->width;
            height = ((int)rint(width / aspect_ratio)) & ~1;
        }
//        x = (is->width - width) / 2;
//        y = (is->height - height) / 2;
        is->no_background = 0;
//        rect.x = is->xleft + x;
//        rect.y = is->ytop  + y;
//        rect.w = FFMAX(width,  1);
//        rect.h = FFMAX(height, 1);

        pthread_mutex_lock(&is->slotinfo->mutex);
        is->slotinfo->img = vp->bmp;
        if(is->slotinfo->vdaframe)
            add_freeable_vdaframe(is->slotinfo, is->slotinfo->vdaframe, 1);
        is->slotinfo->vdaframe = vp->vdaframe;
        vp->vdaframe = NULL;

        is->slotinfo->newimageflag=1;
        if(is->pause_seek) {
            is->slotinfo->newimageflag=3;
        }
        is->slotinfo->status|=(STATUS_PLAYER_IMAGE);

        is->slotinfo->length=is->ic->duration/1000000LL;
        if(is->video_current_pts>0.0)
        {
            is->slotinfo->currentpos=is->video_current_pts;
            is->slotinfo->realpos=is->video_current_pts;
        } else {
            is->slotinfo->currentpos=0.0;
            is->slotinfo->realpos=0.0;
        }

        if(is->slotinfo->currentpos>0.0 && is->video_st && is->video_st->codec && is->video_st->codec->codec_type == AVMEDIA_TYPE_VIDEO){
            if(is->video_st->avg_frame_rate.den && is->video_st->avg_frame_rate.num)
                is->slotinfo->fps=av_q2d(is->video_st->avg_frame_rate);
            else if(is->video_st->codec->time_base.den && is->video_st->codec->time_base.num)
                is->slotinfo->fps=1/av_q2d(is->video_st->codec->time_base);
            else if(is->video_st->r_frame_rate.den && is->video_st->r_frame_rate.num)
                is->slotinfo->fps=av_q2d(is->video_st->r_frame_rate);
            else if(is->video_st->time_base.den && is->video_st->time_base.num)
                is->slotinfo->fps=1/av_q2d(is->video_st->time_base);
            else
                is->slotinfo->fps=0.0;
            is->slotinfo->video_info.valid=1;
            is->slotinfo->video_info.streamid=is->video_stream;
            is->slotinfo->video_info.width=is->video_st->codec->width;
            is->slotinfo->video_info.height=is->video_st->codec->height;
            is->slotinfo->video_info.format=pixfmt2imgfmt(is->video_st->codec->pix_fmt);
            is->slotinfo->video_info.fps=is->slotinfo->fps;
        } else {
            is->slotinfo->fps=0.0;
            is->slotinfo->video_info.valid=0;
        }
        pthread_mutex_unlock(&is->slotinfo->mutex);
    }
}

static inline int compute_mod(int a, int b)
{
    return a < 0 ? a%b + b : a%b;
}

static void stream_close(VideoState *is)
{
    event_t event;
    VideoPicture *vp;
    int i;
    /* XXX: use a special url_shutdown call to abort parse cleanly */
    is->abort_request = 1;
    slotinfo_t* slotinfo = is->slotinfo;
    is->audio_exit_flag=1;
    is->slotinfo->status&=~(STATUS_PLAYER_IMAGE|STATUS_PLAYER_CONNECT|STATUS_PLAYER_PAUSE|STATUS_PLAYER_SEEK);
    if(is->read_tidinited)
        pthread_join(is->read_tid, NULL);
    is->read_tidinited=0;
#ifdef THREAD_DEBUG
    av_log(NULL, AV_LOG_DEBUG,"[debug] stream_close(): join refresh thread: slot: %d %x ...\n",is->slotinfo->slotid,(unsigned int)is->refresh_tid);
#endif
    if(is->refresh_tidinited)
        pthread_join(is->refresh_tid, NULL);
    is->refresh_tidinited=0;
#ifdef THREAD_DEBUG
    av_log(NULL, AV_LOG_DEBUG,"[debug] stream_close(): join refresh thread: slot: %d %x OK\n",is->slotinfo->slotid,(unsigned int)is->refresh_tid);
#endif

    /* free all pictures */
    for(i=0;i<VIDEO_PICTURE_QUEUE_SIZE; i++) {
        vp = &is->pictq[i];
#if CONFIG_AVFILTER
        if (vp->picref) {
            avfilter_unref_buffer(vp->picref);
            vp->picref = NULL;
        }
#endif
        if(vp->bmp) {
            if(is->slotinfo->doneflag) {
                free_mp_image(vp->bmp);
                mpi_free++;
            } else {
                add_freeable_image(is->slotinfo, vp->bmp);
            }
        }
        vp->bmp = NULL;
        if(vp->tempimg) {
            if(is->slotinfo->doneflag) {
                free_mp_image(vp->tempimg);
                mpi_free++;
            } else {
                add_freeable_image(is->slotinfo, vp->tempimg);
            }
        }
        vp->tempimg = NULL;
        if(vp->vdaframe) {
            if(is->slotinfo->doneflag)
#if defined(__APPLE__)
            {
                vdaframeno--;
                vdaframes_release++;
                ff_vda_release_vda_frame(vp->vdaframe);
            }
#else
            {}
#endif
            else
                add_freeable_vdaframe(is->slotinfo, vp->vdaframe, 0);
            vp->vdaframe=NULL;
        }
    }
    is->slotinfo->status|=(STATUS_WAIT_FOR_FREE_IMAGE);
    wait_for_free_image(is->slotinfo);
    is->slotinfo->status&=~(STATUS_WAIT_FOR_FREE_IMAGE);
    is->slotinfo->status&=~(STATUS_PLAYER_OPENED);
    pthread_mutex_destroy(&is->event_mutex);
    pthread_mutex_destroy(&is->pictq_mutex);
    pthread_cond_destroy(&is->pictq_cond);
    pthread_mutex_destroy(&is->subpq_mutex);
    pthread_cond_destroy(&is->subpq_cond);
#if !CONFIG_AVFILTER
    if (is->img_convert_ctx)
        sws_freeContext(is->img_convert_ctx);
    if (is->img_scale_ctx)
        sws_freeContext(is->img_scale_ctx);
#endif
    pthread_mutex_lock(&slotinfo->audiomutex);
    is->slotinfo->streampriv=NULL;
#ifdef USE_WAV_DUMP
    if(is->fhwav1) {
        update_wav_header(is->fhwav1,is->wav1size);
        fclose(is->fhwav1);
    }
    if(is->fhwav2) {
        update_wav_header(is->fhwav2,is->wav2size);
        fclose(is->fhwav2);
    }
#endif
    pthread_mutex_destroy(&is->audio_decode_buffer_mutex);
    av_free(is);
    slotinfo->audiolock=0;
    pthread_mutex_unlock(&slotinfo->audiomutex);
    event.type = FF_QUIT_EVENT;
    event.data = slotinfo->streampriv;
    clear_event(slotinfo->eventqueue);
    push_event(slotinfo->eventqueue, &event);
}

static void do_exit(VideoState *is)
{
    if (is) {
        stream_close(is);
    }
}

static int video_open(VideoState *is){
    int w,h;

    w=640;
    h=480;
#if CONFIG_AVFILTER
    if (is->out_video_filter && is->out_video_filter->inputs[0]){
        w = is->out_video_filter->inputs[0]->w;
        h = is->out_video_filter->inputs[0]->h;
    } else
#endif

    if (is->video_st && is->video_st->codec->width){
        w = is->video_st->codec->width;
        h = is->video_st->codec->height;
    }
    is->width = w;
    is->height = h;
    is->screen=1;
    return 0;
}

/* display the current picture, if any */
static void video_display(VideoState *is)
{
    if(!is->screen)
        video_open(is);
    if (is->video_st)
        video_image_display(is);
}

static void* refresh_thread(void *opaque)
{
#ifdef USE_DEBUG_LIB
    double stime = 0.0;
    double etime = 0.0;
    double ltime = 0.0;
#endif

#ifdef THREAD_DEBUG
    av_log(NULL, AV_LOG_DEBUG,"[debug] refresh_thread(): pthread_create: refresh_thread 0x%x \n",(unsigned int)pthread_self());
#endif
    VideoState *is= opaque;
    event_t event;
    while(!is->abort_request){
        event.type = FF_REFRESH_EVENT;
        event.data = opaque;
        if(!is->refresh){
            is->refresh=1;
            push_event(is->slotinfo->eventqueue, &event);
        }
        //FIXME ideally we should wait the correct time but SDLs event passing is so slow it would be silly
#ifdef USE_DEBUG_LIB
        ltime=etime;
        etime=xplayer_clock();

        if(etime>0.0 && stime>0.0) {
            threadtime(is->slotinfo->slotid, REFRESH_THREAD_ID, etime-stime, etime-ltime);
        }
#endif
        usleep(is->audio_st && is->show_mode != SHOW_MODE_VIDEO ? is->slotinfo->rdftspeed*1000 : 5000);
#ifdef USE_DEBUG_LIB
        stime=xplayer_clock();
#endif
    }
    return NULL;
}

/* get the current audio clock value */
static double get_audio_clock(VideoState *is)
{
    if (ispaused(is) || globalpause(is->slotinfo)) {
        return is->audio_current_pts;
    } else if(is->audio_current_pts_drift) {
        return is->audio_current_pts_drift + av_gettime() / 1000000.0;
    }
    return 0.0;
}

/* get the current video clock value */
static double get_video_clock(VideoState *is)
{
    if (ispaused(is) || globalpause(is->slotinfo)) {
        return is->video_current_pts;
    } else if(is->video_current_pts_drift && is->video_current_pts!=AV_NOPTS_VALUE) {
        return is->video_current_pts_drift + av_gettime() / 1000000.0;
    }
    return 0.0;
}

/* get the current external clock value */
static double get_external_clock(VideoState *is)
{
    int64_t ti;
    ti = av_gettime();
    return is->external_clock + ((ti - is->external_clock_time) * 1e-6);
}

/* get the current master clock value */
static double get_master_clock(VideoState *is)
{
    double val;

    if (is->av_sync_type == AV_SYNC_VIDEO_MASTER) {
        if (is->video_st)
            val = get_video_clock(is);
        else
            val = get_audio_clock(is);
    } else if (is->av_sync_type == AV_SYNC_AUDIO_MASTER) {
        if (is->audio_st)
            val = get_audio_clock(is);
        else
            val = get_video_clock(is);
    } else {
        val = get_external_clock(is);
    }
    return val;
}

static VideoState* get_group_master(VideoState *is, int lock)
{
    if(is->slotinfo->groupid) {
        if(xplayer_global_status->groups[is->slotinfo->groupid].master_slot!=-1) {
            slotinfo_t* slotinfo = get_slot_info(xplayer_global_status->groups[is->slotinfo->groupid].master_slot,0,lock);
            if(slotinfo && slotinfo->streampriv) {
                return (VideoState*)slotinfo->streampriv;
            }
        }
    }
    return is;
}

#if 0
static double get_group_clock(VideoState *is)
{
    int masterid;
    slotinfo_t* slotinfo = NULL;
    if(is->slotinfo->groupid) {
        masterid = xplayer_global_status->groups[is->slotinfo->groupid].master_slot;
        if(masterid!=-1) {
            slotinfo = get_slot_info(masterid,0,1);
        }
    }
    if(slotinfo && slotinfo->streampriv) {
        return get_master_clock((VideoState*)slotinfo->streampriv);
    }
    return get_master_clock(is);
}
#endif

/* seek in the stream */
static void stream_seek(VideoState *is, int64_t pos, int64_t rel, int seek_by_bytes)
{
    pthread_mutex_lock(&is->event_mutex);
    if (is->seek_req!=1) {
        is->seek_pos = pos;
        is->slotinfo->currentpos=pos;
        is->seek_rel = rel;
        is->seek_flags &= ~AVSEEK_FLAG_BYTE;
        if (is->slotinfo->seek_by_bytes)
            is->seek_flags |= AVSEEK_FLAG_BYTE;
        is->seek_req = 1;
    }
    pthread_mutex_unlock(&is->event_mutex);
}

/* pause or resume the video */
static void stream_toggle_pause(VideoState *is)
{
    pthread_mutex_lock(&is->event_mutex);
    if (ispaused(is)) {
        if(is->video_current_pts_drift)
            is->frame_timer += av_gettime() / 1000000.0 + is->video_current_pts_drift - is->video_current_pts;
        if(is->read_pause_return != AVERROR(ENOSYS)){
            is->video_current_pts = is->video_current_pts_drift + av_gettime() / 1000000.0;
        }
        is->video_current_pts_drift = is->video_current_pts - av_gettime() / 1000000.0;
        is->audio_current_pts_drift = is->audio_current_pts - av_gettime() / 1000000.0;
    }
    is->paused = !is->paused;
    if((is->slotinfo->debugflag & DEBUGFLAG_GROUP)) {
        av_log(NULL,DEBUG_FLAG_LOG_LEVEL,"Slot: %d toggle pause: %d \n",is->slotinfo->slotid,is->paused);
    }
    if(ispaused(is) || is->pause_seek) {
        is->slotinfo->status|=STATUS_PLAYER_PAUSE;
#if 0
    } else {
        is->slotinfo->status&=~(STATUS_PLAYER_PAUSE);
#endif
    }
    pthread_mutex_unlock(&is->event_mutex);
}

static void global_toggle_pause(slotinfo_t* slotinfo)
{
    VideoState *is = slotinfo->streampriv;
    double callback_time;
    if(!is)
        return;
    if(is->global_paused==globalpause(slotinfo))
        return;
    pthread_mutex_lock(&is->event_mutex);
    is->global_paused=globalpause(slotinfo);
    callback_time=av_gettime() / 1000000.0;
    if (globalpause(slotinfo)) {
        if(is->video_current_pts_drift)
            is->frame_timer += av_gettime() / 1000000.0 + is->video_current_pts_drift - is->video_current_pts;
        if(is->read_pause_return != AVERROR(ENOSYS)){
            is->video_current_pts = is->video_current_pts_drift + av_gettime() / 1000000.0;
        }
        is->video_current_pts_drift = is->video_current_pts - av_gettime() / 1000000.0;
        is->audio_current_pts_drift = is->audio_current_pts - av_gettime() / 1000000.0;
    } else {
        if(is->audio_current_pts_drift) {
            is->audio_current_pts = is->audio_current_pts_drift + callback_time;
            is->audio_current_pts_drift = is->audio_current_pts - callback_time;
        }
    }
    pthread_mutex_unlock(&is->event_mutex);
}

static void group_toggle_pause(VideoState* is, int event)
{
    pthread_mutex_lock(&xplayer_global_status->mutex);
    slotinfo_t* slotinfo=xplayer_global_status->slotinfo;
    while(slotinfo) {
        if(slotinfo->groupid==is->slotinfo->groupid && slotinfo->streampriv) {
            VideoState* sis = (VideoState*)slotinfo->streampriv;
            switch(event) {
            case TOGGLE_PAUSE_EVENT:
                sis->pause_seek=0;
                sis->slotinfo->newimageflag&=2;
                sis->groupsync=0;
                if(!sis->paused) {
                    sis->slotinfo->pauseflag=1;
                    sis->slotinfo->playflag=1;
                    if((is->slotinfo->debugflag & DEBUGFLAG_GROUP)) {
                        av_log(NULL,DEBUG_FLAG_LOG_LEVEL,"Slot: %d group toggle: %d\n",sis->slotinfo->slotid,sis->paused);
                    }
                    toggle_pause(sis);
                    break;
                }
            case PAUSE_CLEAR_EVENT:
                sis->slotinfo->pauseflag=0;
                sis->slotinfo->playflag=1;
                sis->pause_seek=0;
                sis->slotinfo->newimageflag&=2;
                if(ispaused(sis)) {
                    av_log(NULL,DEBUG_FLAG_LOG_LEVEL,"Slot: %d group sync\n",sis->slotinfo->slotid);
                    sis->groupsync=1;
                }
                break;
            case PAUSE_SET_EVENT:
                sis->slotinfo->pauseflag=1;
                sis->slotinfo->playflag=1;
                sis->pause_seek=0;
                sis->slotinfo->newimageflag&=2;
                sis->groupsync=0;
                if(!sis->paused) {
                    if((is->slotinfo->debugflag & DEBUGFLAG_GROUP)) {
                        av_log(NULL,DEBUG_FLAG_LOG_LEVEL,"Slot: %d group set toggle: %d\n",sis->slotinfo->slotid,sis->paused);
                    }
                    toggle_pause(sis);
                }
                break;
            default:
                break;
            }
        }
        slotinfo=(slotinfo_t*)slotinfo->next;
    }
    pthread_mutex_unlock(&xplayer_global_status->mutex);
}

static void group_stream_seek(VideoState* is, double pos)
{
    double spos;
    pthread_mutex_lock(&xplayer_global_status->mutex);
    slotinfo_t* slotinfo=xplayer_global_status->slotinfo;
    while(slotinfo) {
        if(slotinfo->groupid==is->slotinfo->groupid && slotinfo->streampriv) {
            VideoState* sis = (VideoState*)slotinfo->streampriv;
            VideoState* mis = get_group_master(sis,0);

            spos=pos+mis->slotinfo->grouptime-sis->slotinfo->grouptime;
            if(spos<0.0)
                spos=0.0;

            sis->slotinfo->status|=(STATUS_PLAYER_SEEK);
            sis->audio_clock = 0.0;
            sis->audio_clock_diff_len = 0;
            sis->audio_diff=0;
            sis->slotinfo->currentpos=spos;
#ifndef USES_PAUSESEEK
            if (sis->paused) {
                sis->pause_seek = 1;
                sis->pause_seek_pos = spos;
                sis->pause_seek_curr = -1.0;
                if((is->slotinfo->debugflag & DEBUGFLAG_GROUP)) {
                    av_log(NULL,DEBUG_FLAG_LOG_LEVEL,"Slot: %d group stream seek toggle: %d\n",sis->slotinfo->slotid,sis->paused);
                }
                toggle_pause(sis);
            }
#endif
            if((is->slotinfo->debugflag & DEBUGFLAG_GROUP)) {
                av_log(NULL,DEBUG_FLAG_LOG_LEVEL,"group stream seek: slot: %d pos: %8.3f\n",slotinfo->slotid,spos);
            }
            if(!sis->realtime)
                sis->slotinfo->pauseseekreq=1;
            stream_seek(sis, (int64_t)(spos * AV_TIME_BASE), 0, 0);
            if(!sis->realtime)
                sis->groupsync=1;
        }
        slotinfo=(slotinfo_t*)slotinfo->next;
    }
    pthread_mutex_unlock(&xplayer_global_status->mutex);
}

static int group_eof(VideoState* is)
{
    int ret = is->readeof;
    if(!is->slotinfo->groupid || is->slotinfo->length<=0.0) {
        return ret;
    }
    pthread_mutex_lock(&xplayer_global_status->mutex);
    slotinfo_t* slotinfo=xplayer_global_status->slotinfo;
    while(slotinfo) {
        if(slotinfo->groupid==is->slotinfo->groupid && slotinfo->streampriv) {
            VideoState* sis = (VideoState*)slotinfo->streampriv;
            if(sis->readeof)
                ret=1;
        }
        slotinfo=(slotinfo_t*)slotinfo->next;
    }
    pthread_mutex_unlock(&xplayer_global_status->mutex);
    return ret;
}

static double compute_target_delay(double delay, VideoState *is)
{
    double sync_threshold, diff;

    if(is->realtime)
        return 0.0;
    /* update delay to follow master synchronisation source */
    if (((is->av_sync_type == AV_SYNC_AUDIO_MASTER && is->audio_st) ||
         is->av_sync_type == AV_SYNC_EXTERNAL_CLOCK)) {
        /* if video is slave, we try to correct big delays by
           duplicating or deleting a frame */
        diff = get_video_clock(is) - get_master_clock(is);
        /* skip or repeat frame. We take into account the
           delay to compute the threshold. I still don't know
           if it is the best guess */
        sync_threshold = FFMAX(AV_SYNC_THRESHOLD, delay);
        if (fabs(diff) < AV_NOSYNC_THRESHOLD) {
            if (diff <= -sync_threshold)
                delay = 0;
            else if (diff >= sync_threshold)
                delay = 2 * delay;
        }
    }

    av_dlog(NULL, "[debug] compute_target_delay(): video: delay=%0.3f A-V=%f\n",
            delay, -diff);

    return delay;
}

static void pictq_next_picture(VideoState *is) {
    /* update queue size and signal for next picture */
    if (++is->pictq_rindex == VIDEO_PICTURE_QUEUE_SIZE)
        is->pictq_rindex = 0;

    pthread_mutex_lock(&is->pictq_mutex);
    is->pictq_size--;
    pthread_cond_signal(&is->pictq_cond);
    pthread_mutex_unlock(&is->pictq_mutex);
}

static void update_video_pts(VideoState *is, double pts, int64_t pos) {
    double time = av_gettime() / 1000000.0;
    /* update current video pts */
    is->video_current_pts = pts;
    is->video_current_pts_drift = is->video_current_pts - time;
    is->video_current_pos = pos;
    is->frame_last_pts = pts;
}

/* called to display each frame */
static void video_refresh(void *opaque)
{
    VideoState *is = opaque;
    VideoPicture *vp;
    double time;
    char realtimetext[30];

    SubPicture *sp, *sp2;

    if (is->video_st) {
retry:
        if (is->pictq_size == 0) {
            pthread_mutex_lock(&is->pictq_mutex);
            if (is->frame_last_dropped_pts != AV_NOPTS_VALUE && is->frame_last_dropped_pts > is->frame_last_pts) {
                update_video_pts(is, is->frame_last_dropped_pts, is->frame_last_dropped_pos);
                is->frame_last_dropped_pts = AV_NOPTS_VALUE;
            }
            pthread_mutex_unlock(&is->pictq_mutex);
            //nothing to do, no picture to display in the que
        } else {
            double last_duration, duration, delay;
            /* dequeue the picture */
            vp = &is->pictq[is->pictq_rindex];

            if (vp->skip) {
                pictq_next_picture(is);
                goto retry;
            }

            /* compute nominal last_duration */
            last_duration = vp->pts - is->frame_last_pts;
            if (last_duration > 0 && last_duration < 10.0) {
                /* if duration of the last frame was sane, update last_duration in video state */
                is->frame_last_duration = last_duration;
            }
            delay = compute_target_delay(is->frame_last_duration, is);
            if(is->valid_delay==INVALID_FRAME) {
                delay=0.0;
            }
            if(is->valid_delay==INVALID_DELAY && delay>0.001) {
                is->valid_delay=VALID_DELAY;
                delay=0.0;
            }
            if(is->valid_delay==VALID_DELAY && is->seek_pts && is->frame_last_pts<is->seek_pts && vp->pts>=is->seek_pts && delay>0.1) {
                delay=0.0;
                is->seek_pts=0.0;
                is->frame_last_pts=vp->pts;
            }
            if((is->slotinfo->debugflag & DEBUGFLAG_DELAY)) {
                if(delay>0.1 || is->valid_delay!=VALID_DELAY) {
                    av_log(NULL, DEBUG_FLAG_LOG_LEVEL, "[debug] video_refresh(): slot: %d delay: %0.3f last: %0.3f pts: %0.3f lpts: %0.3f valid: %d seek pos: %0.3f \n",is->slotinfo->slotid,delay,last_duration,vp->pts,is->frame_last_pts,is->valid_delay,is->seek_pts);
                }
            }

            time= av_gettime()/1000000.0;
            if(time < is->frame_timer + delay) {
                return;
            }

            if (delay > 0)
                is->frame_timer += delay * FFMAX(1, floor((time-is->frame_timer) / delay));

            pthread_mutex_lock(&is->pictq_mutex);
            update_video_pts(is, vp->pts, vp->pos);
            pthread_mutex_unlock(&is->pictq_mutex);

            if(is->pictq_size > 1) {
                 VideoPicture *nextvp= &is->pictq[(is->pictq_rindex+1)%VIDEO_PICTURE_QUEUE_SIZE];
                 duration = nextvp->pts - vp->pts; // More accurate this way, 1/time_base is often not reflecting FPS
            } else {
                 duration = vp->duration;
            }

            if((is->slotinfo->framedrop>0 || (is->slotinfo->framedrop && is->audio_st)) && time > is->frame_timer + duration){
                if(is->pictq_size > 1){
                    is->frame_drops_late++;
                    pictq_next_picture(is);
                    goto retry;
                }
            }

            if(is->subtitle_st) {
                if (is->subtitle_stream_changed) {
                    pthread_mutex_lock(&is->subpq_mutex);

                    while (is->subpq_size) {
                        free_subpicture(&is->subpq[is->subpq_rindex]);

                        /* update queue size and signal for next picture */
                        if (++is->subpq_rindex == SUBPICTURE_QUEUE_SIZE)
                            is->subpq_rindex = 0;

                        is->subpq_size--;
                    }
                    is->subtitle_stream_changed = 0;

                    pthread_cond_signal(&is->subpq_cond);
                    pthread_mutex_unlock(&is->subpq_mutex);
                } else {
                    if (is->subpq_size > 0) {
                        sp = &is->subpq[is->subpq_rindex];

                        if (is->subpq_size > 1)
                            sp2 = &is->subpq[(is->subpq_rindex + 1) % SUBPICTURE_QUEUE_SIZE];
                        else
                            sp2 = NULL;

                        if ((is->video_current_pts > (sp->pts + ((float) sp->sub.end_display_time / 1000)))
                                || (sp2 && is->video_current_pts > (sp2->pts + ((float) sp2->sub.start_display_time / 1000))))
                        {
                            free_subpicture(sp);

                            /* update queue size and signal for next picture */
                            if (++is->subpq_rindex == SUBPICTURE_QUEUE_SIZE)
                                is->subpq_rindex = 0;

                            pthread_mutex_lock(&is->subpq_mutex);
                            is->subpq_size--;
                            pthread_cond_signal(&is->subpq_cond);
                            pthread_mutex_unlock(&is->subpq_mutex);
                        }
                    }
                }
            }

            /* display picture */
            if (!is->slotinfo->display_disable)
                video_display(is);

            pictq_next_picture(is);
        }
    } else if (is->audio_st) {
        /* draw the next audio frame */

        /* if only audio stream, then display the audio bars (better
           than nothing, just to test the implementation */

        /* display picture */
        if (!is->slotinfo->display_disable)
            video_display(is);
    }
    if (is->slotinfo->show_status) {
        int64_t cur_time;
        int aqsize, vqsize, sqsize;
        double av_diff;

        cur_time = av_gettime();
        if (!is->last_time || (cur_time - is->last_time) >= 30000) {
            aqsize = 0;
            vqsize = 0;
            sqsize = 0;
            if (is->audio_st)
                aqsize = is->audio_decode_buffer_len;
            if (is->video_st)
                vqsize = is->videoq.size;
            if (is->subtitle_st)
                sqsize = is->subtitleq.size;
            av_diff = 0;
            if (is->audio_st && is->video_st)
                av_diff = get_audio_clock(is) - get_video_clock(is);
            if (is->realtime) {
                if(av_diff<-0.04) {
                    is->speed=1.05;
                } else if(av_diff>0.04) {
                    is->speed=0.95;
                } else {
                    is->speed=1.00;
                }
            }
            if(is->realtime)
                snprintf(realtimetext,sizeof(realtimetext)," (RT)");
            else
                memset(realtimetext,0,sizeof(realtimetext));
            if((is->slotinfo->debugflag & DEBUGFLAG_AV)) {
                av_log(NULL, DEBUG_FLAG_LOG_LEVEL, "[debug] video_refresh():  slot: %d A-V: %7.3f audio: %7.3f video: %7.3f drift: %7.3f + %7.3f pts: %7.3f %7.3f - %llx %llx\n",
                        is->slotinfo->slotid,
                        av_diff,
                        get_audio_clock(is),
                        get_video_clock(is),
                        is->video_current_pts_drift, 
                        av_gettime() / 1000000.0,
                        is->video_current_pts,
                        (float)AV_NOPTS_VALUE,
                        (long long int)is->video_current_pts,
                        AV_NOPTS_VALUE);
            }
            if((is->slotinfo->debugflag & DEBUGFLAG_STATUS) && !logfh) {
                printf("Slot: %3d %7.2f A-V:%7.3f fd=%4d aq=%5dKB vq=%5dKB sq=%5dB f=%"PRId64"/%"PRId64" QV: %7.3f QA: %7.3f  %s\r",
                       is->slotinfo->slotid,
                       get_master_clock(is),
                       av_diff,
                       is->frame_drops_early + is->frame_drops_late,
                       aqsize / 1024,
                       vqsize / 1024,
                       sqsize,
                       is->video_st ? is->video_st->codec->pts_correction_num_faulty_dts : 0,
                       is->video_st ? is->video_st->codec->pts_correction_num_faulty_pts : 0,
                       queue_time(is, &is->videoq, 0),
                       queue_time(is, &is->audioq, 0),
                       realtimetext
                       );
            }
            snprintf(is->slotinfo->statusline,STATUSLINE_SIZE,"Slot: %3d %7.2f A-V:%7.3f fd=%4d aq=%5dKB vq=%5dKB sq=%5dB f=%"PRId64"/%"PRId64" QV: %7.3f QA: %7.3f  %s\r",
                   is->slotinfo->slotid,
                   get_master_clock(is),
                   av_diff,
                   is->frame_drops_early + is->frame_drops_late,
                   aqsize / 1024,
                   vqsize / 1024,
                   sqsize,
                   is->video_st ? is->video_st->codec->pts_correction_num_faulty_dts : 0,
                   is->video_st ? is->video_st->codec->pts_correction_num_faulty_pts : 0,
                   queue_time(is, &is->videoq, 0),
                   queue_time(is, &is->audioq, 0),
                   realtimetext
                   );
            fflush(stdout);

#ifdef USE_DEBUG_LIB
            if(dmem && is->slotinfo->slotid<MAX_DEBUG_SLOT) {
                slotdebug_t* slotdebugs = debugmem_getslot(dmem);
                if(slotdebugs) {
                    slotdebugs[is->slotinfo->slotid].uses=1;
                    slotdebugs[is->slotinfo->slotid].slotid=is->slotinfo->slotid;
                    slotdebugs[is->slotinfo->slotid].master_clock=get_master_clock(is);
                    slotdebugs[is->slotinfo->slotid].av_diff=av_diff;
                    slotdebugs[is->slotinfo->slotid].framedrop=is->frame_drops_early + is->frame_drops_late;
                    slotdebugs[is->slotinfo->slotid].aqsize=aqsize / 1024;
                    slotdebugs[is->slotinfo->slotid].vqsize=vqsize / 1024;
                    slotdebugs[is->slotinfo->slotid].sqsize=sqsize;
                    slotdebugs[is->slotinfo->slotid].f1=is->video_st ? is->video_st->codec->pts_correction_num_faulty_dts : 0;
                    slotdebugs[is->slotinfo->slotid].f2=is->video_st ? is->video_st->codec->pts_correction_num_faulty_pts : 0;
                    slotdebugs[is->slotinfo->slotid].vqtime=queue_time(is, &is->videoq, 0);
                    slotdebugs[is->slotinfo->slotid].aqtime=queue_time(is, &is->audioq, 0);
                    slotdebugs[is->slotinfo->slotid].display_proc++;
                }
            }
#endif
            is->last_time = cur_time;
        }
    }
}

/* allocate a picture (needs to do that in main thread to avoid
   potential locking problems */
static void alloc_picture(void *opaque)
{
    VideoState *is = opaque;
    VideoPicture *vp;

    vp = &is->pictq[is->pictq_windex];

    if (vp->bmp) {
        if(is->slotinfo->doneflag) {
            free_mp_image(vp->bmp);
            mpi_free++;
        } else {
            add_freeable_image(is->slotinfo, vp->bmp);
        }
    }
    vp->bmp=NULL;
    if (vp->tempimg) {
        if(is->slotinfo->doneflag) {
            free_mp_image(vp->tempimg);
            mpi_free++;
        } else {
            add_freeable_image(is->slotinfo, vp->tempimg);
        }
    }
    vp->tempimg=NULL;
    if(vp->vdaframe) {
        if(is->slotinfo->doneflag) {
#if defined(__APPLE__)
            vdaframeno--;
            vdaframes_release++;
            ff_vda_release_vda_frame(vp->vdaframe);
#endif
        } else {
#if defined(__APPLE__)
            add_freeable_vdaframe(is->slotinfo, vp->vdaframe, 0);
#endif
        }
    }
    vp->vdaframe=NULL;
#if CONFIG_AVFILTER
    if (vp->picref)
        avfilter_unref_buffer(vp->picref);
    vp->picref = NULL;

    vp->width   = is->out_video_filter->inputs[0]->w;
    vp->height  = is->out_video_filter->inputs[0]->h;
    vp->owidth   = is->out_video_filter->inputs[0]->w;
    vp->oheight  = is->out_video_filter->inputs[0]->h;
    vp->pix_fmt = is->out_video_filter->inputs[0]->format;
#else
    vp->owidth   = is->video_st->codec->width/DEBUG_DIVIMGSIZE;
    vp->oheight  = is->video_st->codec->height/DEBUG_DIVIMGSIZE;
    vp->width   = is->video_st->codec->width/DEBUG_DIVIMGSIZE;
    vp->height  = is->video_st->codec->height/DEBUG_DIVIMGSIZE;
    if(is->slotinfo->w && is->slotinfo->h) {
        vp->width   = is->slotinfo->w/DEBUG_DIVIMGSIZE;
        vp->height  = is->slotinfo->h/DEBUG_DIVIMGSIZE;
    }
    vp->pix_fmt = is->video_st->codec->pix_fmt;
#endif
    if(is->slotinfo->fps==0.0)
        is->slotinfo->video_info.valid=0;
    else
        is->slotinfo->video_info.valid=1;
    is->slotinfo->video_info.streamid=is->video_stream;
    is->slotinfo->video_info.width=is->video_st->codec->width;
    is->slotinfo->video_info.height=is->video_st->codec->height;
    is->slotinfo->video_info.format=pixfmt2imgfmt(is->video_st->codec->pix_fmt);
    is->slotinfo->video_info.fps=is->slotinfo->fps;

    is->pix_fmt = PIX_FMT_YUV420P;
    if(is->slotinfo->imgfmt) {
        is->pix_fmt = imgfmt2pixfmt(is->slotinfo->imgfmt);
    } else {
        is->slotinfo->imgfmt=IMGFMT_YV12;
        is->pix_fmt = imgfmt2pixfmt(is->slotinfo->imgfmt);
    }
    av_log(NULL, AV_LOG_DEBUG,"[debug] alloc_picture(): set output format: %s\n",vo_format_name(is->slotinfo->imgfmt));
    if(is->usesvda!=FLAG_VDA_FRAME)
    {
        vp->bmp = alloc_mpi(vp->width, vp->height, is->slotinfo->imgfmt);
        if (!vp->bmp || vp->bmp->stride[0] < vp->width) {
            /* SDL allocates a buffer smaller than requested if the video
             * overlay hardware is unable to support the requested size. */
            av_log(NULL, AV_LOG_ERROR, "[error] Error: the video system does not support an image\n"
                            "size of %dx%d pixels. Try using -lowres or -vf \"scale=w:h\"\n"
                            "to reduce the image size.\n", vp->width, vp->height );
            do_exit(is);
        }
        mpi_alloc++;
        if(is->slotinfo->w && is->slotinfo->h && (is->slotinfo->w/DEBUG_DIVIMGSIZE!=vp->owidth || is->slotinfo->h/DEBUG_DIVIMGSIZE!=vp->oheight)) {
            vp->tempimg = alloc_mpi(vp->owidth, vp->oheight, is->slotinfo->imgfmt);
            if (!vp->tempimg || vp->tempimg->stride[0] < vp->owidth) {
                /* SDL allocates a buffer smaller than requested if the video
                 * overlay hardware is unable to support the requested size. */
                av_log(NULL, AV_LOG_ERROR, "[error] Error: the video system does not support an image\n"
                                "size of %dx%d pixels. Try using -lowres or -vf \"scale=w:h\"\n"
                                "to reduce the image size.\n", vp->owidth, vp->oheight );
                do_exit(is);
            }
            mpi_alloc++;
        }
    }

    pthread_mutex_lock(&is->pictq_mutex);
    vp->allocated = 1;
    pthread_cond_signal(&is->pictq_cond);
    pthread_mutex_unlock(&is->pictq_mutex);
}

static int queue_picture(VideoState *is, AVFrame *src_frame, double pts1, int64_t pos)
{
    VideoPicture *vp;
    double frame_delay, pts = pts1;
    double st,et;

    /* compute the exact PTS for the picture if it is omitted in the stream
     * pts1 is the dts of the pkt / pts of the frame */
    if (pts != 0) {
        /* update video clock with pts, if present */
        is->video_clock = pts;
    } else {
        pts = is->video_clock;
    }
    /* update video clock for next frame */
    frame_delay = av_q2d(is->video_st->codec->time_base);
    /* for MPEG2, the frame can be repeated, so we update the
       clock accordingly */
    frame_delay += src_frame->repeat_pict * (frame_delay * 0.5);
    is->video_clock += frame_delay;

    /* wait until we have space to put a new picture */
    pthread_mutex_lock(&is->pictq_mutex);

    while (is->pictq_size >= VIDEO_PICTURE_QUEUE_SIZE &&
           !is->videoq.abort_request) {
        st=xplayer_clock();
        pthread_cond_wait(&is->pictq_cond, &is->pictq_mutex);
        et=xplayer_clock();
        is->videowait+=et-st;
    }
    pthread_mutex_unlock(&is->pictq_mutex);

    if (is->videoq.abort_request)
        return -1;

    vp = &is->pictq[is->pictq_windex];

    vp->duration = frame_delay;

    /* alloc or resize hardware picture buffer */
    if ((!vp->bmp && is->usesvda!=FLAG_VDA_FRAME) || vp->reallocate ||
#if CONFIG_AVFILTER
        vp->width  != is->out_video_filter->inputs[0]->w ||
        vp->height != is->out_video_filter->inputs[0]->h) {
#else
        vp->width != (is->slotinfo->w?is->slotinfo->w/DEBUG_DIVIMGSIZE:is->video_st->codec->width/DEBUG_DIVIMGSIZE) ||
        vp->height != (is->slotinfo->h?is->slotinfo->h/DEBUG_DIVIMGSIZE:is->video_st->codec->height/DEBUG_DIVIMGSIZE)) {
#endif
        event_t event;

        vp->allocated  = 0;
        vp->reallocate = 0;

        /* the allocation must be done in the main thread to avoid
           locking problems */
        event.type = FF_ALLOC_EVENT;
        event.data = is;
        push_event(is->slotinfo->eventqueue, &event);

        /* wait until the picture is allocated */
        pthread_mutex_lock(&is->pictq_mutex);
        while (!vp->allocated && !is->videoq.abort_request) {
            st=xplayer_clock();
            pthread_cond_wait(&is->pictq_cond, &is->pictq_mutex);
            et=xplayer_clock();
            is->videowait+=et-st;
        }
        /* if the queue is aborted, we have to pop the pending ALLOC event or wait for the allocation to complete */
        pthread_mutex_unlock(&is->pictq_mutex);

        if (is->videoq.abort_request)
            return -1;
    }

    /* if the frame is not skipped, then display it */
    if (vp->bmp) {
#ifdef USE_DEBUG_LIB
        if(dmem && is->slotinfo->slotid<MAX_DEBUG_SLOT) {
            slotdebug_t* slotdebugs = debugmem_getslot(dmem);
            if(slotdebugs) {
                slotdebugs[is->slotinfo->slotid].vcpts=pts;
            }
        }
#endif
        AVPicture pict;
        AVPicture tpict;
#if CONFIG_AVFILTER
        if(vp->picref)
            avfilter_unref_buffer(vp->picref);
        vp->picref = src_frame->opaque;
#endif

        /* get a pointer on the bitmap */
        memset(&pict,0,sizeof(AVPicture));
        memset(&tpict,0,sizeof(AVPicture));
        pthread_mutex_lock(&is->slotinfo->mutex);
        pict.data[0] = vp->bmp->planes[0];
        pict.data[1] = vp->bmp->planes[2];
        pict.data[2] = vp->bmp->planes[1];

        pict.linesize[0] = vp->bmp->stride[0];
        pict.linesize[1] = vp->bmp->stride[2];
        pict.linesize[2] = vp->bmp->stride[1];

        if(vp->tempimg) {
            tpict.data[0] = vp->tempimg->planes[0];
            tpict.data[1] = vp->tempimg->planes[2];
            tpict.data[2] = vp->tempimg->planes[1];

            tpict.linesize[0] = vp->tempimg->stride[0];
            tpict.linesize[1] = vp->tempimg->stride[2];
            tpict.linesize[2] = vp->tempimg->stride[1];
        }

        is->slotinfo->codec_imgfmt=pixfmt2imgfmt(is->video_st->codec->pix_fmt);
        is->slotinfo->codec_w=is->video_st->codec->width;
        is->slotinfo->codec_h=is->video_st->codec->height;
#if CONFIG_AVFILTER
        //FIXME use direct rendering
        av_picture_copy(&pict, (AVPicture *)src_frame,
                        vp->pix_fmt, vp->width, vp->height);
#else
        is->sws_flags = av_get_int(is->slotinfo->sws_opts, "sws_flags", NULL);
        if(is->usesvda)
        {
            is->img_convert_ctx = sws_getCachedContext(is->img_convert_ctx,
                vp->width, vp->height, PIX_FMT_UYVY422, vp->width, vp->height,
                is->pix_fmt, is->sws_flags, NULL, NULL, NULL);
        } else {
            if(vp->tempimg) {
                is->img_convert_ctx = sws_getCachedContext(is->img_convert_ctx,
                    vp->owidth, vp->oheight, vp->pix_fmt, vp->owidth, vp->oheight,
                    is->pix_fmt, is->sws_flags, NULL, NULL, NULL);
                is->img_scale_ctx = sws_getCachedContext(is->img_scale_ctx,
                    vp->owidth, vp->oheight, is->pix_fmt, vp->width, vp->height,
                    is->pix_fmt, is->sws_flags, NULL, NULL, NULL);
            } else {
                is->img_convert_ctx = sws_getCachedContext(is->img_convert_ctx,
                    vp->width, vp->height, vp->pix_fmt, vp->width, vp->height,
                    is->pix_fmt, is->sws_flags, NULL, NULL, NULL);
            }
        }
        if (is->img_convert_ctx == NULL) {
            av_log(NULL, AV_LOG_ERROR, "[error] queue_picture(): Cannot initialize the conversion context\n");
            pthread_mutex_unlock(&is->slotinfo->mutex);
            do_exit(is);
            return -1;
        }
        if (!is->usesvda && vp->tempimg && is->img_scale_ctx == NULL) {
            av_log(NULL, AV_LOG_ERROR, "[error] queue_picture(): Cannot initialize the conversion context\n");
            pthread_mutex_unlock(&is->slotinfo->mutex);
            do_exit(is);
            return -1;
        }
#if defined(__APPLE__)
        if(is->usesvda)
        {
            if(is->usesvda==FLAG_VDA_FRAME)
            {
               if(vp->vdaframe && is->slotinfo->vdaframe!=vp->vdaframe) {
                    add_freeable_vdaframe(is->slotinfo, vp->vdaframe, 1);
                }
                vp->vdaframe = ff_vda_queue_pop(&is->vdactx);
                if(vp->vdaframe) {
                    vdaframeno++;
                    vdaframes_pop++;
                }
            }
            else
            {
                vda_frame * vdaframe = ff_vda_queue_pop(&is->vdactx);
                if(vdaframe)
                {
                    AVPicture vdapict;
                    memset(&vdapict,0,sizeof(AVPicture));
                    int i;
                    CVPixelBufferLockBaseAddress(vdaframe->cv_buffer, 0);
                    for( i = 0; i < 3; i++ )
                    {
                        vdapict.data[i] = CVPixelBufferGetBaseAddressOfPlane( vdaframe->cv_buffer, i );
                        vdapict.linesize[i] = CVPixelBufferGetBytesPerRowOfPlane( vdaframe->cv_buffer, i );
                    }
                    av_log(NULL, AV_LOG_DEBUG,"[debug] queue_picture(): vda frame: %p ptr: %p %p %p pitch: %d %d %d pict.data: %p ||\n",vdaframe,vdapict.data[0],vdapict.data[1],vdapict.data[2], vdapict.linesize[0], vdapict.linesize[1], vdapict.linesize[2], pict.data[0]);
                    sws_scale(is->img_convert_ctx,
                                vdapict.data,
                                vdapict.linesize,
                                0, vp->height, pict.data, pict.linesize);
                    CVPixelBufferUnlockBaseAddress(vdaframe->cv_buffer, 0);
                    ff_vda_release_vda_frame(vdaframe);
                }
            }
        }
        else
#endif
        {
            if(vp->tempimg) {
                sws_scale(is->img_convert_ctx, (const uint8_t * const*)src_frame->data, src_frame->linesize,
                          0, vp->oheight, tpict.data, tpict.linesize);
                sws_scale(is->img_scale_ctx, tpict.data, tpict.linesize,
                          0, vp->oheight, pict.data, pict.linesize);
            } else {
                sws_scale(is->img_convert_ctx, (const uint8_t * const*)src_frame->data, src_frame->linesize,
                          0, vp->height, pict.data, pict.linesize);
            }
        }
#endif
        pthread_mutex_unlock(&is->slotinfo->mutex);
        /* update the bitmap content */

        vp->pts = pts;
        vp->pos = pos;
        vp->skip = 0;

        /* now we can update the picture count */
        pthread_mutex_lock(&is->pictq_mutex);
        if (++is->pictq_windex == VIDEO_PICTURE_QUEUE_SIZE)
            is->pictq_windex = 0;
        is->pictq_size++;
        pthread_mutex_unlock(&is->pictq_mutex);
    }
#if defined(__APPLE__)
    else if (is->usesvda && 0) {
        AVPicture pict;
#if CONFIG_AVFILTER
        if(vp->picref)
            avfilter_unref_buffer(vp->picref);
        vp->picref = src_frame->opaque;
#endif
        pthread_mutex_lock(&is->slotinfo->mutex);
        if(vp->vdaframe && is->slotinfo->vdaframe!=vp->vdaframe) {
            add_freeable_vdaframe(is->slotinfo, vp->vdaframe, 1);
        }
        vp->vdaframe = ff_vda_queue_pop(&is->vdactx);
        if(vp->vdaframe) {
            vdaframeno++;
            vdaframes_pop++;
        }
        pthread_mutex_unlock(&is->slotinfo->mutex);
        vp->pts = pts;
        vp->pos = pos;
        vp->skip = 0;

        /* now we can update the picture count */
        pthread_mutex_lock(&is->pictq_mutex);
        if (++is->pictq_windex == VIDEO_PICTURE_QUEUE_SIZE)
            is->pictq_windex = 0;
        is->pictq_size++;
        pthread_mutex_unlock(&is->pictq_mutex);
    }
#endif
    return 0;
}

static int get_video_frame(VideoState *is, AVFrame *frame, int64_t *pts, AVPacket *pkt)
{
    int got_picture, i;
    double st,et;

    st=xplayer_clock();
    if (packet_queue_get(is, &is->videoq, pkt, 1) < 0) {
        et=xplayer_clock();
        is->videowait+=et-st;
        return -1;
    }
    et=xplayer_clock();
    is->videowait+=et-st;

    if (pkt->data == is->flush_pkt.data) {
        avcodec_flush_buffers(is->video_st->codec);

        pthread_mutex_lock(&is->pictq_mutex);
        //Make sure there are no long delay timers (ideally we should just flush the que but thats harder)
        for (i = 0; i < VIDEO_PICTURE_QUEUE_SIZE; i++) {
            is->pictq[i].skip = 1;
        }
        while (is->pictq_size && !is->videoq.abort_request) {
            st=xplayer_clock();
            pthread_cond_wait(&is->pictq_cond, &is->pictq_mutex);
            et=xplayer_clock();
            is->videowait+=et-st;
        }
        is->video_current_pos = -1;
        is->frame_last_pts = AV_NOPTS_VALUE;
        is->frame_last_duration = 0;
        is->frame_timer = (double)av_gettime() / 1000000.0;
        is->frame_last_dropped_pts = AV_NOPTS_VALUE;
        pthread_mutex_unlock(&is->pictq_mutex);

        return 0;
    }

    avcodec_decode_video2(is->video_st->codec, frame, &got_picture, pkt);

    if (got_picture) {
        int ret = 1;

        if (is->slotinfo->decoder_reorder_pts == -1) {
            *pts = *(int64_t*)av_opt_ptr(avcodec_get_frame_class(), frame, "best_effort_timestamp");
        } else if (is->slotinfo->decoder_reorder_pts) {
            *pts = frame->pkt_pts;
        } else {
            *pts = frame->pkt_dts;
        }

        if (*pts == AV_NOPTS_VALUE) {
            *pts = 0;
        }

        if (((is->av_sync_type == AV_SYNC_AUDIO_MASTER && is->audio_st) || is->av_sync_type == AV_SYNC_EXTERNAL_CLOCK) &&
             (is->slotinfo->framedrop>0 || (is->slotinfo->framedrop && is->audio_st))) {
            pthread_mutex_lock(&is->pictq_mutex);
            if (is->frame_last_pts != AV_NOPTS_VALUE && *pts) {
                double clockdiff = get_video_clock(is) - get_master_clock(is);
                double dpts = av_q2d(is->video_st->time_base) * *pts;
                double ptsdiff = dpts - is->frame_last_pts;
                if (fabs(clockdiff) < AV_NOSYNC_THRESHOLD &&
                     ptsdiff > 0 && ptsdiff < AV_NOSYNC_THRESHOLD &&
                     clockdiff + ptsdiff - is->frame_last_filter_delay < 0) {
                    is->frame_last_dropped_pos = pkt->pos;
                    is->frame_last_dropped_pts = dpts;
                    is->frame_drops_early++;
                    ret = 0;
                }
            }
            pthread_mutex_unlock(&is->pictq_mutex);
        }

        if (ret)
            is->frame_last_returned_time = av_gettime() / 1000000.0;

        return ret;
    }
    return 0;
}

#if CONFIG_AVFILTER
typedef struct {
    VideoState *is;
    AVFrame *frame;
    int use_dr1;
} FilterPriv;

static int input_get_buffer(AVCodecContext *codec, AVFrame *pic)
{
    AVFilterContext *ctx = codec->opaque;
    AVFilterBufferRef  *ref;
    int perms = AV_PERM_WRITE;
    int i, w, h, stride[4];
    unsigned edge;
    int pixel_size;

    av_assert0(codec->flags & CODEC_FLAG_EMU_EDGE);

    if (codec->codec->capabilities & CODEC_CAP_NEG_LINESIZES)
        perms |= AV_PERM_NEG_LINESIZES;

    if(pic->buffer_hints & FF_BUFFER_HINTS_VALID) {
        if(pic->buffer_hints & FF_BUFFER_HINTS_READABLE) perms |= AV_PERM_READ;
        if(pic->buffer_hints & FF_BUFFER_HINTS_PRESERVE) perms |= AV_PERM_PRESERVE;
        if(pic->buffer_hints & FF_BUFFER_HINTS_REUSABLE) perms |= AV_PERM_REUSE2;
    }
    if(pic->reference) perms |= AV_PERM_READ | AV_PERM_PRESERVE;

    w = codec->width;
    h = codec->height;

    if(av_image_check_size(w, h, 0, codec))
        return -1;

    avcodec_align_dimensions2(codec, &w, &h, stride);
    edge = codec->flags & CODEC_FLAG_EMU_EDGE ? 0 : avcodec_get_edge_width();
    w += edge << 1;
    h += edge << 1;
    if (codec->pix_fmt != ctx->outputs[0]->format) {
        av_log(codec, AV_LOG_ERROR, "[error] input_get_buffer(): Pixel format mismatches %d %d\n", codec->pix_fmt, ctx->outputs[0]->format);
        return -1;
    }
    if(!(ref = avfilter_get_video_buffer(ctx->outputs[0], perms, w, h)))
        return -1;

    pixel_size = av_pix_fmt_descriptors[ref->format].comp[0].step_minus1+1;
    ref->video->w = codec->width;
    ref->video->h = codec->height;
    for(i = 0; i < 4; i ++) {
        unsigned hshift = (i == 1 || i == 2) ? av_pix_fmt_descriptors[ref->format].log2_chroma_w : 0;
        unsigned vshift = (i == 1 || i == 2) ? av_pix_fmt_descriptors[ref->format].log2_chroma_h : 0;

        if (ref->data[i]) {
            ref->data[i]    += ((edge * pixel_size) >> hshift) + ((edge * ref->linesize[i]) >> vshift);
        }
        pic->data[i]     = ref->data[i];
        pic->linesize[i] = ref->linesize[i];
    }
    pic->opaque = ref;
//    pic->age    = INT_MAX;
    pic->type   = FF_BUFFER_TYPE_USER;
    pic->reordered_opaque = codec->reordered_opaque;
    if(codec->pkt) pic->pkt_pts = codec->pkt->pts;
    else           pic->pkt_pts = AV_NOPTS_VALUE;
    return 0;
}

static void input_release_buffer(AVCodecContext *codec, AVFrame *pic)
{
    memset(pic->data, 0, sizeof(pic->data));
    avfilter_unref_buffer(pic->opaque);
}

static int input_reget_buffer(AVCodecContext *codec, AVFrame *pic)
{
    AVFilterBufferRef *ref = pic->opaque;

    if (pic->data[0] == NULL) {
        pic->buffer_hints |= FF_BUFFER_HINTS_READABLE;
        return codec->get_buffer(codec, pic);
    }

    if ((codec->width != ref->video->w) || (codec->height != ref->video->h) ||
        (codec->pix_fmt != ref->format)) {
        av_log(codec, AV_LOG_ERROR, "[error] input_reget_buffer(): Picture properties changed.\n");
        return -1;
    }

    pic->reordered_opaque = codec->reordered_opaque;
    if(codec->pkt) pic->pkt_pts = codec->pkt->pts;
    else           pic->pkt_pts = AV_NOPTS_VALUE;
    return 0;
}

static int input_init(AVFilterContext *ctx, const char *args, void *opaque)
{
    FilterPriv *priv = ctx->priv;
    AVCodecContext *codec;
    if(!opaque) return -1;

    priv->is = opaque;
    codec    = priv->is->video_st->codec;
    codec->opaque = ctx;
    if((codec->codec->capabilities & CODEC_CAP_DR1)
    ) {
        av_assert0(codec->flags & CODEC_FLAG_EMU_EDGE);
        priv->use_dr1 = 1;
        codec->get_buffer     = input_get_buffer;
        codec->release_buffer = input_release_buffer;
        codec->reget_buffer   = input_reget_buffer;
        codec->thread_safe_callbacks = 1;
    }

    priv->frame = avcodec_alloc_frame();

    return 0;
}

static void input_uninit(AVFilterContext *ctx)
{
    FilterPriv *priv = ctx->priv;
    av_free(priv->frame);
}

static int input_request_frame(AVFilterLink *link)
{
    FilterPriv *priv = link->src->priv;
    AVFilterBufferRef *picref;
    int64_t pts = 0;
    AVPacket pkt;
    int ret;

    while (!(ret = get_video_frame(priv->is, priv->frame, &pts, &pkt)))
        av_free_packet(&pkt);
    if (ret < 0)
        return -1;

    if(priv->use_dr1 && priv->frame->opaque) {
        picref = avfilter_ref_buffer(priv->frame->opaque, ~0);
    } else {
        picref = avfilter_get_video_buffer(link, AV_PERM_WRITE, link->w, link->h);
        av_image_copy(picref->data, picref->linesize,
                      priv->frame->data, priv->frame->linesize,
                      picref->format, link->w, link->h);
    }
    av_free_packet(&pkt);

    avfilter_copy_frame_props(picref, priv->frame);
    picref->pts = pts;

    avfilter_start_frame(link, picref);
    avfilter_draw_slice(link, 0, link->h, 1);
    avfilter_end_frame(link);

    return 0;
}

static int input_query_formats(AVFilterContext *ctx)
{
    FilterPriv *priv = ctx->priv;
    enum PixelFormat pix_fmts[] = {
        priv->is->video_st->codec->pix_fmt, PIX_FMT_NONE
    };

    avfilter_set_common_pixel_formats(ctx, avfilter_make_format_list(pix_fmts));
    return 0;
}

static int input_config_props(AVFilterLink *link)
{
    FilterPriv *priv  = link->src->priv;
    AVStream *s = priv->is->video_st;

    link->w = s->codec->width;
    link->h = s->codec->height;
    link->sample_aspect_ratio = s->sample_aspect_ratio.num ?
        s->sample_aspect_ratio : s->codec->sample_aspect_ratio;
    link->time_base = s->time_base;

    return 0;
}

static AVFilter input_filter =
{
    .name      = "ffplay_input",

    .priv_size = sizeof(FilterPriv),

    .init      = input_init,
    .uninit    = input_uninit,

    .query_formats = input_query_formats,

    .inputs    = (AVFilterPad[]) {{ .name = NULL }},
    .outputs   = (AVFilterPad[]) {{ .name = "default",
                                    .type = AVMEDIA_TYPE_VIDEO,
                                    .request_frame = input_request_frame,
                                    .config_props  = input_config_props, },
                                  { .name = NULL }},
};

static int configure_video_filters(AVFilterGraph *graph, VideoState *is, const char *vfilters)
{
    char sws_flags_str[128];
    int ret;
    enum PixelFormat pix_fmts[] = { PIX_FMT_YUV420P, PIX_FMT_NONE };
    pix_fmts[0] = is->pix_fmt;
    AVBufferSinkParams *buffersink_params = av_buffersink_params_alloc();
    AVFilterContext *filt_src = NULL, *filt_out = NULL;
    snprintf(sws_flags_str, sizeof(sws_flags_str), "flags=%d", is->sws_flags);
    graph->scale_sws_opts = av_strdup(sws_flags_str);

    if ((ret = avfilter_graph_create_filter(&filt_src, &input_filter, "src",
                                            NULL, is, graph)) < 0)
        return ret;
#if FF_API_OLD_VSINK_API
    ret = avfilter_graph_create_filter(&filt_out, avfilter_get_by_name("buffersink"), "out",
                                       NULL, pix_fmts, graph);
#else
    buffersink_params->pixel_fmts = pix_fmts;
    ret = avfilter_graph_create_filter(&filt_out, avfilter_get_by_name("buffersink"), "out",
                                       NULL, buffersink_params, graph);
#endif
    av_freep(&buffersink_params);
    if (ret < 0)
        return ret;

    if(vfilters) {
        AVFilterInOut *outputs = avfilter_inout_alloc();
        AVFilterInOut *inputs  = avfilter_inout_alloc();

        outputs->name    = av_strdup("in");
        outputs->filter_ctx = filt_src;
        outputs->pad_idx = 0;
        outputs->next    = NULL;

        inputs->name    = av_strdup("out");
        inputs->filter_ctx = filt_out;
        inputs->pad_idx = 0;
        inputs->next    = NULL;

        if ((ret = avfilter_graph_parse(graph, vfilters, &inputs, &outputs, NULL)) < 0)
            return ret;
    } else {
        if ((ret = avfilter_link(filt_src, 0, filt_out, 0)) < 0)
            return ret;
    }

    if ((ret = avfilter_graph_config(graph, NULL)) < 0)
        return ret;

    is->out_video_filter = filt_out;

    return ret;
}

#endif  /* CONFIG_AVFILTER */

static void* video_thread(void *arg)
{
#ifdef USE_DEBUG_LIB
    double etime = 0.0;
    double ltime = 0.0;
    double stime = 0.0;
#endif
    double wtime = 0.0;
    double st,et;

    VideoState *is = arg;
    AVFrame *frame= avcodec_alloc_frame();
    int64_t pts_int = AV_NOPTS_VALUE, pos = -1;
    double pts;
    int ret;
    int pause_seek_cnt = 0;
    int fake_key_frame = 0;

#ifdef THREAD_DEBUG
    av_log(NULL, AV_LOG_DEBUG,"[debug] video_thread(): video thread start, slot: %d 0x%x \n",is->slotinfo->slotid,(unsigned int)is->video_tid);
#endif
#if CONFIG_AVFILTER
    AVFilterGraph *graph = avfilter_graph_alloc();
    AVFilterContext *filt_out = NULL;
    int last_w = is->video_st->codec->width;
    int last_h = is->video_st->codec->height;

    if ((ret = configure_video_filters(graph, is, vfilters)) < 0)
        goto the_end;
    filt_out = is->out_video_filter;
#endif

    for(;;) {

#ifdef USE_DEBUG_LIB
        ltime=etime;
        etime=xplayer_clock();
        if(etime>0.0 && stime>0.0) {
            threadtime(is->slotinfo->slotid, VIDEO_THREAD_ID, etime-stime-wtime, etime-ltime);
        }
#endif
        wtime=0.0;
#ifdef USE_DEBUG_LIB
        stime=xplayer_clock();
        if(dmem && is->slotinfo->slotid<MAX_DEBUG_SLOT) {
            slotdebug_t* slotdebugs = debugmem_getslot(dmem);
            if(slotdebugs) {
                slotdebugs[is->slotinfo->slotid].video_proc++;
            }
        }
#endif

#if !CONFIG_AVFILTER
        AVPacket pkt;
#else
        AVFilterBufferRef *picref;
        AVRational tb = filt_out->inputs[0]->time_base;
#endif
        is->slotinfo->tbden = is->video_st->time_base.den;
        is->slotinfo->pts = pts_int;

        while ((ispaused(is) || globalpause(is->slotinfo)) && !triggermeh && !(is->slotinfo->status&(STATUS_PLAYER_SEEK)) && !is->videoq.abort_request) {
            st=xplayer_clock();
            usleep(1000);
            et=xplayer_clock();
            wtime+=et-st;
        }
#if CONFIG_AVFILTER
        if (   last_w != is->video_st->codec->width
            || last_h != is->video_st->codec->height) {
            av_log(NULL, AV_LOG_INFO, "[info] video_thread(): Frame changed from size:%dx%d to size:%dx%d\n",
                   last_w, last_h, is->video_st->codec->width, is->video_st->codec->height);
            avfilter_graph_free(&graph);
            graph = avfilter_graph_alloc();
            if ((ret = configure_video_filters(graph, is, vfilters)) < 0)
                goto the_end;
            filt_out = is->out_video_filter;
            last_w = is->video_st->codec->width;
            last_h = is->video_st->codec->height;
        }
        ret = av_buffersink_get_buffer_ref(filt_out, &picref, 0);
        if (picref) {
            avfilter_fill_frame_from_video_buffer_ref(frame, picref);
            pts_int = picref->pts;
            pos     = picref->pos;
            frame->opaque = picref;
        }

        if (av_cmp_q(tb, is->video_st->time_base)) {
            av_unused int64_t pts1 = pts_int;
            pts_int = av_rescale_q(pts_int, tb, is->video_st->time_base);
            av_dlog(NULL, "[debug] video_thread(): "
                    "tb:%d/%d pts:%"PRId64" -> tb:%d/%d pts:%"PRId64"\n",
                    tb.num, tb.den, pts1,
                    is->video_st->time_base.num, is->video_st->time_base.den, pts_int);
        }
#else
        frame->key_frame=0;
        is->videowait=0;
        memset(&pkt,0,sizeof(AVPacket));

        if (lastret && triggermeh && !(is->slotinfo->status&(STATUS_PLAYER_SEEK))) {
            int count, sum;
            int currentframe = 0;

            ok:
            sum = 0;

            for (count=0;count<100;count++) {
                sum += is->read_enable;
                usleep(1000);
            }

            av_log(NULL, "[debug] video_thread(): ", "sum = %d\n", sum);

#if FORCE_ACCURACY // current implementation causes troubles (i.e. too much wait time)
            if (sum != 100) {
                xplayer_API_stepframe(is->slotinfo->slotid, triggermeh);
                is->step = 0;
            }
            else
#endif
            {
                if (is->video_st->time_base.den != 0)
                currentframe = (double)(pts_int * is->slotinfo->fps/(is->video_st->time_base.den));

                is->step = triggermeh - currentframe;

                av_log(NULL, "[debug] video_thread(): ", "lastret = %d, is->step = %d, triggermeh = %d, currentframe = %d\n", lastret, is->step, triggermeh, currentframe);

                if (is->step == -1) laserbeams = 1;

                if (is->step == 0) { // we are lucky! keyframe == frame...
                    triggermeh = 0;
                    dont = 1;
                }
#if FORCE_ACCURACY
                else if ((triggermeh+1)%AV_TIME_BASE == 0 || is->step < 0 || currentframe < 0) {
                    if ((triggermeh+1)%AV_TIME_BASE == 0) triggermeh = (triggermeh + 1)/AV_TIME_BASE;
                    xplayer_API_stepframe(is->slotinfo->slotid, triggermeh);
                    is->step = 0;
                }
#endif
                else triggermeh = 0;
            }
        }

        if (is->step > 0 && !(is->slotinfo->status&(STATUS_PLAYER_SEEK))) {
            //is->step--;
            is->valid_delay = INVALID_DELAY; // instant frame show, no delay
            while (is->step > 0) {
                is->step--;
                ret = get_video_frame(is, frame, &pts_int, &pkt);
                if (ret < 0) {
                    goto the_end;
                }
            }
            if (!ispaused(is)) {
                stream_toggle_pause(is);
                //xplayer_API_pause(is->slotinfo->slotid);
            }
        }

        if (is->slotinfo->status&(STATUS_PLAYER_SEEK)) {
            is->valid_delay = INVALID_DELAY;
            is->slotinfo->status&=~(STATUS_PLAYER_SEEK);
        }

        if (!dont) lastret = ret = get_video_frame(is, frame, &pts_int, &pkt);
        else dont = 0;

        wtime+=is->videowait;
        is->videowait=0.0;
        pos = pkt.pos;
        av_free_packet(&pkt);
#endif

        if (!dont && ret < 0) {
#ifdef THREAD_DEBUG
            av_log(NULL, AV_LOG_DEBUG,"[debug] video_thread(): goto end 1 ret=%d, slot: %d 0x%x \n",ret,is->slotinfo->slotid,(unsigned int)is->video_tid);
#endif
            goto the_end;
        }

        if (!ret) continue;

        is->frame_last_filter_delay = av_gettime() / 1000000.0 - is->frame_last_returned_time;
        if (fabs(is->frame_last_filter_delay) > AV_NOSYNC_THRESHOLD / 10.0)
            is->frame_last_filter_delay = 0;

#if CONFIG_AVFILTER
        if (!picref)
            continue;
#endif

        if(pts_int==AV_NOPTS_VALUE || pts_int<0.0)
            pts = 0.0;
        else
            pts = pts_int*av_q2d(is->video_st->time_base);

        //if (pts_int != 0)
        //av_log(NULL, "[debug] video_thread(): ", "pts_int = %d, den = %d, calc = %lf\n", pts_int, is->video_st->time_base.den, (double)(pts_int/(is->video_st->time_base.den/is->slotinfo->fps)));

#ifdef THREAD_DEBUG
        if(is->videoq.abort_request)
        {
            av_log(NULL, AV_LOG_DEBUG,"[debug] video_thread(): abort req, slot: %d 0x%x \n",is->slotinfo->slotid,(unsigned int)is->video_tid);
        }
#endif
        if(is->valid_delay==INVALID_FRAME) {
            if((is->slotinfo->debugflag & DEBUGFLAG_DELAY)) {
                av_log(NULL, DEBUG_FLAG_LOG_LEVEL, "[debug] video_thread(): frame type: %c \n",av_get_picture_type_char(frame->pict_type));
            }
            if(av_get_picture_type_char(frame->pict_type)!='?') {
                is->valid_delay=INVALID_DELAY;
            }
        }

        is->videowait=0;
        if (!triggermeh) ret = queue_picture(is, frame, pts, pos);
        wtime+=is->videowait;
        is->videowait=0.0;

        if (ret < 0)
        {
#ifdef THREAD_DEBUG
            if(is->slotinfo)
            {
                av_log(NULL, AV_LOG_DEBUG,"[debug] video_thread(): goto end 2 ret=%d, slot: %d 0x%x \n",ret,is->slotinfo->slotid,(unsigned int)is->video_tid);
            }
#endif
            goto the_end;
        }

        is->vpts = pts;

        if(fake_key_frame)
            fake_key_frame--;
        if(is->slotinfo->status&(STATUS_PLAYER_SEEK) && !fake_key_frame)
        {
            fake_key_frame=10;
        }
        if(is->pause_seek) {
            if(is->pause_seek_curr==-1) {
                is->pause_seek_curr=pts;
                pause_seek_cnt = 0;
            }
            av_log(NULL, AV_LOG_DEBUG,"[debug] video_thread(): pauseseek: pts: %12.6f paused: %d seek pos: %12.6f %12.6f QV: %12.6f QA: %12.6f\n",
                        pts,
                        ispaused(is),
                        is->pause_seek_pos,
                        is->pause_seek_curr,
                        queue_time(is, &is->videoq, 0),
                        queue_time(is, &is->audioq, 0)
                    );
        }
        if(is->pause_seek) {
            if(is->pause_seek_curr!=pts)
            {
                if(pause_seek_cnt || frame->key_frame || !fake_key_frame)
                    pause_seek_cnt++;
                if(pause_seek_cnt>1)
                {
                    stream_toggle_pause(is);
                    is->pause_seek=0;
                    is->slotinfo->newimageflag&=2;
                    pause_seek_cnt=0;
                    is->slotinfo->status&=~(STATUS_PLAYER_SEEK);
                }
            }
        } else {
            if(is->pause_seek_curr!=pts)
            {
                if(pause_seek_cnt || frame->key_frame || !fake_key_frame)
                    pause_seek_cnt++;
                if(pause_seek_cnt>1)
                {
                    pause_seek_cnt=0;
                    is->slotinfo->status&=~(STATUS_PLAYER_SEEK);
                }
            }
        }
        if(is->slotinfo->pauseafterload && !strcmp(is->ic->iformat->name, "rtsp") && is->ic->duration==AV_NOPTS_VALUE) {
            if(!(is->slotinfo->status & STATUS_PLAYER_OPENED)) {
                is->slotinfo->status|=(STATUS_PLAYER_OPENED);
            }
            is->slotinfo->pauseafterload=0;
        }
        if(is->slotinfo->pauseafterload && pts_int!=AV_NOPTS_VALUE) {
            av_log(NULL, AV_LOG_DEBUG,"[debug] video_thread(): slot: %d pause after load: pts: %12.6f paused: %d seek pos: %12.6f %12.6f QV: %12.6f QA: %12.6f\n",
                    is->slotinfo->slotid,
                    pts,
                    ispaused(is),
                    is->pause_seek_pos,
                    is->pause_seek_curr,
                    queue_time(is, &is->videoq, 0),
                    queue_time(is, &is->audioq, 0)
            );
            pthread_mutex_lock(&is->slotinfo->mutex);
            is->slotinfo->pauseflag=1;
            is->slotinfo->pauseafterload=0;
            is->paused=0;
            pthread_mutex_unlock(&is->slotinfo->mutex);
            if(!(is->slotinfo->status & STATUS_PLAYER_OPENED)) {
                is->slotinfo->status|=(STATUS_PLAYER_OPENED);
            }
            stream_toggle_pause(is);
        }
        if(!is->flushflag && !strcmp(is->ic->iformat->name, "rtsp") && is->ic->duration==AV_NOPTS_VALUE) {
            is->flushflag=1;
            is->realtime=1;
            is->read_enable=1;
            event_t event;
            event.type = QUEUE_FLUSH_EVENT;
            event.data = is->slotinfo->streampriv;
            push_event(is->slotinfo->eventqueue, &event);
        }
    }
 the_end:
    triggermeh = 0;
    is->slotinfo->status&=~(STATUS_PLAYER_SEEK);
#ifdef THREAD_DEBUG
    if(is->slotinfo)
    {
        av_log(NULL, AV_LOG_DEBUG,"[debug] video_thread(): video thread end, slot: %d 0x%x \n",is->slotinfo->slotid,(unsigned int)is->video_tid);
    }
#endif
#if CONFIG_AVFILTER
    avfilter_graph_free(&graph);
#endif
    av_free(frame);
#ifdef THREAD_DEBUG
    if(is->slotinfo)
    {
        av_log(NULL, AV_LOG_DEBUG,"[debug] video_thread(): video thread done, slot: %d 0x%x \n",is->slotinfo->slotid,(unsigned int)is->video_tid);
    }
#endif
    return NULL;
}

static void* subtitle_thread(void *arg)
{
    VideoState *is = arg;
    SubPicture *sp;
    AVPacket pkt1, *pkt = &pkt1;
    int got_subtitle;
    double pts;
    int i, j;
    int r, g, b, y, u, v, a;

    for(;;) {
        while ((ispaused(is) || globalpause(is->slotinfo)) && !is->subtitleq.abort_request) {
            usleep(1000);
//            SDL_Delay(10);
        }
        if (packet_queue_get(is, &is->subtitleq, pkt, 1) < 0)
            break;

        if(pkt->data == is->flush_pkt.data){
            avcodec_flush_buffers(is->subtitle_st->codec);
            continue;
        }
        pthread_mutex_lock(&is->subpq_mutex);
        while (is->subpq_size >= SUBPICTURE_QUEUE_SIZE &&
               !is->subtitleq.abort_request) {
            pthread_cond_wait(&is->subpq_cond, &is->subpq_mutex);
        }
        pthread_mutex_unlock(&is->subpq_mutex);

        if (is->subtitleq.abort_request)
            return NULL;

        sp = &is->subpq[is->subpq_windex];

       /* NOTE: ipts is the PTS of the _first_ picture beginning in
           this packet, if any */
        pts = 0;
        if (pkt->pts != AV_NOPTS_VALUE)
            pts = av_q2d(is->subtitle_st->time_base)*pkt->pts;

        avcodec_decode_subtitle2(is->subtitle_st->codec, &sp->sub,
                                 &got_subtitle, pkt);

        if (got_subtitle && sp->sub.format == 0) {
            sp->pts = pts;

            for (i = 0; i < sp->sub.num_rects; i++)
            {
                for (j = 0; j < sp->sub.rects[i]->nb_colors; j++)
                {
                    RGBA_IN(r, g, b, a, (uint32_t*)sp->sub.rects[i]->pict.data[1] + j);
                    y = RGB_TO_Y_CCIR(r, g, b);
                    u = RGB_TO_U_CCIR(r, g, b, 0);
                    v = RGB_TO_V_CCIR(r, g, b, 0);
                    YUVA_OUT((uint32_t*)sp->sub.rects[i]->pict.data[1] + j, y, u, v, a);
                }
            }

            /* now we can update the picture count */
            if (++is->subpq_windex == SUBPICTURE_QUEUE_SIZE)
                is->subpq_windex = 0;
            pthread_mutex_lock(&is->subpq_mutex);
            is->subpq_size++;
            pthread_mutex_unlock(&is->subpq_mutex);
        }
        av_free_packet(pkt);
    }
    return NULL;
}

#if 0
/* return the new audio buffer size (samples can be added or deleted
   to get better sync if video or external master clock) */
static int synchronize_audio(VideoState *is, short *samples,
                             int samples_size1, double pts)
{
    int n, samples_size;
    double ref_clock;

    n = av_get_bytes_per_sample(is->audio_tgt_fmt) * is->audio_tgt_channels;
    samples_size = samples_size1;

    /* if not master, then we try to remove or add samples to correct the clock */
    if (((is->av_sync_type == AV_SYNC_VIDEO_MASTER && is->video_st) ||
         is->av_sync_type == AV_SYNC_EXTERNAL_CLOCK)) {
        double diff, avg_diff;
        int wanted_size, min_size, max_size, nb_samples;

        ref_clock = get_master_clock(is);
        diff = get_audio_clock(is) - ref_clock;

        if (diff < AV_NOSYNC_THRESHOLD) {
            is->audio_diff_cum = diff + is->audio_diff_avg_coef * is->audio_diff_cum;
            if (is->audio_diff_avg_count < AUDIO_DIFF_AVG_NB) {
                /* not enough measures to have a correct estimate */
                is->audio_diff_avg_count++;
            } else {
                /* estimate the A-V difference */
                avg_diff = is->audio_diff_cum * (1.0 - is->audio_diff_avg_coef);

                if (fabs(avg_diff) >= is->audio_diff_threshold) {
                    wanted_size = samples_size + ((int)(diff * is->audio_tgt_freq) * n);
                    nb_samples = samples_size / n;

                    min_size = ((nb_samples * (100 - SAMPLE_CORRECTION_PERCENT_MAX)) / 100) * n;
                    max_size = ((nb_samples * (100 + SAMPLE_CORRECTION_PERCENT_MAX)) / 100) * n;
                    if (wanted_size < min_size)
                        wanted_size = min_size;
                    else if (wanted_size > FFMIN3(max_size, samples_size, sizeof(is->audio_buf2)))
                        wanted_size = FFMIN3(max_size, samples_size, sizeof(is->audio_buf2));

                    /* add or remove samples to correction the synchro */
                    if (wanted_size < samples_size) {
                        /* remove samples */
                        samples_size = wanted_size;
                    } else if (wanted_size > samples_size) {
                        uint8_t *samples_end, *q;
                        int nb;

                        /* add samples */
                        nb = (samples_size - wanted_size);
                        samples_end = (uint8_t *)samples + samples_size - n;
                        q = samples_end + n;
                        while (nb > 0) {
                            memcpy(q, samples_end, n);
                            q += n;
                            nb -= n;
                        }
                        samples_size = wanted_size;
                    }
                }
                av_log(NULL, AV_LOG_WARNING,"[warning] synchronize_audio(): diff=%f adiff=%f sample_diff=%d apts=%0.3f vpts=%0.3f %f\n",
                        diff, avg_diff, samples_size - samples_size1,
                        is->audio_clock, is->video_clock, is->audio_diff_threshold);
            }
        } else {
            /* too big difference : may be initial PTS errors, so
               reset A-V filter */
            is->audio_diff_avg_count = 0;
            is->audio_diff_cum = 0;
        }
    }

    return samples_size;
}
#endif

static void audio_add_to_buffer(VideoState *is, unsigned char* data, int data_size)
{
    char* tmp;
    if(!data_size)
        return;
    if(!is->audio_decode_buffer) {
        is->audio_decode_buffer_size=data_size+is->audio_decode_buffer_len;
        is->audio_decode_buffer=av_malloc(is->audio_decode_buffer_size);
    }
    if(is->audio_decode_buffer_len+data_size>is->audio_decode_buffer_size) {
        tmp = is->audio_decode_buffer;
        is->audio_decode_buffer_size=data_size+is->audio_decode_buffer_len;
        is->audio_decode_buffer=av_malloc(is->audio_decode_buffer_size*2);
        memcpy(is->audio_decode_buffer,tmp,is->audio_decode_buffer_len);
        av_free(tmp);
        is->audio_decode_buffer_size*=2;
    }
    if(data)
        memcpy(is->audio_decode_buffer+is->audio_decode_buffer_len,data,data_size);
    else
        memset(is->audio_decode_buffer+is->audio_decode_buffer_len,0,data_size);
    is->audio_decode_buffer_len+=data_size;
}

#if USE_AUDIOCORR_ZERO
static void fill_zero(VideoState *is)
{
    int len,n,ch,okch,c;
    int cmin=0;
    int16_t min[6];
    int16_t last[6];
    int ok[6];
    int16_t* audio;
    int16_t* audioptr;

    if(av_get_bytes_per_sample(is->audio_tgt_fmt)!=2 || is->audio_tgt_channels>6)
        return;
    len=is->audio_decode_buffer_len/2/is->audio_tgt_channels;
    len--;
    if(len<1)
        return;
    audio = (int16_t*)(is->audio_decode_buffer);
    for(ch=0;ch<is->audio_tgt_channels;ch++) {
        audioptr=&audio[len*is->audio_tgt_channels+ch];
        min[ch]=*audioptr;
        last[ch]=*audioptr;
        ok[ch]=0;
    }
    okch=0;
    audio = (int16_t*)is->audio_decode_buffer;
    c=0;
    cmin=0;
    for(n=len;n>0;n--) {
        for(ch=0;ch<is->audio_tgt_channels;ch++) {
            c++;
            audioptr=&audio[n*is->audio_tgt_channels+ch];
            if(ok[ch]) {
                continue;
            }
            if((*audioptr==0) || (last[ch]>0 && *audioptr<0) || (last[ch]<0 && *audioptr>0)) {
                ok[ch]=1;
                okch++;
                continue;
            }
            last[ch]=*audioptr;
            if(abs(min[ch])>abs(*audioptr)) {
                min[ch]=*audioptr;
                if(c<8192)
                    cmin=*audioptr;
            }
            *audioptr=0;
        }
        if(okch==is->audio_tgt_channels)
            break;
    }
#ifdef DEBUG_AUDIOCORR
    fprintf(stderr,"slot: %d post fill: %d min: %d ok: %d n: %d\n",is->slotinfo->slotid,c,cmin,okch,n);
#else
    if(cmin>0) cmin=0;
#endif
}

static void fill_zero_package(VideoState *is, unsigned char* data, int datalen)
{
    int len,n,ch,okch,c,cmin;
    int16_t min[6];
    int16_t last[6];
    int ok[6];
    int16_t* audio;

    if(av_get_bytes_per_sample(is->audio_tgt_fmt)!=2 || is->audio_tgt_channels>6 || !datalen) {
        return;
    }
    len=datalen/2/is->audio_tgt_channels;
    audio = (int16_t*)(data);
    for(ch=0;ch<is->audio_tgt_channels;ch++) {
        min[ch]=*audio;
        last[ch]=*audio;
        ok[ch]=0;
        audio++;
    }
    okch=0;
    audio = (int16_t*)(data);
    c=0;
    cmin=0;
    for(n=0;n<len;n++) {
        for(ch=0;ch<is->audio_tgt_channels;ch++) {
            c++;
            if(ok[ch]) {
                audio++;
                continue;
            }
            if((*audio==0) || (last[ch]>0 && *audio<0) || (last[ch]<0 && *audio>0)) {
                ok[ch]=1;
                okch++;
                audio++;
                continue;
            }
            if(abs(min[ch])>abs(*audio)) {
                min[ch]=*audio;
                if(c<8192)
                    cmin=*audio;
            }
            last[ch]=*audio;
            *audio=0;
            audio++;
        }
        if(okch==is->audio_tgt_channels)
            break;
    }
#ifdef DEBUG_AUDIOCORR
    fprintf(stderr,"slot: %d pre  fill: %d min: %d\n",is->slotinfo->slotid,c,cmin);
#else
    if(cmin>0) cmin=0;
#endif
}
#endif

static int audio_get_from_buffer(VideoState *is, unsigned char* data, int data_size)
{
    unsigned char* tmp;
    if(data_size>is->audio_decode_buffer_len) {
        data_size=is->audio_decode_buffer_len;
    }
    if(data_size) {
        if(data) {
            memcpy(data,is->audio_decode_buffer,data_size);
        }
        is->audio_decode_buffer_len-=data_size;
    }
    if(is->audio_decode_buffer_len) {
        tmp=av_malloc(is->audio_decode_buffer_len);
        memcpy(tmp,is->audio_decode_buffer+data_size,is->audio_decode_buffer_len);
        memcpy(is->audio_decode_buffer,tmp,is->audio_decode_buffer_len);
        av_free(tmp);
    }
    return data_size;
}

static int audio_decode_frame(VideoState *is, AVPacket *pkt)
{
    int got_frame;
    int len;
    int data_size;
    int plane_size;
    char* tmp;
    double clock = 0.0;
    double diff = 0.0;
    double l;
    double last_clock = 0;
    AVCodecContext *dec= is->audio_st->codec;
    int len2,resampled_data_size;
    int64_t dec_channel_layout;
    int64_t last_pts = AV_NOPTS_VALUE;
    int first = 1;
    int pktdataoffset;
    uint8_t *pktdata;
    int dlen = 0;
    int sdlen = 0;
    int ptslen = 0;
    int difflen = 0;
    int fillzero = 0;

#ifdef USE_DEBUG_LIB
    if(dmem && is->slotinfo->slotid<MAX_DEBUG_SLOT) {
        slotdebug_t* slotdebugs = debugmem_getslot(dmem);
        if(slotdebugs) {
            slotdebugs[is->slotinfo->slotid].audio_proc++;
        }
    }
#endif
    if(!pkt)
    {
        return 0;
    }
    if(is->read_enable==-1)
    {
        av_free_packet(pkt);
        return 0;
    }
    if (!is->frame) {
        if (!(is->frame = avcodec_alloc_frame())) {
            return AVERROR(ENOMEM);
        }
    } else
        avcodec_get_frame_defaults(is->frame);
    difflen=0;
    if (pkt->pts != AV_NOPTS_VALUE) {
        last_pts=is->audio_decode_last_pts;
        if(is->audio_decode_last_pts!=pkt->pts && is->audio_decode_last_pts!=AV_NOPTS_VALUE) {
            ptslen=is->audio_tgt_channels * av_get_bytes_per_sample(is->audio_tgt_fmt) * is->audio_tgt_freq * av_q2d(is->audio_st->time_base);
            dlen=(pkt->pts-is->audio_decode_last_pts)*ptslen;
            difflen=dlen-is->audio_clock_diff_len;
            is->audio_clock_diff_len=0;
            is->decode_audio_clock = av_q2d(is->audio_st->time_base)*pkt->pts;
        }
        l=is->audio_tgt_channels * av_get_bytes_per_sample(is->audio_tgt_fmt) * is->audio_tgt_freq;
        is->audio_decode_last_pts=pkt->pts;
        last_clock = is->decode_audio_clock;
        if(last_clock) {
            diff=is->decode_audio_clock-last_clock;
            if(is->pause_seek || !is->read_enable) {
                is->audio_clock_diff_len=0;
                last_clock=0.0;
                difflen=0;
            } else if(is->audio_seek_flag) {
                is->audio_clock_diff_len=0;
                is->audio_diff=0;
                is->audio_seek_flag=0;
                last_clock=0.0;
                difflen=0;
            } else if(diff<0.0) {
                last_clock=0.0;
                diff=0.0;
                difflen=0;
            }
        }
//        if(is->realtime) {
//            difflen=0;
//        }
//        difflen+=is->audio_diff;
        diff=difflen;
        diff/=l;
        if(diff>-0.04 && diff<0.04) {
            is->decode_audio_clock+=diff;
            is->diffdclock+=diff;
            diff=0.0;
            difflen=0;
        }
        is->audio_diff=0;
        if(difflen) {
#ifdef DEBUG_AUDIOCORR
            fprintf(stderr,"Slot: %d difflen: %d diff: %8.3f ms\n",is->slotinfo->slotid,difflen,diff*1000.0);
#endif
            fprintf(stderr,"Slot: %d difflen: %d diff: %8.3f ms\n",is->slotinfo->slotid,difflen,diff*1000.0);
        }
//        difflen=0;
//        diff=0.0;
        if(difflen>0) {
            pthread_mutex_lock(&is->audio_decode_buffer_mutex);
            fillzero=1;
#if USE_AUDIOCORR_ZERO
            fill_zero(is);
            audio_add_to_buffer(is, NULL, difflen);
#else
            audio_add_to_buffer(is, NULL, difflen);
            is->audiocorr+=difflen;
#endif
            clock=difflen;
            l=is->audio_tgt_channels * av_get_bytes_per_sample(is->audio_tgt_fmt) * is->audio_tgt_freq;
            clock/=l;
//            is->decode_audio_clock+=clock;
            pthread_mutex_unlock(&is->audio_decode_buffer_mutex);
            difflen=0;
        }
        if(difflen<0) {
            pthread_mutex_lock(&is->audio_decode_buffer_mutex);
            difflen+=audio_get_from_buffer(is, NULL, -difflen);
            is->audiocorr-=difflen;
            clock=-difflen;
            l=is->audio_tgt_channels * av_get_bytes_per_sample(is->audio_tgt_fmt) * is->audio_tgt_freq;
            clock/=l;
//            is->decode_audio_clock-=clock;
            pthread_mutex_unlock(&is->audio_decode_buffer_mutex);
        }
    }
    pktdataoffset=0;
    pktdata=pkt->data;
    while(pkt->size>0) {
        pkt->data=pktdata+pktdataoffset;
        len = avcodec_decode_audio4(dec, is->frame, &got_frame, pkt);
        pkt->data=pktdata;
        pktdataoffset+=len;
        pkt->size -= len;
        if (!got_frame) {
            break;
        }
        data_size = av_samples_get_buffer_size(&plane_size, dec->channels,
                                               is->frame->nb_samples,
                                               dec->sample_fmt, 1);

        dec_channel_layout = (dec->channel_layout && dec->channels == av_get_channel_layout_nb_channels(dec->channel_layout)) ? dec->channel_layout : av_get_default_channel_layout(dec->channels);

        if (dec->sample_fmt != is->audio_src_fmt || dec_channel_layout != is->audio_src_channel_layout || dec->sample_rate != is->audio_src_freq) {
            if (is->swr_ctx)
                swr_free(&is->swr_ctx);
            is->swr_ctx = swr_alloc_set_opts(NULL,
                                             is->audio_tgt_channel_layout, is->audio_tgt_fmt, is->audio_tgt_freq,
                                             dec_channel_layout,           dec->sample_fmt,   dec->sample_rate,
                                             0, NULL);
            if (!is->swr_ctx || swr_init(is->swr_ctx) < 0) {
                av_log(NULL, AV_LOG_ERROR, "[error] audio_decode_frame(): Cannot create sample rate converter for conversion of %d Hz %s %d channels to %d Hz %s %d channels!\n",
                    dec->sample_rate,
                    av_get_sample_fmt_name(dec->sample_fmt),
                    dec->channels,
                    is->audio_tgt_freq,
                    av_get_sample_fmt_name(is->audio_tgt_fmt),
                    is->audio_tgt_channels);
                break;
            }
            is->audio_src_channel_layout = dec_channel_layout;
            is->audio_src_channels = dec->channels;
            is->audio_src_freq = dec->sample_rate;
            is->audio_src_fmt = dec->sample_fmt;
        }
        resampled_data_size = data_size;
        if (is->swr_ctx) {
#ifdef USE_WAV_DUMP
            if(!is->fhwav2) {
                char tmp[8192];
                snprintf(tmp,sizeof(tmp),"/tmp/slot-%d-orig.wav",is->slotinfo->slotid);
                is->fhwav2=fopen(tmp,"w+");
                if(is->fhwav2)
                    write_wav_header(is->fhwav2, is->audio_src_freq, is->audio_src_channels, 16, 0);
                is->wav2size=0;
            }
            if(is->fhwav2) {
                fwrite(is->frame->data[0],data_size,1,is->fhwav2);
                is->wav2size+=data_size;
            }
#endif
            const uint8_t *in[] = { is->frame->data[0] };
            uint8_t *out[] = {is->audio_buf2};
            len2 = swr_convert(is->swr_ctx, out, sizeof(is->audio_buf2) / is->audio_tgt_channels / av_get_bytes_per_sample(is->audio_tgt_fmt),
                                            in, data_size / dec->channels / av_get_bytes_per_sample(dec->sample_fmt));
            if (len2 < 0) {
                av_log(NULL, AV_LOG_ERROR, "[error] audio_decode_frame():audio_resample() failed\n");
                break;
            }
            if (len2 == sizeof(is->audio_buf2) / is->audio_tgt_channels / av_get_bytes_per_sample(is->audio_tgt_fmt)) {
                av_log(NULL, AV_LOG_ERROR, "[warning] audio_decode_frame(): audio buffer is probably too small\n");
                swr_init(is->swr_ctx);
            }
            is->audio_buf = is->audio_buf2;
            resampled_data_size = len2 * is->audio_tgt_channels * av_get_bytes_per_sample(is->audio_tgt_fmt);
        } else {
            is->audio_buf = is->frame->data[0];
        }

        data_size=resampled_data_size;
#ifdef USE_WAV_DUMP
        if(!is->fhwav1) {
            char tmp[8192];
            snprintf(tmp,sizeof(tmp),"/tmp/slot-%d-conv.wav",is->slotinfo->slotid);
            is->fhwav1=fopen(tmp,"w+");
            is->wav1size=0;
            if(is->fhwav1)
                write_wav_header(is->fhwav1, is->audio_tgt_freq, is->audio_tgt_channels, 16, 0);
        }
        if(is->fhwav1) {
            fwrite(is->audio_buf,data_size,1,is->fhwav1);
            is->wav1size+=data_size;
        }
#endif
        sdlen+=data_size;

        if(fillzero) {
#if USE_AUDIOCORR_ZERO
            fill_zero_package(is, is->audio_buf, data_size);
#else
            fillzero=0;
        }
#endif
#if USE_AUDIOCORR_INETRNPOL

#endif
        pthread_mutex_lock(&is->audio_decode_buffer_mutex);
        if(!is->audio_decode_buffer) {
            is->audio_decode_buffer_size=resampled_data_size+is->audio_decode_buffer_len;
            is->audio_decode_buffer=av_malloc(is->audio_decode_buffer_size);
        }
        if(is->audio_decode_buffer_len+resampled_data_size>is->audio_decode_buffer_size) {
            tmp = is->audio_decode_buffer;
            is->audio_decode_buffer_size=resampled_data_size+is->audio_decode_buffer_len;
            is->audio_decode_buffer=av_malloc(is->audio_decode_buffer_size*2);
            memcpy(is->audio_decode_buffer,tmp,is->audio_decode_buffer_len);
            av_free(tmp);
            is->audio_decode_buffer_size*=2;
        }
        if(last_clock || resampled_data_size) {
            clock=resampled_data_size;
            dlen+=resampled_data_size;
            is->audio_clock_diff_len+=resampled_data_size;
            l=is->audio_tgt_channels * av_get_bytes_per_sample(is->audio_tgt_fmt) * is->audio_tgt_freq;
            clock/=l;
            diff-=clock;
            is->decode_audio_clock+=clock;
        }

        memcpy(is->audio_decode_buffer+is->audio_decode_buffer_len,is->audio_buf,resampled_data_size);
        clock=is->audio_decode_buffer_len;
        is->audio_decode_buffer_len+=resampled_data_size;
        l=is->audio_tgt_channels * av_get_bytes_per_sample(is->audio_tgt_fmt) * is->audio_tgt_freq;
        clock/=l;
        if(first && last_pts != AV_NOPTS_VALUE && last_pts!=is->audio_decode_last_pts && !is->read_enable) {
            if((is->slotinfo->debugflag & DEBUGFLAG_AV)) {
                av_log(NULL, DEBUG_FLAG_LOG_LEVEL, "[debug] audio_decode_frame(): audio decode_buffer_clock: %12.6f = %12.6f - %12.6f \n",is->audio_decode_buffer_clock, is->decode_audio_clock, clock);
            }
        }
        first=0;
        pthread_mutex_unlock(&is->audio_decode_buffer_mutex);
    }

//fprintf(stderr,"Slot: %d write len: %d buf len: %d pts: %8.3f \n",is->slotinfo->slotid,sdlen,is->audio_decode_buffer_len,is->decode_audio_clock);

    clock=dlen;
    l=is->audio_tgt_channels * av_get_bytes_per_sample(is->audio_tgt_fmt) * is->audio_tgt_freq;
    clock/=l;
    if(difflen<0) {
        pthread_mutex_lock(&is->audio_decode_buffer_mutex);
        difflen+=audio_get_from_buffer(is, NULL, -difflen);
        is->audiocorr-=difflen;
//        is->audio_diff+=difflen;
        clock=-difflen;
        l=is->audio_tgt_channels * av_get_bytes_per_sample(is->audio_tgt_fmt) * is->audio_tgt_freq;
        clock/=l;
        is->diffd2clock-=clock;
//        is->decode_audio_clock-=clock;
        pthread_mutex_unlock(&is->audio_decode_buffer_mutex);
    }
    clock=is->audio_decode_buffer_len;
    clock/=l;
//    fprintf(stderr,"Slot: %d audio clock: %8.3f buffer: %8.3f (%8.3f)\n",is->slotinfo->slotid,is->decode_audio_clock,is->decode_audio_clock-clock,clock);
    av_free_packet(pkt);
    return 0;
}

static void audio_decode_flush(VideoState *is)
{
    pthread_mutex_lock(&is->audio_decode_buffer_mutex);
    if(is->audio_decode_buffer)
        av_free(is->audio_decode_buffer);
    is->audio_decode_buffer=NULL;
    is->audio_decode_buffer_len=0;
    is->audio_decode_buffer_size=0;
    is->audio_decode_buffer_clock=0.0;
    is->audio_decode_last_pts=AV_NOPTS_VALUE;
    is->audio_drop=0;
    is->audiocorr=0;
    is->decode_audio_clock=0;
    av_log(NULL, AV_LOG_DEBUG,"[debug] audio_decode_flush(): slot: %d audio decode flush\n",is->slotinfo->slotid);
    pthread_mutex_unlock(&is->audio_decode_buffer_mutex);
}

static double buffered_audio_data(VideoState *is)
{
    double len,l;
    len=is->audio_decode_buffer_len-is->audio_drop;
    if(len<0.0)
        return 0;
    l=is->audio_tgt_channels * av_get_bytes_per_sample(is->audio_tgt_fmt) * is->audio_tgt_freq;
    len=len/l;
    return len;
}

static af_data_t* read_audio_data(void *opaque, af_data_t* basedata, int len, double ctime)
{
    VideoState *is = opaque;
    char* tmp;
    double clock,l;
    double droptime = 0.0;
    int wlen,dlen;
    af_data_t* adata = NULL;
    af_data_t* af_data = NULL;
#if 0
    event_t event;
#endif
    char info[8192];
    int ptsvalid = 1;
    double oldclock,newclock,diffclock,diffiter;
    long long int calclen;
    int speedlen;

    if(basedata->rate && is->speedrate && basedata->rate!=is->speedrate) {
        calclen=len;
        calclen=calclen*basedata->rate/is->speedrate;
        speedlen=af_round_len(basedata, calclen);
//fprintf(stderr,"Slot: %d speedlen: %d calclen: %d [%d %4.2f]\n",is->slotinfo->slotid,speedlen,calclen,len,is->speed);
    } else {
        speedlen=len;
    }
    if(!is->audio_st || is->audio_exit_flag || is->read_enable!=1) {
        is->slotinfo->audio_info.valid=0;
        is->start_time=0;
        if(!is->slotinfo->pausereq)
            return basedata;
    }
    pthread_mutex_lock(&is->audio_decode_buffer_mutex);

    if(speedlen>is->audio_decode_buffer_len) {
#if 0
        double mc = get_master_clock(is);
        mc=is->decode_audio_clock;
        fprintf(stderr,"Slot: %d len: %8.3f mc: %8.3f stop?: %d \n",is->slotinfo->slotid,is->slotinfo->length,mc,(is->slotinfo->length<mc && is->slotinfo->length+2.0>=mc));
        if(is->slotinfo->length>0.0 && is->slotinfo->length<mc && is->slotinfo->length+2.0>=mc)
        {
            pthread_mutex_unlock(&is->audio_decode_buffer_mutex);
            av_log(NULL, DEBUG_FLAG_LOG_LEVEL, "[debug] read_audio_data(): slot: %d, send stop event (master clock: %7.3f duration: %7.3f\n",is->slotinfo->slotid,mc,is->slotinfo->length);
            is->slotinfo->playflag=0;
            is->slotinfo->pauseafterload=1;
            is->slotinfo->reopen=0;
            event.type = FF_STOP_EVENT;
            event.data = is->slotinfo->streampriv;
            push_event(is->slotinfo->eventqueue, &event);
fprintf(stderr,"Slot: %d STOP!!!\n",is->slotinfo->slotid);
            return basedata;
        }
#endif
    }
    if(globalpause(is->slotinfo) && !is->slotinfo->pausereq && speedlen<=is->audio_decode_buffer_len) {
        pthread_mutex_unlock(&is->audio_decode_buffer_mutex);
        return basedata;
    }
    if(is->slotinfo->pausereq) {
        clock=GLOBAL_PAUSE_LIMIT;
        l=is->audio_tgt_channels * av_get_bytes_per_sample(is->audio_tgt_fmt) * is->audio_tgt_freq;
        clock=clock*l;
        wlen=clock;
        if(is->audio_decode_buffer_len>=wlen+is->audio_drop && is->audio_decode_buffer_len>=speedlen+is->audio_drop) {
            is->slotinfo->pausereq=0;
        } else {
        }
    }
    if(globalpause(is->slotinfo))
    {
        pthread_mutex_unlock(&is->audio_decode_buffer_mutex);
        return basedata;
    }
    if(ispaused(is) || is->pause_seek) {
        is->start_time=0;
        pthread_mutex_unlock(&is->audio_decode_buffer_mutex);
        return basedata;
    }
    diffiter=len;
    l=is->audio_tgt_channels * av_get_bytes_per_sample(is->audio_tgt_fmt) * is->audio_tgt_freq;
    diffiter/=l;
    diffiter-=xplayer_global_status->itertime;
    is->slotinfo->audio_callback_time = av_gettime();
    if(is->slotinfo->af && (is->slotinfo->setvolume || is->slotinfo->setmute)) {
        if(is->slotinfo->setmute) {
            if(is->slotinfo->mute) {
                af_volume_level(is->slotinfo->af, 0);
            } else {
                af_volume_level(is->slotinfo->af, is->slotinfo->volume);
                is->slotinfo->setvolume=0;
            }
            is->slotinfo->setmute=0;
        } else if(is->slotinfo->setvolume) {
            af_volume_level(is->slotinfo->af, is->slotinfo->volume);
            is->slotinfo->setvolume=0;
        }
    }
    if(!strcmp(is->ic->iformat->name, "rtsp") && is->ic->duration==AV_NOPTS_VALUE) {
//        is->audio_clock_diff_len=0;
        is->audio_diff=0;
    }
    is->audio_buf_size = sizeof(is->silence_buf);
#ifdef USE_DEBUG_LIB
    if(dmem && is->slotinfo->slotid<MAX_DEBUG_SLOT) {
        slotdebug_t* slotdebugs = debugmem_getslot(dmem);
        if(slotdebugs) {
            slotdebugs[is->slotinfo->slotid].ablen=is->audio_decode_buffer_len;
        }
    }
#endif

    if(is->audio_drop) {
        dlen=is->audio_drop;
        if(dlen>is->audio_decode_buffer_len)
            dlen=is->audio_decode_buffer_len;
        if(dlen) {
            is->audio_decode_buffer_len-=dlen;
            if(is->audio_decode_buffer_len) {
                tmp=av_malloc(is->audio_decode_buffer_len);
                memcpy(tmp,is->audio_decode_buffer+dlen,is->audio_decode_buffer_len);
                memcpy(is->audio_decode_buffer,tmp,is->audio_decode_buffer_len);
                av_free(tmp);
            }
            is->audio_drop-=dlen;
        }
    }
    if(speedlen>is->audio_decode_buffer_len) {
#ifdef DEBUG_AUDIOCORR
fprintf(stderr,"Slot: %d len: %d buff: %d diff: %d <--------------------------\n",is->slotinfo->slotid,speedlen,is->audio_decode_buffer_len,is->audio_decode_buffer_len-speedlen);
#endif
        ptsvalid = 0;
        is->audio_drop+=speedlen-is->audio_decode_buffer_len;
        speedlen=is->audio_decode_buffer_len;
        double mc = get_master_clock(is);
        mc=is->decode_audio_clock;
//fprintf(stderr,"Slot: %d len: %8.3f mc: %8.3f stop?: %d \n",is->slotinfo->slotid,is->slotinfo->length,mc,(is->slotinfo->length<mc && is->slotinfo->length+2.0>=mc));
        if(is->slotinfo->length>0.0 && is->slotinfo->length<mc && is->slotinfo->length+2.0>=mc)
        {
#if 0
            av_log(NULL, DEBUG_FLAG_LOG_LEVEL, "[debug] read_audio_data(): slot: %d, send stop event (master clock: %7.3f duration: %7.3f\n",is->slotinfo->slotid,mc,is->slotinfo->length);
            is->slotinfo->playflag=0;
            is->slotinfo->pauseafterload=1;
            is->slotinfo->reopen=0;
            event.type = FF_STOP_EVENT;
            event.data = is->slotinfo->streampriv;
            push_event(is->slotinfo->eventqueue, &event);
#endif
        } else {
            if(!is->realtime)
                is->slotinfo->pausereq=1;
        }
//        is->audio_drop=0;
    } else if(is->slotinfo->pausereq) {
        clock=GLOBAL_PAUSE_LIMIT;
        l=is->audio_tgt_channels * av_get_bytes_per_sample(is->audio_tgt_fmt) * is->audio_tgt_freq;
        clock=clock*l;
        wlen=clock;
        if(is->audio_decode_buffer_len>=wlen+is->audio_drop) {
            is->slotinfo->pausereq=0;
        }
    }
    if(!speedlen) {
#ifdef DEBUG_AUDIOCORR
        fprintf(stderr,"Slot: %d len %d\n",is->slotinfo->slotid,speedlen);
#endif
        pthread_mutex_unlock(&is->audio_decode_buffer_mutex);
        return basedata;
    }
    if(speedlen && is->audio_tgt_channels) {
        adata=af_empty(is->audio_tgt_freq, is->audio_tgt_channels, AUDIO_FORMAT, 0, speedlen);
        memcpy(adata->audio,is->audio_decode_buffer,speedlen);
        is->audio_decode_buffer_len-=speedlen;
    }
    if(is->audio_decode_buffer_len) {
        tmp=av_malloc(is->audio_decode_buffer_len);
        memcpy(tmp,is->audio_decode_buffer+speedlen,is->audio_decode_buffer_len);
        memcpy(is->audio_decode_buffer,tmp,is->audio_decode_buffer_len);
        av_free(tmp);
    }
    memset(info,0,sizeof(info));
    clock=0.0;
    if(speedlen && ptsvalid) {
        l=is->audio_tgt_channels * av_get_bytes_per_sample(is->audio_tgt_fmt) * is->audio_tgt_freq;
        clock=speedlen;
        clock/=l;
        snprintf(info,sizeof(info),"slot: %d audio: %8.3f decode_buffer_clock: %8.3f  old: %8.3f diff: %8.3f (clock: %8.3f len: %8.3f)\n",is->slotinfo->slotid,
                                    is->audio_clock,(is->decode_audio_clock - clock - is->audio_decode_buffer_len/l),is->audio_decode_buffer_clock,
                                    (is->decode_audio_clock - clock - is->audio_decode_buffer_len/l)-is->audio_decode_buffer_clock,
                                    clock,is->audio_decode_buffer_len/l);
        oldclock=get_audio_clock(is);
        is->audio_decode_buffer_clock=is->decode_audio_clock - clock - is->audio_decode_buffer_len/l - droptime;
        is->audio_clock=is->decode_audio_clock - (double)(is->audio_decode_buffer_len+speedlen)/l;
        if(is->audio_clock) {
            is->slotinfo->audio_callback_time = av_gettime();
            is->audio_current_pts = is->audio_clock;
            is->audio_current_pts_drift = is->audio_current_pts - is->slotinfo->audio_callback_time / 1000000.0;
        }
        newclock=get_audio_clock(is);
        diffclock=(newclock-oldclock)*1000.0;
        is->diffclock+=diffclock;

#if 0
        if(is->dbgcnt) {
            VideoState* mis = get_group_master(is,1);
            double mtime = get_master_clock(mis);
            double stime = get_master_clock(is);
            fprintf(stderr,"Slot: %d mc: %8.3f => %8.3f sc: %8.3f => %8.3f diff: %8.3f ac: %8.3f\n",
                is->slotinfo->slotid,
                mtime,mtime+mis->slotinfo->grouptime,
                stime,stime+is->slotinfo->grouptime,
                (mtime+mis->slotinfo->grouptime) - (stime+is->slotinfo->grouptime),
                is->decode_audio_clock);
//            is->dbgcnt--;
        }
#endif

#if 0
        if(diffclock<-0.0001 || diffclock>0.0001) {
            fprintf(stderr,"Slot: %d old clock: %8.3f new clock: %8.3f diff: %8.3f ms sum: %10.3f ms decode diff: %10.3f ms (%10.3f) iter diff: %8.3f ms\n",is->slotinfo->slotid,oldclock,newclock,diffclock,is->diffclock,is->diffdclock*1000.0,is->diffd2clock*1000.0,diffiter*1000.0);
        }
#endif
#if 0
        if(diffclock<-1.0 || diffclock>1.0 || 1) {
            fprintf(stderr,"Slot: %d old clock: %8.3f new clock: %8.3f diff: %8.3f ms sum: %10.3f ms decode diff: %10.3f ms (%10.3f) iter diff: %8.3f ms\n",is->slotinfo->slotid,oldclock,newclock,diffclock,is->diffclock,is->diffdclock*1000.0,is->diffd2clock*1000.0,diffiter*1000.0);
        }
#endif
#ifdef USE_DEBUG_LIB
        if(dmem && is->slotinfo->slotid<MAX_DEBUG_SLOT) {
            slotdebug_t* slotdebugs = debugmem_getslot(dmem);
            if(slotdebugs) {
                slotdebugs[is->slotinfo->slotid].audio_clock=is->audio_clock;
                slotdebugs[is->slotinfo->slotid].master_clock=get_master_clock(is);
            }
        }
#endif
    } else {
        is->audio_decode_buffer_clock=0.0;
#ifdef DEBUG_AUDIOCORR
        fprintf(stderr,"Slot: %d len: %d ptsvalid: %d <-------------------\n",is->slotinfo->slotid,speedlen,ptsvalid);
#endif
    }
    l=is->audio_tgt_channels * av_get_bytes_per_sample(is->audio_tgt_fmt) * is->audio_tgt_freq;
    if((is->slotinfo->debugflag & DEBUGFLAG_AV)) {
        av_log(NULL, DEBUG_FLAG_LOG_LEVEL, "[debug] read_audio_data(): slot: %d audio decode_buffer_clock: %12.6f = %12.6f - %12.6f len: %d %d \n",
                is->slotinfo->slotid,
                is->audio_decode_buffer_clock, is->decode_audio_clock, clock, speedlen, is->audio_decode_buffer_len);
    }
    pthread_mutex_unlock(&is->audio_decode_buffer_mutex);

    is->audio_write_buf_size = speedlen;
    /* Let's assume the audio driver that is used by SDL has two periods. */
    if((is->slotinfo->debugflag & DEBUGFLAG_AV)) {
        av_log(NULL, DEBUG_FLAG_LOG_LEVEL, "[debug] read_audio_data(): slot: %d audio pts drift: %12.6f = %12.6f - %12.6f \n",
                is->slotinfo->slotid,
                is->audio_current_pts_drift, is->audio_current_pts, is->slotinfo->audio_callback_time / 1000000.0);
    }
    is->slotinfo->audio_info.valid=1;
    is->slotinfo->audio_info.streamid=is->audio_stream;
    is->slotinfo->audio_info.channels=is->audio_src_channels;
    is->slotinfo->audio_info.rate=is->audio_src_freq;
    is->slotinfo->audio_info.format=audio_ffmpeg_format(is->audio_src_fmt);
    if(!is->slotinfo->af) {
        is->slotinfo->af=af_init(basedata->rate, basedata->nch, basedata->format, basedata->bps, 5.0);
        is->speed=1.0;
        is->oldspeed=is->speed;
        af_volume_level(is->slotinfo->af, is->slotinfo->volume);
        is->slotinfo->setvolume=0;
    }

    if(adata) {
        af_data = af_play(is->slotinfo->af,adata);
    }
    if(!is->start_time) {
        is->start_time=av_gettime();
        is->start_pts=is->audio_current_pts;
        is->start_pts=get_audio_clock(is);
        is->slotinfo->start_pts=is->start_pts;
    }
    double spts = (av_gettime()-is->start_time) / 1000000.0;
    double rpts = is->audio_current_pts-is->start_pts;
    rpts = get_audio_clock(is)-is->start_pts;
    double max_diff = spts-rpts;
    if(max_diff<0.0)
        max_diff=-max_diff;
    if(max_diff>is->max_diff) {
//        fprintf(stderr,"slot: %d max: %12.6f spts: %12.6f rpts: %12.6f apts: %12.6f len: %d\n",is->slotinfo->slotid,max_diff,spts,rpts,is->audio_current_pts,len);
        is->max_diff=max_diff;
    }

#if 0
if(is->speed!=1.0 && adata) {
    fprintf(stderr,"Slot: %d speed: %4.2f len: %d alen: %d [%d] speedlen: %d adatalen: %d af_rate: %d arate: %d\n",is->slotinfo->slotid,is->speed,len,af_data->len,len-af_data->len,speedlen,adata->len,af_data->rate,adata->rate);
}
#endif
    af_data->rate=basedata->rate;
    basedata=af_data_mixer(basedata, 0, 0, af_data);
    if(adata)
        af_data_free(adata);
    if(!is->paused && is->slotinfo->groupid && !is->realtime) {
        VideoState* mis = get_group_master(is,1);
        double mtime = get_master_clock(mis)+mis->slotinfo->grouptime;
        double stime = get_master_clock(is)+is->slotinfo->grouptime;
        if(mis!=is) {
            if(mtime>stime+0.1) {
                is->speed=1.10;
            } else if(mtime>stime+0.02) {
                is->speed=1.05;
            } else if(mtime<stime-0.1) {
                is->speed=0.90;
            } else if(mtime<stime-0.02) {
                is->speed=0.95;
            } else {
                is->speed=1.00;
            }
        } else {
//            is->speed=1.0;
        }
    }
    if(is->oldspeed!=is->speed) {
        is->oldspeed=is->speed;
        if(is->slotinfo->af && is->audio_tgt_freq) {
            af_uninit(is->slotinfo->af);
            is->speedrate=round((2.0-is->speed)*basedata->rate);
fprintf(stderr,"Slot: %d speed: %4.2f rate: %d speedrate: %d \n",is->slotinfo->slotid,is->speed,basedata->rate,is->speedrate);
            is->slotinfo->af=af_init(is->speedrate, basedata->nch, basedata->format, basedata->bps, 5.0);
            af_volume_level(is->slotinfo->af, is->slotinfo->volume);
            if(is->speed==1.0 && !is->realtime) {
                is->groupsync=0;
            }
        }
//fprintf(stderr,"Slot: %d master: %8.3f slave: %8.3f => %8.3f groupsync: %d \n",is->slotinfo->slotid,mtime,stime,(stime+is->slotinfo->grouptime)-(mtime+mis->slotinfo->grouptime),is->groupsync);
    }

#ifdef USE_DEBUG_LIB
    clock=is->audiocorr;
    l=is->audio_tgt_channels * av_get_bytes_per_sample(is->audio_tgt_fmt) * is->audio_tgt_freq;
    clock/=l;
    if(dmem && is->slotinfo->slotid<MAX_DEBUG_SLOT) {
        slotdebug_t* slotdebugs = debugmem_getslot(dmem);
        if(slotdebugs) {
            slotdebugs[is->slotinfo->slotid].audio_diff=clock;
            slotdebugs[is->slotinfo->slotid].abytes+=speedlen;
            slotdebugs[is->slotinfo->slotid].acpts=is->audio_current_pts;
            slotdebugs[is->slotinfo->slotid].audio_real_diff=spts-rpts;
            slotdebugs[is->slotinfo->slotid].real_clock=spts;
            slotdebugs[is->slotinfo->slotid].speed=is->speed;
        }
    }
#endif
    if(!af_data) {
        return basedata;
    }
    return basedata;
}

#if defined(__APPLE__)
static int vda_get_buffer(struct AVCodecContext *c, AVFrame *pic)
{
    pic->data[0]=1;
    pic->data[1]=1;
    pic->data[3]=1;
//    pic->age=1;
    pic->type=FF_BUFFER_TYPE_USER;
	return 1;
}

/*
static int vda_release_buffer(struct AVCodecContext *c, AVFrame *pic)
{
}
*/
#endif

/// http://code.mythtv.org/doxygen/privatedecoder__vda_8cpp_source.html:

// TODO: refactor this so as not to need these ffmpeg routines.
// These are not exposed in ffmpeg's API so we dupe them here.
// AVC helper functions for muxers,
//  * Copyright (c) 2006 Baptiste Coudurier <baptiste.coudurier@smartjog.com>
// This is part of FFmpeg
//  * License as published by the Free Software Foundation; either
//  * version 2.1 of the License, or (at your option) any later version.
#define VDA_RB16(x)                             \
    ((((const uint8_t*)(x))[0] <<  8) |         \
    ((const uint8_t*)(x)) [1])

#define VDA_RB24(x)                             \
    ((((const uint8_t*)(x))[0] << 16) |         \
    (((const uint8_t*)(x))[1] <<  8) |          \
    ((const uint8_t*)(x))[2])

#define VDA_RB32(x)                             \
    ((((const uint8_t*)(x))[0] << 24) |         \
    (((const uint8_t*)(x))[1] << 16) |          \
    (((const uint8_t*)(x))[2] <<  8) |          \
    ((const uint8_t*)(x))[3])

#define VDA_WB32(p, d) {                        \
    ((uint8_t*)(p))[3] = (d);                   \
    ((uint8_t*)(p))[2] = (d) >> 8;              \
    ((uint8_t*)(p))[1] = (d) >> 16;             \
    ((uint8_t*)(p))[0] = (d) >> 24; }


static const uint8_t *avc_find_startcode_internal(const uint8_t *p, const uint8_t *end)
{
    const uint8_t *a = p + 4 - ((intptr_t)p & 3);

    for (end -= 3; p < a && p < end; p++)
    {
        if (p[0] == 0 && p[1] == 0 && p[2] == 1)
            return p;
    }

    for (end -= 3; p < end; p += 4)
    {
        uint32_t x = *(const uint32_t*)p;
        if ((x - 0x01010101) & (~x) & 0x80808080) // generic
        {
            if (p[1] == 0)
            {
                if (p[0] == 0 && p[2] == 1)
                    return p;
                if (p[2] == 0 && p[3] == 1)
                    return p+1;
            }
            if (p[3] == 0)
            {
                if (p[2] == 0 && p[4] == 1)
                    return p+2;
                if (p[4] == 0 && p[5] == 1)
                    return p+3;
            }
        }
    }

    for (end += 3; p < end; p++)
    {
        if (p[0] == 0 && p[1] == 0 && p[2] == 1)
            return p;
    }
    return end + 3;
}

const uint8_t *avc_find_startcode(const uint8_t *p, const uint8_t *end)
{
    const uint8_t *out= avc_find_startcode_internal(p, end);
    if (p<out && out<end && !out[-1])
        out--;
    return out;
}

const int avc_parse_nal_units(AVIOContext *pb, const uint8_t *buf_in, int size)
{
    const uint8_t *p = buf_in;
    const uint8_t *end = p + size;
    const uint8_t *nal_start, *nal_end;

    size = 0;
    nal_start = avc_find_startcode(p, end);
    while (nal_start < end)
    {
        while (!*(nal_start++));
        nal_end = avc_find_startcode(nal_start, end);
        avio_wb32(pb, nal_end - nal_start);
        avio_write(pb, nal_start, nal_end - nal_start);
        size += 4 + nal_end - nal_start;
        nal_start = nal_end;
    }
    return size;
}

const int avc_parse_nal_units_buf(const uint8_t *buf_in,
                                  uint8_t **buf, int *size)
{
    AVIOContext *pb;
    int ret = avio_open_dyn_buf(&pb);
    if (ret < 0)
        return ret;

    avc_parse_nal_units(pb, buf_in, *size);

    av_freep(buf);
    *size = avio_close_dyn_buf(pb, buf);
    return 0;
}

const int isom_write_avcc(AVIOContext *pb, const uint8_t *data, int len)
{
    // extradata from bytestream h264, convert to avcC atom data for bitstream
    if (len > 6)
    {
        /* check for h264 start code */
        if (VDA_RB32(data) == 0x00000001 || VDA_RB24(data) == 0x000001)
        {
            uint8_t *buf=NULL, *end, *start;
            uint32_t sps_size=0, pps_size=0;
            uint8_t *sps=0, *pps=0;
            int ret = avc_parse_nal_units_buf(data, &buf, &len);
            if (ret < 0)
                return ret;
            start = buf;
            end = buf + len;

            /* look for sps and pps */
            while (buf < end)
            {
                unsigned int size;
                uint8_t nal_type;
                size = VDA_RB32(buf);
                nal_type = buf[4] & 0x1f;
                if (nal_type == 7) /* SPS */
                {
                    sps = buf + 4;
                    sps_size = size;

                    //parse_sps(sps+1, sps_size-1);
                }
                else if (nal_type == 8) /* PPS */
                {
                    pps = buf + 4;
                    pps_size = size;
                }
                buf += size + 4;
            }
            if (!sps)
            {
                av_log(NULL, AV_LOG_ERROR, "[error] isom_write_avcc(): Invalid data (sps)\n");
                return -1;
            }

            avio_w8(pb, 1); /* version */
            avio_w8(pb, sps[1]); /* profile */
            avio_w8(pb, sps[2]); /* profile compat */
            avio_w8(pb, sps[3]); /* level */
            avio_w8(pb, 0xff); /* 6 bits reserved (111111) + 2 bits nal size length - 1 (11) */
            avio_w8(pb, 0xe1); /* 3 bits reserved (111) + 5 bits number of sps (00001) */

            avio_wb16(pb, sps_size);
            avio_write(pb, sps, sps_size);
            if (pps)
            {
                avio_w8(pb, 1); /* number of pps */
                avio_wb16(pb, pps_size);
                avio_write(pb, pps, pps_size);
            }
            av_free(start);
        }
        else
        {
            avio_write(pb, data, len);
        }
    }
    return 0;
}

#if defined(__APPLE__)
static enum PixelFormat vda_get_format(struct AVCodecContext *s, const enum PixelFormat * fmt)
{
    if(s->request_sample_fmt == PIX_FMT_VDA_VLD && s->codec->id==CODEC_ID_H264)
    {
        return PIX_FMT_VDA_VLD;
    }
    return avcodec_default_get_format(s, fmt);
}
#endif

/* open a given stream. Return 0 if OK */
static int stream_component_open(VideoState *is, int stream_index)
{
#if defined(__APPLE__)
    int n;
#endif
    AVFormatContext *ic = is->ic;
    AVCodecContext *avctx;
    AVCodec *codec;
//    SDL_AudioSpec wanted_spec, spec;
    audio_spec_t wanted_spec, spec;
    AVDictionary *opts = NULL;
    AVDictionaryEntry *t = NULL;
    int64_t wanted_channel_layout = 0;

    if (stream_index < 0 || stream_index >= ic->nb_streams)
        return -1;
    avctx = ic->streams[stream_index]->codec;

    codec = avcodec_find_decoder(avctx->codec_id);
    opts = filter_codec_opts(is->slotinfo->codec_opts, codec, ic, ic->streams[stream_index]);

    switch(avctx->codec_type){
        case AVMEDIA_TYPE_AUDIO   : if(is->slotinfo->audio_codec_name   ) codec= avcodec_find_decoder_by_name(   is->slotinfo->audio_codec_name); break;
        case AVMEDIA_TYPE_SUBTITLE: if(is->slotinfo->subtitle_codec_name) codec= avcodec_find_decoder_by_name(is->slotinfo->subtitle_codec_name); break;
        case AVMEDIA_TYPE_VIDEO   : if(is->slotinfo->video_codec_name   ) codec= avcodec_find_decoder_by_name(   is->slotinfo->video_codec_name); break;
        default: break;
    }
    if (!codec)
        return -1;

    avctx->workaround_bugs = is->slotinfo->workaround_bugs;
    avctx->lowres = is->slotinfo->lowres;
    if(avctx->lowres > codec->max_lowres){
        av_log(avctx, AV_LOG_WARNING, "[warning] stream_component_open(): The maximum value for lowres supported by the decoder is %d\n",
                codec->max_lowres);
        avctx->lowres= codec->max_lowres;
    }
    if(avctx->lowres) avctx->flags |= CODEC_FLAG_EMU_EDGE;
    avctx->idct_algo= is->slotinfo->idct;
    if(is->slotinfo->fast) avctx->flags2 |= CODEC_FLAG2_FAST;
    avctx->skip_frame= is->slotinfo->skip_frame;
    avctx->skip_idct= is->slotinfo->skip_idct;
    avctx->skip_loop_filter= is->slotinfo->skip_loop_filter;
    avctx->error_concealment= is->slotinfo->error_concealment;
    if(avctx->codec_type==AVMEDIA_TYPE_VIDEO && is->slotinfo->imgfmt) {
        avctx->request_sample_fmt = imgfmt2pixfmt(is->slotinfo->imgfmt);
#if defined(__APPLE__)
        if(is->slotinfo->vda) {
            memset(&is->vdactx,0,sizeof(struct vda_context));
            is->vdactx.decoder = NULL;
            is->vdactx.queue = NULL;
            is->vdactx.width = avctx->width;
            is->vdactx.height = avctx->height;
            av_log(NULL, AV_LOG_DEBUG, "[debug] stream_component_open(): w: %d, h: %d\n", avctx->width, avctx->height);
            is->vdactx.format = 'avc1';
            is->vdactx.cv_pix_fmt_type = kCVPixelFormatType_422YpCbCr8;
            av_log(NULL, AV_LOG_DEBUG,"[debug] stream_component_open(): Extra data: (%d):\n",avctx->extradata_size);
            for(n=0;n<avctx->extradata_size;n++) {
                av_log(NULL, AV_LOG_DEBUG,"%02x ",avctx->extradata[n]);
            }
            av_log(NULL, AV_LOG_DEBUG,"\n");
            uint8_t *extradata = NULL;
            int extrasize = avctx->extradata_size;
/// Convert annex-b to AVCC:
            if(extrasize>=7 && avctx->extradata[0]==0x00 && avctx->extradata[1]==0x00 && avctx->extradata[2]==0x01) {
                unsigned int spssize = 0;
                unsigned int ppssize = 0;
                int i;
                for(i=3;i<extrasize-3;i++) {
                    if(avctx->extradata[i]==0x00 && avctx->extradata[i+1]==0x00 && avctx->extradata[i+2]==0x01) {
                        spssize=i-3;
                        break;
                    }
                }
                if(spssize && extrasize>=spssize+6) {
                    ppssize=extrasize-6-spssize;
                }
                if(spssize && ppssize) {
                    extradata = av_malloc(11+spssize+ppssize);
                    extradata[0] = 0x01;
                    extradata[1] = avctx->profile;
                    extradata[2] = 0x00;    /// Compatible: 0
                    extradata[3] = avctx->level;
                    extradata[4] = 0xfc | 0x03;
                    extradata[5] = 0xe0 | 0x01;
                    extradata[6] = spssize >> 8;
                    extradata[7] = spssize & 0xff;
                    memcpy(extradata+8, avctx->extradata+3, spssize);
                    extradata[8+spssize] = 0x01;
                    extradata[9+spssize] = ppssize >> 8;
                    extradata[10+spssize] = ppssize & 0xff;
                    memcpy(extradata+11+spssize, avctx->extradata+6+spssize, ppssize);
//               00 00 01 67 42 00 29 e2 90 0f 00 44 fc b8 0b 70 10 10 1a 41 e2 44 54 00 00 01 68 ce 3c 80
//01 42 00 29 ff e1 00 14 67 42 00 29 e2 90 0f 00 44 fc b8 0b 70 10 10 1a 41 e2 44 54 01 00 04 68 ce 3c 80
// 0  1  2  3 4  5  6  7
                    extrasize = 11+spssize+ppssize;
                }
            }
            if(!extradata) {
                if(extrasize) {
                    extradata = av_malloc(extrasize);
                    memcpy(extradata,avctx->extradata, extrasize);
                }
            }
            av_log(NULL, AV_LOG_DEBUG,"[debug] stream_component_open(): Extra data for VDA: (%d):\n",extrasize);
            for(n=0;n<extrasize;n++) {
                av_log(NULL, AV_LOG_DEBUG,"%02x ",extradata[n]);
            }
            int status = ff_vda_create_decoder(&is->vdactx, extradata, extrasize);
            if(extradata)
                av_free(extradata);
            if(!status) {
                avctx->hwaccel_context = &is->vdactx;
                is->usesvda=is->slotinfo->vda;
                is->slotinfo->usesvda=is->usesvda;
                avctx->get_format=vda_get_format;
                avctx->get_buffer=vda_get_buffer;
                avctx->request_sample_fmt = PIX_FMT_VDA_VLD;
                av_log(NULL, AV_LOG_INFO,"[info] stream_component_open(): VDA decoder init OK\n");
            } else {
                av_log(NULL, AV_LOG_ERROR, "[error] stream_component_open(): VDA decoder init ERROR\n");
            }
        }
#endif
    }
    if(codec->capabilities & CODEC_CAP_DR1)
        avctx->flags |= CODEC_FLAG_EMU_EDGE;

    if (avctx->codec_type == AVMEDIA_TYPE_AUDIO) {
        wanted_channel_layout = (avctx->channel_layout && avctx->channels == av_get_channel_layout_nb_channels(avctx->channels)) ? avctx->channel_layout : av_get_default_channel_layout(avctx->channels);
        wanted_channel_layout &= ~AV_CH_LAYOUT_STEREO_DOWNMIX;
        wanted_spec.channels = av_get_channel_layout_nb_channels(wanted_channel_layout);
        wanted_spec.freq = avctx->sample_rate;
        if (wanted_spec.freq <= 0 || wanted_spec.channels <= 0) {
            av_log(NULL, AV_LOG_ERROR, "[error] stream_component_open(): Invalid sample rate or channel count!\n");
            return -1;
        }
        av_log(NULL, AV_LOG_DEBUG,"[debug] stream_component_open(): audio: freq: %d ch: %d\n",avctx->sample_rate,av_get_channel_layout_nb_channels(wanted_channel_layout));
    }

    if (!codec ||
        avcodec_open2(avctx, codec, &opts) < 0)
        return -1;
    if ((t = av_dict_get(opts, "", NULL, AV_DICT_IGNORE_SUFFIX))) {
        av_log(NULL, AV_LOG_ERROR, "[error] stream_component_open(): Option %s not found.\n", t->key);
        return AVERROR_OPTION_NOT_FOUND;
    }

    /* prepare audio output */
    if (avctx->codec_type == AVMEDIA_TYPE_AUDIO) {
        wanted_spec.format = AF_FORMAT_S16_LE;
        spec = wanted_spec;
        spec.freq=AUDIO_RATE;
        spec.channels=AUDIO_CHANNELS;
        if (spec.format != AF_FORMAT_S16_LE) {
            av_log(NULL, AV_LOG_ERROR, "[error] stream_component_open(): advised audio format %d is not supported!\n", spec.format);
            return -1;
        }
        if (spec.channels != wanted_spec.channels) {
            wanted_channel_layout = av_get_default_channel_layout(spec.channels);
            if (!wanted_channel_layout) {
                av_log(NULL, AV_LOG_ERROR, "[error] stream_component_open(): advised channel count %d is not supported!\n", spec.channels);
                return -1;
            }
        }
        is->audio_src_fmt = is->audio_tgt_fmt = AV_SAMPLE_FMT_S16;
        is->audio_src_freq = is->audio_tgt_freq = spec.freq;
        is->audio_src_channel_layout = is->audio_tgt_channel_layout = wanted_channel_layout;
        is->audio_src_channels = is->audio_tgt_channels = spec.channels;
    }

    ic->streams[stream_index]->discard = AVDISCARD_DEFAULT;
    switch(avctx->codec_type) {
    case AVMEDIA_TYPE_AUDIO:
        is->audio_stream = stream_index;
        is->audio_st = ic->streams[stream_index];
        is->audio_buf_size = 0;
        is->audio_buf_index = 0;

        /* init averaging filter */
        is->audio_diff_avg_coef = exp(log(0.01) / AUDIO_DIFF_AVG_NB);
        is->audio_diff_avg_count = 0;
        /* since we do not have a precise anough audio fifo fullness,
           we correct audio sync only if larger than this threshold */
        is->audio_diff_threshold = 2.0 * SDL_AUDIO_BUFFER_SIZE / wanted_spec.freq;

        memset(&is->audio_pkt, 0, sizeof(is->audio_pkt));
        packet_queue_init(is, &is->audioq);
        break;
    case AVMEDIA_TYPE_VIDEO:
        is->video_stream = stream_index;
        is->video_st = ic->streams[stream_index];

        packet_queue_init(is, &is->videoq);
        pthread_create(&is->video_tid, NULL, video_thread, is);
#ifdef THREAD_DEBUG
        av_log(NULL, AV_LOG_DEBUG,"[debug] stream_component_open(): pthread_create: #4498 slot: %d 0x%x \n",is->slotinfo->slotid,(unsigned int)is->video_tid);
#endif
        break;
    case AVMEDIA_TYPE_SUBTITLE:
        is->subtitle_stream = stream_index;
        is->subtitle_st = ic->streams[stream_index];
        packet_queue_init(is, &is->subtitleq);
        pthread_create(&is->subtitle_tid, NULL,subtitle_thread, is);
#ifdef THREAD_DEBUG
        av_log(NULL, AV_LOG_DEBUG,"[debug] stream_component_open(): pthread_create: #4505 slot: %d 0x%x \n",is->slotinfo->slotid,(unsigned int)is->subtitle_tid);
#endif
        break;
    default:
        break;
    }
    return 0;
}

static void stream_component_close(VideoState *is, int stream_index)
{
    AVFormatContext *ic = is->ic;
    AVCodecContext *avctx;

    if (stream_index < 0 || stream_index >= ic->nb_streams)
        return;
    avctx = ic->streams[stream_index]->codec;

    switch(avctx->codec_type) {
    case AVMEDIA_TYPE_AUDIO:
        packet_queue_abort(&is->audioq);
        packet_queue_end(is, &is->audioq);
        pthread_mutex_lock(&is->slotinfo->audiomutex);
        is->slotinfo->audiolock=1;
        pthread_mutex_unlock(&is->slotinfo->audiomutex);
        audio_decode_flush(is);
        if (is->swr_ctx)
            swr_free(&is->swr_ctx);
        av_free_packet(&is->audio_pkt);
        av_freep(&is->audio_buf1);
        is->audio_buf = NULL;
        av_freep(&is->frame);

        if (is->rdft) {
            av_rdft_end(is->rdft);
            av_freep(&is->rdft_data);
            is->rdft = NULL;
            is->rdft_bits = 0;
        }
        break;
    case AVMEDIA_TYPE_VIDEO:
        packet_queue_abort(&is->videoq);

        /* note: we also signal this mutex to make sure we deblock the
           video thread in all cases */
        pthread_mutex_lock(&is->pictq_mutex);
        pthread_cond_signal(&is->pictq_cond);
        pthread_mutex_unlock(&is->pictq_mutex);

        pthread_join(is->video_tid, NULL);

        packet_queue_end(is, &is->videoq);
#if defined(__APPLE__)
        if(is->usesvda) {
            ff_vda_destroy_decoder(&is->vdactx);
            is->usesvda=0;
            memset(&is->vdactx,0,sizeof( struct vda_context));
        }
#endif
        break;
    case AVMEDIA_TYPE_SUBTITLE:
        packet_queue_abort(&is->subtitleq);

        /* note: we also signal this mutex to make sure we deblock the
           video thread in all cases */
        pthread_mutex_lock(&is->subpq_mutex);
        is->subtitle_stream_changed = 1;

        pthread_cond_signal(&is->subpq_cond);
        pthread_mutex_unlock(&is->subpq_mutex);

        pthread_join(is->subtitle_tid, NULL);

        packet_queue_end(is, &is->subtitleq);
        break;
    default:
        break;
    }

    ic->streams[stream_index]->discard = AVDISCARD_ALL;
    avcodec_close(avctx);
    switch(avctx->codec_type) {
    case AVMEDIA_TYPE_AUDIO:
        is->audio_st = NULL;
        is->audio_stream = -1;
        break;
    case AVMEDIA_TYPE_VIDEO:
        is->video_st = NULL;
        is->video_stream = -1;
        break;
    case AVMEDIA_TYPE_SUBTITLE:
        is->subtitle_st = NULL;
        is->subtitle_stream = -1;
        break;
    default:
        break;
    }
}

/* since we have only one decoding thread, we can use a global
   variable instead of a thread local variable */
static VideoState *global_video_state;

static int decode_interrupt_cb(void *ctx)
{
//fprintf(stderr,"decode_interrupt_cb(%p)\n",ctx);
    VideoState *is = ctx;
    if(is) {
        if(is->seek_req==1 && is->reading) {
            if(is->readingtime+3.0<xplayer_clock()) {
//    fprintf(stderr,"Slot: %d interrupt!!\n",is->slotinfo->slotid);
                return 1;
            }
        }
//        return (is->abort_request || (is->seek_req==1 && is->reading));
        return is->abort_request;
    }
    return (global_video_state && global_video_state->abort_request);
}

static int readpass(VideoState *is, int pass, int passid)
{
    int ret = 0;
#ifdef USE_DEBUG_LIB
        if(dmem && is->slotinfo->slotid<MAX_DEBUG_SLOT) {
            slotdebug_t* slotdebugs = debugmem_getslot(dmem);
            if(slotdebugs) {
                ret=slotdebugs[is->slotinfo->slotid].readpass;
                if(pass!=-1)
                    slotdebugs[is->slotinfo->slotid].readpass=pass;
                if(passid!=-1)
                    slotdebugs[is->slotinfo->slotid].readpassid=passid;
            }
        }
#endif
    return ret;
}

/* this thread gets the stream from the disk or the network */
static void* read_thread(void *arg)
{
    VideoState *is = arg;
    AVFormatContext *ic = NULL;
    int err, i, ret;
    int st_index[AVMEDIA_TYPE_NB];
    AVPacket pkt1, *pkt = &pkt1;
    int eof=0;
    int pkt_in_play_range = 0;
//    AVDictionaryEntry *t;
    AVDictionary **opts;
    int orig_nb_streams;
    int clearpause = 0;
//    int dbgcnt = 0;
//    int slotid = is->slotinfo->slotid;

    int64_t seek_target=0;
    int64_t seek_min=0;
    int64_t seek_max=0;
#ifdef USE_DEBUG_LIB
    double ltime=0.0;
    double stime=0.0;
#endif
    double etime=0.0;
    double wtime=0.0;
    double st, et;
    int pass;

#ifdef THREAD_DEBUG
    av_log(NULL, AV_LOG_DEBUG,"[debug] read_thread(): start read_thread 0x%x \n",(unsigned int)pthread_self());
#endif
    memset(st_index, -1, sizeof(st_index));
    is->video_stream = -1;
    is->audio_stream = -1;
    is->subtitle_stream = -1;

    global_video_state = is;

    ic = avformat_alloc_context();
    ic->interrupt_callback.opaque = is;
    ic->interrupt_callback.callback = decode_interrupt_cb;
    is->slotinfo->status|=STATUS_PLAYER_CONNECT;
    printdebugbuffer(is->slotinfo->slotid, "Open: '%s'", is->filename);
#ifdef THREAD_DEBUG
    av_log(NULL, AV_LOG_DEBUG,"[debug] read_thread(): open stream: %d pass1 '%s' \n",is->slotinfo->slotid,is->filename);
#endif
    err = avformat_open_input(&ic, is->filename, is->iformat, &is->slotinfo->format_opts);
#ifdef THREAD_DEBUG
    av_log(NULL, AV_LOG_DEBUG,"[debug] read_thread(): open stream: %d pass2 '%s' \n",is->slotinfo->slotid,is->filename);
#endif
    if (err < 0) {
        print_error(is->filename, err);
        printdebugbuffer(is->slotinfo->slotid, "Open error: %d. (%s)", err, is->filename);
        ret = -1;
        goto fail;
    }
    ic->probesize=50000;
    is->ic = ic;
    if(is->slotinfo->genpts)
        ic->flags |= AVFMT_FLAG_GENPTS;

    av_dict_set(&is->slotinfo->codec_opts, "request_channels", "2", 0);

#ifdef THREAD_DEBUG
    av_log(NULL, AV_LOG_DEBUG,"[debug] read_thread(): open stream: %d pass3 '%s' \n",is->slotinfo->slotid,is->filename);
#endif
    printdebugbuffer(is->slotinfo->slotid, "Find stream info. (%s)", is->filename);
    opts = setup_find_stream_info_opts(ic, is->slotinfo->codec_opts);
#ifdef THREAD_DEBUG
    av_log(NULL, AV_LOG_DEBUG,"[debug] read_thread(): open stream: %d pass4 '%s' \n",is->slotinfo->slotid,is->filename);
#endif
    orig_nb_streams = ic->nb_streams;

    err = avformat_find_stream_info(ic, opts);
    if (err < 0) {
        av_log(NULL, AV_LOG_ERROR, "[error] read_thread(): slot: %d filename: %s: could not find codec parameters\n", 
                is->slotinfo->slotid,
                is->filename);
        printdebugbuffer(is->slotinfo->slotid, "Info error: %d. (%s)", err, is->filename);
        ret = -1;
        goto fail;
    }
    for (i = 0; i < orig_nb_streams; i++)
        av_dict_free(&opts[i]);
    av_freep(&opts);
    printdebugbuffer(is->slotinfo->slotid, "Info OK. (%s)", is->filename);

    if(ic->pb)
        ic->pb->eof_reached= 0; //FIXME hack, ffplay maybe should not use url_feof() to test for the end

    if(is->slotinfo->seek_by_bytes<0)
        is->slotinfo->seek_by_bytes= !!(ic->iformat->flags & AVFMT_TS_DISCONT);

    /* if seeking requested, we execute it */
    if (is->slotinfo->start_time != AV_NOPTS_VALUE) {
        int64_t timestamp;

        timestamp = is->slotinfo->start_time;
        /* add the stream start time */
        if (ic->start_time != AV_NOPTS_VALUE)
            timestamp += ic->start_time;
        ret = avformat_seek_file(ic, -1, INT64_MIN, timestamp, INT64_MAX, 0);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "[error] read_thread(): slot: %d %s: could not seek to position %0.3f\n",
                    is->slotinfo->slotid,
                    is->filename, (double)timestamp / AV_TIME_BASE);
        }
        av_log(NULL, AV_LOG_DEBUG,"[debug] read_thread(): open stream: %d '%s' start seek: %d %lld %12.6f \n",is->slotinfo->slotid,is->filename,ret,timestamp,(double)timestamp / AV_TIME_BASE);
    }

#ifdef THREAD_DEBUG
    av_log(NULL, AV_LOG_DEBUG,"[debug] read_thread(): open stream: %d pass5 '%s' \n",is->slotinfo->slotid,is->filename);
#endif
    printdebugbuffer(is->slotinfo->slotid, "Find best stream. (%s)", is->filename);
    for (i = 0; i < ic->nb_streams; i++)
        ic->streams[i]->discard = AVDISCARD_ALL;
    if (!is->slotinfo->video_disable)
        st_index[AVMEDIA_TYPE_VIDEO] =
            av_find_best_stream(ic, AVMEDIA_TYPE_VIDEO,
                                is->slotinfo->wanted_stream[AVMEDIA_TYPE_VIDEO], -1, NULL, 0);
    if (!is->slotinfo->audio_disable)
        st_index[AVMEDIA_TYPE_AUDIO] =
            av_find_best_stream(ic, AVMEDIA_TYPE_AUDIO,
                                is->slotinfo->wanted_stream[AVMEDIA_TYPE_AUDIO],
                                st_index[AVMEDIA_TYPE_VIDEO],
                                NULL, 0);
    if (!is->slotinfo->video_disable)
        st_index[AVMEDIA_TYPE_SUBTITLE] =
            av_find_best_stream(ic, AVMEDIA_TYPE_SUBTITLE,
                                is->slotinfo->wanted_stream[AVMEDIA_TYPE_SUBTITLE],
                                (st_index[AVMEDIA_TYPE_AUDIO] >= 0 ?
                                 st_index[AVMEDIA_TYPE_AUDIO] :
                                 st_index[AVMEDIA_TYPE_VIDEO]),
                                NULL, 0);
    if (is->slotinfo->show_status) {
        av_dump_format(ic, 0, is->filename, 0);
    }

#ifdef THREAD_DEBUG
    av_log(NULL, AV_LOG_DEBUG,"[debug] read_thread(): open stream: %d pass6 '%s' audio: %d (dis: %d)\n",is->slotinfo->slotid,is->filename,st_index[AVMEDIA_TYPE_AUDIO],is->slotinfo->audio_disable);
#endif
    is->show_mode = is->slotinfo->show_mode;

    printdebugbuffer(is->slotinfo->slotid, "Open streams. (%s)", is->filename);
    /* open the streams */
    if (st_index[AVMEDIA_TYPE_AUDIO] >= 0) {
        stream_component_open(is, st_index[AVMEDIA_TYPE_AUDIO]);
    }
#ifdef THREAD_DEBUG
    av_log(NULL, AV_LOG_DEBUG,"[debug] read_thread(): open stream: %d pass7 '%s' \n",is->slotinfo->slotid,is->filename);
#endif

    ret=-1;
    if (st_index[AVMEDIA_TYPE_VIDEO] >= 0) {
        ret= stream_component_open(is, st_index[AVMEDIA_TYPE_VIDEO]);
    }
#ifdef THREAD_DEBUG
    av_log(NULL, AV_LOG_DEBUG,"[debug] read_thread(): open stream: %d pass8 '%s' \n",is->slotinfo->slotid,is->filename);
    av_log(NULL, AV_LOG_DEBUG,"[debug] read_thread(): create refresh thread: %d ----\n",is->slotinfo->slotid);
#endif
    pthread_create(&is->refresh_tid, NULL, refresh_thread, is);
    is->refresh_tidinited=1;
#ifdef THREAD_DEBUG
    av_log(NULL, AV_LOG_DEBUG,"[debug] read_thread(): pthread_create: #4736 slot: %d 0x%x \n",is->slotinfo->slotid,(unsigned int)is->refresh_tid);
    av_log(NULL, AV_LOG_DEBUG,"[debug] read_thread(): create refresh thread: %d OK ----\n",is->slotinfo->slotid);
#endif
    if (is->show_mode == SHOW_MODE_NONE)
        is->show_mode = ret >= 0 ? SHOW_MODE_VIDEO : SHOW_MODE_RDFT;

#ifdef THREAD_DEBUG
    av_log(NULL, AV_LOG_DEBUG,"[debug] read_thread(): open stream: %d pass9 '%s' \n",is->slotinfo->slotid,is->filename);
#endif
    if (st_index[AVMEDIA_TYPE_SUBTITLE] >= 0) {
        stream_component_open(is, st_index[AVMEDIA_TYPE_SUBTITLE]);
    }

    if (is->video_stream < 0 && is->audio_stream < 0) {
        av_log(NULL, AV_LOG_ERROR, "[error] read_thread(): %s: could not open codecs\n", is->filename);
        ret = -1;
        printdebugbuffer(is->slotinfo->slotid, "Open streams error. (%s)", is->filename);
        goto fail;
    }

    if(!is->slotinfo->pauseafterload) {
        is->slotinfo->status|=(STATUS_PLAYER_OPENED);
    }
#ifdef THREAD_DEBUG
    av_log(NULL, AV_LOG_DEBUG,"[debug] read_thread(): open stream: %d pass10 '%s' \n",is->slotinfo->slotid,is->filename);
#endif
    if(is->ic->duration==AV_NOPTS_VALUE) {
        printdebugbuffer(is->slotinfo->slotid, "Open streams OK. (%s) Duration: realtime", is->filename);
    } else {
        double d = is->ic->duration/1000000LL;
        printdebugbuffer(is->slotinfo->slotid, "Open streams OK. (%s) Duration: %8.3f", is->filename,d);
    }

    is->slotinfo->callplayerstatus = 3;

    for(;;) {
        if(!is->slotinfo->pauseafterload && !(is->slotinfo->status &STATUS_PLAYER_OPENED)) {
            is->slotinfo->status|=(STATUS_PLAYER_OPENED);
        }
#ifdef USE_DEBUG_LIB
        ltime=etime;
#endif
        etime=xplayer_clock();
#ifdef USE_DEBUG_LIB
        if(etime>0.0 && stime>0.0) {
            threadtime(is->slotinfo->slotid, READ_THREAD_ID, etime-stime-wtime, etime-ltime);
        }
        wtime=0.0;
        stime=xplayer_clock();
#endif
        readpass(is, -1, 1);

#ifdef USE_DEBUG_LIB
        if(dmem && is->slotinfo->slotid<MAX_DEBUG_SLOT) {
            slotdebug_t* slotdebugs = debugmem_getslot(dmem);
            if(slotdebugs) {
                slotdebugs[is->slotinfo->slotid].read_proc++;
                slotdebugs[is->slotinfo->slotid].timeshift=is->slotinfo->grouptime;
                slotdebugs[is->slotinfo->slotid].pausereq=is->slotinfo->pausereq|(is->slotinfo->pauseseekreq<<1)|(is->groupsync<<2);
            }
        }
#endif
        if (is->abort_request)
            break;

        readpass(is, -1, 2);
        if(is->groupsync && (is->ic->duration==AV_NOPTS_VALUE || strcmp(is->ic->iformat->name, "rtsp"))) {
            is->slotinfo->pauseseekreq=0;
            is->groupsync=0;
            if(is->paused) {
                toggle_pause(is);
            }
            is->groupsync=0;
        } else if(is->groupsync && !globalpause(is->slotinfo) && !(is->slotinfo->status&(STATUS_PLAYER_SEEK))) {
            is->slotinfo->pauseseekreq=0;
            VideoState* mis = get_group_master(is,1);
            double mtime = get_master_clock(mis);
            double stime = get_master_clock(is);
            if(buffered_audio_data(is)>=GLOBAL_PAUSE_LIMIT) {
                if(mis==is) {
                    if((is->slotinfo->debugflag & DEBUGFLAG_GROUP)) {
                        av_log(NULL,DEBUG_FLAG_LOG_LEVEL,"Slot: %d mis==is paused: %d master: %8.3f slave: %8.3f\n",is->slotinfo->slotid,is->paused,mtime,stime);
                    }
                    if(is->paused) {
                        if((is->slotinfo->debugflag & DEBUGFLAG_GROUP)) {
                            av_log(NULL,DEBUG_FLAG_LOG_LEVEL,"Slot: %d read proc2 toggle: %d\n",is->slotinfo->slotid,is->paused);
                        }
                        toggle_pause(is);
                    }
                    is->groupsync=0;
                } else if(!(mis->slotinfo->status&(STATUS_PLAYER_SEEK))) {
                    if((mtime+mis->slotinfo->grouptime+0.002>=stime+is->slotinfo->grouptime) && is->paused) {
                        if((is->slotinfo->debugflag & DEBUGFLAG_GROUP)) {
                            av_log(NULL,DEBUG_FLAG_LOG_LEVEL,"Slot: %d mis!=is clear pause master: %8.3f slave: %8.3f\n",is->slotinfo->slotid,mtime,stime);
                        }
                        toggle_pause(is);
                        is->groupsync=0;
                    }
                    if((mtime+mis->slotinfo->grouptime+0.04<stime+is->slotinfo->grouptime) && !is->paused) {
                        if((is->slotinfo->debugflag & DEBUGFLAG_GROUP)) {
                            av_log(NULL,DEBUG_FLAG_LOG_LEVEL,"Slot: %d mis!=is set pause master: %8.3f slave: %8.3f\n",is->slotinfo->slotid,mtime,stime);
                        }
                        toggle_pause(is);
                    }
                }
            } else if(is->paused) {
                toggle_pause(is);
            }
#if 0
            if(is->groupsync) {
                fprintf(stderr,"Slot: %d pause: %d global pause: %d pts: %8.3f drift: %8.3f master: %8.3f => %8.3f ? %8.3f [%8.3f] <= %8.3f alen: %8.3f\n",
                    is->slotinfo->slotid,
                    is->paused,
                    globalpause(is->slotinfo),
                    is->audio_current_pts,
                    is->audio_current_pts_drift + av_gettime() / 1000000.0,
                    mtime,
                    mtime+mis->slotinfo->grouptime,
                    stime+is->slotinfo->grouptime,
                    (stime+is->slotinfo->grouptime)-(mtime+mis->slotinfo->grouptime),
                    stime,
                    buffered_audio_data(is)
                    );
            }
#endif
        }
        readpass(is, -1, 3);

        pthread_mutex_lock(&is->event_mutex);
        if (is->last_paused && (is->slotinfo->pausereq||is->slotinfo->pauseseekreq)) {
            is->last_paused = 0;
            is->read_pause_return= av_read_pause(ic);
        }
        if(ispaused(is) && is->readeof && !(is->slotinfo->status&STATUS_PLAYER_PAUSE)) {
            is->slotinfo->status|=STATUS_PLAYER_PAUSE;
        }
        if ((ispaused(is) != is->last_paused) && (!globalpause(is->slotinfo) || is->last_paused) && !(is->slotinfo->pausereq||is->slotinfo->pauseseekreq) && !is->seek_req && !is->readeof) {
            is->last_paused = ispaused(is);
            pthread_mutex_unlock(&is->event_mutex);
            if (is->last_paused) {
                if((is->slotinfo->debugflag & DEBUGFLAG_GROUP)) {
                    av_log(NULL,DEBUG_FLAG_LOG_LEVEL,"Slot: %d av set pause\n",is->slotinfo->slotid);
                }
//                if(!is->readeof)
                {
                    is->read_pause_return= av_read_pause(ic);
                }
                is->slotinfo->status|=STATUS_PLAYER_PAUSE;
            } else {
                if((is->slotinfo->debugflag & DEBUGFLAG_GROUP)) {
                    av_log(NULL,DEBUG_FLAG_LOG_LEVEL,"Slot: %d av clear pause\n",is->slotinfo->slotid);
                }
                av_read_play(ic);
                is->slotinfo->status&=~(STATUS_PLAYER_PAUSE);
            }
            if(!is->last_paused) {
                clearpause = 5;
            }
        } else {
            pthread_mutex_unlock(&is->event_mutex);
        }
        readpass(is, -1, 4);
#if CONFIG_RTSP_DEMUXER || CONFIG_MMSH_PROTOCOL
        if ((is->last_paused) && (!is->seek_req) &&
                (!strcmp(ic->iformat->name, "rtsp") ||
                 (ic->pb && !strncmp(is->slotinfo->url, "mmsh:", 5)))) {
            /* wait 10 ms to avoid trying to get another packet */
            /* XXX: horrible */
//av_log(NULL, AV_LOG_DEBUG,"sleep 1 --------------\n");
//            av_read_pause(ic);
            st=xplayer_clock();
            usleep(10000);
            et=xplayer_clock();
            wtime+=et-st;
//av_log(NULL, AV_LOG_DEBUG,"*********** read stream: %d pass2a '%s' %d \n",is->slotinfo->slotid,is->filename, dbgcnt++);
            readpass(is, 1, -1);
            continue;
        }
#endif
        pthread_mutex_lock(&is->event_mutex);
        readpass(is, -1, 5);
        if (is->seek_req==1) {
            is->readeof=0;
            is->audio_seek_flag=1;
            seek_target= is->seek_pos;
            seek_min= is->seek_rel > 0 ? seek_target - is->seek_rel + 2: INT64_MIN;
            seek_max= is->seek_rel < 0 ? seek_target - is->seek_rel - 2: INT64_MAX;
//FIXME the +-2 is due to rounding being not done in the correct direction in generation
//      of the seek_pos/seek_rel variables

            int read_enable=is->read_enable;
#warning FIXME!! pause_seek
            if(!is->slotinfo->audio_disable && !is->realtime)
                is->read_enable=-1;
            if(!is->realtime)
                is->slotinfo->pausereq=1;
            //is->valid_delay=INVALID_FRAME;
            is->seek_pts = is->seek_pos / AV_TIME_BASE;
            pthread_mutex_unlock(&is->event_mutex);
            if((is->slotinfo->debugflag & DEBUGFLAG_DELAY)) {
                av_log(NULL, DEBUG_FLAG_LOG_LEVEL, "[debug] read_thread(): clear valid\n");
            }

            readpass(is, -1, 6);
            st=xplayer_clock();
            if(is->last_paused || is->readeof || 1) {
                av_read_play(ic);
                is->last_paused=0;
            }
//fprintf(stderr,"Slot: %d av seek %lld paused: %d last_paused: %d .... \n",is->slotinfo->slotid,is->seek_pos,is->paused,is->last_paused);
            ret = avformat_seek_file(is->ic, -1, seek_min, seek_target, seek_max, is->seek_flags);
//fprintf(stderr,"Slot: %d av seek %lld ret: %d paused: %d last_paused: %d\n",is->slotinfo->slotid,is->seek_pos,ret,is->paused,is->last_paused);
            et=xplayer_clock();
            wtime+=et-st;
            if((is->slotinfo->debugflag & DEBUGFLAG_GROUP)) {
                av_log(NULL,DEBUG_FLAG_LOG_LEVEL,"Slot: %d av seek %lld ret: %d paused: %d last_paused: %d\n",is->slotinfo->slotid,is->seek_pos,ret,is->paused,is->last_paused);
            }
//fprintf(stderr,"Slot: %d av seek %lld ret: %d paused: %d last_paused: %d\n",is->slotinfo->slotid,is->seek_pos,ret,is->paused,is->last_paused);
            readpass(is, -1, 7);
            if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR, "[debug] read_thread(): %s: error while seeking\n", is->ic->filename);
                is->read_enable=read_enable;
            }else{
                if (is->audio_stream >= 0) {
                    packet_queue_flush(is, &is->audioq);
                    packet_queue_put(is, &is->audioq, &is->flush_pkt);
                    audio_decode_flush(is);
                }
                if (is->subtitle_stream >= 0) {
                    packet_queue_flush(is, &is->subtitleq);
                    packet_queue_put(is, &is->subtitleq, &is->flush_pkt);
                }
                if (is->video_stream >= 0) {
                    packet_queue_flush(is, &is->videoq);
                    packet_queue_put(is, &is->videoq, &is->flush_pkt);
                }
                if(is->read_enable==-1) {
                    av_log(NULL, AV_LOG_DEBUG,"[debug] read_thread(): ----------------------------- read disable ----------------- \n");
                    is->read_enable=0;
                }
            }
            if(!is->realtime)
                is->slotinfo->pausereq=1;
            pthread_mutex_lock(&is->event_mutex);
            is->seek_req=0;
            is->slotinfo->pauseseekreq=0;
            pthread_mutex_unlock(&is->event_mutex);
            eof= 0;
            readpass(is, -1, 8);
            if(is->last_paused && !is->seek_req && !is->groupsync) {
                readpass(is, 2, -1);
                continue;
            }
            readpass(is, 0, -1);
        } else {
            pthread_mutex_unlock(&is->event_mutex);
        }
        int aqsize = is->audio_decode_buffer_len;
        /* if the queue are full, no need to read more */
        readpass(is, -1, 9);
        if ((   aqsize + is->videoq.size + is->subtitleq.size > MAX_QUEUE_SIZE
            || (   (            aqsize  > MIN_AUDIOQ_SIZE || is->audio_stream<0)
                && (is->videoq   .nb_packets > MIN_FRAMES || is->video_stream<0)
                && (is->subtitleq.nb_packets > MIN_FRAMES || is->subtitle_stream<0))) && !is->slotinfo->pausereq) {
#if 0
fprintf(stderr,"Slot: %d aqsize+videoq+subq = %d > MAX_Q %d [%d] || (aqsize %d > MINA %d [%d] && vnb %d > MINF %d [%d] && snb %d > MINF %d [%d]) \n",
    is->slotinfo->slotid,
    aqsize + is->videoq.size + is->subtitleq.size, MAX_QUEUE_SIZE,(aqsize + is->videoq.size + is->subtitleq.size > MAX_QUEUE_SIZE),
    aqsize,MIN_AUDIOQ_SIZE,(aqsize  > MIN_AUDIOQ_SIZE || is->audio_stream<0),
    is->videoq.nb_packets,MIN_FRAMES,(is->videoq   .nb_packets > MIN_FRAMES || is->video_stream<0),
    is->subtitleq.nb_packets,MIN_FRAMES,(is->subtitleq.nb_packets > MIN_FRAMES || is->subtitle_stream<0)
    );
#endif
            /* wait 10 ms */
            st=xplayer_clock();
            usleep(10000);
            et=xplayer_clock();
            wtime+=et-st;
            readpass(is, 3, -1);
            continue;
        }
        readpass(is, -1, 10);
        if(eof) {
            if(is->video_stream >= 0){
                av_init_packet(pkt);
                pkt->data=NULL;
                pkt->size=0;
                pkt->stream_index= is->video_stream;
                packet_queue_put(is, &is->videoq, pkt);
            }
            if (is->audio_stream >= 0 &&
                is->audio_st->codec->codec->capabilities & CODEC_CAP_DELAY) {
                av_init_packet(pkt);
                pkt->data = NULL;
                pkt->size = 0;
                pkt->stream_index = is->audio_stream;
            }
            st=xplayer_clock();
            usleep(10000);
            et=xplayer_clock();
            wtime+=et-st;
            if(aqsize + is->videoq.size + is->subtitleq.size ==0){
                if(is->slotinfo->loop!=1 && (!is->slotinfo->loop || --is->slotinfo->loop)){
                    is->audio_clock = 0.0;
                    is->audio_clock_diff_len=0;
                    is->audio_diff=0;

                }else if(is->slotinfo->autoexit){
                    ret=AVERROR_EOF;
                    goto fail;
                }
            }
            //stream_seek(is, is->slotinfo->start_time != AV_NOPTS_VALUE ? is->slotinfo->start_time : 0, 0, 0);
            //is->paused = 1;
            //is->video_current_pts = 0;
            eof=0;
            readpass(is, 4, -1);
            continue;
        }
        readpass(is, -1, 11);
        if(pkt) {
            pkt->pts=AV_NOPTS_VALUE;
        }
        st=xplayer_clock();
        pass=readpass(is, 9, -1);
//fprintf(stderr,"Slot: %d read ... seek_req: %d\n",is->slotinfo->slotid,is->seek_req);
        is->reading=is->slotinfo->buffering=1;
        is->readingtime=xplayer_clock();
        ret = av_read_frame(ic, pkt);
        is->reading=is->slotinfo->buffering=0;
        readpass(is, pass, -1);
        et=xplayer_clock();
        wtime+=et-st;
//fprintf(stderr,"Slot: %d read: %d pts: %lld\n",is->slotinfo->slotid,ret,pkt->pts);
        double pktpts = -1.0;
        if(pkt->pts!=AV_NOPTS_VALUE)
        {
            pktpts = av_q2d(ic->streams[pkt->stream_index]->time_base) * pkt->pts;
            if(is->slotinfo->length>0.0 && pktpts>=is->slotinfo->length) {
                is->readeof=1;
            } else {
                is->readeof=0;
            }
        }
//fprintf(stderr,"Slot: %d read ret: %d pts: %8.3f len: %8.3f seek_req: %d\n",is->slotinfo->slotid,ret,pktpts,is->slotinfo->length,is->seek_req);

        readpass(is, -1, 12);
        if(is->seek_req>1) {
            double dseek = (double)seek_target/AV_TIME_BASE;
//            fprintf(stderr,"Slot: %d After seek: %lld -> %lld = %lld %lld\n",is->slotinfo->slotid,seek_target,pkt->pts,seek_target-pkt->pts,pkt->pts-seek_target);
            if((is->slotinfo->debugflag & DEBUGFLAG_GROUP)) {
                av_log(NULL,DEBUG_FLAG_LOG_LEVEL,"Slot: %d After seek: %8.3f -> %8.3f = %8.3f (%12.6f)\n",is->slotinfo->slotid,dseek,pktpts,pktpts-dseek,av_q2d(ic->streams[pkt->stream_index]->time_base));
            }
            is->seek_req++;
            if(pktpts-dseek>-SEEK_PRECISION && pktpts-dseek<SEEK_PRECISION) {
                if(is->slotinfo->groupid)
                    is->groupsync=1;
                is->seek_req=0;
                if((is->slotinfo->debugflag & DEBUGFLAG_GROUP)) {
                    av_log(NULL,DEBUG_FLAG_LOG_LEVEL,"Slot: %d Aftrer seek OK\n",is->slotinfo->slotid);
                }
                is->slotinfo->pauseseekreq=0;
            }
            if(is->seek_req==25) {
                is->seek_req=1;
            }
        }

        if(!is->groupsync && !globalpause(is->slotinfo) && !(is->slotinfo->status&(STATUS_PLAYER_SEEK))) {
            is->slotinfo->status&=~(STATUS_PLAYER_PAUSE);
        }
        readpass(is, -1, 13);

#ifdef USE_DEBUG_LIB
        if(dmem && is->slotinfo->slotid<MAX_DEBUG_SLOT && pkt) {
            slotdebug_t* slotdebugs = debugmem_getslot(dmem);
            if(slotdebugs) {
                if(pkt->stream_index == is->video_stream)
                    slotdebugs[is->slotinfo->slotid].vreadpts=pktpts;
                if(pkt->stream_index == is->audio_stream)
                    slotdebugs[is->slotinfo->slotid].areadpts=pktpts;
                slotdebugs[is->slotinfo->slotid].readpts=pktpts;
                slotdebugs[is->slotinfo->slotid].readno++;
            }
        }
#endif
        readpass(is, -1, 14);
        if((is->slotinfo->debugflag & DEBUGFLAG_READ)) {
            av_log(NULL, DEBUG_FLAG_LOG_LEVEL, "[debug] read_thread(): %d av_read_frame: %d pts: %7.3f index: %d (A: %d V: %d S: %d)\n",is->slotinfo->slotid,ret,pktpts,pkt->stream_index,is->audio_stream,is->video_stream,is->subtitle_stream);
        }
        if (ret < 0) {
            if (ret == AVERROR_EOF || url_feof(ic->pb))
                eof=1;
//fprintf(stderr,"Slot: %d EOF: %d !!! <----------------------------------------\n",is->slotinfo->slotid,eof);
            if (ic->pb && ic->pb->error)
                break;
            if((clearpause || is->seek_req) && eof) {
                av_log(NULL, AV_LOG_DEBUG,"[debug] read_thread(): read stream eof?: %d ret: %d %x AVEOF: %d feof: %d\n",is->slotinfo->slotid,ret,-ret,AVERROR_EOF,url_feof(ic->pb));
                is->slotinfo->reopen=1;
                break;
            }
            st=xplayer_clock();
            usleep(100000);/* wait for user event */
            et=xplayer_clock();
            wtime+=et-st;
            readpass(is, 5, -1);
            continue;
        } else {
            if(clearpause) {
                clearpause--;
            }
        }
        if(is->seek_req) {
            if((is->slotinfo->debugflag & DEBUGFLAG_GROUP)) {
                av_log(NULL,DEBUG_FLAG_LOG_LEVEL,"Slot: %d Drop packet after seek.\n",is->slotinfo->slotid);
            }
            av_free_packet(pkt);
            continue;
        }
        readpass(is, -1, 15);
        /* check if packet is in play range specified by user, then queue, otherwise discard */
        pkt_in_play_range = is->slotinfo->duration == AV_NOPTS_VALUE ||
                (pkt->pts - ic->streams[pkt->stream_index]->start_time) *
                av_q2d(ic->streams[pkt->stream_index]->time_base) -
                (double)(is->slotinfo->start_time != AV_NOPTS_VALUE ? is->slotinfo->start_time : 0)/1000000
                <= ((double)is->slotinfo->duration/1000000);
        if (pkt->stream_index == is->audio_stream && pkt_in_play_range) {
//fprintf(stderr,"Slot: %d pts: %lld %8.3f \n",is->slotinfo->slotid,pkt->pts,av_q2d(ic->streams[pkt->stream_index]->time_base) * pkt->pts);
            audio_decode_frame(is, pkt);
        } else if (pkt->stream_index == is->video_stream && pkt_in_play_range) {
            packet_queue_put(is, &is->videoq, pkt);
        } else if (pkt->stream_index == is->subtitle_stream && pkt_in_play_range) {
            packet_queue_put(is, &is->subtitleq, pkt);
        } else {
            av_log(NULL, AV_LOG_DEBUG,"[debug] read_thread(): drop readed packet %d (A: %d V: %d S: %d)\n",pkt->stream_index,is->audio_stream,is->video_stream,is->subtitle_stream);
            av_free_packet(pkt);
        }
        readpass(is, 6, -1);
    }
#ifdef THREAD_DEBUG
    av_log(NULL, AV_LOG_DEBUG,"[debug] read_thread(): read stream: %d end1 '%s' \n",is->slotinfo->slotid,is->filename);
#endif
//fprintf(stderr,"Slot: %d wait end ... <----------------------------------------\n",is->slotinfo->slotid);
    /* wait until the end */
    while (!is->abort_request && !is->slotinfo->reopen) {
        st=xplayer_clock();
        usleep(100000);
        et=xplayer_clock();
        wtime+=et-st;
    }
#ifdef THREAD_DEBUG
    av_log(NULL, AV_LOG_DEBUG,"[debug] read_thread(): read stream: %d end2 '%s' \n",is->slotinfo->slotid,is->filename);
#endif

    ret = 0;
 fail:
//fprintf(stderr,"Slot: %d wait end !!! <----------------------------------------\n",is->slotinfo->slotid);
    is->slotinfo->status&=~(STATUS_PLAYER_CONNECT|STATUS_PLAYER_OPENED|STATUS_PLAYER_PAUSE);
    /* disable interrupting */
    global_video_state = NULL;

    is->slotinfo->callplayerstatus = 2;

    /* close each stream */
    if (is->audio_stream >= 0)
        stream_component_close(is, is->audio_stream);
    if (is->video_stream >= 0)
        stream_component_close(is, is->video_stream);
    if (is->subtitle_stream >= 0)
        stream_component_close(is, is->subtitle_stream);
    if (is->ic) {
//        av_close_input_file(is->ic);
        avformat_close_input(&is->ic);
        is->ic = NULL; /* safety */
    }
//    avio_set_interrupt_cb(NULL);

    if (ret != 0 || is->slotinfo->reopen) {
        event_t event;
        event.type = FF_STOP_EVENT;
        event.data = is;
        push_event(is->slotinfo->eventqueue, &event);
    }
#ifdef THREAD_DEBUG
    av_log(NULL, AV_LOG_DEBUG,"[debug] read_thread(): end read_thread 0x%x \n",(unsigned int)pthread_self());
#endif
    return NULL;
}

static VideoState *stream_open(slotinfo_t* slotinfo, const char *filename, AVInputFormat *iformat)
{
    VideoState *is;

    is = av_mallocz(sizeof(VideoState));
    if (!is)
        return NULL;

#ifdef USE_DEBUG_LIB
    if(dmem && slotinfo->slotid<MAX_DEBUG_SLOT) {
        slotdebug_t* slotdebugs = debugmem_getslot(dmem);
        if(slotdebugs) {
            slotdebugs[slotinfo->slotid].silence=0;
        }
    }
#endif
    is->run=1;
    is->slotinfo = slotinfo;
    if(slotinfo->reopen) {
        av_log(NULL, AV_LOG_DEBUG,"[debug] stream_open(): reopen: %d end2 '%s' pos: %12.6f \n",is->slotinfo->slotid,is->filename,slotinfo->currentpos);
        slotinfo->start_time=slotinfo->currentpos * AV_TIME_BASE;
    }
    is->slotinfo->reopen=0;
    is->slotinfo->streampriv=is;
    is->slotinfo->eventqueue=init_eventqueue();
    is->slotinfo->loop=1;
    is->slotinfo->audio_callback_time=0;
    is->slotinfo->usesvda=0;
    is->valid_delay=INVALID_DELAY;
    is->audio_hw_buf_size=is->slotinfo->audio_hw_buf_size;
    av_init_packet(&is->flush_pkt);
    is->flush_pkt.data= (unsigned char*)"FLUSH";
    av_strlcpy(is->filename, filename, sizeof(is->filename));
    is->slotinfo->seek_by_bytes=-1;
    is->sws_flags = SWS_BICUBIC;
    is->slotinfo->status&=~(STATUS_PLAYER_IMAGE|STATUS_PLAYER_OPENED|STATUS_PLAYER_CONNECT);
    is->read_enable=0;
    if(!is->slotinfo->audio_disable)
        is->read_enable=1;

    is->buffer_time = is->slotinfo->buffer_time;

    is->iformat = iformat;
    is->ytop = 0;
    is->xleft = 0;

//    is->sws_opts = sws_getContext(16, 16, 0, 16, 16, 0, SWS_BICUBIC, NULL, NULL, NULL);
    pthread_mutex_init(&is->audio_decode_buffer_mutex, NULL);

    pthread_mutex_init(&is->event_mutex, NULL);
    /* start video display */
    pthread_mutex_init(&is->pictq_mutex, NULL);
    pthread_cond_init(&is->pictq_cond, NULL);

    pthread_mutex_init(&is->subpq_mutex, NULL);
    pthread_cond_init(&is->subpq_cond, NULL);

    is->av_sync_type = is->slotinfo->av_sync_type;
    is->slotinfo->status|=(STATUS_PLAYER_INITED);
#ifdef THREAD_DEBUG
    av_log(NULL, AV_LOG_DEBUG,"[debug] stream_open(): create read thread: %d ----\n",slotinfo->slotid);
#endif
    pthread_create(&is->read_tid, NULL, read_thread, is);
    is->read_tidinited=1;
#ifdef THREAD_DEBUG
    av_log(NULL, AV_LOG_DEBUG,"[debug] stream_open(): pthread_create: #4998 slot: %d 0x%x \n",is->slotinfo->slotid,(unsigned int)is->read_tid);
    av_log(NULL, AV_LOG_DEBUG,"[debug] stream_open(): create read thread: %d OK ----\n",slotinfo->slotid);
#endif
    if (!is->read_tidinited) {
#ifdef USE_WAV_DUMP
        if(is->fhwav1) {
            update_wav_header(is->fhwav1,is->wav1size);
            fclose(is->fhwav1);
        }
        if(is->fhwav2) {
            update_wav_header(is->fhwav2,is->wav2size);
            fclose(is->fhwav2);
        }
#endif
        pthread_mutex_destroy(&is->audio_decode_buffer_mutex);
        av_free(is);
        return NULL;
    }
    return is;
}

#if 0
static void stream_cycle_channel(VideoState *is, int codec_type)
{
    AVFormatContext *ic = is->ic;
    int start_index, stream_index;
    AVStream *st;

    if (codec_type == AVMEDIA_TYPE_VIDEO)
        start_index = is->video_stream;
    else if (codec_type == AVMEDIA_TYPE_AUDIO)
        start_index = is->audio_stream;
    else
        start_index = is->subtitle_stream;
    if (start_index < (codec_type == AVMEDIA_TYPE_SUBTITLE ? -1 : 0))
        return;
    stream_index = start_index;
    for(;;) {
        if (++stream_index >= is->ic->nb_streams)
        {
            if (codec_type == AVMEDIA_TYPE_SUBTITLE)
            {
                stream_index = -1;
                goto the_end;
            } else
                stream_index = 0;
        }
        if (stream_index == start_index)
            return;
        st = ic->streams[stream_index];
        if (st->codec->codec_type == codec_type) {
            /* check that parameters are OK */
            switch(codec_type) {
            case AVMEDIA_TYPE_AUDIO:
                if (st->codec->sample_rate != 0 &&
                    st->codec->channels != 0)
                    goto the_end;
                break;
            case AVMEDIA_TYPE_VIDEO:
            case AVMEDIA_TYPE_SUBTITLE:
                goto the_end;
            default:
                break;
            }
        }
    }
 the_end:
    stream_component_close(is, start_index);
    stream_component_open(is, stream_index);
}
#endif

static void toggle_pause(VideoState *is)
{
    stream_toggle_pause(is);
    is->step = 0;
}

static void step_to_next_frame(VideoState *is, int step)
{
    /* if the stream is paused unpause it, then step */
    if (is->paused)
        stream_toggle_pause(is);
    is->step = step;
}

static void event_loop(slotinfo_t* slotinfo)
{
    event_t* event;
    VideoState* is = (VideoState*)slotinfo->streampriv;
    int read_enable;
    double pos;
#ifdef USE_DEBUG_LIB
    double stime = 0.0;
    double etime = 0.0;
    double ltime = 0.0;
#endif

    while(slotinfo->streampriv) {
#ifdef USE_DEBUG_LIB
        ltime=etime;
        etime=xplayer_clock();
        if(etime>0.0 && stime>0.0) {
            threadtime(is->slotinfo->slotid, PROCESS_THREAD_ID, etime-stime, etime-ltime);
        }
#endif
        event = wait_event(is->slotinfo->eventqueue);
#ifdef USE_DEBUG_LIB
        stime=xplayer_clock();
#endif
        if(!event)
            break;
        switch(event->type) {
        case FF_QUIT_EVENT:
            av_log(NULL, AV_LOG_DEBUG,"[debug] event_loop(): Slot: %d Pos: %7.2f Event: quit\n",is->slotinfo->slotid,get_master_clock(is));
            av_free(event);
            return;
            break;
        case FF_STOP_EVENT:
            av_log(NULL, AV_LOG_DEBUG,"[debug] event_loop(): Slot: %d Pos: %7.2f  Event: stop\n",is->slotinfo->slotid,get_master_clock(is));
            do_exit(is);
            break;
        case FF_ALLOC_EVENT:
            video_open(event->data);
            alloc_picture(event->data);
            break;
        case FF_REFRESH_EVENT:
            video_refresh(event->data);
            is->refresh=0;
            break;
        case TOGGLE_PAUSE_EVENT:
            av_log(NULL, AV_LOG_DEBUG,"[debug] event_loop(): Slot: %d Pos: %7.2f  Event: toggle pause (pause: %d)\n",is->slotinfo->slotid,get_master_clock(is),is->paused);
            toggle_pause(is);
            is->pause_seek=0;
            is->slotinfo->newimageflag&=2;
            break;
        case PAUSE_SET_EVENT:
            av_log(NULL, AV_LOG_DEBUG,"[debug] event_loop(): Slot: %d Pos: %7.2f  Event: set pause (pause: %d)\n",is->slotinfo->slotid,get_master_clock(is),is->paused);
            is->pause_seek=0;
            is->slotinfo->newimageflag&=2;
            if(!is->paused) {
                toggle_pause(is);
            }
            break;
        case PAUSE_CLEAR_EVENT:
            av_log(NULL, AV_LOG_DEBUG,"[debug] event_loop(): Slot: %d Pos: %7.2f  Event: clear pause (pause: %d)\n",is->slotinfo->slotid,get_master_clock(is),is->paused);
            is->pause_seek=0;
            is->slotinfo->newimageflag&=2;
            if(is->paused) {
                toggle_pause(is);
            }
            break;
        case GROUP_PAUSE_CLEAR_EVENT:
            av_log(NULL, AV_LOG_DEBUG,"[debug] event_loop(): Slot: %d Pos: %7.2f  Event: clear pause (pause: %d)\n",is->slotinfo->slotid,get_master_clock(is),is->paused);
            is->pause_seek=0;
            is->slotinfo->newimageflag&=2;
            if(group_eof(is)) {
                group_stream_seek(is, 0.0);
            }
            if(is->slotinfo->groupid) {
                group_toggle_pause(is,PAUSE_CLEAR_EVENT);
            }
            break;
        case GROUP_PAUSE_SET_EVENT:
            av_log(NULL, AV_LOG_DEBUG,"[debug] event_loop(): Slot: %d Pos: %7.2f  Event: clear pause (pause: %d)\n",is->slotinfo->slotid,get_master_clock(is),is->paused);
            is->pause_seek=0;
            is->slotinfo->newimageflag&=2;
            if(is->slotinfo->groupid) {
                group_toggle_pause(is,PAUSE_SET_EVENT);
            }
            break;
        case GROUP_SEEK_EVENT:
            is->slotinfo->status|=(STATUS_PLAYER_SEEK);
            pos = event->vdouble;
            if(is->slotinfo->groupid) {
                group_stream_seek(is, pos);
            } else {
                is->audio_clock = 0.0;
                is->audio_clock_diff_len = 0;
                is->audio_diff=0;
#ifndef USES_PAUSESEEK
                if (is->paused) {
                    is->pause_seek = 1;
                    is->pause_seek_pos = pos;
                    is->pause_seek_curr = -1.0;
                    toggle_pause(is);
                }
#endif
            stream_seek(is, (int64_t)(pos * AV_TIME_BASE), 0, 0);
            }
            break;
        case PAUSE_STEP_EVENT:
            av_log(NULL, AV_LOG_DEBUG,"[debug] event_loop(): Slot: %d Pos: %7.2f  Event: step (pause: %d)\n",is->slotinfo->slotid,get_master_clock(is),is->paused);
            int s_step = 2;
            if (isstep) {
                s_step = isstep;
                isstep = 0;
            }
            step_to_next_frame(is, s_step);
            break;
        case QUEUE_FLUSH_EVENT:
            av_log(NULL, AV_LOG_DEBUG,"[debug] event_loop(): Slot: %d Pos: %7.2f  Event: flush (pause: %d)\n",is->slotinfo->slotid,get_master_clock(is),is->paused);
            read_enable = is->read_enable;
            is->read_enable=-1;
            if (is->audio_stream >= 0) {
                packet_queue_flush(is, &is->audioq);
                packet_queue_put(is, &is->audioq, &is->flush_pkt);
                audio_decode_flush(is);
            }
            if (is->subtitle_stream >= 0) {
                packet_queue_flush(is, &is->subtitleq);
                packet_queue_put(is, &is->subtitleq, &is->flush_pkt);
            }
            if (is->video_stream >= 0) {
                packet_queue_flush(is, &is->videoq);
                packet_queue_put(is, &is->videoq, &is->flush_pkt);
            }
            is->audio_clock_diff_len=0;
            is->audio_diff=0;
            is->read_enable=read_enable;
            break;
        case STEP_FRAME:
            is->slotinfo->status|=(STATUS_PLAYER_SEEK);
            pos = event->vdouble / is->slotinfo->fps - laserbeams;
            laserbeams = 0;

            if (pos < 0 || event->vdouble < 100) pos = 0;

            pos *= AV_TIME_BASE;
            is->audio_clock = 0.0;
            is->audio_clock_diff_len = 0;
            is->audio_diff = 0;

            stream_seek(is, (int64_t)(pos), 0, 0);

            is->slotinfo->seekpos = pos;
            is->slotinfo->seekflag = 1;

            triggermeh = event->vdouble;

            break;
        case SEEK_EVENT:
            is->slotinfo->status|=(STATUS_PLAYER_SEEK);
            pos = event->vdouble;
            av_log(NULL, AV_LOG_DEBUG,"[debug] event_loop(): Slot: %d Pos: %7.2f  Event: seek: %12.6f (pause: %d)\n",is->slotinfo->slotid,get_master_clock(is),pos,is->paused);
//            pos = get_master_clock(is);
            av_log(NULL, AV_LOG_DEBUG,"[debug] event_loop(): seek: %12.6f %lld\n",pos,(int64_t)(pos * AV_TIME_BASE));

            if (pos == 0) {
                is->step = triggermeh = dont = 0;
            }

            is->audio_clock = 0.0;
            is->audio_clock_diff_len = 0;
            is->audio_diff=0;
#ifndef USES_PAUSESEEK
            if (0 && is->paused) {
                is->pause_seek = 1;
                is->pause_seek_pos = pos;
                is->pause_seek_curr = -1.0;
                toggle_pause(is);
            }
#endif
            pos *= AV_TIME_BASE;
            stream_seek(is, (int64_t)(pos), 0, 0);
            break;
        case SEEK_REL_EVENT:
            is->slotinfo->status|=(STATUS_PLAYER_SEEK);
            pos = is->slotinfo->currentpos+event->vdouble;
            if(pos<0.0)
                pos=0.0;
            if(pos>is->slotinfo->length)
                pos=is->slotinfo->length;
            av_log(NULL, AV_LOG_DEBUG,"[debug] event_loop(): Slot: %d Pos: %7.2f  Event: seek rel: %12.6f (pause: %d)\n",is->slotinfo->slotid,get_master_clock(is),pos,is->paused);
            av_log(NULL, AV_LOG_DEBUG,"[debug] event_loop(): seek: %12.6f %lld\n",pos,(int64_t)(pos * AV_TIME_BASE));

            is->audio_clock = 0.0;
            is->audio_clock_diff_len = 0;
            is->audio_diff=0;

#ifndef USES_PAUSESEEK
            if (is->paused) {
                is->pause_seek = 1;
                is->pause_seek_pos = pos;
                is->pause_seek_curr = -1.0;
                toggle_pause(is);
            }
#endif
            stream_seek(is, (int64_t)(pos * AV_TIME_BASE), 0, 0);
            break;
        default:
            break;
        }
        av_free(event);
    }
}

/* Called from the main */
static void *player_process(void *data)
{
    slotinfo_t* slotinfo = (slotinfo_t*)data;
    VideoState *is = NULL;
#ifdef USE_DEBUG_LIB
    if(dmem && slotinfo->slotid<MAX_DEBUG_SLOT) {
        slotdebug_t* slotdebugs = debugmem_getslot(dmem);
        if(slotdebugs) {
            if(!slotdebugs[slotinfo->slotid].uses) {
                slotdebugs[slotinfo->slotid].slotid=slotinfo->slotid;
                slotdebugs[slotinfo->slotid].master_clock=0.0;
                slotdebugs[slotinfo->slotid].av_diff=0;
                slotdebugs[slotinfo->slotid].framedrop=0;
                slotdebugs[slotinfo->slotid].aqsize=0;
                slotdebugs[slotinfo->slotid].vqsize=0;
                slotdebugs[slotinfo->slotid].sqsize=0;
                slotdebugs[slotinfo->slotid].f1=0;
                slotdebugs[slotinfo->slotid].f2=0;
                slotdebugs[slotinfo->slotid].vqtime=0.0;
                slotdebugs[slotinfo->slotid].aqtime=0.0;
                slotdebugs[slotinfo->slotid].playtime=0;
                slotdebugs[slotinfo->slotid].uses=1;
            }
        }
    }
#endif

    slotinfo->status|=(STATUS_PLAYER_INITED);
    while(slotinfo->status) {
        if(safestrcmp(slotinfo->currenturl,slotinfo->url)) {
            if(slotinfo->currenturl) {
                av_free(slotinfo->currenturl);
                slotinfo->currenturl=NULL;
                slotinfo->reopen=0;
            }
            if(slotinfo->url) {
                slotinfo->currenturl=av_strdup(slotinfo->url);
            }
        }
        if(slotinfo->currenturl && slotinfo->playflag) {
            slotinfo->playflag=0;
            slotinfo->usesvda=0;
#ifdef THREAD_DEBUG
            av_log(NULL, AV_LOG_DEBUG,"[debug] player_process(): Open stream: %d %s *****************************\n\n",slotinfo->slotid,slotinfo->currenturl);
#endif
            slotinfo->callplayerstatus = 1;
            is = stream_open(slotinfo, slotinfo->currenturl, slotinfo->file_iformat);
        }
        if(is) {
            event_loop(slotinfo);
            is = slotinfo->streampriv;
        } else {
            usleep(40000);
        }
        if(slotinfo->doneflag)
            break;
    }
    slotinfo->status&=~(STATUS_PLAYER_INITED);
    /* never returns */
    if(slotinfo->url)
        av_free(slotinfo->url);
    if(slotinfo->opt_def)
        av_free(slotinfo->opt_def);
    if(slotinfo->sws_opts)
        sws_freeContext(slotinfo->sws_opts);
    slotinfo->freeable=1;
    return 0;
}
