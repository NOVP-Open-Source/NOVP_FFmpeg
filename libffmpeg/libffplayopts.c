/*
 * Various utilities for command line tools
 * Copyright (c) 2000-2003 Fabrice Bellard
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <math.h>

/* Include only the enabled headers since some compilers (namely, Sun
   Studio) will not omit unused inline functions and create undefined
   references to libraries that are not being built. */

#include "ffmpeg_config.h"
#undef CONFIG_AVFILTER
#undef CONFIG_SWSCALE
#include "config.h"
//#include "../ffmpeg/config.h"
#include "libavformat/avformat.h"
#include "libavfilter/avfilter.h"
#include "libavdevice/avdevice.h"
#include "libswscale/swscale.h"
//#include "libpostproc/postprocess.h"
#include "libavutil/avstring.h"
#include "libavutil/mathematics.h"
#include "libavutil/parseutils.h"
#include "libavutil/pixdesc.h"
#include "libavutil/eval.h"
#include "libavutil/dict.h"
#include "libavutil/opt.h"
//#include "version.h"
#if CONFIG_NETWORK
#include "libavformat/network.h"
#endif
#if HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif

#include "libavutil/log.h"

#if HAVE_PTHREADS
#include <pthread.h>
#elif HAVE_W32THREADS
#include "libavcodec/w32pthreads.h"
#elif HAVE_OS2THREADS
#include "libavcodec/os2threads.h"
#endif 

#include "libffplayopts.h"

AVDictionary *filter_codec_opts(AVDictionary *opts, AVCodec *codec, AVFormatContext *s, AVStream *st)
{
    AVDictionary    *ret = NULL;
    AVDictionaryEntry *t = NULL;
    int            flags = s->oformat ? AV_OPT_FLAG_ENCODING_PARAM : AV_OPT_FLAG_DECODING_PARAM;
    char          prefix = 0;
    const AVClass    *cc = avcodec_get_class();

    if (!codec)
        return NULL;

    switch (codec->type) {
    case AVMEDIA_TYPE_VIDEO:    prefix = 'v'; flags |= AV_OPT_FLAG_VIDEO_PARAM;    break;
    case AVMEDIA_TYPE_AUDIO:    prefix = 'a'; flags |= AV_OPT_FLAG_AUDIO_PARAM;    break;
    case AVMEDIA_TYPE_SUBTITLE: prefix = 's'; flags |= AV_OPT_FLAG_SUBTITLE_PARAM; break;
    default: break;
    }

    while ((t = av_dict_get(opts, "", t, AV_DICT_IGNORE_SUFFIX))) {
        char *p = strchr(t->key, ':');

        /* check stream specification in opt name */
        if (p)
            switch (check_stream_specifier(s, st, p + 1)) {
            case  1: *p = 0; break;
            case  0:         continue;
            default:         return NULL;
            }

        if (av_opt_find(&cc, t->key, NULL, flags, AV_OPT_SEARCH_FAKE_OBJ) ||
            (codec && codec->priv_class && av_opt_find(&codec->priv_class, t->key, NULL, flags, AV_OPT_SEARCH_FAKE_OBJ)))
            av_dict_set(&ret, t->key, t->value, 0);
        else if (t->key[0] == prefix && av_opt_find(&cc, t->key+1, NULL, flags, AV_OPT_SEARCH_FAKE_OBJ))
            av_dict_set(&ret, t->key+1, t->value, 0);

        if (p)
            *p = ':';
    }
    return ret;
}

AVDictionary **setup_find_stream_info_opts(AVFormatContext *s, AVDictionary *codec_opts)
{
    int i;
    AVDictionary **opts;

    if (!s->nb_streams)
        return NULL;
    opts = av_mallocz(s->nb_streams * sizeof(*opts));
    if (!opts) {
        av_log(NULL, AV_LOG_ERROR, "Could not alloc memory for stream options.\n");
        return NULL;
    }
    for (i = 0; i < s->nb_streams; i++)
        opts[i] = filter_codec_opts(codec_opts, avcodec_find_decoder(s->streams[i]->codec->codec_id), s, s->streams[i]);
    return opts;
}

int check_stream_specifier(AVFormatContext *s, AVStream *st, const char *spec)
{
    if (*spec <= '9' && *spec >= '0')                                        /* opt:index */
        return strtol(spec, NULL, 0) == st->index;
    else if (*spec == 'v' || *spec == 'a' || *spec == 's' || *spec == 'd' || *spec == 't') { /* opt:[vasdt] */
        enum AVMediaType type;

        switch (*spec++) {
        case 'v': type = AVMEDIA_TYPE_VIDEO;    break;
        case 'a': type = AVMEDIA_TYPE_AUDIO;    break;
        case 's': type = AVMEDIA_TYPE_SUBTITLE; break;
        case 'd': type = AVMEDIA_TYPE_DATA;     break;
        case 't': type = AVMEDIA_TYPE_ATTACHMENT; break;
        default: abort(); // never reached, silence warning
        }
        if (type != st->codec->codec_type)
            return 0;
        if (*spec++ == ':') {                                   /* possibly followed by :index */
            int i, index = strtol(spec, NULL, 0);
            for (i = 0; i < s->nb_streams; i++)
                if (s->streams[i]->codec->codec_type == type && index-- == 0)
                   return i == st->index;
            return 0;
        }
        return 1;
    } else if (*spec == 'p' && *(spec + 1) == ':') {
        int prog_id, i, j;
        char *endptr;
        spec += 2;
        prog_id = strtol(spec, &endptr, 0);
        for (i = 0; i < s->nb_programs; i++) {
            if (s->programs[i]->id != prog_id)
                continue;

            if (*endptr++ == ':') {
                int stream_idx = strtol(endptr, NULL, 0);
                return (stream_idx >= 0 && stream_idx < s->programs[i]->nb_stream_indexes &&
                        st->index == s->programs[i]->stream_index[stream_idx]);
            }

            for (j = 0; j < s->programs[i]->nb_stream_indexes; j++)
                if (st->index == s->programs[i]->stream_index[j])
                    return 1;
        }
        return 0;
    } else if (!*spec) /* empty specifier, matches everything */
        return 1;

    av_log(s, AV_LOG_ERROR, "Invalid stream specifier: %s.\n", spec);
    return AVERROR(EINVAL);
}

void *grow_array(void *array, int elem_size, int *size, int new_size)
{
    if (new_size >= INT_MAX / elem_size) {
        av_log(NULL, AV_LOG_ERROR, "Array too big.\n");
//        exit_program(1);
    }
    if (*size < new_size) {
        uint8_t *tmp = av_realloc(array, new_size*elem_size);
        if (!tmp) {
            av_log(NULL, AV_LOG_ERROR, "Could not alloc buffer.\n");
//            exit_program(1);
        }
        memset(tmp + *size*elem_size, 0, (new_size-*size) * elem_size);
        *size = new_size;
        return tmp;
    }
    return array;
}

double parse_number_or_die(const char *context, const char *numstr, int type, double min, double max)
{
    char *tail;
    const char *error;
    double d = av_strtod(numstr, &tail);
    if (*tail)
        error= "Expected number for %s but found: %s\n";
    else if (d < min || d > max)
        error= "The value for %s was %s which is not within %f - %f\n";
    else if(type == OPT_INT64 && (int64_t)d != d)
        error= "Expected int64 for %s but found %s\n";
    else if (type == OPT_INT && (int)d != d)
        error= "Expected int for %s but found %s\n";
    else
        return d;
    av_log(NULL, AV_LOG_FATAL, error, context, numstr, min, max);
    return 0;
}

int64_t parse_time_or_die(const char *context, const char *timestr, int is_duration)
{
    int64_t us = 0;
    if (av_parse_time(&us, timestr, is_duration) < 0) {
        av_log(NULL, AV_LOG_FATAL, "Invalid %s specification for %s: %s\n",
               is_duration ? "duration" : "date", context, timestr);
    }
    return us;
}

static const OptionDef* find_option(const OptionDef *po, const char *name){
    const char *p = strchr(name, ':');
    int len = p ? p - name : strlen(name);

    while (po->name != NULL) {
        if (!strncmp(name, po->name, len) && strlen(po->name) == len)
            break;
        po++;
    }
    return po;
}

int parse_option(slotinfo_t* slotinfo, const char *opt, const char *arg)
{
    OptionDef* options = (OptionDef*)slotinfo->opt_def;
    const OptionDef *po;
    int bool_val = 1;
    int *dstcount;
    void *dst;

    if(!options)
        return -1;
    po = find_option(options, opt);
    if (!po->name && opt[0] == 'n' && opt[1] == 'o') {
        /* handle 'no' bool option */
        po = find_option(options, opt + 2);
        if (!(po->name && (po->flags & OPT_BOOL)))
            goto unknown_opt;
        bool_val = 0;
    }
    if (!po->name)
        po = find_option(options, "default");
    if (!po->name) {
unknown_opt:
        av_log(NULL, AV_LOG_ERROR, "Unrecognized option '%s'\n", opt);
        return AVERROR(EINVAL);
    }
    if (po->flags & HAS_ARG && !arg) {
        av_log(NULL, AV_LOG_ERROR, "Missing argument for option '%s'\n", opt);
        return AVERROR(EINVAL);
    }

    /* new-style options contain an offset into optctx, old-style address of
     * a global var*/
    dst = po->flags & (OPT_OFFSET|OPT_SPEC) ? (uint8_t*)slotinfo->optctx + po->u.off : po->u.dst_ptr;

    if (po->flags & OPT_SPEC) {
        SpecifierOpt **so = dst;
        char *p = strchr(opt, ':');

        dstcount = (int*)(so + 1);
        *so = grow_array(*so, sizeof(**so), dstcount, *dstcount + 1);
        (*so)[*dstcount - 1].specifier = av_strdup(p ? p + 1 : "");
        dst = &(*so)[*dstcount - 1].u;
    }

    if (po->flags & OPT_STRING) {
        char *str;
        str = av_strdup(arg);
        *(char**)dst = str;
    } else if (po->flags & OPT_BOOL) {
        *(int*)dst = bool_val;
    } else if (po->flags & OPT_INT) {
        *(int*)dst = parse_number_or_die(opt, arg, OPT_INT64, INT_MIN, INT_MAX);
    } else if (po->flags & OPT_INT64) {
        *(int64_t*)dst = parse_number_or_die(opt, arg, OPT_INT64, INT64_MIN, INT64_MAX);
    } else if (po->flags & OPT_TIME) {
        *(int64_t*)dst = parse_time_or_die(opt, arg, 1);
    } else if (po->flags & OPT_FLOAT) {
        *(float*)dst = parse_number_or_die(opt, arg, OPT_FLOAT, -INFINITY, INFINITY);
    } else if (po->flags & OPT_DOUBLE) {
        *(double*)dst = parse_number_or_die(opt, arg, OPT_DOUBLE, -INFINITY, INFINITY);
    } else if (po->u.func_arg) {
        int ret = po->flags & OPT_FUNC2 ? po->u.func2_arg(slotinfo, slotinfo->optctx, opt, arg) :
                                          po->u.func_arg(slotinfo, opt, arg);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Failed to set value '%s' for option '%s'\n", arg, opt);
            return ret;
        }
    }
    if (po->flags & OPT_EXIT) {
//        exit_program(0);
    }
    return !!(po->flags & HAS_ARG);
}

#define FLAGS(o) ((o)->type == AV_OPT_TYPE_FLAGS) ? AV_DICT_APPEND : 0
int opt_default(slotinfo_t* slotinfo, const char *opt, const char *arg)
{
    const AVOption *oc, *of, *os = 0;
    char opt_stripped[128];
    const char *p;
    const AVClass *cc = avcodec_get_class(), *fc = avformat_get_class();
#if CONFIG_SWSCALE
    const AVClass *sc;
#endif

    if (!(p = strchr(opt, ':')))
        p = opt + strlen(opt);
    av_strlcpy(opt_stripped, opt, FFMIN(sizeof(opt_stripped), p - opt + 1));

    if ((oc = av_opt_find(&cc, opt_stripped, NULL, 0, AV_OPT_SEARCH_CHILDREN|AV_OPT_SEARCH_FAKE_OBJ)) ||
         ((opt[0] == 'v' || opt[0] == 'a' || opt[0] == 's') &&
          (oc = av_opt_find(&cc, opt+1, NULL, 0, AV_OPT_SEARCH_FAKE_OBJ)))) {
        if(strlen(arg))
            av_dict_set(&slotinfo->codec_opts, opt, arg, FLAGS(oc));
        else
            av_dict_set(&slotinfo->codec_opts, opt, arg, 0);
    }
    if ((of = av_opt_find(&fc, opt, NULL, 0, AV_OPT_SEARCH_CHILDREN | AV_OPT_SEARCH_FAKE_OBJ))) {
        if(strlen(arg))
            av_dict_set(&slotinfo->format_opts, opt, arg, FLAGS(of));
        else
            av_dict_set(&slotinfo->format_opts, opt, arg, 0);
    }
#if CONFIG_SWSCALE
    sc = sws_get_class();
    if ((os = av_opt_find(&sc, opt, NULL, 0, AV_OPT_SEARCH_CHILDREN | AV_OPT_SEARCH_FAKE_OBJ))) {
        // XXX we only support sws_flags, not arbitrary sws options
        int ret = av_opt_set(sws_opts, opt, arg, 0);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Error setting option %s.\n", opt);
            return ret;
        }
    }
#endif

    if (oc || of || os)
        return 0;
    av_log(NULL, AV_LOG_ERROR, "Unrecognized option '%s'\n", opt);
    return AVERROR_OPTION_NOT_FOUND;
}

static int opt_seek(slotinfo_t* slotinfo, const char *opt, const char *arg)
{
    slotinfo->start_time = parse_time_or_die(opt, arg, 1);
    return 0;
}

static int opt_duration(slotinfo_t* slotinfo, const char *opt, const char *arg)
{
    slotinfo->duration = parse_time_or_die(opt, arg, 1);
    return 0;
}

static int opt_format(slotinfo_t* slotinfo, const char *opt, const char *arg)
{
    slotinfo->file_iformat = av_find_input_format(arg);
    if (!slotinfo->file_iformat) {
        av_log(NULL, AV_LOG_ERROR, "Unknown input format: %s\n", arg);
        return AVERROR(EINVAL);
    }
    return 0;
}

static int opt_sync(slotinfo_t* slotinfo, const char *opt, const char *arg)
{
    if (!strcmp(arg, "audio"))
        slotinfo->av_sync_type = AV_SYNC_AUDIO_MASTER;
    else if (!strcmp(arg, "video"))
        slotinfo->av_sync_type = AV_SYNC_VIDEO_MASTER;
    else if (!strcmp(arg, "ext"))
        slotinfo->av_sync_type = AV_SYNC_EXTERNAL_CLOCK;
    else {
        av_log(NULL, AV_LOG_ERROR, "Unknown value for %s: %s\n", opt, arg);
    }
    return 0;
}

static int opt_codec(slotinfo_t* slotinfo, void *o, const char *opt, const char *arg)
{
    switch(opt[strlen(opt)-1]){
    case 'a':
        if(slotinfo->audio_codec_name)
            av_free(slotinfo->audio_codec_name);
        slotinfo->audio_codec_name = NULL;
        if(arg && strlen(arg))
            slotinfo->audio_codec_name = strdup(arg);
        break;
    case 's':
        if(slotinfo->subtitle_codec_name)
            av_free(slotinfo->subtitle_codec_name);
        slotinfo->subtitle_codec_name = NULL;
        if(arg && strlen(arg))
            slotinfo->subtitle_codec_name = strdup(arg);
        break;
    case 'v':
        if(slotinfo->video_codec_name)
            av_free(slotinfo->video_codec_name);
        slotinfo->video_codec_name = NULL;
        if(arg && strlen(arg))
            slotinfo->video_codec_name = strdup(arg);
        break;
    }
    return 0;
}

void init_options(slotinfo_t* slotinfo)
{
    slotinfo->sws_opts = sws_getContext(16, 16, 0, 16, 16, 0, SWS_BICUBIC, NULL, NULL, NULL);

    OptionDef* opt = av_malloc(27*sizeof(OptionDef));
    memset(opt,0,27*sizeof(OptionDef));

    opt[0].name="ast";
    opt[0].flags=OPT_INT | HAS_ARG | OPT_EXPERT;
    opt[0].u.dst_ptr=(void*)&slotinfo->wanted_stream[AVMEDIA_TYPE_AUDIO];
    opt[0].help="select desired audio stream";
    opt[0].argname="stream_number";

    opt[1].name="vst";
    opt[1].flags=OPT_INT | HAS_ARG | OPT_EXPERT;
    opt[1].u.dst_ptr=(void*)&slotinfo->wanted_stream[AVMEDIA_TYPE_VIDEO];
    opt[1].help="select desired video stream";
    opt[1].argname="stream_number";

    opt[2].name="sst";
    opt[2].flags=OPT_INT | HAS_ARG | OPT_EXPERT;
    opt[2].u.dst_ptr=(void*)&slotinfo->wanted_stream[AVMEDIA_TYPE_SUBTITLE];
    opt[2].help="select desired subtitle stream";
    opt[2].argname="stream_number";

    opt[3].name="ss";
    opt[3].flags=HAS_ARG;
    opt[3].u.func_arg=(void*)&opt_seek;
    opt[3].help="seek to a given position in seconds";
    opt[3].argname="pos";

    opt[4].name="t";
    opt[4].flags=HAS_ARG;
    opt[4].u.func_arg=(void*)&opt_duration;
    opt[4].help="play  \"duration\" seconds of audio/video";
    opt[4].argname="duration";

    opt[5].name="bytes";
    opt[5].flags=OPT_INT | HAS_ARG;
    opt[5].u.dst_ptr=(void*)&slotinfo->seek_by_bytes;
    opt[5].help="seek by bytes 0=off 1=on -1=auto";
    opt[5].argname="val";

    opt[6].name="f";
    opt[6].flags=HAS_ARG;
    opt[6].u.func_arg=(void*)opt_format;
    opt[6].help="force format";
    opt[6].argname="fmt";

    opt[7].name="bug";
    opt[7].flags=OPT_INT | HAS_ARG | OPT_EXPERT;
    opt[7].u.dst_ptr=(void*)&slotinfo->workaround_bugs;
    opt[7].help="workaround bugs";
    opt[7].argname="";

    opt[8].name="fast";
    opt[8].flags=OPT_BOOL | OPT_EXPERT;
    opt[8].u.dst_ptr=(void*)&slotinfo->fast;
    opt[8].help="non spec compliant optimizations";
    opt[8].argname="";

    opt[9].name="genpts";
    opt[9].flags=OPT_BOOL | OPT_EXPERT;
    opt[9].u.dst_ptr=(void*)&slotinfo->genpts;
    opt[9].help="generate pts";
    opt[9].argname="";

    opt[10].name="drp";
    opt[10].flags=OPT_INT | HAS_ARG | OPT_EXPERT;
    opt[10].u.dst_ptr=(void*)&slotinfo->decoder_reorder_pts;
    opt[10].help="let decoder reorder pts 0=off 1=on -1=auto";
    opt[10].argname="";

    opt[11].name="lowres";
    opt[11].flags=OPT_INT | HAS_ARG | OPT_EXPERT;
    opt[11].u.dst_ptr=(void*)&slotinfo->lowres;
    opt[11].help="";
    opt[11].argname="";

    opt[12].name="lowres";
    opt[12].flags=OPT_INT | HAS_ARG | OPT_EXPERT;
    opt[12].u.dst_ptr=(void*)&slotinfo->lowres;
    opt[12].help="";
    opt[12].argname="";

    opt[13].name="skiploop";
    opt[13].flags=OPT_INT | HAS_ARG | OPT_EXPERT;
    opt[13].u.dst_ptr=(void*)&slotinfo->skip_loop_filter;
    opt[13].help="";
    opt[13].argname="";

    opt[14].name="skipframe";
    opt[14].flags=OPT_INT | HAS_ARG | OPT_EXPERT;
    opt[14].u.dst_ptr=(void*)&slotinfo->skip_frame;
    opt[14].help="";
    opt[14].argname="";

    opt[15].name="skipidct";
    opt[15].flags=OPT_INT | HAS_ARG | OPT_EXPERT;
    opt[15].u.dst_ptr=(void*)&slotinfo->skip_idct;
    opt[15].help="";
    opt[15].argname="";

    opt[16].name="idct";
    opt[16].flags=OPT_INT | HAS_ARG | OPT_EXPERT;
    opt[16].u.dst_ptr=(void*)&slotinfo->idct;
    opt[16].help="set idct algo";
    opt[16].argname="algo";

    opt[17].name="ec";
    opt[17].flags=OPT_INT | HAS_ARG | OPT_EXPERT;
    opt[17].u.dst_ptr=(void*)&slotinfo->error_concealment;
    opt[17].help="set error concealment options";
    opt[17].argname="bit_mask";

    opt[18].name="sync";
    opt[18].flags=HAS_ARG | OPT_EXPERT;
    opt[18].u.func_arg=(void*)opt_sync;
    opt[18].help="set audio-video sync. type (type=audio/video/ext)";
    opt[18].argname="type";

    opt[19].name="loop";
    opt[19].flags=OPT_INT | HAS_ARG | OPT_EXPERT;
    opt[19].u.dst_ptr=(void*)&slotinfo->loop;
    opt[19].help="set number of times the playback shall be looped";
    opt[19].argname="loop count";

    opt[20].name="framedrop";
    opt[20].flags=OPT_BOOL | OPT_EXPERT;
    opt[20].u.dst_ptr=(void*)&slotinfo->framedrop;
    opt[20].help="drop frames when cpu is too slow";
    opt[20].argname="";

    opt[21].name="rdftspeed";
    opt[21].flags=OPT_INT | HAS_ARG| OPT_AUDIO | OPT_EXPERT;
    opt[21].u.dst_ptr=(void*)&slotinfo->rdftspeed;
    opt[21].help="rdft speed";
    opt[21].argname="msecs";

    opt[22].name="default";
    opt[22].flags=HAS_ARG | OPT_AUDIO | OPT_VIDEO | OPT_EXPERT;
    opt[22].u.func_arg=(void*)opt_default;
    opt[22].help="generic catch all option";
    opt[22].argname="";

    opt[23].name="codec";
    opt[23].flags=HAS_ARG | OPT_FUNC2;
    opt[23].u.func2_arg=(void*)opt_codec;
    opt[23].help="force decoder";
    opt[23].argname="decoder";

#if CONFIG_AVFILTER
    opt[24].name="vf";
    opt[24].flags=OPT_STRING | HAS_ARG;
    opt[24].u.dst_ptr=(void*)&vfilters;
    opt[24].help="video filters";
    opt[24].argname="filter list";
#endif
    slotinfo->opt_def = opt;
}
