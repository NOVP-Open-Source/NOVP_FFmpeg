#!/bin/bash
cd libffmpeg
make clean
make distclean
cd ..
cd ffmpeg
make clean
make distclean
cd ..