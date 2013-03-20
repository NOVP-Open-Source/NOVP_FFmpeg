/**********************************************************\ 
Original Author: Richard Bateman and Georg Fritzsche 

Created:    December 3, 2009
License:    Dual license model; choose one of two:
            New BSD License
            http://www.opensource.org/licenses/bsd-license.php
            - or -
            GNU Lesser General Public License, version 2.1
            http://www.gnu.org/licenses/lgpl-2.1.html

Copyright 2009 PacketPass Inc, Georg Fritzsche,
               Firebreath development team
\**********************************************************/

#ifdef FB_WIN32
#define _WIN32_DCOM
#endif

#include "NpapiTypes.h"
#ifdef FB_WIN32
#include "atlbase.h"
#endif
#include "SPlayer.h"
#ifdef FB_WIN
#include "Win/PluginWindowWin.h"
#include "MediaPlayer.h"
#endif
#ifdef FB_X11
#include "X11/PluginWindowX11.h"
#include "MediaPlayer.h"
#endif
#include "SPlayerPlugin.h"
#include "stdio.h"

#include "DOM/Document.h"

#include "URI.h"
#include "stdio.h"

void SPlayerPlugin::StaticInitialize()
{
    // Place one-time initialization stuff here; note that there isn't an absolute guarantee that
    // this will only execute once per process, just a guarantee that it won't execute again until
    // after StaticDeinitialize is called
#if FB_X11
    FBLOG_INFO("", "static initialize called");
    MediaPlayer::StaticInitialize();
#endif
#if FB_WIN
    MediaPlayer::StaticInitialize();
#endif
}

void SPlayerPlugin::StaticDeinitialize()
{
#if FB_X11
    // Place one-time deinitialization stuff here
    MediaPlayer::StaticDeinitialize();
#endif
#ifdef FB_WIN
    MediaPlayer::StaticDeinitialize();
#endif
}


SPlayerPlugin::SPlayerPlugin(int pluginIdentifier, int slotIdentifier)
  : m_player()
  , m_window(0)
{
    playerId = pluginIdentifier;
    slotId = slotIdentifier;
    long timeout = 1000;
    bool recursive = true;
//    m_timer = FB::Timer::getTimer(timeout, recursive, boost::bind(&SPlayerPlugin::timerCallback, this));
//    m_timer->start();
    FBLOG_INFO("", "SPlayerPlugin( pluginIdentifier: " << playerId << ", slotIdentifier: " << slotId << " )");
}

SPlayerPlugin::~SPlayerPlugin()
{
    FBLOG_INFO("","Destory SPlayerPlugin() slot: " << slotId);
}

void SPlayerPlugin::timerCallback(void)
{
    if(m_player)
    {
        m_player->EventProcess();
    }
//    callback->Invoke("", FB::variant_list_of());
    // TODO: delete This timer
}

FB::JSAPIPtr SPlayerPlugin::createJSAPI()
{
    int loglevel = 0;
    int debugflag = 0;
    std::string logfile;
    boost::optional<std::string> par = getParam("loglevel");
    if(par.is_initialized())
    {
        loglevel = atoi(par.get().c_str());
    }
    par = getParam("debugflag");
    if(par.is_initialized())
    {
        debugflag = atoi(par.get().c_str());
    }
    par = getParam("logfile");
    if(par.is_initialized())
    {
        logfile = par.get();
    }
    FBLOG_INFO("","SPlayerPlugin log level: " << loglevel);
    m_player = SPlayerPtr(new SPlayer(m_host, playerId, slotId, loglevel, debugflag, logfile));

    for (FB::VariantMap::const_iterator it = m_params.begin(); it != m_params.end(); ++it)
    {
        std::string key(it->first);
        par = getParam(key);
        if(par.is_initialized())
        {
            if(key == "src")
            {
                const FB::URI& uri = FB::URI::fromString(par.get());
                fprintf(stderr,"host: %s src: %s\n",uri.domain.c_str(),par.get().c_str());
            }

//            m_player->setParam(key, par.get());
            m_player->SetProperty(key, par);
        }
    }
    m_player->setWindow(m_window);
    return m_player;
}

bool SPlayerPlugin::onMouseDown(FB::MouseDownEvent *evt, FB::PluginWindow*)
{
#if 0
    if(m_player)
        return m_player->onMouseDown();
#endif
    return false;
}

bool SPlayerPlugin::onMouseUp(FB::MouseUpEvent *evt, FB::PluginWindow*)
{
    return false;
}

bool SPlayerPlugin::onMouseMove(FB::MouseMoveEvent *evt, FB::PluginWindow*)
{
    return false;
}

bool SPlayerPlugin::onWindowAttached(FB::AttachedEvent* evt, FB::PluginWindow* win)
{
    m_window = win;
    if(m_player)
        m_player->setWindow(win);
    return true;
}

bool SPlayerPlugin::onWindowDetached(FB::DetachedEvent* evt, FB::PluginWindow* win)
{
    if(m_player)
        m_player->setWindow(NULL);
    return true;
}

