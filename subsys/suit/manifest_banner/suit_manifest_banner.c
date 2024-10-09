/*
 * Copyright (c) 2024 Nordic Semiconductor ASA.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/init.h>
#include <sdfw/sdfw_services/suit_service.h>
#include <zephyr/kernel.h>

#define IMG_MGMT_HASH_LEN	       64 /* SHA512 */
#define SUIT_MANIFEST_BANNER_INIT_PRIO 0

static const char *suit_release_type_str_get(suit_version_release_type_t type)
{
	switch (type) {
	case SUIT_VERSION_RELEASE_NORMAL:
		return NULL;
	case SUIT_VERSION_RELEASE_RC:
		return "rc";
	case SUIT_VERSION_RELEASE_BETA:
		return "beta";
	case SUIT_VERSION_RELEASE_ALPHA:
		return "alpha";
	default:
		return NULL;
	}
};

static const char *get_digest_name(int digest_alg)
{
	switch (digest_alg) {
	case -44:
		return "SHA-512";
	case -16:
		return "SHA-256";
	default:
		return "UNKNOWN";
	}
}

static int suit_manifest_banner(void)
{
	uint8_t digest_buf[IMG_MGMT_HASH_LEN] = {0};
	suit_plat_err_t plat_ret;
	suit_manifest_role_t roles[CONFIG_MGMT_SUITFU_GRP_SUIT_MFSTS_STATE_MFSTS_COUNT] = {0};
	size_t class_info_count = ARRAY_SIZE(roles);
	suit_ssf_manifest_class_info_t class_info = {0};
	unsigned int seq_num = 0;
	suit_semver_raw_t semver_raw = {0};
	suit_version_t version;
	const char *release_type;
	suit_digest_status_t digest_status = SUIT_DIGEST_UNKNOWN;
	int digest_alg_id = 0;
	suit_plat_mreg_t digest;

	plat_ret = suit_get_supported_manifest_roles(roles, &class_info_count);
	if (plat_ret != SUIT_PLAT_SUCCESS) {
		return -ENOTSUP;
	}

	for (size_t i = 0; i < class_info_count; i++) {
		plat_ret = suit_get_supported_manifest_info(roles[i], &class_info);
		if (plat_ret != SUIT_PLAT_SUCCESS) {
			continue;
		}

		printk("Manifest with role 0x%x%s:\n", roles[i], suit_role_name_get(roles[i]));
		printk("\tclass_id: " SUIT_MANIFEST_CLASS_ID_LOG_FORMAT "\n",
		       SUIT_MANIFEST_CLASS_ID_LOG_ARGS(&class_info.class_id));

		digest.mem = digest_buf;
		digest.size = sizeof(digest_buf);
		plat_ret = suit_get_installed_manifest_info(&class_info.class_id, &seq_num,
							    &semver_raw, &digest_status,
							    &digest_alg_id, &digest);
		if (plat_ret != SUIT_PLAT_SUCCESS) {
			continue;
		}

		switch (digest_status) {
		case SUIT_DIGEST_UNAUTHENTICATED:
			printk("\tintegrity: valid unauthenticated digest\n");
			break;
		case SUIT_DIGEST_INCORRECT_SIGNATURE:
			printk("\tintegrity: invalid signature\n");
			break;
		case SUIT_DIGEST_AUTHENTICATED:
			printk("\tintegrity: valid signature\n");
			break;
		case SUIT_DIGEST_MISMATCH:
			printk("\tintegrity: incorrect manifest digest\n");
			break;
		default:
			printk("\tintegrity: unknown\n");
			break;
		}
		if (digest.size > 8) {
			printk("\tdigest algorithm: %s\n", get_digest_name(digest_alg_id));
			printk("\tdigest: %02X%02X%02X%02X...%02X%02X%02X%02X\n", digest.mem[0],
			       digest.mem[1], digest.mem[2], digest.mem[3],
			       digest.mem[digest.size - 4], digest.mem[digest.size - 3],
			       digest.mem[digest.size - 2], digest.mem[digest.size - 1]);
		}
		if (seq_num != UINT32_MAX) {
			printk("\tsequence number: 0x%X\n", seq_num);
		}
		if (semver_raw.len > 0) {
			plat_ret = suit_metadata_version_from_array(&version, semver_raw.raw,
								    semver_raw.len);
			if (plat_ret != SUIT_PLAT_SUCCESS) {
				continue;
			}

			release_type = suit_release_type_str_get(version.type);
			if (release_type) {
				printk("\tversion: %d.%d.%d-%s%d\n", version.major, version.minor,
				       version.patch, release_type, version.pre_release_number);
			} else {
				printk("\tversion: %d.%d.%d\n", version.major, version.minor,
				       version.patch);
			}
		}
	}

	return 0;
}

SYS_INIT(suit_manifest_banner, APPLICATION, SUIT_MANIFEST_BANNER_INIT_PRIO);
