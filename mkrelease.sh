#!/bin/sh

#edit your ESP-IDF path here, tested with release/v3.3 branch
export IDF_PATH=~/esp-idf

#tune this to match yours
export ESPLAY_SDK=~/esplay-retro-emulation/esplay-sdk

#ffmpeg path
export PATH=/c/ffmpeg/bin:$PATH

read -p "1:KT, 2:GBA    " number

if [ $number -eq 1 ];then                     
	cp /e/espproject/esplay-retro-emulation/esplay-sdk/esplay-components/esplay-hal/include/disp_spi_kt.h /e/espproject/esplay-retro-emulation/esplay-sdk/esplay-components/esplay-hal/include/disp_spi.h 
else  
   if [ $number -eq 2 ];then                   
	cp /e/espproject/esplay-retro-emulation/esplay-sdk/esplay-components/esplay-hal/include/disp_spi_gba.h /e/espproject/esplay-retro-emulation/esplay-sdk/esplay-components/esplay-hal/include/disp_spi.h 
   else
	echo "ERROR,exit"
	exit
fi

fi

#generate gfxtile
ffmpeg -i ~/esplay-retro-emulation/assets/gfxTile.png -f rawvideo -pix_fmt rgb565 ~/esplay-retro-emulation/assets/tile.raw -y
rm ~/esplay-retro-emulation/esplay-launcher/main/gfxTile.inc
cat ~/esplay-retro-emulation/assets/tile.raw | xxd -i > ~/esplay-retro-emulation/esplay-launcher/main/gfxTile.inc

#generate battile
ffmpeg -i ~/esplay-retro-emulation/assets/BAT.bmp -f rawvideo -pix_fmt rgb565 ~/esplay-retro-emulation/assets/battile.raw -y
rm ~/esplay-retro-emulation/esplay-sdk/esplay-components/esplay-hal/include/bat_tile.inc
cat ~/esplay-retro-emulation/assets/battile.raw | xxd -i > ~/esplay-retro-emulation/esplay-sdk/esplay-components/esplay-hal/include/bat_tile.inc

#generate osd
ffmpeg -i ~/esplay-retro-emulation/assets/menu_bg.bmp -f rawvideo -pix_fmt rgb565 ~/esplay-retro-emulation/assets/menu_bg.raw -y
rm ~/esplay-retro-emulation/esplay-sdk/esplay-components/osd-menu/include/menu_bg_tile.inc
cat ~/esplay-retro-emulation/assets/menu_bg.raw | xxd -i > ~/esplay-retro-emulation/esplay-sdk/esplay-components/osd-menu/include/menu_bg_tile.inc

ffmpeg -i ~/esplay-retro-emulation/assets/menu_gfx.bmp -f rawvideo -pix_fmt rgb565 ~/esplay-retro-emulation/assets/menu_gfx.raw -y
rm ~/esplay-retro-emulation/esplay-sdk/esplay-components/osd-menu/include/menu_gfx_tile.inc
cat ~/esplay-retro-emulation/assets/menu_gfx.raw | xxd -i > ~/esplay-retro-emulation/esplay-sdk/esplay-components/osd-menu/include/menu_gfx_tile.inc


#generate splash
rm ~/esplay-retro-emulation/assets/tile.raw
ffmpeg -i ~/esplay-retro-emulation/assets/show.png -f rawvideo -pix_fmt rgb565 ~/esplay-retro-emulation/assets/tile.raw -y
#cd esplay-txtreader
#make -j4
cd esplay-launcher
make -j4
cd ../esplay-gnuboy
make -j4
cd ../esplay-nofrendo
make -j4
cd ../esplay-smsplusgx
make -j4

cd ..


~/esplay-base-firmware/tools/mkfw/mkfw.exe Retro-Emulation assets/tile.raw 0 16 1835008 launcher esplay-launcher/build/RetroLauncher.bin 0 17 1048576 esplay-nofrendo esplay-nofrendo/build/esplay-nofrendo.bin 0 18 1048576 esplay-gnuboy esplay-gnuboy/build/esplay-gnuboy.bin 0 19 1376256 esplay-smsplusgx esplay-smsplusgx/build/esplay-smsplusgx.bin
rm espmini-emu.fw
mv firmware.fw espmini-emu.fw
