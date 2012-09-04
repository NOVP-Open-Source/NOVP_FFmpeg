/**********************************************************\ 
 Original Author: Georg Fritzsche 
 
 Created:    September 20, 2010
 License:    Dual license model; choose one of two:
 New BSD License
 http://www.opensource.org/licenses/bsd-license.php
 - or -
 GNU Lesser General Public License, version 2.1
 http://www.gnu.org/licenses/lgpl-2.1.html
 
 Copyright 2010 Georg Fritzsche,
 Firebreath development team
 \**********************************************************/

#include <list>
#include <boost/assign/list_of.hpp>
#include "JSAPI.h"
#include "Mac/PluginWindowMacCA.h"
#include "../MediaPlayer.h"

#include "SPlayerPluginMac.h"
#include "iostream.h"
#include "stdio.h"

#include "libxplayer.h"

#include <AudioToolbox/AudioQueue.h>
#include <AudioToolbox/AudioFile.h>
#include <AudioToolbox/AudioFormat.h>

struct PlayerContext 
{
    int w;
    int h;
    int n;
    
    int playerId;
    int slotId;
        
    CGContextRef hwnd;
    FB::PluginWindowMac* wnd;
    
    std::string error;
    std::string file;
    
    PlayerContext() : n(0), playerId(-1), slotId(-1), hwnd(0) {}
};

namespace 
{
    PlayerContextPtr make_context(CGContextRef hwnd, int pId, int sId )
    {
        PlayerContextPtr context(new PlayerContext);
        if(!context) throw MediaPlayer::InitializationException("failed to create context");
        context->hwnd = hwnd;
        context->playerId = pId;
        context->slotId = sId;
        return context;
    }
}


MediaPlayer::MediaPlayer(int pluginIdentifier, int slotIdentifier, int loglevel, int debugflag)
: m_context()
, m_version("")
, m_type("Mac")
{
    playerId = pluginIdentifier;
    slotId = slotIdentifier;
    logLevel = loglevel;
    FBLOG_INFO("", "MediaPlayer( pluginId: "<<pluginIdentifier<<", slotId: "<<slotIdentifier<<", logLevel: "<<loglevel<<")");
    xplayer_API_setloglevel(slotId, logLevel);
    xplayer_API_setdebug(slotId, debugflag);

    try 
    {
        m_context = make_context(0, playerId, slotId);
    }
    catch(const InitializationException& e)
    {
        m_context = PlayerContextPtr(new PlayerContext);
        m_context->playerId = playerId;
        m_context->slotId = slotId;
        m_context->error = e.what();
        throw;
    }
}

MediaPlayer::~MediaPlayer()
{
    FBLOG_INFO("", "MediaPlayer Destructor called for slot " << slotId);
    stop();
    close();
    xplayer_API_slotfree(slotId);
}

void MediaPlayer::setWindow(FB::PluginWindow* pluginWindow)
{
    CGContextRef cref=0;
    if(pluginWindow) {
        FB::PluginWindowMacCG* wnd = reinterpret_cast<FB::PluginWindowMacCG*>(pluginWindow);
        cref = (CGContextRef)wnd->getDrawingPrimitive();
        m_context->wnd = wnd;
        m_context->w = wnd->getWindowWidth();
        m_context->h = wnd->getWindowHeight();
    }
    m_context->hwnd = cref;
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
    xplayer_API_setbuffertime(slotId, 0.0);
    xplayer_API_setimage(slotId, 0, 0, IMGFMT_BGR32);
    
    xplayer_API_setvda(slotId, FLAG_VDA_FRAME);
 //   xplayer_API_setvda(slotId, FLAG_VDA_NONE);
    xplayer_API_sethwbuffersize(slotId, xplayer_API_prefilllen());
    xplayer_API_loadurl(slotId, (char*)url.c_str());
    return false;
}

bool MediaPlayer::close()
{
    FBLOG_INFO("", "slotfree called from close()");
    xplayer_API_videoprocessdone(slotId);
    xplayer_API_slotfree(slotId);
    
    FBLOG_INFO("", "slotfree done from close()");
    
    return false;
}

bool MediaPlayer::play()
{
    xplayer_API_play(slotId);
    m_context->wnd->StartAutoInvalidate(1.0/25.0);
    return false;
}

bool MediaPlayer::stop()
{
    xplayer_API_stop(slotId);
//    if(m_context->wnd)
//        m_context->wnd->StartAutoInvalidate(1.0/2.0);
    return false;
}

bool MediaPlayer::flush()
{
    xplayer_API_flush(slotId);
    return false;
}

bool MediaPlayer::pause()
{
    xplayer_API_pause(slotId);
    m_context->wnd->StartAutoInvalidate(1.0/2.0);
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

bool MediaPlayer::seek(const double percent)
{
    double length = xplayer_API_getmovielength(slotId);
    if(length>0.0 && percent>=0.0 && percent<=100.0) {
        double timepos = length * percent / 100;
        xplayer_API_seek(slotId, timepos);
    }
    return false;
}

int MediaPlayer::getstatus()
{
    return xplayer_API_getstatus(slotId);
}

bool MediaPlayer::audiodisable(bool dis)
{
//    xplayer_API_enableaudio(slotId, !dis);
    return false;
}

char* MediaPlayer::getstatusline()
{
    return xplayer_API_getstatusline(slotId);
}

int MediaPlayer::getvideoformat()
{
    video_info_t* fmt= xplayer_API_getvideoformat(slotId);
    return fmt->format;
}

int MediaPlayer::getaudioformat()
{
    audio_info_t* fmt = xplayer_API_getaudioformat(slotId);
    return fmt->format;
}

double MediaPlayer::getmovielength()
{
    return xplayer_API_getmovielength(slotId);
}

char* MediaPlayer::geturl()
{
    return xplayer_API_geturl(slotId);
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


double MediaPlayer::getrealpts()
{
    return xplayer_API_getrealpts(slotId);
}

double MediaPlayer::getcurrentpts()
{
    return xplayer_API_getcurrentpts(slotId);
}

double MediaPlayer::getfps()
{
    return xplayer_API_getfps(slotId);
}

bool MediaPlayer::rtspmode(const std::string& mode)
{
//    xplayer_API_setoptions(slotId, "rtsp_transport", mode.c_str());
    return false;
}

bool MediaPlayer::setOptions(const std::string& optname, const std::string& optvalue)
{
//    xplayer_API_setoptions(slotId, optname.c_str(), optvalue.c_str());
    return false;
}
