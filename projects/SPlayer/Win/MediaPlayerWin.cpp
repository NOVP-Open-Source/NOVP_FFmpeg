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
    mp_image_t*	img;
    std::string file;

    char* filename;
    int status;
    int pause;
    int stop;
} MYDATA, *PMYDATA;

#define VO_THREAD	0
#define AO_THREAD	1
#define MAX_THREADS	2

struct PlayerContext 
{
    voconf_t* voconf;
    int w;
    int h;
	int n;

    PMYDATA pDataArray;
    DWORD   dwThreadIdArray[MAX_THREADS];
	HANDLE  hThreadArray[MAX_THREADS];

    HWND hwnd;
	int slot;
    std::string error;
    std::string file;

    PlayerContext() : pDataArray(0), hwnd(0), voconf(0), n(0) {}
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
	context->voconf = preinit(1,context->hwnd);
        context->pDataArray->run = 1;
        context->pDataArray->mode = 1;
        context->pDataArray->voconf= context->voconf;
		context->pDataArray->status=0;
		context->pDataArray->pause=0;
		context->pDataArray->stop=0;
		context->pDataArray->filename=strdup(context->file.c_str());
        context->n++;

	context->hThreadArray[AO_THREAD] = CreateThread(
            NULL,                       // default security attributes
            0,                          // use default stack size  
            AoThreadFunction,           // thread function name
            context->pDataArray,        // argument to thread function 
            0,                          // use default creation flags 
            &context->dwThreadIdArray[AO_THREAD]); // returns the thread identifier 
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
}

void MediaPlayer::StaticDeinitialize()
{
}

void MediaPlayer::setWindow(FB::PluginWindow* pluginWindow)
{
    HRESULT hr;
    HWND hwnd = 0;

    if(pluginWindow) {
        FB::PluginWindowWin* wnd = reinterpret_cast<FB::PluginWindowWin*>(pluginWindow);
        hwnd = wnd->getHWND();
    }
    
    if(m_context->hwnd) {
    }
    
    m_context->hwnd = hwnd;
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
//    xplayer_API_setimage(slotId, 0, 0, IMGFMT_BGR32);
    xplayer_API_setimage(slotId, 0, 0, IMGFMT_RGB24);
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
	if(m_context && m_context->pDataArray)
	{
		m_context->pDataArray->pause=1;
		return true;
	}
    return false;
}

bool MediaPlayer::stop()
{
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
    return xplayer_API_getstatusline(slotId);
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

DWORD WINAPI AoThreadFunction( LPVOID lpParam )
{
	PMYDATA priv;
    priv = (PMYDATA)lpParam;
	char abuffer[0x10000];

	priv->run|=1;
    while(priv->run) {
		xplayer_API_getaudio(abuffer, sizeof(abuffer));
        Sleep(40);
        if((priv->run & 2))
        {
             break;
        }
    }

    return NULL;
}

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
                                     
    priv->run|=1;

    while(priv->run) {
		if(hwnd != priv->hwnd) {
			hwnd=0;
			vo_uninit(priv->voconf);
		}
		if(priv->hwnd) {
			hwnd = priv->hwnd;
		}	
        if(xplayer_API_isnewimage(priv->slot))
        {
			xplayer_API_getimage(priv->slot, &img);
            if(img) {
				w=img->w;
                h=img->h;
				fmt=img->imgfmt;
  				vo_config(priv->voconf, w,h,w,h,0,NULL,fmt);
            } else if(img && (w!=img->w || h!=img->h)) {
                w=img->w;
                h=img->h;
				fmt=img->imgfmt;
				vo_config(priv->voconf, w,h,w,h,0,NULL,fmt);
            }
            if(img) {
                if(img->imgfmt==IMGFMT_I420) {
					unsigned char* tmp = img->planes[1];
                    img->planes[1] = img->planes[2];
                    img->planes[2] = tmp;
                }
 				if(hwnd) {
					vo_draw_frame(priv->voconf, priv->img->planes);
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
        Sleep(40000);
        stime=xplayer_clock();
        if((priv->run & 2) && !(xplayer_API_getstatus(priv->slot)&STATUS_PLAYER_OPENED))
        {
            break;
        }
    }
    if(hwnd) {
		vo_uninit(priv->voconf);
	}
    xplayer_API_videoprocessdone(priv->slot);
    priv->run=0;
	return NULL;
}





