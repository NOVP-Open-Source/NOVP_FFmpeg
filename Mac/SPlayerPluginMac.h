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
#ifndef H_SPLAYERPLUGINMAC
#define H_SPLAYERPLUGINMAC

#include "PluginEvents/MacEventCocoa.h"
#include "Mac/PluginWindowMac.h"
#include "Mac/PluginWindowMacCG.h"

#include "../SPlayerPlugin.h"
#include "libxplayer.h"

#include <CoreFoundation/CoreFoundation.h>
#include <AudioToolbox/AudioToolbox.h>
//#import <OpenAL/al.h>
//#import <OpenAL/alc.h>

class SPlayerPluginMac : public SPlayerPlugin
{
public:
    static void StaticInitialize();
    static void StaticDeinitialize();
    SPlayerPluginMac(int playerIdentifier, int slotIdentifier);
    virtual ~SPlayerPluginMac();

    BEGIN_PLUGIN_EVENT_MAP()
//        EVENTTYPE_CASE(FB::CoreGraphicsDraw, onDrawCG, FB::PluginWindowMacCG)
    
        EVENTTYPE_CASE(FB::AttachedEvent, onWindowAttached, FB::PluginWindowMac)
        EVENTTYPE_CASE(FB::DetachedEvent, onWindowDetached, FB::PluginWindowMac)
        PLUGIN_EVENT_MAP_CASCADE(SPlayerPlugin)
    END_PLUGIN_EVENT_MAP()

    virtual bool onWindowAttached(FB::AttachedEvent *evt, FB::PluginWindowMac*);
    virtual bool onWindowDetached(FB::DetachedEvent *evt, FB::PluginWindowMac*);
    int playerId;
    int slotId;
    FB::PluginWindowMac* pluginWindow;
    
protected:
    bool onDrawCG(FB::CoreGraphicsDraw *evt, FB::PluginWindowMacCG*);
    void DrawCoreGraphics(CGContextRef context, const FB::Rect& bounds, const FB::Rect& clip);

private:
    void* m_layer;
    CGImageRef bg;
    mp_image_t *frame;
    void* vda_frame;
};

#endif

