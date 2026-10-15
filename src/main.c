#define SDL_MAIN_USE_CALLBACKS 1

#include <stdint.h>
#include <stdio.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include "core/chip8.h"
#include "core/chip8_err.h"

#define DISPLAY_SCALE_FACTOR 15
#define CHIP8_CYCLES_PER_FRAME 60

static chip8_t chip8;

static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static SDL_Texture *gfx_texture = NULL;

static uint8_t rom_buffer[4096];

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[]) {
  if (argc < 2) {
    printf("ERROR: No file path specified\n");
    return SDL_APP_FAILURE;
  } else if (argc > 2) {
    printf("ERROR: Too many arguments\n");
    return SDL_APP_FAILURE;
  }

  chip8_error_t err = chip8_init(&chip8);
  if (err != CHIP8_OK) {
    printf("Initialization failed with error code %d\n", err);
    return SDL_APP_FAILURE;
  }

  FILE *file = fopen(argv[1], "rb");
  if (file == NULL) {
    printf("ERROR: File not found\n");
    return SDL_APP_FAILURE;
  }

  size_t filesize = fread(rom_buffer, 1, MAX_ROM_SIZE, file);
  fclose(file);

  err = chip8_load_rom(&chip8, rom_buffer, filesize);
  if (err != CHIP8_OK) {
    printf("ERROR: Could not load ROM file\n");
  }
  SDL_SetAppMetadata("Chip8-Emulator", "1.0", "Chip8-Emulator");

  if (!SDL_Init(SDL_INIT_VIDEO)) {
    SDL_Log("Couldn't initialize SDL: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }

  if (!SDL_CreateWindowAndRenderer("Chip8-Emulator",
                                   SCREEN_WIDTH * DISPLAY_SCALE_FACTOR,
                                   SCREEN_HEIGHT * DISPLAY_SCALE_FACTOR,
                                   SDL_WINDOW_RESIZABLE, &window, &renderer)) {
    SDL_Log("Couldn't create window/renderer: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }
  SDL_SetRenderLogicalPresentation(
      renderer, SCREEN_WIDTH * DISPLAY_SCALE_FACTOR,
      SCREEN_HEIGHT * DISPLAY_SCALE_FACTOR, SDL_LOGICAL_PRESENTATION_LETTERBOX);

  if (!SDL_SetRenderVSync(renderer, 1)) {
    SDL_Log("Warning: VSync not supported: %s", SDL_GetError());
  }

  SDL_Color colors[2] = {{0, 0, 0, 255}, {255, 255, 255, 255}};

  gfx_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_INDEX8,
                                  SDL_TEXTUREACCESS_STREAMING, SCREEN_WIDTH,
                                  SCREEN_HEIGHT);

  SDL_Palette *palette = SDL_CreatePalette(2);
  SDL_SetPaletteColors(palette, colors, 0, 2);
  SDL_SetTexturePalette(gfx_texture, palette);
  SDL_SetTextureScaleMode(gfx_texture, SDL_SCALEMODE_PIXELART);

  return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event) {
  if (event->type == SDL_EVENT_QUIT) {
    return SDL_APP_SUCCESS;
  } else if (event->type == SDL_EVENT_KEY_DOWN || event->type == SDL_EVENT_KEY_UP) {
        uint8_t state;
        if (event->type == SDL_EVENT_KEY_DOWN){
            state = 1;
        } else {
            state = 0;
        }

        switch (event->key.scancode) {
            case SDL_SCANCODE_1: chip8.keyboard[1] = state; printf("TEST\n"); break;
            case SDL_SCANCODE_2: chip8.keyboard[2] = state; break;
            case SDL_SCANCODE_3: chip8.keyboard[3] = state; break;
            case SDL_SCANCODE_4: chip8.keyboard[0xC] = state; break;
            case SDL_SCANCODE_Q: chip8.keyboard[4] = state; break;
            case SDL_SCANCODE_W: chip8.keyboard[5] = state; break;
            case SDL_SCANCODE_E: chip8.keyboard[6] = state; break;
            case SDL_SCANCODE_R: chip8.keyboard[0xD] = state; break;
            case SDL_SCANCODE_A: chip8.keyboard[7] = state; break;
            case SDL_SCANCODE_S: chip8.keyboard[8] = state; break;
            case SDL_SCANCODE_D: chip8.keyboard[9] = state; break;
            case SDL_SCANCODE_F: chip8.keyboard[0xE] = state; break;
            case SDL_SCANCODE_Z: chip8.keyboard[0xA] = state; break;
            case SDL_SCANCODE_X: chip8.keyboard[0] = state; break;
            case SDL_SCANCODE_C: chip8.keyboard[0xB] = state; break;
            case SDL_SCANCODE_V: chip8.keyboard[0xF] = state; break;
            default: break;
        }
    }
  return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate) {
  uint16_t opcode;

  for (int i = 0; i < CHIP8_CYCLES_PER_FRAME; i++) {
    chip8_error_t err = chip8_fetch(&chip8, &opcode);
    err = chip8_execute(&chip8, opcode);
  }

  if (chip8.ST > 0) {
    // Sound On
    chip8.ST--;
  } else {
    // Sound Off
  }

  if (chip8.DT > 0) {
    chip8.DT--;
  }

  SDL_SetRenderDrawColorFloat(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE_FLOAT);

  SDL_UpdateTexture(gfx_texture, NULL, chip8.gfx, SCREEN_WIDTH);

  SDL_RenderClear(renderer);
  SDL_RenderTexture(renderer, gfx_texture, NULL, NULL);
  SDL_RenderPresent(renderer);

  return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result) {
}
