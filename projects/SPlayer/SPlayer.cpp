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

SPlayer::SPlayer(FB::BrowserHostPtr host, int pluginId, int sId, int loglevel, int debugflag, std::string logfile) 
  : m_host(host)
  , m_player()
  , m_valid(false)
  , m_loglevel(0)
  , m_logfile(logfile)
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
  , m_qver("1.0.0")

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

    registerMethod  ("SetGroupId",              make_method  (this, &SPlayer::SetGroupId));
    registerMethod  ("GetGroupId",              make_method  (this, &SPlayer::GetGroupId));
    registerMethod  ("SetTimeShift",            make_method  (this, &SPlayer::SetTimeShift));
    registerMethod  ("GroupPlay",               make_method  (this, &SPlayer::GroupPlay));
    registerMethod  ("GroupStop",               make_method  (this, &SPlayer::GroupStop));
    registerMethod  ("GroupSetTime",            make_method  (this, &SPlayer::GroupSetTime));

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
    registerMethod  ("GetQuickTimeVersion",     make_method  (this, &SPlayer::GetQuickTimeVersion));

//    registerEvent   ("onqt_progress");
//    registerEvent   ("onTestEvent");

/// ------------------------------------------------------

//    registerEvent("onplaylistChanged");
//    registerEvent("oncurrentItemChanged");

    registerProperty("loglevel",                make_property(this, &SPlayer::getloglevel, &SPlayer::setloglevel));

    registerProperty("id",                      make_property(this, &SPlayer::GetID, &SPlayer::SetID));
    registerProperty("width",                   make_property(this, &SPlayer::GetWidth, &SPlayer::SetWidth));
    registerProperty("height",                  make_property(this, &SPlayer::GetHeight, &SPlayer::SetHeight));
    registerProperty("type",                    make_property(this, &SPlayer::GetType, &SPlayer::SetType));
    registerProperty("codebase",                make_property(this, &SPlayer::GetCodeBase, &SPlayer::SetCodeBase));
    registerProperty("classid",                 make_property(this, &SPlayer::GetClassID, &SPlayer::SetClassID));
//    registerProperty("src",                     make_property(this, &SPlayer::GetSrc, &SPlayer::SetSrc));
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
        m_player  = MediaPlayerPtr(new MediaPlayer(playerId, slotId, logLevel, debugflag, logfile));
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
    plugincall(slotId, 2);
    return m_player->pause();
}

void SPlayer::setloglevel(const FB::variant& arg)
{
    plugincall(slotId, 44);
    m_loglevel = arg.convert_cast<int>();
    m_player->setloglevel(m_loglevel);
}

int SPlayer::getloglevel()
{
    plugincall(slotId, 44);
    return m_loglevel;
}

bool SPlayer::open(const std::string& entry)
{
    plugincall(slotId, 3);
    if(!entry.length())
        return false;
    m_player->open(entry);
    return true;

}

bool SPlayer::close()
{
    plugincall(slotId, 4);
    m_player->close();
    return true;
}


bool SPlayer::play(const FB::CatchAll& catchall)
{
    plugincall(slotId, 1);
    return m_player->play();
}

bool SPlayer::GroupPlay()
{
    return m_player->groupplay();
}

bool SPlayer::GroupStop()
{
    return m_player->groupstop();
}

bool SPlayer::stop()
{
    plugincall(slotId, 12);
    return m_player->stop();
}

bool SPlayer::flush()
{
    plugincall(slotId, 13);
    return m_player->flush();
}


bool SPlayer::seek(const FB::variant& arg)
{
    plugincall(slotId, 5);
    double pos = arg.convert_cast<double>();
    if (pos < 0.0) {
        return false;
    }
    m_player->seek(pos);
    return true;
}

bool SPlayer::seekpos(const FB::variant& arg)
{
    plugincall(slotId, 6);
    double pos = arg.convert_cast<double>();
    if (pos < 0.0) {
        return false;
    }
    m_player->seekpos(pos);
    return true;
}

bool SPlayer::seekrel(const FB::variant& arg)
{
    plugincall(slotId, 7);
    double pos = arg.convert_cast<double>();
    if (pos == 0.0) {
        return false;
    }
    m_player->seekrel(pos);
    return true;
}

bool SPlayer::audiodisable(const FB::variant& arg)
{
    plugincall(slotId, 21);
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
    plugincall(slotId, 20);
    return m_player->setOptions(optname, optvalue);
}

int SPlayer::getvideoformat()
{
    plugincall(slotId, 8);
//    return m_player->getvideoformat();
    return 0;
}

int SPlayer::getaudioformat()
{
    plugincall(slotId, 9);
//    return m_player->getaudioformat();
    return 0;
}

double SPlayer::getmovielength()
{
    plugincall(slotId, 10);
    return m_player->getmovielength();
}

char* SPlayer::geturl()
{
    plugincall(slotId, 11);
//    return m_player->geturl();
    return NULL;
}

char* SPlayer::getstatusline()
{
    plugincall(slotId, 22);
    char* l = m_player->getstatusline();
    strncpy(statusline,l,1024);
    free(l);
    return statusline;
}

bool SPlayer::volume(const FB::variant& vol)
{
    plugincall(slotId, 14);
    int v = vol.convert_cast<int>();
    return m_player->volume(v);
}

double SPlayer::getcurrentpts()
{
    plugincall(slotId, 17);
    return m_player->getcurrentpts();
}

double SPlayer::getrealpts()
{
    plugincall(slotId, 18);
//    return m_player->getrealpts();
    return 0.0;
}

double SPlayer::getfps()
{
    plugincall(slotId, 19);
//    return m_player->getfps();
    return 0.0;
}

int SPlayer::getstatus()
{
    plugincall(slotId, 16);
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

int SPlayer::GetGroupId()
{
    return m_player->getgroup();
}

void SPlayer::SetGroupId(const FB::variant& group)
{
    int g = group.convert_cast<int>();
    m_player->setgroup(g);
}

void SPlayer::SetTimeShift(const FB::variant& time)
{
    int t = time.convert_cast<double>();
    m_player->settimeshift(t);
}

bool SPlayer::GroupSetTime(const FB::variant& arg)
{
    int timepos = arg.convert_cast<int>();
    if (timepos < 0) {
        return false;
    }
    double pos = (double)timepos / (double)GetTimeScale();
    m_player->groupseekpos(pos);
    return true;
}

/// *******************************************************************
/// Compatible functions to QuickTime plugin:
/// *******************************************************************

#ifdef _WIN32
#define snprintf _snprintf
#endif

char* SPlayer::GetPluginStatus()
{
    plugincall(slotId, 23);
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
    plugincall(slotId, 24);
    return m_player->play();
}

bool SPlayer::Stop()
{
    plugincall(slotId, 25);
    return m_player->pause();
}

bool SPlayer::Rewind()
{
    plugincall(slotId, 26);
    m_player->pause();
    m_player->seekpos(0.0);
    return false;
}

bool SPlayer::Step(const FB::variant& arg)
{
    plugincall(slotId, 27);
    int timepos = arg.convert_cast<int>();
    double pos = (double)timepos / (double)GetTimeScale();
    m_player->pause();
    m_player->seekrel(pos);
    return true;
}

int SPlayer::GetTimeScale()
{
    plugincall(slotId, 28);
    return 30;
}

int SPlayer::GetDuration()
{
    plugincall(slotId, 29);
    int ret;
    ret = m_player->getmovielength() * GetTimeScale();
    return ret;
    return m_player->getmovielength() * GetTimeScale();
}

bool SPlayer::SetURL(const std::string& entry)
{
    plugincall(slotId, 30);
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
    plugincall(slotId, 31);
    return m_url;
}

bool SPlayer::SetTime(const FB::variant& arg)
{
    plugincall(slotId, 32);
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
    plugincall(slotId, 33);
    int ret;
    ret = m_player->getcurrentpts() * GetTimeScale();
    return ret;
    return m_player->getcurrentpts() * GetTimeScale();
}

bool SPlayer::SetRate(const FB::variant& arg)
{
    plugincall(slotId, 34);
    return false;
}

float SPlayer::GetRate()
{
    plugincall(slotId, 35);
    int st = m_player->getstatus();
    if((st & STATUS_PLAYER_OPENED) && !(st & STATUS_PLAYER_PAUSE))
        return 1.0;
    return 0.0;
}

void SPlayer::SetVolume(const FB::variant& vol)
{
    plugincall(slotId, 37);
    int v = vol.convert_cast<int>();
    m_player->volume(v / 2.55);
}

int  SPlayer::GetVolume()
{
    plugincall(slotId, 15);
    return m_player->getvolume() * 2.55;
}

bool SPlayer::SetMute(const FB::variant& flag)
{
    plugincall(slotId, 40);
    bool f = flag.convert_cast<bool>();
    return m_player->mute(f);
}

bool SPlayer::GetMute()
{
    plugincall(slotId, 41);
    return m_player->getmute();
}

bool SPlayer::SetResetPropertiesOnReload(const FB::variant& arg)
{
    plugincall(slotId, 42);
    m_resetprop = arg.convert_cast<bool>();
    return true;
}

bool SPlayer::GetResetPropertiesOnReload()
{
    plugincall(slotId, 43);
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
    plugincall(slotId, 45);
    return m_width;
}

void SPlayer::SetWidth(const FB::variant& arg)
{
    plugincall(slotId, 45);
    m_width = arg.convert_cast<int>();
}

int SPlayer::GetHeight()
{
    plugincall(slotId, 46);
    return m_height;
}

void SPlayer::SetHeight(const FB::variant& arg)
{
    plugincall(slotId, 46);
    m_height = arg.convert_cast<int>();
}

std::string SPlayer::GetType()
{
    plugincall(slotId, 47);
    return m_type;
}

void SPlayer::SetType(const std::string& val)
{
    plugincall(slotId, 47);
    m_type = val;
}

std::string SPlayer::GetCodeBase()
{
    plugincall(slotId, 48);
    return m_codebase;
}

void SPlayer::SetCodeBase(const std::string& val)
{
    plugincall(slotId, 48);
    m_codebase = val;
}

std::string SPlayer::GetClassID()
{
    plugincall(slotId, 49);
    return m_classid;
}

void SPlayer::SetClassID(const std::string& val)
{
    plugincall(slotId, 49);
    m_classid = val;
}

std::string SPlayer::GetSrc()
{
    plugincall(slotId, 50);
    return m_src;
}

void SPlayer::SetSrc(const std::string& val)
{
    plugincall(slotId, 50);
    m_src = val;
}

std::string SPlayer::GetQTSrc()
{
    plugincall(slotId, 51);
    return m_qtsrc;
}

void SPlayer::SetQTSrc(const std::string& val)
{
    plugincall(slotId, 52);

    if(m_host->getDOMWindow())
    {
        std::string locurl = m_host->getDOMWindow()->getLocation();
        if(val.substr(0,3)=="../" && !locurl.empty())
        {
            size_t n = locurl.rfind('/');
            if(n != std::string::npos)
            {
                locurl = locurl.substr(0,n);
            }
            n = locurl.rfind('/');
            if(n != std::string::npos)
            {
                locurl = locurl.substr(0,n);
            }
            m_qtsrc = locurl+val.substr(2);
            return;
        }
        if(val.substr(0,2)=="./" && !locurl.empty())
        {
            size_t n = locurl.rfind('/');
            if(n != std::string::npos)
            {
                locurl = locurl.substr(0,n);
            }
            m_qtsrc = locurl+val.substr(1);
            return;
        }
    }
    m_qtsrc = val;
}

bool SPlayer::GetEnableJavascript()
{
    plugincall(slotId, 52);
    return m_enablejavascript;
}

void SPlayer::SetEnableJavascript(const FB::variant& arg)
{
    plugincall(slotId, 52);
    m_enablejavascript = arg.convert_cast<bool>();
}

bool SPlayer::GetKioskMode()
{
    plugincall(slotId, 53);
    return m_kioskmode;
}

void SPlayer::SetKioskMode(const FB::variant& arg)
{
    plugincall(slotId, 53);
    m_kioskmode = arg.convert_cast<bool>();
}

bool SPlayer::GetController()
{
    plugincall(slotId, 54);
    return m_controller;
}

void SPlayer::SetController(const FB::variant& arg)
{
    plugincall(slotId, 54);
    m_controller = arg.convert_cast<bool>();
}

bool SPlayer::GetAutoPlay()
{
    plugincall(slotId, 37);
    return m_autoplay;
}

void SPlayer::SetAutoPlay(const FB::variant& arg)
{
    plugincall(slotId, 36);
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
    plugincall(slotId, 55);
    return m_href;
}

void SPlayer::SetHRef(const std::string& val)
{
    plugincall(slotId, 55);
    m_href = val;
}

std::string SPlayer::GetQuickTimeVersion()
{
    return m_qver;
}

void SPlayer::EventProcess()
{
    FireEvent("onqt_progress", FB::variant_list_of(m_href));
    FireEvent("onTestEvent", FB::variant_list_of(m_href));
}

#if 0
bool SPlayer::onMouseDown()
{
    if(!m_href.empty())
    {
        fprintf(stderr,"Navigate: %s\n",m_href.c_str());
        m_host->Navigate(m_href, "");
        return true;
    }
    return false;
}
#endif
