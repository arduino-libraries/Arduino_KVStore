#pragma once
#include "kvstore/kvstore.h"

#if defined(ARDUINO_UNOR4_WIFI)
#include "kvstore/implementation/UnoR4.h"

// using KVStore = Unor4KVStore<const char*>;
using KVStore = Unor4KVStore;

#elif defined(ARDUINO_PORTENTA_H7_M7) || defined(ARDUINO_NICLA_VISION) \
    || defined(ARDUINO_OPTA) || defined(ARDUINO_GIGA)
#include "kvstore/implementation/stm32h7.h"

using KVStore = STM32H7KVStore;

#else
#error "KVStore not present on current platform"
#endif
