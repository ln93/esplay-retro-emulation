#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "display_gb.h"
#include "display.h"
#include "disp_spi.h"

extern uint16_t *line[];
extern int32_t scaleAlg;
extern uint8_t batlevel;

#define LINE_COUNT (8)
#define AVERAGE(a, b) (((((a) ^ (b)) & 0xf7deU) >> 1) + ((a) & (b)))

static uint16_t getPixel(const uint16_t *bufs, int x, int y, int w1, int h1, int w2, int h2, int x_ratio, int y_ratio)
{
    uint16_t col;
    switch (scaleAlg)
    {
    case NEAREST_NEIGHBOR:
    {
        /* Resize using nearest neighbor alghorithm */
        /* Simple and fastest way but low quality   */
        int x2 = ((x * x_ratio) >> 16);
        int y2 = ((y * y_ratio) >> 16);
        col = bufs[(y2 * w1) + x2];
        return col;
    }
    break;
    case BILINIER_INTERPOLATION:
    {
        /* Resize using bilinear interpolation */
        /* higher quality but lower performance, */
        int x_diff, y_diff, xv, yv, red, green, blue, a, b, c, d, index;

        xv = (int)((x_ratio * x) >> 16);
        yv = (int)((y_ratio * y) >> 16);

        x_diff = (x_ratio * x) - (xv << 16);
        y_diff = (y_ratio * y) - (yv << 16);

        index = yv * w1 + xv;

        a = bufs[index];
        b = bufs[index + 1];
        c = bufs[index + w1];
        d = bufs[index + w1 + 1];

        red = (((a >> 11) & 0x1f) * (16 - (x_diff >> 12)) * (16 - (y_diff >> 12)) + ((b >> 11) & 0x1f) * (x_diff >> 12) * (16 - (y_diff >> 12)) +
               ((c >> 11) & 0x1f) * (y_diff >> 12) * (16 - (x_diff >> 12)) + ((d >> 11) & 0x1f) * ((x_diff >> 12) * (y_diff >> 12)));
        red = red >> 8;

        green = (((a >> 5) & 0x3f) * (16 - (x_diff >> 12)) * (16 - (y_diff >> 12)) + ((b >> 5) & 0x3f) * (x_diff >> 12) * (16 - (y_diff >> 12)) +
                 ((c >> 5) & 0x3f) * (y_diff >> 12) * (16 - (x_diff >> 12)) + ((d >> 5) & 0x3f) * ((x_diff >> 12) * (y_diff >> 12)));
        green = green >> 8;

        blue = (((a)&0x1f) * (16 - (x_diff >> 12)) * (16 - (y_diff >> 12)) + ((b)&0x1f) * (x_diff >> 12) * (16 - (y_diff >> 12)) +
                ((c)&0x1f) * (y_diff >> 12) * (16 - (x_diff >> 12)) + ((d)&0x1f) * ((x_diff >> 12) * (y_diff >> 12)));
        blue = blue >> 8;

        col = ((int)red << 11) | ((int)green << 5) | ((int)blue);

        return col;
    }
    break;

    default: //case BOX_FILTERED:
        // experimental, currently disabled
        {
            int xv, yv, a, b, c, d, index, p, q;

            xv = (int)((x_ratio * x) >> 16);
            yv = (int)((y_ratio * y) >> 16);

            index = yv * w1 + xv;

            a = bufs[index];
            b = bufs[index + 1];
            c = bufs[index + w1];
            d = bufs[index + w1 + 1];

            p = AVERAGE(a, b);
            q = AVERAGE(c, d);

            col = AVERAGE(p, q);

            return col;
        }
    }
}

void write_gb_frame(const uint16_t *data, esplay_scale_option scale)
{
    short x, y, xpos, ypos, outputWidth, outputHeight;
    int sending_line = -1;
    int calc_line = 0;
    int x_ratio, y_ratio;
    static uint8_t skip = 0;
    int temp1, temp2;
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
        for (y = 0; y < LCD_HEIGHT; ++y)
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
            ypos = (LCD_HEIGHT - GB_FRAME_HEIGHT) / 2;
            xpos = (LCD_WIDTH - GB_FRAME_WIDTH) / 2;

            for (y = 0; y < GB_FRAME_HEIGHT; y += LINE_COUNT)
            {
                for (int i = 0; i < LINE_COUNT; ++i)
                {
                    if ((y + i) >= GB_FRAME_HEIGHT)
                        break;

                    int index = (i)*GB_FRAME_WIDTH;
                    int bufferIndex = ((y + i) * GB_FRAME_WIDTH);

                    for (x = 0; x < GB_FRAME_WIDTH; ++x)
                    {
                        uint16_t sample = data[bufferIndex++];
                        line[calc_line][index++] = ((sample >> 8) | ((sample & 0xff) << 8));
                    }
                }
                if (sending_line != -1)
                    send_line_finish();
                sending_line = calc_line;
                calc_line = (calc_line == 1) ? 0 : 1;
                send_lines_ext(y + ypos, xpos, GB_FRAME_WIDTH, line[sending_line], LINE_COUNT);
            }

            send_line_finish();
            uint16_t datpix[24 * 16];
            for (x = 0; x < 24 * 16; x++)
            {
                temp1 = *pbat++;
                temp2 = (temp1 << 8) + *pbat++;
                datpix[x] = temp2;
            }
            send_lines_ext(0, 239 - 24, 24, datpix, 16);
            send_line_finish();
            break;

        case SCALE_STRETCH:
            outputHeight = LCD_HEIGHT;
            outputWidth = LCD_WIDTH;
            x_ratio = (int)((GB_FRAME_WIDTH << 16) / outputWidth) + 1;
            y_ratio = (int)((GB_FRAME_HEIGHT << 16) / outputHeight) + 1;

            for (y = 0; y < outputHeight; y += LINE_COUNT)
            {
                for (int i = 0; i < LINE_COUNT; ++i)
                {
                    if ((y + i) >= outputHeight)
                        break;

                    int index = (i)*outputWidth;

                    if (y < 9)
                    {
                        for (x = 0; x < (outputWidth - 24); ++x)
                        {
                            uint16_t sample = getPixel(data, x, (y + i), GB_FRAME_WIDTH, GB_FRAME_HEIGHT, outputWidth, outputHeight, x_ratio, y_ratio);
                            line[calc_line][index++] = ((sample >> 8) | ((sample) << 8));
                        }
                        for (x = (outputWidth - 24); x < outputWidth; ++x)
                        {
                            temp1 = *pbat++;
                            temp2 = (temp1 << 8) + *pbat++;
                            if (temp2 == 0)
                            {
                                uint16_t sample = getPixel(data, x, (y + i), GB_FRAME_WIDTH, GB_FRAME_HEIGHT, outputWidth, outputHeight, x_ratio, y_ratio);
                                line[calc_line][index++] = ((sample >> 8) | ((sample) << 8));
                            }
                            else
                            {
                                line[calc_line][index++] = temp2;
                            }
                        }
                    }
                    else
                    {
                        for (x = 0; x < outputWidth; ++x)
                        {
                            uint16_t sample = getPixel(data, x, (y + i), GB_FRAME_WIDTH, GB_FRAME_HEIGHT, outputWidth, outputHeight, x_ratio, y_ratio);
                            line[calc_line][index++] = ((sample >> 8) | ((sample) << 8));
                        }
                    }
                }
                if (sending_line != -1)
                    send_line_finish();
                sending_line = calc_line;
                calc_line = (calc_line == 1) ? 0 : 1;
                send_lines_ext(y, 0, outputWidth, line[sending_line], LINE_COUNT);
            }
            send_line_finish();
            break;

        default:
            outputHeight = LCD_HEIGHT;
            outputWidth = GB_FRAME_WIDTH * LCD_HEIGHT / GB_FRAME_HEIGHT; //GB_FRAME_WIDTH + (LCD_HEIGHT - GB_FRAME_HEIGHT);
            xpos = (LCD_WIDTH - outputWidth) / 2;
            x_ratio = (int)((GB_FRAME_WIDTH << 16) / outputWidth) + 1;
            y_ratio = (int)((GB_FRAME_HEIGHT << 16) / outputHeight) + 1;

            for (y = 0; y < outputHeight; y += LINE_COUNT)
            {
                for (int i = 0; i < LINE_COUNT; ++i)
                {
                    if ((y + i) >= outputHeight)
                        break;

                    int index = (i)*outputWidth;

                    if (y < 9)
                    {
                        for (x = 0; x < (outputWidth - 24); ++x)
                        {
                            uint16_t sample = getPixel(data, x, (y + i), GB_FRAME_WIDTH, GB_FRAME_HEIGHT, outputWidth, outputHeight, x_ratio, y_ratio);
                            line[calc_line][index++] = ((sample >> 8) | ((sample) << 8));
                        }
                        for (x = (outputWidth - 24); x < outputWidth; ++x)
                        {
                            temp1 = *pbat++;
                            temp2 = (temp1 << 8) + *pbat++;
                            if (temp2 == 0)
                            {
                                uint16_t sample = getPixel(data, x, (y + i), GB_FRAME_WIDTH, GB_FRAME_HEIGHT, outputWidth, outputHeight, x_ratio, y_ratio);
                                line[calc_line][index++] = ((sample >> 8) | ((sample) << 8));
                            }
                            else
                            {

                                line[calc_line][index++] = temp2;
                            }
                        }
                    }
                    else
                    {
                        for (x = 0; x < outputWidth; ++x)
                        {
                            uint16_t sample = getPixel(data, x, (y + i), GB_FRAME_WIDTH, GB_FRAME_HEIGHT, outputWidth, outputHeight, x_ratio, y_ratio);
                            line[calc_line][index++] = ((sample >> 8) | ((sample) << 8));
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
