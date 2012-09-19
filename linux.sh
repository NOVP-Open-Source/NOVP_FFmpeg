#!/bin/bash

if test -d ../sipalsa ; then
firebreath/prepmake.sh projects build -D CMAKE_BUILD_TYPE="Debug" -DCMAKE_CXX_FLAGS="-DFB_NO_LOGGING_MACROS=1"
git submodule update --recursive --init
patch -p0 < FFmpeg.patch
make -C libffmpeg
make -C build
else
echo "Build fail: Linux audio not supported in current git version."
fi
