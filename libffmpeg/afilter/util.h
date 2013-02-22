
#ifndef __UTIL_H_
#define __UTIL_H_

#include <stdint.h>
#include "af.h"

int64_t         ff_gcd(int64_t a, int64_t b);
void            af_fix_parameters(af_data_t *data);
int             format_2_bps(int format);
float           af_softclip(float a);
int             af_from_dB(int n, float* in, float* out, float k, float mi, float ma);

#endif
