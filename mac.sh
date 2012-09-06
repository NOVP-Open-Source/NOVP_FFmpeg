#!/bin/bash

./gen.sh
git submodule update --recursive --init
patch -p0 < FFmpeg.patch
cd libffmpeg
make
