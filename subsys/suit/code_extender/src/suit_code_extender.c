/*
 * Copyright (c) 2024 Nordic Semiconductor ASA.
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/init.h>

static const uint8_t code_extender[CONFIG_SUIT_CODE_EXTENDER_SIZE] = {0x00};

static int gpio_debug_init(void)
{
	volatile uint32_t sum = 0;
	for (int i = 0; i < sizeof(code_extender); i += 1024) {
		sum += code_extender[i];
	}

	if (sum > 10) {
		sum = 10;
	}

	return 0;
}

SYS_INIT(gpio_debug_init, APPLICATION, 0);
