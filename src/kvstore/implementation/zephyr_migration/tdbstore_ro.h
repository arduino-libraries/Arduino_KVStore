/*
 * Read-only access to an Mbed OS TDBStore image from Zephyr.
 *
 * The on-flash format follows mbed-os TDBStore.cpp: two equal areas
 * (active/standby), each starting with a reserved area, then a master
 * record ("TDBS" key + version), then records of the form
 *   [24-byte header][pad to prog_size][key][data][pad to prog_size]
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef TDBSTORE_RO_H_
#define TDBSTORE_RO_H_

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* KVStore::MAX_KEY_SIZE: stored keys are at most 127 characters. */
#define TDBSTORE_RO_MAX_KEY_SIZE 128

/* User flags, as defined by Mbed's KVStore. */
#define TDBSTORE_RO_WRITE_ONCE_FLAG                (1UL << 0)
#define TDBSTORE_RO_REQUIRE_CONFIDENTIALITY_FLAG   (1UL << 1)
#define TDBSTORE_RO_REQUIRE_REPLAY_PROTECTION_FLAG (1UL << 3)

/* Size of TDBStore's reserved area (TDBStore::RESERVED_AREA_SIZE). */
#ifndef TDBSTORE_RO_RESERVED_AREA_SIZE
#define TDBSTORE_RO_RESERVED_AREA_SIZE 64
#endif

struct flash_area;

struct tdbstore_ro {
	const struct flash_area *fa;
	uint32_t prog_size;     /* program unit Mbed used when writing */
	uint32_t area_addr[2];  /* offsets of the two areas in the partition */
	uint32_t size;          /* usable size of one area */
	uint32_t master_offset; /* offset of the master record in an area */
	uint32_t end_offset;    /* end of the last valid record */
	uint8_t active_area;
	uint16_t version;       /* master record version of the active area */
};

struct tdbstore_ro_info {
	uint32_t data_size;     /* full size of the value */
	uint32_t flags;         /* TDBSTORE_RO_*_FLAG bits */
};

/* Return non-zero to stop iteration; that value is returned by foreach. */
typedef int (*tdbstore_ro_cb_t)(const char *key,
				const struct tdbstore_ro_info *info,
				void *user_data);

/**
 * Open a TDBStore image in a flash partition.
 *
 * The partition must cover exactly the same region as the Mbed
 * BlockDevice that TDBStore was given, because the split into the two
 * areas is computed from its size.
 *
 * @param area_id    Flash map partition ID (FIXED_PARTITION_ID(...)).
 * @param prog_size  Mbed BlockDevice program size, or 0 to auto-detect.
 * @param erase_size Mbed BlockDevice erase size, or 0 to use the
 *                   Zephyr flash page layout.
 *
 * @retval 0        Success.
 * @retval -ENODATA No valid TDBStore found (wrong region, empty or erased).
 * @retval <0       Other negative errno on flash errors.
 */
int tdbstore_ro_open(struct tdbstore_ro *store, uint8_t area_id,
		     uint32_t prog_size, uint32_t erase_size);

void tdbstore_ro_close(struct tdbstore_ro *store);

/**
 * Read the current value of a key.
 *
 * Copies up to buf_size bytes. *actual_size (optional) receives the
 * number of bytes copied; info->data_size (optional) the full size.
 *
 * @retval 0       Success.
 * @retval -ENOENT Key not present or deleted.
 */
int tdbstore_ro_get(const struct tdbstore_ro *store, const char *key,
		    void *buf, size_t buf_size, size_t *actual_size,
		    struct tdbstore_ro_info *info);

/**
 * Call cb for every live key, optionally only keys starting with prefix
 * (NULL or "" for all keys).
 */
int tdbstore_ro_foreach(const struct tdbstore_ro *store, const char *prefix,
			tdbstore_ro_cb_t cb, void *user_data);

#ifdef __cplusplus
}
#endif

#endif /* TDBSTORE_RO_H_ */
