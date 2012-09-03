@echo off
..\..\vendor\objconv\objconv.exe -fcoff libffmpeg\lib\libavcodec.a libffmpeg\lib\libavcodec.lib
..\..\vendor\objconv\objconv.exe -fcoff libffmpeg\lib\libavfilter.a libffmpeg\lib\libavfilter.lib
..\..\vendor\objconv\objconv.exe -fcoff libffmpeg\lib\libavformat.a libffmpeg\lib\libavformat.lib
..\..\vendor\objconv\objconv.exe -fcoff libffmpeg\lib\libavutil.a libffmpeg\lib\libavutil.lib
..\..\vendor\objconv\objconv.exe -fcoff libffmpeg\lib\libswscale.a libffmpeg\lib\libswscale.lib
..\..\vendor\objconv\objconv.exe -fcoff libffmpeg\libxplayer.a libffmpeg\libxplayer.lib

