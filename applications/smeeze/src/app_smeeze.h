/*
 * Copyright (c) 2022 Tomasz Chyrowicz
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef APP_SMEEZE_H_
#define APP_SMEEZE_H_

#include <zboss_api.h>


/** @brief Callback function for setting the new state of the auto mode. */
void app_smeeze_set_auto_mode(bool enabled);

/** @brief Callback function for setting the new desired temperature. */
void app_smeeze_set_desired_temp(int16_t value);

/** @brief Callback function for passing the new temperature readings from an external sensor. */
void app_smeeze_handle_external_temp(int16_t value);

/** @brief Callback function for passing the new reading of the local temperature. */
void app_smeeze_handle_internal_temp(int16_t value);

#endif /* APP_SMEEZE_H_ */