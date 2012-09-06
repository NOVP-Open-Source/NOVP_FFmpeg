
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <sys/time.h>
#include <math.h>

#include "config.h"
#include "afilter/util.h"
#include "afilter/format.h"
#include "afilter/af_internal.h"
#include "af.h"

int64_t ff_gcd(int64_t a, int64_t b) {
    if(b)
        return ff_gcd(b, a%b);
    else
        return a;
}

void af_fix_parameters(af_data_t *data) {
    data->bps = af_fmt2bits(data->format)/8;
}

int format_2_bps(int format) {
    return af_fmt2bits(format)/8;
}

float af_softclip(float a)
{
    if (a >= M_PI/2)
        return 1.0;
    else if (a <= -M_PI/2)
        return -1.0;
    else
        return sin(a);
}

/* Convert to gain value from dB. Returns AF_OK if of and AF_ERROR if
   fail */
int af_from_dB(int n, float* in, float* out, float k, float mi, float ma) {
    int i = 0; 
    // Sanity check
    if(!in || !out)
        return 0;

    for(i=0;i<n;i++) {
        if(in[i]<=-200)
            out[i]=0.0;
        else
            out[i]=pow(10.0,clamp(in[i],mi,ma)/k);
    }
    return 1;
}
