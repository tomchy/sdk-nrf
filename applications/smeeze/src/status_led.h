/*
 * Copyright (c) 2022 Tomasz Chyrowicz
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef STATUS_LED_H_
#define STATUS_LED_H_

#include <stdint.h>
#include <zboss_api.h>

/** @brief Possible colors of the RGB LED. */
enum color {
    BLACK = (1 << 0),
    RED = (1 << 1),
    GREEN = (1 << 2),
    YELLOW = (1 << 3),
    BLUE = (1 << 4),
    MAGENTA = (1 << 5),
    CYAN = (1 << 6),
    WHITE = (1 << 7),
};

/** @brief Add the specified color to the current color mix. */
void status_led_color_add(uint8_t color);

/** @brief Remove the specified color from the current color mix. */
void status_led_color_remove(uint8_t color);

/** @brief Add the specified color to the current color mix for 200ms (blink) . */
void status_led_color_blink(uint8_t color);

/** @brief Callback function for handling ZBOSS signals.
 *
 * @note This callback does not consume ZBOSS signals, but uses them to
 *       show the correct indications on the LED.
 */
void status_led_update(zb_bufid_t bufid);

#endif /* STATUS_LED_H_ */
