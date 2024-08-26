/*
 * This file is part of Arduino_Storage.
 *
 * Copyright (c) 2024 Arduino SA
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
#pragma once

#include "../kvstore.h"
#include <Arduino.h>
#include <Modem.h>
#include <string>

using namespace std;

constexpr char DEFAULT_KVSTORE_NAME[] = "arduino";

class Unor4KVStore: public KVStoreInterface<const char*> {
public:
    typedef enum {
        PT_I8, PT_U8, PT_I16, PT_U16, PT_I32, PT_U32, PT_I64, PT_U64, PT_STR, PT_BLOB, PT_INVALID
    } Type;


    Unor4KVStore(): name(DEFAULT_KVSTORE_NAME) {}

    bool begin();
    bool begin(const char* name, bool readOnly=false, const char* partitionLabel=nullptr);
    bool end();
    bool clear();

    typename KVStoreInterface<const char*>::res_t remove(const key_t& key) override;
    bool exists(const key_t& key) const override;
    typename KVStoreInterface<const char*>::res_t putBytes(const key_t& key, uint8_t b[], size_t s) override;
    typename KVStoreInterface<const char*>::res_t getBytes(const key_t& key, uint8_t b[], size_t s) const override;
    size_t getBytesLength(const key_t& key) const override;

    template<typename T1>
    typename KVStoreInterface<const char*>::res_t put(const key_t& key, T1 value) { return KVStoreInterface<const char*>::put(key, value); }

    template<typename T1>
    KVStoreInterface<const char*>::reference<T1> get(const key_t& key, T1 def = 0) { return KVStoreInterface<const char*>::get(key, def); }


private:
    const char* name;
};

bool Unor4KVStore::begin(const char* name, bool readOnly, const char* partitionLabel) {
    this->name = name;
    string res = "";

    modem.begin();
    if (this->name != nullptr && strlen(this->name) > 0) {
        if (modem.write(string(PROMPT(_PREF_BEGIN)), res, "%s%s,%d,%s\r\n", CMD_WRITE(_PREF_BEGIN), name, readOnly, partitionLabel != NULL ? partitionLabel : "")) {
            return (atoi(res.c_str()) != 0) ? true : false;
        }
    }
    return false;
}

bool Unor4KVStore::begin() {
    return begin(DEFAULT_KVSTORE_NAME);
}

bool Unor4KVStore::end() {
    string res = "";
    modem.write(string(PROMPT(_PREF_END)), res, "%s", CMD(_PREF_END));
}

bool Unor4KVStore::clear() {
    string res = "";
    if (modem.write(string(PROMPT(_PREF_CLEAR)), res, "%s", CMD(_PREF_CLEAR))) {
        return (atoi(res.c_str()) != 0) ? true : false;
    }
    return false;
}

typename KVStoreInterface<const char*>::res_t Unor4KVStore::remove(const key_t& key) {
    string res = "";
    if (key != nullptr && strlen(key) > 0) {
        if (modem.write(string(PROMPT(_PREF_REMOVE)), res, "%s%s\r\n", CMD_WRITE(_PREF_REMOVE), key)) {
            return (atoi(res.c_str()) != 0) ? true : false;
        }
    }
    return false;
}

typename KVStoreInterface<const char*>::res_t Unor4KVStore::putBytes(const key_t& key, uint8_t value[], size_t len) {
    string res = "";
    if ( key != nullptr && strlen(key) > 0 && value != nullptr && len > 0) {
        modem.write_nowait(string(PROMPT(_PREF_PUT)), res, "%s%s,%d,%d\r\n", CMD_WRITE(_PREF_PUT), key, PT_BLOB, len);
        if(modem.passthrough((uint8_t *)value, len, &res)) {
            return atoi(res.c_str());
        }
    }
    return 0;
}

typename KVStoreInterface<const char*>::res_t Unor4KVStore::getBytes(const key_t& key, uint8_t buf[], size_t maxLen) const {
    size_t len = getBytesLength(key);
    string res = "";
    if (key != nullptr && strlen(key) > 0 && buf != nullptr && len > 0) {
        modem.avoid_trim_results();
        modem.read_using_size();
        if (modem.write(string(PROMPT(_PREF_GET)), res, "%s%s,%d\r\n", CMD_WRITE(_PREF_GET), key, PT_BLOB)) {
            if (res.size() >= len && len <= maxLen) {
                memcpy(buf, (uint8_t*)&res[0], len);
                return len;
            }
        }
    }
    return 0;
}

size_t Unor4KVStore::getBytesLength(const key_t& key) const {
    string res = "";
    if (key != nullptr && strlen(key) > 0) {
        if (modem.write(string(PROMPT(_PREF_LEN)), res, "%s%s\r\n", CMD_WRITE(_PREF_LEN), key)) {
            return atoi(res.c_str());
        }
    }
    return 0;
}


bool Unor4KVStore::exists(const key_t& key) const {
    string res = "";
    if (key != nullptr && strlen(key) > 0) {
        if (modem.write(string(PROMPT(_PREF_TYPE)), res, "%s%s\r\n", CMD_WRITE(_PREF_TYPE), key)) {
            return static_cast<Type>(atoi(res.c_str())) != PT_INVALID;
        }
    }
    return false;
}

// specialization of put and get, when esp nvs treats them differently
template<>
typename KVStoreInterface<const char*>::res_t Unor4KVStore::put<int8_t>(const key_t& key, int8_t value) {
    string res = "";
    if (key != nullptr && strlen(key) > 0) {
        if (modem.write(string(PROMPT(_PREF_PUT)), res, "%s%s,%d,%hu\r\n", CMD_WRITE(_PREF_PUT), key, PT_I8, value)) {
            return atoi(res.c_str());
        }
    }
    return 0;
}

template<>
typename KVStoreInterface<const char*>::res_t Unor4KVStore::put<uint8_t>(const key_t& key, uint8_t value) {
    string res = "";
    if (key != nullptr && strlen(key) > 0) {
        if (modem.write(string(PROMPT(_PREF_PUT)), res, "%s%s,%d,%hu\r\n", CMD_WRITE(_PREF_PUT), key, PT_U8, value)) {
            return atoi(res.c_str());
        }
    }
    return 0;
}

template<>
typename KVStoreInterface<const char*>::res_t Unor4KVStore::put<int16_t>(const key_t& key, int16_t value) {
    string res = "";
    if (key != nullptr && strlen(key) > 0) {
        if (modem.write(string(PROMPT(_PREF_PUT)), res, "%s%s,%d,%hu\r\n", CMD_WRITE(_PREF_PUT), key, PT_I16, value)) {
            return atoi(res.c_str());
        }
    }
    return 0;
}

template<>
typename KVStoreInterface<const char*>::res_t Unor4KVStore::put<uint16_t>(const key_t& key, uint16_t value) {
    string res = "";
    if (key != nullptr && strlen(key) > 0) {
        if (modem.write(string(PROMPT(_PREF_PUT)), res, "%s%s,%d,%hu\r\n", CMD_WRITE(_PREF_PUT), key, PT_U16, value)) {
            return atoi(res.c_str());
        }
    }
    return 0;
}

template<>
typename KVStoreInterface<const char*>::res_t Unor4KVStore::put<int32_t>(const key_t& key, int32_t value) {
    string res = "";
    if (key != nullptr && strlen(key) > 0) {
        if (modem.write(string(PROMPT(_PREF_PUT)), res, "%s%s,%d,%hu\r\n", CMD_WRITE(_PREF_PUT), key, PT_I32, value)) {
            return atoi(res.c_str());
        }
    }
    return 0;
}

template<>
typename KVStoreInterface<const char*>::res_t Unor4KVStore::put<uint32_t>(const key_t& key, uint32_t value) {
    string res = "";
    if (key != nullptr && strlen(key) > 0) {
        if (modem.write(string(PROMPT(_PREF_PUT)), res, "%s%s,%d,%hu\r\n", CMD_WRITE(_PREF_PUT), key, PT_U32, value)) {
            return atoi(res.c_str());
        }
    }
    return 0;
}

template<>
typename KVStoreInterface<const char*>::res_t Unor4KVStore::put<int64_t>(const key_t& key, int64_t value) {
    string res = "";
    if (key != nullptr && strlen(key) > 0) {
        if (modem.write(string(PROMPT(_PREF_PUT)), res, "%s%s,%d,%hu\r\n", CMD_WRITE(_PREF_PUT), key, PT_I64, value)) {
            return atoi(res.c_str());
        }
    }
    return 0;
}

template<>
typename KVStoreInterface<const char*>::res_t Unor4KVStore::put<uint64_t>(const key_t& key, uint64_t value) {
    string res = "";
    if (key != nullptr && strlen(key) > 0) {
        if (modem.write(string(PROMPT(_PREF_PUT)), res, "%s%s,%d,%hu\r\n", CMD_WRITE(_PREF_PUT), key, PT_U64, value)) {
            return atoi(res.c_str());
        }
    }
    return 0;
}

template<>
typename KVStoreInterface<const char*>::res_t Unor4KVStore::put<const char*>(const key_t& key, const char* value) {
    string res = "";
    if (key != nullptr && strlen(key) > 0 && value != nullptr && strlen(value) > 0) {
        modem.write_nowait(string(PROMPT(_PREF_PUT)), res, "%s%s,%d,%d\r\n", CMD_WRITE(_PREF_PUT), key, PT_STR, strlen(value));
        if(modem.passthrough((uint8_t *)value, strlen(value))) {
            return strlen(value);
        }
    }
    return 0;
}

template<>
typename KVStoreInterface<string>::res_t Unor4KVStore::put<string>(const key_t& key, string value) {
    string res = "";
    if (key != nullptr && strlen(key) > 0 && value.length() > 0) {
        modem.write_nowait(string(PROMPT(_PREF_PUT)), res, "%s%s,%d,%d\r\n", CMD_WRITE(_PREF_PUT), key, PT_STR, value.length());
        if(modem.passthrough((uint8_t *)value.c_str(), value.length())) {
            return value.length();
        }
    }
    return 0;
}

template<>
typename KVStoreInterface<String>::res_t Unor4KVStore::put<String>(const key_t& key, String value) {
    string res = "";
    if (key != nullptr && strlen(key) > 0 && value.length() > 0) {
        modem.write_nowait(string(PROMPT(_PREF_PUT)), res, "%s%s,%d,%d\r\n", CMD_WRITE(_PREF_PUT), key, PT_STR, value.length());
        if(modem.passthrough((uint8_t *)value.c_str(), value.length())) {
            return value.length();
        }
    }
    return 0;
}

template<>
KVStoreInterface<const char*>::reference<int8_t> Unor4KVStore::get<int8_t>(const key_t& key, int8_t defaultValue) {
    int8_t value = defaultValue;
    string res = "";
    if (key != nullptr && strlen(key) > 0) {
        if (modem.write(string(PROMPT(_PREF_GET)), res, "%s%s,%d,%hd\r\n", CMD_WRITE(_PREF_GET), key, PT_I8, defaultValue)) {
            sscanf(res.c_str(), "%hd", &value);
        }
    }
    return reference<int8_t>(key, value, *this);
}

template<>
KVStoreInterface<const char*>::reference<uint8_t> Unor4KVStore::get<uint8_t>(const key_t& key, uint8_t defaultValue) {
    uint8_t value = defaultValue;
    string res = "";
    if (key != nullptr && strlen(key) > 0) {
        if (modem.write(string(PROMPT(_PREF_GET)), res, "%s%s,%d,%hu\r\n", CMD_WRITE(_PREF_GET), key, PT_U8, defaultValue)) {
            sscanf(res.c_str(), "%hu", &value);
        }
    }
    return reference<uint8_t>(key, value, *this);
}

template<>
KVStoreInterface<const char*>::reference<int16_t> Unor4KVStore::get<int16_t>(const key_t& key, int16_t defaultValue) {
    int16_t value = defaultValue;
    string res = "";
    if (key != nullptr && strlen(key) > 0) {
        if (modem.write(string(PROMPT(_PREF_GET)), res, "%s%s,%d,%hd\r\n", CMD_WRITE(_PREF_GET), key, PT_I16, defaultValue)) {
            sscanf(res.c_str(), "%hd", &value);
        }
    }
    return reference<int16_t>(key, value, *this);
}

template<>
KVStoreInterface<const char*>::reference<uint16_t> Unor4KVStore::get<uint16_t>(const key_t& key, uint16_t defaultValue) {
    uint16_t value = defaultValue;
    string res = "";
    if (key != nullptr && strlen(key) > 0) {
        if (modem.write(string(PROMPT(_PREF_GET)), res, "%s%s,%d,%hu\r\n", CMD_WRITE(_PREF_GET), key, PT_U16, defaultValue)) {
            sscanf(res.c_str(), "%hu", &value);
        }
    }
    return reference<uint16_t>(key, value, *this);
}

template<>
KVStoreInterface<const char*>::reference<int32_t> Unor4KVStore::get<int32_t>(const key_t& key, int32_t defaultValue) {
    int32_t value = defaultValue;
    string res = "";
    if (key != nullptr && strlen(key) > 0) {
        if (modem.write(string(PROMPT(_PREF_GET)), res, "%s%s,%d,%d\r\n", CMD_WRITE(_PREF_GET), key, PT_I32, defaultValue)) {
            sscanf(res.c_str(), "%d", &value);
        }
    }
    return reference<int32_t>(key, value, *this);
}

template<>
KVStoreInterface<const char*>::reference<uint32_t> Unor4KVStore::get<uint32_t>(const key_t& key, uint32_t defaultValue) {
    uint32_t value = defaultValue;
    string res = "";
    if (key != nullptr && strlen(key) > 0) {
        if (modem.write(string(PROMPT(_PREF_GET)), res, "%s%s,%d,%u\r\n", CMD_WRITE(_PREF_GET), key, PT_U32, defaultValue)) {
            sscanf(res.c_str(), "%u", &value);
        }
    }
    return reference<uint32_t>(key, value, *this);
}

template<>
KVStoreInterface<const char*>::reference<int64_t> Unor4KVStore::get<int64_t>(const key_t& key, int64_t defaultValue) {
    int64_t value = defaultValue;
    string res = "";
    if (key != nullptr && strlen(key) > 0) {
        if (modem.write(string(PROMPT(_PREF_GET)), res, "%s%s,%d,%u\r\n", CMD_WRITE(_PREF_GET), key, PT_I64, defaultValue)) {
            sscanf(res.c_str(), "%u", &value);
        }
    }
    return reference<int64_t>(key, value, *this);
}

template<>
KVStoreInterface<const char*>::reference<uint64_t> Unor4KVStore::get<uint64_t>(const key_t& key, uint64_t defaultValue) {
    uint64_t value = defaultValue;
    string res = "";
    if (key != nullptr && strlen(key) > 0) {
        if (modem.write(string(PROMPT(_PREF_GET)), res, "%s%s,%d,%u\r\n", CMD_WRITE(_PREF_GET), key, PT_U64, defaultValue)) {
            sscanf(res.c_str(), "%u", &value);
        }
    }
    return reference<uint64_t>(key, value, *this);
}

template<>
KVStoreInterface<const char*>::reference<const char*> Unor4KVStore::get<const char*>(const key_t& key, const char* defaultValue) {
    string res = defaultValue;
    if (key != nullptr && strlen(key) > 0) {
        modem.read_using_size();
        if (modem.write(string(PROMPT(_PREF_GET)), res, "%s%s,%d,%s\r\n", CMD_WRITE(_PREF_GET), key, PT_STR, defaultValue)) {
            return reference<const char*>(key, res.c_str(), *this);
        }
    }
    return reference<const char*>(key, defaultValue, *this);
}

template<>
KVStoreInterface<const char*>::reference<string> Unor4KVStore::get<string>(const key_t& key, string defaultValue) {
    string res = defaultValue;
    if (key != nullptr && strlen(key) > 0) {
        modem.read_using_size();
        if (modem.write(string(PROMPT(_PREF_GET)), res, "%s%s,%d,%s\r\n", CMD_WRITE(_PREF_GET), key, PT_STR, defaultValue.c_str())) {
            return reference<string>(key, res, *this);
        }
    }
    return reference<string>(key, defaultValue, *this);
}

template<>
KVStoreInterface<const char*>::reference<String> Unor4KVStore::get<String>(const key_t& key, String defaultValue) {
    string res;
    if (key != nullptr && strlen(key) > 0) {
        modem.read_using_size();
        if (modem.write(string(PROMPT(_PREF_GET)), res, "%s%s,%d,%s\r\n", CMD_WRITE(_PREF_GET), key, PT_STR, defaultValue.c_str())) {
            return reference<String>(key, res.c_str(), *this);
        }
    }
    return reference<String>(key, defaultValue, *this);
}
