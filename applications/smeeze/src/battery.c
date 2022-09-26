/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "battery.h"

#include <hal/nrf_saadc.h>

#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/gpio.h>

#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(app);

#define VBATT DT_PATH(vbatt)
#define ZEPHYR_USER DT_PATH(zephyr_user)

const struct gpio_dt_spec power_gpio = GPIO_DT_SPEC_GET(VBATT, power_gpios);
const struct gpio_dt_spec charge_gpio = GPIO_DT_SPEC_GET(ZEPHYR_USER, battery_charge_gpios);
const uint32_t full_ohms = DT_PROP(VBATT, full_ohms);
const uint32_t output_ohms = DT_PROP(VBATT, output_ohms);
const struct adc_dt_spec adc = ADC_DT_SPEC_GET(VBATT);

static int battery_configured = 0;
static int16_t adc_buffer = 0;

static struct adc_sequence adc_seq = {
	.buffer = &adc_buffer,
	.buffer_size = sizeof(adc_buffer),
	.calibrate = true,
};

int battery_init()
{
	int err;

	if (!device_is_ready(power_gpio.port)) {
		LOG_ERR("Battery measurement GPIO device not ready");
		return -ENODEV;
	}

	err = gpio_pin_configure_dt(&power_gpio, GPIO_OUTPUT_INACTIVE);
	if (err != 0) {
		LOG_ERR("Failed to configure battery measurement GPIO %d", err);
		return err;
	}

	if (!device_is_ready(adc.dev)) {
		LOG_ERR("ADC controller not ready");
		return -ENODEV;
	}

	err = adc_channel_setup_dt(&adc);
	if (err) {
		LOG_ERR("Setting up the ADC channel failed");
		return err;
	}

	(void)adc_sequence_init_dt(&adc, &adc_seq);

	if (!device_is_ready(charge_gpio.port)) {
		LOG_ERR("Charge GPIO controller not ready");
		return -ENODEV;
	}

	err = gpio_pin_configure_dt(&charge_gpio, GPIO_INPUT);
	if (err != 0) {
		LOG_ERR("Failed to configure battery charge GPIO %d", err);
		return err;
	}

	battery_configured = 1;

	return err;
}

int battery_enable()
{
	int err = -ECANCELED;
	if (battery_configured) {
		err = gpio_pin_set_dt(&power_gpio, 1);
		if (err != 0) {
			LOG_ERR("Failed to enable measurement pin %d", err);
		}
	}
	return err;
}

int32_t battery_read_voltage_mv()
{
	int32_t result = -ECANCELED;

	if (battery_configured) {
		result = adc_read(adc.dev, &adc_seq);
		if (result == 0) {
			int32_t val = adc_buffer;
			adc_raw_to_millivolts_dt(&adc, &val);
			result = (int32_t)((int64_t)(val) * full_ohms / output_ohms);
		}
	}

	return result;
}

int battery_is_charging()
{
	if (!battery_configured) {
		return -1;
	}

	return !!(gpio_pin_get_dt(&charge_gpio));
}
