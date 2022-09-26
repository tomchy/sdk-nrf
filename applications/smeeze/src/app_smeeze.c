/*
 * Copyright (c) 2022 Tomasz Chyrowicz
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "app_smeeze.h"
#include <zephyr/logging/log.h>
#include "app_door_lock.h"
#include "app_sfinder.h"


static bool auto_mode_enabled = true;
static int16_t last_external_temp = 2000;
static int16_t last_internal_temp = 2000;
static int16_t last_desired_temp = 2000;

LOG_MODULE_REGISTER(app_smeeze, CONFIG_SMEEZE_SMEEZE_LOG_LEVEL);


void update_smeeze_logic()
{
	if (!auto_mode_enabled) {
		return;
	}

	if (last_desired_temp >= last_internal_temp) {
		return;
	}

	if (last_desired_temp >= last_external_temp) {
		LOG_INF("Invite the fresh air!");
		app_door_lock_unlock_smeeze();
	} else {
		LOG_INF("Fresh air collected!");
		app_door_lock_lock_smeeze();
	}
}

/** @brief Callback function for setting the new state of the auto mode. */
void app_smeeze_set_auto_mode(bool enabled)
{
	auto_mode_enabled = enabled;
	LOG_INF("New auto mode: %d", auto_mode_enabled);
	app_finder_set_auto_mode(enabled);
	update_smeeze_logic();
}

/** @brief Callback function for setting the new desired temperature. */
void app_smeeze_set_desired_temp(int16_t value)
{
	last_desired_temp = value;
	LOG_INF("New desired temperature: %d", last_desired_temp);
	update_smeeze_logic();
}

/** @brief Callback function for passing the new temperature readings from an external sensor. */
void app_smeeze_handle_external_temp(int16_t value)
{
	last_external_temp = value;
	LOG_INF("New external temperature: %d", last_external_temp);
	update_smeeze_logic();
}

/** @brief Callback function for passing the new reading of the local temperature. */
void app_smeeze_handle_internal_temp(int16_t value)
{
	last_internal_temp = value;
	LOG_INF("New internal temperature: %d", last_internal_temp);
	update_smeeze_logic();
}
