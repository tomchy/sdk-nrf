#
# Copyright (c) 2024 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

zephyr_interface_library_named(zcbor_noncanonical_decode_interface)
target_include_directories(zcbor_noncanonical_decode_interface INTERFACE include)

zephyr_library()
zephyr_library_sources(src/zcbor_noncanonical_decode.c)
zephyr_library_link_libraries(zcbor_noncanonical_decode_interface)
