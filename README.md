# Chip8-Emulator

A basic Chip8-Emulator.

![Demo](screenshots/demo1.png)

![Demo](screenshots/demo2.png)

## Usage

Run the emulator by providing the path to a CHIP-8 ROM file:

```bash
./build/Chip8_Emulator path/to/rom.ch8
```

## Controls

The CHIP-8 uses a 16-key hexadecimal keypad (`0`-`F`). These keys are mapped to your keyboard as follows:

| CHIP-8 Keypad | Keyboard Input |
| :---: | :---: |
| `1` `2` `3` `C` | `1` `2` `3` `4` |
| `4` `5` `6` `D` | `Q` `W` `E` `R` |
| `7` `8` `9` `E` | `A` `S` `D` `F` |
| `A` `0` `B` `F` | `Z` `X` `C` `V` |

## Dependencies

To build and run this project, you need:

* **CMake** (version 3.12 or higher)
* **SDL3** development libraries

## Building from source

```bash
cmake -B build
cmake --build build
```
