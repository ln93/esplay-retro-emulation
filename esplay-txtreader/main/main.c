#include "freertos/FreeRTOS.h"
#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_event_loop.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/ledc.h"
#include "driver/i2s.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "esp_task_wdt.h"
#include "esp_spiffs.h"
#include "driver/rtc_io.h"
#include "esp_partition.h"
#include "esp_ota_ops.h"
#include "esp_task_wdt.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "display.h"
#include "ugui/ugui.h"
#include <gamepad.h>
#include <audio.h>
#include <sdcard.h>
#include <settings.h>
#include <power.h>
#include "display_txt.h"
#include "menu.h"
extern uint16_t freq;
extern int debug_trace;
uint16_t *fb;
static UG_GUI *ugui;
int showOverlay = 0;
long FilePTR = 0, deltaPTR = 0;
static FILE *txt;
const char *SD_BASE_PATH = "/sd";
int32_t scaleAlg;

#define GAMEBOY_WIDTH (160)
#define GAMEBOY_HEIGHT (144)

#define AUDIO_SAMPLE_RATE (32000)

static void DoMenuHome();
static void SaveState();
static void LoadState();
static void PowerDown();
static void pset(UG_S16 x, UG_S16 y, UG_COLOR color)
{
    fb[y * 240 + x] = color;
}
void ui_clear_screen()
{
    memset(fb, 0, 240 * 240 * 2);
}
void ui_flush()
{
    write_frame_rectangleLE(0, 0, 240, 192, fb);
}
// --- MAIN
QueueHandle_t vidQueue;

volatile bool videoTaskIsRunning = false;
esplay_scale_option scale_opt;
extern uint8_t batlevel;

void videoTask(void *arg)
{
    esp_err_t ret;
    settings_load(SettingScaleMode, &scale_opt);
    videoTaskIsRunning = true;

    uint16_t *param;
    while (1)
    {
        esp_task_wdt_feed();
        xQueuePeek(vidQueue, &param, portMAX_DELAY);

        if (param == 1)
            break;

        if (param == 2)
        {

            deltaPTR = write_txt_frame(txt, FilePTR);
            int ret = showMenu();
            if (batlevel == 0)
                ret = MENU_SAVE_EXIT;
            switch (ret)
            {
            case MENU_SAVE_STATE:
                display_show_hourglass();
                SaveState();
                break;

            case MENU_LOAD:
                display_show_hourglass();
                LoadState();
                break;

            case MENU_SAVE_EXIT:
                display_show_hourglass();
                SaveState();
                system_application_set(0);
                esp_restart();

                break;

            case MENU_RESET:
                FilePTR = 0;
                break;

            case MENU_EXIT:
                display_show_hourglass();
                system_application_set(0);
                esp_restart();
                break;

            default:
                break;
            }
            write_txt_frame(NULL, scale_opt);
            showOverlay = 0;
            vTaskDelay(10);
        }
        else
            write_txt_frame(NULL, scale_opt);
        xQueueReceive(vidQueue, &param, portMAX_DELAY);
    }

    display_show_hourglass();

    videoTaskIsRunning = false;
    vTaskDelete(NULL);

    while (1)
    {
    }
}

static void SaveState()
{
    esp_err_t r = sdcard_open(SD_BASE_PATH);
    if (r != ESP_OK)
    {
        //odroid_display_show_sderr(ODROID_SD_ERR_NOCARD);
        abort();
    }
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

static void LoadState()
{
    esp_err_t r = sdcard_open(SD_BASE_PATH);
    if (r != ESP_OK)
    {
        //odroid_display_show_sderr(ODROID_SD_ERR_NOCARD);
        abort();
    }
    char *romName = settings_load_str(SettingRomPath);
    if (romName)
    {
        printf("TXT:%s\n", romName);
        txt = fopen(romName, "r");
        if (txt == NULL)
        {
            printf("loader: force fopen failed. try lowcase\n");

            int pathlen = strlen(romName);
            romName[pathlen - 3] = 't';
            romName[pathlen - 2] = 'x';
            romName[pathlen - 1] = 't';
            printf("TXT:%s\n", romName);
            txt = fopen(romName, "r");
            if (txt == NULL)
            {
                printf("loader: fopen still failed, restart system.\n");
                system_application_set(0);
                esp_restart();
            }
        }
        printf("loader: TXT open success.\n");
    }
    char *fileName = system_util_GetFileName(romName);
    if (!fileName)
    {
        system_application_set(0);
        esp_restart();
    }

    char *pathName = sdcard_create_savefile_path(SD_BASE_PATH, fileName);
    if (!pathName)
    {
        system_application_set(0);
        esp_restart();
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
}

static void PowerDown()
{
    uint16_t *param = 1;

    // Stop tasks
    printf("PowerDown: stopping tasks.\n");

    xQueueSend(vidQueue, &param, portMAX_DELAY);
    while (videoTaskIsRunning)
    {
    }

    // state
    printf("PowerDown: Saving state.\n");
    SaveState();

    // LCD
    printf("PowerDown: Powerdown LCD panel.\n");
    display_poweroff();

    system_sleep();
    //esp_deep_sleep_start();

    // Should never reach here
    abort();
}

static void DoMenuHome()
{
    esp_err_t err;
    uint16_t *param = 1;

    // Stop tasks
    printf("PowerDown: stopping tasks.\n");

    xQueueSend(vidQueue, &param, portMAX_DELAY);
    while (videoTaskIsRunning)
    {
    }

    // Set menu application
    system_application_set(0);

    // state
    printf("PowerDown: Saving state.\n");
    SaveState();

    // Reset
    esp_restart();
}

void app_main(void)
{
    printf("TXT Reader (%s-%s).\n", COMPILEDATE, GITREV);

    settings_init();

    esplay_system_init();
    // Display
    display_prepare();
    display_init();
    fb = malloc(240 * 192 * sizeof(uint16_t));
    ugui = malloc(sizeof(UG_GUI));
    UG_Init(ugui, pset, 240, 192);
    UG_FontSelect(&FONT_8X12);
    ui_clear_screen();
    ui_flush();
    // set brightness
    int brightness = 50;
    settings_load(SettingBacklight, &brightness);
    set_display_brightness(brightness);

    // volume
    int volume = 25;
    settings_load(SettingAudioVolume, &volume);
    audio_volume_set(volume);

    // load alghorithm
    settings_load(SettingAlg, &scaleAlg);

    // battery
    battery_level_init();

    // Gamepad
    gamepad_init();

    // ui init
    // Boot state overrides
    bool forceConsoleReset = false;

    switch (esp_sleep_get_wakeup_cause())
    {
    case ESP_SLEEP_WAKEUP_EXT0:
    {
        printf("app_main: ESP_SLEEP_WAKEUP_EXT0 deep sleep wake\n");
        break;
    }

    case ESP_SLEEP_WAKEUP_EXT1:
    case ESP_SLEEP_WAKEUP_TIMER:
    case ESP_SLEEP_WAKEUP_TOUCHPAD:
    case ESP_SLEEP_WAKEUP_ULP:
    case ESP_SLEEP_WAKEUP_UNDEFINED:
    {
        printf("app_main: Non deep sleep startup\n");

        input_gamepad_state bootState = gamepad_input_read_raw();

        if (bootState.values[GAMEPAD_INPUT_MENU])
        {
            // Force return to factory app to recover from
            // ROM loading crashes

            // Set menu application
            system_application_set(0);

            // Reset
            esp_restart();
        }

        if (bootState.values[GAMEPAD_INPUT_START])
        {
            // Reset emulator if button held at startup to
            // override save state
            forceConsoleReset = true;
        }

        break;
    }
    default:
        printf("app_main: Not a deep sleep reset\n");
        break;
    }

    // Clear display
    write_txt_frame(NULL, SCALE_STRETCH);

    // video
    vidQueue = xQueueCreate(1, sizeof(uint16_t *));

    xTaskCreatePinnedToCore(&videoTask, "videoTask", 1024 * 3, NULL, 5, NULL, 1);

    // Load state
    LoadState();

    input_gamepad_state lastJoysticState;

    ushort menuButtonFrameCount = 0;
    bool ignoreMenuButton = lastJoysticState.values[GAMEPAD_INPUT_MENU];

    // Reset if button held at startup
    if (forceConsoleReset)
    {
        FilePTR = 0;
    }
    int frame_skip = 0;
    gamepad_read(&lastJoysticState);
    while (true)
    {
        vTaskDelay(10);
        input_gamepad_state joystick;
        gamepad_read(&joystick);

        if (ignoreMenuButton)
        {
            ignoreMenuButton = lastJoysticState.values[GAMEPAD_INPUT_MENU];
        }

        if (!ignoreMenuButton && lastJoysticState.values[GAMEPAD_INPUT_MENU] && !joystick.values[GAMEPAD_INPUT_MENU])
        {
            showOverlay = 1;
            uint16_t *param = 2;
            printf("try to open menu\n");
            xQueueSend(vidQueue, &param, portMAX_DELAY);
            lastJoysticState = joystick;
        }

        if (!ignoreMenuButton && lastJoysticState.values[GAMEPAD_INPUT_MENU] && joystick.values[GAMEPAD_INPUT_MENU])
        {
            ++menuButtonFrameCount;
        }
        else
        {
            menuButtonFrameCount = 0;
        }

        while (showOverlay)
        {
            vTaskDelay(10);
        }

        if (joystick.values[GAMEPAD_INPUT_UP])
        {
            FilePTR += deltaPTR;
            if (FilePTR < 0)
                FilePTR = 0;
            write_txt_frame(txt, FilePTR);
        }
        else if (joystick.values[GAMEPAD_INPUT_DOWN])
        {
            FilePTR -= deltaPTR;
            write_txt_frame(txt, FilePTR);
        }
    }
}
