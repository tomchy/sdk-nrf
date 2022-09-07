/*
 * Copyright (c) 2022 Tomasz Chyrowicz
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/zephyr.h>
#include <zephyr/drivers/gpio.h>
#include <drivers/stepper.h>
#include <zephyr/logging/log.h>
#include <dk_buttons_and_leds.h>

/* 1000 msec = 1 sec */
#define SLEEP_TIME_MS   1000

/* The devicetree node identifier for the "led0" alias. */
#define LED0_NODE DT_ALIAS(led0)
#define STEPPER0_NODE DT_NODELABEL(stepper1)

LOG_MODULE_REGISTER(app, LOG_LEVEL_INF);

/*
 * A build error on this line means your board is unsupported.
 * See the sample documentation for information on how to fix this.
 */
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
static const struct stepper_dt_spec stepper = STEPPER_DT_SPEC_GET(STEPPER0_NODE);
static int32_t pos = 0;

static void button_changed(uint32_t button_state, uint32_t has_changed)
{
	if (DK_BTN1_MSK & has_changed) {
		if (DK_BTN1_MSK & button_state) {
			/* Button changed its state to pressed */
			static stepper_path_t pos1 = {
				.p = {
					.x = 0,
					.cb = NULL,
				},
				.next = NULL,
			};
			pos -= 100;
			pos1.p.x = pos;
			stepper_set_next_coord(stepper.dev, &pos1);
		}
	}

	else if (DK_BTN2_MSK & has_changed) {
		if (DK_BTN2_MSK & button_state) {
			/* Button changed its state to pressed */
			static stepper_path_t pos2 = {
				.p = {
					.x = 500,
					.cb = NULL,
				},
				.next = NULL,
			};
			pos += 100;
			pos2.p.x = pos;
			stepper_set_next_coord(stepper.dev, &pos2);
		}
	}
}

void main(void)
{
	int ret;

	ret = dk_buttons_init(button_changed);
	if (ret) {
		LOG_ERR("Cannot init buttons (err: %d)", ret);
	}

	if (!device_is_ready(led.port)) {
		return;
	}

	ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
	if (ret < 0) {
		return;
	}

	if (!device_is_ready(stepper.dev)) {
		LOG_ERR("Error: Stepper device %s is not ready",
			stepper.dev->name);
	}

	while (1) {
		ret = gpio_pin_toggle_dt(&led);
		if (ret < 0) {
			return;
		}
		k_msleep(SLEEP_TIME_MS);
	}
}
