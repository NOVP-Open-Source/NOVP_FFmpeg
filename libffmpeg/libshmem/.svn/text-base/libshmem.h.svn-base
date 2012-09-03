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

#ifndef __LIBSHMEM_H__
#define __LIBSHMEM_H__

#ifdef __cplusplus
extern "C" {
#endif

#ifndef FFMIN
#define FFMIN(a,b) ((a) > (b) ? (b) : (a))
#endif

#define SHMEM_FLAG_EOF          1
#define SHMEM_FLAG_START        2
#define SHMEM_FLAG_STOP         4
#define SHMEM_FLAG_WRITED       8

#define is_shmem_eof(flag) ((flag & SHMEM_FLAG_EOF)?1:0)
#define is_shmem_start(flag) ((flag & SHMEM_FLAG_START)?1:0)
#define is_shmem_stop(flag) ((flag & SHMEM_FLAG_STOP)?1:0)
#define is_shmem_writed(flag) ((flag & SHMEM_FLAG_WRITED)?1:0)

#define SHMEM_TYPE_RAWDATA      0
#define SHMEM_TYPE_PIPE         1

#define SHMEM_VIDEO_PACKET      0x77aa4401
#define SHMEM_AUDIO_PACKET      0x77aa4402

typedef struct shmem_data_st {
    char          mplayerid[2]; // 'MP'
    int           type;         // shmem packet type
    int           flag;         // flags
    unsigned int  size;         // full shmem size
    unsigned char data[0];
} shmem_data_t;

typedef struct shmem_video_st {
    int           id;
    int           format;
    int           width;
    int           height;
    int           aspect;
    int           size;
    int           bytes;
    double        pts;
    unsigned char data[0];
} shmem_video_t;

typedef struct shmem_audio_st {
    int           id;
    int           format;
    int           channels;
    int           samplerate;
    int           samplesize;
    int           bitrate;
    int           size;
    int           bytes;
    double        pts;
    unsigned char data[0];
} shmem_audio_t;

typedef struct shmem_control_st {
    int           flag;
    double        pts;
    unsigned int  pos;
    unsigned int  bytes;
    unsigned char data[0];
} shmem_control_t;

int     set_shmem_size(int type, unsigned int size);
void*   shmem_open(int shmemid, int write);
int     shmem_read(void* priv, unsigned char *buf, int size);
int     shmem_write(void* priv, unsigned char *buf, int size);
void    shmem_reset(void * priv);
int     shmem_close(void* priv);

#ifdef __cplusplus
};
#endif

#endif
