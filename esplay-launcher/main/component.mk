#
# "main" pseudo-component makefile.
#
# (Uses default behaviour of compiling all source files in directory, adding 'include' to include path.)
COMPONENT_ADD_INCLUDEDIRS := include
COMPONENTS_EXTRA_CLEAN := gfxTile.inc gfxTile.raw
main.o: /e/espproject/esplay-retro-emulation/esplay-launcher/main/gfxTile.inc
