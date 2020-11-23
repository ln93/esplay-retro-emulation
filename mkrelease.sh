#!/bin/sh

#edit your ESP-IDF path here, tested with release/v3.3 branch
export IDF_PATH=~/esp-idf

#tune this to match yours
export ESPLAY_SDK=~/esplay-retro-emulation/esplay-sdk

#ffmpeg path
export PATH=/c/ffmpeg/bin:$PATH

cd esplay-launcher
make -j4

cd ../esplay-gnuboy
make -j4
cd ../esplay-nofrendo
make -j4
cd ../esplay-smsplusgx
make -j4

cd ..

#ffmpeg -i ~/esplay-retro-emulation/assets/Tile.png -f rawvideo -pix_fmt rgb565 ~/esplay-retro-emulation/assets/tile.raw -y
~/esplay-base-firmware/tools/mkfw/mkfw.exe Retro-Emulation assets/tile.raw 0 16 1835008 launcher esplay-launcher/build/RetroLauncher.bin 0 17 1048576 esplay-nofrendo esplay-nofrendo/build/esplay-nofrendo.bin 0 18 1048576 esplay-gnuboy esplay-gnuboy/build/esplay-gnuboy.bin 0 19 1376256 esplay-smsplusgx esplay-smsplusgx/build/esplay-smsplusgx.bin
rm espmini-emu.fw
mv firmware.fw espmini-emu.fw
