/**********************************************************\ 
 
 Auto-generated FactoryMain.cpp
 
 This file contains the auto-generated factory methods 
 for the SPlayer project
 
\**********************************************************/

#include "FactoryBase.h"
#if FB_WIN || FB_X11
#include "SPlayerPlugin.h"
#else
#include "Mac/SPlayerPluginMac.h"
#endif
#include <boost/make_shared.hpp>
#include <iostream>
#include "libxplayer.h"

#if FB_MACOSX 
#include <sys/resource.h>

static bool EnableCoreDumps(void)
{
    struct rlimit   limit;
    
    limit.rlim_cur = RLIM_INFINITY;
    limit.rlim_max = RLIM_INFINITY;
    return setrlimit(RLIMIT_CORE, &limit) == 0;
}
#endif

class PluginFactory : public FB::FactoryBase
{
public:
    
    FB::PluginCorePtr createPlugin(const std::string& mimetype)
    {
        FBLOG_INFO("", "createPlugin (" << mimetype << ")");
        static int staticCounter = 0;
        int slotId = xplayer_API_newfreeslot();

        int playerId = staticCounter++;

#if FB_WIN || FB_X11
        return boost::make_shared<SPlayerPlugin>(playerId, slotId);
#else
        return boost::make_shared<SPlayerPluginMac>(playerId, slotId);
#endif
    }
    
    void getLoggingMethods( FB::Log::LogMethodList& outMethods )
    {
        outMethods.push_back(std::make_pair(FB::Log::LogMethod_Console, std::string()));
        
#if FB_WIN
        outMethods.push_back(std::make_pair(FB::Log::LogMethod_File, "c:\splayer.log"));
#else
        outMethods.push_back(std::make_pair(FB::Log::LogMethod_File, "/tmp/splayer.log"));
#endif
    }
    
    void globalPluginInitialize()
    {  
        FBLOG_INFO("", "initializing libxplayer");
        xplayer_API_init(0,NULL);
        
#if FB_WIN || FB_X11
        SPlayerPlugin::StaticInitialize();
#else
        EnableCoreDumps();
        SPlayerPluginMac::StaticInitialize();
#endif
    }
    
    void globalPluginDeinitialize()
    {
        // FBLOG_INFO("", "deinitializing libxplayer");

#if FB_WIN || FB_X11
        SPlayerPlugin::StaticDeinitialize();
#else
        SPlayerPluginMac::StaticDeinitialize();
#endif
     //   xplayer_API_done();

    }
    
    FB::Log::LogLevel getLogLevel(){
//        // All log messages, no matter how small
//        LogLevel_Trace  = 0x01,
//        // Debug level log messages - messages you won't care about unless you're debugging
//        LogLevel_Debug  = 0x02,
//        // Informational log messages - not critical to know, but you might care
//        LogLevel_Info   = 0x04,
//        // Only log warning and worse messages
//        LogLevel_Warn   = 0x08,
//        // Only log messages that are actual errors
//        LogLevel_Error  = 0x10

        return FB::Log::LogLevel_Info; 
    }
    
};



FB::FactoryBasePtr getFactoryInstance()
{
    static boost::shared_ptr<PluginFactory> factory = boost::make_shared<PluginFactory>();
    return factory;
}



