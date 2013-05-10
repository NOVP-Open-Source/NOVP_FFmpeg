#!/bin/bash

dest="$@"

avutildir="libavutil"
avutillib="libavutil.a"

avcoredir=""
avcorelib=""

avfilterdir="libavfilter"
avfilterlib="libavfilter.a"

avcodecdir="libavcodec"
avcodeclib="libavcodec.a"

avformatdir="libavformat"
avformatlib="libavformat.a"

swscaledir="libswscale"
swscalelib="libswscale.a"

swresampledir="libswresample"
swresamplelib="libswresample.a"

avdevicedir="libavdevice"
avdevicelib="libavdevice.a"

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

###mingw
if test ${TERM} == "cygwin" ; then
#remember to change winsock.h include to winsock2.h in dtls1.h (openssl). what a hack.
    ./configure --disable-doc --disable-ffmpeg --disable-ffprobe --disable-ffserver --disable-dxva2 --enable-runtime-cpudetect --disable-muxers --disable-bsfs --enable-openssl
###mac
else
    ./configure --disable-doc --disable-ffmpeg --disable-ffprobe --disable-ffserver --disable-dxva2 --enable-runtime-cpudetect --disable-muxers --disable-bsfs --enable-openssl --extra-cflags="-arch i386" --extra-ldflags='-arch i386 -read_only_relocs suppress' --arch=x86_32 --target-os=darwin --enable-cross-compile
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

