
/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/logging/log.h>
#include <dk_buttons_and_leds.h>
#include <zboss_api_addons.h>
#include <zigbee/zigbee_error_handler.h>
#include "weather_station.h"
#include "status_led.h"
#include "app_wstation.h"


/* Weather check period */
#define WEATHER_CHECK_PERIOD_MSEC (1000 * CONFIG_WEATHER_CHECK_PERIOD_SECONDS)

/* Delay for first weather check */
#define WEATHER_CHECK_INITIAL_DELAY_MSEC (1000 * CONFIG_FIRST_WEATHER_CHECK_DELAY_SECONDS)

/* Time of LED on state while blinking for identify mode */
#define APP_WSTATION_IDENTIFY_LED_BLINK_TIME_MSEC 500

/* Manufacturer name (32 bytes). */
#define WSTATION_MANUF_NAME      "Smeeze"

/* Model number assigned by manufacturer (32-bytes long string). */
#define WSTATION_MODEL_ID        "Smeeze_v0.2"

/* First 8 bytes specify the date of manufacturer of the device
 * in ISO 8601 format (YYYYMMDD). The rest (8 bytes) are manufacturer specific.
 */
#define WSTATION_DATE_CODE       "20221012"

/* Describes the physical location of the device (16 bytes).
 * May be modified during commissioning process.
 */
#define WSTATION_LOCATION_DESC   "Door leaf"

LOG_MODULE_REGISTER(app_wstation, CONFIG_SMEEZE_WSTATION_LOG_LEVEL);

static internal_temp_cb_t temp_cb;

/* Stores all cluster-related attributes */
static struct zb_weather_station_ctx dev_ctx;


/* Attributes setup */
ZB_ZCL_DECLARE_BASIC_ATTRIB_LIST_EXT(
	ws_basic_attr_list,
	&dev_ctx.basic_attr.zcl_version,
	&dev_ctx.basic_attr.app_version,
	&dev_ctx.basic_attr.stack_version,
	&dev_ctx.basic_attr.hw_version,
	dev_ctx.basic_attr.mf_name,
	dev_ctx.basic_attr.model_id,
	dev_ctx.basic_attr.date_code,
	&dev_ctx.basic_attr.power_source,
	dev_ctx.basic_attr.location_id,
	&dev_ctx.basic_attr.ph_env,
	dev_ctx.basic_attr.sw_ver);

/* Declare attribute list for Identify cluster (client). */
ZB_ZCL_DECLARE_IDENTIFY_CLIENT_ATTRIB_LIST(
	ws_identify_client_attr_list);

/* Declare attribute list for Identify cluster (server). */
ZB_ZCL_DECLARE_IDENTIFY_SERVER_ATTRIB_LIST(
	ws_identify_server_attr_list,
	&dev_ctx.identify_attr.identify_time);

ZB_ZCL_DECLARE_POWER_CONFIG_ATTRIB_LIST_REMAINING(
	ws_power_cfg_attr_list,
	&dev_ctx.power_attrs.voltage,
	&dev_ctx.power_attrs.size,
	&dev_ctx.power_attrs.quantity,
	&dev_ctx.power_attrs.rated_voltage,
	&dev_ctx.power_attrs.alarm_mask,
	&dev_ctx.power_attrs.voltage_min_threshold,
	&dev_ctx.power_attrs.remaining,
	&dev_ctx.power_attrs.min_threshold
	);

ZB_ZCL_DECLARE_TEMP_MEASUREMENT_ATTRIB_LIST(
	ws_temperature_measurement_attr_list,
	&dev_ctx.temp_attrs.measure_value,
	&dev_ctx.temp_attrs.min_measure_value,
	&dev_ctx.temp_attrs.max_measure_value,
	&dev_ctx.temp_attrs.tolerance
	);

ZB_ZCL_DECLARE_PRESSURE_MEASUREMENT_ATTRIB_LIST(
	ws_pressure_measurement_attr_list,
	&dev_ctx.pres_attrs.measure_value,
	&dev_ctx.pres_attrs.min_measure_value,
	&dev_ctx.pres_attrs.max_measure_value,
	&dev_ctx.pres_attrs.tolerance
	);

ZB_ZCL_DECLARE_REL_HUMIDITY_MEASUREMENT_ATTRIB_LIST(
	ws_humidity_measurement_attr_list,
	&dev_ctx.humidity_attrs.measure_value,
	&dev_ctx.humidity_attrs.min_measure_value,
	&dev_ctx.humidity_attrs.max_measure_value
	);

/* Clusters setup */
ZB_HA_DECLARE_WEATHER_STATION_CLUSTER_LIST(
	weather_station_cluster_list,
	ws_basic_attr_list,
	ws_identify_client_attr_list,
	ws_identify_server_attr_list,
	ws_power_cfg_attr_list,
	ws_temperature_measurement_attr_list,
	ws_pressure_measurement_attr_list,
	ws_humidity_measurement_attr_list);

/* Endpoint setup (single) */
ZB_HA_DECLARE_WEATHER_STATION_EP(
	weather_station_ep,
	WEATHER_STATION_ENDPOINT_NB,
	weather_station_cluster_list);

static zb_ret_t get_temperature_value(zb_int16_t *temperature)
{
	zb_zcl_attr_t *attr_desc = zb_zcl_get_attr_desc_a(
		WEATHER_STATION_ENDPOINT_NB,
		ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT,
		ZB_ZCL_CLUSTER_SERVER_ROLE,
		ZB_ZCL_ATTR_TEMP_MEASUREMENT_VALUE_ID);

	if (attr_desc != NULL) {
		*temperature = ZB_ZCL_GET_ATTRIBUTE_VAL_16(attr_desc);
		return RET_OK;
	}

	return RET_ERROR;
}

static void check_weather(zb_bufid_t bufid)
{
	ZVUNUSED(bufid);

	int err = weather_station_check_weather();

	if (err) {
		LOG_ERR("Failed to check weather: %d", err);
	} else {
		err = weather_station_update_temperature();
		if (err) {
			LOG_ERR("Failed to update temperature: %d", err);
		}

		err = weather_station_update_pressure();
		if (err) {
			LOG_ERR("Failed to update pressure: %d", err);
		}

		err = weather_station_update_humidity();
		if (err) {
			LOG_ERR("Failed to update humidity: %d", err);
		}
	}

	if (!err) {
		err = weather_station_update_voltage();
		if (err) {
			LOG_ERR("Failed to update battery voltage: %d", err);
		}
	}

	if (temp_cb) {
		zb_int16_t temperature = 0;
		if (get_temperature_value(&temperature) == RET_OK) {
			temp_cb(temperature);
		}
	}

	zb_ret_t zb_err = ZB_SCHEDULE_APP_ALARM(check_weather,
						0,
						ZB_MILLISECONDS_TO_BEACON_INTERVAL(
							WEATHER_CHECK_PERIOD_MSEC));
	if (zb_err) {
		LOG_ERR("Failed to schedule app alarm: %d", zb_err);
	}
}

static void toggle_identify_led(zb_bufid_t bufid)
{
	static bool led_on;

	led_on = !led_on;
		if (led_on) {
		status_led_color_add(RED);
	} else {
		status_led_color_remove(RED);
	}
	zb_ret_t err = ZB_SCHEDULE_APP_ALARM(toggle_identify_led,
					     bufid,
					     ZB_MILLISECONDS_TO_BEACON_INTERVAL(
						     APP_WSTATION_IDENTIFY_LED_BLINK_TIME_MSEC));
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
			status_led_color_remove(RED);
			LOG_INF("Cancel identify mode");
		}
	}
}


void app_wstation_init(internal_temp_cb_t cb)
{
	temp_cb = cb;

	/* Basic cluster attributes */
	dev_ctx.basic_attr.zcl_version = ZB_ZCL_VERSION;
	dev_ctx.basic_attr.app_version = 01; // 1-byte
	dev_ctx.basic_attr.stack_version = ((ZBOSS_MAJOR & 0x0F) << 4) | (ZBOSS_MINOR & 0x0F); // 1-byte
	dev_ctx.basic_attr.hw_version = 53; // 1-byte

	/* Use ZB_ZCL_SET_STRING_VAL to set strings, because the first byte
	 * should contain string length without trailing zero.
	 *
	 * For example "test" string will be encoded as:
	 *   [(0x4), 't', 'e', 's', 't']
	 */
	ZB_ZCL_SET_STRING_VAL(
		dev_ctx.basic_attr.mf_name,
		WSTATION_MANUF_NAME,
		ZB_ZCL_STRING_CONST_SIZE(WSTATION_MANUF_NAME));

	ZB_ZCL_SET_STRING_VAL(
		dev_ctx.basic_attr.model_id,
		WSTATION_MODEL_ID,
		ZB_ZCL_STRING_CONST_SIZE(WSTATION_MODEL_ID));

	ZB_ZCL_SET_STRING_VAL(
		dev_ctx.basic_attr.date_code,
		WSTATION_DATE_CODE,
		ZB_ZCL_STRING_CONST_SIZE(WSTATION_DATE_CODE));

	dev_ctx.basic_attr.power_source = ZB_ZCL_BASIC_POWER_SOURCE_BATTERY;

	ZB_ZCL_SET_STRING_VAL(
		dev_ctx.basic_attr.location_id,
		WSTATION_LOCATION_DESC,
		ZB_ZCL_STRING_CONST_SIZE(WSTATION_LOCATION_DESC));

	dev_ctx.basic_attr.ph_env = 0x3E; /* Hobby/Craft Room */

	/* Identify cluster attributes */
	dev_ctx.identify_attr.identify_time = ZB_ZCL_IDENTIFY_IDENTIFY_TIME_DEFAULT_VALUE;

	/* Power config attributes */
	dev_ctx.power_attrs.voltage = ZB_ZCL_POWER_CONFIG_BATTERY_VOLTAGE_INVALID;
	dev_ctx.power_attrs.size = ZB_ZCL_POWER_CONFIG_BATTERY_SIZE_BUILT_IN;
	dev_ctx.power_attrs.quantity = 1;
	dev_ctx.power_attrs.rated_voltage = 41; /* Unit: 100mV */
	dev_ctx.power_attrs.alarm_mask = ZB_ZCL_POWER_CONFIG_BATTERY_ALARM_MASK_VOLTAGE_LOW;
	dev_ctx.power_attrs.voltage_min_threshold = 35; /* Unit: 100mV */
	dev_ctx.power_attrs.remaining = 100 * 2; /* Unit: half percent. */
	dev_ctx.power_attrs.min_threshold = 10 * 2; /* Unit: half percent. */

	/* Temperature */
	dev_ctx.temp_attrs.measure_value = ZB_ZCL_ATTR_TEMP_MEASUREMENT_VALUE_UNKNOWN;
	dev_ctx.temp_attrs.min_measure_value = WEATHER_STATION_ATTR_TEMP_MIN;
	dev_ctx.temp_attrs.max_measure_value = WEATHER_STATION_ATTR_TEMP_MAX;
	dev_ctx.temp_attrs.tolerance = WEATHER_STATION_ATTR_TEMP_TOLERANCE;

	/* Pressure */
	dev_ctx.pres_attrs.measure_value = ZB_ZCL_ATTR_PRESSURE_MEASUREMENT_VALUE_UNKNOWN;
	dev_ctx.pres_attrs.min_measure_value = WEATHER_STATION_ATTR_PRESSURE_MIN;
	dev_ctx.pres_attrs.max_measure_value = WEATHER_STATION_ATTR_PRESSURE_MAX;
	dev_ctx.pres_attrs.tolerance = WEATHER_STATION_ATTR_PRESSURE_TOLERANCE;

	/* Humidity */
	dev_ctx.humidity_attrs.measure_value = ZB_ZCL_ATTR_REL_HUMIDITY_MEASUREMENT_VALUE_UNKNOWN;
	dev_ctx.humidity_attrs.min_measure_value = WEATHER_STATION_ATTR_HUMIDITY_MIN;
	dev_ctx.humidity_attrs.max_measure_value = WEATHER_STATION_ATTR_HUMIDITY_MAX;
	/* Humidity measurements tolerance is not supported at the moment */

	weather_station_init();

	/* Register callback to identify notifications */
	ZB_AF_SET_IDENTIFY_NOTIFICATION_HANDLER(WEATHER_STATION_ENDPOINT_NB, identify_callback);
}

zb_ret_t app_wstation_start_measurements(void)
{
	return ZB_SCHEDULE_APP_ALARM(check_weather,
				    0,
				    ZB_MILLISECONDS_TO_BEACON_INTERVAL(
					    WEATHER_CHECK_INITIAL_DELAY_MSEC));
}

void app_wstation_start_identifying(zb_bufid_t bufid)
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
				WEATHER_STATION_ENDPOINT_NB);

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
