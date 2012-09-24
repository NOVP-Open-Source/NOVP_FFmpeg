#!/bin/bash

git submodule update --recursive --init
patch -p0 < FFmpeg.patch
make -C libffmpeg

./gen.sh
