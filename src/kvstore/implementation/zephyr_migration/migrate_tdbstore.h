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

#ifdef __cplusplus
extern "C" {
#endif

enum migrate_result {
	MIGRATE_OK = 0,                 /* data migrated (or resumed and finished) */
	MIGRATE_TDBSTORE_NOT_PRESENT = 1, /* nothing to migrate */
};

bool has_tdb_store(void);

/*
 * Run before settings_subsys_init()/settings_load() and before anything
 * else touches the settings partition. The filesystem holding the backup
 * file must already be mounted.
 *
 * Returns an enum migrate_result value, or a negative errno on failure.
 * -EBADMSG means the backup file is corrupt and the TDBStore is already
 * gone, so the data can't be recovered automatically.
 */
int migrate(void);

#ifdef __cplusplus
}
#endif
