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
#include <string.h>

#include <zephyr/storage/flash_map.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/util.h>
#include <zephyr/logging/log.h>

#include "tdbstore_ro.h"

LOG_MODULE_REGISTER(sketch, LOG_LEVEL_ERR);

#define TDB_MAGIC           0x54686683u /* tdbstore_magic */
#define TDB_HDR_SIZE        24u         /* sizeof(record_header_t) */
#define TDB_HDR_CRC_LEN     20u         /* header bytes covered by the CRC */
#define TDB_DELETE_FLAG     (1UL << 31)
#define TDB_MASTER_KEY      "TDBS"
#define TDB_MASTER_KEY_LEN  4u
#define TDB_TRAILER_SIZE    8u          /* sizeof(reserved_trailer_t) */
#define TDB_MAX_TOTAL_SIZE  0x80000000u /* TDBStore can't exceed 2 GB */
#define TDB_CHUNK           64u

/* record_header_t field offsets (little-endian, no padding) */
#define HDR_MAGIC     0
#define HDR_FLAGS     8
#define HDR_KEY_SIZE  12
#define HDR_DATA_SIZE 16
#define HDR_CRC       20

struct tdb_rec {
	char key[TDBSTORE_RO_MAX_KEY_SIZE];
	uint32_t key_size;
	uint32_t data_size;
	uint32_t flags;
	uint32_t data_off; /* value offset within the area */
	uint32_t next_off; /* next record offset within the area */
};

static uint32_t align_up(uint32_t val, uint32_t size)
{
	return ((val + size - 1) / size) * size;
}

/* Reflected CRC-32 (poly 0x04C11DB7) register update, no pre/post XOR. */
static uint32_t crc_update(uint32_t reg, const uint8_t *p, size_t len)
{
	static const uint32_t tbl[16] = {
		0x00000000, 0x1DB71064, 0x3B6E20C8, 0x26D930AC,
		0x76DC4190, 0x6B6B51F4, 0x4DB26158, 0x5005713C,
		0xEDB88320, 0xF00F9344, 0xD6D6A3E8, 0xCB61B38C,
		0x9B64C2B0, 0x86D3D2D4, 0xA00AE278, 0xBDBDF21C,
	};

	while (len--) {
		reg ^= *p++;
		reg = (reg >> 4) ^ tbl[reg & 0x0F];
		reg = (reg >> 4) ^ tbl[reg & 0x0F];
	}
	return reg;
}

static uint32_t bitrev32(uint32_t x)
{
	x = ((x >> 1) & 0x55555555u) | ((x & 0x55555555u) << 1);
	x = ((x >> 2) & 0x33333333u) | ((x & 0x33333333u) << 2);
	x = ((x >> 4) & 0x0F0F0F0Fu) | ((x & 0x0F0F0F0Fu) << 4);
	x = ((x >> 8) & 0x00FF00FFu) | ((x & 0x00FF00FFu) << 8);
	return (x >> 16) | (x << 16);
}

static int area_read(const struct tdbstore_ro *s, uint8_t area, uint32_t off,
		     void *buf, size_t len)
{
	if ((uint64_t)off + len > s->size) {
		return -EBADMSG;
	}
	return flash_area_read(s->fa, s->area_addr[area] + off, buf, len);
}

/*
 * Parse the record at off, mirroring TDBStore::read_record().
 * Returns -EBADMSG if there is no valid record here (end of log).
 */
static int read_record(const struct tdbstore_ro *s, uint8_t area, uint32_t off,
		       bool verify, struct tdb_rec *rec)
{
	uint8_t hdr[TDB_HDR_SIZE];
	uint8_t chunk[TDB_CHUNK];
	uint32_t stored_crc, reg, pos, remaining, total;
	int rc;

	rc = area_read(s, area, off, hdr, sizeof(hdr));
	if (rc) {
		return rc;
	}
	if (sys_get_le32(&hdr[HDR_MAGIC]) != TDB_MAGIC) {
		return -EBADMSG;
	}

	rec->flags = sys_get_le32(&hdr[HDR_FLAGS]);
	rec->key_size = sys_get_le16(&hdr[HDR_KEY_SIZE]);
	rec->data_size = sys_get_le32(&hdr[HDR_DATA_SIZE]);
	stored_crc = sys_get_le32(&hdr[HDR_CRC]);

	if (rec->key_size == 0 || rec->key_size >= TDBSTORE_RO_MAX_KEY_SIZE) {
		return -EBADMSG;
	}

	off += align_up(TDB_HDR_SIZE, s->prog_size);
	total = rec->key_size + rec->data_size;
	if (total < rec->data_size || (uint64_t)off + total >= s->size) {
		return -EBADMSG;
	}

	rc = area_read(s, area, off, rec->key, rec->key_size);
	if (rc) {
		return rc;
	}
	rec->key[rec->key_size] = '\0';
	rec->data_off = off + rec->key_size;
	rec->next_off = align_up(off + total, s->prog_size);

	if (!verify) {
		return 0;
	}

	/* CRC over header (minus crc field), key and data. */
	reg = crc_update(0xFFFFFFFFu, hdr, TDB_HDR_CRC_LEN);
	reg = crc_update(reg, (const uint8_t *)rec->key, rec->key_size);
	pos = rec->data_off;
	remaining = rec->data_size;
	while (remaining) {
		uint32_t n = MIN(remaining, (uint32_t)sizeof(chunk));

		rc = area_read(s, area, pos, chunk, n);
		if (rc) {
			return rc;
		}
		reg = crc_update(reg, chunk, n);
		pos += n;
		remaining -= n;
	}

	/*
	 * Mbed uses MbedCRC<POLY_32BIT_ANSI>(init 0xFFFFFFFF, final xor 0,
	 * reflect data, no reflected remainder). Depending on how the Mbed
	 * version maps that to the register, the stored value is the raw
	 * register or its bit-reversal, so accept either.
	 */
	if (stored_crc != reg && stored_crc != bitrev32(reg)) {
		return -EBADMSG;
	}
	return 0;
}

static int read_master(const struct tdbstore_ro *s, uint8_t area,
		       uint16_t *version)
{
	struct tdb_rec rec;
	uint8_t ver[2];
	int rc;

	rc = read_record(s, area, s->master_offset, true, &rec);
	if (rc) {
		return rc;
	}
	if (rec.key_size != TDB_MASTER_KEY_LEN ||
	    memcmp(rec.key, TDB_MASTER_KEY, TDB_MASTER_KEY_LEN) != 0 ||
	    rec.data_size < sizeof(ver)) {
		return -EBADMSG;
	}
	rc = area_read(s, area, rec.data_off, ver, sizeof(ver));
	if (rc) {
		return rc;
	}
	*version = sys_get_le16(ver);
	return 0;
}

/* Same selection rules as TDBStore::init(). */
static int select_active(struct tdbstore_ro *s)
{
	uint16_t ver[2] = {0, 0};
	bool valid[2];

	for (uint8_t a = 0; a < 2; a++) {
		int rc = read_master(s, a, &ver[a]);

		if (rc && rc != -EBADMSG) {
			return rc;
		}
		valid[a] = (rc == 0);
	}

	if (!valid[0] && !valid[1]) {
		return -ENODATA;
	}
	if (valid[0] && valid[1]) {
		s->active_area = (ver[0] > ver[1] || ver[0] == 0) ? 0 : 1;
	} else {
		s->active_area = valid[0] ? 0 : 1;
	}
	s->version = ver[s->active_area];
	return 0;
}

/* Walk the log until the first invalid record, like build_ram_table(). */
static int scan_area(const struct tdbstore_ro *s, uint8_t area,
		     uint32_t *count, uint32_t *end)
{
	struct tdb_rec rec;
	uint32_t off = s->master_offset;
	uint32_t n = 0;

	while (off < s->size) {
		int rc = read_record(s, area, off, true, &rec);

		if (rc == -EBADMSG) {
			break;
		}
		if (rc) {
			return rc;
		}
		n++;
		off = rec.next_off;
	}
	*count = n;
	*end = off;
	return 0;
}

/* Area split from TDBStore::calc_area_params(). */
static int compute_areas(struct tdbstore_ro *s, uint32_t erase_size)
{
	uint32_t total = (uint32_t)MIN((uint64_t)s->fa->fa_size,
				       (uint64_t)TDB_MAX_TOTAL_SIZE);
	uint32_t area0 = 0;

	while (area0 < total / 2) {
		if (erase_size) {
			area0 += erase_size;
		} else {
			struct flash_pages_info info;
			int rc = flash_get_page_info_by_offs(
				flash_area_get_device(s->fa),
				s->fa->fa_off + area0, &info);

			if (rc) {
				return rc;
			}
			area0 = (uint32_t)(info.start_offset + info.size -
					   s->fa->fa_off);
		}
	}
	if (area0 >= total) {
		return -EINVAL;
	}

	s->area_addr[0] = 0;
	s->area_addr[1] = area0;
	s->size = MIN(area0, total - area0);
	return 0;
}

static int try_prog_size(struct tdbstore_ro *s, uint32_t prog_size,
			 uint32_t *count)
{
	int rc;

	s->prog_size = prog_size;
	s->master_offset = align_up(TDBSTORE_RO_RESERVED_AREA_SIZE +
				    TDB_TRAILER_SIZE, prog_size);
	if (s->master_offset + TDB_HDR_SIZE >= s->size) {
		return -ENODATA;
	}
	rc = select_active(s);
	if (rc) {
		return rc;
	}
	return scan_area(s, s->active_area, count, &s->end_offset);
}

int tdbstore_ro_open(struct tdbstore_ro *s, uint8_t area_id,
		     uint32_t prog_size, uint32_t erase_size)
{
	static const uint32_t candidates[] = {
		1, 2, 4, 8, 16, 32, 64, 128, 256
	};
	uint32_t count = 0;
	int rc;

	memset(s, 0, sizeof(*s));
	rc = flash_area_open(area_id, &s->fa);
	if (rc) {
		return rc;
	}

	rc = compute_areas(s, erase_size);
	if (rc) {
		goto fail;
	}

	if (prog_size == 0) {
		/* Pick the program size that parses the most valid records. */
		uint32_t best = 0, best_count = 0;

		for (size_t i = 0; i < ARRAY_SIZE(candidates); i++) {
			rc = try_prog_size(s, candidates[i], &count);
			if (rc == 0 && count > best_count) {
				best = candidates[i];
				best_count = count;
			} else if (rc && rc != -ENODATA) {
				goto fail;
			}
		}
		if (!best) {
			rc = -ENODATA;
			goto fail;
		}
		prog_size = best;
		LOG_INF("auto-detected program size %u", prog_size);
	}

	rc = try_prog_size(s, prog_size, &count);
	if (rc) {
		goto fail;
	}

	LOG_INF("TDBStore: area %u active (version %u), %u records, %u/%u bytes used",
		s->active_area, s->version, count, s->end_offset, s->size);
	return 0;

fail:
	flash_area_close(s->fa);
	s->fa = NULL;
	return rc;
}

void tdbstore_ro_close(struct tdbstore_ro *s)
{
	if (s->fa) {
		flash_area_close(s->fa);
		s->fa = NULL;
	}
}

static bool key_eq(const struct tdb_rec *rec, const char *key, size_t len)
{
	return rec->key_size == len && memcmp(rec->key, key, len) == 0;
}

/*
 * The region [master_offset, end_offset) was CRC-checked at open, so
 * later walks skip verification.
 */
int tdbstore_ro_get(const struct tdbstore_ro *s, const char *key,
		    void *buf, size_t buf_size, size_t *actual_size,
		    struct tdbstore_ro_info *info)
{
	struct tdb_rec rec;
	size_t klen;
	bool found = false;
	uint32_t data_off = 0, data_size = 0, flags = 0;
	uint32_t off;
	size_t n;
	int rc;

	if (!s->fa || !key) {
		return -EINVAL;
	}
	klen = strlen(key);
	if (klen == 0 || klen >= TDBSTORE_RO_MAX_KEY_SIZE) {
		return -EINVAL;
	}

	/* The newest record for a key wins; a delete record hides it. */
	for (off = s->master_offset; off < s->end_offset; off = rec.next_off) {
		rc = read_record(s, s->active_area, off, false, &rec);
		if (rc) {
			return rc;
		}
		if (off != s->master_offset && key_eq(&rec, key, klen)) {
			found = !(rec.flags & TDB_DELETE_FLAG);
			data_off = rec.data_off;
			data_size = rec.data_size;
			flags = rec.flags & ~TDB_DELETE_FLAG;
		}
	}
	if (!found) {
		return -ENOENT;
	}

	n = MIN(buf_size, (size_t)data_size);
	if (n) {
		if (!buf) {
			return -EINVAL;
		}
		rc = area_read(s, s->active_area, data_off, buf, n);
		if (rc) {
			return rc;
		}
	}
	if (actual_size) {
		*actual_size = n;
	}
	if (info) {
		info->data_size = data_size;
		info->flags = flags;
	}
	return 0;
}

int tdbstore_ro_foreach(const struct tdbstore_ro *s, const char *prefix,
			tdbstore_ro_cb_t cb, void *user_data)
{
	struct tdb_rec rec, later;
	size_t plen = prefix ? strlen(prefix) : 0;
	uint32_t off, o2;
	int rc;

	if (!s->fa || !cb) {
		return -EINVAL;
	}

	for (off = s->master_offset; off < s->end_offset; off = rec.next_off) {
		bool superseded = false;

		rc = read_record(s, s->active_area, off, false, &rec);
		if (rc) {
			return rc;
		}
		if (off == s->master_offset || (rec.flags & TDB_DELETE_FLAG)) {
			continue;
		}
		if (plen && (rec.key_size < plen ||
			     memcmp(rec.key, prefix, plen) != 0)) {
			continue;
		}

		/* Only report a key at its newest record. */
		for (o2 = rec.next_off; o2 < s->end_offset; o2 = later.next_off) {
			rc = read_record(s, s->active_area, o2, false, &later);
			if (rc) {
				return rc;
			}
			if (key_eq(&later, rec.key, rec.key_size)) {
				superseded = true;
				break;
			}
		}

		if (!superseded) {
			struct tdbstore_ro_info info = {
				.data_size = rec.data_size,
				.flags = rec.flags,
			};

			rc = cb(rec.key, &info, user_data);
			if (rc) {
				return rc;
			}
		}
	}
	return 0;
}

#endif // (defined(ARDUINO_PORTENTA_H7_M7) || defined(ARDUINO_NICLA_VISION) \
       // || defined(ARDUINO_OPTA) || defined(ARDUINO_GIGA)) && defined(ARDUINO_ARCH_ZEPHYR)
