
#ifndef __AF_H_
#define __AF_H_

#define AF_DETACH   2
#define AF_OK       1
#define AF_TRUE     1
#define AF_FALSE    0
#define AF_UNKNOWN -1
#define AF_ERROR   -2
#define AF_FATAL   -3

#define AF_NCH      6

// Audio data chunk
typedef struct af_data_s
{
  void* audio;  // data buffer
  int len;      // buffer length
  int rate;     // sample rate
  int nch;      // number of channels
  int format;   // format
  int bps;      // bytes per sample
} af_data_t;


void *          af_init(int rate, int nch, int format, int bps, float dB);
void            af_volume(void* priv, float dB);
void            af_volume_level(void* priv, int level);

double          af_buffer_time(void *priv);
af_data_t*      af_play(void *priv, af_data_t *af_data);

af_data_t*      af_data_mixer(af_data_t *data, unsigned int offset, unsigned int len, af_data_t *newdata);

af_data_t*      af_copy(af_data_t *indata);
af_data_t*      af_ncopy(af_data_t *indata, int len);
af_data_t*      af_empty(int rate, int nch, int format, int bps, int len);
af_data_t*      af_emptyfromdata(af_data_t *indata, int len);
void            af_drop_data(af_data_t *indata, int len);

double          af_data2time(af_data_t *indata);
double          af_len2time(af_data_t *indata, int len);
int             af_time2len(af_data_t *indata, double ts);
int             af_round_len(af_data_t *indata, int len);
int             af_fix_len(af_data_t *indata);

af_data_t*      af_data_free(af_data_t* af_data);

void            af_uninit(void *priv);
#endif
