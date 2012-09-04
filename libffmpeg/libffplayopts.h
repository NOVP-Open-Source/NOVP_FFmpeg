
#ifndef __LIBFFPLAYOPTS_H__
#define __LIBFFPLAYOPTS_H__

#include <stdint.h>

#include "libavcodec/avcodec.h"
#include "libavfilter/avfilter.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"

#include "libffplay.h"

typedef struct SpecifierOpt {
    char *specifier;    /**< stream/chapter/program/... specifier */
    union {
        uint8_t *str;
        int        i;
        int64_t  i64;
        float      f;
        double   dbl;
    } u;
} SpecifierOpt;

typedef struct {
    const char *name;
    int flags;
#define HAS_ARG    0x0001
#define OPT_BOOL   0x0002
#define OPT_EXPERT 0x0004
#define OPT_STRING 0x0008
#define OPT_VIDEO  0x0010
#define OPT_AUDIO  0x0020
#define OPT_GRAB   0x0040
#define OPT_INT    0x0080
#define OPT_FLOAT  0x0100
#define OPT_SUBTITLE 0x0200
#define OPT_INT64  0x0400
#define OPT_EXIT   0x0800
#define OPT_DATA   0x1000
#define OPT_FUNC2  0x2000
#define OPT_TIME  0x10000
#define OPT_OFFSET 0x4000       /* option is specified as an offset in a passed optctx */
#define OPT_SPEC   0x8000       /* option is to be stored in an array of SpecifierOpt.
                                   Implies OPT_OFFSET. Next element after the offset is
                                   an int containing element count in the array. */
#define OPT_DOUBLE 0x20000
     union {
        void *dst_ptr;
        int (*func_arg)(slotinfo_t*, const char *, const char *);
        int (*func2_arg)(slotinfo_t*, void *, const char *, const char *);
        size_t off;
    } u;
    const char *help;
    const char *argname;
} OptionDef;

/**
 * Filter out options for given codec.
 *
 * Create a new options dictionary containing only the options from
 * opts which apply to the codec with ID codec_id.
 *
 * @param s Corresponding format context.
 * @param st A stream from s for which the options should be filtered.
 * @return a pointer to the created dictionary
 */
AVDictionary *filter_codec_opts(AVDictionary *opts, AVCodec *codec, AVFormatContext *s, AVStream *st);

/**
 * Setup AVCodecContext options for avformat_find_stream_info().
 *
 * Create an array of dictionaries, one dictionary for each stream
 * contained in s.
 * Each dictionary will contain the options from codec_opts which can
 * be applied to the corresponding stream codec context.
 *
 * @return pointer to the created array of dictionaries, NULL if it
 * cannot be created
 */
AVDictionary **setup_find_stream_info_opts(AVFormatContext *s, AVDictionary *codec_opts);

/**
 * Check if the given stream matches a stream specifier.
 *
 * @param s  Corresponding format context.
 * @param st Stream from s to be checked.
 * @param spec A stream specifier of the [v|a|s|d]:[\<stream index\>] form.
 *
 * @return 1 if the stream matches, 0 if it doesn't, <0 on error
 */
int check_stream_specifier(AVFormatContext *s, AVStream *st, const char *spec);

/**
 * Parse one given option.
 *
 * @return on success 1 if arg was consumed, 0 otherwise; negative number on error
 */
int parse_option(slotinfo_t* slotinfo, const char *opt, const char *arg);

/**
 * Realloc array to hold new_size elements of elem_size.
 * Calls exit_program() on failure.
 *
 * @param elem_size size in bytes of each element
 * @param size new element count will be written here
 * @return reallocated array
 */
void *grow_array(void *array, int elem_size, int *size, int new_size);

/**
 * Fallback for options that are not explicitly handled, these will be
 * parsed through AVOptions.
 */
int opt_default(slotinfo_t* slotinfo, const char *opt, const char *arg);

void init_options(slotinfo_t* slotinfo);

#endif


