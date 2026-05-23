/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/*
 * Manufacturing key blob embedded in the running image's MCUboot protected
 * TLV area (see CONFIG_NCS_MCUBOOT_MANUFACTURING_TLV_ID). The blob is packed
 * at build time by scripts/pack_mfg_keys.py and covered by the image signature.
 *
 * Source of truth for the fixed key layout: this file. The Python packer
 * mirrors it. Area digests are appended after the fixed fields as a sequence
 * of struct mfg_area_digest entries.
 */

#ifndef MFG_KEY_BLOB_H_
#define MFG_KEY_BLOB_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MFG_KEY_BLOB_MAGIC   0x4B47464DU   /* 'M','F','G','K' little-endian */
#define MFG_KEY_BLOB_VERSION 1U

#define MFG_ED25519_PUBKEY_LEN 32U
#define MFG_IKG_SEED_LEN       48U
#define MFG_KEYRAM_RAND_LEN    16U
#define MFG_ED25519_SIG_LEN    64U
#define MFG_DIGEST_LEN         32U

struct __packed mfg_area_digest {
	uint32_t address;
	uint32_t size;
	uint8_t  digest[MFG_DIGEST_LEN];
};

struct __packed mfg_key_blob {
	uint32_t magic;
	uint16_t version;
	uint16_t reserved;
	uint32_t total_size;

	uint8_t  urot_pubkey_gen0[MFG_ED25519_PUBKEY_LEN];
	uint8_t  urot_pubkey_gen1[MFG_ED25519_PUBKEY_LEN];
	uint8_t  mfg_app_pubkey  [MFG_ED25519_PUBKEY_LEN];

	uint8_t  ikg_seed       [MFG_IKG_SEED_LEN];
	uint8_t  keyram_random0 [MFG_KEYRAM_RAND_LEN];
	uint8_t  keyram_random1 [MFG_KEYRAM_RAND_LEN];

	uint8_t  urot_pubkey_gen0_sig[MFG_ED25519_SIG_LEN];
	uint8_t  urot_pubkey_gen1_sig[MFG_ED25519_SIG_LEN];
};

#define MFG_KEY_BLOB_FIXED_SIZE sizeof(struct mfg_key_blob)
#define MFG_AREA_DIGEST_SIZE    sizeof(struct mfg_area_digest)

/* Cached pointer to a validated blob, or NULL if the TLV is missing or
 * invalid. Validation checks magic, version, and total_size bounds.
 */
const struct mfg_key_blob *mfg_key_blob_get(void);

/* Number of area digest entries appended after the fixed blob fields. */
size_t mfg_key_blob_digest_count(void);

/* Return digest entry i, or NULL if out of range. */
const struct mfg_area_digest *mfg_key_blob_digest_get(size_t index);

/* True if a buffer of MFG_DIGEST_LEN bytes is all zeros (placeholder). */
static inline bool mfg_digest_is_zero(const uint8_t *digest)
{
	for (size_t i = 0; i < MFG_DIGEST_LEN; i++) {
		if (digest[i] != 0U) {
			return false;
		}
	}
	return true;
}

#endif /* MFG_KEY_BLOB_H_ */
