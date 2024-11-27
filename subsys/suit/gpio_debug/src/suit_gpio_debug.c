/*
 * Copyright (c) 2024 Nordic Semiconductor ASA.
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/init.h>
#include <hal/nrf_gpio.h>
#include <suit_gpio_debug.h>

#define GPIO_DEBUG_PIN_MIN 0
#define GPIO_DEBUG_PIN_MAX 12

void suit_gpio_debug_toggle(int pin)
{
	if ((pin >= GPIO_DEBUG_PIN_MAX) || (pin < GPIO_DEBUG_PIN_MIN)) {
		return;
	}

	nrf_gpio_pin_toggle(pin);
}

static int gpio_debug_init(void)
{
	for (size_t i = GPIO_DEBUG_PIN_MIN; i < GPIO_DEBUG_PIN_MAX; i++) {
		nrf_gpio_cfg_output(i);
		nrf_gpio_pin_set(i);
	}
	for (size_t i = GPIO_DEBUG_PIN_MIN; i < GPIO_DEBUG_PIN_MAX; i++) {
		nrf_gpio_pin_toggle(i);
	}

	return 0;
}

SYS_INIT(gpio_debug_init, APPLICATION, 0);
