/*
 * This file is part of Arduino_KVStore.
 *
 * Copyright (c) 2024 Arduino SA
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once
#include "../kvstore.h"
#include <zephyr/settings/settings.h>

class ZephyrKVStore: public KVStoreInterface {
public:
    ZephyrKVStore();
    ~ZephyrKVStore() { end(); }

    bool begin() override;
    bool end() override;
    bool clear() override;

    typename KVStoreInterface::res_t remove(const key_t& key) override;
    bool exists(const key_t& key) const override;
    typename KVStoreInterface::res_t putBytes(const key_t& key, const uint8_t b[], size_t s) override;
    typename KVStoreInterface::res_t getBytes(const key_t& key, uint8_t b[], size_t s) const override;
    size_t getBytesLength(const key_t& key) const override;
};
