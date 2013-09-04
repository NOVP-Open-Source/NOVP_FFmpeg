#!/bin/bash
dest="$@"

avutildir="libavutil"
avcoredir=""
avcorelib=""
avfilterdir="libavfilter"
avcodecdir="libavcodec"
avformatdir="libavformat"
swscaledir="libswscale"
swresampledir="libswresample"
avdevicedir="libavdevice"

if test ${TERM} == "cygwin" ; then
avutillib="avutil.dll"
avfilterlib="avfilter.dll"
avcodeclib="avcodec.dll"
avformatlib="avformat.dll"
swscalelib="swscale.dll"
swresamplelib="swresample.dll"
avdevicelib="avdevice.dll"
else
avutillib="libavutil.dylib"
avfilterlib="libavfilter.dylib"
avcodeclib="libavcodec.dylib"
avformatlib="libavformat.dylib"
swscalelib="libswscale.dylib"
swresamplelib="libswresample.dylib"
avdevicelib="libavdevice.dylib"
fi

function testlibs
{
    if test ! "" == "$1" ; then
        if test ! -e "$1/$2" ; then
            echo "ERROR: $2 not found!"
            exit 1
        fi
    fi
}

function installlibs
{
    srcdir="$1"
    destdir="$2"
    lib="$3"

    if test ! "" == "$srcdir" ; then
        if test ! "" == "$lib" ; then
            mkdir -p "$destdir/lib"
            if test -e "$destdir/$lib" ; then
                rm -f "$destdir/$lib"
            fi
            cp "$srcdir/$lib" "$destdir/lib/$lib"
        fi
    fi
}

if test "" == "$dest" ; then
    echo "Uses: $0 <destination directory>"
    exit 1
fi

make
if test ! $? == 0 ; then
    exit 1
fi

testlibs "$avutildir" "$avutillib"
testlibs "$avcoredir" "$avcorelib"
testlibs "$avfilteredir" "$avfilterlib"
testlibs "$avcodecdir" "$avcodeclib"
testlibs "$avformatdir" "$avformatlib"
testlibs "$avdevicedir" "$avdevicelib"
testlibs "$swscaledir" "$swscalelib"
testlibs "$swresampledir" "$swresamplelib"

installlibs "$avutildir" "$dest" "$avutillib"
installlibs "$avcoredir" "$dest" "$avcorelib"
installlibs "$avfilterdir" "$dest" "$avfilterlib"
installlibs "$avcodecdir" "$dest" "$avcodeclib"
installlibs "$avformatdir" "$dest" "$avformatlib"
installlibs "$avdevicedir" "$dest" "$avdevicelib"
installlibs "$swscaledir" "$dest" "$swscalelib"
installlibs "$swresampledir" "$dest" "$swresamplelib"

if test -e "$dest/ffmpeg_config.h" ; then
    rm -f "$dest/ffmpeg_config.h"
fi
cp "config.h" "$dest/ffmpeg_config.h"

#make clean

echo "Install dir: $dest"

