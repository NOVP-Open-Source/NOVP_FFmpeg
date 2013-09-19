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
avutildestlib="avutil-51.dll"
avfilterdestlib="avfilter-3.dll"
avcodecdestlib="avcodec-54.dll"
avformatdestlib="avformat-54.dll"
swscaledestlib="swscale-2.dll"
swresampledestlib="swresample-0.dll"
avdevicedestlib="avdevice-54.dll"
else
avutillib="libavutil.dylib"
avfilterlib="libavfilter.dylib"
avcodeclib="libavcodec.dylib"
avformatlib="libavformat.dylib"
swscalelib="libswscale.dylib"
swresamplelib="libswresample.dylib"
avdevicelib="libavdevice.dylib"
avutildestlib="libavutil.51.dylib"
avfilterdestlib="libavfilter.3.dylib"
avcodecdestlib="libavcodec.54.dylib"
avformatdestlib="libavformat.54.dylib"
swscaledestlib="libswscale.2.dylib"
swresampledestlib="libswresample.0.dylib"
avdevicedestlib="libavdevice.54.dylib"
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
    srclib="$3"
    destlib="$4"

    if test ! "" == "$srcdir" ; then
        if test ! "" == "$srclib" ; then
            mkdir -p "$destdir/srclib"
            if test -e "$destdir/$srclib" ; then
                rm -f "$destdir/$srclib"
            fi
            cp "$srcdir/$srclib" "$destdir/lib/$destlib"
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

mkdir -p $dest/lib

testlibs "$avutildir" "$avutillib"
testlibs "$avcoredir" "$avcorelib"
testlibs "$avfilteredir" "$avfilterlib"
testlibs "$avcodecdir" "$avcodeclib"
testlibs "$avformatdir" "$avformatlib"
testlibs "$avdevicedir" "$avdevicelib"
testlibs "$swscaledir" "$swscalelib"
testlibs "$swresampledir" "$swresamplelib"

installlibs "$avutildir" "$dest" "$avutillib" "$avutildestlib"
installlibs "$avcoredir" "$dest" "$avcorelib" "$avcorelib"
installlibs "$avfilterdir" "$dest" "$avfilterlib" "$avfilterdestlib"
installlibs "$avcodecdir" "$dest" "$avcodeclib" "$avcodecdestlib"
installlibs "$avformatdir" "$dest" "$avformatlib" "$avformatdestlib"
installlibs "$avdevicedir" "$dest" "$avdevicelib" "$avdevicedestlib"
installlibs "$swscaledir" "$dest" "$swscalelib" "$swscaledestlib"
installlibs "$swresampledir" "$dest" "$swresamplelib" "$swresampledestlib"

if test -e "$dest/ffmpeg_config.h" ; then
    rm -f "$dest/ffmpeg_config.h"
fi
cp "config.h" "$dest/ffmpeg_config.h"

#make clean

#echo "Install dir: $dest"
