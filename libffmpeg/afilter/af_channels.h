
#ifndef __AF_CHANNELS_H_
#define __AF_CHANNELS_H_

af_priv_t*      af_open_channels(int rate, int nch, int format, int bps);
int             af_init_channels(af_priv_t* af,af_data_t *data);
af_data_t*      af_channels(af_priv_t* af,af_data_t *data);
void            af_uninit_channels(af_priv_t* af);


#endif
