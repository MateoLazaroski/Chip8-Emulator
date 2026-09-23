#ifndef CHIP8_H
#define CHIP8_H

#include "chip8_err.h"
#include <stddef.h>
#include <stdint.h>

#define MEMSIZE 4096
#define SCREEN_WIDTH 64
#define SCREEN_HEIGHT 32
#define MAX_ROM_SIZE ((size_t) 3584)
#define CHIP8_LOAD_ADDR 0x200
#define CHIP8_FONT_ADDR 0x050
#define CHIP8_SPRITE_WIDTH 8
#define CHIP8_MAX_SPRITE_HEIGHT 15

typedef struct {
    uint8_t memory[MEMSIZE];

    uint8_t V[16];
    uint16_t I;
    uint16_t PC;
    int8_t SP;
    uint16_t stack[16];

    uint8_t DT;
    uint8_t ST;

    uint8_t keyboard[16];
    uint8_t gfx[SCREEN_WIDTH * SCREEN_HEIGHT];
}chip8_t;

extern const uint8_t fontset[80];

chip8_error_t chip8_init(chip8_t *sys);
chip8_error_t chip8_load_rom(chip8_t *sys, const uint8_t *rom, size_t rom_size);
chip8_error_t chip8_fetch(chip8_t *sys, uint16_t *out_opcode);
chip8_error_t chip8_execute(chip8_t *sys, uint16_t opcode);

#endif
