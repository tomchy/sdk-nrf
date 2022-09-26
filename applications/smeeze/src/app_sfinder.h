/*
 * Copyright (c) 2022 Tomasz Chyrowicz
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef APP_SFINDER_H
#define APP_SFINDER_H

#include <zboss_api.h>

/** @brief Callback prototype for passing the new state of the auto mode. */
typedef void (*set_auto_mode_cb_t)(bool enabled);

/** @brief Callback prototype for passing the new desired temperature. */
typedef void (*desired_temp_cb_t)(int16_t value);

/** @brief Callback prototype for passing the new temperature readings from an external sensor. */
typedef void (*external_temp_cb_t)(int16_t value);


/** @brief The sensor finder application endpoint structure.
 */
extern zb_af_endpoint_desc_t sfinder_ep;

/** @brief Function for initializing sensor finder application.
 */
void app_sfinder_init(set_auto_mode_cb_t auto_mode_cb, desired_temp_cb_t des_temp_cb, external_temp_cb_t ext_temp_cb);

/** @brief Function for triggering the identify state on the sensor finder endpoint.
 */
void app_sfinder_start_identifying(zb_bufid_t bufid);

/** @brief Callback function for handling On/Off and Level Control ZCL commands.
 *
 * @param bufid  Reference to Zigbee stack buffer used to pass received data.
 */
void app_sfinder_zcl_cb(zb_bufid_t bufid);

/** @brief Callback function for handling ZBOSS signals.
 *
 * @note This callback does not consume ZBOSS signals, but uses them to trigger
 *       the F&B procedure once the device is commissioned or the previous
 *       procedure finishes with an error code.
 */
void app_sfinder_signal_handler(zb_bufid_t bufid);

/** @brief Set the value of the auto mode On/Off attribute.
 */
void app_finder_set_auto_mode(bool enabled);

#endif /* APP_SFINDER_H */
