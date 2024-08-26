#pragma once
#include "kvstore/kvstore.h"

#if defined(ARDUINO_UNOR4_WIFI)
#include "kvstore/implementation/UnoR4.h"

// using KVStore = Unor4KVStore<const char*>;
using KVStore = Unor4KVStore;
#endif
