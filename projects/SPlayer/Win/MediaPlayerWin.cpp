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
#include <atlbase.h>
#include <atlcom.h>
#include <atlstr.h>
#include <dshow.h>
#include "JSAPI.h"
#include "Win/PluginWindowWin.h"
#include "../MediaPlayer.h"
#include "error_mapping.h"

#include "VoDirectX.h"
#include "AoDirect.h"
#include "libxplayer.h"

DWORD WINAPI MyThreadFunction( LPVOID lpParam );
DWORD WINAPI NetThreadFunction( LPVOID lpParam );

struct PlayerContext;

typedef struct {
    voconf_t* voconf;
    aoconf_t* aoconf;
    int run;
    int mode;
    int size;
    mp_image_t*	img;
    std::string file;
    double vpts;
    long long int timestamp;
    double seekpos;
    double sync;
    double startpts;
    double dpts;
    double pts;
    bool seekflag;

	char* filename;
	void* playpriv;
	int status;
	int pause;
	int stop;
} MYDATA, *PMYDATA;

#define VO_THREAD	0
#define NET_THREAD	1
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
			CloseHandle(context->hThreadArray[NET_THREAD]);
		}
		
		context->pDataArray->file=context->file;
		context->voconf = preinit(1,context->hwnd);
//		int res1 = preinit(NULL,NULL);


        context->pDataArray->run = 1;
        context->pDataArray->mode = 1;
        context->pDataArray->voconf= context->voconf;
        context->pDataArray->sync = 0.0;
        context->pDataArray->startpts = 0.0;
        context->pDataArray->seekpos = 0.0;
        context->pDataArray->seekflag = false;
        context->pDataArray->pts = 0.0;
        context->pDataArray->vpts = 0.0;
        context->pDataArray->dpts = 0.0;
		context->pDataArray->status=0;
		context->pDataArray->pause=0;
		context->pDataArray->stop=0;
		context->pDataArray->filename=strdup(context->file.c_str());
		context->pDataArray->playpriv=NULL;
        context->n++;

		context->hThreadArray[VO_THREAD] = CreateThread(
            NULL,                       // default security attributes
            0,                          // use default stack size  
            MyThreadFunction,           // thread function name
            context->pDataArray,        // argument to thread function 
            0,                          // use default creation flags 
            &context->dwThreadIdArray[VO_THREAD]); // returns the thread identifier 
        context->hThreadArray[NET_THREAD] = CreateThread(
            NULL,                       // default security attributes
            0,                          // use default stack size  
            NetThreadFunction,          // thread function name
            context->pDataArray,        // argument to thread function 
            0,                          // use default creation flags 
            &context->dwThreadIdArray[NET_THREAD]); // returns the thread identifier 

#if 0
		char* vo_msg = getmsg(context->voconf);
		LPVOID lpMsgBuf;
		LPVOID lpBuf;
		lpMsgBuf = (LPVOID)LocalAlloc(LMEM_ZEROINIT, 8192 * sizeof(TCHAR));
		lpBuf = (LPVOID)LocalAlloc(LMEM_ZEROINIT, (strlen(vo_msg)+1) * sizeof(TCHAR));
		MultiByteToWideChar(CP_ACP,0,vo_msg,-1,(LPWSTR)lpBuf,strlen(vo_msg)+1);
		StringCchPrintf((LPTSTR)lpMsgBuf,8192,TEXT("%s"), lpBuf);
		free(vo_msg);
		MessageBox(NULL,(LPTSTR)lpMsgBuf, L"Message", MB_OK);	
		LocalFree(lpMsgBuf);
		LocalFree(lpBuf);
#endif
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
}

MediaPlayer::~MediaPlayer()
{
    HRESULT hr;

    m_context->pDataArray->run = 0;
    m_context->pDataArray->mode = 0;

    stop();
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

bool MediaPlayer::play(const std::string& file_)
{
	m_context->file = file_;
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

int MediaPlayer::status()
{
	if(m_context && m_context->pDataArray)
	{
		return m_context->pDataArray->status;
	}
    return 0;
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
		CloseHandle(m_context->hThreadArray[NET_THREAD]);
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

bool MediaPlayer::setsync(const double sync)
{
    if(m_context && m_context->pDataArray) {
        m_context->pDataArray->startpts=sync;
        return true;
    }
    return false;
}

double MediaPlayer::getsync(const double sync)
{
    if(m_context && m_context->pDataArray) {
        return m_context->pDataArray->startpts;
    }
    return 0.0;
}

double MediaPlayer::getvpts(void)
{
    if(m_context && m_context->pDataArray) {
        return m_context->pDataArray->vpts;
    }
    return 0.0;
}

double MediaPlayer::getdpts(void)
{
    if(m_context && m_context->pDataArray) {
        return m_context->pDataArray->dpts;
    }
    return 0.0;
}

double MediaPlayer::getpts(void)
{
    if(m_context && m_context->pDataArray) {
        return m_context->pDataArray->pts;
    }
    return 0.0;
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

DWORD WINAPI _NetThreadFunction( LPVOID lpParam )
{
    PMYDATA pDataArray;
    pDataArray = (PMYDATA)lpParam;

	char filename[] = "c:\\Documents and Settings\\aotvos\\hc\\w\\work\\fplayer2\\a.avi";
//	char filename[] = "http://axis1.ica.vpn.av.hu/mjpg/video.mjpg";

	void* playpriv;
	int avloglevel = 0;
	int alen = 0;
	unsigned char* adata = NULL;
	double pts;

	if(!pDataArray->file.c_str())
	{
	        return 0;
	}
	if(pDataArray->aoconf)
		audio_uninit(pDataArray->aoconf, 0);
	playpriv = xplayer_init(avloglevel);
	xplayer_open(playpriv, pDataArray->file.c_str(), 0);
	xplayer_setimage(playpriv, 320, 200, IMGFMT_BGR32);
#if 0
	if(xplayer_audiorate(playpriv))
	{
		pDataArray->aoconf = audio_init(xplayer_audiorate(playpriv), xplayer_audioch(playpriv), xplayer_audiofmt(playpriv), 0);
		if(!audio_inited(pDataArray->aoconf))
		{
			pDataArray->aoconf=NULL;
		}
	}
#endif
	while(pDataArray->run) {
	        if(pDataArray->seekflag)
	        {
	                pDataArray->seekflag=false;
	                xplayer_seek(playpriv,pDataArray->seekpos);
	                pDataArray->startpts=xplayer_clock(playpriv)-pDataArray->seekpos;
	                pDataArray->vpts=0.0;
	        }
	        if(pDataArray->startpts==0.0)
	        {
	                pDataArray->startpts=xplayer_clock(playpriv);
	        }
	        pts = xplayer_clock(playpriv)-pDataArray->startpts+pDataArray->sync;
	        pDataArray->pts=pts;
	        pDataArray->dpts=pDataArray->vpts-pts;
	        if(((pts>pDataArray->vpts && pDataArray->vpts>0.0) || (pDataArray->vpts==0.0 || !pDataArray->img))) {
	            if(xplayer_loop(playpriv)) {
	                break;
	            } else {
	                if(xplayer_isnewimage(playpriv)) {
	                    pDataArray->vpts=xplayer_vpts(playpriv);
	                    pDataArray->img = xplayer_getimage(playpriv);
	                }
	                if((alen=xplayer_audiolen(playpriv)))
	                {
	                    adata=(unsigned char*)xplayer_audio(playpriv);
	                    if(pDataArray->aoconf)
	                    {
#if 0
	                        audio_play(pDataArray->aoconf, adata, alen, 1);
#endif
	                    }
	                    free(adata);
	                }
	            }
	        }
	}
	if(pDataArray->aoconf)
		audio_uninit(pDataArray->aoconf, 0);
	pDataArray->aoconf=NULL;
	xplayer_close(playpriv);
	xplayer_uninit(playpriv);
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

DWORD WINAPI NetThreadFunction( LPVOID lpParam )
{
    PMYDATA priv;
    priv = (PMYDATA)lpParam;

		char* filename = NULL;

        mp_image_t * img = NULL;
        mp_image_t * pimg = NULL;
        mp_image_t * bimg = NULL;

        int avloglevel = 0;

        int arate = 0;
        int ach = 0;
        unsigned int afmt = 0;
        unsigned char* adata = NULL;
        int alen = 0;

        int newimage = 0;
        int waitimage = 0;
        double timestamp=0.0;
        double pts=0.0;
        long long int startts = 0;

        priv->run=1;

        if(!priv->filename) {
            return NULL;
        }
        priv->playpriv = xplayer_init(avloglevel);
        bimg=alloc_mpi(320, 200, IMGFMT_BGR32);
        xplayer_setimage(priv->playpriv, bimg->w, bimg->h, bimg->imgfmt);
        while(priv->run) {
            newimage = 0;
            if(priv->stop) {
                if(filename) {
                    free(filename);
                    filename=NULL;
                    xplayer_close(priv->playpriv);
                    img=NULL;
                }
                priv->stop=0;
                priv->status=0;
                waitimage=0;
            }
            if((!priv->filename && filename) || (priv->filename && !filename)) {
                priv->status=0;
                waitimage=0;
            }
            if(priv->filename && filename && strcmp(priv->filename,filename)) {
                priv->status=0;
                waitimage=0;
            }
            if(priv->status<=0) {
                priv->img=win_draw(priv->img,bimg);
                Sleep(40);
            }
            if(!priv->status && !waitimage) {
                if(filename) {
                    free(filename);
                    xplayer_close(priv->playpriv);
                    img=NULL;
                }
                if(priv->filename) {
                    filename=strdup(priv->filename);
                    if(xplayer_open(priv->playpriv, filename, 0)) {
                        priv->status=-1;
                        free(filename);
                        filename=NULL;
                    }
                    if(xplayer_audiorate(priv->playpriv)) {
                        arate = xplayer_audiorate(priv->playpriv);
                        ach =  xplayer_audioch(priv->playpriv);
                        afmt = xplayer_audiofmt(priv->playpriv);
                    }
                }
                waitimage=1;
            }
            if(priv->seekflag && filename) {
                priv->seekflag=false;
                xplayer_seek(priv->playpriv, priv->seekpos);
                priv->startpts=xplayer_clock(priv->playpriv)-priv->seekpos;
                priv->vpts=0.0;
                priv->pause=0;
                priv->stop=0;
                if(priv->status==2)
                    priv->status=1;
            }
            if(priv->startpts==0.0) {
                priv->startpts=xplayer_clock(priv->playpriv);
            }
            pts=xplayer_clock(priv->playpriv)-priv->startpts+priv->sync;
            priv->pts=pts;
            priv->dpts=priv->vpts-pts;
            if(!filename || priv->status<0) {
            } else if(xplayer_read(priv->playpriv)) {
                priv->status=-1;
                if(filename) {
                    free(filename);
                    xplayer_close(priv->playpriv);
                    img=NULL;
                }
            } else if(xplayer_loop(priv->playpriv)) {
                priv->status=-1;
                if(filename) {
                    free(filename);
                    xplayer_close(priv->playpriv);
                    img=NULL;
                }
            } else {
                if(xplayer_isnewimage(priv->playpriv)) {
                    priv->vpts=xplayer_vpts(priv->playpriv);
                    img=xplayer_getimage(priv->playpriv);
                    newimage=1;
                    waitimage=0;
                    if(priv->status<=0)
                        priv->status=1;
                }
                if((alen=xplayer_audiolen(priv->playpriv))) {
                    adata=(unsigned char*)xplayer_audio(priv->playpriv);
                    free(adata);
                }
            }
            if(img && newimage && !priv->stop) {
                if(priv->pause) {
                    priv->status=2;
                    if(!pimg) {
                        pimg=alloc_mpi(img->w, img->h, img->imgfmt);
                        copy_mpi(pimg, img);
                    }
                    priv->img=win_draw(priv->img,pimg);
                } else {
                    priv->img=win_draw(priv->img,img);
                    if(pimg) {
                        free_mp_image(pimg);
                        pimg=NULL;
                    }
                }
            }
        }
        priv->status=-1;
        priv->run=0;
        if(bimg)
            free_mp_image(bimg);
        if(pimg)
            free_mp_image(pimg);
        if(img) {
            pimg=alloc_mpi(img->w, img->h, img->imgfmt);
            priv->img=win_draw(priv->img,pimg);
            free_mp_image(pimg);
        }
        if(filename)
            free(filename);
        xplayer_close(priv->playpriv);
        xplayer_uninit(priv->playpriv);
        return NULL;
    }