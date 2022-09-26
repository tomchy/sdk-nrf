#
# Copyright (c) 2022 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

cmake_minimum_required(VERSION 3.20.0)


if (NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/configuration/${BOARD}")
  message(FATAL_ERROR
          "Board ${BOARD} is not supported.\n"
          "Please make sure board specific configuration files are added to "
          "${CMAKE_CURRENT_SOURCE_DIR}/configuration/${BOARD}")
endif()

################################################################################

# The application uses the configuration/<board> scheme for configuration files.
set(APPLICATION_CONFIG_DIR "configuration/\${BOARD}")

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(Smeeze)

################################################################################

# NORDIC SDK APP START
target_sources(app PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src/main.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/status_led.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/app_wstation.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/sensor.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/battery.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/weather_station.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/app_door_lock.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/door_lock.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/app_sfinder.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/sfinder.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/app_smeeze.c
)

target_include_directories(app PRIVATE include)
# NORDIC SDK APP END
