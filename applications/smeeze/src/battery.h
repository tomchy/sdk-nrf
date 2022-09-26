/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdint.h>

/* Initialize GPIOs for reading the state of the battery. */
int battery_init();

/* Enable ADC to read the current battery voltage. */
int battery_enable();

/* Read the battery voltage. */
int32_t battery_read_voltage_mv();

/* Check if the battery is currently charging. */
int battery_is_charging();
