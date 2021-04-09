// Esplay Launcher - launcher for ESPLAY based on Gogo Launcher for Odroid Go.
#include "limits.h" /* PATH_MAX */
#include "freertos/FreeRTOS.h"
#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_event_loop.h"
#include "driver/gpio.h"
#include "esp_partition.h"
#include "esp_ota_ops.h"
#include "esp_heap_caps.h"
#include "esp_http_server.h"
#include "soc/dport_reg.h"

#include "rom/rtc.h"
#include "soc/soc.h"
#include "soc/rtc.h"
#include "soc/rtc_cntl_reg.h"

#include "stdio.h"
#include "settings.h"
#include "gamepad.h"
#include "event.h"
#include "display.h"
#include "audio.h"
#include "power.h"
#include "sdcard.h"
#include "esplay-ui.h"
#include "ugui.h"
#include "graphics.h"
#include "audio_player.h"
#include "file_ops.h"

#define SCROLLSPD 64
#define NUM_EMULATOR 8
#define AUDIO_FILE_PATH "/sd/audio"

char emu_dir[7][10] = {"nes", "gb", "gbc", "sms", "gg", "col", "txt"};
char emu_name[7][20] = {"Nintendo", "Gameboy", "Gameboy Color", "Sega Master System", "Game Gear", "Coleco Vision", "TXT Reader"};
int emu_slot[7] = {1, 2, 2, 3, 3, 3, 4};
char *base_path = "/sd/roms/";
battery_state bat_state;
int num_menu = 6;
char menu_text[6][20] = {"WiFi热点", "音量", "亮度", "缩放", "缩放算法", "退出"};
char scaling_text[3][20] = {"原生点对点", "等比例缩放", "拉伸至全屏"};
char scaling_alg_text[3][20] = {"最近邻插值", "双线性插值", "Box Filtered"};
int32_t wifi_en = 0;
int32_t volume = 25;
int32_t bright = 50;
int32_t scaling = SCALE_STRETCH;
int32_t scale_alg = BILINIER_INTERPOLATION;
extern uint16_t freq;
extern const unsigned char fontzn[261696];
esp_err_t start_file_server(const char *base_path);
long write_txt_frame(FILE *txt, long PTR, char *FILENAME)
{
    short x, y, xpos, ypos, outputWidth, outputHeight;
    int sending_line = -1;
    int calc_line = 0;
    ui_clear_screen();
    UG_FillFrame(0, 0, 240, 26, ui_get_title_color());
    UG_FillFrame(0, 27, 240, 192, ui_get_background_color());
    UG_SetForecolor(ui_get_word_color());
    UG_SetBackcolor(ui_get_title_color());
    UG_PutSingleString(10, 8, FILENAME + 13);
    if (!txt)
    {
        return 0;
    }
    else
    {
        int dPTR = 0;
        char data[256];
        memset(data, '\n', 256);
        fseek(txt, PTR, SEEK_SET);
        printf("Try Open TXT. PTR:%ld\n", PTR);
        int FetchDataLen = fread(data, 1, 256, txt);
        printf("Open TXT Success. Length:%d\n", FetchDataLen);
        char *ScreenLine;
        for (int lineCount = 0; lineCount < 9; lineCount++)
        {

            char line[32];
            memset(line, '\n', 32);
            int singleLength;
            for (singleLength = 0; singleLength < 25;)
            {
                if (dPTR >= FetchDataLen)
                    break;
                char chr = data[dPTR];
                if (chr >= 0x80)
                {
                    line[singleLength++] = data[dPTR++];
                    line[singleLength++] = data[dPTR++];
                }
                else if (chr == '\n')
                {
                    line[singleLength++] = data[dPTR++];
                    break;
                }
                else
                {
                    line[singleLength++] = data[dPTR++];
                }
            }
            printf("Print Line:%d:%d\n", lineCount, singleLength);
            line[singleLength] = '\n';
            ScreenLine = malloc(singleLength + 1);
            memset(ScreenLine, '\n', singleLength + 1);
            for (int p = 0; p < singleLength + 1; p++)
            {
                ScreenLine[p] = line[p];
            }
            UG_SetForecolor(ui_get_word_color());
            UG_SetBackcolor(ui_get_background_color());
            UG_PutSingleString(10, 27 + lineCount * 18, ScreenLine);
            free(ScreenLine);
        }
        ui_flush();
        battery_level_read(&bat_state);
        drawBattery(bat_state.percentage);
        return (dPTR);
    }
}

esp_err_t event_handler(void *ctx, system_event_t *event)
{
    return ESP_OK;
}

static void handleCharging()
{
    int r;
    int fullCtr = 0;
    //The LiIon charger sometimes goes back from 'full' to 'charging', which is
    //confusing to the end user. This variable becomes true if the LiIon has indicated 'full'
    //for a while, and it being true causes the 'full' icon to always show.
    int fixFull = 0;

    //Force brightness low to decrease chance of burn-in
    set_display_brightness(30);
    printf("Detected charger.\n");
    /*
	guiCharging(0);

	rtc_clk_cpu_freq_set(RTC_CPU_FREQ_2M);
    input_gamepad_state prevKey;
    gamepad_read(&prevKey);
	do {
        input_gamepad_state key;
        gamepad_read(&key);
		r=getChargeStatus();
		if (r==FULL_CHARGED || fixFull) {
			guiFull();
			printf("Full!\n");
			fullCtr++;
		} else if (r==CHARGING) {
            battery_level_read(&bat_state);
			guiCharging(bat_state.millivolts > 4100);
			printf("Charging...\n");
			fullCtr=0;
		}

		if (!prevKey.values[GAMEPAD_INPUT_A] && key.values[GAMEPAD_INPUT_A]) {
			printf("Power btn A; go to launcher menu\n");
			break;
		}
		if (fullCtr==32) {
			fixFull=1;
		}
		vTaskDelay(1);
        prevKey = key;
	} while (r!=NO_CHRG);
    */
    rtc_clk_cpu_freq_set(RTC_CPU_FREQ_240M);
}

static void drawHomeScreen()
{
    ui_clear_screen();
    UG_FillFrame(0, 0, 239, 191, ui_get_title_color());
    UG_SetForecolor(ui_get_word_color());
    UG_SetBackcolor(ui_get_title_color());
    char *title = "ESP32 Mini";
    if (wifi_en)
    {
        title = "192.168.4.1";
    }
    UG_PutString((240 / 2) - (strlen(title) * 9 / 2), 12, title);
    uint8_t volume = 25;
    settings_load(SettingAudioVolume, &volume);
    char volStr[3];
    sprintf(volStr, "%i", volume);
    if (volume == 0)
    {
        UG_SetForecolor(ui_get_word_color());
        UG_SetBackcolor(ui_get_title_color());
    }
    else
    {
        UG_SetForecolor(ui_get_word_color());
        UG_SetBackcolor(ui_get_title_color());
    }
    UG_PutString(30, 8, volStr);
    battery_level_read(&bat_state);
    drawVolume(volume);
    drawBattery(bat_state.percentage);
    if (wifi_en)
    {
        renderGraphics(240 - (55), 3, 24 * 7, 0, 24, 24);
    }
}

// Return to last emulator if 'B' pressed....
static int resume(void)
{
    int i;
    char *extension;
    char *romPath = settings_load_str(SettingRomPath);
    return (0); // nope!
}

static void showOptionPage(int selected)
{
    ui_clear_screen();
    /* Header */
    UG_FillFrame(0, 0, 240 - 1, 16 - 1, ui_get_title_color());
    UG_SetForecolor(ui_get_word_color());
    UG_SetBackcolor(ui_get_title_color());
    char *msg = "Device Options";
    //UG_PutString((240 / 2) - (strlen(msg) * 9 / 2), 2, msg);
    /* End Header */

    /* Footer */
    UG_FillFrame(0, 192 - 16 - 1, 240 - 1, 192 - 1, ui_get_title_color());
    UG_SetForecolor(ui_get_word_color());
    UG_SetBackcolor(ui_get_title_color());
    msg = "     Browse      Change       ";
    //UG_PutString((240 / 2) - (strlen(msg) * 9 / 2), 192 - 15, msg);

    UG_FillRoundFrame(15, 192 - 15 - 1, 15 + (5 * 9) + 8, 237, 7, ui_get_title_color());
    UG_SetForecolor(ui_get_word_color());
    UG_SetBackcolor(ui_get_title_color());
    //UG_PutString(20, 192 - 15, "Up/Dn");

    UG_FillRoundFrame(140, 192 - 15 - 1, 140 + (3 * 9) + 8, 237, 7, ui_get_title_color());
    //UG_PutString(145, 192 - 15, "< >");
    /* End Footer */

    UG_FillFrame(0, 16, 240 - 1, 192 - 20, ui_get_background_color());

    UG_SetForecolor(C_RED);
    UG_SetBackcolor(ui_get_background_color());
    //UG_PutString(0, 192 - 30, "* restart required");
    esp_app_desc_t *desc = esp_ota_get_app_description();
    char idfVer[512];
    sprintf(idfVer, "IDF %s", desc->idf_ver);
    UG_SetForecolor(ui_get_word_color());
    UG_SetBackcolor(ui_get_background_color());
    //UG_PutString(0, 240 - 72, desc->project_name);
    //UG_PutString(0, 240 - 58, desc->version);
    //UG_PutString(0, 240 - 44, idfVer);

    for (int i = 0; i < num_menu; i++)
    {
        short top = 18 + i * 15 + 8;
        if (i == selected)
            UG_SetForecolor(ui_get_title_color());
        else
            UG_SetForecolor(ui_get_word_color());

        UG_PutString(0, top, menu_text[i]);

        // show value on right side
        switch (i)
        {
        case 0:
            if (i == selected)
                ui_display_switch(192 - 13, top, wifi_en, ui_get_title_color(), ui_get_word_color(), ui_get_background_color());
            else
                ui_display_switch(192 - 13, top, wifi_en, ui_get_word_color(), ui_get_title_color(), ui_get_background_color());
            break;
        case 1:
            if (i == selected)
                ui_display_seekbar((240 - 103), top + 4, 100, (volume * 100) / 100, ui_get_title_color(), ui_get_background_color());
            else
                ui_display_seekbar((240 - 103), top + 4, 100, (volume * 100) / 100, ui_get_word_color(), ui_get_background_color());
            break;
        case 2:
            if (i == selected)
                ui_display_seekbar((240 - 103), top + 4, 100, (bright * 100) / 100, ui_get_title_color(), ui_get_background_color());
            else
                ui_display_seekbar((240 - 103), top + 4, 100, (bright * 100) / 100, ui_get_word_color(), ui_get_background_color());
            break;
        case 3:
            UG_PutString(239 - (strlen(scaling_text[scaling]) * 9), top, scaling_text[scaling]);
            break;

        case 4:
            UG_PutString(239 - (strlen(scaling_alg_text[scale_alg]) * 9), top, scaling_alg_text[scale_alg]);
            break;

        default:
            break;
        }
    }
    ui_flush();
}

static int showOption()
{
    int32_t wifi_state = wifi_en;
    int selected = 0;
    showOptionPage(selected);

    input_gamepad_state prevKey;
    gamepad_read(&prevKey);
    while (true)
    {
        input_gamepad_state key;
        gamepad_read(&key);
        if (!prevKey.values[GAMEPAD_INPUT_DOWN] && key.values[GAMEPAD_INPUT_DOWN])
        {
            ++selected;
            if (selected > num_menu - 1)
                selected = 0;
            showOptionPage(selected);
        }
        if (!prevKey.values[GAMEPAD_INPUT_UP] && key.values[GAMEPAD_INPUT_UP])
        {
            --selected;
            if (selected < 0)
                selected = num_menu - 1;
            showOptionPage(selected);
        }
        if (!prevKey.values[GAMEPAD_INPUT_LEFT] && key.values[GAMEPAD_INPUT_LEFT])
        {
            switch (selected)
            {
            case 0:
                wifi_en = !wifi_en;
                settings_save(SettingWifi, (int32_t)wifi_en);
                break;
            case 1:
                volume -= 5;
                if (volume < 0)
                    volume = 0;
                settings_save(SettingAudioVolume, (int32_t)volume);
                break;
            case 2:
                bright -= 5;
                if (bright < 1)
                    bright = 1;
                set_display_brightness(bright);
                settings_save(SettingBacklight, (int32_t)bright);
                break;
            case 3:
                scaling--;
                if (scaling < 0)
                    scaling = 2;
                settings_save(SettingScaleMode, (int32_t)scaling);
                break;
            case 4:
                scale_alg--;
                if (scale_alg < 0)
                    scale_alg = 1;
                settings_save(SettingAlg, (int32_t)scale_alg);
                break;

            default:
                break;
            }
            showOptionPage(selected);
        }
        if (!prevKey.values[GAMEPAD_INPUT_RIGHT] && key.values[GAMEPAD_INPUT_RIGHT])
        {
            switch (selected)
            {
            case 0:
                wifi_en = !wifi_en;
                settings_save(SettingWifi, (int32_t)wifi_en);
                break;
            case 1:
                volume += 5;
                if (volume > 100)
                    volume = 100;
                settings_save(SettingAudioVolume, (int32_t)volume);
                break;
            case 2:
                bright += 5;
                if (bright > 100)
                    bright = 100;
                set_display_brightness(bright);
                settings_save(SettingBacklight, (int32_t)bright);
                break;
            case 3:
                scaling++;
                if (scaling > 2)
                    scaling = 0;
                settings_save(SettingScaleMode, (int32_t)scaling);
                break;
            case 4:
                scale_alg++;
                if (scale_alg > 1)
                    scale_alg = 0;
                settings_save(SettingAlg, (int32_t)scale_alg);
                break;

            default:
                break;
            }
            showOptionPage(selected);
        }
        if (!prevKey.values[GAMEPAD_INPUT_A] && key.values[GAMEPAD_INPUT_A])
            if (selected == 5)
            {
                vTaskDelay(10);
                break;
            }

        prevKey = key;
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }

    if (wifi_en != wifi_state)
        return 1;

    return 0;
}
void slotRenderGraphics(int item, int tick, int bg_freq, int form_freq)
{
    //render background
    renderGraphics(0, 32, 0, 24 + ((tick / bg_freq) % 2) * 161, 240, 160);

    //render mouse
    int mouseNo = (tick / form_freq) % 4;
    int mouseBatch = mouseNo / 2; //used to skip the purple line
    renderGraphicsAdd((item % 4) * 60, 32 + (item / 4) * 80, 240, 24 + (mouseNo)*80 + mouseBatch, 60, 80);
}
const char *SD_BASE_PATH = "/sd";
static void SaveTxtState(long FilePTR)
{

    // Save sram
    char *romPath = settings_load_str(SettingRomPath);

    if (romPath)
    {
        char *fileName = system_util_GetFileName(romPath);
        if (!fileName)
        {
            system_application_set(0);
            esp_restart();
        }

        char *pathName = sdcard_create_savefile_path(SD_BASE_PATH, fileName);
        if (!pathName)
        {
            printf("%s: can't generate path, save failed\n", __func__);
            system_application_set(0);
            esp_restart();
        }

        FILE *f = fopen(pathName, "w");
        if (f == NULL)
        {
            printf("%s: fopen save failed\n", __func__);
            {
                system_application_set(0);
                esp_restart();
            }
        }
        else
        {
            fprintf(f, "%ld", FilePTR);
            fclose(f);

            printf("SaveState: savestate OK.\n");
        }
        fclose(f);

        printf("%s: savestate OK.\n", __func__);

        free(pathName);
        free(fileName);
        free(romPath);
    }
    else
    {
        free(romPath);
    }
}

static long LoadTxtState()
{
    long FilePTR = 0;
    char *romName = settings_load_str(SettingRomPath);
    if (romName)
    {

        char *fileName = system_util_GetFileName(romName);
        if (!fileName)
        {
            printf("LoadState: save fopen load failed, treat as a new book.\n");
            return (0);
        }

        char *pathName = sdcard_create_savefile_path(SD_BASE_PATH, fileName);
        if (!pathName)
        {
            printf("LoadState: save fopen load failed, treat as a new book.\n");
            return (0);
        }
        else
        {
            FILE *f = fopen(pathName, "r");
            if (f == NULL)
            {
                printf("LoadState: save fopen load failed, treat as a new book.\n");
            }
            else
            {
                fscanf(f, "%ld", &FilePTR);
                fclose(f);
            }
        }

        free(pathName);
        free(fileName);
        free(romName);
        return (FilePTR);
    }
    return (0);
}
//----------------------------------------------------------------
void app_main(void)
{ //char m=9;
    //   m=m+font1.height+font1.pixel_data[3]+font2.height+font2.pixel_data[3]+font3.height+font3.pixel_data[3]+font4.height+font4.pixel_data[3]+font5.height+font5.pixel_data[3];
    //   if (m==8)
    //       m=m+font1.height+font1.pixel_data[5]
    //   else
    //   {
    //       /* code */
    //   }

    //   printf("the value is %d.--kelly\n",m);

    settings_init();
    esplay_system_init();

    audio_init(44100);
    freq = 44100;
    audio_amp_disable();
    printf("\nReady, system init \n");
    gamepad_init();

    // Display
    display_prepare();
    display_init();

    int brightness = 50;
    settings_load(SettingBacklight, &brightness);
    set_display_brightness(brightness);

    battery_level_init();

    battery_level_read(&bat_state);
    if (bat_state.percentage == 0)
    {
        display_show_empty_battery();

        printf("PowerDown: Powerdown LCD panel.\n");
        display_poweroff();

        printf("PowerDown: Entering deep sleep.\n");
        system_sleep();

        // Should never reach here
        abort();
    }

    //  printf("dat is %d, %d, %d, %d, %d, %d, %d,  %d ",fontzn[261688],fontzn[261689],fontzn[261690],fontzn[261691],fontzn[261692],fontzn[261693],fontzn[261694],fontzn[261695]);
    //   if (esp_reset_reason() == ESP_RST_POWERON)
    //       display_show_splash();

    sdcard_open("/sd"); // map SD card.

    ui_init();
    //   charging_state st = getChargeStatus();
    //  if (st == CHARGING)
    //       handleCharging();

    UG_FontSelect(&FONT_8X12);
    if (settings_load(SettingWifi, &wifi_en) != 0)
        settings_save(SettingWifi, (int32_t)wifi_en);
    if (settings_load(SettingAudioVolume, &volume) != 0)
        settings_save(SettingAudioVolume, (int32_t)volume);
    if (settings_load(SettingBacklight, &bright) != 0)
        settings_save(SettingBacklight, (int32_t)bright);
    if (settings_load(SettingScaleMode, &scaling) != 0)
        settings_save(SettingScaleMode, (int32_t)scaling);
    if (settings_load(SettingAlg, &scale_alg) != 0)
        settings_save(SettingAlg, (int32_t)scale_alg);

    if (wifi_en)
    {
        tcpip_adapter_init();
        ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));
        ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
        wifi_config_t ap_config = {
            .ap = {
                .ssid = "esp32 Mini",
                .authmode = WIFI_AUTH_OPEN,
                .max_connection = 2,
                .beacon_interval = 200}};
        uint8_t channel = 5;
        ap_config.ap.channel = channel;
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));

        ESP_ERROR_CHECK(esp_wifi_start());

        /* Start the file server */
        ESP_ERROR_CHECK(start_file_server("/sd"));

        printf("\nReady, AP on channel %d\n", (int)channel);
    }
    else
    {
        printf("\nAP Disabled, enabled wifi to use File Manager\n");
    }

    drawHomeScreen();
    int menuItem = 0;
    int prevItem = 0;
    int scroll = 0;
    int doRefresh = 1;
    int oldArrowsTick = -1;
    int lastUpdate = 0;
    charging_state chrg_st = getChargeStatus();
    input_gamepad_state prevKey;
    gamepad_read(&prevKey);
    gamepad_read(&prevKey);
    //    printf("first time read key is %d \n",prevKey.values[GAMEPAD_INPUT_MENU]);
    ushort GFX_TICK = 0;
    ushort form_freq = 3;
    ushort bg_freq = 15;
    while (1)
    {
        GFX_TICK++;
        input_gamepad_state joystick;
        gamepad_read(&joystick);
        if (!prevKey.values[GAMEPAD_INPUT_RIGHT] && joystick.values[GAMEPAD_INPUT_RIGHT])
        {
            menuItem++;
            if (menuItem > NUM_EMULATOR - 1)
                menuItem = 0;
        }
        if (!prevKey.values[GAMEPAD_INPUT_LEFT] && joystick.values[GAMEPAD_INPUT_LEFT])
        {
            menuItem--;
            if (menuItem < 0)
                menuItem = NUM_EMULATOR - 1;
        }
        if ((!prevKey.values[GAMEPAD_INPUT_UP] && joystick.values[GAMEPAD_INPUT_UP]) || (!prevKey.values[GAMEPAD_INPUT_DOWN] && joystick.values[GAMEPAD_INPUT_DOWN]))
        {
            menuItem += NUM_EMULATOR / 2;
            menuItem = menuItem % (NUM_EMULATOR);
        }

        //render box

        slotRenderGraphics(menuItem, GFX_TICK, bg_freq, form_freq);

        if (!prevKey.values[GAMEPAD_INPUT_A] && joystick.values[GAMEPAD_INPUT_A])
        {
            if (menuItem < 6)
            {
                char ext[4];
                strcpy(ext, ".");
                strcat(ext, emu_dir[menuItem]);

                char *path = malloc(strlen(base_path) + strlen(emu_dir[menuItem]) + 1);
                strcpy(path, base_path);
                strcat(path, emu_dir[menuItem]);
                char *filename = ui_file_chooser_usercolor(path, ext, 0, emu_name[menuItem], ui_get_word_color(), ui_get_background_color(), ui_get_title_color());
                if (filename)
                {
                    settings_save_str(SettingRomPath, filename);
                    system_application_set(emu_slot[menuItem]);
                    ui_clear_screen();
                    ui_flush();
                    display_show_hourglass();
                    esp_restart();
                }
                free(path);
            }
            else if (menuItem == 6) //txtreader
            {
                char ext[4];
                strcpy(ext, ".");
                strcat(ext, emu_dir[menuItem]);

                char *path = malloc(strlen(base_path) + strlen(emu_dir[menuItem]) + 1);
                strcpy(path, base_path);
                strcat(path, emu_dir[menuItem]);
                char *filename = ui_file_chooser_usercolor(path, ext, 0, emu_name[menuItem], ui_get_word_color(), ui_get_background_color(), ui_get_title_color());
                if (filename)
                {
                    settings_save_str(SettingRomPath, filename);
                    char *romName = settings_load_str(SettingRomPath);
                    FILE *txt;
                    printf("TXT:%s\n", romName);
                    txt = fopen(romName, "r");

                    long PTR = LoadTxtState();
                    long dPTR = 0;
                    dPTR = write_txt_frame(txt, PTR, filename);
                    while (1)
                    {
                        vTaskDelay(10);
                        prevKey = joystick;
                        gamepad_read(&joystick);
                        if (joystick.values[GAMEPAD_INPUT_B])
                        {
                            SaveTxtState(PTR);
                            break;
                        }
                        if (!prevKey.values[GAMEPAD_INPUT_RIGHT] && joystick.values[GAMEPAD_INPUT_RIGHT])
                        {
                            PTR = PTR + dPTR;
                            dPTR = write_txt_frame(txt, PTR, filename);
                        }
                        if (!prevKey.values[GAMEPAD_INPUT_LEFT] && joystick.values[GAMEPAD_INPUT_LEFT])
                        {
                            if (dPTR > 10)
                                PTR -= dPTR;
                            else
                                PTR -= 10;
                            if (PTR < 0)
                                PTR = 0;
                            dPTR = write_txt_frame(txt, PTR, filename);
                        }
                    }
                    free(romName);
                    system_application_set(0);
                    esp_restart();
                }
            }
            else
            {
                Entry *new_entries;

                int n_entries = fops_list_dir(&new_entries, AUDIO_FILE_PATH);
                audio_player((AudioPlayerParam){new_entries, n_entries, 0, AUDIO_FILE_PATH, true});

                fops_free_entries(&new_entries, n_entries);

                UG_FontSelect(&FONT_8X12);
                drawHomeScreen();
                lastUpdate = 0;
                doRefresh = 1;
            }

            // B Pressed instead of A
            drawHomeScreen();
            lastUpdate = 0;
            doRefresh = 1;
        }
        if (!prevKey.values[GAMEPAD_INPUT_B] && joystick.values[GAMEPAD_INPUT_B])
            resume();
        //       printf("read key is %d \n",joystick.values[GAMEPAD_INPUT_MENU]);
        if (!prevKey.values[GAMEPAD_INPUT_MENU] && joystick.values[GAMEPAD_INPUT_MENU])
        {
            int r = showOption();
            if (r)
                esp_restart();

            drawHomeScreen();
            lastUpdate = 0;
            doRefresh = 1;
        }

        //if (getChargeStatus() != chrg_st)
        {
            battery_level_read(&bat_state);
            drawBattery(bat_state.percentage);
            doRefresh = 1;
            //chrg_st = getChargeStatus();
        }

        if (doRefresh)
            ui_flush();

        doRefresh = 0;
        prevKey = joystick;
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}
