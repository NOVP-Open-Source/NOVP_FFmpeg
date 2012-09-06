#!/bin/bash

./gen.sh
patch -p0 < FFmpeg.patch
cd libffmpeg
make
