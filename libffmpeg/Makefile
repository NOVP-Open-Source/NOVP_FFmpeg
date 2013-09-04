CC=gcc
LD=gcc

ifeq ("${TERM}", "cygwin")
CFLAGS = -g -I. -I../FFmpeg -DCONFIG_FASTMEMCPY=1
LDFLAGS = 
else
CFLAGS = -g -I. -I../FFmpeg -DCONFIG_FASTMEMCPY=1 -arch i386
LDFLAGS = -arch i386 -read_only_relocs suppress
endif

FFCFLAGS = -I../FFmpeg -Wformat-extra-args -Wformat $(shell sdl-config --cflags)
LIBS = -lz -lm $(shell sdl-config --libs) -lSDL_ttf
TARGETS = 

CONFIGURATION=configuration
FFMPEGDIR=../FFmpeg/
DYNAMICLIBSMAC= lib/libavformat.dylib lib/libavfilter.dylib lib/libavcodec.dylib lib/libswscale.dylib lib/libavutil.dylib lib/libswresample.dylib
DYNAMICLIBSWIN= lib/avformat.dll lib/avfilter.dll lib/avcodec.dll lib/swscale.dll lib/avutil.dll lib/swresample.dll
FFLIBSMAC=lib/libavdevice.dylib
FFLIBSWIN=lib/libavdevice.dll
OBJECTS = $(addsuffix .o, $(TARGETS)) 
AFILTER=af.o afilter/format.o afilter/af_channels.o afilter/af_format.o afilter/af_resample.o afilter/af_volume.o afilter/af_volnorm.o afilter/af_import.o afilter/filter.o afilter/window.o afilter/util.o
OBJS = aclib.o fmt-conversion.o libxplayer.o $(AFILTER) eventqueue.o libffplayopts.o libffplay.o debug.o
# wav.o
OBJFF=cmdutils.o
OBJSHM=libshmem/libshmem.o

ALL = libxplayer.a debugview done

ifeq ("${TERM}", "cygwin")
all:	$(DYNAMICLIBSWIN) $(ALL)
$(TARGETS):	%:%.o Makefile $(DYNAMICLIBSWIN)
	$(CC) -o $@ $< $(LIBS) $(DYNAMICLIBSWIN)

$(OBJECTS):	%.o:%.c $(DYNAMICLIBSWIN) config.h libffplay.h
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJS):	%.o:%.c $(DYNAMICLIBSWIN) config.h libffplay.h debug.h
	$(CC) $(CFLAGS) -c $< -o $@
else
all:	$(DYNAMICLIBSMAC) $(ALL)
$(TARGETS):	%:%.o Makefile $(DYNAMICLIBSMAC)
	$(CC) -o $@ $< $(LIBS) $(DYNAMICLIBSMAC)

$(OBJECTS):	%.o:%.c $(DYNAMICLIBSMAC) config.h libffplay.h
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJS):	%.o:%.c $(DYNAMICLIBSMAC) config.h libffplay.h debug.h
	$(CC) $(CFLAGS) -c $< -o $@
endif

debugview:	debugview.o debug.o
	$(LD) $< $(LDFLAGS) $(CFLAGS) $($@_OBJ) -o $@ debug.o

done:
ifeq ("${TERM}", "cygwin")
	@echo 'Windows detected, converting libraries...' ; \
	opwd=`pwd` ; \
	cd "$(FFMPEGDIR)" ; \
	$$opwd/objconv.exe -fcoff $$opwd/libxplayer.a $$opwd/libxplayer.lib > /dev/null
	@echo 'Build done.'
endif

libxplayer.so.0.0: $(OBJS) $(DYNAMICLIBS)
	$(LD) -shared -gcc -Wl,-soname,libxplayer.so.0 -o libxplayer.so.0.0 $(OBJS) $(DYNAMICLIBS) -lm -g $(LDFLAGS)
	/sbin/ldconfig -n .
	ln -sf libxplayer.so.0 libxplayer.so

libxplayer.a: $(OBJS)
	$(AR) -r libxplayer.a $(OBJS)

ifeq ("${TERM}", "cygwin")
configuration:
	@echo 'Configuring ffmpeg...' ; \
	opwd=`pwd` ; \
	cd "$(FFMPEGDIR)" ; \
	./configure --disable-doc --disable-ffmpeg --disable-ffprobe --disable-ffserver --disable-dxva2 --enable-runtime-cpudetect --disable-muxers --disable-bsfs --enable-openssl --disable-static --enable-shared --enable-pthreads ; \
	cd "$$opwd"
else
configuration:
	@echo 'Configuring ffmpeg...' ; \
	opwd=`pwd` ; \
	cd "$(FFMPEGDIR)" ; \
	./configure --disable-doc --disable-ffmpeg --disable-ffprobe --disable-ffserver --disable-dxva2 --enable-runtime-cpudetect --disable-muxers --disable-bsfs --enable-openssl --extra-cflags="-arch i386" --extra-ldflags='-arch i386 -read_only_relocs suppress' --arch=i386 --cpu=i386 --target-os=darwin --enable-cross-compile --disable-static --enable-shared ; \
	cd "$$opwd"
endif

lib/libswscale.dylib: build-to-splugin.sh
	@echo 'Build ffmpeg...' ; \
	opwd=`pwd` ; \
	cd "$(FFMPEGDIR)" ; \
	$$opwd/build-to-splugin.sh "$$opwd" ; \
	cd "$$opwd"

lib/libavcodec.dylib: build-to-splugin.sh
	@echo 'Build ffmpeg...' ; \
	opwd=`pwd` ; \
	cd "$(FFMPEGDIR)" ; \
	$$opwd/build-to-splugin.sh "$$opwd" ; \
	cd "$$opwd"

lib/libavfilter.dylib: build-to-splugin.sh
	@echo 'Build ffmpeg...' ; \
	opwd=`pwd` ; \
	cd "$(FFMPEGDIR)" ; \
	$$opwd/build-to-splugin.sh "$$opwd" ; \
	cd "$$opwd"

lib/libavutil.dylib: build-to-splugin.sh
	@echo 'Build ffmpeg...' ; \
	opwd=`pwd` ; \
	cd "$(FFMPEGDIR)" ; \
	$$opwd/build-to-splugin.sh "$$opwd" ; \
	cd "$$opwd"

lib/libavformat.dylib: build-to-splugin.sh
	@echo 'Build ffmpeg...' ; \
	opwd=`pwd` ; \
	cd "$(FFMPEGDIR)" ; \
	$$opwd/build-to-splugin.sh "$$opwd" ; \
	cd "$$opwd"

lib/libswresample.dylib: build-to-splugin.sh
	@echo 'Build ffmpeg...' ; \
	opwd=`pwd` ; \
	cd "$(FFMPEGDIR)" ; \
	$$opwd/build-to-splugin.sh "$$opwd" ; \
	cd "$$opwd"

lib/libavdevice.dylib: build-to-splugin.sh
	@echo 'Build ffmpeg...' ; \
	opwd=`pwd` ; \
	cd "$(FFMPEGDIR)" ; \
	$$opwd/build-to-splugin.sh "$$opwd" ; \
	cd "$$opwd"

lib/swscale.dll: build-to-splugin.sh
	@echo 'Build ffmpeg...' ; \
	opwd=`pwd` ; \
	cd "$(FFMPEGDIR)" ; \
	$$opwd/build-to-splugin.sh "$$opwd" ; \
	cd "$$opwd"

lib/avcodec.dll: build-to-splugin.sh
	@echo 'Build ffmpeg...' ; \
	opwd=`pwd` ; \
	cd "$(FFMPEGDIR)" ; \
	$$opwd/build-to-splugin.sh "$$opwd" ; \
	cd "$$opwd"

lib/avfilter.dll: build-to-splugin.sh
	@echo 'Build ffmpeg...' ; \
	opwd=`pwd` ; \
	cd "$(FFMPEGDIR)" ; \
	$$opwd/build-to-splugin.sh "$$opwd" ; \
	cd "$$opwd"

lib/avutil.dll: build-to-splugin.sh
	@echo 'Build ffmpeg...' ; \
	opwd=`pwd` ; \
	cd "$(FFMPEGDIR)" ; \
	$$opwd/build-to-splugin.sh "$$opwd" ; \
	cd "$$opwd"

lib/avformat.dll: build-to-splugin.sh
	@echo 'Build ffmpeg...' ; \
	opwd=`pwd` ; \
	cd "$(FFMPEGDIR)" ; \
	$$opwd/build-to-splugin.sh "$$opwd" ; \
	cd "$$opwd"

lib/swresample.dll: build-to-splugin.sh
	@echo 'Build ffmpeg...' ; \
	opwd=`pwd` ; \
	cd "$(FFMPEGDIR)" ; \
	$$opwd/build-to-splugin.sh "$$opwd" ; \
	cd "$$opwd"

lib/avdevice.dll: build-to-splugin.sh
	@echo 'Build ffmpeg...' ; \
	opwd=`pwd` ; \
	cd "$(FFMPEGDIR)" ; \
	$$opwd/build-to-splugin.sh "$$opwd" ; \
	cd "$$opwd"

ffmpeg_config.h: build-to-splugin.sh
	@echo 'Build ffmpeg...' ; \
	opwd=`pwd` ; \
	cd "$(FFMPEGDIR)" ; \
	$$opwd/build-to-splugin.sh "$$opwd" ; \
	cd "$$opwd"

config.h: configure
	./configure

clean:
	@rm -f *.o $(TARGETS)
	@rm -f *.o $(ALL) libxplayer.so.0 libxplayer.so
	@rm -f configure.log config.h

distclean: dist-clean

dist-clean:
	@rm -f *.o $(TARGETS)
	@rm -fr lib
	@rm -fr libavcodec
	@rm -fr libavcore
	@rm -fr libavformat
	@rm -fr libavutil
	@rm -fr libswscale
	@rm -fr libxplayer.lib
	@rm -f *.o $(ALL) libxplayer.so.0 libxplayer.so
	@rm -f configure.log config.h
