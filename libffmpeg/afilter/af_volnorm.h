
#ifndef __AF_VOLNORM_H_
#define __AF_VOLNORM_H_

af_priv_t*      af_open_volnorm(int rate, int nch, int format, int bps, int upper, int method, float target);
int             af_init_volnorm(af_priv_t* af,af_data_t *data);
void            af_uninit_volnorm(af_priv_t* af);


#endif
