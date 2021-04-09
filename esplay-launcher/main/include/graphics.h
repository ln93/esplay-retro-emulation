#pragma once

void renderGraphics(int dx, int dy, int sx, int sy, int sw, int sh);
void renderGraphicsAdd(int dx, int dy, int sx, int sy, int sw, int sh);
void drawBattery(int batPercent);
void drawVolume(int volume);
void drawWave(int x, int y, int waveSize, char *waveValue);
void guiCharging(int almostFull);
void guiFull();
void guiBatEmpty();
ushort ui_get_title_color();
ushort ui_get_background_color();
ushort ui_get_word_color();
