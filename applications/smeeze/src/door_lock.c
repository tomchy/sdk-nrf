/*
 * Copyright (c) 2022 Tomasz Chyrowicz
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>
#include <drivers/stepper.h>
#include "door_lock.h"


/* Define stepper motor driver DT node. */
#define STEPPER0_NODE DT_NODELABEL(stepper1)

LOG_MODULE_REGISTER(door_lock, CONFIG_SMEEZE_DOOR_LOCK_LOG_LEVEL);

static bool locked = true;
static stepper_point_t position = {
	.x = 0,
	.cb = NULL,
};
static door_lock_locked_cb state_cb = NULL;


static int settings_set(const char *name, size_t len, settings_read_cb read_cb, void *cb_arg)
{
	static const struct stepper_dt_spec stepper = STEPPER_DT_SPEC_GET(STEPPER0_NODE);

	if (!strcmp(name, "state")) {
		if (len != sizeof(locked)) {
			return -EINVAL;
		}
		if (read_cb(cb_arg, &locked, len) > 0) {
			LOG_INF("Restored door lock locked state: %d", locked);
			return 0;
		}
	} else if (!strcmp(name, "position")) {
		if (len != sizeof(position.x)) {
			return -EINVAL;
		}
		if (read_cb(cb_arg, &position.x, len) > 0) {
			LOG_INF("Restored door lock position: %d", position.x);
			stepper_set_position(stepper.dev, &position);
			return 0;
		}
	}

	return -ENOENT;
}

static void door_lock_save(void)
{
	int err = settings_save_one("door_lock/state", &(locked), sizeof(locked));
	if (err) {
		LOG_ERR("Unable to store door lock locked state: %d", err);
	}

	err = settings_save_one("door_lock/position", &(position.x), sizeof(position.x));
	if (err) {
		LOG_ERR("Unable to store door lock position: %d", err);
	}
}

static struct settings_handler settings_conf = {
	.name = "door_lock",
	.h_set = settings_set
};


static void door_lock_locked(int x)
{
	position.x = x;
	locked = true;

	if (state_cb != NULL) {
		state_cb(locked);
	}

	door_lock_save();
}

static void door_lock_unlocked(int x)
{
	position.x = x;
	locked = false;

	if (state_cb != NULL) {
		state_cb(locked);
	}

	door_lock_save();
}

/**
 * @brief Initializes HW lock and read it's state from NVM.
 *
 * @return 0 if success, error code if failure.
 */
int door_lock_init(void)
{
	static const struct stepper_dt_spec stepper = STEPPER_DT_SPEC_GET(STEPPER0_NODE);
	int err = 0;

	if (!device_is_ready(stepper.dev)) {
		LOG_ERR("Unable to initialize stepper motor driver: %d", err);
		return err;
	}

	err = settings_register(&settings_conf);
	if (err) {
		LOG_ERR("Unable to register settings handler: %d", err);
		return err;
	}

	err = settings_load_subtree(settings_conf.name);
	if (err) {
		LOG_ERR("Unable to load settings: %d", err);
		return err;
	}

	return err;
}

/**
 * @brief Read the current status of the door lock.
 *
 * @retval true   if the door lock is locked
 * @retval false  if the door lock is unlocked
 */
bool door_lock_is_locked(void)
{
	return locked;
}

/**
 * @brief Lock the physical door lock.
 *
 * @note This API does not update Zigbee attribute value.
 *       It manipulates the HW lock and updates the state
 *       stored in NVM.
 */
void door_lock_lock(door_lock_locked_cb cb)
{
	static const struct stepper_dt_spec stepper = STEPPER_DT_SPEC_GET(STEPPER0_NODE);
	static stepper_path_t pos = {
		.p = {
			.x = 0,
			.cb = door_lock_locked,
		},
		.next = NULL,
	};
	state_cb = cb;

	stepper_set_next_coord(stepper.dev, &pos);
}

/**
 * @brief Unlock the physical door lock.
 *
 * @note This API does not update Zigbee attribute value.
 *       It manipulates the HW lock and updates the state
 *       stored in NVM.
 */
void door_lock_unlock(door_lock_locked_cb cb)
{
	static const struct stepper_dt_spec stepper = STEPPER_DT_SPEC_GET(STEPPER0_NODE);
	static stepper_path_t pos = {
		.p = {
			.x = 10000,
			.cb = door_lock_unlocked,
		},
		.next = NULL,
	};
	state_cb = cb;

	stepper_set_next_coord(stepper.dev, &pos);
}
