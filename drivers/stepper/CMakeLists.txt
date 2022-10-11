#
# Copyright (c) 2022 Tomasz Chyrowicz
#
# SPDX-License-Identifier: Apache-2.0

zephyr_library()

zephyr_library_sources_ifdef(CONFIG_STEPPER stepper.c)
