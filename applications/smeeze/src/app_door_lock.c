/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/logging/log.h>
#include <dk_buttons_and_leds.h>
#include <zboss_api_addons.h>
#include <zigbee/zigbee_error_handler.h>
#include "door_lock.h"
#include "status_led.h"
#include "app_smeeze.h"


/* Time of LED on state while blinking for identify mode */
#define APP_DOOR_LOCK_IDENTIFY_LED_BLINK_TIME_MSEC 500


LOG_MODULE_REGISTER(app_door_lock, CONFIG_SMEEZE_DOOR_LOCK_LOG_LEVEL);

/* Stores all cluster-related attributes */
static struct zb_door_lock_ctx dev_ctx;

/* Declare attribute list for Identify cluster (client). */
ZB_ZCL_DECLARE_IDENTIFY_CLIENT_ATTRIB_LIST(
	dl_identify_client_attr_list);

/* Declare attribute list for Identify cluster (server). */
ZB_ZCL_DECLARE_IDENTIFY_SERVER_ATTRIB_LIST(
	dl_identify_server_attr_list,
	&dev_ctx.identify_attr.identify_time);

/* Declare attribute list for Groups cluster (server). */
ZB_ZCL_DECLARE_GROUPS_ATTRIB_LIST(
	dl_groups_attr_list,
	&dev_ctx.groups_attr.name_support);

/* Declare attribute list for Door Lock cluster (server). */
ZB_ZCL_DECLARE_DOOR_LOCK_ATTRIB_LIST(
	door_lock_attr_list,
	&dev_ctx.door_lock_attr.lock_state,
	&dev_ctx.door_lock_attr.lock_type,
	&dev_ctx.door_lock_attr.actuator_enabled);

/* Clusters setup */
ZB_HA_DECLARE_DOOR_LOCK_CLUSTER_LIST(
	door_lock_clusters,
	door_lock_attr_list,
	dl_identify_client_attr_list,
	dl_identify_server_attr_list,
	dl_groups_attr_list);

/* Endpoint setup (single) */
ZB_HA_DECLARE_DOOR_LOCK_EP(
	door_lock_ep,
	DOOR_LOCK_ENDPOINT_NB,
	door_lock_clusters);


static void door_lock_cb(bool locked)
{
	zb_uint8_t value = (locked ? ZB_ZCL_ATTR_DOOR_LOCK_LOCK_STATE_LOCKED : ZB_ZCL_ATTR_DOOR_LOCK_LOCK_STATE_UNLOCKED);

	status_led_color_remove(BLUE);
	ZVUNUSED(zb_zcl_set_attr_val(
			DOOR_LOCK_ENDPOINT_NB,
			ZB_ZCL_CLUSTER_ID_DOOR_LOCK,
			ZB_ZCL_CLUSTER_SERVER_ROLE,
			ZB_ZCL_ATTR_DOOR_LOCK_LOCK_STATE_ID,
			&value,
			ZB_FALSE));
}

/**@brief Functiom which essentially sets the lock state via PWM and stores the value in flash.
 *
 * @param[in] value ZB_ZCL_ATTR_DOOR_LOCK_LOCK_STATE_LOCKED to lock,
 *                  ZB_ZCL_ATTR_DOOR_LOCK_LOCK_STATE_UNLOCKED to unlock.
 */
static void set_lock_state(zb_uint8_t value)
{
	if (value == ZB_ZCL_ATTR_DOOR_LOCK_LOCK_STATE_LOCKED) {
		status_led_color_add(BLUE);
		door_lock_lock(door_lock_cb);
	} else if (value == ZB_ZCL_ATTR_DOOR_LOCK_LOCK_STATE_UNLOCKED) {
		status_led_color_add(BLUE);
		door_lock_unlock(door_lock_cb);
	} else {
		LOG_WRN("Wrong value of lock state - omitting");
		return;
	}
}

static void toggle_identify_led(zb_bufid_t bufid)
{
	static bool led_on;

	led_on = !led_on;
	if (led_on) {
		status_led_color_add(BLUE);
	} else {
		status_led_color_remove(BLUE);
	}
	zb_ret_t err = ZB_SCHEDULE_APP_ALARM(toggle_identify_led,
					     bufid,
					     ZB_MILLISECONDS_TO_BEACON_INTERVAL(
						     APP_DOOR_LOCK_IDENTIFY_LED_BLINK_TIME_MSEC));
	if (err) {
		LOG_ERR("Failed to schedule app alarm: %d", err);
	}
}

static void identify_callback(zb_bufid_t bufid)
{
	zb_ret_t err = RET_OK;

	if (bufid) {
		/* Schedule a self-scheduling function that will toggle the LED */
		err = ZB_SCHEDULE_APP_CALLBACK(toggle_identify_led, bufid);
		if (err) {
			LOG_ERR("Failed to schedule app callback: %d", err);
		} else {
			LOG_INF("Enter identify mode");
		}
	} else {
		/* Cancel the toggling function alarm and turn off LED */
		err = ZB_SCHEDULE_APP_ALARM_CANCEL(toggle_identify_led,
						   ZB_ALARM_ANY_PARAM);
		if (err) {
			LOG_ERR("Failed to schedule app alarm cancel: %d", err);
		} else {
			status_led_color_remove(BLUE);
			LOG_INF("Cancel identify mode");
		}
	}
}


void app_door_lock_init(void)
{
	/* Initialize the HW lock. */
	ZVUNUSED(door_lock_init());

	/* Identify cluster attributes */
	dev_ctx.identify_attr.identify_time = ZB_ZCL_IDENTIFY_IDENTIFY_TIME_DEFAULT_VALUE;

	/* Door Lock cluster attributes data */
	dev_ctx.door_lock_attr.lock_type = ZB_ZCL_ATTR_DOOR_LOCK_LOCK_TYPE_OTHER;
	dev_ctx.door_lock_attr.lock_state = (door_lock_is_locked() ? ZB_ZCL_ATTR_DOOR_LOCK_LOCK_STATE_LOCKED : ZB_ZCL_ATTR_DOOR_LOCK_LOCK_STATE_UNLOCKED);
	dev_ctx.door_lock_attr.actuator_enabled = ZB_TRUE;

	/* Sync with the HW lock state */
	set_lock_state(dev_ctx.door_lock_attr.lock_state);

	/* Register callback to identify notifications */
	ZB_AF_SET_IDENTIFY_NOTIFICATION_HANDLER(DOOR_LOCK_ENDPOINT_NB, identify_callback);
}

void app_door_lock_start_identifying(zb_bufid_t bufid)
{
	ZVUNUSED(bufid);

	if (ZB_JOINED()) {
		/*
		 * Check if endpoint is in identifying mode,
		 * if not put desired endpoint in identifying mode.
		 */
		if (dev_ctx.identify_attr.identify_time ==
		    ZB_ZCL_IDENTIFY_IDENTIFY_TIME_DEFAULT_VALUE) {

			zb_ret_t zb_err_code = zb_bdb_finding_binding_target(
				DOOR_LOCK_ENDPOINT_NB);

			if (zb_err_code == RET_OK) {
				LOG_INF("Manually enter identify mode");
			} else if (zb_err_code == RET_INVALID_STATE) {
				LOG_WRN("RET_INVALID_STATE - Cannot enter identify mode");
			} else {
				ZB_ERROR_CHECK(zb_err_code);
			}
		} else {
			LOG_INF("Manually cancel identify mode");
			zb_bdb_finding_binding_target_cancel();
		}
	} else {
		LOG_WRN("Device not in a network - cannot identify itself");
	}
}

void app_door_lock_zcl_cb(zb_bufid_t bufid)
{
	zb_zcl_device_callback_param_t * device_cb_param = ZB_BUF_GET_PARAM(bufid, zb_zcl_device_callback_param_t);

	/* Set default response value. */
	device_cb_param->status = RET_OK;

	switch (device_cb_param->device_cb_id) {
		case ZB_ZCL_DOOR_LOCK_UNLOCK_DOOR_CB_ID:
			set_lock_state(ZB_ZCL_ATTR_DOOR_LOCK_LOCK_STATE_UNLOCKED);
			app_smeeze_set_auto_mode(false);
			break;

		case ZB_ZCL_DOOR_LOCK_LOCK_DOOR_CB_ID:
			set_lock_state(ZB_ZCL_ATTR_DOOR_LOCK_LOCK_STATE_LOCKED);
			app_smeeze_set_auto_mode(false);
			break;

		default:
			device_cb_param->status = RET_NOT_IMPLEMENTED;
			break;
	}
}

void app_door_lock_toggle(void)
{
	if (door_lock_is_locked()) {
		set_lock_state(ZB_ZCL_ATTR_DOOR_LOCK_LOCK_STATE_UNLOCKED);
	} else {
		set_lock_state(ZB_ZCL_ATTR_DOOR_LOCK_LOCK_STATE_LOCKED);
	}
	app_smeeze_set_auto_mode(false);
}

void app_door_lock_lock_smeeze(void)
{
	if (door_lock_is_locked()) {
	} else {
		set_lock_state(ZB_ZCL_ATTR_DOOR_LOCK_LOCK_STATE_LOCKED);
	}
}

void app_door_lock_unlock_smeeze(void)
{
	if (door_lock_is_locked()) {
		set_lock_state(ZB_ZCL_ATTR_DOOR_LOCK_LOCK_STATE_UNLOCKED);
	} else {
	}
}
