/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef SUITFU_MGMT_PRIV_H_
#define SUITFU_MGMT_PRIV_H_

#include <stdint.h>
#include <zcbor_common.h>
#include <zephyr/mgmt/mcumgr/smp/smp.h>
#include <mgmt/mcumgr/util/zcbor_bulk.h>

#ifdef __cplusplus
extern "C" {
#endif

#define IMG_MGMT_VER_MAX_STR_LEN 32
/* The value here sets how many "characteristics" that describe image is
 * encoded into a map per each image (like bootable flags, and so on).
 * This value is only used for zcbor to predict map size and map encoding
 * and does not affect memory allocation.
 * In case when more "characteristics" are added to image map then
 * zcbor_map_end_encode may fail it this value does not get updated.
 */
#define MAX_IMG_CHARACTERISTICS	 15
#define IMG_MGMT_HASH_STR	 48
#define IMG_MGMT_HASH_LEN	 64 /* SHA512 */

/*
 * Command IDs for image management group.
 */
#define IMG_MGMT_ID_STATE    0
#define IMG_MGMT_ID_UPLOAD   1
#define IMG_MGMT_ID_FILE     2
#define IMG_MGMT_ID_CORELIST 3
#define IMG_MGMT_ID_CORELOAD 4
#define IMG_MGMT_ID_ERASE    5

/*
 * Command IDs for suit management group.
 */
#define SUIT_MGMT_ID_MANIFESTS_LIST	  0
#define SUIT_MGMT_ID_MANIFEST_STATE	  1
#define SUIT_MGMT_ID_ENVELOPE_UPLOAD	  2
#define SUIT_MGMT_ID_MISSING_IMAGE_STATE  3
#define SUIT_MGMT_ID_MISSING_IMAGE_UPLOAD 4

/**
 * @brief	Verifies if the device associated to DFU partition is ready for use
 *
 * @return MGMT_ERR_EOK on success
 *		MGMT_ERR_EBADSTATE if the device is not ready for use
 *
 */
int suitfu_mgmt_is_dfu_partition_ready(void);

/**
 * @brief	Returns size of DFU partition, in bytes
 *
 */
size_t suitfu_mgmt_get_dfu_partition_size(void);

/**
 * @brief	Erases first num_bytes of DFU partition rounded up to the end of erase
 * block size
 *
 * @return MGMT_ERR_EOK on success
 *		MGMT_ERR_ENOMEM if DFU partition is smaller than num_bytes
 *		MGMT_ERR_EUNKNOWN if erase operation has failed
 */
int suitfu_mgmt_erase_dfu_partition(size_t num_bytes);

/**
 * @brief	Writes image chunk to DFU partition
 *
 * @return MGMT_ERR_EOK on success
 *		MGMT_ERR_EUNKNOWN if write operation has failed
 */
int suitfu_mgmt_write_dfu_image_data(unsigned int req_offset, const void *addr, unsigned int size,
				     bool flush);

/**
 * @brief	Called once entire update candidate is written to DFU partition
 * Implementation triggers further processing of the candidate
 *
 * @return MGMT_ERR_EOK on success
 *		MGMT_ERR_EBUSY on candidate processing error
 */
int suitfu_mgmt_candidate_envelope_stored(size_t image_size);

/**
 * @brief	Process Manifests List Get Request
 *
 */
int suitfu_mgmt_suit_manifests_list(struct smp_streamer *ctx);

/**
 * @brief	Process Manifest State Get Request
 *
 */
int suitfu_mgmt_suit_manifest_state_read(struct smp_streamer *ctx);

/**
 * @brief	Process Candidate Envelope Upload Request
 *
 */
int suitfu_mgmt_suit_envelope_upload(struct smp_streamer *ctx);

/**
 * @brief	Initialization of Image Fetch functionality
 *
 */
void suitfu_mgmt_suit_image_fetch_init(void);

/**
 * @brief	Process Get Missing Image State Request.
 *
 * @note	SMP Client sends that request periodically,
 *		getting requested image identifier (i.e. image name)
 *		as response
 *
 */
int suitfu_mgmt_suit_missing_image_state_read(struct smp_streamer *ctx);

/**
 * @brief	Process Image Upload Request
 *
 * @note	Executed as result of Get Missing Image State Request.
 * It delivers chunks of image requested by the device
 *
 */
int suitfu_mgmt_suit_missing_image_upload(struct smp_streamer *ctx);

/**
 * @brief Returns SUIT bootloader info
 *
 */
int suitfu_mgmt_suit_bootloader_info_read(struct smp_streamer *ctx);

/** @brief Decodes single level map according to a provided key-decode map.
 *
 * The function takes @p map of key to decoder array defined as:
 *
 *	struct zcbor_map_decode_key_val map[] = {
 *		ZCBOR_MAP_DECODE_KEY_DECODER("key0", decode_fun0, val_ptr0),
 *		ZCBOR_MAP_DECODE_KEY_DECODER("key1", decode_fun1, val_ptr1),
 *		...
 *	};
 *
 * where "key?" is string representing key; the decode_fun? is
 * zcbor_decoder_t compatible function, either from zcbor or defined by
 * user; val_ptr? are pointers to variables where decoder function for
 * a given key will place a decoded value - they have to agree in type
 * with decoder function.
 *
 * Failure to decode any of values will cause the function to return
 * negative error, and leave the map open: map is broken anyway or key-decoder
 * mapping is broken, and we can not really decode the map.
 *
 * Note that the function opens map by itself and will fail if map
 * is already opened.
 *
 * @param zsd		zcbor decoder state;
 * @param map		key-decoder mapping list;
 * @param map_size	size of maps, both maps have to have the same size;
 * @param matched	pointer to the  counter of matched keys, zeroed upon
 *			successful map entry and incremented only for successful
 *			decoded fields.
 * @return		0 when the whole map has been parsed, there have been
 *			no decoding errors, and map has been closed successfully;
 *			-ENOMSG when given decoder function failed to decode
 *			value;
 *			-EADDRINUSE when key appears twice within map, map is then
 *			parsed up to they key that has appeared twice;
 *			-EBADMSG when failed to close map.
 */
int zcbor_noncanonical_map_decode_bulk(zcbor_state_t *zsd, struct zcbor_map_decode_key_val *map,
                          size_t map_size, size_t *matched);

#ifdef __cplusplus
}
#endif

#endif /* __IMG_PRIV_H */
