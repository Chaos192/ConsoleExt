#pragma once
#include <vector>
#include <Windows.h>

std::uint8_t* PatternScan(void* module, const char* signature);
std::uint8_t* PatternScan(void* module, const char* signature, uintptr_t offset);