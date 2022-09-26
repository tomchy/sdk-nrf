/*
 * Copyright (c) 2022 Tomasz Chyrowicz
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>
#include "sfinder.h"


LOG_MODULE_REGISTER(sfinder, CONFIG_SMEEZE_SFINDER_LOG_LEVEL);


static zb_ieee_addr_t sensor_address_long;
static bool sensor_address_found = false;
static uint8_t sensor_ep = 255;


static bool is_valid_ieee(zb_ieee_addr_t ieee_addr)
{
	zb_ieee_addr_t test_ieee_addr;

	zb_osif_get_ieee_eui64(test_ieee_addr);
	if (memcmp(test_ieee_addr, ieee_addr, sizeof(zb_ieee_addr_t)) == 0) {
		return false;
	}

	memset(test_ieee_addr, 0xFF, sizeof(zb_ieee_addr_t));
	if (memcmp(test_ieee_addr, ieee_addr, sizeof(zb_ieee_addr_t)) == 0) {
		return false;
	}

	return true;
}

static int settings_set(const char *name, size_t len, settings_read_cb read_cb, void *cb_arg)
{
	if (!strcmp(name, "ieee")) {
		if (len != sizeof(sensor_address_long)) {
			return -EINVAL;
		}
		if (read_cb(cb_arg, sensor_address_long, len) > 0) {
			LOG_INF("Restored sensor long address: %x:%x:%x:%x:%x:%x:%x:%x",
				sensor_address_long[0], sensor_address_long[1],
				sensor_address_long[2], sensor_address_long[3],
				sensor_address_long[4], sensor_address_long[5],
				sensor_address_long[6], sensor_address_long[7]);

			if (!is_valid_ieee(sensor_address_long)) {
				memset(sensor_address_long, 0xFF, sizeof(zb_ieee_addr_t));
				sensor_address_found = false;
				LOG_WRN("Sensor IEEE address invalidated");
			}

			return 0;
		}
	} else if (!strcmp(name, "valid")) {
		if (len != sizeof(sensor_address_found)) {
			return -EINVAL;
		}
		if (read_cb(cb_arg, &sensor_address_found, len) > 0) {
			LOG_INF("Restored sensor address validity: %d", sensor_address_found);
			return 0;
		}
	} else if (!strcmp(name, "ep")) {
		if (len != sizeof(sensor_ep)) {
			return -EINVAL;
		}
		if (read_cb(cb_arg, &sensor_ep, len) > 0) {
			LOG_INF("Restored sensor endpoint: %d", sensor_ep);
			return 0;
		}
	}

	return -ENOENT;
}

static void sensor_save(void)
{
	int err = settings_save_one("sensor/ieee", sensor_address_long, sizeof(sensor_address_long));
	if (err) {
		LOG_ERR("Unable to store sensor long address: %d", err);
	}

	err = settings_save_one("sensor/valid", &(sensor_address_found), sizeof(sensor_address_found));
	if (err) {
		LOG_ERR("Unable to store sensor address validity: %d", err);
	}

	err = settings_save_one("sensor/ep", &(sensor_ep), sizeof(sensor_ep));
	if (err) {
		LOG_ERR("Unable to store sensor endpoint: %d", err);
	}

	LOG_INF("Saved address: %x:%x:%x:%x:%x:%x:%x:%x",
		sensor_address_long[0], sensor_address_long[1],
		sensor_address_long[2], sensor_address_long[3],
		sensor_address_long[4], sensor_address_long[5],
		sensor_address_long[6], sensor_address_long[7]);
}

static struct settings_handler settings_conf = {
	.name = "sensor",
	.h_set = settings_set
};


/**
 * @brief Initializes sensor finder and read it's state from NVM.
 *
 * @return 0 if success, error code if failure.
 */
int sfinder_init(void)
{
	int err = 0;

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
 * @brief Read the current status of the sensor finder.
 *
 * @retval true   if the temperature sensor is found
 * @retval false  if the temperature sensor is not found
 */
bool sfinder_is_found(void)
{
	return sensor_address_found;
}

/**
 * @brief Mark sensor as found in NVM.
 */
void sfinder_found(zb_ieee_addr_t ieee_addr, zb_uint8_t ep)
{
	if (is_valid_ieee(ieee_addr)) {
		memcpy(sensor_address_long, ieee_addr, sizeof(zb_ieee_addr_t));
		sensor_address_found = true;
		sensor_ep = ep;
		sensor_save();
	}
}

/**
 * @brief Remove data about sensor in NVM.
 */
void sfinder_forget(void)
{
	memset(sensor_address_long, 0xFF, sizeof(zb_ieee_addr_t));
	sensor_address_found = false;
	sensor_ep = 255;
	sensor_save();
}

int sfinder_get_ieee(zb_ieee_addr_t ieee_addr)
{
	if (!sfinder_is_found()) {
		return -1;
	}

	memcpy(ieee_addr, sensor_address_long, sizeof(zb_ieee_addr_t));
	return 0;
}

int sfinder_get_ep(uint8_t *ep)
{
	if (!sfinder_is_found()) {
		return -1;
	}

	*ep = sensor_ep;
	return 0;
}
