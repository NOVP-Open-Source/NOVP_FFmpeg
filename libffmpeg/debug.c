
#include <stdio.h>

#include "debug.h"

#ifdef USE_DEBUG_LIB

#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <errno.h>

typedef struct {
    int id;
    int shmid;
    int size;
    maindebug_t* maindebug;
} debug_t;

static int get_shmem_attach(int shmid) {
    struct shmid_ds smem_ds;

    smem_ds.shm_nattch=0;
    if (shmctl(shmid,IPC_STAT,&smem_ds)==0)
        return smem_ds.shm_nattch-1;
    return -1;
}

static int shmem_open(debug_t* debug)
{
    if(0>(debug->shmid=shmget(debug->id,debug->size, 0666))) {
        return 1;
    }
    if((char*)(debug->maindebug=shmat(debug->shmid,0,0))==(char*)-1) {
        return 1;
    }
    return 0;
}

static int shmem_create(debug_t* debug)
{
    if(0>(debug->shmid=shmget(debug->id, debug->size, 0666 | IPC_CREAT))) {
        if(0>(debug->shmid=shmget(debug->id, debug->size, 0666))) {
            fprintf(stderr,"Error: %s\n",strerror(errno));
            return 1;
        }
        if(get_shmem_attach(debug->shmid)) {
            fprintf(stderr,"Error: shared memory is busy.\n");
            return 1;
        }
        shmctl(debug->shmid, IPC_RMID, 0);          // Delete if unused
        if(0>(debug->shmid=shmget(debug->id, debug->size, 0666 | IPC_CREAT))) {
            fprintf(stderr,"Error: %s\n",strerror(errno));
            return 1;
        }
    }
    if ((char*)(debug->maindebug=shmat(debug->shmid,0,0))==(char*)-1) {
        fprintf(stderr,"Error: %s\n",strerror(errno));
        return 1;
    }
    memset(debug->maindebug,0,debug->size);
    return 0;
}

void* debugmem_open(int id)
{
    debug_t* debug = (debug_t*)calloc(1,sizeof(debug_t));

    if(!id)
        id=DEFAULT_DEBUG_ID;
    debug->id=id;
    debug->size=sizeof(maindebug_t)+sizeof(slotdebug_t)*MAX_DEBUG_SLOT;
    if(shmem_open(debug)) {
        if(shmem_create(debug)) {
            free(debug);
            return NULL;
        }
    }
    return debug;
}
maindebug_t* debugmem_getmain(void* dmem)
{
    debug_t* debug = (debug_t*)dmem;

    if(!dmem)
        return NULL;
    return debug->maindebug;
}

slotdebug_t* debugmem_getslot(void* dmem)
{
    debug_t* debug = (debug_t*)dmem;

    if(!dmem)
        return NULL;
    return (slotdebug_t*)debug->maindebug->slot;
}

void debugmem_close(void* dmem)
{
    debug_t* debug = (debug_t*)dmem;

    if(!dmem)
        return;
    if(debug->maindebug)
        shmdt(debug->maindebug);
    if(get_shmem_attach(debug->shmid)<0) {
        shmctl(debug->shmid, IPC_RMID, 0);
    }
    free(debug);
}

#else
void* debugmem_open(int id)
{
    return NULL;
}

slotdebug_t* debugmem_getptr(void* dmem)
{
    return NULL;
}

void debugmem_close(void* dmem)
{
}
#endif
