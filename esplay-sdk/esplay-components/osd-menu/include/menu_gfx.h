/* GIMP RGB C-Source image dump (menu_gfx.h) */

static const struct
{
  unsigned int width;
  unsigned int height;
  unsigned int bytes_per_pixel; /* 2:RGB16, 3:RGB, 4:RGBA */
  unsigned char pixel_data[80 * 30 * 2 + 1];
} menu_gfx = {
    80,
    30,
    2,
    {
#include "menu_gfx_tile.inc"
    }};
