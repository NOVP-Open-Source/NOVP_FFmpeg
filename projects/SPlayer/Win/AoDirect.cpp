
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#define DIRECTSOUND_VERSION 0x0600
#include <dsound.h>
#include <math.h>

#include "AoDirect.h"

//#include "libaf/af_format.h"
//#include "audio_out.h"
//#include "audio_out_internal.h"
//#include "mp_msg.h"
//#include "libvo/fastmemcpy.h"
//#include "osdep/timer.h"
//#include "subopt-helper.h"

#define fast_memcpy(a,b,c) memcpy(a,b,c)

#ifdef __cplusplus
extern "C" {
#endif

#include "af_format.h"

/**
\todo use the definitions from the win32 api headers when they define these
*/
#define WAVE_FORMAT_IEEE_FLOAT 0x0003
#define WAVE_FORMAT_DOLBY_AC3_SPDIF 0x0092
#define WAVE_FORMAT_EXTENSIBLE 0xFFFE

static const GUID KSDATAFORMAT_SUBTYPE_PCM = {0x1,0x0000,0x0010, {0x80,0x00,0x00,0xaa,0x00,0x38,0x9b,0x71}};

#define SPEAKER_FRONT_LEFT             0x1
#define SPEAKER_FRONT_RIGHT            0x2
#define SPEAKER_FRONT_CENTER           0x4
#define SPEAKER_LOW_FREQUENCY          0x8
#define SPEAKER_BACK_LEFT              0x10
#define SPEAKER_BACK_RIGHT             0x20
#define SPEAKER_FRONT_LEFT_OF_CENTER   0x40
#define SPEAKER_FRONT_RIGHT_OF_CENTER  0x80
#define SPEAKER_BACK_CENTER            0x100
#define SPEAKER_SIDE_LEFT              0x200
#define SPEAKER_SIDE_RIGHT             0x400
#define SPEAKER_TOP_CENTER             0x800
#define SPEAKER_TOP_FRONT_LEFT         0x1000
#define SPEAKER_TOP_FRONT_CENTER       0x2000
#define SPEAKER_TOP_FRONT_RIGHT        0x4000
#define SPEAKER_TOP_BACK_LEFT          0x8000
#define SPEAKER_TOP_BACK_CENTER        0x10000
#define SPEAKER_TOP_BACK_RIGHT         0x20000
#define SPEAKER_RESERVED               0x80000000

#define DSSPEAKER_HEADPHONE         0x00000001
#define DSSPEAKER_MONO              0x00000002
#define DSSPEAKER_QUAD              0x00000003
#define DSSPEAKER_STEREO            0x00000004
#define DSSPEAKER_SURROUND          0x00000005
#define DSSPEAKER_5POINT1           0x00000006

#ifndef _WAVEFORMATEXTENSIBLE_
typedef struct {
    WAVEFORMATEX    Format;
    union {
        WORD wValidBitsPerSample;       /* bits of precision  */
        WORD wSamplesPerBlock;          /* valid if wBitsPerSample==0 */
        WORD wReserved;                 /* If neither applies, set to zero. */
    } Samples;
    DWORD           dwChannelMask;      /* which channels are */
                                        /* present in stream  */
    GUID            SubFormat;
} WAVEFORMATEXTENSIBLE, *PWAVEFORMATEXTENSIBLE;
#endif

static const int channel_mask[] = {
  SPEAKER_FRONT_LEFT   | SPEAKER_FRONT_RIGHT  | SPEAKER_LOW_FREQUENCY,
  SPEAKER_FRONT_LEFT   | SPEAKER_FRONT_RIGHT  | SPEAKER_BACK_LEFT    | SPEAKER_BACK_RIGHT,
  SPEAKER_FRONT_LEFT   | SPEAKER_FRONT_RIGHT  | SPEAKER_BACK_LEFT    | SPEAKER_BACK_RIGHT   | SPEAKER_LOW_FREQUENCY,
  SPEAKER_FRONT_LEFT   | SPEAKER_FRONT_CENTER | SPEAKER_FRONT_RIGHT  | SPEAKER_BACK_LEFT    | SPEAKER_BACK_RIGHT     | SPEAKER_LOW_FREQUENCY
};

static HINSTANCE hdsound_dll = NULL;      ///handle to the dll
static LPDIRECTSOUND hds = NULL;          ///direct sound object
static LPDIRECTSOUNDBUFFER hdspribuf = NULL; ///primary direct sound buffer
static LPDIRECTSOUNDBUFFER hdsbuf = NULL; ///secondary direct sound buffer (stream buffer)
static int buffer_size = 0;               ///size in bytes of the direct sound buffer
static int write_offset = 0;              ///offset of the write cursor in the direct sound buffer
static int min_free_space = 0;            ///if the free space is below this value get_space() will return 0
                                          ///there will always be at least this amout of free space to prevent
                                          ///get_space() from returning wrong values when buffer is 100% full.
                                          ///will be replaced with nBlockAlign in init()
static int device_num = 0;                ///wanted device number
static GUID device;                       ///guid of the device

/***************************************************************************************/
static int loc_af_fmt2bits(int format) {
    if (AF_FORMAT_IS_AC3(format)) return 16;
    return (format & AF_FORMAT_BITS_MASK)+8;
//    return (((format & AF_FORMAT_BITS_MASK)>>3)+1) * 8;
#if 0
    switch(format & AF_FORMAT_BITS_MASK)
    {
	case AF_FORMAT_8BIT: return 8;
	case AF_FORMAT_16BIT: return 16;
	case AF_FORMAT_24BIT: return 24;
	case AF_FORMAT_32BIT: return 32;
	case AF_FORMAT_48BIT: return 48;
    }
#endif
    return -1;
}

static struct {
    const char *name;
    const int format;
} af_fmtstr_table[] = {
    { "mulaw", AF_FORMAT_MU_LAW },
    { "alaw", AF_FORMAT_A_LAW },
    { "mpeg2", AF_FORMAT_MPEG2 },
    { "ac3le", AF_FORMAT_AC3_LE },
    { "ac3be", AF_FORMAT_AC3_BE },
    { "ac3ne", AF_FORMAT_AC3_NE },
    { "imaadpcm", AF_FORMAT_IMA_ADPCM },

    { "u8", AF_FORMAT_U8 },
    { "s8", AF_FORMAT_S8 },
    { "u16le", AF_FORMAT_U16_LE },
    { "u16be", AF_FORMAT_U16_BE },
    { "u16ne", AF_FORMAT_U16_NE },
    { "s16le", AF_FORMAT_S16_LE },
    { "s16be", AF_FORMAT_S16_BE },
    { "s16ne", AF_FORMAT_S16_NE },
    { "u24le", AF_FORMAT_U24_LE },
    { "u24be", AF_FORMAT_U24_BE },
    { "u24ne", AF_FORMAT_U24_NE },
    { "s24le", AF_FORMAT_S24_LE },
    { "s24be", AF_FORMAT_S24_BE },
    { "s24ne", AF_FORMAT_S24_NE },
    { "u32le", AF_FORMAT_U32_LE },
    { "u32be", AF_FORMAT_U32_BE },
    { "u32ne", AF_FORMAT_U32_NE },
    { "s32le", AF_FORMAT_S32_LE },
    { "s32be", AF_FORMAT_S32_BE },
    { "s32ne", AF_FORMAT_S32_NE },
    { "floatle", AF_FORMAT_FLOAT_LE },
    { "floatbe", AF_FORMAT_FLOAT_BE },
    { "floatne", AF_FORMAT_FLOAT_NE },

    { NULL, 0 }
};

static const char *loc_af_fmt2str_short(int format)
{
    int i;

    for (i = 0; af_fmtstr_table[i].name; i++)
	if (af_fmtstr_table[i].format == format)
	    return af_fmtstr_table[i].name;

    return "??";
}
/***************************************************************************************/

static void printmsg(aoconf_t* aoconf, char* format_str, ...) {
	va_list ap;
	char* tmp;
	char* tmp2;

	tmp=(char*)malloc(8192);
	va_start(ap, format_str);
	vsnprintf(tmp,8192,format_str,ap);
	va_end(ap);
	if(!aoconf->msg) {
		aoconf->msg=strdup(tmp);
	} else {
		tmp2=(char*)malloc(strlen(aoconf->msg)+strlen(tmp)+4);
		sprintf(tmp2,"%s%s",aoconf->msg,tmp);
		free(aoconf->msg);
		aoconf->msg=tmp2;
	}
	free(tmp);
}

char* agetmsg(aoconf_t* aoconf) {
	char* ret;
	if(aoconf->msg) {
		ret=strdup(aoconf->msg);
		free(aoconf->msg);
		aoconf->msg=NULL;
		return ret;
	}
	return strdup("(none)");
}

/**
\brief output error message
\param err error code
\return string with the error message
*/
static char * dserr2str(int err)
{
	switch (err) {
		case DS_OK: return "DS_OK";
		case DS_NO_VIRTUALIZATION: return "DS_NO_VIRTUALIZATION";
		case DSERR_ALLOCATED: return "DS_NO_VIRTUALIZATION";
		case DSERR_CONTROLUNAVAIL: return "DSERR_CONTROLUNAVAIL";
		case DSERR_INVALIDPARAM: return "DSERR_INVALIDPARAM";
		case DSERR_INVALIDCALL: return "DSERR_INVALIDCALL";
		case DSERR_GENERIC: return "DSERR_GENERIC";
		case DSERR_PRIOLEVELNEEDED: return "DSERR_PRIOLEVELNEEDED";
		case DSERR_OUTOFMEMORY: return "DSERR_OUTOFMEMORY";
		case DSERR_BADFORMAT: return "DSERR_BADFORMAT";
		case DSERR_UNSUPPORTED: return "DSERR_UNSUPPORTED";
		case DSERR_NODRIVER: return "DSERR_NODRIVER";
		case DSERR_ALREADYINITIALIZED: return "DSERR_ALREADYINITIALIZED";
		case DSERR_NOAGGREGATION: return "DSERR_NOAGGREGATION";
		case DSERR_BUFFERLOST: return "DSERR_BUFFERLOST";
		case DSERR_OTHERAPPHASPRIO: return "DSERR_OTHERAPPHASPRIO";
		case DSERR_UNINITIALIZED: return "DSERR_UNINITIALIZED";
		case DSERR_NOINTERFACE: return "DSERR_NOINTERFACE";
		case DSERR_ACCESSDENIED: return "DSERR_ACCESSDENIED";
		default: return "unknown";
	}
}

/**
\brief uninitialize direct sound
*/
static void UninitDirectSound(aoconf_t* aoconf)
{
    // finally release the DirectSound object
    if (hds) {
    	IDirectSound_Release(hds);
    	hds = NULL;
    }
    // free DSOUND.DLL
    if (hdsound_dll) {
    	FreeLibrary(hdsound_dll);
    	hdsound_dll = NULL;
    }
	printmsg(aoconf, "ao_dsound: DirectSound uninitialized\n");
}



/**
\brief enumerate direct sound devices
\return TRUE to continue with the enumeration
*/
static BOOL CALLBACK DirectSoundEnum(LPGUID guid,LPCSTR desc,LPCSTR module,LPVOID context)
{
    int* device_index=(int*)context;
//    mp_msg(MSGT_AO, MSGL_V,"%i %s ",*device_index,desc);
    if(device_num==*device_index){
        //mp_msg(MSGT_AO, MSGL_V,"<--");
        if(guid){
            memcpy(&device,guid,sizeof(GUID));
        }
    }
//    mp_msg(MSGT_AO, MSGL_V,"\n");
    (*device_index)++;
    return TRUE;
}


/**
\brief initilize direct sound
\return 0 if error, 1 if ok
*/
static int InitDirectSound(aoconf_t* aoconf)
{
	DSCAPS dscaps;

	// initialize directsound
    typedef HRESULT (WINAPI* tOurDirectSoundCreate)(LPGUID, LPDIRECTSOUND *, LPUNKNOWN);
	tOurDirectSoundCreate OurDirectSoundCreate;
	typedef HRESULT (WINAPI* tOurDirectSoundEnumerate)(LPDSENUMCALLBACKA, LPVOID);
	tOurDirectSoundEnumerate OurDirectSoundEnumerate;
	int device_index=0;

/*	const opt_t subopts[] = {
	  {"device", OPT_ARG_INT, &device_num,NULL},
	  {NULL}
	};
	if (subopt_parse(ao_subdevice, subopts) != 0) {
		print_help();
		return 0;
	}
*/
	hdsound_dll = LoadLibrary(L"DSOUND.DLL");
	if (hdsound_dll == NULL) {
		printmsg(aoconf, "ao_dsound: cannot load DSOUND.DLL\n");
		return 0;
	}
	OurDirectSoundCreate = (tOurDirectSoundCreate)GetProcAddress(hdsound_dll, "DirectSoundCreate");
	OurDirectSoundEnumerate = (tOurDirectSoundEnumerate)GetProcAddress(hdsound_dll, "DirectSoundEnumerateA");

	if (OurDirectSoundCreate == NULL || OurDirectSoundEnumerate == NULL) {
		printmsg(aoconf, "ao_dsound: GetProcAddress FAILED\n");
		FreeLibrary(hdsound_dll);
		return 0;
	}

	// Enumerate all directsound devices
	printmsg(aoconf,"ao_dsound: Output Devices:\n");
	OurDirectSoundEnumerate(DirectSoundEnum,&device_index);

	// Create the direct sound object
	if FAILED(OurDirectSoundCreate((device_num)?&device:NULL, &hds, NULL )) {
		printmsg(aoconf, "ao_dsound: cannot create a DirectSound device\n");
		FreeLibrary(hdsound_dll);
		return 0;
	}

	/* Set DirectSound Cooperative level, ie what control we want over Windows
	 * sound device. In our case, DSSCL_EXCLUSIVE means that we can modify the
	 * settings of the primary buffer, but also that only the sound of our
	 * application will be hearable when it will have the focus.
	 * !!! (this is not really working as intended yet because to set the
	 * cooperative level you need the window handle of your application, and
	 * I don't know of any easy way to get it. Especially since we might play
	 * sound without any video, and so what window handle should we use ???
	 * The hack for now is to use the Desktop window handle - it seems to be
	 * working */
	if (IDirectSound_SetCooperativeLevel(hds, GetDesktopWindow(), DSSCL_EXCLUSIVE)) {
		printmsg(aoconf, "ao_dsound: cannot set direct sound cooperative level\n");
		IDirectSound_Release(hds);
		FreeLibrary(hdsound_dll);
		return 0;
	}
	printmsg(aoconf, "ao_dsound: DirectSound initialized\n");

	memset(&dscaps, 0, sizeof(DSCAPS));
	dscaps.dwSize = sizeof(DSCAPS);
	if (DS_OK == IDirectSound_GetCaps(hds, &dscaps)) {
		if (dscaps.dwFlags & DSCAPS_EMULDRIVER) printmsg(aoconf, "ao_dsound: DirectSound is emulated, waveOut may give better performance\n");
	} else {
		printmsg(aoconf, "ao_dsound: cannot get device capabilities\n");
	}

	return 1;
}

/**
\brief destroy the direct sound buffer
*/
static void DestroyBuffer(void)
{
	if (hdsbuf) {
		IDirectSoundBuffer_Release(hdsbuf);
		hdsbuf = NULL;
	}
	if (hdspribuf) {
		IDirectSoundBuffer_Release(hdspribuf);
		hdspribuf = NULL;
	}
}

/**
\brief fill sound buffer
\param data pointer to the sound data to copy
\param len length of the data to copy in bytes
\return number of copyed bytes
*/
static int write_buffer(aoconf_t* aoconf, unsigned char *data, int len)
{
  HRESULT res;
  LPVOID lpvPtr1;
  DWORD dwBytes1;
  LPVOID lpvPtr2;
  DWORD dwBytes2;

  // Lock the buffer
  res = IDirectSoundBuffer_Lock(hdsbuf,write_offset, len, &lpvPtr1, &dwBytes1, &lpvPtr2, &dwBytes2, 0);
  // If the buffer was lost, restore and retry lock.
  if (DSERR_BUFFERLOST == res)
  {
    IDirectSoundBuffer_Restore(hdsbuf);
	res = IDirectSoundBuffer_Lock(hdsbuf,write_offset, len, &lpvPtr1, &dwBytes1, &lpvPtr2, &dwBytes2, 0);
  }


  if (SUCCEEDED(res))
  {
  	if( (aoconf->ao_data.channels == 6) && !AF_FORMAT_IS_AC3(aoconf->ao_data.format) ) {
  	    // reorder channels while writing to pointers.
  	    // it's this easy because buffer size and len are always
  	    // aligned to multiples of channels*bytespersample
  	    // there's probably some room for speed improvements here
  	    const int chantable[6] = {0, 1, 4, 5, 2, 3}; // reorder "matrix"
  	    int i, j;
  	    int numsamp,sampsize;

  	    sampsize = loc_af_fmt2bits(aoconf->ao_data.format)>>3; // bytes per sample
  	    numsamp = dwBytes1 / (aoconf->ao_data.channels * sampsize);  // number of samples for each channel in this buffer

  	    for( i = 0; i < numsamp; i++ ) for( j = 0; j < aoconf->ao_data.channels; j++ ) {
  	        memcpy((char*)lpvPtr1+(i*aoconf->ao_data.channels*sampsize)+(chantable[j]*sampsize),data+(i*aoconf->ao_data.channels*sampsize)+(j*sampsize),sampsize);
  	    }

  	    if (NULL != lpvPtr2 )
  	    {
  	        numsamp = dwBytes2 / (aoconf->ao_data.channels * sampsize);
  	        for( i = 0; i < numsamp; i++ ) for( j = 0; j < aoconf->ao_data.channels; j++ ) {
  	            memcpy((char*)lpvPtr2+(i*aoconf->ao_data.channels*sampsize)+(chantable[j]*sampsize),data+dwBytes1+(i*aoconf->ao_data.channels*sampsize)+(j*sampsize),sampsize);
  	        }
  	    }

  	    write_offset+=dwBytes1+dwBytes2;
  	    if(write_offset>=buffer_size)write_offset=dwBytes2;
  	} else {
  	    // Write to pointers without reordering.
	fast_memcpy(lpvPtr1,data,dwBytes1);
    if (NULL != lpvPtr2 )fast_memcpy(lpvPtr2,data+dwBytes1,dwBytes2);
	write_offset+=dwBytes1+dwBytes2;
    if(write_offset>=buffer_size)write_offset=dwBytes2;
  	}

   // Release the data back to DirectSound.
    res = IDirectSoundBuffer_Unlock(hdsbuf,lpvPtr1,dwBytes1,lpvPtr2,dwBytes2);
    if (SUCCEEDED(res))
    {
	  // Success.
	  DWORD status;
	  IDirectSoundBuffer_GetStatus(hdsbuf, &status);
      if (!(status & DSBSTATUS_PLAYING)){
	    res = IDirectSoundBuffer_Play(hdsbuf, 0, 0, DSBPLAY_LOOPING);
	  }
	  return dwBytes1+dwBytes2;
    }
  }
  // Lock, Unlock, or Restore failed.
  return 0;
}

/***************************************************************************************/

/**
\brief handle control commands
\param cmd command
\param arg argument
\return CONTROL_OK or -1 in case the command can't be handled
*/
#if 0
static int control(int cmd, void *arg)
{
	DWORD volume;
	switch (cmd) {
		case AOCONTROL_GET_VOLUME: {
			ao_control_vol_t* vol = (ao_control_vol_t*)arg;
			IDirectSoundBuffer_GetVolume(hdsbuf, &volume);
			vol->left = vol->right = pow(10.0, (float)(volume+10000) / 5000.0);
			//printf("ao_dsound: volume: %f\n",vol->left);
			return CONTROL_OK;
		}
		case AOCONTROL_SET_VOLUME: {
			ao_control_vol_t* vol = (ao_control_vol_t*)arg;
			volume = (DWORD)(log10(vol->right) * 5000.0) - 10000;
			IDirectSoundBuffer_SetVolume(hdsbuf, volume);
			//printf("ao_dsound: volume: %f\n",vol->left);
			return CONTROL_OK;
		}
	}
	return -1;
}
#endif
/**
\brief setup sound device
\param rate samplerate
\param channels number of channels
\param format format
\param flags unused
\return 1=success 0=fail
*/
aoconf_t* audio_init(int rate, int channels, int format, int flags)
{
	aoconf_t* aoconf = (aoconf_t*)malloc(sizeof(aoconf_t));
	memset(aoconf,0,sizeof(aoconf_t));
	aoconf->ao_data.buffersize=-1;

    int res;
	if (!InitDirectSound(aoconf)) {
		return aoconf;
	}

	// ok, now create the buffers
	WAVEFORMATEXTENSIBLE wformat;
	DSBUFFERDESC dsbpridesc;
	DSBUFFERDESC dsbdesc;

	//check if the channel count and format is supported in general
	if (channels > 6) {
		UninitDirectSound(aoconf);
		printmsg(aoconf, "ao_dsound: 8 channel audio not yet supported\n");
		return aoconf;
	}

	if (AF_FORMAT_IS_AC3(format))
		format = AF_FORMAT_AC3_NE;
	switch(format){
		case AF_FORMAT_AC3_NE:
		case AF_FORMAT_S24_LE:
		case AF_FORMAT_S16_LE:
		case AF_FORMAT_U8:
			break;
		default:
			printmsg(aoconf,"ao_dsound: format %s not supported defaulting to Signed 16-bit Little-Endian\n",loc_af_fmt2str_short(format));
			format=AF_FORMAT_S16_LE;
	}
	//fill global ao_data
	aoconf->ao_data.channels = channels;
	aoconf->ao_data.samplerate = rate;
	aoconf->ao_data.format = format;
	aoconf->ao_data.bps = channels * rate * (loc_af_fmt2bits(format)>>3);
	if(aoconf->ao_data.buffersize==-1) aoconf->ao_data.buffersize = aoconf->ao_data.bps; // space for 1 sec
	printmsg(aoconf,"ao_dsound: Samplerate:%iHz Channels:%i Format:%s\n", rate, channels, loc_af_fmt2str_short(format));
	printmsg(aoconf,"ao_dsound: Buffersize:%d bytes (%d msec)\n", aoconf->ao_data.buffersize, aoconf->ao_data.buffersize / aoconf->ao_data.bps * 1000);

	//fill waveformatex
	ZeroMemory(&wformat, sizeof(WAVEFORMATEXTENSIBLE));
	wformat.Format.cbSize          = (channels > 2) ? sizeof(WAVEFORMATEXTENSIBLE)-sizeof(WAVEFORMATEX) : 0;
	wformat.Format.nChannels       = channels;
	wformat.Format.nSamplesPerSec  = rate;
	if (AF_FORMAT_IS_AC3(format)) {
		wformat.Format.wFormatTag      = WAVE_FORMAT_DOLBY_AC3_SPDIF;
		wformat.Format.wBitsPerSample  = 16;
		wformat.Format.nBlockAlign     = 4;
	} else {
		wformat.Format.wFormatTag      = (channels > 2) ? WAVE_FORMAT_EXTENSIBLE : WAVE_FORMAT_PCM;
		wformat.Format.wBitsPerSample  = loc_af_fmt2bits(format);
		wformat.Format.nBlockAlign     = wformat.Format.nChannels * (wformat.Format.wBitsPerSample >> 3);
	}

	// fill in primary sound buffer descriptor
	memset(&dsbpridesc, 0, sizeof(DSBUFFERDESC));
	dsbpridesc.dwSize = sizeof(DSBUFFERDESC);
	dsbpridesc.dwFlags       = DSBCAPS_PRIMARYBUFFER;
	dsbpridesc.dwBufferBytes = 0;
	dsbpridesc.lpwfxFormat   = NULL;


	// fill in the secondary sound buffer (=stream buffer) descriptor
	memset(&dsbdesc, 0, sizeof(DSBUFFERDESC));
	dsbdesc.dwSize = sizeof(DSBUFFERDESC);
	dsbdesc.dwFlags = DSBCAPS_GETCURRENTPOSITION2 /** Better position accuracy */
	                | DSBCAPS_GLOBALFOCUS         /** Allows background playing */
	                | DSBCAPS_CTRLVOLUME;         /** volume control enabled */

	if (channels > 2) {
		wformat.dwChannelMask = channel_mask[channels - 3];
		wformat.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
		wformat.Samples.wValidBitsPerSample = wformat.Format.wBitsPerSample;
		// Needed for 5.1 on emu101k - shit soundblaster
		dsbdesc.dwFlags |= DSBCAPS_LOCHARDWARE;
	}
	wformat.Format.nAvgBytesPerSec = wformat.Format.nSamplesPerSec * wformat.Format.nBlockAlign;

	dsbdesc.dwBufferBytes = aoconf->ao_data.buffersize;
	dsbdesc.lpwfxFormat = (WAVEFORMATEX *)&wformat;
	buffer_size = dsbdesc.dwBufferBytes;
	write_offset = 0;
	min_free_space = wformat.Format.nBlockAlign;
	aoconf->ao_data.outburst = wformat.Format.nBlockAlign * 512;

	// create primary buffer and set its format

	res = IDirectSound_CreateSoundBuffer( hds, &dsbpridesc, &hdspribuf, NULL );
	if ( res != DS_OK ) {
		UninitDirectSound(aoconf);
		printmsg(aoconf,"ao_dsound: cannot create primary buffer (%s)\n", dserr2str(res));
		return aoconf;
	}
	res = IDirectSoundBuffer_SetFormat( hdspribuf, (WAVEFORMATEX *)&wformat );
	if ( res != DS_OK ) printmsg(aoconf,"ao_dsound: cannot set primary buffer format (%s), using standard setting (bad quality)", dserr2str(res));

	printmsg(aoconf, "ao_dsound: primary buffer created\n");

	// now create the stream buffer

	res = IDirectSound_CreateSoundBuffer(hds, &dsbdesc, &hdsbuf, NULL);
	if (res != DS_OK) {
		if (dsbdesc.dwFlags & DSBCAPS_LOCHARDWARE) {
			// Try without DSBCAPS_LOCHARDWARE
			dsbdesc.dwFlags &= ~DSBCAPS_LOCHARDWARE;
			res = IDirectSound_CreateSoundBuffer(hds, &dsbdesc, &hdsbuf, NULL);
		}
		if (res != DS_OK) {
			UninitDirectSound(aoconf);
			printmsg(aoconf, "ao_dsound: cannot create secondary (stream)buffer (%s)\n", dserr2str(res));
			return aoconf;
		}
	}
	printmsg(aoconf, "ao_dsound: secondary (stream)buffer created\n");
	aoconf->inited=1;
	return aoconf;
}

int audio_inited(aoconf_t* aoconf)
{
	return aoconf->inited;
}


/**
\brief stop playing and empty buffers (for seeking/pause)
*/
void audio_reset(aoconf_t* aoconf)
{
	IDirectSoundBuffer_Stop(hdsbuf);
	// reset directsound buffer
	IDirectSoundBuffer_SetCurrentPosition(hdsbuf, 0);
	write_offset=0;
}

/**
\brief stop playing, keep buffers (for pause)
*/
void audio_pause(aoconf_t* aoconf)
{
	IDirectSoundBuffer_Stop(hdsbuf);
}

/**
\brief resume playing, after audio_pause()
*/
static void audio_resume(void)
{
	IDirectSoundBuffer_Play(hdsbuf, 0, 0, DSBPLAY_LOOPING);
}

/**
\brief close audio device
\param immed stop playback immediately
*/
void audio_uninit(aoconf_t* aoconf, int immed)
{
	if(immed)
		audio_reset(aoconf);
	else{
		DWORD status;
		IDirectSoundBuffer_Play(hdsbuf, 0, 0, 0);
		while(!IDirectSoundBuffer_GetStatus(hdsbuf,&status) && (status&DSBSTATUS_PLAYING))
//			usec_sleep(20000);
			Sleep(20);
	}
	DestroyBuffer();
	UninitDirectSound(aoconf);
}

/**
\brief find out how many bytes can be written into the audio buffer without
\return free space in bytes, has to return 0 if the buffer is almost full
*/
int audio_get_space(aoconf_t* aoconf)
{
	int space;
	DWORD play_offset;
	IDirectSoundBuffer_GetCurrentPosition(hdsbuf,&play_offset,NULL);
	space=buffer_size-(write_offset-play_offset);
	//                |                                                      | <-- const --> |                |                 |
	//                buffer start                                           play_cursor     write_cursor     write_offset      buffer end
	// play_cursor is the actual postion of the play cursor
	// write_cursor is the position after which it is assumed to be save to write data
	// write_offset is the postion where we actually write the data to
	if(space > buffer_size)space -= buffer_size; // write_offset < play_offset
	if(space < min_free_space)return 0;
	return space-min_free_space;
}

/**
\brief play 'len' bytes of 'data'
\param data pointer to the data to play
\param len size in bytes of the data buffer, gets rounded down to outburst*n
\param flags currently unused
\return number of played bytes
*/
int audio_play(aoconf_t* aoconf, void* data, int len, int flags)
{
	DWORD play_offset;
	int space;

	// make sure we have enough space to write data
	IDirectSoundBuffer_GetCurrentPosition(hdsbuf,&play_offset,NULL);
	space=buffer_size-(write_offset-play_offset);
	if(space > buffer_size)space -= buffer_size; // write_offset < play_offset
	if(space < len) len = space;

	if (!(flags & AOPLAY_FINAL_CHUNK))
	len = (len / aoconf->ao_data.outburst) * aoconf->ao_data.outburst;
	return write_buffer(aoconf, (unsigned char*)data, len);
}

/**
\brief get the delay between the first and last sample in the buffer
\return delay in seconds
*/
float audio_get_delay(aoconf_t* aoconf)
{
	DWORD play_offset;
	int space;
	IDirectSoundBuffer_GetCurrentPosition(hdsbuf,&play_offset,NULL);
	space=play_offset-write_offset;
	if(space <= 0)space += buffer_size;
	return (float)(buffer_size - space) / (float)aoconf->ao_data.bps;
}

#ifdef __cplusplus
};
#endif

