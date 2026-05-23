/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * @file image_validation.h
 *
 * Step 3: Validate digests of all sub-images in the initial image.
 */

#ifndef IMAGE_VALIDATION_H_
#define IMAGE_VALIDATION_H_

/**
 * @brief Step 3: Validate all sub-image digests.
 *
 * Checks SHA-256 digests of:
 *   - Manufacturing application (MCUboot IMAGE_TLV_SHA256/SHA512 self-check)
 *   - BL0/BL1/BL2 slots, application candidate, and other images (manufacturing
 *     TLV area digests)
 *
 * Suspends execution if any verification fails.
 */
void step3_image_validate_all(void);

#endif /* IMAGE_VALIDATION_H_ */
