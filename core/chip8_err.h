#ifndef CHIP8_ERR
#define CHIP8_ERR

typedef enum {
    CHIP8_OK,
    CHIP8_ROM_TOO_LARGE,
    CHIP8_NULL_PTR,
    CHIP8_PC_OUT_OF_BOUNDS,
    CHIP8_UNKNOWN_OPCODE,
    CHIP8_SEGFAULT
}chip8_error_t;

#endif
