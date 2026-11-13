/*
 * This file is part of Arduino_KVStore.
 *
 * Copyright (c) 2024 Arduino SA
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifdef ARDUINO_ARCH_ZEPHYR

#if !defined(CONFIG_SETTINGS) || !defined(CONFIG_NVS)
#error "unable to use kvstore on zephyr without CONFIG_SETTINGS and CONFIG_NVS"
#endif

#include "zephyr.h"
#include <zephyr/kvss/nvs.h>
#include "./zephyr_migration/config.h"

#if ZEPHYR_MIGRATE_TDBSTORE_AUTO
#include "./zephyr_migration/migrate_tdbstore.h"
#endif

ZephyrKVStore::ZephyrKVStore() {

}

bool ZephyrKVStore::begin() {
#if ZEPHYR_MIGRATE_TDBSTORE_AUTO
	if (has_tdb_store() && migrate() != MIGRATE_OK) {
		return false;
	}
#endif

	return settings_subsys_init() == 0;
}

bool ZephyrKVStore::end() {
	return true;
}

bool ZephyrKVStore::clear() {
	// Zephyr settings do not provide a function to clear all settings present in the used
	// backend, we make the assumption the backend is always nvs and clear that
	void *storage;
	if (settings_storage_get(&storage) == 0) {
		nvs_clear((struct nvs_fs *)storage);
		nvs_mount((struct nvs_fs *)storage);
	}
	return true;
}

typename KVStoreInterface::res_t ZephyrKVStore::remove(const key_t& key) {
	return settings_delete(key) == 0;
}

typename KVStoreInterface::res_t ZephyrKVStore::putBytes(const key_t& key, const uint8_t buf[], size_t len) {
	int rc = settings_save_one(key, buf, len);
	return rc == 0 ? len : rc;
}

typename KVStoreInterface::res_t ZephyrKVStore::getBytes(const key_t& key, uint8_t buf[], size_t maxLen) const {
	return settings_load_one(key, buf, maxLen);
}

size_t ZephyrKVStore::getBytesLength(const key_t& key) const {
	return settings_get_val_len(key);
}

bool ZephyrKVStore::exists(const key_t& key) const {
	return settings_get_val_len(key) > 0;
}

#endif // ARDUINO_ARCH_ZEPHYR
