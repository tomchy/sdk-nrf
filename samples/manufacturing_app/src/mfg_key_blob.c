/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "mfg_key_blob.h"

#include <bootutil/bootutil_public.h>
#include <errno.h>
#include <stddef.h>
#include <string.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/logging/log.h>
#include <zephyr/storage/flash_map.h>

LOG_MODULE_REGISTER(mfg_key_blob, LOG_LEVEL_INF);

#define MFG_IMAGE_PARTITION DT_CHOSEN(zephyr_code_partition)
#define MFG_FLASH           DEVICE_DT_GET(PARTITION_NODE_MTD(MFG_IMAGE_PARTITION))
#define MFG_PARTITION_BASE  PARTITION_NODE_OFFSET(MFG_IMAGE_PARTITION)

#ifndef CONFIG_NCS_MCUBOOT_MANUFACTURING_TLV_ID
#define MFG_MANUFACTURING_TLV_ID 0x00A2
#else
#define MFG_MANUFACTURING_TLV_ID CONFIG_NCS_MCUBOOT_MANUFACTURING_TLV_ID
#endif

/* Upper bound for the manufacturing TLV payload read from flash. */
#define MFG_TLV_PAYLOAD_MAX 1024U

BUILD_ASSERT(DT_NODE_EXISTS(MFG_IMAGE_PARTITION),
	     "zephyr,code-partition is required to locate the signed image");

static uint8_t tlv_payload[MFG_TLV_PAYLOAD_MAX];
static struct mfg_key_blob blob_cache;
static const struct mfg_area_digest *digest_cache;
static size_t digest_count;
static const struct mfg_key_blob *blob_ptr;
static bool blob_loaded;

static int partition_read(uint32_t part_off, void *dst, size_t len)
{
	int rc;

	rc = flash_read(MFG_FLASH, MFG_PARTITION_BASE + part_off, dst, len);
	return rc != 0 ? -EIO : 0;
}

static int find_protected_tlv(const struct image_header *hdr, uint16_t tlv_type,
			      uint32_t *payload_off, uint16_t *payload_len)
{
	const uint32_t area_off = hdr->ih_hdr_size + hdr->ih_img_size;
	struct image_tlv_info info;
	struct image_tlv tlv;
	uint32_t tlv_off;
	uint32_t prot_end;

	if (hdr->ih_protect_tlv_size == 0U) {
		return -ENOENT;
	}

	if (partition_read(area_off, &info, sizeof(info)) != 0) {
		return -EIO;
	}

	if (info.it_magic != IMAGE_TLV_PROT_INFO_MAGIC) {
		LOG_ERR("Missing protected TLV info magic at 0x%x", area_off);
		return -EINVAL;
	}

	if (info.it_tlv_tot != hdr->ih_protect_tlv_size) {
		LOG_ERR("Protected TLV size mismatch (hdr %u, info %u)",
			hdr->ih_protect_tlv_size, info.it_tlv_tot);
		return -EINVAL;
	}

	prot_end = area_off + hdr->ih_protect_tlv_size;
	tlv_off = area_off + sizeof(info);

	while (tlv_off + sizeof(tlv) <= prot_end) {
		if (partition_read(tlv_off, &tlv, sizeof(tlv)) != 0) {
			return -EIO;
		}

		if (tlv_off + sizeof(tlv) + tlv.it_len > prot_end) {
			LOG_ERR("Protected TLV entry overruns area");
			return -EINVAL;
		}

		if (tlv.it_type == tlv_type) {
			*payload_off = tlv_off + sizeof(tlv);
			*payload_len = tlv.it_len;
			return 0;
		}

		tlv_off += sizeof(tlv) + tlv.it_len;
	}

	return -ENOENT;
}

static const struct mfg_key_blob *parse_blob(const uint8_t *payload, uint16_t len)
{
	size_t digest_bytes;

	if (len < MFG_KEY_BLOB_FIXED_SIZE) {
		LOG_ERR("Manufacturing TLV too small (%u B)", len);
		return NULL;
	}

	memcpy(&blob_cache, payload, MFG_KEY_BLOB_FIXED_SIZE);

	if (blob_cache.magic != MFG_KEY_BLOB_MAGIC) {
		LOG_ERR("Bad magic 0x%08x (expected 0x%08x)",
			(unsigned int)blob_cache.magic,
			(unsigned int)MFG_KEY_BLOB_MAGIC);
		return NULL;
	}

	if (blob_cache.version != MFG_KEY_BLOB_VERSION) {
		LOG_ERR("Unsupported version %u (expected %u)",
			(unsigned int)blob_cache.version,
			(unsigned int)MFG_KEY_BLOB_VERSION);
		return NULL;
	}

	if (blob_cache.total_size < MFG_KEY_BLOB_FIXED_SIZE ||
	    blob_cache.total_size > len) {
		LOG_ERR("Bad total_size %u (payload %u, min %zu)",
			(unsigned int)blob_cache.total_size, len,
			MFG_KEY_BLOB_FIXED_SIZE);
		return NULL;
	}

	digest_bytes = blob_cache.total_size - MFG_KEY_BLOB_FIXED_SIZE;
	if ((digest_bytes % MFG_AREA_DIGEST_SIZE) != 0U) {
		LOG_ERR("Trailing digest area size %zu is not a multiple of %zu",
			digest_bytes, MFG_AREA_DIGEST_SIZE);
		return NULL;
	}

	digest_count = digest_bytes / MFG_AREA_DIGEST_SIZE;
	digest_cache = (const struct mfg_area_digest *)(payload + MFG_KEY_BLOB_FIXED_SIZE);

	return &blob_cache;
}

static const struct mfg_key_blob *load_blob(void)
{
	struct image_header hdr;
	uint32_t payload_off;
	uint16_t payload_len;
	int rc;

	if (!device_is_ready(MFG_FLASH)) {
		LOG_ERR("Flash device for code partition is not ready");
		return NULL;
	}

	rc = partition_read(0, &hdr, sizeof(hdr));
	if (rc != 0) {
		LOG_ERR("Failed to read image header: %d", rc);
		return NULL;
	}

	if (hdr.ih_magic != IMAGE_MAGIC) {
		LOG_ERR("Invalid image magic 0x%08x", hdr.ih_magic);
		return NULL;
	}

	rc = find_protected_tlv(&hdr, MFG_MANUFACTURING_TLV_ID, &payload_off, &payload_len);
	if (rc == -ENOENT) {
		LOG_ERR("Manufacturing TLV 0x%04x not found in protected area",
			MFG_MANUFACTURING_TLV_ID);
		return NULL;
	}
	if (rc != 0) {
		return NULL;
	}

	if (payload_len > sizeof(tlv_payload)) {
		LOG_ERR("Manufacturing TLV payload %u B exceeds buffer %zu B",
			payload_len, sizeof(tlv_payload));
		return NULL;
	}

	rc = partition_read(payload_off, tlv_payload, payload_len);
	if (rc != 0) {
		LOG_ERR("Failed to read manufacturing TLV payload: %d", rc);
		return NULL;
	}

	return parse_blob(tlv_payload, payload_len);
}

const struct mfg_key_blob *mfg_key_blob_get(void)
{
	if (!blob_loaded) {
		blob_ptr = load_blob();
		blob_loaded = true;
	}
	return blob_ptr;
}

size_t mfg_key_blob_digest_count(void)
{
	if (!blob_loaded) {
		(void)mfg_key_blob_get();
	}
	return digest_count;
}

const struct mfg_area_digest *mfg_key_blob_digest_get(size_t index)
{
	if (!blob_loaded) {
		(void)mfg_key_blob_get();
	}
	if (index >= digest_count) {
		return NULL;
	}
	return &digest_cache[index];
}
