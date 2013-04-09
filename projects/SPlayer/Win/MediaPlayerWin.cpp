/**********************************************************\ 
Original Author: Richard Bateman and Georg Fritzsche 

Created:    December 3, 2009
License:    Dual license model; choose one of two:
            New BSD License
            http://www.opensource.org/licenses/bsd-license.php
            - or -
            GNU Lesser General Public License, version 2.1
            http://www.gnu.org/licenses/lgpl-2.1.html

Copyright 2009 Georg Fritzsche,
               Firebreath development team
\**********************************************************/

#define _WIN32_DCOM
#pragma warning(disable : 4995)

#pragma comment(lib, "dsound.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "winmm.lib")

#include <list>
#include <boost/assign/list_of.hpp>
//#include <atlbase.h>
//extern CComModule _Module;
//#include <atlcom.h>
//#include <atlstr.h>
//#include <dshow.h>
#include "JSAPI.h"
#include "Win/PluginWindowWin.h"
#include "../MediaPlayer.h"
#include "error_mapping.h"

#include "VoDirectX.h"
#include "AoDirect.h"
#include "libxplayer.h"
#include "af_format.h"

#include <dsound.h>

DWORD WINAPI AoThreadFunction( LPVOID lpParam );
DWORD WINAPI VoThreadFunction( LPVOID lpParam );

struct PlayerContext;

typedef struct {
    voconf_t* voconf;
    aoconf_t* aoconf;
    int run;
    int mode;
    int size;
    int slot;
    HWND hwnd;
	FB::PluginWindow* win;
    mp_image_t*	img;
    std::string file;

    char* filename;
    int status;
    int pause;
    int stop;
} MYDATA, *PMYDATA;

#define VO_THREAD	0
#define AO_THREAD	1
#define MAX_THREADS	1

static HANDLE aTh[1];
static DWORD aThId[1];
static int aThRun = 0;

struct PlayerContext 
{
    int w;
    int h;
	int n;

    PMYDATA pDataArray;
    DWORD   dwThreadIdArray[MAX_THREADS];
	HANDLE  hThreadArray[MAX_THREADS];
	
    HWND hwnd;
	FB::PluginWindow* win;

	int slot;
    std::string error;
    std::string file;

    PlayerContext() : pDataArray(0), hwnd(0), n(0) {}
};

namespace 
{
    std::string vfwErrorString(const std::string& what, HRESULT hr)
    {
        std::ostringstream os;
        os << what << ": " << mapVfwError(hr);
        return os.str();
    }

    PlayerContextPtr make_context(HWND hwnd)
    {
        PlayerContextPtr context(new PlayerContext);
        if(!context) throw MediaPlayer::InitializationException("failed to create context");

        context->hwnd = hwnd;


        return context;
    }

    bool activateVideo(PlayerContextPtr context)
    {
        if(!context->hwnd)
            return true;

	if(!context->pDataArray)
	{
		context->pDataArray = (PMYDATA) HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(MYDATA));
	}

	if(context->pDataArray->run) {
		context->pDataArray->run = 0;
		WaitForMultipleObjects(MAX_THREADS, context->hThreadArray, TRUE, 1000);
		if(context->pDataArray->aoconf)
			audio_uninit(context->pDataArray->aoconf, 0);
		context->pDataArray->aoconf=NULL;
		CloseHandle(context->hThreadArray[VO_THREAD]);
		CloseHandle(context->hThreadArray[AO_THREAD]);
	}
		
		context->pDataArray->file=context->file;
		context->pDataArray->voconf = NULL;
		context->pDataArray->hwnd = context->hwnd;
		context->pDataArray->win = context->win;
        context->pDataArray->run = 1;
        context->pDataArray->mode = 1;
		context->pDataArray->status=0;
		context->pDataArray->pause=0;
		context->pDataArray->stop=0;
		context->pDataArray->filename=strdup(context->file.c_str());
        context->n++;

#if 0
	context->hThreadArray[AO_THREAD] = CreateThread(
            NULL,                       // default security attributes
            0,                          // use default stack size  
            AoThreadFunction,           // thread function name
            context->pDataArray,        // argument to thread function 
            0,                          // use default creation flags 
            &context->dwThreadIdArray[AO_THREAD]); // returns the thread identifier 
#endif
	context->hThreadArray[VO_THREAD] = CreateThread(
            NULL,                       // default security attributes
            0,                          // use default stack size  
            VoThreadFunction,          // thread function name
            context->pDataArray,        // argument to thread function 
            0,                          // use default creation flags 
            &context->dwThreadIdArray[VO_THREAD]); // returns the thread identifier 

	return true;
    }
};

MediaPlayer::MediaPlayer(int pluginIdentifier, int slotIdentifier, int loglevel, int debugflag, std::string logfile)
  : m_context()
  , m_version("")
  , m_type("DirectDraw")
{
    ::CoInitializeEx(0, COINIT_MULTITHREADED);

    playerId = pluginIdentifier;
    slotId = slotIdentifier;
    logLevel = loglevel;
    xplayer_API_setloglevel(slotId, logLevel);
    xplayer_API_setlogfile(logfile.c_str());
    xplayer_API_setdebug(slotId, debugflag);

    try
    {
        m_context = make_context(0);
    }
    catch(const InitializationException& e)
    {
        if(!m_context)
            m_context = PlayerContextPtr(new PlayerContext);
        m_context->error = e.what();
        throw;
    }
    m_context->slot = slotId;
}

MediaPlayer::~MediaPlayer()
{
    HRESULT hr;

    m_context->pDataArray->run = 0;
    m_context->pDataArray->mode = 0;

    stop();
}

void MediaPlayer::StaticInitialize()
{
	slog("static init\n");
	aTh[0] = CreateThread(
            NULL,                       // default security attributes
            0,                          // use default stack size  
            AoThreadFunction,           // thread function name
            NULL,        // argument to thread function 
            0,                          // use default creation flags 
            &aThId[0]); // returns the thread identifier 
}

void MediaPlayer::StaticDeinitialize()
{
	slog("static deinit\n");
	aThRun = 0;
	WaitForMultipleObjects(1, aTh, TRUE, 1000);
	CloseHandle(aTh[0]);
}

void MediaPlayer::setWindow(FB::PluginWindow* pluginWindow)
{
    HRESULT hr;
    HWND hwnd = 0;

    if(pluginWindow) {
        FB::PluginWindowWin* wnd = reinterpret_cast<FB::PluginWindowWin*>(pluginWindow);
        hwnd = wnd->getHWND();
    }
    
    if(m_context->pDataArray) {
        m_context->pDataArray->hwnd = hwnd;
		m_context->pDataArray->win = pluginWindow;
	    slog("set plugin window: %p\n",pluginWindow);
    }
    
    m_context->hwnd = hwnd;
	m_context->win = pluginWindow;

	slog("set window: %p\n",hwnd);
}

bool MediaPlayer::setloglevel(int logLevel)
{
    xplayer_API_setloglevel(slotId, logLevel);
    return true;
}


const std::string& MediaPlayer::version() const
{
    return m_version;
}

const std::string& MediaPlayer::type() const
{
    return m_type;
}

const std::string& MediaPlayer::lastError() const
{
    return m_context->error;
}

bool MediaPlayer::open(const std::string& url)
{
    m_context->file = url;
    activateVideo(m_context);
    xplayer_API_setimage(slotId, 0, 0, IMGFMT_BGR32);
//    xplayer_API_setimage(slotId, 0, 0, IMGFMT_RGB24);
//    xplayer_API_setimage(slotId, 0, 0, IMGFMT_YV12);
//    xplayer_API_setimage(slotId, 0, 0, IMGFMT_I420);
    xplayer_API_sethwbuffersize(slotId, xplayer_API_prefilllen());

    xplayer_API_loadurl(slotId, (char*)url.c_str());
//    xplayer_API_setimage(slotId, 0, 0, IMGFMT_RGB24);
    return false;
}


bool MediaPlayer::play()
{
    activateVideo(m_context);
    xplayer_API_play(slotId);
    return true;
}

bool MediaPlayer::groupplay()
{
    xplayer_API_groupplay(slotId);
    return true;
}

bool MediaPlayer::groupstop()
{
    xplayer_API_groupstop(slotId);
    return true;
}

bool MediaPlayer::groupseekpos(const double pos)
{
    xplayer_API_groupseekpos(slotId, pos);
    return false;
}

bool MediaPlayer::pause()
{
    xplayer_API_pause(slotId);
    return false;
}

bool MediaPlayer::stop()
{
	xplayer_API_stop(slotId);
	if(m_context->pDataArray && m_context->pDataArray->run) 
	{
		m_context->pDataArray->run = 0;
		WaitForMultipleObjects(MAX_THREADS, m_context->hThreadArray, TRUE, 1000);
		if(m_context->pDataArray->aoconf)
			audio_uninit(m_context->pDataArray->aoconf, 0);
		m_context->pDataArray->aoconf=NULL;
		CloseHandle(m_context->hThreadArray[VO_THREAD]);
		CloseHandle(m_context->hThreadArray[AO_THREAD]);
	}
    m_context->pDataArray->mode = 0;
    return true;
}    

bool MediaPlayer::seek(const double pos)
{
    double length = xplayer_API_getmovielength(slotId);
    if(length>0.0 && pos>=0.0 && pos<=100.0) {
        double timepos = length * pos / 100;
        xplayer_API_seek(slotId, timepos);
    }
    return false;
}

bool MediaPlayer::seekpos(const double pos)
{
    xplayer_API_seekpos(slotId, pos);
    return false;
}

bool MediaPlayer::seekrel(const double pos)
{
    xplayer_API_seekrel(slotId, pos);
    return false;
}

bool MediaPlayer::volume(int vol)
{
    xplayer_API_volume(slotId, vol);
    return false;
}

int MediaPlayer::getvolume()
{
    return xplayer_API_getvolume(slotId);
}

bool MediaPlayer::mute(bool mute)
{
    return xplayer_API_mute(slotId, mute);
}

bool MediaPlayer::getmute()
{
    return xplayer_API_getmute(slotId);
}

bool MediaPlayer::close()
{
    xplayer_API_slotfree(slotId);
    return false;
}

bool MediaPlayer::rtspmode(const std::string& mode)
{
    xplayer_API_setoptions(slotId, "rtsp_transport", mode.c_str());
    return false;
}

bool MediaPlayer::setOptions(const std::string& optname, const std::string& optvalue)
{
    xplayer_API_setoptions(slotId, optname.c_str(),optvalue.c_str());
    return false;
}

int MediaPlayer::getstatus(void)
{
    return xplayer_API_getstatus(slotId);
}

char* MediaPlayer::getstatusline(void)
{
    char* line = xplayer_API_getstatusline(slotId);
    char* sline = NULL;

    if(line)
    {
        sline=strdup(line);
        xplayer_API_freestatusline(line);
    }
    return sline;
}

bool MediaPlayer::flush()
{
    xplayer_API_flush(slotId);
    return false;
}

bool MediaPlayer::audiodisable(bool dis)
{
    xplayer_API_enableaudio(slotId, !dis);
    return false;
}

double MediaPlayer::getcurrentpts()
{
    return xplayer_API_getcurrentpts(slotId);
}

double MediaPlayer::getmovielength()
{
    return xplayer_API_getmovielength(slotId);
}

int MediaPlayer::getgroup()
{
    return xplayer_API_getgroup(slotId);
}

void MediaPlayer::setgroup(int group)
{
    xplayer_API_setgroup(slotId, group);
}

void MediaPlayer::settimeshift(double time)
{
    xplayer_API_settimeshift(slotId, time);
}

DWORD WINAPI MyThreadFunction( LPVOID lpParam )
{
    PMYDATA pDataArray;

    pDataArray = (PMYDATA)lpParam;

	int w = 0;
	int h = 0;
	int fmt = 0;

    while(pDataArray->run) {
		if(pDataArray->img) {
			if(pDataArray->img->w!=w || pDataArray->img->h!=h || pDataArray->img->imgfmt!=fmt) {
				w=pDataArray->img->w;
				h=pDataArray->img->h;
				fmt=pDataArray->img->imgfmt;
				vo_config(pDataArray->voconf, w,h,w,h,0,NULL,fmt);
			}
	        vo_draw_frame(pDataArray->voconf, pDataArray->img->planes);
		    vo_flip_page(pDataArray->voconf);
		}
        Sleep(40);
    }

	vo_uninit(pDataArray->voconf);
    return 0; 
}

extern "C" {
	mp_image_t* win_draw(mp_image_t* img, mp_image_t* src)
	{
		mp_image_t* ret = img;

		if(!img || img->w!=src->w || img->h!=src->h || img->imgfmt!=src->imgfmt)
		{
            ret=alloc_mpi(src->w, src->h, src->imgfmt);
			if(img)
				free_mp_image(img);
		}
        copy_mpi(ret, src);

		return ret;
	};
};

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

DWORD WINAPI AoThreadFunction( LPVOID lpParam )
{
	char abuffer[0x10000];
	aoconf_t* aoconf;
	int rate = xplayer_API_getaudio_rate();
	int channels = xplayer_API_getaudio_channels();
	int len;
	int plen = 0;

	slog("audio thread start\n");
	aThRun=1;
#if 0

	aoconf = audio_init(rate, channels, AF_FORMAT_S16_LE, 0);
	slog("%s\n" ,agetmsg(aoconf));
	memset(abuffer,0,sizeof(abuffer));
	while(1) {
		len=xplayer_API_prefillaudio(abuffer, sizeof(abuffer), plen);
		plen+=len;
		if(!len)
			break;
		if(len)
			audio_play(aoconf, abuffer, len, 1);
	}
	while(aThRun) {
		len=xplayer_API_getaudio(abuffer, sizeof(abuffer));
		if(len)
			audio_play(aoconf, abuffer, len, 1);
        Sleep(1);
    }
	audio_uninit(aoconf, 1);
#else
	HRESULT result;
	DSBUFFERDESC bufferDesc;
	WAVEFORMATEX waveFormat;
	IDirectSound8* m_DirectSound = 0;
 	IDirectSoundBuffer* m_primaryBuffer = 0;
 	IDirectSoundBuffer8* m_secondaryBuffer = 0;
	IDirectSoundBuffer* tempBuffer;
	unsigned char *bufferPtr;
	unsigned long bufferSize;

	// Initialize the direct sound interface pointer for the default sound device.
	result = DirectSoundCreate8(NULL, &m_DirectSound, NULL);
	if(FAILED(result))
	{
		slog("audio thread: DirectSoundCreate8() fail.\n");
		slog("Result: %d %s\n",result,dserr2str(result));
		return 0;
	}

	// Set the cooperative level to priority so the format of the primary sound buffer can be modified.
	result = m_DirectSound->SetCooperativeLevel(GetDesktopWindow(), DSSCL_PRIORITY);
	if(FAILED(result))
	{
		slog("audio thread: SetCooperativeLevel() fail.\n");
		slog("Result: %d %s\n",result,dserr2str(result));
		return 0;
	} 

	// Setup the format of the primary sound bufffer.
	// In this case it is a .WAV file recorded at 44,100 samples per second in 16-bit stereo (cd audio format).
	waveFormat.wFormatTag = WAVE_FORMAT_PCM;
	waveFormat.nSamplesPerSec = rate;
	waveFormat.wBitsPerSample = 16;
	waveFormat.nChannels = channels;
	waveFormat.nBlockAlign = (waveFormat.wBitsPerSample / 8) * waveFormat.nChannels;
	waveFormat.nAvgBytesPerSec = waveFormat.nSamplesPerSec * waveFormat.nBlockAlign;
	waveFormat.cbSize = 0;
 
	// Setup the primary buffer description.
	bufferDesc.dwSize = sizeof(DSBUFFERDESC);
	bufferDesc.dwFlags = DSBCAPS_PRIMARYBUFFER | DSBCAPS_CTRLVOLUME;
	bufferDesc.dwBufferBytes = 0;
	bufferDesc.dwReserved = 0;
	bufferDesc.lpwfxFormat = NULL;
	bufferDesc.guid3DAlgorithm = GUID_NULL;

	// Get control of the primary sound buffer on the default sound device.
	result = m_DirectSound->CreateSoundBuffer(&bufferDesc, &m_primaryBuffer, NULL);
	if(FAILED(result))
	{
		slog("audio thread: CreateSoundBuffer() fail.\n");
		slog("Result: %d %s\n",result,dserr2str(result));
		return 0;
	}

	// Set the primary buffer to be the wave format specified.
	result = m_primaryBuffer->SetFormat(&waveFormat);
	if(FAILED(result))
	{
		slog("audio thread: SetFormat() fail.\n");
		slog("nSamplesPerSec : %d\n",waveFormat.nSamplesPerSec);
		slog("wBitsPerSample : %d\n",waveFormat.wBitsPerSample);
		slog("nChannels      : %d\n",waveFormat.nChannels);
		slog("nBlockAlign    : %d\n",waveFormat.nBlockAlign);
		slog("nAvgBytesPerSec: %d\n",waveFormat.nAvgBytesPerSec);
		slog("Result: %d %s\n",result,dserr2str(result));
		return 0;
	}


	// Set the buffer description of the secondary sound buffer that the wave file will be loaded onto.
	bufferDesc.dwSize = sizeof(DSBUFFERDESC);
	bufferDesc.dwFlags = DSBCAPS_CTRLVOLUME | DSBCAPS_GLOBALFOCUS;
	bufferDesc.dwBufferBytes = sizeof(abuffer);
	bufferDesc.dwReserved = 0;
	bufferDesc.lpwfxFormat = &waveFormat;
	bufferDesc.guid3DAlgorithm = GUID_NULL;
	// Create a temporary sound buffer with the specific buffer settings.
	result = m_DirectSound->CreateSoundBuffer(&bufferDesc, &tempBuffer, NULL);
	if(FAILED(result))
	{
		slog("audio thread: create temp buffer fail.\n");
		slog("Result: %d %s\n",result,dserr2str(result));
		return 0;
	}
	// Test the buffer format against the direct sound 8 interface and create the secondary buffer.
	result = tempBuffer->QueryInterface(IID_IDirectSoundBuffer8, (void**)&m_secondaryBuffer);
	if(FAILED(result))
	{
		slog("audio thread: query interface fail.\n");
		slog("Result: %d %s\n",result,dserr2str(result));
		return false;
	}
 
	// Release the temporary buffer.
	tempBuffer->Release();
	tempBuffer = 0; 

	slog("audio thread: start audio\n");
	while(aThRun) {
		len=xplayer_API_getaudio(abuffer, sizeof(abuffer));
		if(len)
		{
			m_secondaryBuffer->Lock(0, len, (void**)&bufferPtr, (DWORD*)&bufferSize, NULL, 0, 0);
//			slog("audio thread: copy %d (%d) bytes from %p to %p\n",len,bufferSize,abuffer,bufferPtr);
			if(bufferPtr)
				memcpy(bufferPtr,abuffer,len);
			result = m_secondaryBuffer->Unlock((void*)bufferPtr, bufferSize, NULL, 0);
			if(FAILED(result))
			{
				slog("audio thread: unlock fail.\n");
				slog("Result: %d %s\n",result,dserr2str(result));
//				return false;
			}
			result = m_secondaryBuffer->SetCurrentPosition(0);
			if(FAILED(result))
			{
				slog("audio thread: setCurrentPosition fail.\n");
				slog("Result: %d %s\n",result,dserr2str(result));
//				return false;
			}
			result = m_secondaryBuffer->SetVolume(DSBVOLUME_MAX);
			if(FAILED(result))
			{
				slog("audio thread: setVolume fail.\n");
				slog("Result: %d %s\n",result,dserr2str(result));
//				return false;
			}
//			result = m_secondaryBuffer->Play(0, 0, 0);
			result = m_secondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);
			if(FAILED(result))
			{
				slog("audio thread: play fail.\n");
			}
 		}
        Sleep(1);
    }
	slog("audio thread: end audio\n");



	if(m_secondaryBuffer)
	{
		m_secondaryBuffer->Release();
		m_secondaryBuffer = 0;
	}
	if(m_primaryBuffer)
	{
		m_primaryBuffer->Release();
		m_primaryBuffer = 0;
	}
 
	// Release the direct sound interface pointer.
	if(m_DirectSound)
	{
		m_DirectSound->Release();
		m_DirectSound = 0;
	}

	// Shutdown the Direct Sound API.
//	ShutdownDirectSound();
#endif

	slog("audio thread end\n");
    return NULL;
}

#if 1

DWORD WINAPI VoThreadFunction( LPVOID lpParam )
{
    PMYDATA priv;
    priv = (PMYDATA)lpParam;

    mp_image_t* img = NULL;
    mp_image_t* frame = NULL;
    int w=0;
    int h=0;
    int fmt=0;
    int winw = 640;
    int winh = 480;
    int owinw = 0;
    int owinh = 0;

    double stime=0.0;
    double etime=0.0;
    double ltime=0.0;
    HWND hwnd = 0;
	HDC hdc;
	PAINTSTRUCT ps;
	HGDIOBJ oldBitMap;
	HDC hdcMem;
	BITMAP bitmap;
	HBITMAP hBitmap;              
					


    priv->run|=1;
    while(priv->run) {
		if(hwnd != priv->hwnd) {
			hwnd=0;
			w=h=fmt=0;
		}
		if(priv->hwnd && !hwnd) {
			hwnd = priv->hwnd;
			slog("hwnd: %p\n",hwnd);
		}	
		if (priv->win)
		{
			FB::Rect pos = priv->win->getWindowPosition();
			winw = pos.right-pos.left;
			winh = pos.bottom-pos.top;
//			slog("Window size is %d, %d\n",winw, winh);
		}
		if(xplayer_API_isnewimage(priv->slot))
        {
			xplayer_API_getimage(priv->slot, &img);
			if(img && (w!=img->w || h!=img->h)) {
                w=img->w;
                h=img->h;
				fmt=img->imgfmt;
				slog("video config: w: %d, h: %d, fmt: 0x%x\n",w,h,fmt);
            }
            if(img) {
                if(img->imgfmt==IMGFMT_I420) {
					unsigned char* tmp = img->planes[1];
                    img->planes[1] = img->planes[2];
                    img->planes[2] = tmp;
                }
 				if(hwnd && w && h) {
					if(w && h) {
						if ( owinw != winw || owinh != winh) {
							slog("Resize required: new size is %d, %d\n",winw, winh);
							xplayer_API_setimage(priv->slot, winw, winh, IMGFMT_BGR32);
							owinw=winw;
							owinh=winh;
						}	
					}
					slog("create bitmap ...\n");
					hBitmap = CreateBitmap(w,h,1,32,img->planes[0]);
					slog("hBitmap: %p\n",hBitmap);
					hdcMem = CreateCompatibleDC(NULL);
					SelectObject(hdcMem, hBitmap);				
					hdc = GetDC(hwnd);
					BitBlt(hdc, 0, 0, w, h, hdcMem, 0, 0, SRCCOPY);
					ReleaseDC(hwnd, hdc);
					DeleteDC(hdcMem);
					DeleteObject(hBitmap);
				}
                if(img->imgfmt==IMGFMT_I420) {
					unsigned char* tmp = img->planes[1];
                    img->planes[1] = img->planes[2];
                    img->planes[2] = tmp;
                }
                xplayer_API_imagedone(priv->slot);
            }
		}
        ltime=etime;
        etime=xplayer_clock();
        Sleep(40);
        stime=xplayer_clock();
        if((priv->run & 2) && !(xplayer_API_getstatus(priv->slot)&STATUS_PLAYER_OPENED))
        {
            break;
        }
    }
	slog("end of video process\n");
    xplayer_API_videoprocessdone(priv->slot);
    priv->run=0;
	return NULL;
}

#else

DWORD WINAPI VoThreadFunction( LPVOID lpParam )
{
    PMYDATA priv;
    priv = (PMYDATA)lpParam;

    mp_image_t* img = NULL;
    mp_image_t* frame = NULL;
    int w=0;
    int h=0;
    int fmt=0;
    int winw = 640;
    int winh = 480;

    double stime=0.0;
    double etime=0.0;
    double ltime=0.0;
    HWND hwnd = 0;
	HDC hdc;
                                     
    priv->run|=1;

    while(priv->run) {
		if(hwnd != priv->hwnd) {
			hwnd=0;
			if(priv->voconf)
				vo_uninit(priv->voconf);
			priv->voconf=NULL;
		}
		if(priv->hwnd) {
			hwnd = priv->hwnd;
		}	
		if(!priv->voconf && hwnd) {
			priv->voconf = preinit(1,hwnd);
			w=h=fmt=0;
			slog("voconf: %p hwnd: %p\n",priv->voconf,hwnd);
		}
        if(xplayer_API_isnewimage(priv->slot))
        {
			xplayer_API_getimage(priv->slot, &img);
			if(img && (w!=img->w || h!=img->h) && priv->voconf) {
                w=img->w;
                h=img->h;
				fmt=img->imgfmt;
				slog("video config: w: %d, h: %d, fmt: 0x%x\n",w,h,fmt);
				vo_config(priv->voconf, w,h,w,h,0,NULL,fmt);
            }
            if(img) {
                if(img->imgfmt==IMGFMT_I420) {
					unsigned char* tmp = img->planes[1];
                    img->planes[1] = img->planes[2];
                    img->planes[2] = tmp;
                }
 				if(hwnd && priv->voconf) {
					vo_draw_frame(priv->voconf, img->planes);
					vo_flip_page(priv->voconf);
				}
                if(img->imgfmt==IMGFMT_I420) {
					unsigned char* tmp = img->planes[1];
                    img->planes[1] = img->planes[2];
                    img->planes[2] = tmp;
                }
                xplayer_API_imagedone(priv->slot);
            }
		}
        ltime=etime;
        etime=xplayer_clock();
        Sleep(40);
        stime=xplayer_clock();
        if((priv->run & 2) && !(xplayer_API_getstatus(priv->slot)&STATUS_PLAYER_OPENED))
        {
            break;
        }
    }
	slog("end of video process\n");
	if(hwnd && priv->voconf) {
		vo_uninit(priv->voconf);
	}
    xplayer_API_videoprocessdone(priv->slot);
    priv->run=0;
	return NULL;
}
#endif



