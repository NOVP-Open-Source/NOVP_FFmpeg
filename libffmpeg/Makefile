CC=gcc
LD=gcc

ifeq ("${TERM}", "cygwin")
CFLAGS = -g -I. -I../FFmpeg -DCONFIG_FASTMEMCPY=1
LDFLAGS = 
configurestr = --disable-doc --disable-ffmpeg --disable-ffprobe --disable-ffserver --disable-dxva2 --enable-runtime-cpudetect --disable-muxers --disable-bsfs --enable-openssl --disable-static --enable-shared --enable-pthreads ; \
DEBUGVIEW=debugview.exe
else
ifeq ("${FFMPEGMAC}", "32")
CFLAGS = -g -I. -I../FFmpeg -DCONFIG_FASTMEMCPY=1 -arch i386
LDFLAGS = -arch i386 -read_only_relocs suppress
configurestr = --disable-asm --disable-doc --disable-ffmpeg --disable-ffprobe --disable-ffserver --disable-dxva2 --enable-runtime-cpudetect --disable-muxers --disable-bsfs --enable-openssl --extra-cflags="-arch i386" --extra-ldflags='-arch i386 -read_only_relocs suppress' --arch=i386 --cpu=i386--target-os=darwin --enable-cross-compile --disable-static --enable-shared
else
CFLAGS = -g -I. -I../FFmpeg -DCONFIG_FASTMEMCPY=1
LDFLAGS = 
configurestr = --disable-asm --disable-doc --disable-ffmpeg --disable-ffprobe --disable-ffserver --disable-dxva2 --enable-runtime-cpudetect --disable-muxers --disable-bsfs --enable-openssl --target-os=darwin --disable-static --enable-shared
endif
DEBUGVIEW=debugview
endif

FFCFLAGS = -I../FFmpeg -Wformat-extra-args -Wformat $(shell sdl-config --cflags)
LIBS = -lz -lm $(shell sdl-config --libs) -lSDL_ttf
TARGETS = 

CONFIGURATION=configuration
FFMPEGDIR=../FFmpeg/
DYNAMICLIBSMAC= lib/libavformat.dylib lib/libavfilter.dylib lib/libavcodec.dylib lib/libswscale.dylib lib/libavutil.dylib lib/libswresample.dylib
DYNAMICLIBSWIN= lib/avformat-54.dll lib/avfilter-3.dll lib/avcodec-54.dll lib/swscale-2.dll lib/avutil-51.dll lib/swresample-0.dll
FFLIBSMAC=lib/libavdevice.dylib
FFLIBSWIN=lib/libavdevice.dll
OBJECTS = $(addsuffix .o, $(TARGETS)) 
AFILTER=af.o afilter/format.o afilter/af_channels.o afilter/af_format.o afilter/af_resample.o afilter/af_volume.o afilter/af_volnorm.o afilter/af_import.o afilter/filter.o afilter/window.o afilter/util.o
OBJS = aclib.o fmt-conversion.o libxplayer.o $(AFILTER) eventqueue.o libffplayopts.o libffplay.o debug.o
# wav.o
OBJFF=cmdutils.o
OBJSHM=libshmem/libshmem.o

ALL = libxplayer.a $(DEBUGVIEW) done

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

$(DEBUGVIEW):	debugview.o debug.o
	$(LD) $< $(LDFLAGS) $(CFLAGS) $($@_OBJ) -o $@ debug.o

ifeq ("${TERM}", "cygwin")
done:
	@echo 'Windows detected, converting libraries...' ; \
	opwd=`pwd` ; \
	cd "$(FFMPEGDIR)" ; \
	$$opwd/objconv.exe -fcoff $$opwd/libxplayer.a $$opwd/libxplayer.lib > /dev/null
	@echo 'Build done.'
else
done:
	@for f in lib/*.dylib ; do \
	echo Processing $$f file.. ; \
	install_name_tool -change /usr/local/lib/libavcodec.54.dylib @loader_path/libavcodec.54.dylib $$f ; \
	install_name_tool -change /usr/local/lib/libavdevice.54.dylib @loader_path/libavdevice.54.dylib $$f ; \
	install_name_tool -change /usr/local/lib/libavfilter.3.dylib @loader_path/libavfilter.3.dylib $$f ; \
	install_name_tool -change /usr/local/lib/libavformat.54.dylib @loader_path/libavformat.54.dylib $$f ; \
	install_name_tool -change /usr/local/lib/libavutil.51.dylib @loader_path/libavutil.51.dylib $$f ; \
	install_name_tool -change /usr/local/lib/libswresample.0.dylib @loader_path/libswresample.0.dylib $$f ; \
	install_name_tool -change /usr/local/lib/libswscale.2.dylib @loader_path/libswscale.2.dylib $$f ; \
	install_name_tool -id $$f $$f ; \
	done
endif

libxplayer.so.0.0: $(OBJS) $(DYNAMICLIBS)
	$(LD) -shared -gcc -Wl,-soname,libxplayer.so.0 -o libxplayer.so.0.0 $(OBJS) $(DYNAMICLIBS) -lm -g $(LDFLAGS)
	/sbin/ldconfig -n .
	ln -sf libxplayer.so.0 libxplayer.so

libxplayer.a: $(OBJS)
	$(AR) -r libxplayer.a $(OBJS)

configuration:
	@echo 'Configuring ffmpeg...' ; \
	opwd=`pwd` ; \
	cd "$(FFMPEGDIR)" ; \
	./configure $(configurestr) ; \
	cd "$$opwd"

lib/libswscale.dylib: build-to-libffmpeg.sh
	@echo 'Build libswscale...' ; \
	opwd=`pwd` ; \
	cd "$(FFMPEGDIR)" ; \
	$$opwd/build-to-libffmpeg.sh "$$opwd" ; \
	cd "$$opwd"

lib/libavcodec.dylib: build-to-libffmpeg.sh
	@echo 'Build libavcodec...' ; \
	opwd=`pwd` ; \
	cd "$(FFMPEGDIR)" ; \
	$$opwd/build-to-libffmpeg.sh "$$opwd" ; \
	cd "$$opwd"

lib/libavfilter.dylib: build-to-libffmpeg.sh
	@echo 'Build libavfilter...' ; \
	opwd=`pwd` ; \
	cd "$(FFMPEGDIR)" ; \
	$$opwd/build-to-libffmpeg.sh "$$opwd" ; \
	cd "$$opwd"

lib/libavutil.dylib: build-to-libffmpeg.sh
	@echo 'Build libavutil...' ; \
	opwd=`pwd` ; \
	cd "$(FFMPEGDIR)" ; \
	$$opwd/build-to-libffmpeg.sh "$$opwd" ; \
	cd "$$opwd"

lib/libavformat.dylib: build-to-libffmpeg.sh
	@echo 'Build libavformat...' ; \
	opwd=`pwd` ; \
	cd "$(FFMPEGDIR)" ; \
	$$opwd/build-to-libffmpeg.sh "$$opwd" ; \
	cd "$$opwd"

lib/libswresample.dylib: build-to-libffmpeg.sh
	@echo 'Build libswresample...' ; \
	opwd=`pwd` ; \
	cd "$(FFMPEGDIR)" ; \
	$$opwd/build-to-libffmpeg.sh "$$opwd" ; \
	cd "$$opwd"

lib/libavdevice.dylib: build-to-libffmpeg.sh
	@echo 'Build libavdevice...' ; \
	opwd=`pwd` ; \
	cd "$(FFMPEGDIR)" ; \
	$$opwd/build-to-libffmpeg.sh "$$opwd" ; \
	cd "$$opwd"

lib/swscale-2.dll: build-to-libffmpeg.sh
	@echo 'Build libswscale...' ; \
	opwd=`pwd` ; \
	cd "$(FFMPEGDIR)" ; \
	$$opwd/build-to-libffmpeg.sh "$$opwd" ; \
	cd "$$opwd"

lib/avcodec-54.dll: build-to-libffmpeg.sh
	@echo 'Build libavcodec...' ; \
	opwd=`pwd` ; \
	cd "$(FFMPEGDIR)" ; \
	$$opwd/build-to-libffmpeg.sh "$$opwd" ; \
	cd "$$opwd"

lib/avfilter-3.dll: build-to-libffmpeg.sh
	@echo 'Build libavfilter...' ; \
	opwd=`pwd` ; \
	cd "$(FFMPEGDIR)" ; \
	$$opwd/build-to-libffmpeg.sh "$$opwd" ; \
	cd "$$opwd"

lib/avutil-51.dll: build-to-libffmpeg.sh
	@echo 'Build libavutil...' ; \
	opwd=`pwd` ; \
	cd "$(FFMPEGDIR)" ; \
	$$opwd/build-to-libffmpeg.sh "$$opwd" ; \
	cd "$$opwd"

lib/avformat-54.dll: build-to-libffmpeg.sh
	@echo 'Build libavformat...' ; \
	opwd=`pwd` ; \
	cd "$(FFMPEGDIR)" ; \
	$$opwd/build-to-libffmpeg.sh "$$opwd" ; \
	cd "$$opwd"

lib/swresample-0.dll: build-to-libffmpeg.sh
	@echo 'Build libswresample...' ; \
	opwd=`pwd` ; \
	cd "$(FFMPEGDIR)" ; \
	$$opwd/build-to-libffmpeg.sh "$$opwd" ; \
	cd "$$opwd"

lib/avdevice-54.dll: build-to-libffmpeg.sh
	@echo 'Build libavdevice...' ; \
	opwd=`pwd` ; \
	cd "$(FFMPEGDIR)" ; \
	$$opwd/build-to-libffmpeg.sh "$$opwd" ; \
	cd "$$opwd"

ffmpeg_config.h: build-to-libffmpeg.sh
	@echo 'Build ffmpeg_config...' ; \
	opwd=`pwd` ; \
	cd "$(FFMPEGDIR)" ; \
	$$opwd/build-to-libffmpeg.sh "$$opwd" ; \
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
