@echo off
objconv\objconv.exe -fcoff libffmpeg\lib\libavcodec.a libffmpeg\lib\libavcodec.lib
objconv\objconv.exe -fcoff libffmpeg\lib\libavfilter.a libffmpeg\lib\libavfilter.lib
objconv\objconv.exe -fcoff libffmpeg\lib\libavformat.a libffmpeg\lib\libavformat.lib
objconv\objconv.exe -fcoff libffmpeg\lib\libavutil.a libffmpeg\lib\libavutil.lib
objconv\objconv.exe -fcoff libffmpeg\lib\libswscale.a libffmpeg\lib\libswscale.lib
objconv\objconv.exe -fcoff libffmpeg\lib\libswresample.a libffmpeg\lib\libswresample.lib
objconv\objconv.exe -fcoff libffmpeg\libxplayer.a libffmpeg\libxplayer.lib

