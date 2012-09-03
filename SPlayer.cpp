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

#include <boost/assign.hpp>
#include "SPlayer.h"
#include "MediaPlayer.h"
#include "DOM/Window.h"
#include "variant_list.h"
#include <stdio.h>
#include <string.h>
#include <iostream>

SPlayer::SPlayer(FB::BrowserHostPtr host, int pluginId, int sId, int loglevel, int debugflag) 
  : m_host(host)
  , m_player()
  , m_valid(false)
  , m_loglevel(0)
  , m_url("")
  , m_autoplay(false)
  , m_resetprop(false)
  , m_inited(false)

  , m_classid("")
  , m_codebase("")
  , m_id("")
  , m_height(0)
  , m_width(0)
  , m_type("")
  , m_src("")
  , m_qtsrc("")
  , m_enablejavascript(false)
  , m_kioskmode(false)
  , m_controller(false)
  , m_scale("")
  , m_href("")

{

    playerId = pluginId;
    slotId = sId;
    logLevel = loglevel;
    using FB::make_method;
    using FB::make_property;
    memset(qtstatus,0,sizeof(qtstatus));

    registerMethod  ("play",                    make_method  (this, &SPlayer::play));
    registerMethod  ("pause",                   make_method  (this, &SPlayer::pause));
    registerMethod  ("open",                    make_method  (this, &SPlayer::open));
    registerMethod  ("close",                   make_method  (this, &SPlayer::close));
    registerMethod  ("seek",                    make_method  (this, &SPlayer::seek));
    registerMethod  ("seekpos",                 make_method  (this, &SPlayer::seekpos));
    registerMethod  ("seekrel",                 make_method  (this, &SPlayer::seekrel));
    registerMethod  ("getvideoformat",          make_method  (this, &SPlayer::getvideoformat));
    registerMethod  ("getaudioformat",          make_method  (this, &SPlayer::getaudioformat));
    registerMethod  ("getmovielength",          make_method  (this, &SPlayer::getmovielength));
    registerMethod  ("geturl",                  make_method  (this, &SPlayer::geturl));
    registerMethod  ("stop",                    make_method  (this, &SPlayer::stop));
    registerMethod  ("flush",                   make_method  (this, &SPlayer::flush));
    registerMethod  ("volume",                  make_method  (this, &SPlayer::volume));
    registerMethod  ("getvolume",               make_method  (this, &SPlayer::GetVolume));
    registerMethod  ("getstatus",               make_method  (this, &SPlayer::getstatus));
    registerMethod  ("getcurrentpts",           make_method  (this, &SPlayer::getcurrentpts));
    registerMethod  ("getrealpts",              make_method  (this, &SPlayer::getrealpts));
    registerMethod  ("getfps",                  make_method  (this, &SPlayer::getfps));
    registerMethod  ("setOptions",              make_method  (this, &SPlayer::setOptions));
    registerMethod  ("audiodisable",            make_method  (this, &SPlayer::audiodisable));
    registerMethod  ("getstatusline",           make_method  (this, &SPlayer::getstatusline));

/// Compatible functions to QuickTime plugin:
    registerMethod  ("GetPluginStatus",         make_method  (this, &SPlayer::GetPluginStatus));
    registerMethod  ("Play",                    make_method  (this, &SPlayer::Play));
    registerMethod  ("Stop",                    make_method  (this, &SPlayer::Stop));
    registerMethod  ("Rewind",                  make_method  (this, &SPlayer::Rewind));
    registerMethod  ("Step",                    make_method  (this, &SPlayer::Step));
    registerMethod  ("GetTimeScale",            make_method  (this, &SPlayer::GetTimeScale));
    registerMethod  ("GetDuration",             make_method  (this, &SPlayer::GetDuration));
    registerMethod  ("SetURL",                  make_method  (this, &SPlayer::SetURL));
    registerMethod  ("GetURL",                  make_method  (this, &SPlayer::GetURL));
    registerMethod  ("SetTime",                 make_method  (this, &SPlayer::SetTime));
    registerMethod  ("GetTime",                 make_method  (this, &SPlayer::GetTime));
    registerMethod  ("SetRate",                 make_method  (this, &SPlayer::SetRate));
    registerMethod  ("GetRate",                 make_method  (this, &SPlayer::GetRate));
    registerMethod  ("SetAutoPlay",             make_method  (this, &SPlayer::SetAutoPlay));
    registerMethod  ("GetAutoPlay",             make_method  (this, &SPlayer::GetAutoPlay));
    registerMethod  ("SetVolume",               make_method  (this, &SPlayer::SetVolume));
    registerMethod  ("GetVolume",               make_method  (this, &SPlayer::GetVolume));
    registerMethod  ("SetMute",                 make_method  (this, &SPlayer::SetMute));
    registerMethod  ("GetMute",                 make_method  (this, &SPlayer::GetMute));
    registerMethod  ("SetResetPropertiesOnReload", make_method  (this, &SPlayer::SetResetPropertiesOnReload));
    registerMethod  ("GetResetPropertiesOnReload", make_method  (this, &SPlayer::GetResetPropertiesOnReload));

    registerEvent   ("onqt_progress");
    registerEvent   ("onTestEvent");

/// ------------------------------------------------------

    registerEvent("onplaylistChanged");
    registerEvent("oncurrentItemChanged");

    registerProperty("loglevel",                make_property(this, &SPlayer::getloglevel, &SPlayer::setloglevel));

    registerProperty("id",                      make_property(this, &SPlayer::GetID, &SPlayer::SetID));
    registerProperty("width",                   make_property(this, &SPlayer::GetWidth, &SPlayer::SetWidth));
    registerProperty("height",                  make_property(this, &SPlayer::GetHeight, &SPlayer::SetHeight));
    registerProperty("type",                    make_property(this, &SPlayer::GetType, &SPlayer::SetType));
    registerProperty("codebase",                make_property(this, &SPlayer::GetCodeBase, &SPlayer::SetCodeBase));
    registerProperty("classid",                 make_property(this, &SPlayer::GetClassID, &SPlayer::SetClassID));
    registerProperty("src",                     make_property(this, &SPlayer::GetSrc, &SPlayer::SetSrc));
    registerProperty("qtsrc",                   make_property(this, &SPlayer::GetQTSrc, &SPlayer::SetQTSrc));
    registerProperty("enablejavascript",        make_property(this, &SPlayer::GetEnableJavascript, &SPlayer::SetEnableJavascript));
    registerProperty("kioskmode",               make_property(this, &SPlayer::GetKioskMode, &SPlayer::SetKioskMode));
    registerProperty("controller",              make_property(this, &SPlayer::GetController, &SPlayer::SetController));
    registerProperty("autoplay",                make_property(this, &SPlayer::GetAutoPlay, &SPlayer::SetAutoPlay));
    registerProperty("volume",                  make_property(this, &SPlayer::GetVolume, &SPlayer::SetVolume));
    registerProperty("scale",                   make_property(this, &SPlayer::GetScale, &SPlayer::SetScale));
    registerProperty("href",                    make_property(this, &SPlayer::GetHRef, &SPlayer::SetHRef));

    FBLOG_INFO("","SPLayer.cpp log level: " << logLevel);

    try 
    {
        m_player  = MediaPlayerPtr(new MediaPlayer(playerId, slotId, logLevel, debugflag));
        m_valid   = true;
    } 
    catch(const MediaPlayer::InitializationException&) 
    {
        m_host->htmlLog("failed to initialize media player");
    }
}

SPlayer::~SPlayer()
{
    m_player.reset();
}

bool SPlayer::pause()
{
    return m_player->pause();
}

void SPlayer::setloglevel(const FB::variant& arg)
{
    m_loglevel = arg.convert_cast<int>();
    m_player->setloglevel(m_loglevel);
}

int SPlayer::getloglevel()
{
    return m_loglevel;
}

bool SPlayer::open(const std::string& entry)
{
    if(!entry.length())
        return false;
    m_player->open(entry);
    return true;

}

bool SPlayer::close()
{
    m_player->close();
    return true;
}


bool SPlayer::play(const FB::CatchAll& catchall)
{
    return m_player->play();
}

bool SPlayer::stop()
{
    return m_player->stop();
}

bool SPlayer::flush()
{
    return m_player->flush();
}


bool SPlayer::seek(const FB::variant& arg)
{
    double pos = arg.convert_cast<double>();
    if (pos < 0.0) {
        return false;
    }
    m_player->seek(pos);
    return true;
}

bool SPlayer::seekpos(const FB::variant& arg)
{
    double pos = arg.convert_cast<double>();
    if (pos < 0.0) {
        return false;
    }
    m_player->seekpos(pos);
    return true;
}

bool SPlayer::seekrel(const FB::variant& arg)
{
    double pos = arg.convert_cast<double>();
    if (pos == 0.0) {
        return false;
    }
    m_player->seekrel(pos);
    return true;
}

bool SPlayer::audiodisable(const FB::variant& arg)
{
    bool dis = arg.convert_cast<bool>();
    m_player->audiodisable(dis);
    return true;
}

bool SPlayer::rtspmode(const std::string& entry)
{
    if(!entry.length())
        return false;
    m_player->rtspmode(entry);
    return true;
}

bool SPlayer::setOptions(const std::string& optname, const std::string& optvalue)
{
    return m_player->setOptions(optname, optvalue);
}

int SPlayer::getvideoformat()
{
//    return m_player->getvideoformat();
    return 0;
}

int SPlayer::getaudioformat()
{
//    return m_player->getaudioformat();
    return 0;
}

double SPlayer::getmovielength()
{
    return m_player->getmovielength();
}

char* SPlayer::geturl()
{
//    return m_player->geturl();
    return NULL;
}

char* SPlayer::getstatusline()
{
    char* l = m_player->getstatusline();
    strncpy(statusline,l,1024);
    free(l);
    return statusline;
}

bool SPlayer::volume(const FB::variant& vol)
{
    int v = vol.convert_cast<int>();
    return m_player->volume(v);
}

double SPlayer::getcurrentpts()
{
    return m_player->getcurrentpts();
}

double SPlayer::getrealpts()
{
//    return m_player->getrealpts();
    return 0.0;
}

double SPlayer::getfps()
{
//    return m_player->getfps();
    return 0.0;
}

int SPlayer::getstatus()
{
    return m_player->getstatus();
}


void SPlayer::setWindow(FB::PluginWindow* win)
{
    m_player->setWindow(win);
    if(win && !m_inited)
    {
        m_inited = true;
        m_player->open(m_qtsrc);
        if(m_autoplay)
        {
            m_player->play();
        }
    }
}

/// *******************************************************************
/// Compatible functions to QuickTime plugin:
/// *******************************************************************

char* SPlayer::GetPluginStatus()
{
    int st = m_player->getstatus();
    int err = 0;
    if(!(st & STATUS_PLAYER_CONNECT)) {
        snprintf(qtstatus,sizeof(qtstatus),"Waiting");
        return qtstatus;
    }
    if(!(st & STATUS_PLAYER_OPENED)) {
        snprintf(qtstatus,sizeof(qtstatus),"Loading");
        return qtstatus;
    }
    if((st & STATUS_PLAYER_SEEK)) {
        snprintf(qtstatus,sizeof(qtstatus),"Loading");
        return qtstatus;
    }
    if((st & STATUS_PLAYER_OPENED)) {
        snprintf(qtstatus,sizeof(qtstatus),"Playable");
        return qtstatus;
    }
    snprintf(qtstatus,sizeof(qtstatus),"Error %d",err);
    return qtstatus;
}

bool SPlayer::Play()
{
    return m_player->play();
}

bool SPlayer::Stop()
{
    return m_player->pause();
}

bool SPlayer::Rewind()
{
    m_player->pause();
    m_player->seekpos(0.0);
    return false;
}

bool SPlayer::Step(const FB::variant& arg)
{
    int timepos = arg.convert_cast<int>();
    double pos = (double)timepos / (double)GetTimeScale();
    m_player->pause();
    m_player->seekrel(pos);
    return true;
}

int SPlayer::GetTimeScale()
{
    return 30;
}

int SPlayer::GetDuration()
{
    int ret;
    ret = m_player->getmovielength() * GetTimeScale();
    return ret;
    return m_player->getmovielength() * GetTimeScale();
}

bool SPlayer::SetURL(const std::string& entry)
{
    if(!entry.length())
        return false;
    m_url = entry;
    m_player->open(entry);
    if(m_autoplay)
        m_player->play();
    return true;
}

std::string SPlayer::GetURL()
{
    return m_url;
}

bool SPlayer::SetTime(const FB::variant& arg)
{
    int timepos = arg.convert_cast<int>();
    if (timepos < 0) {
        return false;
    }
    double pos = (double)timepos / (double)GetTimeScale();
    m_player->seekpos(pos);
    return true;
}

int  SPlayer::GetTime()
{
    int ret;
    ret = m_player->getcurrentpts() * GetTimeScale();
    return ret;
    return m_player->getcurrentpts() * GetTimeScale();
}

bool SPlayer::SetRate(const FB::variant& arg)
{
    return false;
}

float SPlayer::GetRate()
{
    int st = m_player->getstatus();
    if((st & STATUS_PLAYER_OPENED) && !(st & STATUS_PLAYER_PAUSE))
        return 1.0;
    return 0.0;
}

void SPlayer::SetVolume(const FB::variant& vol)
{
    int v = vol.convert_cast<int>();
    m_player->volume(v / 2.55);
}

int  SPlayer::GetVolume()
{
    return m_player->getvolume() * 2.55;
}

bool SPlayer::SetMute(const FB::variant& flag)
{
    bool f = flag.convert_cast<bool>();
    return m_player->mute(f);
}

bool SPlayer::GetMute()
{
    return m_player->getmute();
}

bool SPlayer::SetResetPropertiesOnReload(const FB::variant& arg)
{
    m_resetprop = arg.convert_cast<bool>();
    return true;
}

bool SPlayer::GetResetPropertiesOnReload()
{
    return m_resetprop;
}

std::string SPlayer::GetID()
{
    return m_id;
}

void SPlayer::SetID(const std::string& val)
{
    m_id = val;
}

int SPlayer::GetWidth()
{
    return m_width;
}

void SPlayer::SetWidth(const FB::variant& arg)
{
    m_width = arg.convert_cast<int>();
}

int SPlayer::GetHeight()
{
    return m_height;
}

void SPlayer::SetHeight(const FB::variant& arg)
{
    m_height = arg.convert_cast<int>();
}

std::string SPlayer::GetType()
{
    return m_type;
}

void SPlayer::SetType(const std::string& val)
{
    m_type = val;
}

std::string SPlayer::GetCodeBase()
{
    return m_codebase;
}

void SPlayer::SetCodeBase(const std::string& val)
{
    m_codebase = val;
}

std::string SPlayer::GetClassID()
{
    return m_classid;
}

void SPlayer::SetClassID(const std::string& val)
{
    m_classid = val;
}

std::string SPlayer::GetSrc()
{
    return m_src;
}

void SPlayer::SetSrc(const std::string& val)
{
    m_src = val;
}

std::string SPlayer::GetQTSrc()
{
    return m_qtsrc;
}

void SPlayer::SetQTSrc(const std::string& val)
{
    m_qtsrc = val;
}

bool SPlayer::GetEnableJavascript()
{
    return m_enablejavascript;
}

void SPlayer::SetEnableJavascript(const FB::variant& arg)
{
    m_enablejavascript = arg.convert_cast<bool>();
}

bool SPlayer::GetKioskMode()
{
    return m_kioskmode;
}

void SPlayer::SetKioskMode(const FB::variant& arg)
{
    m_kioskmode = arg.convert_cast<bool>();
}

bool SPlayer::GetController()
{
    return m_controller;
}

void SPlayer::SetController(const FB::variant& arg)
{
    m_controller = arg.convert_cast<bool>();
}

bool SPlayer::GetAutoPlay()
{
    return m_autoplay;
}

void SPlayer::SetAutoPlay(const FB::variant& arg)
{
    m_autoplay = arg.convert_cast<bool>();
}

std::string SPlayer::GetScale()
{
    return m_scale;
}

void SPlayer::SetScale(const std::string& val)
{
    m_scale = val;
}

std::string SPlayer::GetHRef()
{
    return m_href;
}

void SPlayer::SetHRef(const std::string& val)
{
    m_href = val;
}

void SPlayer::EventProcess()
{
    FireEvent("onqt_progress", FB::variant_list_of(m_href));
    FireEvent("onTestEvent", FB::variant_list_of(m_href));
}

