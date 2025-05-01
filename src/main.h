#pragma once

#include <cinttypes>

#define CONSOLEPRINT_PATTERN				"48 89 4c 24 08 48 89 54 24 10 4c 89 44 24 18 4c 89 4c 24 20 48 83 ec 28 80 3d 41 ? ? ? ?"
#define EXECUTECMD_PATTERN					"48 89 5c 24 08 48 89 74 24 20 57 48 81 ec d0 00 00 00 48 8b 05 ? ? ? ? 48 33 c4 48 89 84 24 c8 00 00 00"
#define PRINT_FLAG_PATTERN					"c6 05 ? ? ? ? 01"

static uintptr_t base = 0;
static uintptr_t _console_print = 0;
static uintptr_t _execute_cmd = 0;
static uintptr_t _init_commands = 0;
static uintptr_t _console_print_flag = 0;
static uintptr_t _add_func = 0;