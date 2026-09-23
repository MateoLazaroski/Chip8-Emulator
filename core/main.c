#include "buzzer.h"
#include "chip8.h"
#include "chip8_err.h"
#include "display.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include "freertos/task.h"
#include "input.h"
#include "jingle.h"
#include "portmacro.h"
#include "roms.h"
#include "soc/soc.h"
#include "storage.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CHIP8_EMULATION_STACK_SIZE 4096
#define CHIP8_EMULATION_PRIORITY 5
#define CHIP8_EMULATION_TASK_NAME "emulation_loop"
#define CHIP8_FRAME_TIME 16
#define CHIP8_CYCLES_PER_FRAME 12

static const char *TAG = "MAIN";
static chip8_t chip8;
static uint8_t rom_buffer[MAX_ROM_SIZE];

typedef enum { GAME_SELECTION, CHIP8_EMULATOR } handheld_state;

void handheld_core_task(void *pvParameters);

void app_main(void) {
  storage_init();
  display_init();
  buzzer_init();
  input_init();

  display_draw_startup();
  vTaskDelay(pdMS_TO_TICKS(2000));
  play_jingle(JINGLE_BOOT);
  vTaskDelay(pdMS_TO_TICKS(1000));
  play_jingle(JINGLE_STARTUP);
  vTaskDelay(pdMS_TO_TICKS(1000));

  chip8_error_t err = chip8_init(&chip8);
  if (err != CHIP8_OK) {
    ESP_LOGE(TAG,
             "Initialization failed with error code %d, starting error screen",
             err);
    display_draw_err(err);
    vTaskSuspend(NULL);
  }

  xTaskCreatePinnedToCore(handheld_core_task, CHIP8_EMULATION_TASK_NAME,
                          CHIP8_EMULATION_STACK_SIZE, NULL,
                          CHIP8_EMULATION_PRIORITY, NULL, APP_CPU_NUM);
}

void handheld_core_task(void *pvParameters) {
  handheld_state state = GAME_SELECTION;

  uint16_t game_index = 0;
  char files[4][64] = {
      "Tetris.ch8",
      "Pong.ch8",
      "Tron.ch8",
      "GTA-VI.ch8",
  };

  TickType_t previous_time = xTaskGetTickCount();
  chip8_error_t err;
  while (1) {
    switch (state) {
    case GAME_SELECTION: {
      storage_fill_page_array(files, game_index, 4);
      display_draw_game_selection(files);
      vTaskDelay(pdMS_TO_TICKS(1000));

      size_t bytes_read = storage_read_file_to_buffer(
          rom_buffer, "/roms/slipperyslope.ch8", MAX_ROM_SIZE);
      if (bytes_read == 0) {
        continue;
      }

      err = chip8_load_rom(&chip8, rom_buffer, bytes_read);
      if (err != CHIP8_OK) {
        ESP_LOGE(TAG, "Loading failed with error code %d", err);
        continue;
      }
      previous_time = xTaskGetTickCount();
      state = CHIP8_EMULATOR;
      break;
    }

    case CHIP8_EMULATOR: {
      uint16_t opcode;

      handle_input(&chip8);

      for (int i = 0; i < CHIP8_CYCLES_PER_FRAME; i++) {
        err = chip8_fetch(&chip8, &opcode);
        if (err != CHIP8_OK) {
          display_draw_err(err);
          vTaskSuspend(NULL);
        }

        err = chip8_execute(&chip8, opcode);
        if (err != CHIP8_OK) {
          display_draw_err(err);
          vTaskSuspend(NULL);
        }
      }

      if (chip8.ST > 0) {
        buzzer_set_level(1);
        chip8.ST--;
      } else {
        buzzer_set_level(0);
      }

      if (chip8.DT > 0) {
        chip8.DT--;
      }

      display_draw(chip8.gfx);
      vTaskDelayUntil(&previous_time, pdMS_TO_TICKS(CHIP8_FRAME_TIME));
      break;
    }
    }
  }
}
