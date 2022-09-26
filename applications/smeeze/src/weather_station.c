/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/logging/log.h>

#include "weather_station.h"

LOG_MODULE_DECLARE(app, CONFIG_SMEEZE_LOG_LEVEL);

int weather_station_init(void)
{
	int err = sensor_init();

	if (err) {
		LOG_ERR("Failed to initialize sensor: %d", err);
	}

	return err;
}

int weather_station_check_weather(void)
{
	int err = sensor_update_measurements();

	if (err) {
		LOG_ERR("Failed to update sensor");
	}

	return err;
}

int weather_station_update_temperature(void)
{
	int err = 0;

	float measured_temperature = 0.0f;
	int16_t temperature_attribute = 0;

	err = sensor_get_temperature(&measured_temperature);
	if (err) {
		LOG_ERR("Failed to get sensor temperature: %d", err);
	} else {
		/* Convert measured value to attribute value, as specified in ZCL */
		temperature_attribute = (int16_t)
					(measured_temperature *
					 ZCL_TEMPERATURE_MEASUREMENT_MEASURED_VALUE_MULTIPLIER);
		LOG_INF("Attribute T:%10d", temperature_attribute);

		/* Set ZCL attribute */
		zb_zcl_status_t status = zb_zcl_set_attr_val(WEATHER_STATION_ENDPOINT_NB,
							     ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT,
							     ZB_ZCL_CLUSTER_SERVER_ROLE,
							     ZB_ZCL_ATTR_TEMP_MEASUREMENT_VALUE_ID,
							     (zb_uint8_t *)&temperature_attribute,
							     ZB_FALSE);
		if (status) {
			LOG_ERR("Failed to set ZCL attribute: %d", status);
			err = status;
		}
	}

	return err;
}

int weather_station_update_pressure(void)
{
	int err = 0;

	float measured_pressure = 0.0f;
	int16_t pressure_attribute = 0;

	err = sensor_get_pressure(&measured_pressure);
	if (err) {
		LOG_ERR("Failed to get sensor pressure: %d", err);
	} else {
		/* Convert measured value to attribute value, as specified in ZCL */
		pressure_attribute = (int16_t)
				     (measured_pressure *
				      ZCL_PRESSURE_MEASUREMENT_MEASURED_VALUE_MULTIPLIER);
		LOG_INF("Attribute P:%10d", pressure_attribute);

		/* Set ZCL attribute */
		zb_zcl_status_t status = zb_zcl_set_attr_val(
			WEATHER_STATION_ENDPOINT_NB,
			ZB_ZCL_CLUSTER_ID_PRESSURE_MEASUREMENT,
			ZB_ZCL_CLUSTER_SERVER_ROLE,
			ZB_ZCL_ATTR_PRESSURE_MEASUREMENT_VALUE_ID,
			(zb_uint8_t *)&pressure_attribute,
			ZB_FALSE);
		if (status) {
			LOG_ERR("Failed to set ZCL attribute: %d", status);
			err = status;
		}
	}

	return err;
}

int weather_station_update_humidity(void)
{
	int err = 0;

	float measured_humidity = 0.0f;
	int16_t humidity_attribute = 0;

	err = sensor_get_humidity(&measured_humidity);
	if (err) {
		LOG_ERR("Failed to get sensor humidity: %d", err);
	} else {
		/* Convert measured value to attribute value, as specified in ZCL */
		humidity_attribute = (int16_t)
				     (measured_humidity *
				      ZCL_HUMIDITY_MEASUREMENT_MEASURED_VALUE_MULTIPLIER);
		LOG_INF("Attribute H:%10d", humidity_attribute);

		zb_zcl_status_t status = zb_zcl_set_attr_val(
			WEATHER_STATION_ENDPOINT_NB,
			ZB_ZCL_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT,
			ZB_ZCL_CLUSTER_SERVER_ROLE,
			ZB_ZCL_ATTR_REL_HUMIDITY_MEASUREMENT_VALUE_ID,
			(zb_uint8_t *)&humidity_attribute,
			ZB_FALSE);
		if (status) {
			LOG_ERR("Failed to set ZCL attribute: %d", status);
			err = status;
		}
	}

	return err;
}

static zb_ret_t get_rated_voltage_value(zb_uint8_t *voltage_mv)
{
	zb_zcl_attr_t *attr_desc = zb_zcl_get_attr_desc_a(
		WEATHER_STATION_ENDPOINT_NB,
		ZB_ZCL_CLUSTER_ID_POWER_CONFIG,
		ZB_ZCL_CLUSTER_SERVER_ROLE,
		ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_RATED_VOLTAGE_ID);

	if (attr_desc != NULL) {
		*voltage_mv = ZB_ZCL_GET_ATTRIBUTE_VAL_8(attr_desc);
		return RET_OK;
	}

	return RET_ERROR;
}

static zb_ret_t get_min_voltage_value(zb_uint8_t *voltage_mv)
{
	zb_zcl_attr_t *attr_desc = zb_zcl_get_attr_desc_a(
		WEATHER_STATION_ENDPOINT_NB,
		ZB_ZCL_CLUSTER_ID_POWER_CONFIG,
		ZB_ZCL_CLUSTER_SERVER_ROLE,
		ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_VOLTAGE_MIN_THRESHOLD_ID);

	if (attr_desc != NULL) {
		*voltage_mv = ZB_ZCL_GET_ATTRIBUTE_VAL_8(attr_desc);
		return RET_OK;
	}

	return RET_ERROR;
}

int weather_station_update_voltage(void)
{
	zb_uint16_t remaining_mv;
	zb_uint8_t remaining_dmv;
	int err = 0;

	err = sensor_get_voltage(&remaining_mv);
	if (err) {
		LOG_ERR("Failed to get battery voltage: %d", err);
	} else {
		remaining_dmv = remaining_mv / 100;
		LOG_INF("Attribute batt: %6d [0.1V]", remaining_dmv);
		zb_zcl_status_t status = zb_zcl_set_attr_val(
			WEATHER_STATION_ENDPOINT_NB,
			ZB_ZCL_CLUSTER_ID_POWER_CONFIG,
			ZB_ZCL_CLUSTER_SERVER_ROLE,
			ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_VOLTAGE_ID,
			(zb_uint8_t *)&remaining_dmv,
			ZB_FALSE);
		if (status) {
			LOG_ERR("Failed to set ZCL battery voltage attribute: %d", status);
			err = status;
		}

		zb_uint8_t rated_mv;
		zb_uint8_t min_mv;
		zb_ret_t ret_code = get_rated_voltage_value(&rated_mv);
		if (ret_code == RET_OK) {
			ret_code = get_min_voltage_value(&min_mv);
		}
		if (ret_code == RET_OK) {
			uint8_t remaining = (uint8_t)((((uint16_t)(remaining_mv - min_mv * 100)) * 200) / ((uint16_t)(rated_mv - min_mv) * 100));
			LOG_INF("Attribute batt: %6d [0.5%%]", remaining);
			status = zb_zcl_set_attr_val(
				WEATHER_STATION_ENDPOINT_NB,
				ZB_ZCL_CLUSTER_ID_POWER_CONFIG,
				ZB_ZCL_CLUSTER_SERVER_ROLE,
				ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_PERCENTAGE_REMAINING_ID,
				(zb_uint8_t *)&remaining,
				ZB_FALSE);
		} else {
			status = ZB_ZCL_STATUS_UNSUP_ATTRIB;
		}
		if (status) {
			LOG_ERR("Failed to set ZCL battery remaining attribute: %d", status);
			err = status;
		}
	}

	return err;
}