# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(shell_console)

target_sources(app PRIVATE ../src/shell_parse.c)
target_sources(app PRIVATE src/shell_console.c)
target_sources(app PRIVATE src/main.c)

zephyr_include_directories(../src)
