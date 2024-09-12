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
#include <KVStore.h>
#include <TDBStore.h>
#include <Arduino_DebugUtils.h>
#include "QSPIFBlockDevice.h"
#include "MBRBlockDevice.h"

class STM32H7KVStore: public KVStoreInterface<const char*> {
public:
    STM32H7KVStore();
    ~STM32H7KVStore() { end(); }

    bool begin() override;
    bool begin(bool reformat, mbed::KVStore* store = nullptr);
    bool end() override;
    bool clear() override;

    typename KVStoreInterface<const char*>::res_t remove(const key_t& key) override;
    bool exists(const key_t& key) const override;
    typename KVStoreInterface<const char*>::res_t putBytes(const key_t& key, uint8_t b[], size_t s) override;
    typename KVStoreInterface<const char*>::res_t getBytes(const key_t& key, uint8_t b[], size_t s) const override;
    size_t getBytesLength(const key_t& key) const override;
private:
    mbed::MBRBlockDevice* bd;
    mbed::KVStore* kvstore;
};

STM32H7KVStore::STM32H7KVStore(): kvstore(nullptr), bd(nullptr) {}

bool STM32H7KVStore::begin() {
    return begin(false);
}

bool STM32H7KVStore::begin(bool reformat, mbed::KVStore* store) {
    // bd gets allocated if a kvstore is not provided as parameter here
    // if either one of bd or kvstore is different from NULL it means that the kvstore
    // had already been called begin on
    if(bd != nullptr || kvstore != nullptr) {
        return false;
    }

    if(store != nullptr) {
        kvstore = store;
    } else {
        auto root = mbed::BlockDevice::get_default_instance();

        if (root->init() != QSPIF_BD_ERROR_OK) {
            Serial.println(F("Error: QSPI init failure."));
            return false;
        }

        bd = new mbed::MBRBlockDevice(root, 4);
        int res = bd->init();
        if (res != QSPIF_BD_ERROR_OK && !reformat) {
            Serial.println(F("Error: QSPI is not properly formatted, "
                "run QSPIformat.ino set reformat to true"));
            return false;
        } else if (res != QSPIF_BD_ERROR_OK && reformat) {
            Serial.println(F("Error: QSPI is not properly formatted, "
                "reformatting it according to the following scheme:"));
            Serial.println(F("Partition 1: WiFi firmware and certificates 1MB"));
            Serial.println(F("Partition 2: OTA 5MB"));
            Serial.println(F("Partition 3: User data 7MB")),
            Serial.println(F("Partition 4: Provisioning KVStore 1MB"));

            mbed::MBRBlockDevice::partition(root, 1, 0x0B, 0, 1024 * 1024);
            mbed::MBRBlockDevice::partition(root, 2, 0x0B, 1024 * 1024, 6 * 1024 * 1024);
            mbed::MBRBlockDevice::partition(root, 3, 0x0B, 6 * 1024 * 1024, 13 * 1024 * 1024);
            mbed::MBRBlockDevice::partition(root, 4, 0x0B, 13* 1024 * 1024, 14 * 1024 * 1024);
        }

        kvstore = new mbed::TDBStore(bd);
    }

    return kvstore->init() == MBED_SUCCESS;
}

bool STM32H7KVStore::end() {
    bool res = false;

    if(kvstore != nullptr && bd == nullptr) {
        res = kvstore->deinit() == MBED_SUCCESS;
        kvstore = nullptr;
    } else if(kvstore != nullptr && bd != nullptr) {
        res = kvstore->deinit() == MBED_SUCCESS;

        delete kvstore;
        kvstore = nullptr;

        delete bd;
        bd = nullptr;
    }

    return res;
}

template<typename T=int>
static inline typename KVStoreInterface<const char*>::res_t fromMbedErrors(int error, T res=1) {
    return error == MBED_SUCCESS ? res : -error;
}

bool STM32H7KVStore::clear() {
    return kvstore != nullptr ? kvstore->reset() == MBED_SUCCESS : false;
}

typename KVStoreInterface<const char*>::res_t STM32H7KVStore::remove(const key_t& key) {
    return kvstore != nullptr ? fromMbedErrors(kvstore->remove(key)) : -1;
}

typename KVStoreInterface<const char*>::res_t STM32H7KVStore::putBytes(const key_t& key, uint8_t buf[], size_t len) {
    return kvstore != nullptr ? fromMbedErrors(kvstore->set(key, buf, len, 0), len) : -1; // TODO flags
}

typename KVStoreInterface<const char*>::res_t STM32H7KVStore::getBytes(const key_t& key, uint8_t buf[], size_t maxLen) const {
    size_t actual_size;
    auto res = kvstore->get(key, buf, maxLen, &actual_size);

    return kvstore != nullptr ? fromMbedErrors(res, actual_size) : -1;
}

size_t STM32H7KVStore::getBytesLength(const key_t& key) const {
    if(kvstore == nullptr) {
        return 0;
    }

    mbed::KVStore::info_t info;
    auto res = kvstore->get_info(key, &info);

    return res == MBED_SUCCESS ? info.size : 0;
}

bool STM32H7KVStore::exists(const key_t& key) const {
    return getBytesLength(key) > 0;
}
