#!/bin/sh

../../vendor/firebreath-dev/prepmac.sh projects build -D CMAKE_OSX_ARCHITECTURES="x86_64" -DCMAKE_CXX_FLAGS="-DFB_NO_LOGGING_MACROS=1"
