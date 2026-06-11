#pragma once
#include <Arduino.h>

// Lightweight serial logging that compiles to nothing unless DEBUG_BUILD is set
// (controlled via build_flags in platformio.ini).
#ifdef DEBUG_BUILD
#define LOG_BEGIN(baud) Serial.begin(baud)
#define LOGLN(...) Serial.println(__VA_ARGS__)
#define LOGF(...) Serial.printf(__VA_ARGS__)
#else
#define LOG_BEGIN(baud) ((void)0)
#define LOGLN(...) ((void)0)
#define LOGF(...) ((void)0)
#endif
