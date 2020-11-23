#include "power.h"
#include "sdcard.h"
#include "esplay-ui.h"
#include "ugui.h"

static const char gfxTile[]={
#include "gfxTile.inc"
};

#define GFX_O_EMPTY 0
#define GFX_O_FULL 1
#define GFX_O_CHG 2
#define GFX_O_CHGNEARFULL 3

const char * chargeinfo = "Hold A Button to Play";

void renderGraphics(int dx, int dy, int sx, int sy, int sw, int sh)
{
    uint16_t *fb = ui_get_fb();
    uint16_t *gfx = (uint16_t*)gfxTile;
    int x,y,i;
    if (dx < 0)
    {
        sx -= dx;
        sw += dx;
        dx = 0;
    }
    if ((dx + sw) > 240)
    {
        sw -= ((dx + sw) - 240);
        dx = 240 - sw;
    }
    if (dy < 0)
    {
        sy -= dy;
        sh += dy;
        dy = 0;
    }
    if ((dy + sh) > 192)
    {
        sh -= ((dy + sh) - 192);
        dy = 192 - sh;
    }
    for (y=0; y<sh; y++) {
		for (x=0; x<sw; x++) {
			i=gfx[(sy+y)*320+(sx+x)];
			fb[(dy+y)*240+(dx+x)]=i;
		}
	}
}

void guiCharging(int almostFull) {
	ui_clear_screen();
    UG_FontSelect(&FONT_6X8);
    UG_PutString((240 - strlen(chargeinfo) * 7)/2, ((192 - 11)/2)+15, chargeinfo);
	if (!almostFull) {
		renderGraphics((240-41)/2, (192 - 11)/2, 208, 439 + (11*GFX_O_CHG), 41, 11);
	} else {
		renderGraphics((240-41)/2, (192 - 11)/2, 208, 439 + (11*GFX_O_CHGNEARFULL), 41, 11);
	}
	ui_flush();
}

void guiFull() {
	ui_clear_screen();
    UG_FontSelect(&FONT_6X8);
    UG_PutString((240 - strlen(chargeinfo) * 7)/2, ((192 - 11)/2)+15, chargeinfo);
	renderGraphics((240-41)/2, (192 - 11)/2, 208, 439 + (11*GFX_O_FULL), 41, 11);
	ui_flush();
}

void guiBatEmpty() {
	ui_clear_screen();
    UG_FontSelect(&FONT_6X8);
    UG_PutString((240 - strlen(chargeinfo) * 7)/2, ((192 - 11)/2)+15, chargeinfo);
	renderGraphics((240-41)/2, (192 - 11)/2, 208, 439 + (11*GFX_O_EMPTY), 41, 11);
	ui_flush();
}

void drawBattery(int batPercent)
{
//    charging_state st = getChargeStatus();
//    if (st == CHARGING)
//        renderGraphics(240 - 25, 0, 24 * 5, 0, 24, 24);
//    if (st == FULL_CHARGED)
//        renderGraphics(240 - 25, 0, 24 * 6, 0, 24, 24);
//    if ((st == NO_CHRG) | (st == FULL_CHARGED))
//    {
        if (batPercent > 100)
            renderGraphics(240 - 25, 0, 24 * 5, 0, 24, 24);  
        if (batPercent > 75 && batPercent <= 100)
            renderGraphics(240 - 25, 0, 24 * 4, 0, 24, 24);
        else if (batPercent > 50 && batPercent <= 75)
            renderGraphics(240 - 25, 0, 24 * 3, 0, 24, 24);
        else if (batPercent > 25 && batPercent <= 50)
            renderGraphics(240 - 25, 0, 24 * 2, 0, 24, 24);
        else if (batPercent > 0 && batPercent <= 25)
            renderGraphics(240 - 25, 0, 24 * 1, 0, 24, 24);
        else if (batPercent == 0)
            renderGraphics(240 - 25, 0, 0, 0, 24, 24);
//    }
}

void drawVolume(int volume)
{
    if (volume == 0)
        renderGraphics(0, 0, 24 * 9, 0, 24, 24);
    else
        renderGraphics(0, 0, 24 * 8, 0, 24, 24);
}