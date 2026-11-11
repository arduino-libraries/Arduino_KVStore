/*
 * This file is part of Arduino_KVStore.
 *
 * Copyright (c) 2024 Arduino SA
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "config.h"

#if (defined(ARDUINO_PORTENTA_H7_M7) || defined(ARDUINO_NICLA_VISION) \
    || defined(ARDUINO_OPTA) || defined(ARDUINO_GIGA)) && defined(ARDUINO_ARCH_ZEPHYR) \
    && ZEPHYR_MIGRATE_TDBSTORE

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <zephyr/devicetree.h>
#include <zephyr/fs/fs.h>
#include <zephyr/settings/settings.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/sys/crc.h>
#include <zephyr/sys/util.h>

#include "tdbstore_ro.h"
#include "migrate_tdbstore.h"

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(sketch, LOG_LEVEL_INF);

#if DT_HAS_CHOSEN(zephyr_settings_partition)
#define SETTINGS_PARTITION DT_PARTITION_ID(DT_CHOSEN(zephyr_settings_partition))
#else
#error "No settings partition is defined, unable to migrate kvstore"
#endif

#ifndef MIGRATE_BACKUP_FILE
#define MIGRATE_BACKUP_FILE "/ota:/tdbstore_migration.bak"
#endif
#ifndef MIGRATE_BACKUP_TMP
#define MIGRATE_BACKUP_TMP "/ota:/tdbstore_migration.tmp"
#endif

/* Settings name for a TDBStore key: "<prefix>/<key>". */
#ifndef MIGRATE_SETTINGS_PREFIX
#define MIGRATE_SETTINGS_PREFIX ""
#endif

/* Largest value accepted; keep it well below the NVS sector size. */
#ifndef MIGRATE_MAX_VALUE_SIZE
#define MIGRATE_MAX_VALUE_SIZE 256
#endif

/* Mbed BlockDevice program/erase size; 0 = auto-detect / flash layout. */
#ifndef MIGRATE_TDB_PROG_SIZE
#define MIGRATE_TDB_PROG_SIZE 0
#endif
#ifndef MIGRATE_TDB_ERASE_SIZE
#define MIGRATE_TDB_ERASE_SIZE 0
#endif

/*
 * Backup file layout (native little-endian):
 *   backup_header
 *   N x { backup_record_header, key bytes (no NUL), value bytes }
 *   uint32_t crc32_ieee over everything above
 */
#define BACKUP_MAGIC   0x4D424454u /* "TDBM" */
#define BACKUP_VERSION 1u
#define FOOTER_SIZE    (sizeof(uint32_t))

static struct tdbstore_ro store;

bool has_tdb_store(void)
{
	return tdbstore_ro_open(&store, SETTINGS_PARTITION,
				MIGRATE_TDB_PROG_SIZE,
				MIGRATE_TDB_ERASE_SIZE) == 0;
}

struct backup_header {
	uint32_t magic;
	uint32_t version;
};

struct backup_record_header {
	uint32_t key_len;
	uint32_t value_len;
};

struct backup_ctx {
	struct fs_file_t *file;
	uint32_t crc;
};

static int write_all(struct fs_file_t *f, const void *buf, size_t len,
		     uint32_t *crc)
{
	ssize_t n = fs_write(f, buf, len);

	if (n < 0) {
		return (int)n;
	}
	if ((size_t)n != len) {
		return -ENOSPC;
	}
	if (crc) {
		*crc = crc32_ieee_update(*crc, (const uint8_t *)buf, len);
	}
	return 0;
}

static int read_all(struct fs_file_t *f, void *buf, size_t len, uint32_t *crc)
{
	ssize_t n = fs_read(f, buf, len);

	if (n < 0) {
		return (int)n;
	}
	if ((size_t)n != len) {
		return -EBADMSG;
	}
	if (crc) {
		*crc = crc32_ieee_update(*crc, (const uint8_t *)buf, len);
	}
	return 0;
}

static int make_settings_name(char *out, size_t out_size, const char *key)
{
	int n;

	if (strchr(key, SETTINGS_NAME_END) != NULL) {
		return -EINVAL;
	}

	if (strlen(MIGRATE_SETTINGS_PREFIX) == 0) {
		n = snprintf(out, out_size, "%s", key);
	} else {
		n = snprintf(out, out_size, "%s%c%s", MIGRATE_SETTINGS_PREFIX,
			SETTINGS_NAME_SEPARATOR, key);
	}

	if (n < 0 || (size_t)n >= out_size || n > SETTINGS_MAX_NAME_LEN) {
		return -ENAMETOOLONG;
	}

	return 0;
}

/* tdbstore_ro_foreach() callback: append one key to the backup file. */
static int handle_key(const char *key, const struct tdbstore_ro_info *info,
		      void *user_data)
{
	struct backup_ctx *ctx = (struct backup_ctx *)user_data;
	struct backup_record_header rh;
	char name[SETTINGS_MAX_NAME_LEN + 1];
	uint8_t *value = NULL;
	size_t len = 0;
	int rc;

	/*
	 * Reject anything that couldn't be restored now, while the TDBStore
	 * is still intact, rather than after the partition is erased.
	 */
	rc = make_settings_name(name, sizeof(name), key);
	if (rc) {
		LOG_ERR("key \"%s\" can't be mapped to a settings name (%d)",
			key, rc);
		goto out;
	}
	if (info->data_size > MIGRATE_MAX_VALUE_SIZE) {
		LOG_ERR("key \"%s\": value too large (%u bytes)", key,
			info->data_size);
		rc = -EFBIG;
		goto out;
	}
	if (info->data_size == 0) {
		/* Settings treats an empty value as a delete. */
		LOG_WRN("key \"%s\" has an empty value, skipping", key);
		goto out;
	}

	value = (uint8_t *)malloc(info->data_size);
	if (!value) {
		rc = -ENOMEM;
		goto out;
	}
	rc = tdbstore_ro_get(&store, key, value, info->data_size, &len, NULL);
	if (rc) {
		goto out;
	}
	if (len != info->data_size) {
		rc = -EIO;
		goto out;
	}

	rh.key_len = (uint32_t)strlen(key);
	rh.value_len = info->data_size;
	rc = write_all(ctx->file, &rh, sizeof(rh), &ctx->crc);
	if (rc != 0) {
		goto out;
	}

	rc = write_all(ctx->file, key, rh.key_len, &ctx->crc);
	if (rc != 0) {
		goto out;
	}

	rc = write_all(ctx->file, value, rh.value_len, &ctx->crc);
	if (rc != 0) {
		goto out;
	}
out:
	free(value);

	return rc; /* non-zero stops the iteration */
}

static int create_backup(const char *path)
{
	struct fs_file_t file;
	struct backup_ctx ctx = { &file, 0 };
	struct backup_header hdr = { BACKUP_MAGIC, BACKUP_VERSION };
	int rc, rc2;

	fs_file_t_init(&file);
	rc = fs_open(&file, path, FS_O_CREATE | FS_O_WRITE);
	if (rc) {
		return rc;
	}

	rc = write_all(&file, &hdr, sizeof(hdr), &ctx.crc);
	if (rc != 0) {
		goto exit;
	}

	rc = tdbstore_ro_foreach(&store, NULL, handle_key, &ctx);
	if (rc != 0) {
		goto exit;
	}

	rc = write_all(&file, &ctx.crc, sizeof(ctx.crc), NULL);
	if (rc != 0) {
		goto exit;
	}

	rc = fs_sync(&file);
	if (rc != 0) {
		goto exit;
	}

exit:
	rc2 = fs_close(&file);
	return rc ? rc : rc2;
}

static int erase_settings_partition(void)
{
	const struct flash_area *fa;
	int rc;

	rc = flash_area_open(SETTINGS_PARTITION, &fa);
	if (rc) {
		return rc;
	}
	rc = flash_area_erase(fa, 0, fa->fa_size);
	flash_area_close(fa);
	return rc;
}

/* Check the CRC32 over the whole file except its trailing 4-byte CRC. */
static int verify_backup(const char *path)
{
	struct fs_dirent entry;
	struct fs_file_t file;
	struct backup_header hdr;
	uint8_t chunk[256];
	uint32_t crc = 0, stored_crc;
	size_t remaining;
	int rc, rc2;

	rc = fs_stat(path, &entry);
	if (rc != 0) {
		return rc;
	}
	if (entry.size < sizeof(struct backup_header) + FOOTER_SIZE) {
		return -EBADMSG;
	}

	fs_file_t_init(&file);
	rc = fs_open(&file, path, FS_O_READ);
	if (rc != 0) {
		return rc;
	}

	rc = read_all(&file, &hdr, sizeof(hdr), &crc);
	if (rc != 0) {
		goto exit;
	}
	if (hdr.magic != BACKUP_MAGIC || hdr.version != BACKUP_VERSION) {
		rc = -EBADMSG;
		goto exit;
	}

	remaining = entry.size - (sizeof(stored_crc) + sizeof(hdr));

	while (rc == 0 && remaining > 0) {
		size_t n = MIN(remaining, sizeof(chunk));

		rc = read_all(&file, chunk, n, &crc);
		remaining -= n;
	}
	if (rc != 0) {
		goto exit;
	}

	rc = read_all(&file, &stored_crc, sizeof(stored_crc), NULL);
	if (rc != 0 || stored_crc != crc) {
		rc = -EBADMSG;
	}

exit:
	rc2 = fs_close(&file);
	return rc ? rc : rc2;
}

static int perform_migration(const char *path)
{
	struct fs_file_t file;
	struct fs_dirent entry;
	char key[TDBSTORE_RO_MAX_KEY_SIZE + 1];
	char name[SETTINGS_MAX_NAME_LEN + 1];
	uint8_t *value = NULL;
	int rc, rc2;
	int remaining;

	rc = fs_stat(path, &entry);
	if (rc) {
		return rc;
	}

	fs_file_t_init(&file);
	rc = fs_open(&file, path, FS_O_READ);
	if (rc != 0) {
		return rc;
	}

	rc = fs_seek(&file, sizeof(struct backup_header), FS_SEEK_SET);
	if (rc != 0) {
		goto exit;
	}

	remaining = entry.size - (sizeof(uint32_t) + sizeof(struct backup_header));

	while (remaining > 0) {
		struct backup_record_header r_hdr;
		rc = read_all(&file, &r_hdr, sizeof(r_hdr), NULL);
		if (rc != 0) {
			goto exit;
		}

		if (r_hdr.key_len == 0 || r_hdr.key_len > TDBSTORE_RO_MAX_KEY_SIZE ||
		    r_hdr.value_len == 0 || r_hdr.value_len > MIGRATE_MAX_VALUE_SIZE ||
		    sizeof(r_hdr) + r_hdr.key_len + r_hdr.value_len > (size_t)remaining) {
			rc = -EBADMSG;
			goto exit;
		}
		rc = read_all(&file, key, r_hdr.key_len, NULL);
		if (rc != 0) {
			goto exit;
		}
		key[r_hdr.key_len] = '\0';

		value = (uint8_t *) malloc(r_hdr.value_len);
		if (value == NULL) {
			rc = -ENOMEM;
			goto exit;
		}

		rc = read_all(&file, value, r_hdr.value_len, NULL);
		if (rc != 0) {
			goto exit;
		}

		rc = make_settings_name(name, sizeof(name), key);
		if (rc != 0) {
			goto exit;
		}

		rc = settings_save_one(name, value, r_hdr.value_len);
		free(value);
		value = NULL;
		if (rc != 0) {
			goto exit;
		}

		remaining -= sizeof(r_hdr) + r_hdr.key_len + r_hdr.value_len;
	}

	if (remaining < 0) {
		rc = -EBADMSG;
	}
exit:
	rc2 = fs_close(&file);

	if (value != NULL) {
		free(value);
		value = NULL;
	}
	return rc ? rc : rc2;
}

int migrate(void)
{
	struct fs_dirent entry;
	int rc;

	/* A temp file means a backup was interrupted; TDBStore is intact. */
	rc = fs_unlink(MIGRATE_BACKUP_TMP);
	if (rc && rc != -ENOENT) {
		return rc;
	}

	rc = fs_stat(MIGRATE_BACKUP_FILE, &entry);
	if (rc == -ENOENT) {
		// backup file not present, check if tdb store exists
		if (!has_tdb_store()) {
			return MIGRATE_TDBSTORE_NOT_PRESENT;
		}

		// we create the backup file
		rc = create_backup(MIGRATE_BACKUP_TMP);
		tdbstore_ro_close(&store);
		if (rc != 0) {
			return rc;
		}

		// everything checks, let's make the backup file ready
		rc = fs_rename(MIGRATE_BACKUP_TMP, MIGRATE_BACKUP_FILE);
		if (rc) {
			LOG_ERR("committing backup failed (%d)", rc);
			return rc;
		}
	} else if (rc != 0) {
		return rc;
	}

	// we verify that the backup file checks with crc32
	rc = verify_backup(MIGRATE_BACKUP_FILE);
	if (rc) {
		LOG_ERR("backup verify failed (%d)", rc);
		if (!has_tdb_store()) {
		/* TDBStore already erased: keep the backup for manual recovery. */
		return -EBADMSG;
		}
		/* TDBStore still intact: drop the bad backup, retry on next call. */
		tdbstore_ro_close(&store);
		rc = fs_unlink(MIGRATE_BACKUP_FILE);
		return rc ? rc : -EAGAIN;
	}

	rc = erase_settings_partition();
	if (rc) {
		LOG_ERR("erasing settings partition failed (%d)", rc);
		return rc;
	}
	rc = settings_subsys_init();
	if (rc) {
		LOG_ERR("settings_subsys_init failed (%d)", rc);
		return rc;
	}
	rc = perform_migration(MIGRATE_BACKUP_FILE);
	if (rc) {
		LOG_ERR("restoring from backup failed (%d)", rc);
		return rc;
	}
	rc = fs_unlink(MIGRATE_BACKUP_FILE);
	if (rc) {
		LOG_ERR("removing backup failed (%d)", rc);
		return rc;
	}
	LOG_INF("migration complete");
	rc = MIGRATE_OK;

	return rc;
}

#endif // (defined(ARDUINO_PORTENTA_H7_M7) || defined(ARDUINO_NICLA_VISION) \
       // || defined(ARDUINO_OPTA) || defined(ARDUINO_GIGA)) && defined(ARDUINO_ARCH_ZEPHYR)
