#
# Copyright (c) 2024 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

zephyr_interface_library_named(suit_gpio_debug)
target_include_directories(suit_gpio_debug INTERFACE include)

zephyr_library()
zephyr_library_sources_ifdef(CONFIG_SUIT_GPIO_DEBUG src/suit_gpio_debug.c)
zephyr_library_link_libraries(suit_gpio_debug)
