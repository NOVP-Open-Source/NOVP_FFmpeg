/*
 *  Copyright (C) 2007 Attila Ötvös <attila@onebithq.com>
 *
 *  MPlayer is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  MPlayer is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with MPlayer; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */


#include <unistd.h>
#include <stdarg.h>
#include <sys/time.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#ifdef HAVE_SEMAPHORE
#include <sys/sem.h>
#endif
#include <signal.h>
#ifdef __MINGW32__
#define SIGUSR2 12
#endif

#include "libshmem/libshmem.h"

typedef struct SHMEM_Header {
    char headerid[2];
} SHMEM_Header;

typedef struct SHMEMContext {
    int shmem_id;
    int shmem_fd;

    char *ids;
    int mode;
    int id;
    int type;
    unsigned int size;
    int shmid;
    int semid;
    int flag;
    int ptr;
    int bytes;
    int started;
    char* data;
    char* shmem;
    SHMEM_Header header;
} SHMEMContext;

//--------------------------------------
int stream_at_eof = 0;
pid_t parent_pid = 0;
int child_start = 0;

#define STREAM_READ	0
#define STREAM_WRITE	1

#define STREAM_OK	0
#define STREAM_UNSUPPORTED -1
#undef HAVE_MPMSG
//--------------------------------------

#undef HAVE_SEMAPHORE                    // disable semaphore support
//#define HAVE_SEMAPHORE 1                   // enable semaphore support

#define WAIT_TIME          100             // 0.1 ms
#define SHMEM_SIZE         0x10000         // 64kB

#ifdef HAVE_SEMAPHORE

#define SEM_FILE          "/dev/null"      // file with create unique semID
#define SEM_NUM           1                // semaphores num
#define SEM_INIT          1                // semaphore init values
#define SEM_USLEEP        1000             // usleep
#define SEM_WAIT_TIME     20               // wait time = 20*1000 usec
#define SEM_SYNC_TIME     1000             // wait time = 1000*1000 usec
#define SEM_REQUIRE       1                // if can't open semaphore then exit
#define SEM_MEMBER        0                // semaphore num to read/write

#define SEM_OPEN          0                // reader - open exist semaphore
#define SEM_CREATE        1                // writer - create new semaphore

#if !defined(__GNU_LIBRARY__) || defined(_SEM_SEMUN_UNDEFINED)
// semun undefined in <sys/sem.h>
union semun
{
    int val;
    struct semid_ds *buf;
    unsigned short int *array;
    struct seminfo *__buf;
};
#endif
#endif

static int shmem_type = SHMEM_TYPE_PIPE;        // default type: pipe
static unsigned int shmem_size = SHMEM_SIZE;    // default size

#ifdef HAVE_SEMAPHORE
/**
 * \brief get semaphore values
 * \param semid: semaphore ID
 * \param member: semaphore number
 * \return semaphore value
 */
static int semgetval(int semid, int member) {
    return semctl(semid, member, GETVAL, 0);
}

/**
 * \brief open semaphore
 * \param shmid: shared memory ID
 * \param writer: 0 - reader (open semaphore), 1 - writer (create semaphore)
 * \return semaphore ID, -1 on error
 */
static int semopen(int shmid, int writer) {
    int cpid, semid, i;
    key_t key;
    struct shmid_ds shmem_ds;
    union semun semopts;

    memset(&shmem_ds,0,sizeof(shmem_ds));
    cpid = 0;
    if (shmctl(shmid,IPC_STAT,&shmem_ds)==0)
        cpid = shmem_ds.shm_cpid;
    key = ftok(SEM_FILE,cpid);
#ifdef HAVE_MPMSG
    mp_msg(MSGT_OPEN,MSGL_V,"semaphore cpid: %d key: %X\n",cpid,key);
#endif
    if (writer) {
        if ((semid = semget(key, SEM_NUM, IPC_CREAT|IPC_EXCL|0666)) == -1) {
            if ((semid = semget(key, 0, 0666))!=-1)
                semctl(semid, IPC_RMID, 0);
            if ((semid = semget(key, SEM_NUM, IPC_CREAT|IPC_EXCL|0666)) == -1) {
#ifdef HAVE_MPMSG
                mp_msg(MSGT_OPEN,MSGL_ERR,
                       "Can't open shemaphore. (%s)\n",strerror(errno));
#endif
            return -1;
            }
        }
        semopts.val = SEM_INIT;
        for(i=0; i<SEM_NUM; i++)
            semctl(semid, i, SETVAL, semopts);
    } else {
        if ((semid = semget(key, 0, 0666))==-1) {
#ifdef HAVE_MPMSG
            mp_msg(MSGT_OPEN,MSGL_ERR,
                   "Can't open shemaphore. (%s)\n",strerror(errno));
#endif
            return -1;
        }
    }
    return semid;
}

/**
 * \brief check member is valid
 * \param semid: semaphore ID
 * \param member: semaphore number
 * \return 0 - on error, 1 - member is valid
 */
static int semismember(int semid, int member) {
    int membernum;
    union semun semopts;
    struct semid_ds semids;

    if (semid==-1) return 0;
    semopts.buf = &semids;
    semctl(semid, 0, IPC_STAT, semopts);
    membernum=semopts.buf->sem_nsems;
    if (member<0 || member>(membernum-1))
        return 1;
    return 0;
}

/**
 * \brief semaphore lock (no wait)
 * \param semid: semaphore ID
 * \param member: semaphore number
 * \return 0 - on error
 *         1 - OK
 *        -1 - semaphore locked
 */
static int semlock(int semid, int member) {
    struct sembuf sem_lock = { 0, -1, IPC_NOWAIT };

    if (semismember(semid, member)) return 0;
    if (!semgetval(semid, member)) return -1;
    sem_lock.sem_num = member;
    if ((semop(semid, &sem_lock, 1)) == -1) return 0;
    return 1;
}

/**
 * \brief semaphore lock (wait)
 * \param semid: semaphore ID
 * \param member: semaphore number
 * \return 0 - on error
 *         1 - OK
 */
static int semlockwait(int semid, int member) {
    struct sembuf sem_lock = { 0, -1, 0 };

    if (semismember(semid, member)) return 0;
    sem_lock.sem_num = member;
    if ((semop(semid, &sem_lock, 1)) == -1) return 0;
    return 1;
}

/**
 * \brief semaphore unlock
 * \param semid: semaphore ID
 * \param member: semaphore number
 */
static void semunlock(int semid, int member) {
    struct sembuf sem_unlock = { member, 1, IPC_NOWAIT };
    int semval;

    if (semismember(semid, member)) return;
    semval = semgetval(semid, member);
    if (semval == SEM_INIT) return;
    sem_unlock.sem_num = member;
    if ((semop(semid, &sem_unlock, 1)) == -1) return;
    return;
}
#define sem_lock(semid,member) semlock(semid,member)
#define sem_unlock(semid,member) semunlock(semid,member)
#else
#define sem_lock(semid,member) {}
#define sem_unlock(semid,member) {}
#endif

int set_shmem_size(int type, unsigned int size) {
    switch(type) {
    case SHMEM_TYPE_RAWDATA:
        if(!size)
            return 1;
        shmem_type=SHMEM_TYPE_RAWDATA;
        shmem_size=size+sizeof(shmem_data_t);
        return 0;
    case SHMEM_TYPE_PIPE:
        shmem_type=SHMEM_TYPE_PIPE;
        shmem_size=SHMEM_SIZE;
        return 0;
    }
    return 1;
}

/**
 * \brief get number of attach clients
 * \param semid: shmem ID
 * \return: clients number
 */
static int get_shmem_attach(int shmid) {
    struct shmid_ds smem_ds;

    smem_ds.shm_nattch=0;
    if (shmctl(shmid,IPC_STAT,&smem_ds)==0)
        return smem_ds.shm_nattch-1;
    return 0;
}

/**
 * \brief get memory size
 * \param semid: shmem ID
 * \return: memory size
 */
static int get_shmem_size(int shmid) {
    struct shmid_ds smem_ds;

    smem_ds.shm_segsz=0;
    if (shmctl(shmid,IPC_STAT,&smem_ds)==0)
        return smem_ds.shm_segsz;
    return 0;
}

static int fill_buffer_raw(SHMEMContext *s, unsigned char* buffer, int max_len) {
    shmem_data_t *shmem_data;
    int len=-1;
    char *ptr;

    if (s->bytes) {
        len=FFMIN(s->bytes,max_len);
        ptr=s->data+s->ptr;
        memcpy(buffer,ptr,len);
        s->ptr+=len;
        s->bytes-=len;
        return len;
    }
#ifdef HAVE_SEMAPHORE
    if (s->semid!=-1) {
        if (!semlockwait(s->semid, SEM_MEMBER))
            return -1;
    }
#endif
    shmem_data=(shmem_data_t*)s->shmem;
    if (!get_shmem_attach(s->shmid)) {
        sem_unlock(s->semid, SEM_MEMBER);
        return -1;
    }
//    s->bytes=FFMIN(s->size,shmem_data->size)+sizeof(SHMEM_Header);
    s->bytes=FFMIN(s->size,shmem_data->size);
    s->ptr=0;
//    memcpy(s->data,&s->header,sizeof(SHMEM_Header));
//    memcpy(s->data+sizeof(SHMEM_Header),shmem_data->data,s->bytes);
    memcpy(s->data,shmem_data->data,s->bytes);
    sem_unlock(s->semid, SEM_MEMBER);
    len=FFMIN(s->bytes,max_len);
    ptr=s->data+s->ptr;
    memcpy(buffer,ptr,len);
    s->ptr+=len;
    s->bytes-=len;
    return len;
}

static int fill_buffer_pipe(SHMEMContext *s, unsigned char* buffer, int max_len) {
    shmem_data_t *shmem_data;
    int len=-1;
    char *ptr;

    if (s->bytes) {
        len=FFMIN(s->bytes,max_len);
        ptr=s->data+s->ptr;
        memcpy(buffer,ptr,len);
        s->ptr+=len;
        s->bytes-=len;
        return len;
    }
    sem_lock(s->semid, SEM_MEMBER);
    shmem_data=(shmem_data_t*)s->shmem;
    if(shmem_data->mplayerid[0]!='M' || shmem_data->mplayerid[1]!='P') {
        sem_unlock(s->semid, SEM_MEMBER);
        return -1;
    }
    if (stream_at_eof || is_shmem_eof(shmem_data->flag)) {
        sem_unlock(s->semid, SEM_MEMBER);
        return -1;
    }
    if (!get_shmem_attach(s->shmid)) {
        sem_unlock(s->semid, SEM_MEMBER);
        return -1;
    }
    while (!is_shmem_writed(shmem_data->flag)) {
        sem_unlock(s->semid, SEM_MEMBER);
        usleep(WAIT_TIME);
        sem_lock(s->semid, SEM_MEMBER);
        if (stream_at_eof || is_shmem_eof(shmem_data->flag)) {
            sem_unlock(s->semid, SEM_MEMBER);
            return -1;
        }
        if (!get_shmem_attach(s->shmid)) {
            sem_unlock(s->semid, SEM_MEMBER);
            return -1;
        }
    }
    s->bytes=FFMIN(s->size-sizeof(shmem_data_t),shmem_data->size)+sizeof(SHMEM_Header);
    s->ptr=0;
    memcpy(s->data,&s->header,sizeof(SHMEM_Header));
    memcpy(s->data+sizeof(SHMEM_Header),shmem_data->data,s->bytes);
    shmem_data->flag&=~SHMEM_FLAG_WRITED;
    sem_unlock(s->semid, SEM_MEMBER);
    len=FFMIN(s->bytes,max_len);
    ptr=s->data+s->ptr;
    memcpy(buffer,ptr,len);
    s->ptr+=len;
    s->bytes-=len;
    return len;
}

static int write_buffer_raw(SHMEMContext *s, unsigned char* buffer, int len) {
    shmem_data_t *shmem_data;
    unsigned int max_len=FFMIN(s->size,(unsigned int)len);
#ifdef HAVE_SEMAPHORE
    int i;
    int lockstatus;

    if(s->semid != -1) {
        for(i=0;i<SEM_WAIT_TIME;i++) {
            lockstatus = semlock(s->semid, SEM_MEMBER);
            if(lockstatus !=-1) break;
            usleep(SEM_USLEEP);
        }
    if (lockstatus == -1) return len;              // skip frame
    if (lockstatus == 0) return -1;                // error
    }
#endif
    shmem_data=(shmem_data_t*)s->shmem;
    shmem_data->mplayerid[0]='M';
    shmem_data->mplayerid[1]='P';
    shmem_data->type=s->type;
    shmem_data->size=len;
    memcpy(shmem_data->data,buffer,max_len);
    sem_unlock(s->semid, SEM_MEMBER);
    return max_len;
}

static int write_buffer_pipe(SHMEMContext *s, unsigned char* buffer, int len) {
    shmem_data_t *shmem_data;
    int max_len=FFMIN(s->size-sizeof(shmem_data_t),(unsigned int)len);
    int attached=0;

    sem_lock(s->semid, SEM_MEMBER);
    shmem_data=(shmem_data_t*)s->shmem;
    if (stream_at_eof || is_shmem_eof(shmem_data->flag)) {
        sem_unlock(s->semid, SEM_MEMBER);
        return -1;
    }
    attached=get_shmem_attach(s->shmid);
    if (!attached && s->started) {
        sem_unlock(s->semid, SEM_MEMBER);
        return -1;
    }
    while (is_shmem_writed(shmem_data->flag)) {
        sem_unlock(s->semid, SEM_MEMBER);
        usleep(WAIT_TIME);
        sem_lock(s->semid, SEM_MEMBER);
        if (stream_at_eof || is_shmem_eof(shmem_data->flag)) {
            sem_unlock(s->semid, SEM_MEMBER);
            return -1;
        }
        if (s->started && !get_shmem_attach(s->shmid)) {
            sem_unlock(s->semid, SEM_MEMBER);
            return -1;
        }
    }
    shmem_data->mplayerid[0]='M';
    shmem_data->mplayerid[1]='P';
    shmem_data->type=s->type;
    shmem_data->size=max_len;
    memcpy(shmem_data->data,buffer,max_len);
    shmem_data->flag|=SHMEM_FLAG_WRITED;
    sem_unlock(s->semid, SEM_MEMBER);
    if (parent_pid && !child_start) {
        kill(parent_pid,SIGUSR2);
        child_start=1;
    }
    if (attached) s->started=1;
    return max_len;
}


#if 0
static int control(stream_t *stream,int cmd,void* arg) {
    struct stream_priv_s* p = (struct stream_priv_s*)stream->priv;
    switch(cmd) {
    case STREAM_CTRL_GET_TYPE:
        *(int*)arg=p->type;
        return 1;
    }
    return STREAM_UNSUPPORTED;
}
#endif

static int open_r(SHMEMContext *s, int mode) {
    shmem_data_t *shmem_data;

    if(0>(s->shmid=shmget(s->id,sizeof(shmem_data_t), 0666))) {
#ifdef HAVE_MPMSG
        mp_msg(MSGT_STREAM,MSGL_ERR,
               "Can't open shared memory: #%d. (%s)\n",s->id,strerror(errno));
#endif
        return STREAM_UNSUPPORTED;
    }
    if((s->shmem=shmat(s->shmid,0,0))==(char*)-1) {
        shmdt(s->shmem);
#ifdef HAVE_MPMSG
        mp_msg(MSGT_STREAM,MSGL_ERR,
               "Can't attach shared memory: #%d. (%s)\n",s->id,strerror(errno));
#endif
        return STREAM_UNSUPPORTED;
    }
#ifdef HAVE_SEMAPHORE
    s->semid = semopen(s->shmid,SEM_OPEN);
    if (s->semid == -1 && SEM_REQUIRE) {
        shmdt(s->shmem);
        return STREAM_UNSUPPORTED;
    }
    if (s->semid != -1) {
        if (!semlockwait(s->semid, SEM_MEMBER)) {
            semctl(s->semid, IPC_RMID, 0);
            shmdt(s->shmem);
            return STREAM_UNSUPPORTED;
        }
    }
#endif
    shmem_data=(shmem_data_t*)s->shmem;
    if(shmem_data->mplayerid[0]!='M' || shmem_data->mplayerid[1]!='P') {
        sem_unlock(s->semid, SEM_MEMBER);
        shmdt(s->shmem);
        return STREAM_UNSUPPORTED;
    }
    s->size=get_shmem_size(s->shmid);
    switch (shmem_data->type) {
    case SHMEM_TYPE_RAWDATA:
    case SHMEM_TYPE_PIPE:
        s->type=shmem_data->type;
        break;
    default:
#ifdef HAVE_MPMSG
        mp_msg(MSGT_STREAM,MSGL_ERR,
               "Unknown type %d in shared memory: #%d.\n",
               shmem_data->type,s->id);
#endif
        sem_unlock(s->semid, SEM_MEMBER);
        shmdt(s->shmem);
        return STREAM_UNSUPPORTED;
    }
    shmdt(s->shmem);
    if(0>(s->shmid=shmget(s->id,s->size, 0666))) {
        sem_unlock(s->semid, SEM_MEMBER);
#ifdef HAVE_MPMSG
        mp_msg(MSGT_STREAM,MSGL_ERR,
               "Can't reopen shared memory: #%d. (%s)\n",s->id,strerror(errno));
#endif
        return STREAM_UNSUPPORTED;
    }
    if((s->shmem=shmat(s->shmid,0,0))==(char*)-1) {
        sem_unlock(s->semid, SEM_MEMBER);
        shmdt(s->shmem);
#ifdef HAVE_MPMSG
        mp_msg(MSGT_STREAM,MSGL_ERR,
               "Can't attach shared memory: #%d. (%s)\n",s->id,strerror(errno));
#endif
        return STREAM_UNSUPPORTED;
    }
    s->data=av_malloc(s->size);
    s->header.headerid[0]='M';
    s->header.headerid[1]='P';
    memset(s->data,0,s->size);
    sem_unlock(s->semid, SEM_MEMBER);
    s->mode=mode;
    return STREAM_OK;
}

static int open_w(SHMEMContext *s, int mode) {
    shmem_data_t *shmem_data;

    s->size=shmem_size;
    s->type=shmem_type;
    shmem_type = SHMEM_TYPE_PIPE;
    shmem_size = SHMEM_SIZE;

    if(0>(s->shmid=shmget(s->id, s->size, 0666 | IPC_CREAT))) {
        if(0>(s->shmid=shmget(s->id, sizeof(shmem_data_t), 0666))) {
#ifdef HAVE_MPMSG
            mp_msg(MSGT_STREAM,MSGL_ERR,
                   "Can't create shared memory: #%d. (%s)\n",
                   s->id,strerror(errno));
#endif
            return STREAM_UNSUPPORTED;
        }
        if(get_shmem_attach(s->shmid)) {
#ifdef HAVE_MPMSG
            mp_msg(MSGT_STREAM,MSGL_ERR,
                   "Shared memory is busy. Can't create.: #%d. (%s)\n",
                   s->id,strerror(errno));
#endif
            return STREAM_UNSUPPORTED;
        }
        shmctl(s->shmid, IPC_RMID, 0);          // Delete if unused
        if(0>(s->shmid=shmget(s->id, s->size, 0666 | IPC_CREAT))) {
#ifdef HAVE_MPMSG
            mp_msg(MSGT_STREAM,MSGL_ERR,
                   "Can't create shared memory: #%d. (%s)\n",
                   s->id,strerror(errno));
#endif
            return STREAM_UNSUPPORTED;
        }
    }
    if ((s->shmem=shmat(s->shmid,0,0))==(char*)-1) {
#ifdef HAVE_MPMSG
        mp_msg(MSGT_STREAM,MSGL_ERR,
               "Can't attach shared memory: #%d (%s)\n",s->id,strerror(errno));
#endif
        return STREAM_UNSUPPORTED;
    }
#ifdef HAVE_SEMAPHORE
    s->semid = semopen(s->shmid,SEM_CREATE);
    if (s->semid == -1 && SEM_REQUIRE) {
        shmdt(s->shmem);
        shmctl(s->shmid, IPC_RMID, 0);
#ifdef HAVE_MPMSG
        mp_msg(MSGT_STREAM,MSGL_ERR,
               "Can't create semapore: #%d (%s)\n",s->id,strerror(errno));
#endif
        return STREAM_UNSUPPORTED;
    }
    if (s->semid != -1) {
        if (!semlockwait(s->semid, SEM_MEMBER)) {
            semctl(s->semid, IPC_RMID, 0);
            shmdt(s->shmem);
            shmctl(s->shmid, IPC_RMID, 0);
#ifdef HAVE_MPMSG
            mp_msg(MSGT_STREAM,MSGL_ERR,
                   "Can't lock semapore: #%d (%s)\n",s->id,strerror(errno));
#endif
            return STREAM_UNSUPPORTED;
        }
    }
#endif
    memset(s->shmem,0,s->size);
    shmem_data=(shmem_data_t*)s->shmem;
    shmem_data->mplayerid[0]='M';
    shmem_data->mplayerid[1]='P';
    shmem_data->type=s->type;
    shmem_data->size=0;
    s->data=av_malloc(s->size);
    memset(s->data,0,s->size);
    sem_unlock(s->semid, SEM_MEMBER);
    s->mode=mode;

    return STREAM_OK;
}

void* shmem_open(int shmemid, int write) {
    SHMEMContext *s;
    int res;

    if (!shmemid)
        return NULL;
    s = av_malloc(sizeof(SHMEMContext),1);
    memet,s,sizeof(SHMEMContext),1);
    s->id=shmemid;
    if (write)
        res=open_w(s,STREAM_WRITE);
    else
        res=open_r(s,STREAM_READ);
    if(res==STREAM_OK)
        return s;
    av_free(s);
    return NULL;
}

int shmem_read(void* priv, unsigned char *buf, int size) {
    SHMEMContext *s = (SHMEMContext*)priv;

    if(!priv)
        return STREAM_UNSUPPORTED;
    switch(s->type) {
    case SHMEM_TYPE_RAWDATA:
        return fill_buffer_raw(s,buf,size);
    case SHMEM_TYPE_PIPE:
        return fill_buffer_pipe(s,buf,size);
    }
    return STREAM_UNSUPPORTED;
}

int shmem_write(void* priv, unsigned char *buf, int size) {
    SHMEMContext *s = (SHMEMContext*)priv;

    if(!priv)
        return STREAM_UNSUPPORTED;
    switch(s->type) {
    case SHMEM_TYPE_RAWDATA:
        return write_buffer_raw(s,buf,size);
    case SHMEM_TYPE_PIPE:
        return write_buffer_pipe(s,buf,size);
    }
    return -1;
}

void shmem_reset(void * priv) {
    SHMEMContext *s = (SHMEMContext*)priv;

    if(!priv)
        return;
    s->bytes=0;
    return;
}

int shmem_close(void* priv) {
    SHMEMContext *s = (SHMEMContext*)priv;

    if(!priv)
        return 0;
#ifdef HAVE_SEMAPHORE
    if(s->semid!=-1 && s->mode==STREAM_WRITE)
        semctl(s->semid, IPC_RMID, 0);
    s->semid=-1;
#endif
    shmdt(s->shmem);
    if(s->mode==STREAM_WRITE)
        shmctl(s->shmid, IPC_RMID, 0);
    s->shmem=NULL;
    if(s->data) av_free(s->data);
    av_free(s);
    return 0;
}
