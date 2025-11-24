#
# Copyright (c) 2019 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#
cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(peripheral_hids_mouse_Toast)

# Add all source files
target_sources(app PRIVATE
    src/main.c
    src/hid.c
    src/bluetooth.c
    src/encoder.c

)

target_include_directories(app PRIVATE src)
