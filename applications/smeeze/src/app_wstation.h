/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef APP_WSTATION_H
#define APP_WSTATION_H

#include <zboss_api.h>

/** @brief Callback prototype for passing the new reading of the local temperature.
 */
typedef void (*internal_temp_cb_t)(int16_t value);


/** @brief The weather station application endpoint structure.
 */
extern zb_af_endpoint_desc_t weather_station_ep;

/** @brief Function for initializing weather station application.
 */
void app_wstation_init(internal_temp_cb_t cb);

/** @brief Function for starting periodic weatehr sensor measurements.
 */
zb_ret_t app_wstation_start_measurements(void);

/** @brief Function for triggering the identify state on the weather station endpoint.
 */
void app_wstation_start_identifying(zb_bufid_t bufid);

#endif /* APP_WSTATION_H */
