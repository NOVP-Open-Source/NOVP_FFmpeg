CC=gcc
LD=gcc
CFLAGS = -g -Wall -I. -I../FFmpeg
FFCFLAGS = -I../FFmpeg $(shell sdl-config --cflags)
LDFLAGS =
LIBS = -lz -lm -lpthread $(shell sdl-config --libs) -lSDL_ttf
TARGETS = 

FFMPEGDIR=../FFmpeg/
STATICLIBS= lib/libavformat.a lib/libavfilter.a lib/libavcodec.a lib/libswscale.a lib/libavutil.a lib/libswresample.a
FFLIBS=lib/libavdevice.a
OBJECTS = $(addsuffix .o, $(TARGETS)) 
AFILTER=af.o afilter/format.o afilter/af_channels.o afilter/af_format.o afilter/af_resample.o afilter/af_volume.o afilter/af_volnorm.o afilter/af_import.o afilter/filter.o afilter/window.o afilter/util.o
OBJS = aclib.o fmt-conversion.o libxplayer.o $(AFILTER) eventqueue.o libffplayopts.o libffplay.o debug.o
# wav.o
OBJFF=cmdutils.o
OBJSHM=libshmem/libshmem.o

ALL = libxplayer.a debugview

all:	$(STATICLIBS) $(ALL)

debugview:	debugview.o debug.o
	$(LD) $< $(LDFLAGS) $(CFLAGS) $($@_OBJ) -o $@ debug.o

$(TARGETS):	%:%.o Makefile $(STATICLIBS)
	$(CC) -o $@ $< $(LIBS) $(STATICLIBS)

$(OBJECTS):	%.o:%.c $(STATICLIBS) config.h libffplay.h
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJS):	%.o:%.c $(STATICLIBS) config.h libffplay.h debug.h
	$(CC) $(CFLAGS) -c $< -o $@

libxplayer.so.0.0: $(OBJS) $(STATICLIBS)
	$(LD) -shared -gcc -Wl,-soname,libxplayer.so.0 -o libxplayer.so.0.0 $(OBJS) $(STATICLIBS) -lm -g $(LDFLAGS)
	/sbin/ldconfig -n .
	ln -sf libxplayer.so.0 libxplayer.so

libxplayer.a: $(OBJS)
	$(AR) -r libxplayer.a $(OBJS)

lib/libswscale.a: build-to-splugin.sh
	@echo 'Build ffmpeg...' ; \
	opwd=`pwd` ; \
	cd "$(FFMPEGDIR)" ; \
	$$opwd/build-to-splugin.sh "$$opwd" ; \
	cd "$$opwd"

lib/libavcodec.a: build-to-splugin.sh
	@echo 'Build ffmpeg...' ; \
	opwd=`pwd` ; \
	cd "$(FFMPEGDIR)" ; \
	$$opwd/build-to-splugin.sh "$$opwd" ; \
	cd "$$opwd"

lib/libavfilter.a: build-to-splugin.sh
	@echo 'Build ffmpeg...' ; \
	opwd=`pwd` ; \
	cd "$(FFMPEGDIR)" ; \
	$$opwd/build-to-splugin.sh "$$opwd" ; \
	cd "$$opwd"

lib/libavutil.a: build-to-splugin.sh
	@echo 'Build ffmpeg...' ; \
	opwd=`pwd` ; \
	cd "$(FFMPEGDIR)" ; \
	$$opwd/build-to-splugin.sh "$$opwd" ; \
	cd "$$opwd"

lib/libavformat.a: build-to-splugin.sh
	@echo 'Build ffmpeg...' ; \
	opwd=`pwd` ; \
	cd "$(FFMPEGDIR)" ; \
	$$opwd/build-to-splugin.sh "$$opwd" ; \
	cd "$$opwd"

lib/libswresample.a: build-to-splugin.sh
	@echo 'Build ffmpeg...' ; \
	opwd=`pwd` ; \
	cd "$(FFMPEGDIR)" ; \
	$$opwd/build-to-splugin.sh "$$opwd" ; \
	cd "$$opwd"

lib/libavdevice.a: build-to-splugin.sh
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



