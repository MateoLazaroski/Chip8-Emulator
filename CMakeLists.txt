cmake_minimum_required(VERSION 3.12)

project(Chip8_Emulator C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

find_package(SDL3 REQUIRED)

add_executable(Chip8_Emulator src/main.c src/core/chip8.c)

target_link_libraries(Chip8_Emulator PRIVATE SDL3::SDL3)
