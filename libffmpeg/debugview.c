
#include <stdio.h>

#include "debugview.h"
#include "debug.h"

#ifdef USE_DEBUG_LIB

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <term.h>

#define ESC 27

int initialized_flags = 0;
int async_quit_request = 0;

typedef struct {
    unsigned char page;
    unsigned char pause;
    int long_disp;
    int read_disp;
    int count_disp;
    int ffmpeg_disp;
    int mess_disp;
    int call_disp;
    int group_disp;
    int readproc_disp;
    int plugin_disp;
    int thread_disp;
    int line;
    double min_mc;
    double max_mc;
    double min_gc;
    double max_gc;
    double min_ac;
    double max_ac;
    double min_vc;
    double max_vc;
    double min_v;
    double max_v;
    double min_a;
    double max_a;
    double min_ra;
    double max_ra;
} debugview_t;

typedef struct {
    int id;
    char* name;
} apicallnames_t;

static apicallnames_t apicallnames[] = {
    {1  , "xplayer_API_getmovielength()"},
    {2  , "xplayer_API_setimage()"},
    {3  , "xplayer_API_setvda()"},
    {4  , "xplayer_API_isvda()"},
    {5  , "xplayer_API_enableaudio()"},
    {6  , "xplayer_API_setbuffertime()"},
    {7  , "xplayer_API_loadurl()"},
    {8  , "xplayer_API_geturl()"},
    {9  , "xplayer_API_unloadurl()"},
    {10 , "xplayer_API_play()"},
    {11 , "xplayer_API_pause()"},
    {12 , "xplayer_API_flush()"},
    {13 , "xplayer_API_pause_step()"},
    {14 , "xplayer_API_stop()"},
    {15 , "xplayer_API_seek()"},
    {16 , "xplayer_API_seekpos()"},
    {17 , "xplayer_API_seekrel()"},
    {18 , "xplayer_API_volume()"},
    {19 , "xplayer_API_getvolume()"},
    {20 , "xplayer_API_mute()"},
    {21 , "xplayer_API_getmute()"},
    {22 , "xplayer_API_getstatus()"},
    {23 , "xplayer_API_getcurrentpts()"},
    {24 , "xplayer_API_getrealpts()"},
    {25 , "xplayer_API_getfps()"},
    {26 , "xplayer_API_isnewimage()"},
    {27 , "xplayer_API_getimage()"},
    {28 , "xplayer_API_imagedone()"},
    {29 , "xplayer_API_vdaframedone()"},
    {30 , "xplayer_API_freeableimage()"},
    {31 , "xplayer_API_freeimage()"},
    {32 , "xplayer_API_videoprocessdone()"},
    {33 , "xplayer_API_getvdaframe()"},
    {34 , "xplayer_API_freeablevdaframe()"},
    {35 , "xplayer_API_freevdaframe()"},
    {36 , "xplayer_API_setvideocodec()"},
    {37 , "xplayer_API_setaudiocodec()"},
    {38 , "xplayer_API_setsubtitlecodec()"},
    {39 , "xplayer_API_setsynctype()"},
    {40 , "xplayer_API_sethwbuffersize()"},
    {42 , "xplayer_API_getstatusline()"},
    {0, NULL}
};

static apicallnames_t plugincallnames[] = {
    {1  , "play()"},
    {2  , "pause()"},
    {3  , "open()"},
    {4  , "close()"},
    {5  , "seek()"},
    {6  , "seekpos()"},
    {7  , "seekrel()"},
    {8  , "getvideoformat()"},
    {9  , "getaudioformat()"},
    {10 , "getmovielength()"},
    {11 , "geturl()"},
    {12 , "stop()"},
    {13 , "flush()"},
    {14 , "volume()"},
    {15 , "GetVolume()*"},
    {16 , "getstatus()"},
    {17 , "getcurrentpts()"},
    {18 , "getrealpts()"},
    {19 , "getfps()"},
    {20 , "setOptions"},
    {21 , "audiodisable()"},
    {22 , "getstatusline()"},
    {23 , "GetPluginStatus()*"},
    {24 , "Play()*"},
    {25 , "Stop()*"},
    {26 , "Rewind()*"},
    {27 , "Step()*"},
    {28 , "GetTimeScale()*"},
    {29 , "GetDuration()*"},
    {30 , "SetURL()*"},
    {31 , "GetURL()*"},
    {32 , "SetTime()*"},
    {33 , "GetTime()*"},
    {34 , "SetRate()*"},
    {35 , "GetRate()*"},
    {36 , "SetAutoPlay()*"},
    {37 , "GetAutoPlay()*"},
    {38 , "SetVolume()*"},
//    {39 , "GetVolume()*"},
    {40 , "SetMute()*"},
    {41 , "GetMute()*"},
    {42 , "SetResetPropertiesOnReload()*"},
    {43 , "GetResetPropertiesOnReload()*"},
    {44 , "loglevel"},
    {45 , "width"},
    {46 , "height"},
    {47 , "type"},
    {48 , "codebase"},
    {49 , "classid"},
    {50 , "src"},
    {51 , "qtsrc"},
    {52 , "enablehavascript"},
    {53 , "kioskmode"},
    {54 , "controller"},
    {55 , "href"},
//    {56 , ""},
//    {57 , ""},
//    {58 , ""},
//    {59 , ""},
//    {60 , ""},
    {0, NULL}
};

typedef struct {
    char* name;
} threadnames_t;

static char* threadnames[MAX_DEBUG_THREADS] = {
    "process",          // 0
    "refresh",          // 1
    "read",             // 2
    "video",            // 3
    "display",          // 4
    "",                 // 5
    "",                 // 6
    "",                 // 7
};

static void exit_sighandler(int x){
    static int sig_count=0;

    ++sig_count;
    if(initialized_flags==0 && sig_count>1) exit(1);
    if(sig_count==6) exit(1);
    if(sig_count<=1)
        switch(x){
        case SIGINT:
        case SIGQUIT:
        case SIGTERM:
        case SIGKILL:
            async_quit_request = 1;
            return;
        }
    exit(1);
}

static void settermmode(int mode) {
    struct termios t;

    tcgetattr(0,&t);
    if(mode) {
        t.c_lflag &= ~ECHO;
        t.c_lflag &= ~ICANON;
    } else {
        t.c_lflag |= ECHO;
        t.c_lflag |= ICANON;
    }
    tcsetattr(0, TCSADRAIN, &t);
}

static void pad(int n) {
    int i;

    for(i=0;i<n;i++)
        printf(" ");
}

static void status_disp(slotdebug_t* slotdebug, debugview_t* debugview, double mastertime, double playtime)
{
    if(!slotdebug) {
        if(debugview->long_disp || debugview->read_disp) {
            debugview->line++;
            printf("Diff:     %7.2f %7.2f                            (AC: %7.2f VC: %7.2f V: %7.2f A: %7.2f RA: %7.2f) %7.2f %7.2f  %c[K\n",
                    debugview->max_mc-debugview->min_mc,
                    debugview->max_gc-debugview->min_gc,
                    debugview->max_ac-debugview->min_ac,
                    debugview->max_vc-debugview->min_vc,
                    debugview->max_v-debugview->min_v,
                    debugview->max_a-debugview->min_a,
                    debugview->max_ra-debugview->min_ra,
                    debugview->max_ra,debugview->min_ra,
                    ESC
                    );
        }
        return;
    }
    if(debugview->long_disp) {
        printf("Slot: %3d %7.2f %7.2f [%8.3f] %7.2f (AC: %7.2f VC: %7.2f V: %7.2f A: %7.2f RA: %7.2f AL: %8d) A-V:%7.3f PT: %8.3f AE: %7.3f fd=%4d aq=%5dKB vq=%5dKB sq=%5dB f=%lld/%lld QV: %7.3f QA: %7.3f V:%3d A:%3d R:%3d P:%3d S: %3d D: %3d API: %3d AB: %12lld  %c[K\n",
                slotdebug->slotid,                      // %3d
                slotdebug->master_clock,                // %7.2f
                slotdebug->real_clock,                  // %7.2f
                slotdebug->master_clock-mastertime,     // [%8.3f]
                slotdebug->audio_clock,                 // %7.2f
                slotdebug->acpts,                       // AC
                slotdebug->vcpts,                       // VC
                slotdebug->vreadpts,                    // V
                slotdebug->areadpts,                    // A
                slotdebug->audio_real_diff,             // RA
                slotdebug->ablen,                       // AL
                slotdebug->av_diff,
                slotdebug->playtime-playtime,
                slotdebug->audio_diff,
                slotdebug->framedrop,
                slotdebug->aqsize,
                slotdebug->vqsize,
                slotdebug->sqsize,
                slotdebug->f1,
                slotdebug->f2,
                slotdebug->vqtime,
                slotdebug->aqtime,
                slotdebug->video_proc % 1000,
                slotdebug->audio_proc % 1000,
                slotdebug->read_proc % 1000,
                slotdebug->plugin_proc % 1000,
                slotdebug->silence,
                slotdebug->display_proc % 1000,
                slotdebug->apicall,
                slotdebug->abytes,
                ESC
               );
    } else {
#if 0
        printf("Slot: %3d %7.2f %7.2f [%8.3f] %7.2f PQ: %d ",
                slotdebug->slotid,                      // %3d
                slotdebug->master_clock,                // %7.2f
//                slotdebug->real_clock,                  // %7.2f
                slotdebug->master_clock-slotdebug->real_clock,                  // %7.2f
                mastertime-slotdebug->master_clock,     // [%8.3f]
//                slotdebug->audio_clock,                 // %7.2f
                (mastertime-slotdebug->master_clock)-(slotdebug->playtime-playtime),     // %7.2f
                slotdebug->pausereq
               );
#else
        printf("Slot: %3d %7.2f %7.2f [%8.3f] PQ: %d ",
                slotdebug->slotid,                      // %3d
                slotdebug->master_clock,                // %7.2f
                slotdebug->master_clock+slotdebug->timeshift,                  // %7.2f
                slotdebug->timeshift,     // [%8.3f]
                slotdebug->pausereq
               );
#endif
        if(debugview->group_disp) {
            printf(" GI: %3d GP: %d SP: %4.2f ",
                slotdebug->groupid,
                slotdebug->group_paused,
                slotdebug->speed
                );
        }
        if(debugview->read_disp) {
            printf("(AC: %7.2f VC: %7.2f V: %7.2f A: %7.2f RA: %7.2f AL: %8d) ",
                    slotdebug->acpts,                       // AC
                    slotdebug->vcpts,                       // VC
                    slotdebug->vreadpts,                    // V
                    slotdebug->areadpts,                    // A
                    slotdebug->audio_real_diff,             // RA
                    slotdebug->ablen                        // AL
                   );
        }
        printf("A-V:%7.3f PT: %8.3f AE: %7.3f ",
                slotdebug->av_diff,
                slotdebug->playtime-playtime,
                slotdebug->audio_diff
               );
        if(debugview->readproc_disp) {
            printf("RT: %8.3f RN: %3d RP: %2d ",
                    slotdebug->readpts,
                    slotdebug->readno % 1000,
                    slotdebug->readpass
                    );
        }
        if(debugview->ffmpeg_disp) {
            printf("fd=%4d aq=%5dKB vq=%5dKB sq=%5dB f=%lld/%lld ",
                    slotdebug->framedrop,
                    slotdebug->aqsize,
                    slotdebug->vqsize,
                    slotdebug->sqsize,
                    slotdebug->f1,
                    slotdebug->f2
                   );
        }
        printf("QV: %7.3f QA: %7.3f ",
                slotdebug->vqtime,
                slotdebug->aqtime
               );
        if(debugview->count_disp) {
            printf("V:%3d A:%3d R:%3d P:%3d S: %3d D: %3d API: %3d AB: %12lld ",
                    slotdebug->video_proc % 1000,
                    slotdebug->audio_proc % 1000,
                    slotdebug->read_proc % 1000,
                    slotdebug->plugin_proc % 1000,
                    slotdebug->silence,
                    slotdebug->display_proc % 1000,
                    slotdebug->apicall,
                    slotdebug->abytes
                   );
        }
        printf(" %c[K\n",
                ESC
               );
        }
    if(debugview->line) {
        if(slotdebug->master_clock<debugview->min_mc)
            debugview->min_mc=slotdebug->master_clock;
        if(slotdebug->master_clock>debugview->max_mc)
            debugview->max_mc=slotdebug->master_clock;
        if(slotdebug->master_clock+slotdebug->timeshift<debugview->min_gc)
            debugview->min_gc=slotdebug->master_clock+slotdebug->timeshift;
        if(slotdebug->master_clock+slotdebug->timeshift>debugview->max_gc)
            debugview->max_gc=slotdebug->master_clock+slotdebug->timeshift;
        if(slotdebug->acpts<debugview->min_ac)
            debugview->min_ac=slotdebug->acpts;
        if(slotdebug->acpts>debugview->max_ac)
            debugview->max_ac=slotdebug->acpts;
        if(slotdebug->vcpts<debugview->min_vc)
            debugview->min_vc=slotdebug->vcpts;
        if(slotdebug->vcpts>debugview->max_vc)
            debugview->max_vc=slotdebug->vcpts;
        if(slotdebug->vreadpts<debugview->min_v)
            debugview->min_v=slotdebug->vreadpts;
        if(slotdebug->vreadpts>debugview->max_v)
            debugview->max_v=slotdebug->vreadpts;
        if(slotdebug->areadpts<debugview->min_a)
            debugview->min_a=slotdebug->areadpts;
        if(slotdebug->areadpts>debugview->max_a)
            debugview->max_a=slotdebug->areadpts;
        if(slotdebug->audio_real_diff<debugview->min_ra)
            debugview->min_ra=slotdebug->audio_real_diff;
        if(slotdebug->audio_real_diff>debugview->max_ra)
            debugview->max_ra=slotdebug->audio_real_diff;
    } else {
        debugview->min_mc=debugview->max_mc=slotdebug->master_clock;
        debugview->min_gc=debugview->max_gc=slotdebug->master_clock+slotdebug->timeshift;
        debugview->min_ac=debugview->max_ac=slotdebug->acpts;
        debugview->min_vc=debugview->max_vc=slotdebug->vcpts;
        debugview->min_v=debugview->max_v=slotdebug->vreadpts;
        debugview->min_a=debugview->max_a=slotdebug->areadpts;
        debugview->min_ra=debugview->max_ra=slotdebug->audio_real_diff;
    }
    debugview->line++;
    if(debugview->mess_disp) {
        printf("\tMsg: '%s' %c[K\n",
            slotdebug->msg,
            ESC
            );
        debugview->line++;
    }
}

static void call_header(debugview_t* debugview)
{
    int i;
    printf("         ");
    if(debugview->long_disp) {
        if(debugview->page & 0x01) {
            for(i=MAX_DEBUG_APINUM/2;i<MAX_DEBUG_APINUM;i++) {
                printf("  %2d ",i);
            }
        } else {
            for(i=1;i<MAX_DEBUG_APINUM/2;i++) {
                printf("  %2d ",i);
            }
        }
    } else {
        for(i=1;i<MAX_DEBUG_APINUM;i++) {
            printf(" %2d",i);
        }
    }
    printf(" %c[K\n",ESC);
    debugview->line++;
}

static void call_disp(slotdebug_t* slotdebug, debugview_t* debugview)
{
    int i,j,n,l,k,p,h;
    if(!slotdebug) {
        i=0;
        n=0;
        l=0;
        while(apicallnames[i].id) {
            if(strlen(apicallnames[i].name)>l) {
                l=strlen(apicallnames[i].name);
            }
            i++;
            n++;
        }
        h=(n+3)/4;
        printf(" %c[K\n",ESC);
        debugview->line++;
        for(i=0;i<h;i++) {
            for(j=0;j<4;j++) {
                k=j*h+i;
                if(k>=n)
                    continue;
                printf("%02d %s",apicallnames[k].id,apicallnames[k].name);
                p=l-strlen(apicallnames[k].name);
                pad(p);
            }
            printf(" %c[K\n",ESC);
            debugview->line++;
        }
        return;
    }
    printf("Slot: %3d",slotdebug->slotid);
    if(debugview->long_disp) {
        if(debugview->page & 0x01) {
            for(i=MAX_DEBUG_APINUM/2;i<MAX_DEBUG_APINUM;i++) {
                printf(" %04x",slotdebug->proccount[i]);
            }
        } else {
            for(i=1;i<MAX_DEBUG_APINUM/2;i++) {
                printf(" %04x",slotdebug->proccount[i]);
            }
        }
    } else {
        for(i=1;i<MAX_DEBUG_APINUM;i++) {
            printf(" %02x",slotdebug->proccount[i]%256);
        }
    }
    printf(" %c[K\n",ESC);
    debugview->line++;
}

static void plugin_header(debugview_t* debugview)
{
    int i;
    printf("         ");
    if(debugview->long_disp) {
        if(debugview->page & 0x01) {
            for(i=MAX_DEBUG_PLUGINNUM/2;i<MAX_DEBUG_APINUM;i++) {
                printf("  %2d ",i);
            }
        } else {
            for(i=1;i<MAX_DEBUG_PLUGINNUM/2;i++) {
                printf("  %2d ",i);
            }
        }
    } else {
        for(i=1;i<MAX_DEBUG_PLUGINNUM;i++) {
            printf(" %2d",i);
        }
    }
    printf(" %c[K\n",ESC);
    debugview->line++;
}

static void plugin_disp(slotdebug_t* slotdebug, debugview_t* debugview)
{
    int i,j,n,l,k,p,h;
    if(!slotdebug) {
        i=0;
        n=0;
        l=0;
        while(plugincallnames[i].id) {
            if(strlen(plugincallnames[i].name)>l) {
                l=strlen(plugincallnames[i].name);
            }
            i++;
            n++;
        }
        h=(n+3)/4;
        printf(" %c[K\n",ESC);
        debugview->line++;
        for(i=0;i<h;i++) {
            for(j=0;j<4;j++) {
                k=j*h+i;
                if(k>=n)
                    continue;
                printf("%02d %s",plugincallnames[k].id,plugincallnames[k].name);
                p=l-strlen(plugincallnames[k].name);
                pad(p);
            }
            printf(" %c[K\n",ESC);
            debugview->line++;
        }
        return;
    }
    printf("Slot: %3d",slotdebug->slotid);
    if(debugview->long_disp) {
        if(debugview->page & 0x01) {
            for(i=MAX_DEBUG_PLUGINNUM/2;i<MAX_DEBUG_PLUGINNUM;i++) {
                printf(" %04x",slotdebug->plugincount[i]);
            }
        } else {
            for(i=1;i<MAX_DEBUG_PLUGINNUM/2;i++) {
                printf(" %04x",slotdebug->plugincount[i]);
            }
        }
    } else {
        for(i=1;i<MAX_DEBUG_PLUGINNUM;i++) {
            printf(" %02x",slotdebug->plugincount[i]%256);
        }
    }
    printf(" %c[K\n",ESC);
    debugview->line++;
}

static void thread_header(debugview_t* debugview)
{
    int i,l,p;

    printf("         |");
    i=0;
    l=strlen(" proc  run   load");
    for(i=0;i<MAX_DEBUG_THREADS;i++) {
        if(threadnames[i]) {
            p=l-strlen(threadnames[i]);
            pad(p);
            printf("%s",threadnames[i]);
        } else {
            pad(l);
        }
        printf("|");
    }
    printf(" %c[K\n",ESC);
    debugview->line++;
    printf("         |");
    for(i=0;i<MAX_DEBUG_THREADS;i++) {
        printf(" proc  run   load|");
    }
    printf(" %c[K\n",ESC);
    debugview->line++;
}

static void thread_disp(slotdebug_t* slotdebug, debugview_t* debugview)
{
    int i;
    double load;
    if(!slotdebug) {
        return;
    }
    printf("Slot: %3d|",slotdebug->slotid);
    for(i=0;i<MAX_DEBUG_THREADS;i++) {
        load = 0.0;
        if(slotdebug->thread_time[i].proc>0.0 && slotdebug->thread_time[i].run>0.0)
            load=slotdebug->thread_time[i].proc/slotdebug->thread_time[i].run;
        printf("%5.1f %5.1f %4.1f%%|",slotdebug->thread_time[i].proc,slotdebug->thread_time[i].run,load*100.0);
    }
    printf(" %c[K\n",ESC);
    debugview->line++;
    if(debugview->mess_disp) {
        printf("\tMsg: '%s' %c[K\n",
            slotdebug->msg,
            ESC
            );
        debugview->line++;
    }
}

void help_info(debugview_t* debugview)
{
    printf("%s %c[K\n",
            "next page: <n>, prev page: <r>, pause: <p>, exit: <enter>",
            ESC
            );
    debugview->line++;
}

int main(int argc, char** argv)
{
    void* debug = NULL;
    maindebug_t* maindebug = NULL;
    slotdebug_t* slotdebug = NULL;
    debugview_t* debugview = calloc(sizeof(debugview_t),1);
    fd_set fds;
    struct timeval tv;
    char termbuf[64];
    double playtime;
    double mastertime;
    int lastline = 0;
    int i;

    for(i=1;i<argc;i++) {
        if(!strcmp(argv[i],"-d")) {
            debugview->read_disp=1;
            debugview->count_disp=1;
            debugview->ffmpeg_disp=1;
        }
        if(!strcmp(argv[i],"-l"))
            debugview->long_disp=1;
        if(!strcmp(argv[i],"-r"))
            debugview->read_disp=1;
        if(!strcmp(argv[i],"-rp"))
            debugview->readproc_disp=1;
        if(!strcmp(argv[i],"-n"))
            debugview->count_disp=1;
        if(!strcmp(argv[i],"-f"))
            debugview->ffmpeg_disp=1;
        if(!strcmp(argv[i],"-m"))
            debugview->mess_disp=1;
        if(!strcmp(argv[i],"-g"))
            debugview->group_disp=1;
        if(!strcmp(argv[i],"-c"))
            debugview->call_disp=1;
        if(!strcmp(argv[i],"-p"))
            debugview->plugin_disp=1;
        if(!strcmp(argv[i],"-t"))
            debugview->thread_disp=1;
        if(!strcmp(argv[i],"-h")) {
            fprintf(stderr,"Usage: %s [-l][-r][-rp][-n][-g][-f][-m][-c][-p][-t][-h]\n",argv[0]);
            fprintf(stderr,"\t-d:  default info (read, count and ffmpeg)\n");
            fprintf(stderr,"\t-r:  read info\n");
            fprintf(stderr,"\t-rp: read proc info\n");
            fprintf(stderr,"\t-n:  count info\n");
            fprintf(stderr,"\t-g:  group info\n");
            fprintf(stderr,"\t-f:  ffmpeg info\n");
            fprintf(stderr,"\t-l:  long info\n");
            fprintf(stderr,"\t-m:  messages\n");
            fprintf(stderr,"\t-c:  call page\n");
            fprintf(stderr,"\t-p:  plugin page\n");
            fprintf(stderr,"\t-t:  thread page\n");
            fprintf(stderr,"\t-h:  this list\n");
            return 0;
        }
    }
    if(!(debug=debugmem_open(0))) {
        fprintf(stderr,"Can't open debug shared mem.\n");
        return 1;
    }

    signal(SIGTERM,exit_sighandler);
    signal(SIGINT,exit_sighandler);
    signal(SIGQUIT,exit_sighandler);

    maindebug = debugmem_getmain(debug);
    slotdebug = debugmem_getslot(debug);
    printf("%c[2J",ESC);
    fflush(stdout);
    settermmode(1);
    while(slotdebug && !async_quit_request)
    {
        debugview->line=0;
        printf("%c[1;1H",ESC);
        fflush(stdout);
        double load = 0.0;
        playtime=0.0;
        mastertime=0.0;
        for(i=0;i<MAX_DEBUG_SLOT;i++) {
            if(!slotdebug[i].uses)
                continue;
            if(slotdebug[i].playtime==0.0)
                continue;
            if(playtime==0.0) {
                mastertime=slotdebug[i].master_clock;
                playtime=slotdebug[i].playtime;
            }
            if(playtime>slotdebug[i].playtime) {
                mastertime=slotdebug[i].master_clock;
                playtime=slotdebug[i].playtime;
            }
        }
        if(!debugview->pause) {
            if(maindebug->thread_time.proc>0.0 && maindebug->thread_time.run>0.0)
                load=maindebug->thread_time.proc/maindebug->thread_time.run;
                printf("Debug view v0.1 Audio: %3d Slot: %3d pass: %d proc: %5.1f run: %5.1f load: %4.1f%% master buffer: %12ld mpi: %d %d => %d vda frames: %d %d => %d %c[K\n\n",
                    maindebug->audio_proc % 1000,maindebug->audio_slot,maindebug->pass,
                    maindebug->thread_time.proc,maindebug->thread_time.run,load*100.0,
                    maindebug->audio_buffer,
                    maindebug->mpi_alloc,maindebug->mpi_free,maindebug->mpi_alloc-maindebug->mpi_free,
                    maindebug->vdaframes_pop,maindebug->vdaframes_release,maindebug->vdaframes_pop-maindebug->vdaframes_release,
                    ESC);
            if(debugview->call_disp) {
                call_header(debugview);
            } else if(debugview->plugin_disp) {
                plugin_header(debugview);
            } else if(debugview->thread_disp) {
                thread_header(debugview);
            }
            for(i=0;i<MAX_DEBUG_SLOT;i++) {
                if(!slotdebug[i].uses)
                    continue;
                if(debugview->call_disp) {
                    call_disp(&slotdebug[i], debugview);
                } else if(debugview->plugin_disp) {
                    plugin_disp(&slotdebug[i], debugview);
                } else if(debugview->thread_disp) {
                    thread_disp(&slotdebug[i], debugview);
                } else {
                    status_disp(&slotdebug[i], debugview, mastertime, playtime);
                }
            }

            if(debugview->call_disp) {
                call_disp(NULL, debugview);
            } else if(debugview->plugin_disp) {
                plugin_disp(NULL, debugview);
            } else if(debugview->thread_disp) {
                thread_disp(NULL, debugview);
            } else {
                status_disp(NULL, debugview, mastertime, playtime);
            }
            help_info(debugview);
            for(i=debugview->line;i<lastline;i++) {
                printf("%c[K\n",ESC);
            }
        }

        fflush(stdout);
        lastline=debugview->line;

        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO,&fds);
        tv.tv_sec=0;
        tv.tv_usec=100000;
        if(0<=(select(STDIN_FILENO+1,&fds,NULL,NULL,&tv))) {
            if(FD_ISSET(STDIN_FILENO,&fds)) {
                memset(termbuf,0,64);
                read(STDIN_FILENO,termbuf,64);
                if(strchr(termbuf,'\n'))
                    break;
                if(strchr(termbuf,'n'))
                    debugview->page++;
                if(strchr(termbuf,'r')) {
                    for(i=0;i<MAX_DEBUG_SLOT;i++) {
                        if(!slotdebug[i].uses)
                            continue;
                        memset(slotdebug[i].proccount,0,sizeof(unsigned int)*MAX_DEBUG_APINUM);
                        memset(slotdebug[i].plugincount,0,sizeof(unsigned int)*MAX_DEBUG_APINUM);
                    }
                }
                if(strchr(termbuf,'p'))
                    debugview->pause=1-debugview->pause;
            }
        }
    }
    settermmode(0);
    printf("%c[2J",ESC);
    fflush(stdout);
    fprintf(stderr,"Bye.\n");
    debugmem_close(debug);
    free(debugview);
    return 0;
}

#else

int main(int argc, char** argv)
{
    fprintf(stderr,"Debug lib not supported.\n");
    return 0;
}

#endif

