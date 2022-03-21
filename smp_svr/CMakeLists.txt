cmake_minimum_required(VERSION 3.20.0)
# Top-level CMakeLists.txt for the skeleton application.
#
# Copyright (c) 2017 Open Source Foundries Limited
#
# SPDX-License-Identifier: Apache-2.0
#
# This provides a basic application structure suitable for communication using
# mcumgr.  It can be used as a starting point for new applications.

# Standard Zephyr application boilerplate.
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(smp_svr)

include(${ZEPHYR_BASE}/cmake/cfb.cmake NO_POLICY_SCOPE)

target_sources(app PRIVATE src/main.c)
target_sources_ifdef(CONFIG_MCUMGR_SMP_BT app PRIVATE src/bluetooth.c)
target_sources_ifdef(CONFIG_MCUMGR_SMP_UDP app PRIVATE src/udp.c)
target_sources(app PRIVATE src/uievq.c)
target_sources(app PRIVATE src/buttons.c)

set(gen_dir ${ZEPHYR_BINARY_DIR}/include/generated/)
generate_cfb_font_for_target(app fonts/magic5.ttf ${gen_dir}/cfb_font_magic5.h 8 8 -s 8 --first 32 --last 126)
