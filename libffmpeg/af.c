
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include "config.h"
#include "af.h"
#include "afilter/af_internal.h"
#include "afilter/af_format.h"
#include "afilter/af_resample.h"
#include "afilter/af_channels.h"
#include "afilter/af_volume.h"
#include "afilter/af_volnorm.h"
#include "afilter/af_import.h"
#include "afilter/util.h"
#include "afilter/format.h"

#include "libavutil/log.h"

typedef struct af_filters_s {
    af_priv_t *af_format;
    af_priv_t *af_channels;
    af_priv_t *af_resample;
    af_priv_t *af_volume;
    af_priv_t *af_volnorm;
    af_data_t *data;
    af_data_t *wdata;
    af_data_t *drop;
    unsigned char * audio;
    int size;
    unsigned char * inaudio;
    int inlen;
} af_filters_t;


void *af_init(int rate, int nch, int format, int bps, float dB) {
    af_filters_t* af_filters = av_malloc(sizeof(af_filters_t));
    memset(af_filters,0,sizeof(af_filters_t));

    bps = af_fmt2bits(format)/8;
    af_filters->af_format = af_open_format(rate,nch,format,bps);
    af_filters->af_channels = af_open_channels(rate,nch,format,bps);
    af_filters->af_resample = af_open_resample(rate,nch,format,bps);
    af_filters->af_volume = af_open_volume(rate,nch,format,bps,dB);
    af_filters->af_volnorm = af_open_volnorm(rate,nch,format,bps,0,0,0.0);
    af_filters->data=av_malloc(sizeof(af_data_t));
    memset(af_filters->data,0,sizeof(af_data_t));
    af_filters->data->format=format;
    af_filters->data->rate=rate;
    af_filters->data->nch=nch;
    af_filters->data->bps=bps;
    af_filters->wdata = av_malloc(sizeof(af_data_t));
    memset(af_filters->wdata,0,sizeof(af_data_t));
    af_filters->audio=NULL;
    af_filters->drop=NULL;
    af_filters->size=0;
    af_filters->inaudio=NULL;
    af_filters->inlen=0;
    return af_filters;
}

void af_volume(void* priv, float dB) {
    af_filters_t* af_filters = (af_filters_t*)priv;

    if(af_filters->af_volume)
        af_set_volume(af_filters->af_volume, dB);
}

void af_volume_level(void* priv, int level) {
    af_filters_t* af_filters = (af_filters_t*)priv;

    if(af_filters->af_volume)
        af_set_volume_level(af_filters->af_volume, level);
}

double af_buffer_time(void *priv) {
    af_filters_t* af_filters = (af_filters_t*)priv;

    if(!af_filters->drop)
        return 0.0;
    return af_data2time(af_filters->drop);
}

af_data_t* af_play(void *priv, af_data_t *indata) {
    af_filters_t* af_filters = (af_filters_t*)priv;
    af_data_t* data;
    int len, inlen;
    unsigned char * inaudio;
    int div,samples,dropframes=0,ptr,dropbytes;

    if(!indata)
        return NULL;
    if(!indata->rate || !indata->nch) {
        av_log(NULL, AV_LOG_ERROR,"Wrong input audio rate in filter. Skip.\n");
        return indata;
    }
    af_fix_parameters(indata);
    if((len=af_fix_len(indata)))
        av_log(NULL, AV_LOG_ERROR,"Wrong af input data: %d [%d]\n",len,indata->len);
    af_filters->wdata->audio=inaudio=indata->audio;
    af_filters->wdata->len=inlen=indata->len;
    af_filters->wdata->format=indata->format;
    af_filters->wdata->rate=indata->rate;
    af_filters->wdata->nch=indata->nch;
    af_filters->wdata->bps=indata->bps;

    if(af_filters->inaudio)
        av_free(af_filters->inaudio);
    af_filters->inaudio=NULL;
    if(af_filters->drop) {
        inlen+=af_filters->drop->len;
        af_filters->inaudio=inaudio=av_malloc(inlen);
        memcpy(af_filters->inaudio,af_filters->drop->audio,af_filters->drop->len);
        memcpy(af_filters->inaudio+af_filters->drop->len,indata->audio,indata->len);
        av_free(af_filters->drop->audio);
        av_free(af_filters->drop);
        af_filters->drop=NULL;
        af_filters->wdata->audio=af_filters->inaudio;
    }
    div=indata->rate/af_filters->data->rate;
    samples=inlen/indata->nch/indata->bps;
    if(div)
        dropframes=samples % div;
    dropbytes=dropframes*indata->nch*indata->bps;
    if(dropframes) {
        ptr=inlen-dropbytes;
        af_filters->drop=af_empty(indata->rate, indata->nch, indata->format, indata->bps, dropbytes);
        memcpy(af_filters->drop->audio,af_filters->wdata->audio+ptr,dropbytes);
    }
    af_filters->wdata->len=inlen-dropbytes;

    data=af_filters->wdata;

    // resample
    if(af_filters->af_resample && af_filters->af_resample->inited!=AF_OK)
        af_filters->af_resample->inited=af_init_resample(af_filters->af_resample,data);
    if(af_filters->af_resample && af_filters->af_resample->inited==AF_OK)
        data=af_filters->af_resample->play(af_filters->af_resample,data);
    if(!data)
        return NULL;

    // format
    if(af_filters->af_format && af_filters->af_format->inited!=AF_OK)
        af_filters->af_format->inited=af_init_format(af_filters->af_format,data);
    if(af_filters->af_format && af_filters->af_format->inited==AF_OK)
        data=af_filters->af_format->play(af_filters->af_format,data);
    if(!data)
        return NULL;

    // channels
    if(af_filters->af_channels && af_filters->af_channels->inited!=AF_OK)
        af_filters->af_channels->inited=af_init_channels(af_filters->af_channels,data);
    if(af_filters->af_channels && af_filters->af_channels->inited==AF_OK)
        data=af_filters->af_channels->play(af_filters->af_channels,data);
    if(!data)
        return NULL;

    // volume
    if(af_filters->af_volume && af_filters->af_volume->inited!=AF_OK)
        af_filters->af_volume->inited=af_init_volume(af_filters->af_volume,data);
    if(af_filters->af_volume && af_filters->af_volume->inited==AF_OK)
        data=af_filters->af_volume->play(af_filters->af_volume,data);
    if(!data)
        return NULL;

    // volnorm
    if(af_filters->af_volnorm && af_filters->af_volnorm->inited!=AF_OK)
        af_filters->af_volnorm->inited=af_init_volnorm(af_filters->af_volnorm,data);
    if(af_filters->af_volnorm && af_filters->af_volnorm->inited==AF_OK)
        data=af_filters->af_volnorm->play(af_filters->af_volnorm,data);
    if(!data)
        return NULL;

    af_fix_parameters(data);
    if(data->audio==indata->audio && data->audio!=af_filters->audio) {
        if(af_filters->size<data->len) {
            if(af_filters->audio)
                av_free(af_filters->audio);
            af_filters->audio=av_malloc(data->len);
            af_filters->size=data->len;
        }
        memcpy(af_filters->audio,data->audio,data->len);
        data->audio=af_filters->audio;
    }
    return data;
}

void af_uninit(void *priv) {
    af_filters_t* af_filters = (af_filters_t*)priv;

    if(!priv)
        return;
    af_uninit_volnorm(af_filters->af_volnorm);
    af_uninit_volume(af_filters->af_volume);
    af_uninit_format(af_filters->af_format);
    af_uninit_resample(af_filters->af_resample);
    af_uninit_channels(af_filters->af_channels);
    if(af_filters->audio)
        av_free(af_filters->audio);
    av_free(af_filters->data);
    av_free(af_filters->wdata);
    if(af_filters->drop) {
        if(af_filters->drop->audio)
            av_free(af_filters->drop->audio);
        av_free(af_filters->drop);
    }
    if(af_filters->inaudio)
        av_free(af_filters->inaudio);
    av_free(af_filters);
}

int af_lencalc(double mul, af_data_t* d) {
    int t = d->bps * d->nch;

    return d->len * mul + t + 1;
}

int af_resize_local_buffer(af_priv_t* af, af_data_t* data) {
    // Calculate new length
    register int len = af_lencalc(af->mul,data);

    // If there is a buffer free it
    if(af->data->audio) 
        av_free(af->data->audio);
    // Create new buffer and check that it is OK
    af->data->audio = av_malloc(len);
    if(!af->data->audio) {
        av_log(NULL, AV_LOG_FATAL,"[libaf] Could not allocate memory \n");
        return AF_ERROR;
    }
    af->data->len=len;
    return AF_OK;
}

af_data_t* af_data_mixer(af_data_t *data, unsigned int offset, unsigned int len, af_data_t *newdata) {
    return af_import_play(data,offset,len,newdata);
}

af_data_t* af_copy(af_data_t *indata) {
    af_data_t* data;

    if(!indata)
        return NULL;
    data=av_malloc(sizeof(af_data_t));
    memset(data,0,sizeof(af_data_t));
    data->rate=indata->rate;
    data->nch=indata->nch;
    data->format=indata->format;
    data->bps=indata->bps;
    data->len=indata->len;
    if(indata->len) {
        data->audio=av_malloc(indata->len);
        memcpy(data->audio,indata->audio,indata->len);
    } else {
        data->audio=NULL;
    }
    return data;
}

af_data_t* af_ncopy(af_data_t *indata, int len) {
    af_data_t* data;

    if(!indata)
        return NULL;
    if(len>indata->len)
        len=indata->len;
    data=av_malloc(sizeof(af_data_t));
    memset(data,0,sizeof(af_data_t));
    data->rate=indata->rate;
    data->nch=indata->nch;
    data->format=indata->format;
    data->bps=indata->bps;
    data->len=len;
    if(len) {
        data->audio=av_malloc(len);
        memcpy(data->audio,indata->audio,len);
    } else {
        data->audio=NULL;
    }
    return data;
}

af_data_t* af_empty(int rate, int nch, int format, int bps, int len) {
    af_data_t* data;

    data=av_malloc(sizeof(af_data_t));
    memset(data,0,sizeof(af_data_t));
    data->rate=rate;
    data->nch=nch;
    data->format=format;
    data->bps=bps;
    data->len=len;
    if(len) {
        data->audio=av_malloc(len);
        memset(data->audio,0,len);
    } else
        data->audio=NULL;
    af_fix_parameters(data);
    return data;
}

af_data_t* af_emptyfromdata(af_data_t *indata, int len) {
    af_data_t* data;

    data=av_malloc(sizeof(af_data_t));
    memset(data,0,sizeof(af_data_t));
    data->rate=indata->rate;
    data->nch=indata->nch;
    data->format=indata->format;
    data->bps=indata->bps;
    data->len=len;
    if(len) {
        data->audio=av_malloc(len);
        memset(data->audio,0,len);
    } else
        data->audio=NULL;
    af_fix_parameters(data);
    return data;
}

double af_data2time(af_data_t *indata) {
    double ret,l;

    ret=indata->len;
    l=indata->rate*indata->nch*indata->bps;
    ret/=l;
    return ret;
}

double af_len2time(af_data_t *indata, int len) {
    double ret,l;

    ret=len;
    l=indata->rate*indata->nch*indata->bps;
    ret/=l;
    return ret;
}


int af_time2len(af_data_t *indata, double ts) {
    double ret;
    int l;

    l=ts*indata->rate;
    ret=l*indata->nch*indata->bps;
    return ret;
}

int af_round_len(af_data_t *indata, int len) {
    int l = indata->nch*indata->bps;

    len/=l;
    len*=l;
    return len;
}

int af_fix_len(af_data_t *indata) {
    int l, len, ret;

    af_fix_parameters(indata);
    l = indata->nch*indata->bps;
    ret = len = indata->len;
    len/=l;
    len*=l;
    if(len==indata->len)
        return 0;
    indata->len=len;
    return ret;
}

af_data_t*  af_data_free(af_data_t* af_data) {
    if(af_data) {
        if(af_data->audio)
            av_free(af_data->audio);
        av_free(af_data);
    }
    return NULL;
}

void af_drop_data(af_data_t *indata, int len) {
    int newlen;
    char* tmp;
    if(!indata || !indata->audio)
        return;
    if(len>=indata->len) {
        av_free(indata->audio);
        indata->audio=0;
        indata->len=0;
        return;
    }
    newlen=indata->len-len;
    tmp=av_malloc(newlen);
    memcpy(tmp,indata->audio+len,newlen);
    av_free(indata->audio);
    indata->audio=tmp;
    indata->len=newlen;
}
