/* GIMP RGB C-Source image dump (menu_bg.h) */

static const struct
{
  unsigned int width;
  unsigned int height;
  unsigned int bytes_per_pixel; /* 2:RGB16, 3:RGB, 4:RGBA */
  unsigned char pixel_data[150 * 192 * 2 + 1];
} menu_bg = {
    150,
    192,
    2,
    {
#include "menu_bg_tile.inc"
    }};
