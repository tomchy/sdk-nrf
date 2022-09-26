/*
 * Copyright (c) 2022 Tomasz Chyrowicz
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "status_led.h"
#include <zephyr/drivers/pwm.h>

#define PWM_PERIOD 1024
#define BLINK_TIME_MS 200
#define PWM_LED_RED_NODE DT_NODELABEL(red_led_pwm)
#define PWM_LED_GREEN_NODE DT_NODELABEL(green_led_pwm)
#define PWM_LED_BLUE_NODE DT_NODELABEL(blue_led_pwm)


#if DT_NODE_HAS_STATUS(PWM_LED_RED_NODE, okay)
static const struct pwm_dt_spec led_red = PWM_DT_SPEC_GET(PWM_LED_RED_NODE);
#else
#error "Unsupported board: red_led_pwm devicetree node is not defined"
#endif

#if DT_NODE_HAS_STATUS(PWM_LED_GREEN_NODE, okay)
static const struct pwm_dt_spec led_green = PWM_DT_SPEC_GET(PWM_LED_GREEN_NODE);
#else
#error "Unsupported board: green_led_pwm devicetree node is not defined"
#endif

#if DT_NODE_HAS_STATUS(PWM_LED_BLUE_NODE, okay)
static const struct pwm_dt_spec led_blue = PWM_DT_SPEC_GET(PWM_LED_BLUE_NODE);
#else
#error "Unsupported board: blue_led_pwm devicetree node is not defined"
#endif


const uint8_t colors[] = {
	BLACK,
	RED,
	GREEN,
	YELLOW,
	BLUE,
	MAGENTA,
	CYAN,
	WHITE,
};

const int16_t intensity_levels[] = {
	0,
	64,
	128,
	172,
	256,
};

static uint8_t current_color = 0;


static void status_led_show(void)
{
	uint8_t intensity[] = {0, 0, 0};

	for (uint8_t i = 0; i < sizeof(colors); i++) {
		if (current_color & colors[i]) {
			intensity[0] += !!(i & 0b0001);
			intensity[1] += !!(i & 0b0010);
			intensity[2] += !!(i & 0b0100);
		}
	}

	pwm_set_dt(&led_red, PWM_USEC(PWM_PERIOD), PWM_USEC((PWM_PERIOD * intensity_levels[intensity[0]]) / 256));
	pwm_set_dt(&led_green, PWM_USEC(PWM_PERIOD), PWM_USEC((PWM_PERIOD * intensity_levels[intensity[1]]) / 256));
	pwm_set_dt(&led_blue, PWM_USEC(PWM_PERIOD), PWM_USEC((PWM_PERIOD * intensity_levels[intensity[2]]) / 256));
}


void status_led_init(void)
{
	if (!device_is_ready(led_red.dev)) {
		printk("Error: PWM device %s is not ready\n", led_red.dev->name);
		return;
	}
	if (!device_is_ready(led_green.dev)) {
		printk("Error: PWM device %s is not ready\n", led_green.dev->name);
		return;
	}
	if (!device_is_ready(led_blue.dev)) {
		printk("Error: PWM device %s is not ready\n", led_blue.dev->name);
		return;
	}
}

void status_led_color_add(uint8_t color)
{
	current_color |= color;
	status_led_show();
}

void status_led_color_remove(uint8_t color)
{
	current_color &= (~color);
	status_led_show();
}

void status_led_color_blink(uint8_t color)
{
	status_led_color_add(color);
	ZB_SCHEDULE_APP_ALARM(
		status_led_color_remove,
		color,
		ZB_MILLISECONDS_TO_BEACON_INTERVAL(BLINK_TIME_MS));
}

void status_led_update(zb_bufid_t bufid)
{
	zb_zdo_app_signal_hdr_t *p_sg_p = NULL;
	zb_zdo_app_signal_type_t sig = zb_get_app_signal(bufid, &p_sg_p);
	zb_ret_t status = ZB_GET_APP_SIGNAL_STATUS(bufid);

	switch (sig) {
	case ZB_BDB_SIGNAL_DEVICE_FIRST_START:
		/* At this point Zigbee stack is ready to operate and the BDB
		 * initialization procedure has finished.
		 * There is no network configuration stored inside NVRAM.
		 */
		if (status == RET_OK) {
			status_led_color_blink(RED);
		} else {
			status_led_color_add(RED);
		}
		break;

	case ZB_BDB_SIGNAL_DEVICE_REBOOT:
		/* At this point Zigbee stack is ready to operate and the BDB
		 * initialization procedure has finished. There is network
		 * configuration stored inside NVRAM, so the device
		 * will try to rejoin.
		 */
		/* fall-through */
	case ZB_BDB_SIGNAL_STEERING:
		/* At this point the Zigbee stack has finished network steering
		 * procedure. The device may have rejoined the network,
		 * which is indicated by signal's status code.
		 */
		status_led_color_blink(RED);
		break;

	case ZB_ZDO_SIGNAL_LEAVE:
		/* This signal is generated when the device itself has left
		 * the network by sending leave command.
		 */
		if (status == RET_OK) {
			status_led_color_blink(WHITE);
		} else {
			status_led_color_blink(RED);
		}
		break;

	default:
		break;
	}
}
