
#ifndef __AF_INTERNAL_H_
#define __AF_INTERNAL_H_

typedef struct af_priv_s {
    int inited;

    int rate;
    int nch;
    int format;
    int bps;
    af_data_t *data;
    af_data_t* (*play)(struct af_priv_s* af, af_data_t* data);
    void *setup;

    double delay;
    double mul;
} af_priv_t;

#define RESIZE_LOCAL_BUFFER(a,d)\
((a->data->len < af_lencalc(a->mul,d))?af_resize_local_buffer(a,d):AF_OK)

#ifndef min
#define min(a,b)(((a)>(b))?(b):(a))
#endif

#ifndef max
#define max(a,b)(((a)>(b))?(a):(b))
#endif

#ifndef clamp
#define clamp(a,min,max) (((a)>(max))?(max):(((a)<(min))?(min):(a)))
#endif

int af_lencalc(double mul, af_data_t* d);
int af_resize_local_buffer(af_priv_t* af, af_data_t* data);

#endif
