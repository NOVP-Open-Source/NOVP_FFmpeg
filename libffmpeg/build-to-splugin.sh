#!/bin/bash
dest="$@"

avutildir="libavutil"
avutillib="libavutil.a"
avutilinc="log.h samplefmt.h intfloat_readwrite.h common.h dict.h avconfig.h rational.h mathematics.h audioconvert.h mem.h attributes.h error.h cpu.h avutil.h pixfmt.h x86_cpu.h colorspace.h pixdesc.h imgutils.h parseutils.h avassert.h opt.h eval.h avstring.h"

avcoredir=""
avcorelib=""
avcoreinc=""

avfilterdir="libavfilter"
avfilterlib="libavfilter.a"
avfilterinc="avcodec.h avfilter.h avfiltergraph.h buffersink.h vsrc_buffer.h"

avcodecdir="libavcodec"
avcodeclib="libavcodec.a"
avcodecinc="avcodec.h version.h audioconvert.h avfft.h"

avformatdir="libavformat"
avformatlib="libavformat.a"
avformatinc="avformat.h avio.h version.h network.h os_support.h"

swscaledir="libswscale"
swscalelib="libswscale.a"
swscaleinc="swscale.h"

swresampledir="libswresample"
swresamplelib="libswresample.a"
swresampleinc="swresample.h"

avdevicedir="libavdevice"
avdevicelib="libavdevice.a"
avdeviceinc="avdevice.h"

function testlibs
{
    if test ! "" == "$1" ; then
        if test ! -e "$1/$2" ; then
            echo "ERROR: $2 not found!"
            exit 1
        fi
        for n in $3 ; do
            if test ! -e "$1/$n" ; then
                echo "ERROR: $n not found!"
                exit 1
            fi
        done
    fi
}

function installlibs
{
    srcdir="$1"
    destdir="$2"
    lib="$3"
    inc="$4"

    if test ! "" == "$srcdir" ; then
        if test ! "" == "$lib" ; then
            mkdir -p "$destdir/lib"
            if test -e "$destdir/$lib" ; then
                rm -f "$destdir/$lib"
            fi
            cp "$srcdir/$lib" "$destdir/lib/$lib"
        fi
        if test ! "" == "$inc" ; then
            mkdir -p "$destdir/$srcdir"
            for n in $inc ; do
                if test -e "$destdir/$srcdir/$n" ; then
                    rm -f "$destdir/$srcdir/$n"
                fi
                cp "$srcdir/$n" "$destdir/$srcdir/$n"
            done
        fi
    fi
}

if test "" == "$dest" ; then
    echo "Uses: $0 <destination directory>"
    exit 1
fi

./configure --disable-yasm --disable-doc --disable-ffmpeg --disable-ffprobe --disable-ffserver --disable-dxva2 --enable-runtime-cpudetect --disable-muxers --disable-bsfs 
make

testlibs "$avutildir" "$avutillib" "$avutilinc"
testlibs "$avcoredir" "$avcorelib" "$avcoreinc"
testlibs "$avfilteredir" "$avfilterlib" "$avfilterinc"
testlibs "$avcodecdir" "$avcodeclib" "$avcodecinc"
testlibs "$avformatdir" "$avformatlib" "$avformatinc"
testlibs "$avdevicedir" "$avdevicelib" "$avdeviceinc"
testlibs "$swscaledir" "$swscalelib" "$swscaleinc"
testlibs "$swresampledir" "$swresamplelib" "$swresampleinc"

installlibs "$avutildir" "$dest" "$avutillib" "$avutilinc"
installlibs "$avcoredir" "$dest" "$avcorelib" "$avcoreinc"
installlibs "$avfilterdir" "$dest" "$avfilterlib" "$avfilterinc"
installlibs "$avcodecdir" "$dest" "$avcodeclib" "$avcodecinc"
installlibs "$avformatdir" "$dest" "$avformatlib" "$avformatinc"
installlibs "$avdevicedir" "$dest" "$avdevicelib" "$avdeviceinc"
installlibs "$swscaledir" "$dest" "$swscalelib" "$swscaleinc"
installlibs "$swresampledir" "$dest" "$swresamplelib" "$swresampleinc"

if test -e "$dest/ffmpeg_config.h" ; then
    rm -f "$dest/ffmpeg_config.h"
fi
cp "config.h" "$dest/ffmpeg_config.h"

#make clean

echo "Install dir: $dest"
