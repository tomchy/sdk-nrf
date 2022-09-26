/*
 * Copyright (c) 2022 Tomasz Chyrowicz
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef APP_DOOR_LOCK_H
#define APP_DOOR_LOCK_H

#include <zboss_api.h>

/** @brief The door lock application endpoint structure.
 */
extern zb_af_endpoint_desc_t door_lock_ep;


/** @brief Function for initializing door lock application.
 */
void app_door_lock_init(void);

/** @brief Function for triggering the identify state on the door lock endpoint.
 */
void app_door_lock_start_identifying(zb_bufid_t bufid);

/** @brief Callback function for handling Door Lock ZCL commands.
 *
 * @param bufid  Reference to Zigbee stack buffer used to pass received data.
 */
void app_door_lock_zcl_cb(zb_bufid_t bufid);

/** @brief Toggle the current state of the door lock.
 */
void app_door_lock_toggle(void);

/** @brief Lock the door lock without disabling the smeeze logic. */
void app_door_lock_lock_smeeze(void);

/** @brief Unlock the door lock without disabling the smeeze logic. */
void app_door_lock_unlock_smeeze(void);

#endif /* APP_DOOR_LOCK_H */
