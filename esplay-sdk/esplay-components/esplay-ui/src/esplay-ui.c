#include "freertos/FreeRTOS.h"
#include "esp_system.h"
#include "esp_event.h"
#include <stdio.h>
#include "ugui.h"
#include "display.h"
#include "ugui.h"
#include "sdcard.h"
#include "gamepad.h"
#include "esplay-ui.h"

#define MAX_CHR (240 / 9)
#define MAX_ITEM 8 //(145 / 15) // 193 = LCD Height (240) - header(16) - footer(16) - char height (15)

uint16_t *fb;
static UG_GUI *ugui;

static void pset(UG_S16 x, UG_S16 y, UG_COLOR color)
{
  fb[y * 240 + x] = color;
}

uint16_t *ui_get_fb()
{
  return fb;
}

void ui_clear_screen()
{
  memset(fb, 0, 240 * 192 * 2);
}

void ui_flush()
{
  write_frame_rectangleLE(0, 0, 240, 192, fb);
}

void ui_init()
{
  fb = malloc(240 * 192 * sizeof(uint16_t));
  ugui = malloc(sizeof(UG_GUI));
  UG_Init(ugui, pset, 240, 192);
  UG_FontSelect(&FONT_8X12);
  ui_clear_screen();
  ui_flush();
}

void ui_deinit()
{
  free(fb);
  free(ugui);
  fb = NULL;
  ugui = NULL;
}

void ui_display_progress(int x, int y, int width, int height, int percent, UG_COLOR frameColor, UG_COLOR infillColor, UG_COLOR progressColor)
{
  if (percent > 100)
    percent = 100;

  const int fillWidth = width * (percent / 100.0f);

  UG_FillFrame(x - 1, y - 1, x + width + 1, y + height + 1, infillColor);
  UG_DrawFrame(x - 1, y - 1, x + width + 1, y + height + 1, frameColor);

  if (fillWidth > 0)
    UG_FillFrame(x, y, x + fillWidth, y + height, progressColor);
}

void ui_display_seekbar(int x, int y, int width, int percent, UG_COLOR barColor, UG_COLOR seekColor)
{
  if (percent > 100)
    percent = 100;

  const int barHeight = 4;
  int pos = x + (width * (percent / 100.0f));

  UG_FillFrame(x, y, x + width, y + barHeight, barColor);

  if (pos > 0)
  {
    if (pos > (x + (width - 6)))
      pos = x + (width - 6);
    UG_FillFrame(pos, y - 2, pos + 6, y + barHeight + 2, seekColor);
    UG_DrawFrame(pos - 1, y - 3, pos + 7, y + barHeight + 3, barColor);
  }
  else
  {
    UG_FillFrame(x, y - 2, x + 6, y + barHeight + 2, seekColor);
    UG_DrawFrame(x - 1, y - 3, x + 7, y + barHeight + 3, barColor);
  }
}

void ui_display_switch(int x, int y, int state, UG_COLOR backColor, UG_COLOR enabledColor, UG_COLOR disabledColor)
{
  if (state)
  {
    UG_FillFrame(x, y, x + 10, y + 8, backColor);
    UG_FillFrame(x + 5, y + 1, x + 5 + 4, y + 1 + 6, enabledColor);
    UG_DrawFrame(x + 5, y + 1, x + 5 + 4, y + 1 + 6, C_BLACK);
  }
  else
  {
    UG_FillFrame(x, y, x + 10, y + 8, backColor);
    UG_FillFrame(x + 1, y + 1, x + 1 + 4, y + 1 + 6, disabledColor);
    UG_DrawFrame(x + 1, y + 1, x + 1 + 4, y + 1 + 6, C_BLACK);
  }
}

/// Make the file name nicer by cutting at brackets or (last) dot.
static int cut_file_name(char *filename)
{

  char *dot = strrchr(filename, '.');
  char *brack1 = strchr(filename, '(');
  char *brack2 = strchr(filename, '[');

  int len = strlen(filename);

  if (dot != NULL && dot - filename < len)
  {
    len = dot - filename;
  }

  if (brack1 != NULL && brack1 - filename < len)
  {
    len = brack1 - filename;
    if (filename[len - 1] == ' ')
    {
      len--;
    }
  }
  if (brack2 != NULL && brack2 - filename < len)
  {
    len = brack2 - filename;
    if (filename[len - 1] == ' ')
    {
      len--;
    }
  }

  return len;
}
static void ui_draw_page_list_usercolor(char **files, int fileCount, int currentItem, int extLen, char *title, ushort wordcolor, ushort backgroundcolor, ushort titlecolor)
{
  /* Header */
  UG_FillFrame(0, 0, 240 - 1, 16 - 1, titlecolor);
  UG_SetForecolor(wordcolor);
  UG_SetBackcolor(titlecolor);
  char *msg = title;
  UG_PutString((240 / 2) - (strlen(msg) * 9 / 2), 2, msg);
  /* End Header */

  UG_FillFrame(0, 16, 240 - 1, 192 - 1, backgroundcolor);

  const int innerHeight = 176;
  int page = currentItem / MAX_ITEM;
  page *= MAX_ITEM;
  const int itemHeight = innerHeight / MAX_ITEM;

  UG_FillFrame(0, 15, 240 - 1, 145 + 15, backgroundcolor);

  if (fileCount < 1)
  {
    UG_SetForecolor(wordcolor);
    UG_SetBackcolor(backgroundcolor);
    msg = "No Files Found";
    UG_PutString((320 / 2) - (strlen(msg) * 9 / 2), (192 - 16 - 13) / 2, msg);
    ui_flush();
  }
  else
  {
    char *displayStr[MAX_ITEM];
    for (int i = 0; i < MAX_ITEM; ++i)
    {
      displayStr[i] = NULL;
    }

    for (int line = 0; line < MAX_ITEM; ++line)
    {
      if (page + line >= fileCount)
        break;
      short top = 19 + (line * itemHeight) - 1;
      if ((page) + line == currentItem)
      {
        UG_FillFrame(0, top - 1, 240 - 1, top + 20 + 1, wordcolor);
        UG_SetForecolor(titlecolor);
        UG_SetBackcolor(wordcolor);
      }
      else
      {
        UG_SetForecolor(wordcolor);
        UG_SetBackcolor(backgroundcolor);
      }

      char *filename = files[page + line];
      if (!filename)
        abort();
      int length = cut_file_name(filename);
      displayStr[line] = (char *)malloc(length + 1);
      strncpy(displayStr[line], filename, length);
      displayStr[line][length] = 0;
      char truncnm[MAX_CHR];
      strncpy(truncnm, displayStr[line], MAX_CHR);
      truncnm[MAX_CHR - 1] = 0;

      UG_PutString(10, top + 3, truncnm);
    }
    ui_flush();
    for (int i = 0; i < MAX_ITEM; ++i)
    {
      free(displayStr[i]);
    }
  }
}
static void ui_draw_page_list(char **files, int fileCount, int currentItem, int extLen, char *title)
{
  /* Header */
  UG_FillFrame(0, 0, 240 - 1, 16 - 1, C_BLUE);
  UG_SetForecolor(C_WHITE);
  UG_SetBackcolor(C_BLUE);
  char *msg = title;
  UG_PutString((240 / 2) - (strlen(msg) * 9 / 2), 2, msg);
  /* End Header */

  UG_FillFrame(0, 16, 240 - 1, 192 - 1, C_BLACK);

  const int innerHeight = 176;
  int page = currentItem / MAX_ITEM;
  page *= MAX_ITEM;
  const int itemHeight = innerHeight / MAX_ITEM;

  UG_FillFrame(0, 15, 240 - 1, 145 + 15, C_BLACK);

  if (fileCount < 1)
  {
    UG_SetForecolor(C_RED);
    UG_SetBackcolor(C_BLACK);
    msg = "No Files Found";
    UG_PutString((320 / 2) - (strlen(msg) * 9 / 2), (192 - 16 - 13) / 2, msg);
    ui_flush();
  }
  else
  {
    char *displayStr[MAX_ITEM];
    for (int i = 0; i < MAX_ITEM; ++i)
    {
      displayStr[i] = NULL;
    }

    for (int line = 0; line < MAX_ITEM; ++line)
    {
      if (page + line >= fileCount)
        break;
      short top = 19 + (line * itemHeight) - 1;
      if ((page) + line == currentItem)
      {
        UG_FillFrame(0, top - 1, 240 - 1, top + 20 + 1, C_YELLOW);
        UG_SetForecolor(C_BLACK);
        UG_SetBackcolor(C_YELLOW);
      }
      else
      {
        UG_SetForecolor(C_WHITE);
        UG_SetBackcolor(C_BLACK);
      }

      char *filename = files[page + line];
      if (!filename)
        abort();
      int length = cut_file_name(filename);
      displayStr[line] = (char *)malloc(length + 1);
      strncpy(displayStr[line], filename, length);
      displayStr[line][length] = 0;
      char truncnm[MAX_CHR];
      strncpy(truncnm, displayStr[line], MAX_CHR);
      truncnm[MAX_CHR - 1] = 0;

      UG_PutString(10, top + 3, truncnm);
    }
    ui_flush();
    for (int i = 0; i < MAX_ITEM; ++i)
    {
      free(displayStr[i]);
    }
  }
}
char *ui_file_chooser_usercolor(const char *path, const char *filter, int currentItem, char *title, ushort wordcolor, ushort backgroundcolor, ushort titlecolor)
{
  const char *result = NULL;
  int extLen = strlen(filter);
  char **files = 0;
  int fileCount = sdcard_files_get(path, filter, &files);
  printf("%s: fileCount = %d\n", __func__, fileCount);

  int selected = currentItem;
  ui_draw_page_list_usercolor(files, fileCount, selected, extLen, title, wordcolor, backgroundcolor, titlecolor);

  input_gamepad_state prevKey;
  gamepad_read(&prevKey);

  while (true)
  {
    input_gamepad_state key;
    gamepad_read(&key);

    int page = selected / MAX_ITEM;
    page *= MAX_ITEM;

    if (fileCount > 0)
    {
      if (!prevKey.values[GAMEPAD_INPUT_DOWN] && key.values[GAMEPAD_INPUT_DOWN])
      {
        if (selected + 1 < fileCount)
        {
          ++selected;
          ui_draw_page_list_usercolor(files, fileCount, selected, extLen, title, wordcolor, backgroundcolor, titlecolor);
        }
        else
        {
          currentItem = 0;
          ui_draw_page_list_usercolor(files, fileCount, selected, extLen, title, wordcolor, backgroundcolor, titlecolor);
        }
      }
      else if (!prevKey.values[GAMEPAD_INPUT_UP] && key.values[GAMEPAD_INPUT_UP])
      {
        if (selected > 0)
        {
          --selected;
          ui_draw_page_list_usercolor(files, fileCount, selected, extLen, title, wordcolor, backgroundcolor, titlecolor);
        }
        else
        {
          selected = fileCount - 1;
          ui_draw_page_list_usercolor(files, fileCount, selected, extLen, title, wordcolor, backgroundcolor, titlecolor);
        }
      }
      else if (!prevKey.values[GAMEPAD_INPUT_RIGHT] && key.values[GAMEPAD_INPUT_RIGHT])
      {
        if (page + MAX_ITEM < fileCount)
        {
          selected = page + MAX_ITEM;
          ui_draw_page_list_usercolor(files, fileCount, selected, extLen, title, wordcolor, backgroundcolor, titlecolor);
        }
        else
        {
          selected = 0;
          ui_draw_page_list_usercolor(files, fileCount, selected, extLen, title, wordcolor, backgroundcolor, titlecolor);
        }
      }
      else if (!prevKey.values[GAMEPAD_INPUT_LEFT] && key.values[GAMEPAD_INPUT_LEFT])
      {
        if (page - MAX_ITEM > 0)
        {
          selected = page - MAX_ITEM;
          ui_draw_page_list_usercolor(files, fileCount, selected, extLen, title, wordcolor, backgroundcolor, titlecolor);
        }
        else
        {
          selected = page;
          while (selected + MAX_ITEM < fileCount)
          {
            selected += MAX_ITEM;
          }
          ui_draw_page_list_usercolor(files, fileCount, selected, extLen, title, wordcolor, backgroundcolor, titlecolor);
        }
      }
      else if (!prevKey.values[GAMEPAD_INPUT_A] && key.values[GAMEPAD_INPUT_A])
      {
        if (fileCount < 1)
        {
          vTaskDelay(10);
          break;
        }

        size_t fullPathLength = strlen(path) + 1 + strlen(files[selected]) + 1;

        char *fullPath = (char *)malloc(fullPathLength);
        if (!fullPath)
          abort();

        strcpy(fullPath, path);
        strcat(fullPath, "/");
        strcat(fullPath, files[selected]);

        ui_clear_screen();
        ui_flush();

        result = fullPath;
        break;
      }
      else if (!prevKey.values[GAMEPAD_INPUT_B] && key.values[GAMEPAD_INPUT_B])
      {
        vTaskDelay(10);
        break;
      }
    }
    else
    {
      if (!prevKey.values[GAMEPAD_INPUT_B] && key.values[GAMEPAD_INPUT_B])
      {
        vTaskDelay(10);
        break;
      }
    }

    prevKey = key;
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
  sdcard_files_free(files, fileCount);

  return result;
}
char *ui_file_chooser(const char *path, const char *filter, int currentItem, char *title)
{
  const char *result = NULL;
  int extLen = strlen(filter);
  char **files = 0;
  int fileCount = sdcard_files_get(path, filter, &files);
  printf("%s: fileCount = %d\n", __func__, fileCount);

  int selected = currentItem;
  ui_draw_page_list(files, fileCount, selected, extLen, title);

  input_gamepad_state prevKey;
  gamepad_read(&prevKey);

  while (true)
  {
    input_gamepad_state key;
    gamepad_read(&key);

    int page = selected / MAX_ITEM;
    page *= MAX_ITEM;

    if (fileCount > 0)
    {
      if (!prevKey.values[GAMEPAD_INPUT_DOWN] && key.values[GAMEPAD_INPUT_DOWN])
      {
        if (selected + 1 < fileCount)
        {
          ++selected;
          ui_draw_page_list(files, fileCount, selected, extLen, title);
        }
        else
        {
          currentItem = 0;
          ui_draw_page_list(files, fileCount, selected, extLen, title);
        }
      }
      else if (!prevKey.values[GAMEPAD_INPUT_UP] && key.values[GAMEPAD_INPUT_UP])
      {
        if (selected > 0)
        {
          --selected;
          ui_draw_page_list(files, fileCount, selected, extLen, title);
        }
        else
        {
          selected = fileCount - 1;
          ui_draw_page_list(files, fileCount, selected, extLen, title);
        }
      }
      else if (!prevKey.values[GAMEPAD_INPUT_RIGHT] && key.values[GAMEPAD_INPUT_RIGHT])
      {
        if (page + MAX_ITEM < fileCount)
        {
          selected = page + MAX_ITEM;
          ui_draw_page_list(files, fileCount, selected, extLen, title);
        }
        else
        {
          selected = 0;
          ui_draw_page_list(files, fileCount, selected, extLen, title);
        }
      }
      else if (!prevKey.values[GAMEPAD_INPUT_LEFT] && key.values[GAMEPAD_INPUT_LEFT])
      {
        if (page - MAX_ITEM > 0)
        {
          selected = page - MAX_ITEM;
          ui_draw_page_list(files, fileCount, selected, extLen, title);
        }
        else
        {
          selected = page;
          while (selected + MAX_ITEM < fileCount)
          {
            selected += MAX_ITEM;
          }
          ui_draw_page_list(files, fileCount, selected, extLen, title);
        }
      }
      else if (!prevKey.values[GAMEPAD_INPUT_A] && key.values[GAMEPAD_INPUT_A])
      {
        if (fileCount < 1)
        {
          vTaskDelay(10);
          break;
        }

        size_t fullPathLength = strlen(path) + 1 + strlen(files[selected]) + 1;

        char *fullPath = (char *)malloc(fullPathLength);
        if (!fullPath)
          abort();

        strcpy(fullPath, path);
        strcat(fullPath, "/");
        strcat(fullPath, files[selected]);

        ui_clear_screen();
        ui_flush();

        result = fullPath;
        break;
      }
      else if (!prevKey.values[GAMEPAD_INPUT_B] && key.values[GAMEPAD_INPUT_B])
      {
        vTaskDelay(10);
        break;
      }
    }
    else
    {
      if (!prevKey.values[GAMEPAD_INPUT_B] && key.values[GAMEPAD_INPUT_B])
      {
        vTaskDelay(10);
        break;
      }
    }

    prevKey = key;
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
  sdcard_files_free(files, fileCount);

  return result;
}