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

#include <boost/shared_ptr.hpp>
#include "JSAPIAuto.h"
#include "BrowserHost.h"
#include "JSObject.h"
#include "PluginEvents/MouseEvents.h"
#include "PluginEvents/AttachedEvent.h"

#ifdef FB_WIN
#include "PluginEvents/WindowsEvent.h"
#endif

namespace FB { class PluginWindow; };
class MediaPlayer;

class SPlayer : public FB::JSAPIAuto
{
public:
    SPlayer(FB::BrowserHostPtr host, int pluginId, int slotId, int loglevel, int debugflag, std::string logfile);
    virtual ~SPlayer();

    int playerId;
    int slotId;
    int logLevel;
    bool play(const FB::CatchAll&);
    bool stop();
    bool open(const std::string&);
    bool close();
    bool seek(const FB::variant&);
    bool seekpos(const FB::variant&);
    bool seekrel(const FB::variant&);
    bool pause();
    bool flush();
    int getvideoformat();
    int getaudioformat();
    double getmovielength();
    char* geturl();
    bool volume(const FB::variant& vol);
    int getvolume();
    double getcurrentpts();
    double getrealpts();
    double getfps();
    int getstatus();
    bool rtspmode(const std::string&);
    bool setOptions(const std::string&, const std::string&);
    bool audiodisable(const FB::variant&);
    char* getstatusline();

    void setloglevel(const FB::variant& arg);
    int getloglevel();

    int GetGroupId();
    void SetGroupId(const FB::variant&);
    void SetTimeShift(const FB::variant&);
    bool GroupPlay();
    bool GroupStop();
    bool GroupSetTime(const FB::variant&);

/// Compatible parameters to QuickTime plugin
    std::string GetID();
    void SetID(const std::string&);
    int GetWidth();
    void SetWidth(const FB::variant& arg);
    int GetHeight();
    void SetHeight(const FB::variant& arg);
    std::string GetType();
    void SetType(const std::string&);
    std::string GetCodeBase();
    void SetCodeBase(const std::string&);
    std::string GetClassID();
    void SetClassID(const std::string&);
    std::string GetSrc();
    void SetSrc(const std::string&);
    std::string GetQTSrc();
    void SetQTSrc(const std::string&);
    bool GetEnableJavascript();
    void SetEnableJavascript(const FB::variant& arg);
    bool GetKioskMode();
    void SetKioskMode(const FB::variant& arg);
    bool GetController();
    void SetController(const FB::variant& arg);
    bool GetAutoPlay();
    void SetAutoPlay(const FB::variant& arg);
    std::string GetScale();
    void SetScale(const std::string&);
    std::string GetHRef();
    void SetHRef(const std::string&);
    std::string GetQuickTimeVersion();
/// ------------------------------------------------

    void setWindow(FB::PluginWindow*);

    void EventProcess();

/// Compatible functions to QuickTime plugin:
    char* GetPluginStatus();
    /*
        "Waiting” - waiting for the movie data stream to begin
        ”Loading” - data stream has begun, not able to play/display the movie yet
        ”Playable” - movie is playable, although not all data has been downloaded
        ”Complete” - all data has been downloaded
        ”Error: <error number>” - the movie failed with the specified error number
    */
    bool Play();                                                /// play
    bool Stop();                                                /// stop (pause??)
    bool Rewind();                                              /// seek to 0 and pause
    bool Step(const FB::variant&);                              /// relative seek
    int GetTimeScale();                                         /// time scale => GetTimeScale()=30, GetDuration()=120 -> video length 120 * 1/30 = 4 sec
    int GetDuration();                                          /// video length
    bool SetURL(const std::string&);                            /// set video URL
    std::string GetURL();                                       /// get video URL
    bool SetTime(const FB::variant&);                           /// apsolute seek
    int  GetTime();                                             /// get current position
    bool SetRate(const FB::variant&);                           /// set playback speed (float)
    float GetRate();                                            /// get playback speed: 1.0 - play, 0.0 - pause
    void SetVolume(const FB::variant&);                         /// set volume level
    int  GetVolume();                                           /// get volume level
    bool SetMute(const FB::variant&);                           /// set mute flag (bool)
    bool GetMute();                                             /// get mute flag
    bool SetResetPropertiesOnReload(const FB::variant&);        /// reset plugin setting when load new movie (bool)
    bool GetResetPropertiesOnReload();                          /// get reset plugin settings flag


    //about box/context menu related
    bool onMouseDown(FB::MouseDownEvent * evt);
    #ifdef FB_WIN
        bool onWindowsEvent(FB::WindowsEvent* evt, FB::PluginWindow* win);
    #endif

private:
    typedef boost::shared_ptr<MediaPlayer> MediaPlayerPtr;
    FB::BrowserHostPtr m_host;
    MediaPlayerPtr m_player;
    bool m_valid;
    int m_loglevel;
    std::string m_logfile;
    std::string m_url;
    bool m_autoplay;
    bool m_resetprop;
    bool m_inited;
    char statusline[1024];
    char qtstatus[1024];

    std::string m_classid;
    std::string m_codebase;
    std::string m_id;
    int m_height;
    int m_width;
    std::string m_type;
    std::string m_src;
    std::string m_qtsrc;
    bool m_enablejavascript;
    bool m_kioskmode;
    bool m_controller;
    std::string m_scale;
    std::string m_href;
    std::string m_qver;

};

