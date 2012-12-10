
#ifndef __DEBUG_H__
#define __DEBUG_H__

#ifdef __cplusplus
extern "C" {
#endif

#define USE_DEBUG_LIB           1

#define DEFAULT_DEBUG_ID        4000
#define MAX_DEBUG_SLOT          256
#define MAX_DEBUG_APINUM        64
#define MAX_DEBUG_PLUGINNUM     64
#define MAX_DEBUG_THREADS       8

#define PROCESS_THREAD_ID       0
#define REFRESH_THREAD_ID       1
#define READ_THREAD_ID          2
#define VIDEO_THREAD_ID         3
#define DISPLAY_THREAD_ID       4

typedef struct {
    double      proc;
    double      run;
} thread_times_t;

typedef struct {
    int audio_proc;
    int audio_slot;
    int pass;
    unsigned int mpi_alloc;
    unsigned int mpi_free;
    unsigned int vdaframes_pop;
    unsigned int vdaframes_release;
    unsigned long int audio_buffer;
    thread_times_t thread_time;

    char* slot[];
} maindebug_t;

typedef struct {
    int uses;
    int slotid;
    double master_clock;
    double real_clock;
    double av_diff;
    double audio_diff;
    double audio_real_diff;
    double audio_clock;
    int framedrop;
    int aqsize;
    int vqsize;
    int sqsize;
    long long int f1;
    long long int f2;
    double vqtime;
    double aqtime;
    int pausereq;
    unsigned int video_proc;
    unsigned int audio_proc;
    unsigned int read_proc;
    unsigned int plugin_proc;
    unsigned int silence;
    unsigned int display_proc;
    double vreadpts;
    double areadpts;
    double acpts;
    double vcpts;
    unsigned long long int abytes;
    unsigned int ablen;
    unsigned int apicall;
    char msg[512];
    double playtime;
    unsigned int proccount[MAX_DEBUG_APINUM];
    unsigned int plugincount[MAX_DEBUG_PLUGINNUM];
    thread_times_t thread_time[MAX_DEBUG_THREADS];

    int groupid;
    double group_clock;
    double timeshift;
    int group_paused;

    double readpts;
    int readno;
    int readpass;
    int readpassid;
    double speed;
} slotdebug_t;

void*           debugmem_open(int id);
maindebug_t*    debugmem_getmain(void* dmem);
slotdebug_t*    debugmem_getslot(void* dmem);
void            debugmem_close(void* dmem);

#ifdef __cplusplus
};
#endif

#endif
