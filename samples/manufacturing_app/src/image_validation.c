/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * Step 3: Image digest validation.
 */

#include "image_validation.h"
#include "mfg_key_blob.h"
#include "mfg_log.h"
#include "recovery.h"

#include <bootutil/bootutil_public.h>
#include <errno.h>
#include <string.h>
#include <psa/crypto.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/sys/util.h>

#define HASH_CHUNK_SIZE      1024U
#define IMAGE_SHA512_HASH_LEN 64U

#define FLASH_DEV      DEVICE_DT_GET(DT_CHOSEN(zephyr_flash))
#define CODE_PARTITION DT_CHOSEN(zephyr_code_partition)
#define CODE_FLASH     DEVICE_DT_GET(PARTITION_NODE_MTD(CODE_PARTITION))
#define CODE_PART_OFF  PARTITION_NODE_OFFSET(CODE_PARTITION)

BUILD_ASSERT(DT_NODE_EXISTS(DT_CHOSEN(zephyr_flash)),
	     "zephyr,flash chosen node is required for digest verification");
BUILD_ASSERT(DT_NODE_EXISTS(CODE_PARTITION),
	     "zephyr,code-partition is required for manufacturing app self-check");

static int flash_read_abs(uint32_t address, void *dst, size_t len)
{
	const struct device *flash = CODE_FLASH;

	if (!device_is_ready(flash)) {
		return -ENODEV;
	}

	return flash_read(flash, address, dst, len) != 0 ? -EIO : 0;
}

static int flash_read_image(uint32_t img_off, void *dst, size_t len)
{
	const struct device *flash = CODE_FLASH;

	if (!device_is_ready(flash)) {
		return -ENODEV;
	}

	return flash_read(flash, CODE_PART_OFF + img_off, dst, len) != 0 ? -EIO : 0;
}

static int hash_flash_region(psa_hash_operation_t *operation, uint32_t address, uint32_t size)
{
	static uint8_t chunk[HASH_CHUNK_SIZE];
	psa_status_t status;

	for (uint32_t off = 0; off < size; off += HASH_CHUNK_SIZE) {
		const size_t chunk_len = MIN(HASH_CHUNK_SIZE, size - off);
		int rc;

		rc = flash_read_abs(address + off, chunk, chunk_len);
		if (rc != 0) {
			return rc;
		}

		status = psa_hash_update(operation, chunk, chunk_len);
		if (status != PSA_SUCCESS) {
			MFG_LOG_ERR("psa_hash_update failed: %d\n", (int)status);
			return -EIO;
		}
	}

	return 0;
}

static int hash_image_region(psa_hash_operation_t *operation, uint32_t img_off, uint32_t size)
{
	static uint8_t chunk[HASH_CHUNK_SIZE];
	psa_status_t status;

	for (uint32_t off = 0; off < size; off += HASH_CHUNK_SIZE) {
		const size_t chunk_len = MIN(HASH_CHUNK_SIZE, size - off);
		int rc;

		rc = flash_read_image(img_off + off, chunk, chunk_len);
		if (rc != 0) {
			return rc;
		}

		status = psa_hash_update(operation, chunk, chunk_len);
		if (status != PSA_SUCCESS) {
			MFG_LOG_ERR("psa_hash_update failed: %d\n", (int)status);
			return -EIO;
		}
	}

	return 0;
}

static int verify_flash_digest(uint32_t address, uint32_t size, const uint8_t *expected)
{
	psa_hash_operation_t operation = PSA_HASH_OPERATION_INIT;
	psa_status_t status;
	int rc;

	status = psa_hash_setup(&operation, PSA_ALG_SHA_256);
	if (status != PSA_SUCCESS) {
		MFG_LOG_ERR("psa_hash_setup failed: %d\n", (int)status);
		return -EIO;
	}

	rc = hash_flash_region(&operation, address, size);
	if (rc != 0) {
		psa_hash_abort(&operation);
		return rc;
	}

	status = psa_hash_verify(&operation, expected, MFG_DIGEST_LEN);
	if (status == PSA_SUCCESS) {
		return 0;
	}

	if (status == PSA_ERROR_INVALID_SIGNATURE) {
		return -EINVAL;
	}

	MFG_LOG_ERR("psa_hash_verify failed: %d\n", (int)status);
	psa_hash_abort(&operation);
	return -EIO;
}

static int find_image_tlv(const struct image_header *hdr, uint16_t tlv_type,
			  uint8_t *payload, size_t payload_max, uint16_t *payload_len)
{
	const uint32_t area_off = hdr->ih_hdr_size + hdr->ih_img_size;
	struct image_tlv_info info;
	struct image_tlv tlv;
	uint32_t tlv_off;
	uint32_t tlv_end;
	uint32_t prot_end;
	int rc;

	rc = flash_read_image(area_off, &info, sizeof(info));
	if (rc != 0) {
		return rc;
	}

	if (info.it_magic == IMAGE_TLV_PROT_INFO_MAGIC) {
		if (hdr->ih_protect_tlv_size != info.it_tlv_tot) {
			return -EINVAL;
		}

		prot_end = area_off + hdr->ih_protect_tlv_size;
		rc = flash_read_image(prot_end, &info, sizeof(info));
		if (rc != 0) {
			return rc;
		}

		tlv_end = area_off + hdr->ih_protect_tlv_size + info.it_tlv_tot;
		tlv_off = area_off + sizeof(info);
	} else {
		if (hdr->ih_protect_tlv_size != 0U) {
			return -EINVAL;
		}
		if (info.it_magic != IMAGE_TLV_INFO_MAGIC) {
			return -EINVAL;
		}

		prot_end = area_off;
		tlv_end = area_off + info.it_tlv_tot;
		tlv_off = area_off + sizeof(info);
	}

	while (tlv_off < tlv_end) {
		if (hdr->ih_protect_tlv_size > 0U && tlv_off == prot_end) {
			tlv_off += sizeof(struct image_tlv_info);
		}

		if (tlv_off + sizeof(tlv) > tlv_end) {
			return -EINVAL;
		}

		rc = flash_read_image(tlv_off, &tlv, sizeof(tlv));
		if (rc != 0) {
			return rc;
		}

		if (tlv_off + sizeof(tlv) + tlv.it_len > tlv_end) {
			return -EINVAL;
		}

		if (tlv.it_type == tlv_type) {
			if (tlv.it_len > payload_max) {
				return -ENOSPC;
			}

			rc = flash_read_image(tlv_off + sizeof(tlv), payload, tlv.it_len);
			if (rc != 0) {
				return rc;
			}

			*payload_len = tlv.it_len;
			return 0;
		}

		tlv_off += sizeof(tlv) + tlv.it_len;
	}

	return -ENOENT;
}

static int verify_mcuboot_image_hash(const struct image_header *hdr,
				     psa_algorithm_t alg, const uint8_t *expected,
				     size_t expected_len)
{
	psa_hash_operation_t operation = PSA_HASH_OPERATION_INIT;
	psa_status_t status;
	const uint32_t img_hash_len = hdr->ih_hdr_size + hdr->ih_img_size +
				      hdr->ih_protect_tlv_size;
	int rc;

	status = psa_hash_setup(&operation, alg);
	if (status != PSA_SUCCESS) {
		MFG_LOG_ERR("psa_hash_setup failed: %d\n", (int)status);
		return -EIO;
	}

	rc = hash_image_region(&operation, 0, img_hash_len);
	if (rc != 0) {
		psa_hash_abort(&operation);
		return rc;
	}

	status = psa_hash_verify(&operation, expected, expected_len);
	if (status == PSA_SUCCESS) {
		return 0;
	}

	if (status == PSA_ERROR_INVALID_SIGNATURE) {
		return -EINVAL;
	}

	MFG_LOG_ERR("psa_hash_verify failed: %d\n", (int)status);
	psa_hash_abort(&operation);
	return -EIO;
}

static void validate_image_digest(uint32_t address, uint32_t size, const uint8_t *digest)
{
	int rc;

	MFG_LOG_INF("Validating %u bytes at 0x%08x...", size, address);

	if (digest == NULL || mfg_digest_is_zero(digest)) {
		MFG_LOG_INF(" SKIP (no expected digest provided at build time)\n");
		return;
	}

	if (size == 0U) {
		MFG_LOG_ERR(" FAIL (zero-length area)\n");
		recovery_suspend(false);
	}

	rc = verify_flash_digest(address, size, digest);
	if (rc == 0) {
		MFG_LOG_INF(" OK\n");
		return;
	}

	if (rc == -EINVAL) {
		MFG_LOG_ERR(" FAIL (digest mismatch)\n");
	} else {
		MFG_LOG_ERR(" FAIL (verification error %d)\n", rc);
	}
	recovery_suspend(false);
}

static void validate_manufacturing_app(void)
{
	struct image_header hdr;
	uint8_t expected_hash[IMAGE_SHA512_HASH_LEN];
	uint16_t hash_tlv_len;
	psa_algorithm_t alg;
	const char *hash_name;
	size_t hash_len;
	int rc;

	MFG_LOG_INF("Validating Manufacturing application (self-check)...");

	rc = flash_read_image(0, &hdr, sizeof(hdr));
	if (rc != 0) {
		MFG_LOG_ERR(" FAIL (cannot read image header: %d)\n", rc);
		recovery_suspend(false);
	}

	if (hdr.ih_magic != IMAGE_MAGIC) {
		MFG_LOG_ERR(" FAIL (invalid image magic 0x%08x)\n", hdr.ih_magic);
		recovery_suspend(false);
	}

	rc = find_image_tlv(&hdr, IMAGE_TLV_SHA256, expected_hash, sizeof(expected_hash),
			    &hash_tlv_len);
	if (rc == 0) {
		alg = PSA_ALG_SHA_256;
		hash_name = "SHA256";
		hash_len = IMAGE_HASH_LEN;
	} else {
		rc = find_image_tlv(&hdr, IMAGE_TLV_SHA512, expected_hash,
				    sizeof(expected_hash), &hash_tlv_len);
		if (rc == -ENOENT) {
			MFG_LOG_ERR(" FAIL (no IMAGE_TLV_SHA256 or IMAGE_TLV_SHA512 in trailer)\n");
			recovery_suspend(false);
		}
		if (rc != 0) {
			MFG_LOG_ERR(" FAIL (cannot read hash TLV: %d)\n", rc);
			recovery_suspend(false);
		}

		alg = PSA_ALG_SHA_512;
		hash_name = "SHA512";
		hash_len = IMAGE_SHA512_HASH_LEN;
	}

	if (hash_tlv_len != hash_len) {
		MFG_LOG_ERR(" FAIL (%s TLV length %u, expected %zu)\n",
			    hash_name, hash_tlv_len, hash_len);
		recovery_suspend(false);
	}

	rc = verify_mcuboot_image_hash(&hdr, alg, expected_hash, hash_len);
	if (rc == 0) {
		MFG_LOG_INF(" OK (%s)\n", hash_name);
		return;
	}

	if (rc == -EINVAL) {
		MFG_LOG_ERR(" FAIL (%s hash mismatch)\n", hash_name);
	} else {
		MFG_LOG_ERR(" FAIL (%s hash verification error %d)\n", hash_name, rc);
	}
	recovery_suspend(false);
}

/* ---------------------------------------------------------------------------
 * Step 3 entry point
 * ---------------------------------------------------------------------------
 */
void step3_image_validate_all(void)
{
	psa_status_t status;

	MFG_LOG_STEP("Validating images");

	status = psa_crypto_init();
	if (status != PSA_SUCCESS && status != PSA_ERROR_ALREADY_EXISTS) {
		MFG_LOG_ERR("psa_crypto_init failed: %d\n", (int)status);
		recovery_suspend(false);
	}

	if (mfg_key_blob_get() == NULL) {
		MFG_LOG_ERR("Manufacturing TLV key blob is missing or invalid.\n");
		recovery_suspend(false);
	}

	validate_manufacturing_app();

	/* BL0, BL2 slots, application candidate, and other images pass expected
	 * digests via protected TLV entries (address, size, SHA-256).
	 */
	for (size_t i = 0; i < mfg_key_blob_digest_count(); i++) {
		const struct mfg_area_digest *entry = mfg_key_blob_digest_get(i);

		if (entry == NULL) {
			break;
		}
		validate_image_digest(entry->address, entry->size, entry->digest);
	}
}
