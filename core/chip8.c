#include "chip8.h"
#include "chip8_err.h"
#include "freertos/idf_additions.h"
#include <esp_log.h>
#include <esp_random.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define CHIP8_FLAG_REGISTER 0x0F
#define CHIP8_MAX_BIT 7

const char *TAG = "EMULATOR";

const uint8_t fontset[80] = {
    // 0
    0xF0, // 11110000
    0x90, // 10010000
    0x90, // 10010000
    0x90, // 10010000
    0xF0, // 11110000

    // 1
    0x20,
    0x60,
    0x20,
    0x20,
    0x70,

    // 2
    0xF0,
    0x10,
    0xF0,
    0x80,
    0xF0,

    // 3
    0xF0,
    0x10,
    0xF0,
    0x10,
    0xF0,

    // 4
    0x90,
    0x90,
    0xF0,
    0x10,
    0x10,

    // 5
    0xF0,
    0x80,
    0xF0,
    0x10,
    0xF0,

    // 6
    0xF0,
    0x80,
    0xF0,
    0x90,
    0xF0,

    // 7
    0xF0,
    0x10,
    0x20,
    0x40,
    0x40,

    // 8
    0xF0,
    0x90,
    0xF0,
    0x90,
    0xF0,

    // 9
    0xF0,
    0x90,
    0xF0,
    0x10,
    0xF0,

    // A
    0xF0,
    0x90,
    0xF0,
    0x90,
    0x90,

    // B
    0xE0,
    0x90,
    0xE0,
    0x90,
    0xE0,

    // C
    0xF0,
    0x80,
    0x80,
    0x80,
    0xF0,

    // D
    0xE0,
    0x90,
    0x90,
    0x90,
    0xE0,

    // E
    0xF0,
    0x80,
    0xF0,
    0x80,
    0xF0,

    // F
    0xF0,
    0x80,
    0xF0,
    0x80,
    0x80,
};

chip8_error_t chip8_init(chip8_t *sys) {
  if (sys == NULL) {
    ESP_LOGE(TAG, "NULL pointer passed to chip8_init()");
    return CHIP8_NULL_PTR;
  }

  memset(sys->memory, 0, sizeof(sys->memory));
  memset(sys->V, 0, sizeof(sys->V));
  sys->I = 0;
  sys->PC = CHIP8_LOAD_ADDR;
  sys->SP = -1;
  memset(sys->stack, 0, sizeof(sys->stack));
  sys->DT = 0;
  sys->ST = 0;
  memset(sys->keyboard, 0, sizeof(sys->keyboard));
  memset(sys->gfx, 0, sizeof(sys->gfx));

  memcpy(sys->memory + CHIP8_FONT_ADDR, fontset, sizeof(fontset));

  return CHIP8_OK;
}

chip8_error_t chip8_load_rom(chip8_t *sys, const uint8_t *rom,
                             size_t rom_size) {
  if (sys == NULL || rom == NULL) {
    ESP_LOGE(TAG, "NULL pointer passed to chip8_load_rom");
    return CHIP8_NULL_PTR;
  }
  if (rom_size > MAX_ROM_SIZE) {
    ESP_LOGE(TAG, "ROM size: %zu bytes exceeds limit %zu bytes", rom_size,
             MAX_ROM_SIZE);
    return CHIP8_ROM_TOO_LARGE;
  }
  memcpy(sys->memory + CHIP8_LOAD_ADDR, rom, rom_size);
  ESP_LOGI(TAG, "Loaded ROM of size %zu bytes to address 0x200 successfully",
           rom_size);
  return CHIP8_OK;
}

chip8_error_t chip8_fetch(chip8_t *sys, uint16_t *out_opcode) {
  if (sys == NULL || out_opcode == NULL) {
    ESP_LOGE(TAG, "NULL pointer passed to chip8_fetch");
    return CHIP8_NULL_PTR;
  }

  if (sys->PC < 0x200 || sys->PC > 0xFFE) {
    return CHIP8_PC_OUT_OF_BOUNDS;
  }

  uint16_t opcode = sys->memory[sys->PC] << 8 | sys->memory[sys->PC + 1];
  sys->PC += 2;
  *out_opcode = opcode;
  return CHIP8_OK;
}

chip8_error_t chip8_execute(chip8_t *sys, uint16_t opcode) {
  if (sys == NULL) {
    ESP_LOGE(TAG, "NULL pointer passed to chip8_execute");
    return CHIP8_NULL_PTR;
  }

  switch (opcode & 0xF000) {

  case 0x0000: {
    // CLS
    if (opcode == 0x00E0) {
      memset(sys->gfx, 0x00, sizeof(sys->gfx));
    }

    // RET
    else if (opcode == 0x00EE) {
      if (sys->SP <= -1) {
        ESP_LOGE(TAG, "SEGFAULT: RET without CALL");
        return CHIP8_SEGFAULT;
      }
      sys->PC = sys->stack[sys->SP];
      sys->SP--;
    }

    // SYS addr (unused in modern ROMs)
    else {
      ESP_LOGE(TAG, "UNKNOWN OPCODE: %04X", opcode);
      return CHIP8_UNKNOWN_OPCODE;
    }
    break;
  }

  // JP addr
  case 0x1000: {
    uint16_t addr = opcode & 0x0FFF;

    if (addr < 0x200 || addr > 0xFFF) {
      ESP_LOGE(TAG, "SEGFAULT: PC out of bounds");
      return CHIP8_SEGFAULT;
    }
    sys->PC = addr;
    break;
  }

  // CALL addr
  case 0x2000: {
    uint16_t nnn = opcode & 0x0FFF;

    if (sys->SP >= 15) {
      ESP_LOGE(TAG, "SEGFAULT: Stack depth exceeds 16");
      return CHIP8_SEGFAULT;
    }

    if (nnn < 0x200 || nnn > 0xFFF) {
      ESP_LOGE(TAG, "SEGFAULT: PC out of bounds");
      return CHIP8_SEGFAULT;
    }
    sys->SP++;
    sys->stack[sys->SP] = sys->PC;
    sys->PC = nnn;
    break;
  }

  // SE Vx, byte
  case 0x3000: {
    uint8_t vx = (opcode & 0x0F00) >> 8;
    uint8_t byte = (opcode & 0x00FF);

    if (sys->V[vx] == byte) {
      sys->PC += 2;
    }
    break;
  }

  // SNE Vx, byte
  case 0x4000: {
    uint8_t vx = (opcode & 0x0F00) >> 8;
    uint8_t byte = opcode & 0x00FF;

    if (sys->V[vx] != byte) {
      sys->PC += 2;
    }
    break;
  }

  // SE Vx, Vy
  case 0x5000: {
    uint8_t vx = (opcode & 0x0F00) >> 8;
    uint8_t vy = (opcode & 0x00F0) >> 4;

    if (sys->V[vx] == sys->V[vy]) {
      sys->PC += 2;
    }
    break;
  }

  // LD Vx, byte
  case 0x6000: {
    uint8_t vx = (opcode & 0x0F00) >> 8;
    uint8_t byte = opcode & 0x00FF;

    sys->V[vx] = byte;
    break;
  }

  // ADD Vx, byte
  case 0x7000: {
    uint8_t vx = (opcode & 0x0F00) >> 8;
    uint8_t byte = opcode & 0x00FF;

    sys->V[vx] += byte;
    break;
  }

  case 0x8000: {
    // LD Vx, Vy
    if ((opcode & 0x000F) == 0x0000) {
      uint8_t vx = (opcode & 0x0F00) >> 8;
      uint8_t vy = (opcode & 0x00F0) >> 4;

      sys->V[vx] = sys->V[vy];
    }

    // OR Vx, Vy
    else if ((opcode & 0x000F) == 0x0001) {
      uint8_t vx = (opcode & 0x0F00) >> 8;
      uint8_t vy = (opcode & 0x00F0) >> 4;

      sys->V[vx] = sys->V[vx] | sys->V[vy];
    }

    // AND Vx, Vy
    else if ((opcode & 0x000F) == 0x0002) {
      uint8_t vx = (opcode & 0x0F00) >> 8;
      uint8_t vy = (opcode & 0x00F0) >> 4;

      sys->V[vx] = sys->V[vx] & sys->V[vy];
    }

    // XOR Vx, Vy
    else if ((opcode & 0x000F) == 0x0003) {
      uint8_t vx = (opcode & 0x0F00) >> 8;
      uint8_t vy = (opcode & 0x00F0) >> 4;

      sys->V[vx] = sys->V[vx] ^ sys->V[vy];
    }

    // ADD Vx, Vy
    else if ((opcode & 0x000F) == 0x0004) {
      uint8_t vx = (opcode & 0x0F00) >> 8;
      uint8_t vy = (opcode & 0x00F0) >> 4;

      uint16_t result = sys->V[vx] + sys->V[vy];

      if (result > 0xFF) {
        sys->V[CHIP8_FLAG_REGISTER] = 1;
      } else {
        sys->V[CHIP8_FLAG_REGISTER] = 0;
      }

      sys->V[vx] = (result & 0x00FF);
    }

    // SUB Vx, Vy
    else if ((opcode & 0x000F) == 0x0005) {
      uint8_t vx = (opcode & 0x0F00) >> 8;
      uint8_t vy = (opcode & 0x00F0) >> 4;

      if (sys->V[vx] > sys->V[vy]) {
        sys->V[CHIP8_FLAG_REGISTER] = 1;
      } else {
        sys->V[CHIP8_FLAG_REGISTER] = 0;
      }

      sys->V[vx] = sys->V[vx] - sys->V[vy];
    }

    // SHR Vx {, Vy}
    else if ((opcode & 0x000F) == 0x0006) {
      uint8_t vx = (opcode & 0x0F00) >> 8;

      sys->V[CHIP8_FLAG_REGISTER] = sys->V[vx] & 0x01;
      sys->V[vx] /= 2;
    }

    // SUBN Vx, Vy
    else if ((opcode & 0x000F) == 0x0007) {
      uint8_t vx = (opcode & 0x0F00) >> 8;
      uint8_t vy = (opcode & 0x00F0) >> 4;

      sys->V[CHIP8_FLAG_REGISTER] = sys->V[vy] > sys->V[vx];
      sys->V[vx] = sys->V[vy] - sys->V[vx];
    }

    // SHL Vx {, Vy}
    else if ((opcode & 0x000F) == 0x000E) {
      // 1000 0000 = 0x80
      uint8_t vx = (opcode & 0x0F00) >> 8;

      sys->V[CHIP8_FLAG_REGISTER] = (sys->V[vx] & 0x80) >> 7;
      sys->V[vx] *= 2;
    }

    else {
      ESP_LOGE(TAG, "UNKNOWN OPCODE: %04X", opcode);
      return CHIP8_UNKNOWN_OPCODE;
    }
    break;
  }

  // SNE Vx, Vy
  case 0x9000: {
    uint8_t vx = (opcode & 0x0F00) >> 8;
    uint8_t vy = (opcode & 0x00F0) >> 4;

    if (sys->V[vx] != sys->V[vy]) {
      sys->PC += 2;
    }
    break;
  }

  // LD I, addr
  case 0xA000: {
    uint16_t addr = opcode & 0x0FFF;

    sys->I = addr;
    break;
  }

  // JP V0, addr
  case 0xB000: {
    uint16_t addr = opcode & 0x0FFF;

    sys->PC = addr += sys->V[0];
    break;
  }

  // RND Vx, byte
  case 0xC000: {
    uint8_t vx = (opcode & 0x0F00) >> 8;
    uint8_t byte = opcode & 0x00FF;

    uint8_t rand_num = (uint8_t)esp_random();

    sys->V[vx] = rand_num & byte;
    break;
  }

  // DRW Vx, Vy, nibble
  case 0xD000: {
    uint8_t n = opcode & 0x000F;
    uint8_t vy = (opcode & 0x00F0) >> 4;
    uint8_t vx = (opcode & 0x0F00) >> 8;

    uint8_t x_pos = sys->V[vx];
    uint8_t y_pos = sys->V[vy];

    sys->V[CHIP8_FLAG_REGISTER] = 0;

    for (int i = 0; i < n; i++) {
      for (int j = 0; j < CHIP8_SPRITE_WIDTH; j++) {
        uint8_t current_x = (x_pos + j) % SCREEN_WIDTH;
        uint8_t current_y = (y_pos + i) % SCREEN_HEIGHT;
        uint16_t gfx_index = current_y * SCREEN_WIDTH + current_x;
        if (sys->gfx[gfx_index] == 1 &&
            (sys->memory[sys->I + i] >> (CHIP8_MAX_BIT - j) & 1) == 1) {
          sys->V[CHIP8_FLAG_REGISTER] = 1;
        }
        sys->gfx[gfx_index] ^=
            sys->memory[sys->I + i] >> (CHIP8_MAX_BIT - j) & 1;
      }
    }
    break;
  }

  case 0xE000: {
    // SKP Vx
    if ((opcode & 0x00FF) == 0x009E) {
      uint8_t vx = (opcode & 0x0F00) >> 8;

      if (sys->keyboard[sys->V[vx]] == 1) {
        sys->PC += 2;
      }
    }

    // SKNP Vx
    else if ((opcode & 0x00FF) == 0x00A1) {
      uint8_t vx = (opcode & 0x0F00) >> 8;

      if (sys->keyboard[sys->V[vx]] == 0) {
        sys->PC += 2;
      }
    } else {
      ESP_LOGE(TAG, "UNKNOWN OPCODE: %04X", opcode);
      return CHIP8_UNKNOWN_OPCODE;
    }
    break;
  }

  case 0xF000: {
    if ((opcode & 0x00FF) == 0x0007) {
      uint8_t vx = (opcode & 0x0F00) >> 8;

      sys->V[vx] = sys->DT;
    }

    // LD Vx, K
    else if ((opcode & 0x00FF) == 0x000A) {
      uint8_t vx = (opcode & 0x0F00) >> 8;
      uint8_t key_pressed = 0;

      for (int i = 0; i < 16; i++) {
        if (sys->keyboard[i] == 1) {
          key_pressed = 1;
          sys->V[vx] = i;
        }
      }

      if (key_pressed == 0) {
        sys->PC -= 2;
      }
    }

    // LD DT, Vx
    else if ((opcode & 0x00FF) == 0x0015) {
      uint8_t vx = (opcode & 0x0F00) >> 8;

      sys->DT = sys->V[vx];
    }

    // LD ST, Vx
    else if ((opcode & 0x00FF) == 0x0018) {
      uint8_t vx = (opcode & 0x0F00) >> 8;

      sys->ST = sys->V[vx];
    }

    // ADD I, Vx
    else if ((opcode & 0x00FF) == 0x001E) {
      uint8_t vx = (opcode & 0x0F00) >> 8;

      sys->I = sys->V[vx] + sys->I;
    }

    // LD F, Vx
    else if ((opcode & 0x00FF) == 0x0029) {
      uint8_t vx = (opcode & 0x0F00) >> 8;

      sys->I = CHIP8_FONT_ADDR + (sys->V[vx] & 0x000F) * 5;
    }

    // LD B, Vx
    else if ((opcode & 0x00FF) == 0x0033) {
      uint8_t vx = (opcode & 0x0F00) >> 8;

      if (sys->I + 2 > 0xFFF) {
        return CHIP8_SEGFAULT;
      }
      sys->memory[sys->I + 0] = sys->V[vx] / 100;
      sys->memory[sys->I + 1] = (sys->V[vx] / 10) % 10;
      sys->memory[sys->I + 2] = sys->V[vx] % 10;
    }

    // LD [I], Vx
    else if ((opcode & 0x00FF) == 0x0055) {
      uint8_t vx = (opcode & 0x0F00) >> 8;

      for (int i = 0; i <= vx; i++) {
        sys->memory[sys->I + i] = sys->V[i];
      }
    }

    // LD Vx, [I]
    else if ((opcode & 0x00FF) == 0x0065) {
      uint8_t vx = (opcode & 0x0F00) >> 8;

      if (sys->I + vx > 0xFFF) {
        return CHIP8_SEGFAULT;
      }
      for (int i = 0; i <= vx; i++) {
        sys->V[i] = sys->memory[sys->I + i];
      }
    } else {
      ESP_LOGE(TAG, "UNKNOWN OPCODE: %04X", opcode);
      return CHIP8_UNKNOWN_OPCODE;
    }
    break;
  }
  }
  return CHIP8_OK;
}
