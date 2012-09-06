#!/bin/bash

./gen.sh
patch FFmpeg/libavformat/tcp.c tcp.patch
cd libffmpeg
make
