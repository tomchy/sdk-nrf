/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/logging/log.h>
#include <dk_buttons_and_leds.h>
#include <zboss_api_addons.h>
#include <zigbee/zigbee_error_handler.h>
#include "sfinder.h"
#include "app_sfinder.h"
#include "status_led.h"


/* Time of LED on state while blinking for identify mode */
#define APP_SFINDER_IDENTIFY_LED_BLINK_TIME_MSEC 500

/* Zigbee Cluster Library 4.4.2.2.1.1: MeasuredValue = 100x temperature in degrees Celsius */
#define ZCL_TEMPERATURE_MEASUREMENT_MEASURED_VALUE_MULTIPLIER 100


LOG_MODULE_REGISTER(app_sfinder, CONFIG_SMEEZE_SFINDER_LOG_LEVEL);

static set_auto_mode_cb_t set_auto_mode_cb = NULL;
static desired_temp_cb_t desired_temp_cb = NULL;
static external_temp_cb_t external_temp_cb = NULL;

/* Stores all cluster-related attributes */
static struct zb_sfinder_ctx dev_ctx;

/* Declare attribute list for Identify cluster (client). */
ZB_ZCL_DECLARE_IDENTIFY_CLIENT_ATTRIB_LIST(
	sfinder_identify_client_attr_list);

/* Declare attribute list for Identify cluster (server). */
ZB_ZCL_DECLARE_IDENTIFY_SERVER_ATTRIB_LIST(
	sfinder_identify_server_attr_list,
	&dev_ctx.identify_attr.identify_time);

/* Declare attribute list for On/Off cluster (server). */
ZB_ZCL_DECLARE_ON_OFF_ATTRIB_LIST(
	sfinder_on_off_attr_list,
	&dev_ctx.on_off_attr.on_off);

/* Declare attribute list for Level Control cluster (server). */
ZB_ZCL_DECLARE_LEVEL_CONTROL_ATTRIB_LIST(
	sfinder_level_control_attr_list,
	&dev_ctx.level_control_attr.current_level,
	&dev_ctx.level_control_attr.remaining_time);

/* Declare attribute list for Temperature Measurement cluster (client). */
ZB_ZCL_DECLARE_TEMP_MEASUREMENT_CLIENT_ATTRIB_LIST(
	sfinder_temp_measurement_client_attr_list);

/* Clusters setup */
ZB_HA_DECLARE_SFINDER_CLUSTER_LIST(
	sfinder_cluster_list,
	sfinder_identify_client_attr_list,
	sfinder_identify_server_attr_list,
	sfinder_on_off_attr_list,
	sfinder_level_control_attr_list,
	sfinder_temp_measurement_client_attr_list);

/* Endpoint setup (single) */
ZB_HA_DECLARE_SFINDER_EP(
	sfinder_ep,
	SFINDER_ENDPOINT_NB,
	sfinder_cluster_list);


/**@brief Handle the Report Attribute Command
 *
 * @param[in] zcl_hdr	   Pointer to parsed ZCL header
 * @param[in] bufid		 ZBOSS buffer id
 */
static void handle_attr_update(zb_zcl_parsed_hdr_t *zcl_hdr, zb_bufid_t bufid)
{
	zb_zcl_report_attr_req_t *attr_resp = NULL;
	zb_zcl_addr_t remote_node_data = zcl_hdr->addr_data.common_data.source;

	if (remote_node_data.addr_type == ZB_ZCL_ADDR_TYPE_SHORT) {
		LOG_INF("Received value updates from the remote node 0x%04x",
			remote_node_data.u.short_addr);
	} else {
		LOG_INF("Received value updates from the remote node: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
				remote_node_data.u.ieee_addr[0], remote_node_data.u.ieee_addr[1],
				remote_node_data.u.ieee_addr[2], remote_node_data.u.ieee_addr[3],
				remote_node_data.u.ieee_addr[4], remote_node_data.u.ieee_addr[5],
				remote_node_data.u.ieee_addr[6], remote_node_data.u.ieee_addr[7]);
	}

	/* Get the contents of Read Attribute Response frame. */
	ZB_ZCL_GENERAL_GET_NEXT_REPORT_ATTR_REQ(bufid, attr_resp);
	while (attr_resp != NULL) {
		if ((zcl_hdr->profile_id == ZB_AF_HA_PROFILE_ID) &&
			(zcl_hdr->cluster_id == ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT) &&
			(attr_resp->attr_id == ZB_ZCL_ATTR_TEMP_MEASUREMENT_VALUE_ID)) {
			zb_int16_t value = *((zb_uint16_t *)attr_resp->attr_value);
			LOG_INF("Received new temperature data: %d.%02d", value / 100, value % 100);
			if (external_temp_cb) {
				external_temp_cb(value);
			}
		}

		ZB_ZCL_GENERAL_GET_NEXT_REPORT_ATTR_REQ(bufid, attr_resp);
	}
}

/**@brief The Handler to 'intercept' every frame coming to the endpoint
 *
 * @param[in] bufid   ZBOSS buffer id.
 *
 * @returns ZB_TRUE if ZCL command was processed.
 */
static zb_uint8_t app_sfinder_ep_handler(zb_bufid_t bufid)
{
	zb_zcl_parsed_hdr_t *cmd_info = ZB_BUF_GET_PARAM(bufid, zb_zcl_parsed_hdr_t);

	if (cmd_info->cmd_id == ZB_ZCL_CMD_REPORT_ATTRIB) {
		handle_attr_update(cmd_info, bufid);
		zb_buf_free(bufid);
		return ZB_TRUE;

	}

	return ZB_FALSE;
}


/**@brief Callback to finding and binding procedure.
 *
 * @param[IN]   status  Procedure status.
 * @param[IN]   addr	Found device address.
 * @param[IN]   ep	  Found device endpoint.
 * @param[IN]   cluster Common cluster ID.
 *
 * @return	  Returned boolean value is used to decide if found device's cluster (ID given as parameter) should be bound.
 */
static zb_bool_t finding_n_binding_cb(zb_int16_t status, zb_ieee_addr_t addr, zb_uint8_t ep, zb_uint16_t cluster)
{
	zb_bool_t ret = ZB_FALSE;

	switch (status)
	{
		case ZB_BDB_COMM_BIND_SUCCESS:
			LOG_INF("Successfully bound with: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
				addr[0], addr[1],
				addr[2], addr[3],
				addr[4], addr[5],
				addr[6], addr[7]);
			sfinder_found(addr, ep);
			break;

		case ZB_BDB_COMM_BIND_FAIL:
			LOG_INF("Failed to bind");
			break;

		case ZB_BDB_COMM_BIND_ASK_USER:
			switch (cluster)
			{
				case ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT:
					LOG_INF("Trying to bind cluster %hd", cluster);
					ret = ZB_TRUE;
					break;
				default:
					/* We are not interested in this cluster. */
					break;
			}
			break;

		default:
			/* Should not happen */
			break;
	}

	return ret;
}


/**@brief Function to start the Finding & Binding Procedure.
 *		If the Finding & Binding Procedure was already started, cancel it.
 */
static void toggle_find_n_bind(zb_bufid_t unused)
{
	zb_ret_t zb_err_code;

	zb_err_code = zb_bdb_finding_binding_initiator(SFINDER_ENDPOINT_NB, finding_n_binding_cb);
	if (zb_err_code == RET_OK)
	{
		status_led_color_add(YELLOW);
		LOG_INF("F&B: Started Finding & Binding procedure on the endpoint %d.", SFINDER_ENDPOINT_NB);
	}
	else if (zb_err_code == RET_BUSY)
	{
		zb_bdb_finding_binding_initiator_cancel();
		status_led_color_remove(YELLOW);
		status_led_color_blink(RED);
	}
	else if (zb_err_code == RET_INVALID_STATE)
	{
		LOG_WRN("Device not yet commissionned!");
		status_led_color_blink(YELLOW);
	}
}

static void toggle_identify_led(zb_bufid_t bufid)
{
	static bool led_on;

	led_on = !led_on;
	if (led_on) {
		status_led_color_add(GREEN);
	} else {
		status_led_color_remove(GREEN);
	}
	zb_ret_t err = ZB_SCHEDULE_APP_ALARM(toggle_identify_led,
						 bufid,
						 ZB_MILLISECONDS_TO_BEACON_INTERVAL(
							 APP_SFINDER_IDENTIFY_LED_BLINK_TIME_MSEC));
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
			status_led_color_remove(GREEN);
			LOG_INF("Cancel identify mode");
		}
	}
}


void app_sfinder_init(set_auto_mode_cb_t auto_mode_cb, desired_temp_cb_t des_temp_cb, external_temp_cb_t ext_temp_cb)
{
	/* Store callbacks */
	set_auto_mode_cb = auto_mode_cb;
	desired_temp_cb = des_temp_cb;
	external_temp_cb = ext_temp_cb;

	/* Initialize the HW lock. */
	ZVUNUSED(sfinder_init());

	/* Identify cluster attributes */
	dev_ctx.identify_attr.identify_time = ZB_ZCL_IDENTIFY_IDENTIFY_TIME_DEFAULT_VALUE;

	/* On/Off cluster attributes */
	dev_ctx.on_off_attr.on_off = (zb_bool_t)ZB_ZCL_ON_OFF_IS_ON;

	/* Level Control cluster attributes */
	dev_ctx.level_control_attr.current_level = ZB_ZCL_LEVEL_CONTROL_LEVEL_MAX_VALUE;
	dev_ctx.level_control_attr.remaining_time = ZB_ZCL_LEVEL_CONTROL_REMAINING_TIME_DEFAULT_VALUE;

	/* Register callback to identify notifications */
	ZB_AF_SET_IDENTIFY_NOTIFICATION_HANDLER(SFINDER_ENDPOINT_NB, identify_callback);

	/* Register endpoint handler for receiving attribute reports */
	ZB_AF_SET_ENDPOINT_HANDLER(SFINDER_ENDPOINT_NB, app_sfinder_ep_handler);
}

void app_sfinder_start_identifying(zb_bufid_t bufid)
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
				SFINDER_ENDPOINT_NB);

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

void app_sfinder_zcl_cb(zb_bufid_t bufid)
{
	zb_zcl_device_callback_param_t * device_cb_param = ZB_BUF_GET_PARAM(bufid, zb_zcl_device_callback_param_t);
	zb_uint8_t cluster_id;
	zb_uint8_t attr_id;

	/* Set default response value. */
	device_cb_param->status = RET_OK;

	switch (device_cb_param->device_cb_id) {
	case ZB_ZCL_LEVEL_CONTROL_SET_VALUE_CB_ID:
		LOG_INF("Setting the desired temperature level to: %d",
			device_cb_param->cb_param.level_control_set_value_param.new_value);
		if (desired_temp_cb != NULL) {
			uint16_t value = device_cb_param->cb_param.level_control_set_value_param.new_value;
			desired_temp_cb(value * ZCL_TEMPERATURE_MEASUREMENT_MEASURED_VALUE_MULTIPLIER);
		}
		break;

	case ZB_ZCL_SET_ATTR_VALUE_CB_ID:
		cluster_id = device_cb_param->cb_param.set_attr_value_param.cluster_id;
		attr_id = device_cb_param->cb_param.set_attr_value_param.attr_id;

		if ((cluster_id == ZB_ZCL_CLUSTER_ID_ON_OFF) &&
			(attr_id == ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID)) {
			uint8_t value = device_cb_param->cb_param.set_attr_value_param.values.data8;

			LOG_INF("%sabling automatic temperature control", (value ? "En" : "Dis"));
			if (set_auto_mode_cb) {
				set_auto_mode_cb(value);
			}
		} else if ((cluster_id == ZB_ZCL_CLUSTER_ID_LEVEL_CONTROL) &&
			   (attr_id == ZB_ZCL_ATTR_LEVEL_CONTROL_CURRENT_LEVEL_ID)) {
			uint16_t value = device_cb_param->cb_param.set_attr_value_param.values.data16;

			LOG_INF("Setting the desired temperature level to: %d", value);
			if (desired_temp_cb != NULL) {
				desired_temp_cb(value * ZCL_TEMPERATURE_MEASUREMENT_MEASURED_VALUE_MULTIPLIER);
			}
		} else {
			/* Other clusters can be processed here */
			LOG_INF("Unhandled cluster attribute id: (cluster: %d attribute: %d)", cluster_id, attr_id);
			device_cb_param->status = RET_NOT_IMPLEMENTED;
		}
		break;

	default:
		device_cb_param->status = RET_NOT_IMPLEMENTED;
		break;
	}
}

void app_sfinder_signal_handler(zb_bufid_t bufid)
{
	zb_zdo_app_signal_hdr_t *sig_handler = NULL;
	zb_zdo_app_signal_type_t sig = zb_get_app_signal(bufid, &sig_handler);
	zb_ret_t status = ZB_GET_APP_SIGNAL_STATUS(bufid);

	switch (sig) {
	case ZB_BDB_SIGNAL_DEVICE_REBOOT:
	/* fall-through */
	case ZB_BDB_SIGNAL_STEERING:
		if (status == RET_OK) {
			if (!sfinder_is_found()) {
				zb_ret_t err = ZB_SCHEDULE_APP_ALARM(toggle_find_n_bind, 0,
					ZB_SECONDS_TO_BEACON_INTERVAL(CONFIG_FIND_SENSOR_DELAY_SECONDS));
				if (err != RET_OK) {
					LOG_ERR("Unable to start looking for external sensor: %d", err);
				}
			}
		}
		break;

	case ZB_BDB_SIGNAL_FINDING_AND_BINDING_INITIATOR_FINISHED:
		{
			zb_zdo_signal_fb_initiator_finished_params_t * f_n_b_status =
				ZB_ZDO_SIGNAL_GET_PARAMS(sig_handler, zb_zdo_signal_fb_initiator_finished_params_t);
			status_led_color_remove(YELLOW);

			switch(f_n_b_status->status) {
			case ZB_ZDO_FB_INITIATOR_STATUS_SUCCESS:
				LOG_INF("F&B: Remote peer has been bound.");
				break;
			case ZB_ZDO_FB_INITIATOR_STATUS_CANCEL:
				LOG_INF("F&B: Initiator process was cancelled.");
				status_led_color_blink(RED);
				break;
			case ZB_ZDO_FB_INITIATOR_STATUS_ALARM:
				LOG_INF("F&B: Initiator process was timed out.");
				status_led_color_blink(RED);
				break;
			case ZB_ZDO_FB_INITIATOR_STATUS_ERROR:
				LOG_ERR("F&B: Error.");
				status_led_color_blink(RED);
				break;
			default:
				LOG_ERR("F&B: Unknown error, status %d.", f_n_b_status->status);
				status_led_color_blink(RED);
				break;
			}

			if (!sfinder_is_found()) {
				zb_ret_t err = ZB_SCHEDULE_APP_ALARM(toggle_find_n_bind, 0,
					ZB_SECONDS_TO_BEACON_INTERVAL(CONFIG_FIND_SENSOR_RETRY_INTERVAL_SECONDS));
				if (err != RET_OK) {
					LOG_ERR("Unable to restart looking for external sensor: %d", err);
				}
			}
		}
		break;

	default:
		break;
	}
}

void app_finder_set_auto_mode(bool enabled)
{
	zb_uint8_t on_off = (enabled ? 1 : 0);

	ZVUNUSED(zb_zcl_set_attr_val(
			SFINDER_ENDPOINT_NB,
			ZB_ZCL_CLUSTER_ID_ON_OFF,
			ZB_ZCL_CLUSTER_SERVER_ROLE,
			ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID,
			&on_off,
			ZB_FALSE));
}
