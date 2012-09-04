  
#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>

#include "af.h"
#include "afilter/format.h"
#include "afilter/util.h"
#include "libxplayer.h"

#include "libavutil/log.h"

#define AUDIO_PREFILL           ((double)0.2)
#define AUDIO_PRELOAD           ((double)0.2)
#define AUDIO_BUFFER_SIZE       ((double)1.0)
#define AUDIO_ITERTIME          ((double)0.1)

#define AUDIO_RATE              48000
#define AUDIO_CHANNELS          2
#define AUDIO_FORMAT            AF_FORMAT_S16_LE

#define BUFFERED_PACKETS        0

typedef struct slotinfo_st slotinfo_t;

struct slotinfo_st {
    int                 slotid;
    int                 status;
    void*               playerpriv;
    char*               url;

    int                 playflag;
    int                 stopflag;
    int                 pauseflag;
    double              currentpos;
    double              realpos;
    double              length;
    double              fps;
    double              seekpos;
    int                 seekflag;
    int                 volume;

    int                 w;
    int                 h;
    int                 imgfmt;
    mp_image_t *        img;
    mp_image_t *        lastimg;

    video_info_t        video_info;
    audio_info_t        audio_info;

    pthread_t           thread;
    pthread_mutex_t     mutex;

    void*               af;
    af_data_t*          audio;
    pthread_mutex_t     audiomutex;

    slotinfo_t*         next;
    slotinfo_t*         prev;
};


typedef struct {
    slotinfo_t*         slotinfo;
    slotinfo_t*         slotinfo_chache[SLOT_MAX_CACHE];
    pthread_mutex_t     mutex;
    void*               af;
    af_data_t*          audio;
    double              audiopreload;
    double              audioprefill;
    double              audiobuffersize;
    double              itertime;
    int                 run;

    pthread_mutex_t     audiomutex;
    pthread_t           thread;
} xplayer_global_status_t;

typedef struct {
    mp_image_t*         img;
    mp_image_t *        pimg;
    int                 waitimage;
    int                 _seekflag;
} process_data_t;

static xplayer_global_status_t * xplayer_global_status = NULL;

static void *player_process(void *data);
static void *audio_process(void *data);

static void master_init(void) {
    if(xplayer_global_status) {
        return;
    }
    xplayer_global_status = malloc(sizeof(xplayer_global_status_t));
    memset(xplayer_global_status,0,sizeof(xplayer_global_status_t));
    pthread_mutex_init(&xplayer_global_status->mutex, NULL);
    pthread_mutex_init(&xplayer_global_status->audiomutex, NULL);
    xplayer_global_status->audio=af_empty(AUDIO_RATE, AUDIO_CHANNELS, AUDIO_FORMAT, 0, 0);
    xplayer_global_status->audiopreload=AUDIO_PRELOAD;
    xplayer_global_status->audioprefill=AUDIO_PREFILL;
    xplayer_global_status->audiobuffersize=AUDIO_BUFFER_SIZE;
    xplayer_global_status->itertime=AUDIO_ITERTIME;
    pthread_create(&xplayer_global_status->thread, NULL, audio_process, xplayer_global_status);
}

static slotinfo_t* get_slot_info(int slot) {
    slotinfo_t* slotinfo;

    master_init();
    pthread_mutex_lock(&xplayer_global_status->mutex);
    if(slot>=0 && slot<SLOT_MAX_CACHE) {
        slotinfo = xplayer_global_status->slotinfo_chache[slot];
        if(slotinfo) {
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
    if(!slotinfo) {
        slotinfo=malloc(sizeof(slotinfo_t));
        memset(slotinfo,0,sizeof(slotinfo_t));
        slotinfo->next=xplayer_global_status->slotinfo;
        if(slotinfo->next) {
            slotinfo->next->prev=slotinfo;
        }
        slotinfo->prev=NULL;
        xplayer_global_status->slotinfo=slotinfo;
        slotinfo->slotid=slot;
        slotinfo->volume=80;
        slotinfo->w=0;
        slotinfo->h=0;
        slotinfo->imgfmt=0;
        if(slot>=0 && slot<SLOT_MAX_CACHE) {
            xplayer_global_status->slotinfo_chache[slot]=slotinfo;
        }
        pthread_mutex_init(&slotinfo->mutex, NULL);
    }
    pthread_mutex_unlock(&xplayer_global_status->mutex);
    return slotinfo;
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

static void setimage(slotinfo_t* slotinfo, mp_image_t* img) {
    pthread_mutex_lock(&slotinfo->mutex);
    slotinfo->img=img;
    slotinfo->lastimg=img;
    pthread_mutex_unlock(&slotinfo->mutex);
}

static void clearimage(slotinfo_t* slotinfo, mp_image_t* img) {
    pthread_mutex_lock(&slotinfo->mutex);
    if(slotinfo->lastimg==img) {
        slotinfo->img=NULL;
    }
    pthread_mutex_unlock(&slotinfo->mutex);
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
//  av_log(NULL, AV_LOG_DEBUG,"src: %d dst: %d max: %d   old: %d %d + new %d %d\n",src->len,dest->len,maxlen,oldoffset,oldlen,srcoffset,srclen);
    tempdata=malloc(newlen);
    if(oldlen) {
        memcpy(tempdata,dest->audio+oldoffset,oldlen);
    }
    memcpy(tempdata+oldlen,src->audio+srcoffset,srclen);
    free(dest->audio);
    dest->audio=tempdata;
    dest->len=oldlen+srclen;
}

static void *audio_process(void *data) {
    slotinfo_t* slotinfo;
    af_data_t* af_data;
    int run = 1;
    double systime;
    double systimenext;
    double systimediff=0.0;
    double ftime;
    double ptimes=0.0;
    int ml,alen;
    int iterbytes;
    int len;

    xplayer_global_status->run=1;
    systime=xplayer_clock(NULL);
    alen=xplayer_global_status->audio->nch*format_2_bps(xplayer_global_status->audio->format)*xplayer_global_status->audio->rate;
    iterbytes=af_time2len(xplayer_global_status->audio,xplayer_global_status->itertime);
    while (run) {
        systimenext=xplayer_clock(NULL);
        ftime=ml=af_time2len(xplayer_global_status->audio, systimenext-systime+systimediff);
        systimediff+=(systimenext-systime)-af_len2time(xplayer_global_status->audio,ml);

        ftime/=alen;
        ml=af_round_len(xplayer_global_status->audio,ml);
        if(ml>=iterbytes) {

            ptimes=xplayer_clock(NULL);

            af_data=af_emptyfromdata(xplayer_global_status->audio, ml);
            pthread_mutex_lock(&xplayer_global_status->audiomutex);
            slotinfo=xplayer_global_status->slotinfo;
            while(slotinfo) {
                pthread_mutex_lock(&slotinfo->audiomutex);
                if(slotinfo->audio) {
                    len=ml;
                    if(slotinfo->audio->len<len) {
                        len=slotinfo->audio->len;
                    }
                    if(len) {
                        af_data=af_data_mixer(af_data, 0, len, slotinfo->audio);
                        af_drop_data(slotinfo->audio,len);
                    }
                }
                pthread_mutex_unlock(&slotinfo->audiomutex);
                slotinfo=(slotinfo_t*)slotinfo->next;
            }
            audio_append(xplayer_global_status->audio, af_data);
            af_data_free(af_data);
            pthread_mutex_unlock(&xplayer_global_status->audiomutex);
//            av_log(NULL, AV_LOG_DEBUG,"process: %d bytes buffer size: %d\n",ml,xplayer_global_status->audio->len);

            systime=systimenext;
        } else {
            ml=(xplayer_global_status->itertime-ptimes+xplayer_clock(NULL))*1000000.0;
            if(ml<30000 && ml>100)
                usleep(ml);
            else
                usleep(100);
        }
        if(!xplayer_global_status->run)
        {
            break;
        }
    }
    return NULL;
}

static int is_realtime(char* url) {
    if(url && !strncasecmp(url,"rtsp://",7)) {
        return 1;
    }
    return 0;
}


static void slot_audio_init(slotinfo_t* slotinfo, int rate, int nch, int format) {
    int preload;

    if(slotinfo->af) {
        af_uninit(slotinfo->af);
        slotinfo->af=NULL;
    }
    if(!xplayer_global_status->audio) {
        return;
    }
    slotinfo->af=af_init(xplayer_global_status->audio->rate, xplayer_global_status->audio->nch, xplayer_global_status->audio->format, xplayer_global_status->audio->bps, 10.0);
    pthread_mutex_lock(&slotinfo->audiomutex);
    slotinfo->audio=af_data_free(slotinfo->audio);
    slotinfo->audio=af_empty(rate,nch,format,0,0);
    av_log(NULL, AV_LOG_VERBOSE,"input: %d %d %d output: %d %d %d\n",rate,nch,format,xplayer_global_status->audio->rate, xplayer_global_status->audio->nch, xplayer_global_status->audio->format);
    preload=af_time2len(slotinfo->audio, xplayer_global_status->audiopreload);
    slotinfo->audio->audio=malloc(preload);
    memset(slotinfo->audio->audio,0,preload);
    slotinfo->audio->len=preload;
    pthread_mutex_unlock(&slotinfo->audiomutex);
}

static void slot_audio_uninit(slotinfo_t* slotinfo) {
    pthread_mutex_lock(&slotinfo->audiomutex);
    slotinfo->audio=af_data_free(slotinfo->audio);
    pthread_mutex_unlock(&slotinfo->audiomutex);
    if(slotinfo->af) {
        af_uninit(slotinfo->af);
        slotinfo->af=NULL;
    }
}

static void slot_audio_append(slotinfo_t* slotinfo, unsigned char * audio, int len) {
    af_data_t* af_data;
    af_data_t* af_conv;
    unsigned char* tempdata;
    int oldlen;
    int oldoffset;
    int convlen;
    int convoffset;
    int newlen;
    int maxlen;

    if(!slotinfo->audio || !slotinfo->af || !audio || !len) {
        return;
    }
    af_data=af_emptyfromdata(slotinfo->audio, len);
    maxlen=af_time2len(af_data, xplayer_global_status->audiobuffersize);
    memcpy(af_data->audio,audio,len);
    af_conv=af_play(slotinfo->af,af_data);
    pthread_mutex_lock(&slotinfo->audiomutex);
    convlen=af_conv->len;
    newlen=slotinfo->audio->len+convlen;
    if(newlen>maxlen) {
        newlen=maxlen;
    }
    oldlen=newlen-convlen;
    if(oldlen<0)
        oldlen=0;
    if(convlen>maxlen) {
        convlen=maxlen;
    }
    oldoffset=slotinfo->audio->len-oldlen;
    convoffset=af_conv->len-convlen;
    tempdata=malloc(newlen);
    if(oldlen && slotinfo->audio->audio) {
        memcpy(tempdata,slotinfo->audio->audio+oldoffset,oldlen);
    }
    memcpy(tempdata+oldlen,af_conv->audio+convoffset,convlen);
    free(slotinfo->audio->audio);
    slotinfo->audio->audio=tempdata;
    slotinfo->audio->len=oldlen+convlen;
    pthread_mutex_unlock(&slotinfo->audiomutex);
    af_data_free(af_data);
}

static double slot_audio_data_time(slotinfo_t* slotinfo, int master) {
    double ret;
    int len = 0;
    if(!slotinfo->audio || !slotinfo->audio->audio || !slotinfo->af) {
        return 0.0;
    }
    if(master) {
        pthread_mutex_lock(&xplayer_global_status->audiomutex);
        pthread_mutex_lock(&slotinfo->audiomutex);
        ret = af_data2time(slotinfo->audio);
        ret+=af_data2time(xplayer_global_status->audio);
        pthread_mutex_unlock(&slotinfo->audiomutex);
        pthread_mutex_unlock(&xplayer_global_status->audiomutex);
    } else {
        pthread_mutex_lock(&slotinfo->audiomutex);
        ret = af_data2time(slotinfo->audio);
        if(slotinfo->audio) {
            len = slotinfo->audio->len;
        }
        pthread_mutex_unlock(&slotinfo->audiomutex);
//av_log(NULL, AV_LOG_DEBUG,"slot time: %12.6f ms len: %d\n",ret*1000.0,len);
    }
    return ret;
}

void read_stream(slotinfo_t* slotinfo, process_data_t* process_data) {
    if(xplayer_read(slotinfo->playerpriv)) {
        av_log(NULL, AV_LOG_ERROR, "read error\n");
        slotinfo->status&=~(STATUS_PLAYER_IMAGE|STATUS_PLAYER_OPENED|STATUS_PLAYER_CONNECT);
        slotinfo->status|=STATUS_PLAYER_ERROR;
        slot_audio_uninit(slotinfo);
        clearimage(slotinfo,process_data->img);
        xplayer_close(slotinfo->playerpriv);
        slotinfo->length=0.0;
        slotinfo->fps=0.0;
        memset((char*)&(slotinfo->video_info),0,sizeof(video_info_t));
        memset((char*)&(slotinfo->audio_info),0,sizeof(audio_info_t));
        process_data->img=NULL;
    } else {
        xplayer_cache_read(slotinfo->playerpriv);
    }
}

double process_audio(slotinfo_t* slotinfo, process_data_t* process_data, int noread) {
    unsigned char* adata = NULL;
    int alen = 0;

    if(!noread) {
        read_stream(slotinfo, process_data);
    }

    if(xplayer_aloop(slotinfo->playerpriv)) {
        av_log(NULL, AV_LOG_ERROR, "loop error\n");
        slotinfo->status&=~(STATUS_PLAYER_IMAGE|STATUS_PLAYER_OPENED|STATUS_PLAYER_CONNECT);
        slotinfo->status|=STATUS_PLAYER_ERROR;
        slot_audio_uninit(slotinfo);
        clearimage(slotinfo,process_data->img);
        xplayer_close(slotinfo->playerpriv);
        slotinfo->length=0.0;
        slotinfo->fps=0.0;
        memset((char*)&(slotinfo->video_info),0,sizeof(video_info_t));
        memset((char*)&(slotinfo->audio_info),0,sizeof(audio_info_t));
        process_data->img=NULL;
        return 0.0;
    }
    if((alen=xplayer_audiolen(slotinfo->playerpriv))) {
        xplayer_audio_info(slotinfo->playerpriv, &(slotinfo->audio_info));
        adata=(unsigned char*)xplayer_audio(slotinfo->playerpriv);
        if(slotinfo->audio_info.format) {
            if(!slotinfo->af) {
                slot_audio_init(slotinfo, slotinfo->audio_info.rate, slotinfo->audio_info.channels, slotinfo->audio_info.format);
            }
            if(!(slotinfo->status&STATUS_PLAYER_PAUSE)) {
                slot_audio_append(slotinfo, adata, alen);
            }
        }
        free(adata);
    }
    return xplayer_apts(slotinfo->playerpriv);
}

double process_video(slotinfo_t* slotinfo, process_data_t* process_data, int noread) {

    if(!noread) {
        read_stream(slotinfo, process_data);
    }
    if(xplayer_vloop(slotinfo->playerpriv)) {
        av_log(NULL, AV_LOG_ERROR, "loop error\n");
        slotinfo->status&=~(STATUS_PLAYER_IMAGE|STATUS_PLAYER_OPENED|STATUS_PLAYER_CONNECT);
        slotinfo->status|=STATUS_PLAYER_ERROR;
        slot_audio_uninit(slotinfo);
        clearimage(slotinfo,process_data->img);
        xplayer_close(slotinfo->playerpriv);
        slotinfo->length=0.0;
        slotinfo->fps=0.0;
        memset((char*)&(slotinfo->video_info),0,sizeof(video_info_t));
        memset((char*)&(slotinfo->audio_info),0,sizeof(audio_info_t));
        process_data->img=NULL;
        return 0.0;
    }
    if(xplayer_isnewimage(slotinfo->playerpriv)) {
        process_data->img=xplayer_getimage(slotinfo->playerpriv);
        process_data->waitimage=0;
        slotinfo->status|=STATUS_PLAYER_IMAGE;
    }
    return xplayer_vpts(slotinfo->playerpriv);
}

void process_videoimage(slotinfo_t* slotinfo, process_data_t* process_data) {
    if(process_data->img && (slotinfo->status & STATUS_PLAYER_IMAGE) && (slotinfo->status & STATUS_PLAYER_OPENED)) {
        if((slotinfo->status&STATUS_PLAYER_PAUSE) || process_data->_seekflag) {
            if(!(slotinfo->status&STATUS_PLAYER_PAUSE_IMG) || process_data->_seekflag) {
                if(process_data->_seekflag==2)
                    process_data->_seekflag=0;
                slotinfo->status|=STATUS_PLAYER_PAUSE_IMG;
                if(process_data->pimg && (process_data->img->w!=process_data->pimg->w || process_data->img->h!=process_data->pimg->h || process_data->img->imgfmt!=process_data->pimg->imgfmt)) {
                    clearimage(slotinfo,process_data->pimg);
                    free_mp_image(process_data->pimg);
                    process_data->pimg=NULL;
                }
                if(!process_data->pimg) {
                    process_data->pimg=alloc_mpi(process_data->img->w, process_data->img->h, process_data->img->imgfmt);
                }
                if(process_data->pimg) {
                    copy_mpi(process_data->pimg, process_data->img);
                }
                if(process_data->_seekflag!=1) {
                    setimage(slotinfo,process_data->pimg);
                    xplayer_video_info(slotinfo->playerpriv, &(slotinfo->video_info));
                }
            }
        } else {
            slotinfo->status&=~STATUS_PLAYER_PAUSE_IMG;
            if(process_data->pimg) {
                clearimage(slotinfo,process_data->pimg);
                free_mp_image(process_data->pimg);
                process_data->pimg=NULL;
            }
            setimage(slotinfo,process_data->img);
            slotinfo->status&=~STATUS_PLAYER_IMAGE;
        }
    }
}

void *player_process(void *data) {
    slotinfo_t* slotinfo = (slotinfo_t*)data;
    process_data_t* process_data = (process_data_t*)calloc(1,sizeof(process_data_t));
    int run = 1;
    mp_image_t * bimg = NULL;

    int avloglevel = 0;
    int realtime = 0;

    int arate = 0;
    int ach = 0;
    unsigned int afmt = 0;

    int noaudio=0;
    double pts=0.0;
    double vpts=0.0;
    double apts=0.0;
    double currtime=0.0;
    double lasttime=0.0;
    double prevtime=0.0;
    double lastvpts=0.0;
    double prevvpts=0.0;
    double sleeptime=0.0;
    double maxsleep=0.0;
    double seeklastvpts=0.0;
    double abuffertime=0.0;
    double reseekdiff=0.0;

    double _sleeptime=0.0;

    double dpts=0.0;
    double sync=0.0;
    double startpts=0.0;
    double _pts=0.0;

    int loop;
    int audioread = 0;
    int dropframe = 0;

    char* url = NULL;
    int w = 0;
    int h = 0;
    int imgfmt=0;
    int playflag=0;
    int stopflag=0;
    int pauseflag=0;
    double seekpos=0;
    int seekflag=0;
    double pausepos=0.0;
    double _seekpos=0.0;

    char* slot_url = NULL;
    int slot_w = 0;
    int slot_h = 0;
    int slot_imgfmt = 0;
    int slot_pauseflag=0;

    av_log(NULL, AV_LOG_VERBOSE, "process start... %d\n",slotinfo->slotid);
    pthread_mutex_lock(&slotinfo->mutex);
    slotinfo->status=STATUS_PLAYER_STARTED;
    pthread_mutex_unlock(&slotinfo->mutex);
    while(run) {
        pthread_mutex_lock(&slotinfo->mutex);
        run=slotinfo->status;
        slot_w = slotinfo->w;
        slot_h = slotinfo->h;
        slot_imgfmt = slotinfo->imgfmt;
        if(safestrcmp(slot_url,slotinfo->url)) {
            if(slot_url) {
                free(slot_url);
                slot_url=NULL;
            }
            if(slotinfo->url) {
                slot_url=strdup(slotinfo->url);
            }
        }
        playflag=slotinfo->playflag;
        stopflag=slotinfo->stopflag;
        slotinfo->playflag=0;
        slotinfo->stopflag=0;
        slot_pauseflag=slotinfo->pauseflag;
        seekpos=slotinfo->seekpos;
        if(slotinfo->seekflag) {
            seekpos=slotinfo->seekpos;
            seekflag=1;
            slotinfo->seekflag=0;
        }
        pthread_mutex_unlock(&slotinfo->mutex);

        if(!slotinfo->playerpriv) {
            slotinfo->playerpriv = xplayer_init(avloglevel);
            pthread_mutex_lock(&slotinfo->mutex);
            slotinfo->status|=STATUS_PLAYER_INITED;
            pthread_mutex_unlock(&slotinfo->mutex);
        }
        if(slot_w!=w || slot_h!=h) {
            if(bimg)
                free_mp_image(bimg);
            bimg=NULL;
        }
        if(slot_w!=w || slot_h!=h || slot_imgfmt!=imgfmt) {
            w=slot_w;
            h=slot_h;
            imgfmt=slot_imgfmt;
            xplayer_setimage(slotinfo->playerpriv, w, h, imgfmt);
        }
        if(!bimg && w && h && imgfmt) {
            bimg=alloc_mpi(w, h, imgfmt);
        }
        if(safestrcmp(slot_url,url)) {
            stopflag=1;
            if(url) {
                free(url);
                url=NULL;
                process_data->waitimage=0;
            }
            if(slot_url) {
                url=strdup(slot_url);
            }
        }
        if(stopflag) {
            slotinfo->status&=~STATUS_PLAYER_IMAGE;
            if((slotinfo->status & STATUS_PLAYER_OPENED)) {
                slot_audio_uninit(slotinfo);
                clearimage(slotinfo,process_data->img);
                xplayer_close(slotinfo->playerpriv);
                slotinfo->status&=~(STATUS_PLAYER_IMAGE|STATUS_PLAYER_OPENED|STATUS_PLAYER_CONNECT|STATUS_PLAYER_ERROR|STATUS_PLAYER_PAUSE);
                slotinfo->realpos=0.0;
                slotinfo->currentpos=0.0;
                slotinfo->length=0.0;
                slotinfo->fps=0.0;
                pausepos=0.0;
                lasttime=0.0;
                vpts=0.0;
                apts=0.0;
                prevvpts=0.0;
                memset((char*)&(slotinfo->video_info),0,sizeof(video_info_t));
                memset((char*)&(slotinfo->audio_info),0,sizeof(audio_info_t));
            }
            process_data->img=NULL;
            process_data->waitimage=0;
            stopflag=0;
        }
        if(!url) {
            usleep(50000);
            continue;
        }
        if(url && playflag) {
            if(!(slotinfo->status & STATUS_PLAYER_OPENED)) {
                pausepos=0;
                lasttime=0.0;
                slotinfo->status&=~(STATUS_PLAYER_IMAGE|STATUS_PLAYER_OPENED|STATUS_PLAYER_CONNECT|STATUS_PLAYER_ERROR|STATUS_PLAYER_IMAGE|STATUS_PLAYER_PAUSE);
                slotinfo->status|=STATUS_PLAYER_CONNECT;
                memset((char*)&(slotinfo->video_info),0,sizeof(video_info_t));
                memset((char*)&(slotinfo->audio_info),0,sizeof(audio_info_t));
                slotinfo->realpos=0.0;
                slotinfo->currentpos=0.0;
                slotinfo->length=0.0;
                slotinfo->fps=0.0;
                vpts=0.0;
                apts=0.0;
                prevvpts=0.0;
                xplayer_set_buffered_packet(slotinfo->playerpriv, BUFFERED_PACKETS);
                if(xplayer_open(slotinfo->playerpriv, url, 0)) {
                    slotinfo->status&=~(STATUS_PLAYER_OPENED|STATUS_PLAYER_CONNECT|STATUS_PLAYER_IMAGE);
                    slotinfo->status|=STATUS_PLAYER_ERROR;
                } else {
                    slotinfo->status|=STATUS_PLAYER_OPENED;
                    slotinfo->status&=~(STATUS_PLAYER_ERROR|STATUS_PLAYER_IMAGE|STATUS_PLAYER_CONNECT);
                    clearimage(slotinfo,process_data->img);
                    process_data->waitimage=1;
                    slotinfo->length=xplayer_duration(slotinfo->playerpriv);
                    slotinfo->fps=xplayer_framerate(slotinfo->playerpriv);
                    realtime=is_realtime(url);
                    process_data->_seekflag=0;
                }
                noaudio=1;
                if(xplayer_audiorate(slotinfo->playerpriv)) {
                    arate = xplayer_audiorate(slotinfo->playerpriv);
                    ach =  xplayer_audioch(slotinfo->playerpriv);
                    afmt = xplayer_audiofmt(slotinfo->playerpriv);
                    xplayer_audio_info(slotinfo->playerpriv, &(slotinfo->audio_info));
                    noaudio=0;
                }
                xplayer_video_info(slotinfo->playerpriv, &(slotinfo->video_info));
            }
            playflag=0;
        }
        if(!(slotinfo->status & STATUS_PLAYER_OPENED)) {
            usleep(50000);
            continue;
        }
        if(pauseflag!=slot_pauseflag) {
            if(slot_pauseflag) {
                slotinfo->status|=STATUS_PLAYER_PAUSE;
                pausepos=slotinfo->realpos;
                process_data->_seekflag=0;
                seekflag=0;
                lasttime=0.0;
            } else {
                slotinfo->status&=~STATUS_PLAYER_PAUSE;
                seekpos=pausepos;
                seekflag=1;
                pausepos=0.0;
                process_data->_seekflag=0;
                lasttime=0.0;
            }
            pauseflag=slot_pauseflag;
        }
        if((slotinfo->status & STATUS_PLAYER_OPENED) && url && seekflag) {
            seekflag=0;
            _seekpos=seekpos;
            reseekdiff=0.0;
            xplayer_seek(slotinfo->playerpriv, seekpos);
            av_log(NULL, AV_LOG_VERBOSE,"seek: %12.6f vpts: %12.6f\n",seekpos,vpts);
            seeklastvpts=vpts;
            vpts=0.0;
            vpts=0.0;
            apts=0.0;
            prevvpts=0.0;
            lasttime=0.0;
            if((slotinfo->status & STATUS_PLAYER_PAUSE) /*&& realtime*/) {
                process_data->_seekflag=1;
            }
        }
        if(startpts==0.0) {
            startpts=xplayer_clock(slotinfo->playerpriv);
        }
        pts=xplayer_clock(slotinfo->playerpriv)-startpts+sync;
        dpts=vpts-pts;

        if(url && (slotinfo->status & STATUS_PLAYER_OPENED)) {
            if(vpts) {
                slotinfo->realpos=vpts;
            }
        } else {
            slotinfo->realpos=0.0;
        }
        if(url && (slotinfo->status & STATUS_PLAYER_OPENED)) {
            if(!(slotinfo->status & STATUS_PLAYER_PAUSE) || _seekpos==1)
                slotinfo->currentpos=slotinfo->realpos;
        } else {
            slotinfo->currentpos=0.0;
        }
        if((slotinfo->status & STATUS_PLAYER_PAUSE) && process_data->_seekflag==1) {
            if(!realtime && vpts > _seekpos+1.0 && vpts!=0.0 && vpts!=seeklastvpts) {
                reseekdiff += vpts-_seekpos;
                if(reseekdiff>seekpos)
                    reseekdiff=seekpos;
                xplayer_seek(slotinfo->playerpriv, seekpos-reseekdiff);
                seeklastvpts=vpts;
                av_log(NULL, AV_LOG_VERBOSE, "reseek: %12.6f vpts: %12.6f\n",seekpos-reseekdiff,vpts);
                vpts=0.0;
                apts=0.0;
                lasttime=0.0;
            }
            if(slotinfo->realpos>=_seekpos && slotinfo->realpos<_seekpos+1.0) {
                process_data->_seekflag=2;
                slotinfo->currentpos=slotinfo->realpos;
                pausepos=slotinfo->realpos;
            }
        }
        loop = 0;
        if(url && (slotinfo->status & STATUS_PLAYER_OPENED)) {
            if(realtime) {
                loop = 1;
            } else if (!(slotinfo->status&STATUS_PLAYER_PAUSE)) {
                loop=0;
            }
        }
        if(!url || !(slotinfo->status & STATUS_PLAYER_OPENED)) {
            usleep(50000);
            continue;
        }

        if(realtime) {
            read_stream(slotinfo, process_data);
        }

        if(!noaudio) {
            audioread=0;
            if((slot_audio_data_time(slotinfo,0)<AUDIO_PRELOAD*4 && !(slotinfo->status&STATUS_PLAYER_PAUSE))) {
                audioread=1;
            }
            if(realtime || (!(slotinfo->status&STATUS_PLAYER_PAUSE) && (loop || audioread)) || (process_data->_seekflag==1)) {
                _pts=process_audio(slotinfo, process_data, realtime);
                if(_pts!=0.0)
                    apts=_pts;
            } else {
//av_log(NULL, AV_LOG_DEBUG,"no read audio: %12.6f ms\n",slot_audio_data_time(slotinfo,0)*1000.0);
            }
            if(!url && !(slotinfo->status & STATUS_PLAYER_OPENED)) {
                continue;
            }
        }
        if(slotinfo->fps>0.0) {
            maxsleep=1/slotinfo->fps/2;
        }
        if(maxsleep<=0.0 || maxsleep>0.1) {
            maxsleep=0.1;
        }
        lasttime=currtime;
        currtime=xplayer_clock(slotinfo->playerpriv);
        dropframe=0;
        if(noaudio) {
            abuffertime=0;
        } else {
            abuffertime=slot_audio_data_time(slotinfo, 1);
            if(apts-vpts+abuffertime<0.0) {
                dropframe=0;
//av_log(NULL, AV_LOG_DEBUG,"drop frame : apts: %12.6f - vpts: %12.6f + buf: %12.6f = %12.6f\n",apts,vpts,abuffertime,apts-vpts+abuffertime);
            }
        }
        if(realtime) {
            _pts=process_video(slotinfo, process_data, realtime);
            process_videoimage(slotinfo, process_data);
            if(_pts!=0.0) {
                lastvpts=vpts;
                slotinfo->realpos=vpts=_pts;
            }
        } else if(apts-vpts-abuffertime>0.0 || apts-vpts-abuffertime<-5.0 || process_data->_seekflag==1 || noaudio) {
            _pts=process_video(slotinfo, process_data, realtime);
            if(!dropframe) {
                process_videoimage(slotinfo, process_data);
            }
            if(_pts!=0.0) {
                lastvpts=vpts;
                slotinfo->realpos=vpts=_pts;
            } else if(slotinfo->fps>0.0 && !(slotinfo->status&STATUS_PLAYER_PAUSE)) {
                lastvpts=vpts;
                _pts=vpts+(1/slotinfo->fps);
                slotinfo->realpos=vpts=_pts;
            }
//av_log(NULL, AV_LOG_DEBUG,"video: %12.6f (%12.6f)\n",vpts,_pts);
            if(prevtime==0.0) {
                prevtime=currtime;
            }
            sleeptime=(vpts-prevvpts)-(currtime-prevtime);
//            av_log(NULL, AV_LOG_DEBUG,"+ V: %12.6f A: %12.6f A/V: %12.6f (%12.6f) sleep: %12.6f ms\n", vpts, apts, vpts-apts+abuffertime, abuffertime,sleeptime*1000.0);
//            sleeptime=(currtime-prevtime)-(vpts-prevvpts);
            if(sleeptime>0.0 && !process_data->_seekflag && !realtime && lastvpts>0.0 && !dropframe && noaudio) {
//av_log(NULL, AV_LOG_DEBUG,"Sleep: %12.6fms time: %12.6f pts: %12.6f\n",sleeptime*1000.0,(currtime-prevtime)*1000.0,(vpts-prevvpts)*1000.0);
                if(sleeptime>maxsleep)
                    sleeptime=maxsleep;
//                usleep(sleeptime*1e6);
                prevvpts=vpts;
            }
            prevtime=xplayer_clock(slotinfo->playerpriv);
            if(!url && !(slotinfo->status & STATUS_PLAYER_OPENED)) {
                continue;
            }
        } else {
            if(apts-vpts-abuffertime<0.0 && !realtime) {
//                sleeptime=-(apts-vpts-abuffertime);
                sleeptime=0.04-abuffertime;
                _sleeptime=sleeptime;
                if(sleeptime>maxsleep)
                    sleeptime=maxsleep;
                av_log(NULL, AV_LOG_VERBOSE,"* V: %12.6f A: %12.6f A/V: %12.6f (%12.6f) sleep: %12.6f ms (max: %12.6f ms %12.6f)\n", vpts, apts, apts-vpts-abuffertime, abuffertime, sleeptime*1000.0,maxsleep*1000.0,_sleeptime*1000.0);
//                av_log(NULL, AV_LOG_VERBOSE,"Sleep: %12.6fms\n",sleeptime*1000.0);
                if(sleeptime>0.0)
                {
                    usleep(sleeptime*1e6);
                }
            }
        }
//        av_log(NULL, AV_LOG_DEBUG,"fps: %6.3f\n",xplayer_framerate(slotinfo->playerpriv));
#if 0
av_log(NULL, AV_LOG_DEBUG,"Read: %12.6f Actual: %12.6f Buffered: %12.6f\n", xplayer_rpts(slotinfo->playerpriv),xplayer_pts(slotinfo->playerpriv),xplayer_rpts(slotinfo->playerpriv)-xplayer_pts(slotinfo->playerpriv));
#endif
    }
    pthread_mutex_lock(&slotinfo->mutex);
    slotinfo->img=NULL;
    pthread_mutex_unlock(&slotinfo->mutex);
    if(bimg)
        free_mp_image(bimg);
    if(process_data->pimg)
        free_mp_image(process_data->pimg);
    clearimage(slotinfo,process_data->img);
    slot_audio_uninit(slotinfo);
    xplayer_close(slotinfo->playerpriv);
    xplayer_uninit(slotinfo->playerpriv);
    slotinfo->playflag=0;
    slotinfo->length=0.0;
    slotinfo->fps=0.0;
    slotinfo->playerpriv=NULL;
    memset((char*)&(slotinfo->video_info),0,sizeof(video_info_t));
    memset((char*)&(slotinfo->audio_info),0,sizeof(audio_info_t));
    free(process_data);
    return NULL;
}


/// ----------------------------------------------------------

void xplayer_API_init() {
    master_init();
}

void xplayer_API_done() {
    slotinfo_t* slotinfo;
    slotinfo_t* nextslotinfo;

    if(xplayer_global_status) {
        pthread_mutex_lock(&xplayer_global_status->mutex);
        slotinfo=xplayer_global_status->slotinfo;
        while(slotinfo) {
            pthread_mutex_lock(&slotinfo->mutex);
            if(slotinfo->status)
                slotinfo->status=0;
            pthread_mutex_unlock(&slotinfo->mutex);
            if(slotinfo->thread)
                pthread_join(slotinfo->thread, NULL);
            pthread_mutex_destroy(&slotinfo->mutex);
            if(slotinfo->playerpriv)
                xplayer_uninit(slotinfo->playerpriv);
            if(slotinfo->url)
                free(slotinfo->url);
            nextslotinfo=slotinfo->next;
            free(slotinfo);
            slotinfo=nextslotinfo;
        }
        pthread_mutex_unlock(&xplayer_global_status->mutex);
        pthread_mutex_destroy(&xplayer_global_status->mutex);
        xplayer_global_status->run=0;
        if(xplayer_global_status->thread)
                 pthread_join(xplayer_global_status->thread, NULL);
        xplayer_global_status->audio=af_data_free(xplayer_global_status->audio);
        pthread_mutex_destroy(&xplayer_global_status->audiomutex);
        free(xplayer_global_status);
        xplayer_global_status=NULL;
    }
}

void xplayer_API_slotfree(int slot) {
    slotinfo_t* slotinfo = get_slot_info(slot);
    slotinfo_t* next;
    slotinfo_t* prev;

    pthread_mutex_lock(&slotinfo->mutex);
    if(slotinfo->status)
        slotinfo->status=0;
    pthread_mutex_unlock(&slotinfo->mutex);
    if(slotinfo->thread)
    pthread_join(slotinfo->thread, NULL);
    if(slotinfo->playerpriv)
        xplayer_uninit(slotinfo->playerpriv);
    if(slotinfo->url)
        free(slotinfo->url);
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
    free(slotinfo);
    pthread_mutex_unlock(&xplayer_global_status->mutex);
}


video_info_t* xplayer_API_getvideoformat(int slot) {
    slotinfo_t* slotinfo = get_slot_info(slot);
    if(!slotinfo->status) {
        return NULL;
    }
    video_info_t* ret = malloc(sizeof(video_info_t));
    memcpy(ret,(char*)&(slotinfo->video_info),sizeof(video_info_t));
    return ret;
}

audio_info_t* xplayer_API_getaudioformat(int slot) {
    slotinfo_t* slotinfo = get_slot_info(slot);
    if(!slotinfo->status) {
        return NULL;
    }
    audio_info_t* ret = malloc(sizeof(audio_info_t));
    memcpy(ret,(char*)&(slotinfo->audio_info),sizeof(audio_info_t));
    return ret;
}

double xplayer_API_getmovielength(int slot) {
    slotinfo_t* slotinfo = get_slot_info(slot);
    double length = 0.0;

    if(!slotinfo->status) {
        return length;
    }
    pthread_mutex_lock(&slotinfo->mutex);
    if(slotinfo->url && slotinfo->length>0.0) {
        length = slotinfo->length;
    }
    pthread_mutex_unlock(&slotinfo->mutex);
    return length;
}

int xplayer_API_setimage(int slot, int w, int h, unsigned int fmt) {
    slotinfo_t* slotinfo = get_slot_info(slot);

    pthread_mutex_lock(&slotinfo->mutex);
    slotinfo->w=w;
    slotinfo->h=h;
    slotinfo->imgfmt=fmt;
    pthread_mutex_unlock(&slotinfo->mutex);
    return 0;
}

int xplayer_API_loadurl(int slot, char* url) {
    slotinfo_t* slotinfo = get_slot_info(slot);

    if(!url) {
        return -1;
    }
    pthread_mutex_lock(&slotinfo->mutex);
    if(!slotinfo->status && !slotinfo->thread) {
        pthread_create(&slotinfo->thread, NULL, player_process, slotinfo);
    }
    if(slotinfo->url)
        free(slotinfo->url);
    slotinfo->url=strdup(url);
    pthread_mutex_unlock(&slotinfo->mutex);
    return 0;
}

char* xplayer_API_geturl(int slot) {
    slotinfo_t* slotinfo = get_slot_info(slot);
    char* url = NULL;

    pthread_mutex_lock(&slotinfo->mutex);
    if(slotinfo->status) {
        url=slotinfo->url;
    }
    pthread_mutex_unlock(&slotinfo->mutex);
    return url;
}

int xplayer_API_unloadurl(int slot) {
    slotinfo_t* slotinfo = get_slot_info(slot);
    int ret = -1;

    pthread_mutex_lock(&slotinfo->mutex);
    slotinfo->stopflag=1;
    slotinfo->pauseflag=0;
    if(slotinfo->url && slotinfo->status) {
        free(slotinfo->url);
        slotinfo->url=NULL;
        ret=0;
    }
    pthread_mutex_unlock(&slotinfo->mutex);
    return ret;
}

int xplayer_API_play(int slot) {
    slotinfo_t* slotinfo = get_slot_info(slot);
    int ret = -1;

    pthread_mutex_lock(&slotinfo->mutex);
    if(slotinfo->url) {
        slotinfo->playflag=1;
        if(slotinfo->pauseflag) {
            slotinfo->pauseflag=0;
            slotinfo->seekflag=1;
            slotinfo->seekpos=slotinfo->currentpos;
        }
        ret=0;
    }
    pthread_mutex_unlock(&slotinfo->mutex);
    return ret;
}

int xplayer_API_pause(int slot) {
    slotinfo_t* slotinfo = get_slot_info(slot);
    int ret = -1;

    pthread_mutex_lock(&slotinfo->mutex);
    if(slotinfo->url && slotinfo->status) {
        if(!slotinfo->pauseflag)
            slotinfo->pauseflag=1;
        ret=0;
    }
    pthread_mutex_unlock(&slotinfo->mutex);
    return ret;
}

int xplayer_API_stop(int slot) {
    slotinfo_t* slotinfo = get_slot_info(slot);
    int ret = -1;

    pthread_mutex_lock(&slotinfo->mutex);
    if(slotinfo->url && slotinfo->status) {
        slotinfo->stopflag=1;
        ret=0;
    }
    pthread_mutex_unlock(&slotinfo->mutex);
    return ret;
}

int xplayer_API_seek(int slot, double pos) {
    slotinfo_t* slotinfo = get_slot_info(slot);
    int ret = -1;

    pthread_mutex_lock(&slotinfo->mutex);
    if(slotinfo->url && slotinfo->status) {
        slotinfo->seekpos=pos;
        slotinfo->seekflag=1;
    }
    pthread_mutex_unlock(&slotinfo->mutex);
    return ret;
}

int xplayer_API_volume(int slot, int volume) {
    slotinfo_t* slotinfo = get_slot_info(slot);

    if(volume<0 || volume>100) {
        return -1;
    }
    pthread_mutex_lock(&slotinfo->mutex);
    slotinfo->volume=volume;
    pthread_mutex_unlock(&slotinfo->mutex);
    return 0;
}

int xplayer_API_getvolume(int slot) {
    slotinfo_t* slotinfo = get_slot_info(slot);
    int ret;

    pthread_mutex_lock(&slotinfo->mutex);
    ret=slotinfo->volume;
    pthread_mutex_unlock(&slotinfo->mutex);
    return ret;
}


int xplayer_API_getstatus(int slot) {
    slotinfo_t* slotinfo = get_slot_info(slot);
    int ret;

    pthread_mutex_lock(&slotinfo->mutex);
    ret=slotinfo->status;
    pthread_mutex_unlock(&slotinfo->mutex);
    return ret;
}

double xplayer_API_getcurrentpts(int slot) {
    slotinfo_t* slotinfo = get_slot_info(slot);
    double currentpos = 0.0;

    pthread_mutex_lock(&slotinfo->mutex);
    if(slotinfo->url && slotinfo->status) {
        currentpos = slotinfo->currentpos;
    }
    pthread_mutex_unlock(&slotinfo->mutex);
    return currentpos;
}

double xplayer_API_getrealpts(int slot) {
    slotinfo_t* slotinfo = get_slot_info(slot);
    double realpos = 0.0;

    pthread_mutex_lock(&slotinfo->mutex);
    if(slotinfo->url && slotinfo->status) {
        realpos = slotinfo->realpos;
    }
    pthread_mutex_unlock(&slotinfo->mutex);
    return realpos;
}

double xplayer_API_getfps(int slot) {
    slotinfo_t* slotinfo = get_slot_info(slot);
    double fps = 0.0;

    pthread_mutex_lock(&slotinfo->mutex);
    if(slotinfo->url && slotinfo->status) {
        fps = slotinfo->fps;
    }
    pthread_mutex_unlock(&slotinfo->mutex);
    return fps;
}

int xplayer_API_getimage(int slot, mp_image_t** img) {
    slotinfo_t* slotinfo = get_slot_info(slot);
    int ret = -1;

    if(!img) {
        return ret;
    }
    pthread_mutex_lock(&slotinfo->mutex);
    if(slotinfo->url && slotinfo->status) {
        if(slotinfo->img) {
            if((*img) && ((*img)->w!=slotinfo->img->w || (*img)->h!=slotinfo->img->h || (*img)->imgfmt!=slotinfo->imgfmt)) {
                free_mp_image((*img));
                (*img)=NULL;
            }
            if(!(*img)) {
                (*img)=alloc_mpi(slotinfo->img->w, slotinfo->img->h, slotinfo->img->imgfmt);
            }
            if((*img)) {
                copy_mpi((*img),slotinfo->img);
            }
            slotinfo->img=NULL;
            ret=1;
        } else {
            ret=0;
        }
    }
    pthread_mutex_unlock(&slotinfo->mutex);
    return ret;
}

int xplayer_API_prefillaudio(char* buffer, int bufferlen, int playlen) {
    int len;

    master_init();
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

    master_init();
    len = af_time2len(xplayer_global_status->audio, xplayer_global_status->audioprefill);
    return len;
}

int xplayer_API_getaudio(char* buffer, int bufferlen) {
    int len;

    master_init();
    pthread_mutex_lock(&xplayer_global_status->audiomutex);
    if(xplayer_global_status->audio) {
        len=xplayer_global_status->audio->len;
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
    master_init();
    if(xplayer_global_status->audio)
        return xplayer_global_status->audio->rate;
    return 0;
}

int xplayer_API_getaudio_channels() {
    master_init();
    if(xplayer_global_status->audio)
        return xplayer_global_status->audio->nch;
    return 0;
}

int xplayer_API_getaudio_format() {
    master_init();
    if(xplayer_global_status->audio)
        return xplayer_global_status->audio->format;
    return 0;
}

static int wav_le_int(int i)
{
   int ret=i;
#ifdef WORDS_BIGENDIAN
   ret =  i>>24;
   ret += (i>>8)&0x0000ff00;
   ret += (i<<8)&0x00ff0000;
   ret += (i<<24);
#endif
   return ret;
}

unsigned short wav_le_short(unsigned short s)
{
   unsigned short ret=s;
#ifdef WORDS_BIGENDIAN
   ret =  s>>8;
   ret += s<<8;
#endif
   return ret;
}

char* writebuffer(char* dst, void* src, int len, int* size, int maxsize) {
    if(size) {
        if((*size)+len>maxsize) {
            return dst;
        }
        *size+=len;
    }
    if(len && dst && src) {
        memcpy(dst,src,len);
        dst+=len;
    }
    return dst;
}

int xplayer_API_wav_header(char* buffer, int bufferlen, int rate, int channels, int format, int size)
{
    int itmp;
    short stmp;
    char ttmp[10];
    char* ptr = buffer;
    int len = 0;

    if(size<0) {
        size=0x7fffffff;
    }
    snprintf(ttmp,sizeof(ttmp),"RIFF");
    ptr=writebuffer(ptr, &ttmp, strlen(ttmp), &len, bufferlen);
    itmp = 0x7fffffff;
    ptr=writebuffer(ptr, &itmp, 4, &len, size);
    snprintf(ttmp,sizeof(ttmp),"WAVEfmt ");
    ptr=writebuffer(ptr, &ttmp, strlen(ttmp), &len, bufferlen);
    itmp = wav_le_int(16);
    ptr=writebuffer(ptr, &itmp, 4, &len, bufferlen);
    stmp = wav_le_short(1);
    ptr=writebuffer(ptr, &stmp, 2, &len, bufferlen);
    stmp = wav_le_short(channels);
    ptr=writebuffer(ptr, &stmp, 2, &len, bufferlen);
    itmp = wav_le_int(rate);
    ptr=writebuffer(ptr, &itmp, 4, &len, bufferlen);
    itmp = wav_le_int(rate*channels*2);
    ptr=writebuffer(ptr, &itmp, 4, &len, bufferlen);
    stmp = wav_le_short(2*channels);
    ptr=writebuffer(ptr, &stmp, 2, &len, bufferlen);
    stmp = wav_le_short(16);
    ptr=writebuffer(ptr, &stmp, 2, &len, bufferlen);
    snprintf(ttmp,sizeof(ttmp),"data");
    ptr=writebuffer(ptr, &ttmp, strlen(ttmp), &len, bufferlen);
    itmp = wav_le_int(0x7fffffff);
    ptr=writebuffer(ptr, &itmp, 4, &len, bufferlen);
    return len;
}
