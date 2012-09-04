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

#ifndef H_MEDIAPLAYER
#define H_MEDIAPLAYER

#include <boost/shared_ptr.hpp>
#include <stdexcept>
#include <string>
#include "libxplayer.h"

namespace FB { class PluginWindow; };
struct PlayerContext;
typedef boost::shared_ptr<PlayerContext> PlayerContextPtr;

class MediaPlayer
{
public:
    static void StaticInitialize();
    static void StaticDeinitialize();

    struct InitializationException : std::runtime_error {
        InitializationException(const char* const what) : std::runtime_error(what) {}
    };

    MediaPlayer(int pluginIdentifier, int slotIdentifier, int loglevel, int debugflag);
    ~MediaPlayer();

    const std::string& version() const;
    const std::string& type() const;
    const std::string& lastError() const;

    bool open(const std::string&);
    bool close();
    bool play();
    bool stop();
    bool flush();
    bool seek(double percent);
    bool seekpos(double pos);
    bool seekrel(double pos);
    bool pause(void);
    bool audiodisable(bool dis);
    int getvideoformat();
    int getaudioformat();
    double getmovielength();
    char* geturl();
    bool volume(int vol);
    int getvolume();
    bool mute(bool mute);
    bool getmute();
    double getcurrentpts();
    double getrealpts();
    double getfps();
    int getstatus();
    bool rtspmode(const std::string& mode);
    bool setOptions(const std::string&, const std::string&);
    char* getstatusline();
    bool setloglevel(int logLevel);

    void setWindow(FB::PluginWindow*);

    int playerId;
    int slotId;
    int logLevel;

private:
    boost::shared_ptr<PlayerContext> m_context;
    std::string m_version;
    const std::string m_type;
};

#endif

