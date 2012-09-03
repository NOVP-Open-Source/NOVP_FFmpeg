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

#import <AppKit/AppKit.h>

#include "Mac/PluginWindowMac.h"
#include "Mac/PluginWindowMacCG.h"
#include "Mac/PluginWindowMacCA.h"

#include "SPlayerPluginMac.h"
#include "libxplayer.h"

#include <pthread.h>
#include <AudioToolbox/AudioToolbox.h>
#import <AudioUnit/AudioUnit.h>

@interface SPCAOpenGLLayer : CAOpenGLLayer {
    // define private variables
    mp_image_t *frame;
    void* vda_frame;
    int slotId;
    int frame_w;
    int frame_h;
    GLuint g_textureID;
    
}
@end

@implementation SPCAOpenGLLayer

- (id) initWithSlotId : (int) inSlotId {
    if(self = [super init])
    {
        slotId = inSlotId;
        frame_w = 0;
        frame_h = 0;
        g_textureID = 0;
    }
    return self;
}


- (void)drawInCGLContext:(CGLContextObj)ctx pixelFormat:(CGLPixelFormatObj)pf forLayerTime:(CFTimeInterval)t displayTime:(const CVTimeStamp *)ts {
    GLsizei width = CGRectGetWidth([self bounds]), height = CGRectGetHeight([self bounds]);
    
    int drawframe = 0;
    
    if(width!=frame_w || height!=frame_h || !g_textureID)
    {
        frame_w=width;
        frame_h=height;
        glViewport(0, 0, width, height);
        glEnable( GL_TEXTURE_2D );
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glScalef(1.0f, -1.0f, 1.0f);
        glOrtho(0, width, 0, height, -1.0, 1.0);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
    
        if(g_textureID)
        {
            glDeleteTextures( 1, &g_textureID );
        }
        g_textureID=0;
        glGenTextures( 1, &g_textureID );
        glBindTexture( GL_TEXTURE_2D, g_textureID );
    
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
    
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
        glColor4f(0.9, 0.3, 0.4, 1.00);
        NSLog(@"texture init\n");
        drawframe = 1;
   }
    if(!xplayer_API_isnewimage(slotId)){
        while(true) {
            frame = NULL;
            xplayer_API_freeableimage(slotId, &frame);
            if(frame) {
                xplayer_API_freeimage(slotId, frame);
            } else {
                break;
            }
        }
        while(true) {
            vda_frame = NULL;
            xplayer_API_freeablevdaframe(slotId, &vda_frame);
            if(vda_frame){
                xplayer_API_freevdaframe(slotId, vda_frame);
                NSLog(@"free vda\n");
            }
            else
                break;
            
        }
        if(xplayer_API_getstatus(slotId) == 2 && 0){
            glBegin(GL_QUADS);
            glColor4f(0.9, 0.3, 0.4, 1.00);
            glVertex3f(0,       0,      -1.0f);
            glVertex3f(width,   0,      -1.0f);
            glVertex3f(width,   height, -1.0f);
            glVertex3f(0,       height, -1.0f);
            glEnd();
        }

    }
    else
    {
        if( xplayer_API_isvda(slotId) )
        {
            NSLog(@"VDA frame present\n");

            xplayer_API_getvdaframe(slotId, &vda_frame);
            if(vda_frame){
                NSLog(@"get vda: %p\n",vda_frame);
                CVPixelBufferRef cvbuff = (CVPixelBufferRef)xplayer_API_vdaframe2cvbuffer(slotId, vda_frame);
                
                CVPixelBufferLockBaseAddress(cvbuff, 0);
                int i;
                typedef struct AVPicture {
                    uint8_t* data[3];
                    int linesize[3];
                } AVPicture;
                AVPicture vdapict;
                for(i=0; i<3; i++)
                {
                    vdapict.data[i] = (uint8_t*)CVPixelBufferGetBaseAddressOfPlane( cvbuff, i );
                    vdapict.linesize[i] = CVPixelBufferGetBytesPerRowOfPlane( cvbuff, i );
                }
                
                int w = CVPixelBufferGetWidth(cvbuff);
                int h = CVPixelBufferGetHeight(cvbuff);
                


                glTexImage2D( GL_TEXTURE_2D, 0, 3, w, h,
                             0, GL_YCBCR_422_APPLE, GL_UNSIGNED_SHORT_8_8_APPLE, vdapict.data[0] );
                CVPixelBufferUnlockBaseAddress(cvbuff, 0);
             //   NSLog(@"belefutottE %d x %d\n",w,h);
               // CVPixelBufferRelease(cvbuff);
                drawframe = 1;
                xplayer_API_vdaframedone(slotId);
            }
            
            while(1) {
                vda_frame = NULL;
                xplayer_API_freeablevdaframe(slotId, &vda_frame);
                if(vda_frame)
                {
                    NSLog(@"free vda: %p\n",vda_frame);
                    xplayer_API_freevdaframe(slotId, vda_frame);
                }
                    else
                    break;
            }
        }
        else
        {
            
            
            
            frame = NULL;
            xplayer_API_getimage(slotId, &frame);
            
            if(frame)
            {
   

                glTexImage2D( GL_TEXTURE_2D, 0, 3, frame->width, frame->height,
                             0, GL_BGRA, GL_UNSIGNED_BYTE, frame->planes[0] );
                NSLog(@"frame\n");
                drawframe = 1;
                xplayer_API_imagedone(slotId);
            }
            
            while(true) {
                frame = NULL;
                xplayer_API_freeableimage(slotId, &frame);
                if(frame) {
                    xplayer_API_freeimage(slotId, frame);
                } else {
                    break;
                }
            }
            
        }
        
        
    }
 
    glBegin(GL_QUADS);
    glColor4f(1,1,1,1);
    glTexCoord2f(0.0, 0.0);
    glVertex3f(0,      0,      -1.0f);
    glTexCoord2f(1.0, 0.0);
    glVertex3f(width,   0,      -1.0f);
    glTexCoord2f(1.0, 1.0);
    glVertex3f(width,   height, -1.0f);
    glTexCoord2f(0.0, 1.0);
    glVertex3f(0,       height, -1.0f);
    glEnd();

    while(0) {
        vda_frame = NULL;
        xplayer_API_freeablevdaframe(slotId, &vda_frame);
        if(vda_frame)
        {
            NSLog(@"free vda\n");
            xplayer_API_freevdaframe(slotId, vda_frame);
        }
        else
            break;
    }
   
    [super drawInCGLContext:ctx pixelFormat:pf forLayerTime:t displayTime:ts];
}
@end


#define NUM_BUFFERS 2

char *abuff;
int abuffLen=0;

typedef struct
{
    AudioStreamBasicDescription dataFormat;
    AudioQueueRef queue;
    AudioQueueBufferRef buffers[NUM_BUFFERS];
} PlaybackState;

PlaybackState playbackState;

void AudioOutputCallback (
                                 void                 *inUserData,
                                 AudioQueueRef        inAQ,
                                 AudioQueueBufferRef  inBuffer
                                 )
{    
    if(!abuffLen)
        return;
    int bufflen = 0;
    static int plen = 0;
    bufflen = xplayer_API_prefillaudio(abuff, abuffLen, plen);

    plen += bufflen;
    if(!bufflen){
        bufflen = xplayer_API_getaudio(abuff, abuffLen);
    }
    else
    {
        xplayer_API_getaudio(abuff, -1);
        printf("called getaudio with -1\n");
    }
    if(bufflen)
    {
        memcpy(inBuffer->mAudioData, abuff, bufflen);
        inBuffer->mAudioDataByteSize = bufflen;
        AudioQueueEnqueueBuffer(inAQ, inBuffer, 0, NULL);
    }   
#if 1
    else 
    {
        FBLOG_ERROR("", "empty audio buffer received from libxplayer" );
        bufflen = abuffLen;
        memset(abuff, 0, bufflen);
        memcpy(inBuffer->mAudioData, abuff, bufflen);
        inBuffer->mAudioDataByteSize = bufflen;
        AudioQueueEnqueueBuffer(inAQ, inBuffer, 0, NULL);

        return;

    }
#endif
    return;
}


void SPlayerPluginMac::StaticInitialize()
{
    playbackState.dataFormat.mSampleRate = xplayer_API_getaudio_rate();
    playbackState.dataFormat.mFormatID = kAudioFormatLinearPCM;
    playbackState.dataFormat.mFramesPerPacket = 1;
    playbackState.dataFormat.mChannelsPerFrame = xplayer_API_getaudio_channels();
    playbackState.dataFormat.mBytesPerFrame = 2 * playbackState.dataFormat.mChannelsPerFrame;
    playbackState.dataFormat.mBytesPerPacket = playbackState.dataFormat.mBytesPerFrame * playbackState.dataFormat.mFramesPerPacket;
    playbackState.dataFormat.mBitsPerChannel = 16;
    playbackState.dataFormat.mReserved = 0;
    playbackState.dataFormat.mFormatFlags =
            kLinearPCMFormatFlagIsSignedInteger |
            kLinearPCMFormatFlagIsPacked;

    OSStatus status = AudioQueueNewOutput(&playbackState.dataFormat, AudioOutputCallback, &playbackState, NULL, kCFRunLoopCommonModes, 0, &playbackState.queue);
    FBLOG_INFO("", "Audio queue created "<<status);

    abuffLen = sizeof(char) * xplayer_API_prefilllen();
    
    abuff = (char * ) malloc(abuffLen);
    FBLOG_INFO("", "buffer reserved for  "<< abuffLen << " bytes");
    printf("buffer size: %d\n", abuffLen);
    
    for(int i = 0; i < NUM_BUFFERS; i++)
    {
        AudioQueueAllocateBuffer(playbackState.queue,
                                 xplayer_API_prefilllen(), &playbackState.buffers[i]);
        AudioOutputCallback(&playbackState, playbackState.queue, playbackState.buffers[i]);
    }
    
    UInt32 k = 0;
    status = AudioQueuePrime(playbackState.queue, 10,  &k);
    FBLOG_INFO("", "AudioQueuePrime called " << status << " k: " << k );
    status = AudioQueueStart(playbackState.queue, NULL);
    FBLOG_INFO("", "Audio started " << status);
    return;
}

void SPlayerPluginMac::StaticDeinitialize()
{
    OSStatus status = AudioQueueStop(playbackState.queue, NULL);
    abuffLen=0;
    if(status)
        FBLOG_ERROR("", "audioQueueStop error: " << status);
    if(abuff)
        free(abuff);
    printf("api_done called from staticdeinit()\n");
    xplayer_API_done();
    printf("api_done done from staticdeinit()\n");

    return;
}


SPlayerPluginMac::SPlayerPluginMac(int playerIdentifier, int slotIdentifier) 
:SPlayerPlugin(playerIdentifier, slotIdentifier)
, playerId(playerIdentifier)
, slotId(slotIdentifier)
, pluginWindow(NULL)
, m_layer(NULL) 
, bg(NULL)
, frame(NULL)
{
    FBLOG_INFO("", "SPlayerPluginMac( " << playerId << ", " << slotId << " )");
    NSBundle * bundle = [NSBundle bundleWithIdentifier:@"com.FireBreath.SPlayer"];
    if( bundle == nil )
        FBLOG_ERROR("", "error loading the bundle!");
    const char* path = [[bundle pathForResource:@"bg" ofType:@"png"] UTF8String];
    CGDataProviderRef bgfile = CGDataProviderCreateWithFilename(path);
    bg = CGImageCreateWithPNGDataProvider( bgfile, NULL, true, kCGRenderingIntentDefault);
    CGDataProviderRelease(bgfile);
    boost::optional<std::string> strval;
    strval= getParam("fps");
    double fps = 20;
    if(strval)
        fps = boost::lexical_cast<double>(*strval);
        

    FBLOG_INFO("", "fps param: " << strval << " (" << fps << ")");
}

SPlayerPluginMac::~SPlayerPluginMac()
{
    if (m_layer) {
        [(CALayer*)m_layer removeFromSuperlayer];
        [(CALayer*)m_layer release];
        m_layer = NULL;
    }
    if (bg) {
        CGImageRelease(bg);
    }
    // xplayer_API_slotfree();
}


bool SPlayerPluginMac::onWindowAttached(FB::AttachedEvent* evt, FB::PluginWindowMac* wnd)
{
    if (FB::PluginWindowMac::DrawingModelCoreAnimation == wnd->getDrawingModel() || FB::PluginWindowMac::DrawingModelInvalidatingCoreAnimation == wnd->getDrawingModel()) {
        // Setup CAOpenGL drawing.
        SPCAOpenGLLayer* layer = [[SPCAOpenGLLayer alloc] initWithSlotId:slotId];
        layer.asynchronous = (FB::PluginWindowMac::DrawingModelInvalidatingCoreAnimation == wnd->getDrawingModel()) ? NO : YES;
        layer.autoresizingMask = kCALayerWidthSizable | kCALayerHeightSizable;
        layer.needsDisplayOnBoundsChange = YES;
        m_layer = layer;
        if (FB::PluginWindowMac::DrawingModelInvalidatingCoreAnimation == wnd->getDrawingModel())
            wnd->StartAutoInvalidate(1.0/30.0);
        [(CALayer*) wnd->getDrawingPrimitive() addSublayer:layer];
        
#if 0 // debug CA/iCA
        CATextLayer* txtlayer = [CATextLayer layer];
        txtlayer.string = (FB::PluginWindowMac::DrawingModelInvalidatingCoreAnimation == wnd->getDrawingModel()) ? @"CoreAnimation (Invalidating)" : @"CoreAnimation";
        txtlayer.fontSize = 14;
        txtlayer.autoresizingMask = kCALayerWidthSizable | kCALayerHeightSizable;
        txtlayer.needsDisplayOnBoundsChange = YES;
        [(CALayer*) wnd->getDrawingPrimitive() addSublayer:txtlayer];
#endif
    }
//    pluginWindow = wnd;
//    wnd->StartAutoInvalidate(1.0/2.0);
    return SPlayerPlugin::onWindowAttached(evt,wnd);
}


bool SPlayerPluginMac::onWindowDetached(FB::DetachedEvent* evt, FB::PluginWindowMac* wnd)
{
    xplayer_API_videoprocessdone(slotId);
    return SPlayerPlugin::onWindowDetached(evt,wnd);
}

bool SPlayerPluginMac::onDrawCG(FB::CoreGraphicsDraw *evt, FB::PluginWindowMacCG*)
{
    NSLog(@"drawCG\n");
    FB::Rect bounds(evt->bounds);
    
    CGRect lbounds = CGRectMake(0, 0, bounds.right-bounds.left,bounds.bottom-bounds.top);
    
    CGContextRef context(evt->context);
    
    CGContextSaveGState(context);

#ifdef DEBUG_DISPLAY
    CGContextSelectFont (context, "Helvetica-Bold", 30, kCGEncodingMacRoman);
    
    CGContextSetCharacterSpacing (context, 10);
    CGContextSetTextDrawingMode (context, kCGTextFillStroke);
    
    CGContextSetRGBFillColor (context, 0, 0, 0, 1);
    CGContextSetRGBStrokeColor (context, 1, 1, 1, 1); 
    char cbuff[32];
    sprintf(cbuff, "%d, %d", playerId, slotId);
#endif
        
    if(!xplayer_API_isnewimage(slotId)){      
        while(true) {
            frame = NULL;
            xplayer_API_freeableimage(slotId, &frame);
            if(frame) {
                xplayer_API_freeimage(slotId, frame);
            } else {
                break;
            }
        }
        while(1) {
            vda_frame = NULL;
            xplayer_API_freeablevdaframe(slotId, &vda_frame);
            if(vda_frame) 
                xplayer_API_freevdaframe(slotId, vda_frame);
            else 
                break;
            
        }        
        if(xplayer_API_getstatus(slotId) == 0){
            CGContextTranslateCTM(context, 0, lbounds.size.height);
            CGContextScaleCTM(context, 1.0, -1.0);
            CGContextDrawImage(context, lbounds, bg);
#ifdef DEBUG_DISPLAY
            CGContextShowTextAtPoint (context, 20, 20, cbuff, strlen(cbuff));
#endif
            CGContextRestoreGState(context);
        }
        return true;
    }
    if( xplayer_API_isvda(slotId) )
    {
        xplayer_API_getvdaframe(slotId, &vda_frame);
        if(vda_frame){
//            CVPixelBufferRef cvbuff = (CVPixelBufferRef)xplayer_API_vdaframe2cvbuffer(slotId, vda_frame);
            
              xplayer_API_vdaframedone(slotId);
        }
        
        // HERE 
        
        
        while(1) {
            vda_frame = NULL;
            xplayer_API_freeablevdaframe(slotId, &vda_frame);
            if(vda_frame) 
                xplayer_API_freevdaframe(slotId, vda_frame);
            else 
                break;
        }
        

        
    }
    else
    {
        
        static const size_t kComponentsPerPixel = 4;
        static const size_t kBitsPerComponent = sizeof(unsigned char) * 8;
        
        static const CGColorSpaceRef rgb = CGColorSpaceCreateDeviceRGB();
        
        static double last_cpts = -1;
        
        double cpts = xplayer_API_getrealpts(slotId);
        if(1)
        {      
//            CGColorRef bgColor = CGColorCreateGenericRGB(0.0, 0.0, 0.0, 0.05);
//            CGContextSetFillColorWithColor(context, bgColor);
//            CGContextFillRect(context, lbounds);
//            
//            CGColorRelease(bgColor);
            last_cpts = cpts;
            
            CGContextTranslateCTM(context, 0.0, lbounds.size.height);
            CGContextScaleCTM(context, 1.0, -1.0);
            
            frame = NULL;
            xplayer_API_getimage(slotId, &frame);
            
            if(frame)
            {
                CGDataProviderRef provider = 
                CGDataProviderCreateWithData(NULL, frame->planes[0], frame->height*frame->width*kComponentsPerPixel, NULL);
                
                CGImageRef imageRef = 
                CGImageCreate(frame->width, frame->height, kBitsPerComponent, 
                              kBitsPerComponent * kComponentsPerPixel, 
                              kComponentsPerPixel * frame->width, 
                              rgb, 
                              kCGBitmapByteOrder32Little | kCGImageAlphaFirst, 
                              provider, NULL, true, kCGRenderingIntentDefault);
                
                CGContextDrawImage(context, lbounds, imageRef);
                CGImageRelease(imageRef);
                CGDataProviderRelease(provider);
                xplayer_API_imagedone(slotId);
            }
        }
        while(true) {
            frame = NULL;
            xplayer_API_freeableimage(slotId, &frame);
            if(frame) {
                xplayer_API_freeimage(slotId, frame);
            } else {
                break;
            }
        }
        
    }
#ifdef DEBUG_DISPLAY    
    CGContextSetTextMatrix(context, CGAffineTransformMake(1.0,0.0, 0.0, -1.0, 0.0, 0.0));
    CGContextShowTextAtPoint(context, 20, lbounds.size.height - 20, cbuff, strlen(cbuff));
#endif
    
    CGContextRestoreGState(context);
    
    return true; 
}


