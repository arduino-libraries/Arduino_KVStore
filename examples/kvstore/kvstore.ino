/*
 * This file is part of Arduino_Storage.
 *
 * The purpose of this example is to showcase the usage of KVStore apis
 *
 * Copyright (c) 2024 Arduino SA
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <Arduino_KVStore.h>

void setup() {
    Serial.begin(115200);

    while(!Serial);
    delay(5000);
    KVStore kvstore;

    kvstore.begin();

    uint32_t value = 0x01020304;

    kvstore.remove("0");
    Serial.println(kvstore.putUInt("0", value));
    Serial.println(kvstore.getUInt("0"), HEX);
    kvstore.remove("0");

    Serial.println(kvstore.putBytes("0", (uint8_t*)&value, sizeof(value)));
    // Serial.println(kvstore.getbytes("0"), HEX);
    kvstore.remove("0");
    Serial.println(kvstore.exists("4"));
}

void loop() {

}
