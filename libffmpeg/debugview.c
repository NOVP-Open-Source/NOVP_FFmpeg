
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

#define ESC 27

int initialized_flags = 0;
int async_quit_request = 0;

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

int main(int argc, char** argv)
{
    void* debug = NULL;
    maindebug_t* maindebug = NULL;
    slotdebug_t* slotdebug = NULL;
    fd_set fds;
    struct timeval tv;
    char termbuf[64];
    int r;
    int line = 0;
    int lastline = 0;
    int i;
    int long_disp=0;
    int mess_disp=0;
    double min_mc;
    double max_mc;
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

    for(i=1;i<argc;i++) {
        if(!strcmp(argv[i],"-l"))
            long_disp=1;
        if(!strcmp(argv[i],"-m"))
            mess_disp=1;
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
    while(slotdebug && !async_quit_request)
    {
        line=0;
        printf("%c[1;1H",ESC);
        fflush(stdout);
        printf("Debug view v0.1 Audio: %3d Slot: %3d pass: %d  master buffer: %12ld mpi: %d %d => %d vda frames: %d %d => %d %c[K\n\n",
                maindebug->audio_proc % 1000,maindebug->audio_slot,maindebug->pass,maindebug->audio_buffer,
                maindebug->mpi_alloc,maindebug->mpi_free,maindebug->mpi_alloc-maindebug->mpi_free,
                maindebug->vdaframes_pop,maindebug->vdaframes_release,maindebug->vdaframes_pop-maindebug->vdaframes_release,
                ESC);
        for(i=0;i<MAX_DEBUG_SLOT;i++) {
            if(!slotdebug[i].uses)
                continue;
            if(long_disp) {
                printf("Slot: %3d %7.2f (AC: %7.2f VC: %7.2f V: %7.2f A: %7.2f RA: %7.2f AL: %8d) A-V:%7.3f AE: %7.3f fd=%4d aq=%5dKB vq=%5dKB sq=%5dB f=%lld/%lld QV: %7.3f QA: %7.3f V:%3d A:%3d R:%3d P:%3d S: %3d D: %3d API: %3d AB: %12lld  %c[K\n",
                        slotdebug[i].slotid,
                        slotdebug[i].master_clock,
                        slotdebug[i].acpts,
                        slotdebug[i].vcpts,
                        slotdebug[i].vreadpts,
                        slotdebug[i].areadpts,
                        slotdebug[i].audio_real_diff,
                        slotdebug[i].ablen,
                        slotdebug[i].av_diff,
                        slotdebug[i].audio_diff,
                        slotdebug[i].framedrop,
                        slotdebug[i].aqsize,
                        slotdebug[i].vqsize,
                        slotdebug[i].sqsize,
                        slotdebug[i].f1,
                        slotdebug[i].f2,
                        slotdebug[i].vqtime,
                        slotdebug[i].aqtime,
                        slotdebug[i].video_proc % 1000,
                        slotdebug[i].audio_proc % 1000,
                        slotdebug[i].read_proc % 1000,
                        slotdebug[i].plugin_proc % 1000,
                        slotdebug[i].silence,
                        slotdebug[i].display_proc % 1000,
                        slotdebug[i].apicall,
                        slotdebug[i].abytes,
                        ESC
                       );
            } else {
                printf("Slot: %3d %7.2f A-V:%7.3f AE: %7.3f fd=%4d aq=%5dKB vq=%5dKB sq=%5dB f=%lld/%lld QV: %7.3f QA: %7.3f V:%3d A:%3d R:%3d P:%3d S: %3d D: %3d API: %3d %c[K\n",
                        slotdebug[i].slotid,
                        slotdebug[i].master_clock,
                        slotdebug[i].av_diff,
                        slotdebug[i].audio_diff,
                        slotdebug[i].framedrop,
                        slotdebug[i].aqsize,
                        slotdebug[i].vqsize,
                        slotdebug[i].sqsize,
                        slotdebug[i].f1,
                        slotdebug[i].f2,
                        slotdebug[i].vqtime,
                        slotdebug[i].aqtime,
                        slotdebug[i].video_proc % 1000,
                        slotdebug[i].audio_proc % 1000,
                        slotdebug[i].read_proc % 1000,
                        slotdebug[i].plugin_proc % 1000,
                        slotdebug[i].silence,
                        slotdebug[i].display_proc % 1000,
                        slotdebug[i].apicall,
                        ESC
                       );
                }
            if(line) {
                if(slotdebug[i].master_clock<min_mc)
                    min_mc=slotdebug[i].master_clock;
                if(slotdebug[i].master_clock>max_mc)
                    max_mc=slotdebug[i].master_clock;
                if(slotdebug[i].acpts<min_ac)
                    min_ac=slotdebug[i].acpts;
                if(slotdebug[i].acpts>max_ac)
                    max_ac=slotdebug[i].acpts;
                if(slotdebug[i].vcpts<min_vc)
                    min_vc=slotdebug[i].vcpts;
                if(slotdebug[i].vcpts>max_vc)
                    max_vc=slotdebug[i].vcpts;
                if(slotdebug[i].vreadpts<min_v)
                    min_v=slotdebug[i].vreadpts;
                if(slotdebug[i].vreadpts>max_v)
                    max_v=slotdebug[i].vreadpts;
                if(slotdebug[i].areadpts<min_a)
                    min_a=slotdebug[i].areadpts;
                if(slotdebug[i].areadpts>max_a)
                    max_a=slotdebug[i].areadpts;
                if(slotdebug[i].audio_real_diff<min_ra)
                    min_ra=slotdebug[i].audio_real_diff;
                if(slotdebug[i].audio_real_diff>max_ra)
                    max_ra=slotdebug[i].audio_real_diff;
            } else {
                min_mc=max_mc=slotdebug[i].master_clock;
                min_ac=max_ac=slotdebug[i].acpts;
                min_vc=max_vc=slotdebug[i].vcpts;
                min_v=max_v=slotdebug[i].vreadpts;
                min_a=max_a=slotdebug[i].areadpts;
                min_ra=max_ra=slotdebug[i].audio_real_diff;
            }
            line++;
            if(mess_disp) {
                printf("\tMsg: '%s' %c[K\n",
                    slotdebug[i].msg,
                    ESC
                    );
                line++;
            }
        }
        if(long_disp) {
            line++;
            printf("Diff:     %7.2f (AC: %7.2f VC: %7.2f V: %7.2f A: %7.2f RA: %7.2f) %7.2f %7.2f  %c[K\n",
                    max_mc-min_mc,
                    max_ac-min_ac,
                    max_vc-min_vc,
                    max_v-min_v,
                    max_a-min_a,
                    max_ra-min_ra,
                    max_ra,min_ra,
                    ESC
                    );
        }
        for(i=line;i<lastline;i++) {
            printf("%c[K\n",ESC);
        }
        fflush(stdout);
        lastline=line;

        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO,&fds);
        tv.tv_sec=0;
        tv.tv_usec=100000;
        if(0<=(select(STDIN_FILENO+1,&fds,NULL,NULL,&tv))) {
            if(FD_ISSET(STDIN_FILENO,&fds)) {
                memset(termbuf,0,64);
                r=read(STDIN_FILENO,termbuf,64);
                if(strchr(termbuf,'\n'))
                    break;
            }
        }
    }
    printf("%c[2J",ESC);
    fflush(stdout);
    fprintf(stderr,"Bye.\n");
    debugmem_close(debug);
    return 0;
}

#else

int main(int argc, char** argv)
{
    fprintf(stderr,"Debug lib not supported.\n");
    return 0;
}

#endif

