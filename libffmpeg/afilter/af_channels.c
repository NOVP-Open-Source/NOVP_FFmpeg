/*
 * Audio filter that adds and removes channels, according to the
 * command line parameter channels. It is stupid and can only add
 * silence or copy channels, not mix or filter.
 *
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


#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "config.h"
#include "af.h"
#include "afilter/af_internal.h"
#include "afilter/af_channels.h"
#include "libavutil/log.h"

#define FR 0
#define TO 1

typedef struct af_channels_s {
    int route[AF_NCH][2];
    int nr;
    int router;
} af_channels_t;

// Local function for copying data
static void af_copy_ch(void* in, void* out, int ins, int inos,int outs, int outos, int len, int bps) {
    switch(bps) {
    case 1:{
        int8_t* tin  = (int8_t*)in;
        int8_t* tout = (int8_t*)out;
        tin  += inos;
        tout += outos;
        len = len/ins;
        while(len--){
            *tout=*tin;
            tin +=ins;
            tout+=outs;
        }
        break;
    }
    case 2:{
        int16_t* tin  = (int16_t*)in;
        int16_t* tout = (int16_t*)out;
        tin  += inos;
        tout += outos;
        len = len/(2*ins);
        while(len--){
            *tout=*tin;
            tin +=ins;
            tout+=outs;
        }
        break;
    }
    case 3:{
        int8_t* tin  = (int8_t*)in;
        int8_t* tout = (int8_t*)out;
        tin  += 3 * inos;
        tout += 3 * outos;
        len = len / ( 3 * ins);
        while (len--) {
            tout[0] = tin[0];
            tout[1] = tin[1];
            tout[2] = tin[2];
            tin += 3 * ins;
            tout += 3 * outs;
        }
        break;
    }
    case 4:{
        int32_t* tin  = (int32_t*)in;
        int32_t* tout = (int32_t*)out;
        tin  += inos;
        tout += outos;
        len = len/(4*ins);
        while(len--){
            *tout=*tin;
            tin +=ins;
            tout+=outs;
        }
        break;
    }
    case 8:{
        int64_t* tin  = (int64_t*)in;
        int64_t* tout = (int64_t*)out;
        tin  += inos;
        tout += outos;
        len = len/(8*ins);
        while(len--){
            *tout=*tin;
            tin +=ins;
            tout+=outs;
        }
        break;
    }
    default:
        av_log(NULL, AV_LOG_ERROR,"[channels] Unsupported number of bytes/sample: %i" 
                       " please report this error on the MPlayer mailing list.\n",bps);
    }
}

// Make sure the routes are sane
static int check_routes(af_channels_t* s, int nin, int nout) {
    int i;

    if((s->nr < 1) || (s->nr > AF_NCH)){
        av_log(NULL, AV_LOG_ERROR,"[channels] The number of routing pairs must be" 
                        " between 1 and %i. Current value is %i\n",AF_NCH,s->nr);
        return AF_ERROR;
    }
    for(i=0;i<s->nr;i++){
        if((s->route[i][FR] >= nin) || (s->route[i][TO] >= nout)){
            av_log(NULL, AV_LOG_ERROR,"[channels] Invalid routing in pair nr. %i.\n", i);
            return AF_ERROR;
        }
    }
    return AF_OK;
}


int af_init_channels(af_priv_t* af,af_data_t *data) {
    af_channels_t* s = af->setup;
    int i;

    s->nr=af->data->nch=af->nch;
    // Set default channel assignment
    if(!s->router){
        // Make sure this filter isn't redundant 
        if(af->data->nch == data->nch)
            return AF_DETACH;
          // If mono: fake stereo
          if(data->nch == 1){
                s->nr = min(af->data->nch,2);
                for(i=0;i<s->nr;i++){
                    s->route[i][FR] = 0;
                    s->route[i][TO] = i;
                }
          } else {
            s->nr = min(af->data->nch, data->nch);
            for(i=0;i<s->nr;i++){
                s->route[i][FR] = i;
                s->route[i][TO] = i;
            }
        }
    }
    af->data->rate   = data->rate;
    af->data->format = data->format;
    af->data->bps    = data->bps;
    af->mul          = (double)af->data->nch / data->nch;
    return check_routes(s,data->nch,af->data->nch);

}

af_data_t* play(af_priv_t* af,af_data_t *data) {
    af_data_t*          c = data;                       // Current working data
    af_data_t*          l = af->data;                   // Local data
    af_channels_t*      s = af->setup;
    int                 i;

    if(AF_OK != RESIZE_LOCAL_BUFFER(af,data)) {
        return NULL;
    }

    // Reset unused channels
    memset(l->audio,0,c->len / c->nch * l->nch);
    if(AF_OK == check_routes(s,c->nch,l->nch)) {
        for(i=0;i<s->nr;i++) {
            af_copy_ch(c->audio,l->audio,c->nch,s->route[i][FR],
                       l->nch,s->route[i][TO],c->len,c->bps);
        }
    }
    // Set output data
    c->audio = l->audio;
    c->len   = c->len / c->nch * l->nch;
    c->nch   = l->nch;
    return c;
}

af_priv_t* af_open_channels(int rate, int nch, int format, int bps) {
    af_priv_t* af = av_malloc(sizeof(af_priv_t));
    memset(af,0,sizeof(af_priv_t));

    af->nch=nch;
    af->play=play;
    af->mul=1;
    af->data=av_malloc(sizeof(af_data_t));
    memset(af->data,0,sizeof(af_data_t));
    af->setup=av_malloc(sizeof(af_channels_t));
    memset(af->setup,0,sizeof(af_channels_t));
    return af;
}


void af_uninit_channels(af_priv_t* af) {

    if(!af)
        return;
    if(af->data) {
        if(af->data->audio)
            av_free(af->data->audio);
        av_free(af->data);
    }
    if(af->setup)
        av_free(af->setup);
    av_free(af);
}
