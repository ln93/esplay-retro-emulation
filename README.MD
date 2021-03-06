 # ESPlay Retro Emulation 
![retro-emulation](https://raw.githubusercontent.com/ln93/esplay-retro-emulation/master/docs/retro_emulation.png) [![GitHub release](https://img.shields.io/github/release/ln93/esplay-retro-emulation.svg)](https://GitHub.com/ln93/esplay-retro-emulation/releases/) ![Github All Releases](https://img.shields.io/github/downloads/ln93/esplay-retro-emulation/total.svg) ![Github Last Commit](https://img.shields.io/github/last-commit/ln93/esplay-retro-emulation)

Description and Video Review
----------------------------
<a href="http://www.youtube.com/watch?feature=player_embedded&v=NfbxcjU85Ac" target="_blank"><img src="https://i.ytimg.com/vi/NfbxcjU85Ac/hqdefault.jpg" alt="Youtube Video" width="480" height="320" border="10" /></a>

This projects is forked from pebri86, as a set of firmware for playing second and third generation gaming platform and to be runs on [esplay-micro-hardware], an ESP32 Devices with WROVER Module (4MB PSRAM) which you could find the details on my [hackaday] page. This hardware also commercially available on the [Makerfabs] Online Store. Due to small Flash ROM (only 4MB) currently only 3 game emulator could be bundled altough this device support 7 emulator including Atari, PC Engine, ZX-Spectrum, MSX, etc.
The main feature of this projects is bundled as follow:
- Nofrendo NES Emulator for play Nintendo ROM
- GNUBoy for play GameBoy and GameBoy Color
- SMSPlusGX could play Sega Master System, Game Gear and Coleco Vision ROM
- In-Game menu on Emulator
- <b>NEW!</b> Built-in Audio Player for playing music, support WAV, MP3, OGG, and FLAC audio files.
- Esplay launcher, new code base using UGUI library

This fork modified the GUI and translate most elements to Chinese.

Place your Audio files that supported (mp3, flac, wav and ogg) to audio folder on the root of sdcard, if folder not exist just created one and place audio files on them.

![esplay-audio1](https://raw.githubusercontent.com/ln93/esplay-retro-emulation/staging/docs/esplay_audio1.jpg)

![esplay-audio2](https://raw.githubusercontent.com/ln93/esplay-retro-emulation/staging/docs/esplay_audio2.jpg)

Launcher Option
---------------

Now you have options to choose launcher, Retro-ESP32 launcher by [32teeth] or ESPlay launcher by me.
ESPlay Launcher has Wifi AP options so you can upload rom by using web interface via phone or your computer.

Here spoiler Retro-ESP32 launcher running on [esplay-micro-hardware]
![esplay-micro-hardware](https://raw.githubusercontent.com/ln93/esplay-retro-emulation/master/docs/esplay-retro-esp32.png)

Compiling
---------

This code is an esp-idf project. You will need esp-idf to compile it. Newer versions of esp-idf may introduce incompatibilities with this code;
for your reference, the code was tested against release/v3.3 branch of esp-idf.
edit mkrelease.sh with correct location for your mkfw.
Find mkfw tool in the base firmware in my repository.

Also to get compiling correctly you need ffmpeg installed on your system and included in the PATH Environment.

edit mkrelease.sh to match your setup path(to match your location installation of esp-idf and clone of this repo). and simply execute mkrelease.sh
then you'll find esplay-retro-emu.fw in the root of repo.

run mkrelease.sh to compile and make an .fw file that could be flashed by base firmware bootloader. Place .fw file in esplay/firmware folder on sdcard, power on esplay device while press B button until bootloader screen appear. select .fw then A -> Start to flash firmware.

Base firmware is a modified bootloader fork from odroid-go.

ROM
---
Extract skeleton.zip from docs, and copy to the root of sdcard. Please provide ROM by yourself and place them in the appropriate folder on sdcard.

DONATION
--------
The software are free to use but we also accept small donation to make the project lives and continue development
<a href="https://www.paypal.me/ln93/1.99USD" target="_blank"><img src="https://raw.githubusercontent.com/ln93/esplay-retro-emulation/staging/docs/donate.jpeg" alt="Donate" /></a>

Source Author and Special Thanks
-------------------------
- Audio Player based on [ogo-shell] projects by [Passpartout], thank you very much for sharing awesome code.
- All emulator based on [Go-Play] for Odroid-go by [OtherCrashOverride].

All Contributors
------------
Thanks to contribute your work for this project.
- [32teeth]


[32teeth]:https://github.com/32teeth
[ogo-shell]:https://github.com/Paspartout/ogo-shell
[Paspartout]:https://github.com/Paspartout
[OtherCrashOverride]:https://github.com/OtherCrashOverride
[Go-Play]:https://github.com/OtherCrashOverride/go-play
[esplay-micro-hardware]: https://github.com/ln93/esplay_micro_hardware
[Makerfabs]: https://www.makerfabs.com/esplay-micro.html
[hackaday]: https://hackaday.io/project/166707-esplay-micro
[pebri86]: https://github.com/pebri86/esplay-retro-emulation
