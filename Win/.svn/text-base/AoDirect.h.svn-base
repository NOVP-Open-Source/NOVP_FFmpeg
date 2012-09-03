
#include <stdlib.h>
#include <malloc.h>
#include <stdint.h>
#include <windows.h>
#include <windowsx.h>
#include <WTypes.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AOPLAY_FINAL_CHUNK 1 

typedef struct ao_data_s
{
  int samplerate;
  int channels;
  int format;
  int bps;
  int outburst;
  int buffersize;
  int pts;
} ao_data_t;

typedef struct aoconf_s {
	ao_data_t	ao_data;
	int			inited;
	char*		msg;
} aoconf_t;

char* agetmsg(aoconf_t* aoconf);


aoconf_t* audio_init(int rate, int channels, int format, int flags);
int audio_inited(aoconf_t* aoconf);
void audio_reset(aoconf_t* aoconf);
void audio_pause(aoconf_t* aoconf);
void audio_uninit(aoconf_t* aoconf, int immed);
int audio_get_space(aoconf_t* aoconf);
int audio_play(aoconf_t* aoconf, void* data, int len, int flags);
float audio_get_delay(aoconf_t* aoconf);






#ifdef __cplusplus
};
#endif
