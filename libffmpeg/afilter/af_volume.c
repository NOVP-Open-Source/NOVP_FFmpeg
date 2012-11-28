/*
 * Copyright (C)2002 Anders Johansson ajh@atri.curtin.edu.au
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

/* This audio filter changes the volume of the sound, and can be used
   when the mixer doesn't support the PCM channel. It can handle
   between 1 and 6 channels. The volume can be adjusted between -60dB
   to +20dB and is set on a per channels basis. The is accessed through
   AF_CONTROL_VOLUME_LEVEL.

   The filter has support for soft-clipping, it is enabled by
   AF_CONTROL_VOLUME_SOFTCLIPP. It has also a probing feature which
   can be used to measure the power in the audio stream, both an
   instantaneous value and the maximum value can be probed. The
   probing is enable by AF_CONTROL_VOLUME_PROBE_ON_OFF and is done on a
   per channel basis. The result from the probing is obtained using
   AF_CONTROL_VOLUME_PROBE_GET and AF_CONTROL_VOLUME_PROBE_GET_MAX. The
   probed values are calculated in dB. 
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h> 

#include <inttypes.h>
#include <math.h>
#include <limits.h>

#include "config.h"
#include "af.h"
#include "afilter/af_internal.h"
#include "afilter/af_volume.h"
#include "afilter/format.h"
#include "afilter/dsp.h"
#include "afilter/util.h"

// Data for specific instances of this filter
typedef struct af_volume_s {
  int   enable[AF_NCH];         // Enable/disable / channel
  float	pow[AF_NCH];            // Estimated power level [dB]
  float	max[AF_NCH];            // Max Power level [dB]
  float level[AF_NCH];          // Gain level for each channel
  float time;                   // Forgetting factor for power estimate
  int soft;                     // Enable/disable soft clipping
  int fast;                     // Use fix-point volume control
} af_volume_t;

void af_uninit_volume(af_priv_t* af) {
    if(!af)
        return;
    if(af->data) {
        if(af->data->audio)
            free(af->data->audio);
        free(af->data);
    }
    if(af->setup)
        free(af->setup);
    free(af);
}

// Filter data through filter
static af_data_t* play(af_priv_t* af, af_data_t* data) {
    af_data_t*          c   = data;                     // Current working data
    af_volume_t*        s   = (af_volume_t*)af->setup;  // Setup for this instance
    int                 ch  = 0;                        // Channel counter
    register int        nch = c->nch;                   // Number of channels
    register int        i   = 0;

     // Basic operation volume control only (used on slow machines)
    if(af->data->format == (AF_FORMAT_S16_NE)) {
        int16_t*        a   = (int16_t*)c->audio;       // Audio data
        int             len = c->len/2;                 // Number of samples
        for(ch = 0; ch < nch ; ch++) {
            if(s->enable[ch]) {
                register int vol = (int)(255.0 * s->level[ch]);
                for(i=ch;i<len;i+=nch) {
                    register int x = (a[i] * vol) >> 8;
                    a[i]=clamp(x,SHRT_MIN,SHRT_MAX);
                }
            }
        }
    // Machine is fast and data is floating point
    } else if(af->data->format == (AF_FORMAT_FLOAT_NE)) {
        float*          a       = (float*)c->audio;     // Audio data
        int             len     = c->len/4;             // Number of samples
        for(ch = 0; ch < nch ; ch++) {
            // Volume control (fader)
            if(s->enable[ch]) {
                float       t   = 1.0 - s->time;
                for(i=ch;i<len;i+=nch) {
                    register float x        = a[i];
                    register float pow      = x*x;
                    // Check maximum power value
                    if(pow > s->max[ch])
                        s->max[ch] = pow;
                    // Set volume
                    x *= s->level[ch];
                    // Peak meter
                    pow = x*x;
                    if(pow > s->pow[ch])
                        s->pow[ch] = pow;
                    else
                        s->pow[ch] = t*s->pow[ch] + pow*s->time; // LP filter
                    /* Soft clipping, the sound of a dream, thanks to Jon Wattes
                       post to Musicdsp.org */
                    if(s->soft)
                        x=af_softclip(x);
                        // Hard clipping
                    else
                        x=clamp(x,-1.0,1.0);
                    a[i] = x;
                }
            }
        }
    }
    return c;
}

int af_init_volume(af_priv_t* af,af_data_t *data) {

    if(data->format!=(AF_FORMAT_S16_NE) && data->format!=(AF_FORMAT_FLOAT_NE))
        return AF_ERROR;
    af->data->rate   = data->rate;
    af->data->format = data->format;
    af->data->nch    = data->nch;
    af->data->bps    = data->bps;
    return AF_OK;
}

af_priv_t* af_open_volume(int rate, int nch, int format, int bps, float dB){
    af_priv_t* af = calloc(1,sizeof(af_priv_t));
    af_volume_t* s = (af_volume_t*)calloc(1,sizeof(af_volume_t));
    float chdB[AF_NCH];
    int i = 0;

    af->nch=nch;
    af->play=play;
    af->mul=1;
    af->data=calloc(1,sizeof(af_data_t));
    af->setup=s;
    for(i=0;i<AF_NCH;i++) {
        chdB[i]=dB;
        ((af_volume_t*)af->setup)->enable[i] = 1;
        ((af_volume_t*)af->setup)->level[i]  = 1.0;
    }
    if(dB==0.0)
        return af;
    af_from_dB(AF_NCH,chdB,s->level,20.0,-200.0,60.0);
    return af;
}

void af_set_volume(af_priv_t* af, float dB) {
    float chdB[AF_NCH];
    int i = 0;
    af_volume_t* s = af->setup;

    for(i=0;i<AF_NCH;i++) {
        chdB[i]=dB;
        s->enable[i] = 1;
        s->level[i]  = 1.0;
    }
    if(dB==0.0)
        return;
    af_from_dB(AF_NCH,chdB,s->level,20.0,-200.0,60.0);
}

void af_set_volume_level(af_priv_t* af, int vol) {
    float soft_vol_max = 100.0;
    float soft_vol = vol;

    soft_vol = ((soft_vol) / 100.0) * (soft_vol_max / 100.0);
    if(soft_vol == 0.0)
        soft_vol = -200.0;
    else
        soft_vol = 20.0*log10(soft_vol);
    af_set_volume(af, soft_vol);
}
