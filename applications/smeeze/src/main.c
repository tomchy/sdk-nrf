/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/device.h>
#include <dk_buttons_and_leds.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>
#include <zb_nrf_platform.h>
#include <zboss_api.h>
#include <zephyr/kernel.h>
#include <zigbee/zigbee_app_utils.h>
#include <zigbee/zigbee_error_handler.h>

#ifdef CONFIG_USB_DEVICE_STACK
#include <zephyr/usb/usb_device.h>
#endif /* CONFIG_USB_DEVICE_STACK */

#include "zb_mem_config_custom.h"
#include "status_led.h"
#include "app_wstation.h"
#include "app_door_lock.h"
#include "app_sfinder.h"
#include "app_smeeze.h"

#ifdef CONFIG_ZIGBEE_FOTA
#include <zigbee/zigbee_fota.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/dfu/mcuboot.h>
#endif /* CONFIG_ZIGBEE_FOTA */


/* Device context */
#ifndef CONFIG_ZIGBEE_FOTA
ZBOSS_DECLARE_DEVICE_CTX_3_EP(
	main_ctx,
	weather_station_ep,
	door_lock_ep,
	sfinder_ep);
#else
extern zb_af_endpoint_desc_t zigbee_fota_client_ep;
ZBOSS_DECLARE_DEVICE_CTX_4_EP(
	main_ctx,
	weather_station_ep,
	door_lock_ep,
	zigbee_fota_client_ep,
	sfinder_ep);
#endif /* CONFIG_ZIGBEE_FOTA */

/* Delay for console initialization */
#define WAIT_FOR_CONSOLE_MSEC 100
#define WAIT_FOR_CONSOLE_DEADLINE_MSEC 5000

#ifdef CONFIG_ZIGBEE_FOTA
/* LED indicating OTA Client Activity. */
#define OTA_ACTIVITY_LED LED_GREEN
#endif /* CONFIG_ZIGBEE_FOTA */

/* Button used to enter the Identify mode */
#define IDENTIFY_MODE_BUTTON DK_BTN1_MSK

/* Button to start Factory Reset */
#define FACTORY_RESET_BUTTON IDENTIFY_MODE_BUTTON

BUILD_ASSERT(DT_NODE_HAS_COMPAT(DT_CHOSEN(zephyr_console),
				zephyr_cdc_acm_uart),
	     "Console device is not ACM CDC UART device");
LOG_MODULE_REGISTER(app, CONFIG_SMEEZE_LOG_LEVEL);


#ifdef CONFIG_ZIGBEE_FOTA
static void confirm_image(void)
{
	if (!boot_is_img_confirmed()) {
		int ret = boot_write_img_confirmed();

		if (ret) {
			LOG_ERR("Couldn't confirm image: %d", ret);
		} else {
			LOG_INF("Marked image as OK");
		}
	}
}

static void ota_evt_handler(const struct zigbee_fota_evt *evt)
{
	switch (evt->id) {
	case ZIGBEE_FOTA_EVT_PROGRESS:
		if (evt->dl.progress % 2) {
			status_led_color_add(MAGENTA);
		} else {
			status_led_color_add(MAGENTA);
		}
		break;

	case ZIGBEE_FOTA_EVT_FINISHED:
		LOG_INF("Reboot application.");

		sys_reboot(SYS_REBOOT_COLD);
		break;

	case ZIGBEE_FOTA_EVT_ERROR:
		LOG_ERR("OTA image transfer failed.");
		break;

	default:
		break;
	}
}
#endif /* CONFIG_ZIGBEE_FOTA */

static void toggle_door_lock_scheduled(zb_bufid_t unused)
{
	app_door_lock_toggle();
}

static void button_changed(uint32_t button_state, uint32_t has_changed)
{
	if (IDENTIFY_MODE_BUTTON & has_changed) {
		if (IDENTIFY_MODE_BUTTON & button_state) {
			/* Button changed its state to pressed */
		} else {
			/* Button changed its state to released */
			if (was_factory_reset_done()) {
				/* The long press was for Factory Reset */
				LOG_DBG("After Factory Reset - ignore button release");
			} else   {
				/* Button released before Factory Reset */

				ZB_SCHEDULE_APP_CALLBACK(toggle_door_lock_scheduled, 0);

				/* Inform default signal handler about user input at the device */
				user_input_indicate();
			}
		}
	}

	check_factory_reset_button(button_state, has_changed);
}

static void gpio_init(void)
{
	int err = dk_buttons_init(button_changed);
	if (err) {
		LOG_ERR("Cannot init buttons (err: %d)", err);
	}
}

#ifdef CONFIG_USB_DEVICE_STACK
static void wait_for_console(void)
{
	const struct device *console = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
	uint32_t dtr = 0;
	uint32_t time = 0;

	/* Enable the USB subsystem and associated HW */
	if (usb_enable(NULL)) {
		LOG_ERR("Failed to enable USB");
	} else {
		/* Wait for DTR flag or deadline (e.g. when USB is not connected) */
		while (!dtr && time < WAIT_FOR_CONSOLE_DEADLINE_MSEC) {
			uart_line_ctrl_get(console, UART_LINE_CTRL_DTR, &dtr);
			/* Give CPU resources to low priority threads */
			k_sleep(K_MSEC(WAIT_FOR_CONSOLE_MSEC));
			time += WAIT_FOR_CONSOLE_MSEC;
		}
	}
}
#endif /* CONFIG_USB_DEVICE_STACK */

/**@brief Callback function for handling ZCL commands.
 *
 * @param[in]   bufid   Reference to Zigbee stack buffer
 *                      used to pass received data.
 */
static void zcl_device_cb(zb_bufid_t bufid)
{
	zb_zcl_device_callback_param_t *device_cb_param =
		ZB_BUF_GET_PARAM(bufid, zb_zcl_device_callback_param_t);

	if ((device_cb_param->device_cb_id == ZB_ZCL_DOOR_LOCK_UNLOCK_DOOR_CB_ID) ||
	    (device_cb_param->device_cb_id == ZB_ZCL_DOOR_LOCK_LOCK_DOOR_CB_ID)) {
		app_door_lock_zcl_cb(bufid);
	}
	else if ((device_cb_param->device_cb_id == ZB_ZCL_LEVEL_CONTROL_SET_VALUE_CB_ID) ||
	    (device_cb_param->device_cb_id == ZB_ZCL_SET_ATTR_VALUE_CB_ID)) {
		app_sfinder_zcl_cb(bufid);
	}
#ifdef CONFIG_ZIGBEE_FOTA
	else if (device_cb_param->device_cb_id == ZB_ZCL_OTA_UPGRADE_VALUE_CB_ID) {
		zigbee_fota_zcl_cb(bufid);
	}
#endif /* CONFIG_ZIGBEE_FOTA */
	else {
		device_cb_param->status = RET_NOT_IMPLEMENTED;
	}
}

void zboss_signal_handler(zb_bufid_t bufid)
{
	zb_zdo_app_signal_hdr_t *signal_header = NULL;
	zb_zdo_app_signal_type_t signal = zb_get_app_signal(bufid, &signal_header);
	zb_ret_t err = RET_OK;

	status_led_update(bufid);

#ifdef CONFIG_ZIGBEE_FOTA
	/* Pass signal to the OTA client implementation. */
	zigbee_fota_signal_handler(bufid);
#endif /* CONFIG_ZIGBEE_FOTA */

	app_sfinder_signal_handler(bufid);

	/* Detect ZBOSS startup */
	switch (signal) {
	case ZB_ZDO_SIGNAL_SKIP_STARTUP:
		/* ZBOSS framework has started - schedule first weather check */
		err = app_wstation_start_measurements();
		if (err) {
			LOG_ERR("Failed to start weather measurements: %d", err);
		}
		break;
	default:
		break;
	}

	/* Let default signal handler process the signal*/
	ZB_ERROR_CHECK(zigbee_default_signal_handler(bufid));

	/*
	 * All callbacks should either reuse or free passed buffers.
	 * If bufid == 0, the buffer is invalid (not passed).
	 */
	if (bufid) {
		zb_buf_free(bufid);
	}
}

void main(void)
{
	int ret = settings_subsys_init();
	if (ret) {
		LOG_ERR("Unable to initialize settings: %d", ret);
	}

	#ifdef CONFIG_USB_DEVICE_STACK
	wait_for_console();
	#endif /* CONFIG_USB_DEVICE_STACK */

	register_factory_reset_button(FACTORY_RESET_BUTTON);
	gpio_init();

	/* Register device context (endpoints) */
	ZB_AF_REGISTER_DEVICE_CTX(&main_ctx);

#ifdef CONFIG_ZIGBEE_FOTA
	/* Initialize Zigbee FOTA download service. */
	zigbee_fota_init(ota_evt_handler);

	/* Mark the current firmware as valid. */
	confirm_image();
#endif /* CONFIG_ZIGBEE_FOTA */

	/* Init door lock  application */
	app_door_lock_init();

	/* Init weather station application */
	app_wstation_init(app_smeeze_handle_internal_temp);

	/* Init temperature sensor finder application */
	app_sfinder_init(
		app_smeeze_set_auto_mode,
		app_smeeze_set_desired_temp,
		app_smeeze_handle_external_temp);

	/* Register callback for handling ZCL commands. */
	ZB_ZCL_REGISTER_DEVICE_CB(zcl_device_cb);

	/* Enable Sleepy End Device behavior */
	zb_set_rx_on_when_idle(ZB_FALSE);

	/* Start Zigbee stack */
	zigbee_enable();
}
