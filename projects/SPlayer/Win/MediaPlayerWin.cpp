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

//#include "VoDirectX.h"
#include "AoDirect.h"
#include "libxplayer.h"
#include "af_format.h"
#include "D3Drender.h"

#include "aboutBox.h"

#include "winres.h"

#include <dsound.h>

DWORD WINAPI AoThreadFunction( LPVOID lpParam );
DWORD WINAPI VidThreadFunction(LPVOID lpParam);

struct PlayerContext;

//audio thread
static HANDLE aTh[1];
static DWORD aThId[1];
static int aThRun = 0;


struct PlayerContext 
{
    int w;
    int h;
    int n;

    DWORD threadId;
    HANDLE hThread;
    int run;
	
    HWND hwnd;
    FB::PluginWindow* win;

    int slot;
    std::string error;
    std::string file;

    PlayerContext() :  hwnd(0), n(0), win(0) {}
};


void startVidThread(PlayerContext * context)
{
    if (context->run > 0) { return; } //already started the thread
    
    context->run = 1;
    
    context->hThread = CreateThread(NULL, 0, VidThreadFunction, context, 0, &(context->threadId) );
        
}//startvidthread


void endVidThread(PlayerContext * context)
{
  if (context->run <= 0) { return; } //already closed threa
  context->run = 0;
  WaitForSingleObject(context->hThread, INFINITE);
  
}//endvidthread


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

    
    m_context = PlayerContextPtr(new PlayerContext);
    m_context->hwnd = 0;
    m_context->slot = slotId;
    
    startVidThread(m_context.get());
    
    slog("ctor slot: %d \n", slotId);
}//ctor


MediaPlayer::~MediaPlayer()
{

    stop();
    
    endVidThread(m_context.get());
  
    slog("dtor slot:%d", slotId);
}//dtor


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
    HWND hwnd = 0;

    if(pluginWindow) {
        FB::PluginWindowWin* wnd = reinterpret_cast<FB::PluginWindowWin*>(pluginWindow);
        hwnd = wnd->getHWND();
    }
    
    
    m_context->hwnd = hwnd;
    m_context->win = pluginWindow;

	slog("set window: %p\n",hwnd);
}


static void openAboutPopup(HWND hwnd)
{
     HMENU hPopupMenu = CreatePopupMenu();
     POINT cp = {0};

        GetCursorPos( &cp );

            InsertMenu(hPopupMenu, 0, MF_BYPOSITION | MF_STRING, 2, L"About");
            InsertMenu(hPopupMenu, 0, MF_BYPOSITION | MF_STRING, 3, L"---------------------");
            InsertMenu(hPopupMenu, 0, MF_BYPOSITION | MF_STRING, 1, L"Learningspace SPlayer");

            TrackPopupMenu(hPopupMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, cp.x, cp.y, 0, hwnd, NULL);
        //note: trackpopupmenu takes care of deleting the popupmenu after its no longer needed
        //(if its assigned to a window that is)

}//openabout


bool MediaPlayer::onMouseDown(FB::MouseDownEvent * evt)
{
    if (m_context->hwnd == 0) { return false; }

    if (evt->m_Btn ==  FB::MouseDownEvent::MouseButton_Right)
    { 
        openAboutPopup(m_context->hwnd); 

        return true;
    }//endif

    return false;
}//onmousedown


bool MediaPlayer::onWindowsEvent(FB::WindowsEvent* evt, FB::PluginWindow * win)
{

    if (m_context->hwnd == 0) { return false; }
    
    //windows even handling is needed for  WM_COMMAND (that is how the popup menu sends commands)

    if (evt->uMsg == WM_COMMAND && LOWORD(evt->wParam) == 2)
    {
        //MessageBox(m_context->hwnd, L"Made by CAE", L"About", MB_OK);

        showCaeAboutBox(m_context->hwnd);

        return true;
    }//endif


    return false;
}//onwinevent


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

    xplayer_API_setimage(slotId, 0, 0, IMGFMT_BGR32);
    xplayer_API_sethwbuffersize(slotId, xplayer_API_prefilllen());
    xplayer_API_loadurl(slotId, (char*)url.c_str());

    return false;
}


bool MediaPlayer::play()
{
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
    return xplayer_API_mute(slotId, (mute ? 1 : 0) );
}

bool MediaPlayer::getmute()
{
    return (xplayer_API_getmute(slotId) > 0);
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
//	aoconf_t* aoconf;
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
}//aothreadfunction




//based on
//http://stackoverflow.com/questions/9296059/read-pixel-value-in-bmp-file

//load a 24bpp .bmp file into memory
//remember to free it when you no longer need it
static char * readBmp(char * memfile, int * width, int * height)
{
 int w;
 int h;
 int i;
 int k;
 int pad;
 char * data;

 w = *(int*)&memfile[18];
 h = *(int*)&memfile[22];

 *width = w;
 *height = h;
 
 data = new char[w*h*3];

 pad = (w*3 + 3) & (~3);
 
 k = 54;
 
 //copy image (and mirror vertically)
  for(i = 0; i < h; i++)
  {
    memcpy(&(data[(h-i-1)*w*3]), &(memfile[k]), w*3);
    k += pad;
  }//nexti

 return data;
}//readbmp 



DWORD WINAPI VidThreadFunction(LPVOID lpParam)
{

    PlayerContext * priv;
    priv = (PlayerContext *) lpParam;

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
             
				
    D3Drender d3dlocal; //needs to be local to thread
       
    slog("vidthread start :: priv  %p \n", priv);
    slog("priv hwnd  %d \n", priv->hwnd);
    slog("priv slot %d \n", priv->slot);

    while (priv->run > 0)
    {
  
		if(hwnd != priv->hwnd) {
			hwnd=0;
			w=h=fmt=0;
		}
   
		if(priv->hwnd && !hwnd) {
			hwnd = priv->hwnd;
			slog("hwnd: %p\n",hwnd);

                //set image to cae backdrop

                      HMODULE hModule; 
                      HGLOBAL hMem;
                      HRSRC hRsrc;
                      GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, L"npSPlayer.dll", &hModule); 
                      hRsrc = FindResource(hModule, MAKEINTRESOURCE(IDB_BITMAPCAEBG), RT_RCDATA);
                      hMem = LoadResource(hModule, hRsrc); 


                      char * filemem = (char*) LockResource(hMem); //don't free, only need to do that on 16bit windows
  
                      char * data;
                      char * raw;
                      int i, g, m, w, h, num;
  
                      raw = readBmp(filemem, &w, &h);

                      num = w * h;
                      data = new char[num*4]; //allocate for 32bpp
  
                        //convert loaded image to 32bit

                          for (i = 0; i < num; i++)
                          {
                            g = i * 3;
                            m = i * 4;
        
                            data[m] = raw[g];
                            data[m+1] = raw[g+1];
                            data[m+2] = raw[g+2];
                            data[m+3] = -127; //0xFF
      
                          }//nexti

    
                        //load texture in direct3d9
    
                         d3dlocal.init(hwnd);
                         d3dlocal.setBuffer(w,h, (unsigned char *) data);
 
                        //free memory
                          delete [] raw;
                          delete [] data;


		}	
 
		if (priv->win)
		{
			FB::Rect pos = priv->win->getWindowPosition();
			winw = pos.right-pos.left;
			winh = pos.bottom-pos.top;
//			slog("Window size is %d, %d\n",winw, winh);
		}

        //(note: might return -1 which is also true, but its not intended to be)
		if(xplayer_API_isnewimage(priv->slot) > 0)
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
                    //send texture to d3drender (if device is not ready, it returns without doing anything)
                       d3dlocal.setBuffer(w,h, img->planes[0]);
				}//ifhwnd&&w&&h

                if(img->imgfmt==IMGFMT_I420) {
					unsigned char* tmp = img->planes[1];
                    img->planes[1] = img->planes[2];
                    img->planes[2] = tmp;
                }
                xplayer_API_imagedone(priv->slot);
            }//if(img)
		}//newimage

            //render direct3d even if a video is not running; (if no video loaded you just get a blank picture)
            if (hwnd)
            {
                d3dlocal.init(hwnd); //note: safe to call every frame
                d3dlocal.render(); //render image
            }//endif

        ltime=etime;
        etime=xplayer_clock();
        Sleep(40);
        stime=xplayer_clock();
       // if((priv->run & 2) && !(xplayer_API_getstatus(priv->slot)&STATUS_PLAYER_OPENED))
        //{
        //    break;
       // }
    }//wend

    d3dlocal.release(); //release directx9 context

    slog("end of video process (2) \n");
    xplayer_API_videoprocessdone(priv->slot);
    priv->run=0;
	return NULL;
}//vidthread


