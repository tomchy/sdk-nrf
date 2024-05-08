/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef ZCBOR_NONCANONICAL_DECODE_H__
#define ZCBOR_NONCANONICAL_DECODE_H__

/** Decode and consume a noncanonical bstr header, assuming the payload does not contain the whole
 * bstr.
 *
 * The rest of the string can be decoded as CBOR.
 * A state backup is created to keep track of the element count.
 * Call @ref zcbor_update_state followed by @ref zcbor_bstr_next_fragment when
 * the current payload has been exhausted.
 */
bool zcbor_noncanonical_bstr_start_decode_fragment(zcbor_state_t *state,
						   struct zcbor_string_fragment *result);

/** Decode and consume a noncanonical map header.
 *
 * The contents of the map can be decoded via subsequent function calls.
 * A state backup is created to keep track of the element count.
 *
 * @retval true   Header decoded correctly
 * @retval false  Header decoded incorrectly, or backup failed.
 */
bool zcbor_noncanonical_map_start_decode(zcbor_state_t *state);

/** Finalize decoding a list/map
 *
 * Check that the list/map had the correct number of elements, and restore the
 * previous element count from the backup.
 *
 * Use @ref zcbor_list_map_end_force_decode to forcibly consume the backup if
 * something has gone wrong.
 *
 * In all successful cases, the state is returned pointing to the byte/element
 * after the list/map in the payload.
 *
 * @retval true   Everything ok.
 * @retval false  Element count not correct.
 */
bool zcbor_noncanonical_map_end_decode(zcbor_state_t *state);

#endif /* ZCBOR_NONCANONICAL_DECODE_H__ */
