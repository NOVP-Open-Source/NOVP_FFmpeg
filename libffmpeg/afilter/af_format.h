
#ifndef __AF_FORMAT_H_
#define __AF_FORMAT_H_

af_priv_t*      af_open_format(int rate, int nch, int format, int bps);
int             af_init_format(af_priv_t* af,af_data_t *data);
void            af_uninit_format(af_priv_t* af);

#endif
