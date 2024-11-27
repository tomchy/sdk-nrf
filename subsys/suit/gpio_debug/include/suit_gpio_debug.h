/*
 * Copyright (c) 2024 Nordic Semiconductor ASA.
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef SUIT_GPIO_DEBUG_H__
#define SUIT_GPIO_DEBUG_H__

#ifdef __cplusplus
extern "C" {
#endif

#define SUIT_GPIO_ORCHESTRATOR_BOOT_PART 0
#define SUIT_GPIO_MFST_PROCESSING_PART 1
#define SUIT_GPIO_MFST_SEQ_EXEC_PART 2
#define SUIT_GPIO_LOAD_ENVELOPE 3
#define SUIT_GPIO_LOAD_ENVELOPE_PART 4
#define SUIT_GPIO_CMD_PROCESS_DEP_PART 5
#define SUIT_GPIO_CMD_DEP_INTEGRITY_PART 6
#define SUIT_GPIO_PLAT_VID_CID_CHECK 7
#define SUIT_GPIO_PLAT_COPY_PART 8
#define SUIT_GPIO_PLAT_FLASH_STREAM 8
#define SUIT_GPIO_PLAT_ADDRESS_STREAM 9
#define SUIT_GPIO_MFST_AUTHENTICATE 10
#define SUIT_GPIO_PLAT_DIGEST 11

#ifdef CONFIG_SUIT_GPIO_DEBUG
/**
 * @brief Toggle GPIO pin
 *
 * @param[in] pin  Pin number to toggle.
 */
void suit_gpio_debug_toggle(int pin);
#else /* CONFIG_SUIT_GPIO_DEBUG */
#define suit_gpio_debug_toggle(pin) ;
#endif /* CONFIG_SUIT_GPIO_DEBUG */

#ifdef __cplusplus
}
#endif

#endif /* SUIT_GPIO_DEBUG_H__ */
