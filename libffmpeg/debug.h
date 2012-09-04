
#ifndef __DEBUG_H__
#define __DEBUG_H__

#ifdef __cplusplus
extern "C" {
#endif

#define USE_DEBUG_LIB           1

#define DEFAULT_DEBUG_ID        4000
#define MAX_DEBUG_SLOT          256

typedef struct {
    int audio_proc;
    int audio_slot;
    int pass;
    unsigned int mpi_alloc;
    unsigned int mpi_free;
    unsigned int vdaframes_pop;
    unsigned int vdaframes_release;
    unsigned long int audio_buffer;

    char* slot[];
} maindebug_t;

typedef struct {
    int uses;
    int slotid;
    double master_clock;
    double av_diff;
    double audio_diff;
    double audio_real_diff;
    int framedrop;
    int aqsize;
    int vqsize;
    int sqsize;
    long long int f1;
    long long int f2;
    double vqtime;
    double aqtime;
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
} slotdebug_t;

void*           debugmem_open(int id);
maindebug_t*    debugmem_getmain(void* dmem);
slotdebug_t*    debugmem_getslot(void* dmem);
void            debugmem_close(void* dmem);

#ifdef __cplusplus
};
#endif

#endif
