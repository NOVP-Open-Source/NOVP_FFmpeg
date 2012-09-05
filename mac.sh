chmod a+x gen.sh
./gen.sh
chmod a+x libffmpeg/configure
cp libffmpeg/build-to-splugin.sh FFmpeg/build-to-splugin.sh
chmod a+x FFmpeg/build-to-splugin.sh
cd FFmpeg
./build-to-splugin.sh
