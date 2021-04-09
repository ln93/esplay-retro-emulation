#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "display_txt.h"
#include "display.h"
#include "disp_spi.h"
#include "ugui/ugui.h"

extern uint16_t *line[];
extern int32_t scaleAlg;
extern uint8_t batlevel;

#define LINE_COUNT (8)
#define AVERAGE(a, b) (((((a) ^ (b)) & 0xf7deU) >> 1) + ((a) & (b)))

long write_txt_frame(FILE *txt, long PTR)
{
    printf("Display File. PTR:%ld\n", PTR);
    short x, y, xpos, ypos, outputWidth, outputHeight;
    int sending_line = -1;
    int calc_line = 0;
    if (!txt)
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
        return 0;
    }
    else
    {
        int dPTR = 0;
        char data[256];
        fseek(txt, PTR, SEEK_SET);
        printf("Try Open TXT. PTR:%ld\n", PTR);
        fread(data, 256, 1, txt);
        printf("Open TXT Success. PTR:%ld\n", PTR);
        for (int lineCount = 0; lineCount < 8; lineCount++)
        {
            char line[32];
            int singleLength;
            for (singleLength = 0; singleLength < 25;)
            {
                char chr = data[dPTR];
                if (chr >= 0x80)
                {
                    line[singleLength++] = data[dPTR++];
                    line[singleLength++] = data[dPTR++];
                }
                else
                {
                    line[singleLength++] = data[dPTR++];
                }
            }
            printf("Print Line:%d\n", lineCount);
            UG_SetForecolor(C_WHITE);
            UG_SetBackcolor(C_BLACK);
            UG_PutString(10, 10 + lineCount * 22, line);
        }
        return (dPTR);
    }
}
