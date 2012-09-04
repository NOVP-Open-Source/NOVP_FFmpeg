/*
 *  Import audio from shmem stream.
 *
 *  Based on af_pan.c and af_format.c
 *
 *  Copyright (C) 2007 Attila Ötvös <attila@onebithq.com>
 *
 *  MPlayer is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  MPlayer is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with MPlayer; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */
#include <stdio.h>
#include <stdlib.h>

#include <inttypes.h>
#include <math.h>
#include <limits.h>
#include <string.h>

#include "afilter/format.h"
#include "config.h"
#include "af.h"
#include "afilter/af_internal.h"
#include "afilter/af_import.h"

// Filter data through filter
af_data_t* af_import_play(af_data_t* data, unsigned int offset, unsigned int len, af_data_t* cdata) {
    af_data_t*    c0   = data;                                  // Current working data
    af_data_t*    c1   = cdata;                                 // Import data
    float*        inf0 = c0->audio+offset;                      // Input audio data (float)
    int16_t*      ini0 = c0->audio+offset;                      // Input audio data (int)
    float*        inf1 = c1->audio;                             // Import audio data (float)
    int16_t*      ini1 = c1->audio;                             // Import audio data (int)
    float*        endf0 = inf0+(c0->len-offset)/sizeof(float);  // End of loop (int)
    int16_t*      endi0 = ini0+(c0->len-offset)/sizeof(int16_t);// End of loop (float)
    float*        endf1 = inf1+c1->len/sizeof(float);           // End of loop (float)
    int16_t*      endi1 = ini1+c1->len/sizeof(int16_t);         // End of loop (int)
    register int  j, l=0;

    if(len && c1->len>len) {
        endf1 = inf1+len/sizeof(float);
        endi1 = ini1+len/sizeof(int16_t);
    }
    if(c0->format==AF_FORMAT_S16_NE) {
        while(ini0 < endi0) {
            if(ini1>=endi1) break;
            for(j=0;j<min(c0->nch,c1->nch);j++) {
                register float  x   = 0.0;
                x = ini0[j] + ini1[j];
                ini0[j] = x;
            }
        ini0+= c0->nch;
        ini1+= c1->nch;
        l++;
        }
    }
    if(c0->format==AF_FORMAT_FLOAT_NE) {
        while(inf0 < endf0) {
            if(inf1>=endf1) break;
            for(j=0;j<min(c0->nch,c1->nch);j++) {
            register float  x   = 0.0;
            x = inf0[j] + inf1[j];
            inf0[j] = x;
        }
        inf0+= c0->nch;
        inf1+= c1->nch;
        l++;
        }
    }
    return c0;
}
