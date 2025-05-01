#pragma once

#include <cinttypes>

#define HELP_PATTERN						"48 89 5C 24 08 57 48 83 EC 20 48 8D 0D ? ? ? ? E8 ? ? ? ? 48 8D 1D ? ? ? ? BF ? ? ? ?"
#define CONSOLEPRINT_PATTERN				"48 89 4c 24 08 48 89 54 24 10 4c 89 44 24 18 4c 89 4c 24 20 48 83 ec 28 80 3d 41 ? ? ? ?"
#define EXECUTECMD_PATTERN					"48 89 5c 24 08 48 89 74 24 20 57 48 81 ec d0 00 00 00 48 8b 05 ? ? ? ? 48 33 c4 48 89 84 24 c8 00 00 00"
#define PRINT_FLAG_PATTERN					"c6 05 ? ? ? ? 01"

#define NEXT_POW2(x) (((x) | ((x) >> 1) | ((x) >> 2) | ((x) >> 4) | ((x) >> 8) | ((x) >> 16)) + 1)

static uintptr_t base = 0;
static uintptr_t _console_print = 0;
static uintptr_t _execute_cmd = 0;
static uintptr_t _init_commands = 0;
static uintptr_t _console_print_flag = 0;

struct Command_t {
	char* buf;
	uint64_t pad;
	uint64_t length;
	uint64_t capacity;
};