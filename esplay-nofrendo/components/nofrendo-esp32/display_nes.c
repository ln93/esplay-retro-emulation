#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "display_nes.h"
#include "display.h"
#include "disp_spi.h"

extern uint16_t myPalette[];
extern uint16_t *line[];
extern int32_t scaleAlg;
extern uint8_t batlevel;
#define LINE_COUNT (8)
#define AVERAGE(a, b) (((((a) ^ (b)) & 0xf7deU) >> 1) + ((a) & (b)))

static uint16_t getPixelNes(const uint8_t *bufs, int x, int y, int w1, int h1, int w2, int h2, int x_ratio, int y_ratio)
{
    uint16_t col;
    if (scaleAlg == NEAREST_NEIGHBOR || x >= w2 - 1 || y >= h2 - 1) //fuck yeah
    {
        /* Resize using nearest neighbor alghorithm */
        /* Simple and fastest way but low quality   */
        int x2 = ((x * x_ratio) >> 16);
        int y2 = ((y * y_ratio) >> 16);
        col = myPalette[bufs[(y2 * w1) + x2]];
        return (col >> 8) | ((col & 0xff) << 8);
    }
    else if (scaleAlg == BILINIER_INTERPOLATION)
    {
        /* Resize using bilinear interpolation */
        /* higher quality but lower performance, */
        int x_diff, y_diff, xv, yv, red, green, blue, a, b, c, d, index;

        xv = (int)((x_ratio * x) >> 16);
        yv = (int)((y_ratio * y) >> 16);

        y_diff = (y_ratio * y) - (yv << 16);
        x_diff = (x_ratio * x) - (xv << 16);
        index = yv * w1 + xv;

        a = myPalette[bufs[index]];
        b = myPalette[bufs[index + 1]];
        c = myPalette[bufs[index + w1]];
        d = myPalette[bufs[index + w1 + 1]];

        red = (((a >> 11) & 0x1f) * (4 - (x_diff >> 14)) * (4 - (y_diff >> 14)) + ((b >> 11) & 0x1f) * (x_diff >> 14) * (4 - (y_diff >> 14)) +
               ((c >> 11) & 0x1f) * (y_diff >> 14) * (4 - (x_diff >> 14)) + ((d >> 11) & 0x1f) * ((x_diff >> 14) * (y_diff >> 14)));
        red = (red >> 4);

        green = (((a >> 5) & 0x3f) * (4 - (x_diff >> 14)) * (4 - (y_diff >> 14)) + ((b >> 5) & 0x3f) * (x_diff >> 14) * (4 - (y_diff >> 14)) +
                 ((c >> 5) & 0x3f) * (y_diff >> 14) * (4 - (x_diff >> 14)) + ((d >> 5) & 0x3f) * ((x_diff >> 14) * (y_diff >> 14)));
        green = (green >> 4);

        blue = (((a)&0x1f) * (4 - (x_diff >> 14)) * (4 - (y_diff >> 14)) + ((b)&0x1f) * (x_diff >> 14) * (4 - (y_diff >> 14)) +
                ((c)&0x1f) * (y_diff >> 14) * (4 - (x_diff >> 14)) + ((d)&0x1f) * ((x_diff >> 14) * (y_diff >> 14)));
        blue = (blue >> 4);

        col = ((int)red << 11) | ((int)green << 5) | ((int)blue);

        return (col >> 8) | ((col & 0xff) << 8);
    }
    else /* scaleAlg == BOX_FILTERED */
    {
        // experimental, currently disabled
        int xv, yv, a, b, c, d, index, p, q;

        xv = (int)((x_ratio * x) >> 16);
        yv = (int)((y_ratio * y) >> 16);

        index = yv * w1 + xv;

        a = myPalette[bufs[index]];
        b = myPalette[bufs[index + 1]];
        c = myPalette[bufs[index + w1]];
        d = myPalette[bufs[index + w1 + 1]];

        p = AVERAGE(a, b);
        q = AVERAGE(c, d);

        col = AVERAGE(p, q);

        return (col >> 8) | ((col & 0xff) << 8);
    }
}
void write_nes_frame(const uint8_t *data, esplay_scale_option scale)
{
    short x, y, xpos, ypos, outputWidth, outputHeight;
    int sending_line = -1;
    int calc_line = 0;
    int temp1, temp2;
    int x_ratio, y_ratio;
    static const unsigned char *pbat;
    if (batlevel > 4)
    {
        pbat = &bat[24 * 16 * 2 * 0];
    }
    else
    {
        pbat = &bat[24 * 16 * 2 * (5 - batlevel)]; //0 1 2 3 4
    }

    if (data == NULL)
    {
        for (y = 0; y < LCD_HEIGHT; y = y + 1)
        {
            for (x = 0; x < LCD_WIDTH; x++)
            {
                line[calc_line][x] = 0;
            }
            if (sending_line != -1)
                send_line_finish();
            sending_line = calc_line;
            calc_line = (calc_line == 1) ? 0 : 1;
            send_lines_ext(y, 0, LCD_WIDTH, line[sending_line], 1);
        }
        send_line_finish();
    }
    else
    {
        switch (scale)
        {
        case SCALE_NONE:
            outputHeight = LCD_HEIGHT;
            outputWidth = 220;
            xpos = (LCD_WIDTH - outputWidth) / 2;
            x_ratio = (int)(((NES_FRAME_WIDTH - 1) << 16) / (outputWidth - 1));
            y_ratio = (int)(((NES_FRAME_HEIGHT - 1) << 16) / (outputHeight - 1));

            for (y = 0; y < outputHeight; y += LINE_COUNT)
            {
                for (int i = 0; i < LINE_COUNT; i = i + 1)
                {
                    if ((y + i) >= outputHeight)
                        break;

                    int index = (i)*outputWidth;

                    if (y < 9)
                    {
                        for (x = 0; x < (outputWidth - 24); x++)
                        {
                            line[calc_line][index++] = getPixelNes(data, x, (y + i), NES_FRAME_WIDTH, NES_FRAME_HEIGHT, outputWidth, outputHeight, x_ratio, y_ratio);
                        }
                        for (x = (outputWidth - 24); x < outputWidth; x++)
                        {
                            temp1 = *pbat++;
                            temp2 = (temp1 << 8) + *pbat++;
                            if (temp2 == 0)
                            {
                                line[calc_line][index++] = getPixelNes(data, x, (y + i), NES_FRAME_WIDTH, NES_FRAME_HEIGHT, outputWidth, outputHeight, x_ratio, y_ratio);
                            }
                            else
                            {
                                line[calc_line][index++] = temp2;
                            }
                        }
                    }
                    else
                    {
                        for (x = 0; x < outputWidth; x++)
                        {
                            line[calc_line][index++] = getPixelNes(data, x, (y + i), NES_FRAME_WIDTH, NES_FRAME_HEIGHT, outputWidth, outputHeight, x_ratio, y_ratio);
                        }
                    }
                }
                if (sending_line != -1)
                    send_line_finish();
                sending_line = calc_line;
                calc_line = (calc_line == 1) ? 0 : 1;
                send_lines_ext(y, xpos, outputWidth, line[sending_line], LINE_COUNT);
            }
            send_line_finish();
            break;

        case SCALE_STRETCH:
            outputHeight = LCD_HEIGHT;
            outputWidth = LCD_WIDTH;
            x_ratio = (int)(((NES_FRAME_WIDTH - 1) << 16) / (outputWidth - 1));
            y_ratio = (int)(((NES_FRAME_HEIGHT - 1) << 16) / (outputHeight - 1));

            for (y = 0; y < outputHeight; y++)
            {
                int i = 0;
                int index = (i)*outputWidth;
                if (y < 9)
                {
                    for (x = 0; x < (outputWidth - 24); x++)
                    {
                        line[calc_line][index++] = getPixelNes(data, x, (y + i), NES_FRAME_WIDTH, NES_FRAME_HEIGHT, outputWidth, outputHeight, x_ratio, y_ratio);
                    }
                    for (x = (outputWidth - 24); x < outputWidth; x++)
                    {
                        temp1 = *pbat++;
                        temp2 = (temp1 << 8) + *pbat++;
                        if (temp2 == 0)
                        {
                            line[calc_line][index++] = getPixelNes(data, x, (y + i), NES_FRAME_WIDTH, NES_FRAME_HEIGHT, outputWidth, outputHeight, x_ratio, y_ratio);
                        }
                        else
                        {
                            line[calc_line][index++] = temp2;
                        }
                    }
                }
                else
                {
                    for (x = 0; x < outputWidth; x++)
                    {
                        line[calc_line][index++] = getPixelNes(data, x, (y + i), NES_FRAME_WIDTH, NES_FRAME_HEIGHT, outputWidth, outputHeight, x_ratio, y_ratio);
                    }
                }
                if (sending_line != -1)
                    send_line_finish();
                sending_line = calc_line;
                calc_line = (calc_line == 1) ? 0 : 1;
                send_lines_ext(y, 0, outputWidth, line[sending_line], 1);
            }
            send_line_finish();
            break;

        default:
            outputHeight = LCD_HEIGHT;
            outputWidth = 220;
            xpos = (LCD_WIDTH - outputWidth) / 2;
            x_ratio = (int)(((NES_FRAME_WIDTH - 1) << 16) / (outputWidth - 1));
            y_ratio = (int)(((NES_FRAME_HEIGHT - 1) << 16) / (outputHeight - 1));

            for (y = 0; y < outputHeight; y += LINE_COUNT)
            {
                for (int i = 0; i < LINE_COUNT; i = i + 1)
                {
                    if ((y + i) >= outputHeight)
                        break;

                    int index = (i)*outputWidth;

                    if (y < 9)
                    {
                        for (x = 0; x < (outputWidth - 24); x++)
                        {
                            line[calc_line][index++] = getPixelNes(data, x, (y + i), NES_FRAME_WIDTH, NES_FRAME_HEIGHT, outputWidth, outputHeight, x_ratio, y_ratio);
                        }
                        for (x = (outputWidth - 24); x < outputWidth; x++)
                        {
                            temp1 = *pbat++;
                            temp2 = (temp1 << 8) + *pbat++; //temp2=temp1+((*pbat++) << 8);
                            if (temp2 == 0)
                            {
                                line[calc_line][index++] = getPixelNes(data, x, (y + i), NES_FRAME_WIDTH, NES_FRAME_HEIGHT, outputWidth, outputHeight, x_ratio, y_ratio);
                            }
                            else
                            {
                                line[calc_line][index++] = temp2;
                            }
                        }
                    }
                    else
                    {
                        for (x = 0; x < outputWidth; x++)
                        {
                            line[calc_line][index++] = getPixelNes(data, x, (y + i), NES_FRAME_WIDTH, NES_FRAME_HEIGHT, outputWidth, outputHeight, x_ratio, y_ratio);
                        }
                    }
                }
                if (sending_line != -1)
                    send_line_finish();
                sending_line = calc_line;
                calc_line = (calc_line == 1) ? 0 : 1;
                send_lines_ext(y, xpos, outputWidth, line[sending_line], LINE_COUNT);
            }
            send_line_finish();
            break;
        }
    }
}