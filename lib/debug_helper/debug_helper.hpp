#pragma once
#include <Arduino.h>

#define DEBUG 1
#if DEBUG
#define S_DEBUGF(fmt, ...) Serial.printf(fmt, ##__VA_ARGS__)
#else
#define S_DEBUGF(fmt, ...)
#endif