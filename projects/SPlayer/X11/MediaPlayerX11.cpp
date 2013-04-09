
#include <list>
#include <boost/assign/list_of.hpp>
#include "JSAPI.h"
#include "X11/PluginWindowX11.h"
#include "../MediaPlayer.h"

#include <stdint.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdio.h>
#include <unistd.h>
#include <math.h>

#include <GL/glx.h>
#include <GL/gl.h>
#include <GL/glu.h>

#include "libxplayer.h"
#include "libsipalsa.h"

#include "debug.h"

//#define DEBUGPRINT      1

struct PlayerContext;

extern "C" {
    extern void* dmem;
    void* video_process(void * data);
    void* audio_process(void * data);
};

static pthread_t audio_thread;
static void* audio_priv = NULL;

typedef struct {
    int run;
    Window hwnd;

    char* filename;
    void* playpriv;
    int slot;
    mp_image_t* bck;
} video_priv_t;

typedef struct {
    int run;
} audio_priv_t;


struct PlayerContext 
{
    int w;
    int h;

    video_priv_t* video_priv;
    pthread_t thread;

    Window hwnd;
    std::string error;
    std::string file;
    int slotId;

    PlayerContext() : video_priv(0), hwnd(0), slotId() {}
};

extern "C" {

    void setst(int slot, int st) {
        if(dmem && slot<MAX_DEBUG_SLOT) {
            slotdebug_t* slotdebugs = debugmem_getslot(dmem);
            if(slotdebugs) {
                slotdebugs[slot].silence=st;
            }
        }
    }
};

namespace 
{
    std::string vfwErrorString(const std::string& what, int hr)
    {
        std::ostringstream os;
//        os << what << ": " << mapVfwError(hr);
        os << what << ": " << hr;
        return os.str();                                                                                
    }

    PlayerContextPtr make_context(Window hwnd)
    {
        PlayerContextPtr context(new PlayerContext);
        if(!context) throw MediaPlayer::InitializationException("failed to create context");

        context->hwnd = hwnd;

        return context;
    }

    bool activateVideo(PlayerContextPtr context)
    {
#ifdef DEBUGPRINT
fprintf(stderr,"****************** activateVideo: %d hwnd: %p\n",context->slotId,context->hwnd);
#endif
        if(!context->hwnd)
            return true;

        if(context->video_priv && context->video_priv->run) {
#ifdef DEBUGPRINT
fprintf(stderr,"****************** activateVideo: %d run\n",context->slotId);
#endif
/*
            context->video_priv->stop=1;
            context->video_priv->pause=0;
            context->video_priv->status=0;
            context->video_priv->sync=0.0;
            context->video_priv->startpts=0.0;
            context->video_priv->seekpos=0.0;
            context->video_priv->seekflag=false;
            context->video_priv->pts=0.0;
            context->video_priv->vpts=0.0;
            context->video_priv->dpts=0.0;
            context->video_priv->status=0;
            context->video_priv->pause=0;
            context->video_priv->stop=0;
*/
            context->video_priv->filename=strdup(context->file.c_str());
        } else {
            if(context->video_priv) {
                context->video_priv->run=0;
#ifdef DEBUGPRINT
fprintf(stderr,"****************** activateVideo: %d join\n",context->slotId);
#endif
                pthread_join(context->thread, NULL);
#ifdef DEBUGPRINT
fprintf(stderr,"****************** activateVideo: %d join OK\n",context->slotId);
#endif
                if(context->video_priv->filename)
                    free(context->video_priv->filename);
                context->video_priv->filename=NULL;
                free(context->video_priv);
                context->video_priv=NULL;
            }
            context->video_priv = (video_priv_t*)calloc(1,sizeof(video_priv_t));
            context->video_priv->slot = context->slotId;
            context->video_priv->run=1;
            context->video_priv->filename=strdup(context->file.c_str());
            context->video_priv->hwnd=context->hwnd;
#ifdef DEBUGPRINT
fprintf(stderr,"****************** activateVideo: %d create\n",context->slotId);
#endif
            pthread_create(&context->thread, NULL, video_process, context->video_priv);
#ifdef DEBUGPRINT
            fprintf(stderr,"pthread_create: video_priv slot: %d 0x%x \n",context->slotId,context->thread);
#endif
        }

#ifdef DEBUGPRINT
fprintf(stderr,"****************** activateVideo: %d OK\n",context->slotId);
#endif
        return true;
    }
};

MediaPlayer::MediaPlayer(int pluginIdentifier, int slotIdentifier, int loglevel, int debugflag, std::string logfile)
  : m_context()
  , m_version("")
  , m_type("DirectDraw")
{
//    ::CoInitializeEx(0, COINIT_MULTITHREADED);
    playerId = pluginIdentifier;
    slotId = slotIdentifier;
    logLevel = loglevel;
    FBLOG_INFO("", "MediaPlayer( pluginId: "<<pluginIdentifier<<", slotId: "<<slotIdentifier<<", logLevel: "<<loglevel<<")");
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
    m_context->slotId = slotId;
    activateVideo(m_context);
    FBLOG_INFO("", "MediaPlayer( pluginId: "<<pluginIdentifier<<", slotId: "<<slotIdentifier<<") OK");
}

MediaPlayer::~MediaPlayer()
{
#ifdef DEBUGPRINT
    fprintf(stderr,"XXXXXXXXX destructor: MediaPlayer::~MediaPlayer() slot: %d\n",slotId);
#endif
    stop();
    if(m_context->video_priv) {
        m_context->video_priv->run|=2;
        pthread_join(m_context->thread, NULL);
        if(m_context->video_priv->filename)
            free(m_context->video_priv->filename);
        free(m_context->video_priv);
        m_context->video_priv=NULL;
    }
#ifdef DEBUGPRINT
    fprintf(stderr,"XXXXXXXXX slotfree %d ..\n",slotId);
#endif
    xplayer_API_slotfree(slotId);
#ifdef DEBUGPRINT
    fprintf(stderr,"XXXXXXXXX slotfree %d OK\n",slotId);
#endif
}

void MediaPlayer::setWindow(FB::PluginWindow* pluginWindow)
{
    GdkNativeWindow hwnd = 0;

    if(pluginWindow) {
        FB::PluginWindowX11* wnd = reinterpret_cast<FB::PluginWindowX11*>(pluginWindow);
        hwnd = wnd->getWindow();
        m_context->hwnd = hwnd;
#ifdef DEBUGPRINT
fprintf(stderr,"XXXXXXXXX setWindow: %d pluginWindow: %p hwnd: %x\n",slotId,pluginWindow,hwnd);
#endif
    } else {
#ifdef DEBUGPRINT
fprintf(stderr,"XXXXXXXXX setWindow: %d pluginWindow: %p\n",slotId,pluginWindow);
#endif
#if 0
        stop();
        if(m_context->video_priv) {
            m_context->video_priv->run|=2;
#ifdef DEBUGPRINT
fprintf(stderr,"XXXXXXXXX setWindow run: %d slot: %d\n",m_context->video_priv->run,slotId);
#endif
            pthread_join(m_context->thread, NULL);
            if(m_context->video_priv->filename)
                free(m_context->video_priv->filename);
            free(m_context->video_priv);
            m_context->video_priv=NULL;
        }
#endif
        m_context->hwnd = None;
    }
#ifdef DEBUGPRINT
fprintf(stderr,"XXXXXXXXX setWindow done: %d\n",slotId);
#endif
}

bool MediaPlayer::setloglevel(int logLevel)
{
    xplayer_API_setloglevel(slotId, logLevel);
    return true;
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
    m_context->file = url;
    activateVideo(m_context);
//    xplayer_API_setimage(slotId, 0, 0, IMGFMT_BGR32);
    xplayer_API_setimage(slotId, 0, 0, IMGFMT_RGB24);
//    xplayer_API_setimage(slotId, 1920, 1440, IMGFMT_RGB24);
//    xplayer_API_setimage(slotId, 0, 0, IMGFMT_YV12);
//    xplayer_API_setimage(slotId, 0, 0, IMGFMT_I420);
    xplayer_API_sethwbuffersize(slotId, xplayer_API_prefilllen());

#ifdef DEBUGPRINT
    fprintf(stderr,"XXXXXXXXX load: %d\n",slotId);
#endif
    xplayer_API_loadurl(slotId, (char*)url.c_str());
//    xplayer_API_setimage(slotId, 0, 0, IMGFMT_RGB24);
    return false;
}

bool MediaPlayer::close()
{
    xplayer_API_slotfree(slotId);
    return false;
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

bool MediaPlayer::play()
{

    xplayer_API_play(slotId);
#ifdef DEBUGPRINT
    fprintf(stderr,"XXXXXXXXX play: %d\n",slotId);
#endif
//    m_context->wnd->StartAutoInvalidate(1.0/30.0);

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

bool MediaPlayer::seek(const double pos)
{
    double length = xplayer_API_getmovielength(slotId);
    if(length>0.0 && pos>=0.0 && pos<=100.0) {
        double timepos = length * pos / 100;
        xplayer_API_seek(slotId, timepos);
#ifdef DEBUGPRINT
fprintf(stderr,"XXXXXXXXX seek: %12.6f pos: %12.6f => %12.6f\n",length,pos,timepos);
#endif
    } else {
#ifdef DEBUGPRINT
fprintf(stderr,"XXXXXXXXX seek: %12.6f pos: %12.6f => no seek\n",length,pos);
#endif
    }
    return false;
}

int MediaPlayer::getstatus(void)
{
    return xplayer_API_getstatus(slotId);
}

char* MediaPlayer::getstatusline(void)
{
    return xplayer_API_getstatusline(slotId);
}

bool MediaPlayer::stop()
{
/*
    if(m_context->video_priv) {
        if(m_context->video_priv->filename)
            free(m_context->video_priv->filename);
        m_context->video_priv->filename=NULL;
        m_context->video_priv->stop=1;
    }
*/
#ifdef DEBUGPRINT
    fprintf(stderr,"XXXXXXXXX stop: %d\n",slotId);
#endif
    xplayer_API_stop(slotId);
#ifdef DEBUGPRINT
    fprintf(stderr,"XXXXXXXXX stop: %d OK\n",slotId);
#endif
    return true;
}

bool MediaPlayer::pause()
{
#ifdef DEBUGPRINT
    fprintf(stderr,"XXXXXXXXX pause: %d\n",slotId);
#endif
    xplayer_API_pause(slotId);
    return false;
}

bool MediaPlayer::flush()
{
    xplayer_API_flush(slotId);
    return false;
}

bool MediaPlayer::audiodisable(bool dis)
{
#ifdef DEBUGPRINT
    fprintf(stderr,"XXXXXXXXX audiodisable: %d %d\n",slotId,dis);
#endif
    xplayer_API_enableaudio(slotId, !dis);
    return false;
}

double MediaPlayer::getcurrentpts()
{
    return xplayer_API_getcurrentpts(slotId);
}

double MediaPlayer::getmovielength()
{
    return xplayer_API_getmovielength(slotId);
}

void MediaPlayer::StaticInitialize() {
    audio_priv = (audio_priv_t*)calloc(1,sizeof(video_priv_t));
    pthread_create(&audio_thread, NULL, audio_process, audio_priv);
#ifdef THREAD_DEBUG
    fprintf(stderr,"pthread_create: plugin audio 0x%x \n",&audio_thread);
#endif
#ifdef DEBUGPRINT
    fprintf(stderr,"XXXXXXXXX ----------------------- static init ----------------------\n");
#endif
}

void MediaPlayer::StaticDeinitialize() {
#ifdef DEBUGPRINT
    fprintf(stderr,"XXXXXXXXX ----------------------- static uninit ----------------------\n");
#endif
    audio_priv_t* priv = (audio_priv_t*)audio_priv;
    priv->run=0;
    pthread_join(audio_thread, NULL);
    free(audio_priv);
    audio_priv=NULL;
    xplayer_API_done();
}

extern "C" {

    void* video_process(void * data) {
        video_priv_t* priv = (video_priv_t*)data;
        mp_image_t* img = NULL;
        mp_image_t* frame = NULL;
        int w=0;
        int h=0;
        int winw = 640;
        int winh = 480;

        double stime=0.0;
        double etime=0.0;
        double ltime=0.0;

        Display *g_pDisplay = NULL;
        XSetWindowAttributes windowAttributes;
        XVisualInfo *visualInfo = NULL;
        int errorBase = 0;
        int eventBase = 0;
        int g_bDoubleBuffered = 1;
        Colormap colorMap;
        GLXContext glxContext;
        Window g_window = None;

        GLuint   g_textureID = 0;

        float g_fSpinX           = 0.0f;
        float g_fSpinY           = 0.0f;

        struct Vertex
        {
            float tu, tv;
            float x, y, z;
        };

        Vertex g_quadVertices[] =
        {
            { 0.0f, 0.0f,-1.0f,-1.0f, 0.0f },
            { 1.0f, 0.0f, 1.0f,-1.0f, 0.0f },
            { 1.0f, 1.0f, 1.0f, 1.0f, 0.0f },
            { 0.0f, 1.0f,-1.0f, 1.0f, 0.0f }
        };

        static const GLfloat vertexCoord[] = {
            -1.0f, -1.0f,
             1.0f, -1.0f,
            -1.0f,  1.0f,
             1.0f,  1.0f,
        };

        int major = 0;
        int minor = 0;

        float f = -5.0;

        setst(priv->slot, 1);
#ifdef DEBUGPRINT
        fprintf(stderr,"************** plugin video process start: %d\n",priv->slot);
#endif

        g_pDisplay = XOpenDisplay( NULL );

        glXQueryVersion(g_pDisplay, &major, &minor);

        if( !glXQueryExtension(g_pDisplay, &errorBase, &eventBase))
        {
            fprintf(stderr, "X server has no OpenGL GLX extension. error: %d event: %d\n",errorBase,eventBase);
            xplayer_API_videoprocessdone(priv->slot);
            XCloseDisplay(g_pDisplay);
            priv->run=0;
            return NULL;
        }

        int doubleBufferVisual[]  =
        {
            GLX_RGBA,           // Needs to support OpenGL
            GLX_DEPTH_SIZE, 16, // Needs to support a 16 bit depth buffer
            GLX_DOUBLEBUFFER,   // Needs to support double-buffering
            None                // end of list
        };

        int singleBufferVisual[] =
        {
            GLX_RGBA,           // Needs to support OpenGL
            GLX_DEPTH_SIZE, 16, // Needs to support a 16 bit depth buffer
            None                // end of list
        };

        // Try for the double-bufferd visual first
        visualInfo = glXChooseVisual(g_pDisplay, DefaultScreen(g_pDisplay), doubleBufferVisual);
        if( visualInfo == NULL )
        {
            // If we can't find a double-bufferd visual, try for a single-buffered visual...
            visualInfo = glXChooseVisual(g_pDisplay, DefaultScreen(g_pDisplay), singleBufferVisual );

            if( visualInfo == NULL )
            {
                fprintf(stderr, "no RGB visual with depth buffer\n");
                xplayer_API_videoprocessdone(priv->slot);
                XCloseDisplay(g_pDisplay);
                priv->run=0;
                return NULL;
            }
            g_bDoubleBuffered = 0;
        }



//fprintf(stderr,"init ok\n");

        priv->run|=1;
        while(priv->run) {

            if(dmem && priv->slot<MAX_DEBUG_SLOT) {
                slotdebug_t* slotdebugs = debugmem_getslot(dmem);
                if(slotdebugs) {
                    slotdebugs[priv->slot].plugin_proc++;
                }
            }

            if(g_window != priv->hwnd) {
                setst(priv->slot, 2);
                if(g_window != None) {
                    glXDestroyContext(g_pDisplay, glxContext);
                }
                g_window = priv->hwnd;

                if(g_window != None) {
                    setst(priv->slot, 3);
                    // Create an OpenGL rendering context
                    glxContext = glXCreateContext( g_pDisplay,
                                                   visualInfo,
                                                   NULL,      // No sharing of display lists
                                                   GL_TRUE ); // Direct rendering if possible

                    if( glxContext == NULL )
                    {
                        fprintf(stderr, "could not create rendering context\n");
                        break;
                    }

                    // Bind the rendering context to the window
                    glXMakeCurrent( g_pDisplay, g_window, glxContext );

                    // Request the X window to be displayed on the screen
                    XMapWindow( g_pDisplay, g_window );

/////////////////////////////
/////////// Init GL /////////
/////////////////////////////
                    glClearColor( 0.0f, 0.0f, 0.0f, 1.0f );
                    glEnable( GL_TEXTURE_2D );

                    glMatrixMode(GL_PROJECTION);
                    glLoadIdentity();
//        gluPerspective( 45.0f, 640.0f / 480.0f, 0.1f, 100.0f);
//        gluPerspective( 45.0f, 640.0f / 480.0f, 0.1f, 2000.0f);
                    gluPerspective( 45.0f, 1.0f, 0.1f, 2000.0f);
/////////////////////////////

                    glGenTextures( 1, &g_textureID );
                    glBindTexture( GL_TEXTURE_2D, g_textureID );

                    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
                    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );

                    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
                    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
#ifdef DEBUGPRINT
fprintf(stderr,"setWindow openGL inited. slot: %d window: %x\n",priv->slot,g_window);
#endif
                }
                setst(priv->slot, 2);
#ifdef DEBUGPRINT
                fprintf(stderr,"************** plugin video process end: %d\n",priv->slot);
#endif
            }




            setst(priv->slot, 3);
            if(xplayer_API_isnewimage(priv->slot))
            {
                setst(priv->slot, 4);
                xplayer_API_getimage(priv->slot, &img);
                setst(priv->slot, 5);

                if(img) {
                    w=img->w;
                    h=img->h;
                } else if(img && (w!=img->w || h!=img->h)) {
                    w=img->w;
                    h=img->h;
                }

                if(img) {
                    if(img->imgfmt==IMGFMT_I420) {
                        unsigned char* tmp = img->planes[1];
                        img->planes[1] = img->planes[2];
                        img->planes[2] = tmp;
                    }



                    setst(priv->slot, 6);
                    if(g_window != None) {
/////////////////////////////////////////
/////////// Load texture GL /////////////
/////////////////////////////////////////
//fprintf(stderr,"Load texture: %dx%d %s\n",img->width,img->height,vo_format_name(img->imgfmt));
#ifdef DEBUGPRINT
fprintf(stderr,"Load texture. slot: %d img: %p\n",priv->slot,img);
#endif
#if 1
                        glBindTexture( GL_TEXTURE_2D, g_textureID );
                        glTexImage2D( GL_TEXTURE_2D, 0, 3, img->width, img->height,
                                       0, GL_RGB, GL_UNSIGNED_BYTE, img->planes[0] );

#else
                        glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, img->width, img->height, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, NULL );
                        glBindTexture( GL_TEXTURE_2D, g_textureID );
                        glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, img->width, img->height, GL_RGBA, GL_UNSIGNED_BYTE, img->planes[0] );
#endif
///////////////////////////////////
/////////// Render GL /////////////
///////////////////////////////////
                        glMatrixMode( GL_MODELVIEW );
                        glLoadIdentity();
                        glTranslatef( 0.0f, 0.0f, -2.5f );

                        glRotatef(180.0, 180.0, 0.0, 0.0);

//                            glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, img->width, img->height, GL_RGBA, GL_UNSIGNED_BYTE, img->planes[0] );

                        glInterleavedArrays( GL_T2F_V3F, 0, g_quadVertices );
                        glDrawArrays( GL_QUADS, 0, 4 );

                        if( g_bDoubleBuffered )
                            glXSwapBuffers( g_pDisplay, g_window ); // Buffer swap does implicit glFlush
                        else
                            glFlush(); // Explicit flush for single buffered case
///////////////////////////////////
                    }
                    setst(priv->slot, 7);

//                    win_draw(winpriv,img);

                    if(img->imgfmt==IMGFMT_I420) {
                        unsigned char* tmp = img->planes[1];
                        img->planes[1] = img->planes[2];
                        img->planes[2] = tmp;
                    }
                    xplayer_API_imagedone(priv->slot);
                }
            }

            setst(priv->slot, 8);
            while(true) {
                frame = NULL;
                setst(priv->slot, 9);
                xplayer_API_freeableimage(priv->slot, &frame);
                setst(priv->slot, 10);
                if(frame) {
                    setst(priv->slot, 11);
                    xplayer_API_freeimage(priv->slot, frame);
                    setst(priv->slot, 12);
                } else {
                    break;
                }
            }
            setst(priv->slot, 13);
            ltime=etime;
            etime=xplayer_clock();
            if(etime>0.0 && stime>0.0) {
                threadtime(priv->slot, DISPLAY_THREAD_ID, etime-stime, etime-ltime);
            }
            usleep(40000);
            stime=xplayer_clock();
            if((priv->run & 2) && !(xplayer_API_getstatus(priv->slot)&STATUS_PLAYER_OPENED))
            {
                break;
            }
        }
        setst(priv->slot, 15);
        if(g_window != None) {
            setst(priv->slot, 16);
            glXDestroyContext(g_pDisplay, glxContext);
            setst(priv->slot, 17);
        }
        setst(priv->slot, 18);
        xplayer_API_videoprocessdone(priv->slot);
        XCloseDisplay(g_pDisplay);
        priv->run=0;
        setst(priv->slot, 19);
        if(dmem && priv->slot<MAX_DEBUG_SLOT) {
            slotdebug_t* slotdebugs = debugmem_getslot(dmem);
            if(slotdebugs) {
                slotdebugs[priv->slot].plugin_proc=0;
            }
        }
    }


    void* audio_process(void * data) {
        audio_priv_t* priv = (audio_priv_t*)data;
        char abuffer[0x10000];
        int alen = 0;
        int plen = 0;
        int rate = xplayer_API_getaudio_rate();
        int channels = xplayer_API_getaudio_channels();
        static void* sipalsa = NULL;

        alen=0;

        if(!sipalsa) {
            sipalsa=sipalsa_open("hw:0", rate , channels, 0, 100, 100, 100, 0, 0);
        }
        memset(abuffer,0,sizeof(abuffer));

        while(1) {
            alen=xplayer_API_prefillaudio(abuffer, sizeof(abuffer), plen);
            plen+=alen;
            if(sipalsa && alen)
                sipalsa_play(sipalsa, rate, channels, alen, (unsigned char*) abuffer);
            if(!alen)
                break;
        }
        alen=0;
        priv->run=1;
        while(priv->run && sipalsa)
        {
            alen=xplayer_API_getaudio(abuffer, sizeof(abuffer));
            if(sipalsa && alen) {
                sipalsa_play(sipalsa, rate, channels, alen, (unsigned char*) abuffer);
            } else {
                usleep(10000);
            }
        }
        if(sipalsa)
            sipalsa_close(sipalsa);
        sipalsa=NULL;
        priv->run=false;
    }



};
