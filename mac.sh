#!/bin/bash

./gen.sh
patch FFmpeg FFmpeg.patch
cd libffmpeg
make
