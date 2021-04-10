#ifndef _DISPLAY_GB_H_
#define _DISPLAY_GB_H_
#include <stdint.h>
#include "settings.h"

#define GB_FRAME_WIDTH 160
#define GB_FRAME_HEIGHT 144
static int init_back_ground_image = 0;
void write_gb_frame(const uint16_t *data, esplay_scale_option scale);
static const unsigned char bat[24 * 16 * 6 * 2 + 1] = {
#include "bat_tile.inc"
};
#endif