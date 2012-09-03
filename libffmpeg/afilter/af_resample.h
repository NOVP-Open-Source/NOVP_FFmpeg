
#ifndef __AF_RESAMPLE_H_
#define __AF_RESAMPLE_H_

af_priv_t*      af_open_resample(int rate, int nch, int format, int bps);
int             af_init_resample(af_priv_t* af,af_data_t *data);
void            af_uninit_resample(af_priv_t* af);

#endif
