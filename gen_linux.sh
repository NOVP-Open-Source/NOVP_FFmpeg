#!/bin/sh
../../vendor/firebreath-dev/prepmake.sh projects build -D CMAKE_BUILD_TYPE="Debug" -DCMAKE_CXX_FLAGS="-DFB_NO_LOGGING_MACROS=1"
