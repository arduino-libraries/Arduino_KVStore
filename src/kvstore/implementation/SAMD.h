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

// #if ! __has_include(<utility/wifi_drv.h>)
// #error "WiFiNina library is required for this to work"
// #endif

#include <WiFi.h>

using namespace std;

const char DEFAULT_KVSTORE_NAME[] = "arduino";

class SAMDKVStore: public KVStoreInterface<const char*> {
public:
    SAMDKVStore(const char* name=DEFAULT_KVSTORE_NAME): name(name) {}
    bool begin();
    bool begin(const char* name);
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

bool SAMDKVStore::begin(const char* name) {
    bool readOnly=false; const char* partition_label=NULL; // FIXME this was a parameter

    WiFiDrv::wifiDriverInit();
    return WiFiDrv::prefBegin(name, readOnly, partition_label);
}

bool SAMDKVStore::begin() {
    return begin(DEFAULT_KVSTORE_NAME);
}

bool SAMDKVStore::end() {
    WiFiDrv::prefEnd();
    return true;
}

bool SAMDKVStore::clear() {
    return WiFiDrv::prefClear();
}

typename KVStoreInterface<const char*>::res_t SAMDKVStore::remove(const key_t& key) {
    return WiFiDrv::prefRemove(key);
}

typename KVStoreInterface<const char*>::res_t SAMDKVStore::putBytes(const key_t& key, uint8_t value[], size_t len) {
    return WiFiDrv::prefPut(key, PT_BLOB, value, len);
}

typename KVStoreInterface<const char*>::res_t SAMDKVStore::getBytes(const key_t& key, uint8_t buf[], size_t maxLen) const {
    return WiFiDrv::prefGet(key, PT_BLOB, buf, maxLen);
}

size_t SAMDKVStore::getBytesLength(const key_t& key) const {
    return WiFiDrv::prefLen(key);
}


bool SAMDKVStore::exists(const key_t& key) const {
    return getBytesLength(key) > 0;
}

// specialization of put and get, when esp nvs treats them differently
template<>
typename KVStoreInterface<const char*>::res_t SAMDKVStore::put<int8_t>(const key_t& key, int8_t value) {
    return WiFiDrv::prefPut(key, PT_I8, (uint8_t*) &value, sizeof(value));
}

template<>
typename KVStoreInterface<const char*>::res_t SAMDKVStore::put<uint8_t>(const key_t& key, uint8_t value) {
    return WiFiDrv::prefPut(key, PT_U8, (uint8_t*) &value, sizeof(value));
}

template<>
typename KVStoreInterface<const char*>::res_t SAMDKVStore::put<int16_t>(const key_t& key, int16_t value) {
    return WiFiDrv::prefPut(key, PT_I16, (uint8_t*) &value, sizeof(value));
}

template<>
typename KVStoreInterface<const char*>::res_t SAMDKVStore::put<uint16_t>(const key_t& key, uint16_t value) {
    return WiFiDrv::prefPut(key, PT_U16, (uint8_t*) &value, sizeof(value));
}

template<>
typename KVStoreInterface<const char*>::res_t SAMDKVStore::put<int32_t>(const key_t& key, int32_t value) {
    return WiFiDrv::prefPut(key, PT_I32, (uint8_t*) &value, sizeof(value));
}

template<>
typename KVStoreInterface<const char*>::res_t SAMDKVStore::put<uint32_t>(const key_t& key, uint32_t value) {
    return WiFiDrv::prefPut(key, PT_U32, (uint8_t*) &value, sizeof(value));
}

template<>
typename KVStoreInterface<const char*>::res_t SAMDKVStore::put<int64_t>(const key_t& key, int64_t value) {
    return WiFiDrv::prefPut(key, PT_I64, (uint8_t*) &value, sizeof(value));
}

template<>
typename KVStoreInterface<const char*>::res_t SAMDKVStore::put<uint64_t>(const key_t& key, uint64_t value) {
    return WiFiDrv::prefPut(key, PT_U64, (uint8_t*) &value, sizeof(value));
}

template<>
typename KVStoreInterface<const char*>::res_t SAMDKVStore::put<const char*>(const key_t& key, const char* value) {
    return WiFiDrv::prefPut(key, PT_STR, (uint8_t*) value, strlen(value)+1); // we also send the \0
}

template<>
KVStoreInterface<const char*>::reference<int8_t> SAMDKVStore::get<int8_t>(const key_t& key, int8_t defaultValue) {
    int8_t res=0;

    if(!exists(key)) {
        return reference<int8_t>(key, defaultValue, *this);
    }

    WiFiDrv::prefGet(key, PT_I8, (uint8_t*)&res, sizeof(res));

    return reference<int8_t>(key, res, *this);
}

template<>
KVStoreInterface<const char*>::reference<uint8_t> SAMDKVStore::get<uint8_t>(const key_t& key, uint8_t defaultValue) {
    uint8_t res=0;

    if(!exists(key)) {
        return reference<uint8_t>(key, defaultValue, *this);
    }

    WiFiDrv::prefGet(key, PT_U8, (uint8_t*)&res, sizeof(res));

    return reference<uint8_t>(key, res, *this);
}

template<>
KVStoreInterface<const char*>::reference<int16_t> SAMDKVStore::get<int16_t>(const key_t& key, int16_t defaultValue) {
    int16_t res=0;

    if(!exists(key)) {
        return reference<int16_t>(key, defaultValue, *this);
    }

    WiFiDrv::prefGet(key, PT_I16, (uint8_t*)&res, sizeof(res));

    return reference<int16_t>(key, res, *this);
}

template<>
KVStoreInterface<const char*>::reference<uint16_t> SAMDKVStore::get<uint16_t>(const key_t& key, uint16_t defaultValue) {
    uint16_t res=0;

    if(!exists(key)) {
        return reference<uint16_t>(key, defaultValue, *this);
    }

    WiFiDrv::prefGet(key, PT_U16, (uint8_t*)&res, sizeof(res));

    return reference<uint16_t>(key, res, *this);
}

template<>
KVStoreInterface<const char*>::reference<int32_t> SAMDKVStore::get<int32_t>(const key_t& key, int32_t defaultValue) {
    int32_t res=0;

    if(!exists(key)) {
        return reference<int32_t>(key, defaultValue, *this);
    }

    WiFiDrv::prefGet(key, PT_I32, (uint8_t*)&res, sizeof(res));

    return reference<int32_t>(key, res, *this);
}

template<>
KVStoreInterface<const char*>::reference<uint32_t> SAMDKVStore::get<uint32_t>(const key_t& key, uint32_t defaultValue) {
    uint32_t res=0;

    if(!exists(key)) {
        return reference<uint32_t>(key, defaultValue, *this);
    }

    WiFiDrv::prefGet(key, PT_U32, (uint8_t*)&res, sizeof(res));

    return reference<uint32_t>(key, res, *this);
}

template<>
KVStoreInterface<const char*>::reference<int64_t> SAMDKVStore::get<int64_t>(const key_t& key, int64_t defaultValue) {
    int64_t res=0;

    if(!exists(key)) {
        return reference<int64_t>(key, defaultValue, *this);
    }

    WiFiDrv::prefGet(key, PT_I64, (uint8_t*)&res, sizeof(res));

    return reference<int64_t>(key, res, *this);
}

template<>
KVStoreInterface<const char*>::reference<uint64_t> SAMDKVStore::get<uint64_t>(const key_t& key, uint64_t defaultValue) {
    uint64_t res=0;

    if(!exists(key)) {
        return reference<uint64_t>(key, defaultValue, *this);
    }

    WiFiDrv::prefGet(key, PT_U64, (uint8_t*)&res, sizeof(res));

    return reference<uint64_t>(key, res, *this);
}
