#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "display_sms.h"
#include "display.h"
#include "disp_spi.h"

extern uint16_t *line[];
extern int32_t scaleAlg;
extern uint8_t batlevel;

#define LINE_COUNT (8)
#define AVERAGE(a, b) (((((a) ^ (b)) & 0xf7deU) >> 1) + ((a) & (b)))

static uint16_t getPixelSms(const uint8_t *bufs, int x, int y, int w1, int h1, int w2, int h2, bool isGameGear, int x_ratio, int y_ratio, uint16_t color[])
{
    uint16_t col;
    if (scaleAlg == NEAREST_NEIGHBOR)
    {
        /* Resize using nearest neighbor alghorithm */
        /* Simple and fastest way but low quality   */
        int x2 = ((x * x_ratio) >> 16);
        int y2 = ((y * y_ratio) >> 16);
        if (isGameGear)
            col = color[bufs[y2 * 256 + x2 + 48] & 0x1f];
        else
            col = color[bufs[(y2 * w1) + x2] & 0x1f];

        return col;
    }
    else if (scaleAlg == BILINIER_INTERPOLATION)
    {
        /* Resize using bilinear interpolation */
        /* higher quality but lower performance, */
        int x_diff, y_diff, xv, yv, red, green, blue, col, a, b, c, d, index;

        xv = (int)((x_ratio * x) >> 16);
        yv = (int)((y_ratio * y) >> 16);
        x_diff = (x_ratio * x) - (xv << 16);
        y_diff = (y_ratio * y) - (yv << 16);

        if (isGameGear)
        {
            index = yv * 256 + xv + 48;
            a = color[bufs[index] & 0x1f];
            b = color[bufs[index + 1] & 0x1f];
            c = color[bufs[index + 256] & 0x1f];
            d = color[bufs[index + 256 + 1] & 0x1f];
        }
        else
        {
            index = yv * w1 + xv;
            a = color[bufs[index] & 0x1f];
            b = color[bufs[index + 1] & 0x1f];
            c = color[bufs[index + w1] & 0x1f];
            d = color[bufs[index + w1 + 1] & 0x1f];
        }

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
    else /* scaleAlg == BOX_FILTERED */
    {
        // experimental, currently disabled
        int xv, yv, col, a, b, c, d, index, p, q;

        xv = (int)((x_ratio * x) >> 16);
        yv = (int)((y_ratio * y) >> 16);

        if (isGameGear)
        {
            index = yv * 256 + xv + 48;
        }
        else
        {
            index = yv * w1 + xv;
        }

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

void write_sms_frame(const uint8_t *data, uint16_t color[], bool isGameGear, esplay_scale_option scale)
{
    short x, y, xpos, ypos, outputWidth, outputHeight;
    int sending_line = -1;
    int calc_line = 0;
    int x_ratio, y_ratio;
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
    static uint8_t skip = 0;
    if (skip)
    {
        skip = 0;
        return;
    }
    else
    {
        skip = 1;
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
        if (!isGameGear)
        {
            switch (scale)
            {
            case SCALE_NONE:
                outputHeight = LCD_HEIGHT;
                outputWidth = LCD_WIDTH;
                x_ratio = (int)((SMS_FRAME_WIDTH << 16) / outputWidth) + 1;
                y_ratio = (int)((SMS_FRAME_HEIGHT << 16) / outputHeight) + 1;

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
                                uint16_t sample = getPixelSms(data, x, (y + i), SMS_FRAME_WIDTH, SMS_FRAME_HEIGHT, outputWidth, outputHeight, isGameGear, x_ratio, y_ratio, color);
                                line[calc_line][index++] = ((sample >> 8) | ((sample) << 8));
                            }
                            for (x = (outputWidth - 24); x < outputWidth; ++x)
                            {
                                temp1 = *pbat++;
                                temp2 = (temp1 << 8) + *pbat++;
                                if (temp2 == 0)
                                {
                                    uint16_t sample = getPixelSms(data, x, (y + i), SMS_FRAME_WIDTH, SMS_FRAME_HEIGHT, outputWidth, outputHeight, isGameGear, x_ratio, y_ratio, color);
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
                                uint16_t sample = getPixelSms(data, x, (y + i), SMS_FRAME_WIDTH, SMS_FRAME_HEIGHT, outputWidth, outputHeight, isGameGear, x_ratio, y_ratio, color);
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
            case SCALE_STRETCH:

                outputHeight = LCD_HEIGHT;
                outputWidth = LCD_WIDTH;
                x_ratio = (int)((SMS_FRAME_WIDTH << 16) / outputWidth) + 1;
                y_ratio = (int)((SMS_FRAME_HEIGHT << 16) / outputHeight) + 1;

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
                                uint16_t sample = getPixelSms(data, x, (y + i), SMS_FRAME_WIDTH, SMS_FRAME_HEIGHT, outputWidth, outputHeight, isGameGear, x_ratio, y_ratio, color);
                                line[calc_line][index++] = ((sample >> 8) | ((sample) << 8));
                            }
                            for (x = (outputWidth - 24); x < outputWidth; ++x)
                            {
                                temp1 = *pbat++;
                                temp2 = (temp1 << 8) + *pbat++;
                                if (temp2 == 0)
                                {
                                    uint16_t sample = getPixelSms(data, x, (y + i), SMS_FRAME_WIDTH, SMS_FRAME_HEIGHT, outputWidth, outputHeight, isGameGear, x_ratio, y_ratio, color);
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
                                uint16_t sample = getPixelSms(data, x, (y + i), SMS_FRAME_WIDTH, SMS_FRAME_HEIGHT, outputWidth, outputHeight, isGameGear, x_ratio, y_ratio, color);
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
                outputWidth = LCD_WIDTH; //SMS_FRAME_WIDTH + (LCD_HEIGHT - SMS_FRAME_HEIGHT);
                xpos = (LCD_WIDTH - outputWidth) / 2;
                x_ratio = (int)((SMS_FRAME_WIDTH << 16) / outputWidth) + 1;
                y_ratio = (int)((SMS_FRAME_HEIGHT << 16) / outputHeight) + 1;

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
                                uint16_t sample = getPixelSms(data, x, (y + i), SMS_FRAME_WIDTH, SMS_FRAME_HEIGHT, outputWidth, outputHeight, isGameGear, x_ratio, y_ratio, color);
                                line[calc_line][index++] = ((sample >> 8) | ((sample) << 8));
                            }
                            for (x = (outputWidth - 24); x < outputWidth; ++x)
                            {
                                temp1 = *pbat++;
                                temp2 = (temp1 << 8) + *pbat++;
                                if (temp2 == 0)
                                {
                                    uint16_t sample = getPixelSms(data, x, (y + i), SMS_FRAME_WIDTH, SMS_FRAME_HEIGHT, outputWidth, outputHeight, isGameGear, x_ratio, y_ratio, color);
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
                                uint16_t sample = getPixelSms(data, x, (y + i), SMS_FRAME_WIDTH, SMS_FRAME_HEIGHT, outputWidth, outputHeight, isGameGear, x_ratio, y_ratio, color);
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
        else
        {
            switch (scale)
            {
            case SCALE_NONE:
                outputHeight = GAMEGEAR_FRAME_HEIGHT;
                outputWidth = GAMEGEAR_FRAME_WIDTH;
                xpos = (LCD_WIDTH - outputWidth) / 2;
                ypos = (LCD_HEIGHT - outputHeight) / 2;

                for (y = 0; y < outputHeight; y += LINE_COUNT)
                {
                    for (int i = 0; i < LINE_COUNT; ++i)
                    {
                        if ((y + i) >= outputHeight)
                            break;

                        int index = (i)*outputWidth;
                        int bufferIndex = ((y + i) * 256) + 48;

                        for (x = 0; x < outputWidth; ++x)
                        {
                            uint16_t sample = color[data[bufferIndex++] & PIXEL_MASK];
                            line[calc_line][index++] = ((sample >> 8) | ((sample) << 8));
                        }
                    }
                    if (sending_line != -1)
                        send_line_finish();
                    sending_line = calc_line;
                    calc_line = (calc_line == 1) ? 0 : 1;
                    send_lines_ext(ypos + y, xpos, outputWidth, line[sending_line], LINE_COUNT);
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
                x_ratio = (int)((GAMEGEAR_FRAME_WIDTH << 16) / outputWidth) + 1;
                y_ratio = (int)((GAMEGEAR_FRAME_HEIGHT << 16) / outputHeight) + 1;

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
                                uint16_t sample = getPixelSms(data, x, (y + i), GAMEGEAR_FRAME_WIDTH, GAMEGEAR_FRAME_HEIGHT, outputWidth, outputHeight, isGameGear, x_ratio, y_ratio, color);
                                line[calc_line][index++] = ((sample >> 8) | ((sample) << 8));
                            }
                            for (x = (outputWidth - 24); x < outputWidth; ++x)
                            {
                                temp1 = *pbat++;
                                temp2 = (temp1 << 8) + *pbat++;
                                if (temp2 == 0)
                                {
                                    uint16_t sample = getPixelSms(data, x, (y + i), GAMEGEAR_FRAME_WIDTH, GAMEGEAR_FRAME_HEIGHT, outputWidth, outputHeight, isGameGear, x_ratio, y_ratio, color);
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
                                uint16_t sample = getPixelSms(data, x, (y + i), GAMEGEAR_FRAME_WIDTH, GAMEGEAR_FRAME_HEIGHT, outputWidth, outputHeight, isGameGear, x_ratio, y_ratio, color);
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
                outputWidth = GAMEGEAR_FRAME_WIDTH * LCD_HEIGHT / GAMEGEAR_FRAME_HEIGHT; // GAMEGEAR_FRAME_WIDTH    GAMEGEAR_FRAME_WIDTH + (LCD_HEIGHT - GAMEGEAR_FRAME_HEIGHT);
                xpos = (LCD_WIDTH - outputWidth) / 2;
                x_ratio = (int)((GAMEGEAR_FRAME_WIDTH << 16) / outputWidth) + 1;
                y_ratio = (int)((GAMEGEAR_FRAME_HEIGHT << 16) / outputHeight) + 1;

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
                                uint16_t sample = getPixelSms(data, x, (y + i), GAMEGEAR_FRAME_WIDTH, GAMEGEAR_FRAME_HEIGHT, outputWidth, outputHeight, isGameGear, x_ratio, y_ratio, color);
                                line[calc_line][index++] = ((sample >> 8) | ((sample) << 8));
                            }
                            for (x = (outputWidth - 24); x < outputWidth; ++x)
                            {
                                temp1 = *pbat++;
                                temp2 = (temp1 << 8) + *pbat++;
                                if (temp2 == 0)
                                {
                                    uint16_t sample = getPixelSms(data, x, (y + i), GAMEGEAR_FRAME_WIDTH, GAMEGEAR_FRAME_HEIGHT, outputWidth, outputHeight, isGameGear, x_ratio, y_ratio, color);
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
                                uint16_t sample = getPixelSms(data, x, (y + i), GAMEGEAR_FRAME_WIDTH, GAMEGEAR_FRAME_HEIGHT, outputWidth, outputHeight, isGameGear, x_ratio, y_ratio, color);
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
}