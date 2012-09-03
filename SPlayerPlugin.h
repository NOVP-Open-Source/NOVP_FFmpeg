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
#ifndef H_SPLAYERPLUGIN
#define H_SPLAYERPLUGIN

#include "PluginEvents/MouseEvents.h"
#include "PluginEvents/AttachedEvent.h"
#include "PluginWindow.h"
#include "Timer.h"

#include "PluginCore.h"

class SPlayer;

class SPlayerPlugin : public FB::PluginCore
{
public:
    static void StaticInitialize();
    static void StaticDeinitialize();

public:
    SPlayerPlugin(int pluginIdentifier, int slotIdentifier);
    virtual ~SPlayerPlugin();
    int playerId;
    int slotId;

public:
    virtual FB::JSAPIPtr createJSAPI();
    virtual bool isWindowless() { return false; }

    BEGIN_PLUGIN_EVENT_MAP()
//        EVENTTYPE_CASE(FB::MouseDownEvent, onMouseDown, FB::PluginWindow)
//        EVENTTYPE_CASE(FB::MouseUpEvent, onMouseUp, FB::PluginWindow)
//        EVENTTYPE_CASE(FB::MouseMoveEvent, onMouseMove, FB::PluginWindow)
        EVENTTYPE_CASE(FB::AttachedEvent, onWindowAttached, FB::PluginWindow)
        EVENTTYPE_CASE(FB::DetachedEvent, onWindowDetached, FB::PluginWindow)
    END_PLUGIN_EVENT_MAP()

    virtual bool onMouseDown(FB::MouseDownEvent *evt, FB::PluginWindow*);
    virtual bool onMouseUp(FB::MouseUpEvent *evt, FB::PluginWindow*);
    virtual bool onMouseMove(FB::MouseMoveEvent *evt, FB::PluginWindow*);
    virtual bool onWindowAttached(FB::AttachedEvent* evt, FB::PluginWindow*);
    virtual bool onWindowDetached(FB::DetachedEvent* evt, FB::PluginWindow*);

    void timerCallback(void);

private:
    typedef boost::shared_ptr<SPlayer> SPlayerPtr;
    SPlayerPtr m_player;
    FB::PluginWindow* m_window;
    FB::TimerPtr m_timer;
};

#endif

