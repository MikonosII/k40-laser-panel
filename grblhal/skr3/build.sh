#!/usr/bin/env bash
# Builds grblHAL for the K40 on a BTT SKR 3 (STM32H723) -> build/firmware.bin
# Needs: git, make, arm-none-eabi-gcc (Debian/Ubuntu: apt install gcc-arm-none-eabi libnewlib-arm-none-eabi)
set -euo pipefail
cd "$(dirname "$0")"
mkdir -p work && cd work
if [ ! -d STM32H7xx ]; then git clone -q https://github.com/dresco/STM32H7xx; fi
git -C STM32H7xx checkout -q bfe5d4f6ec271cc9929336244c3b1c8e55d1d040
git -C STM32H7xx submodule update -q --init --recursive
# K40 changes: laser PWM on the RGB header, lid switch on PWR-DET
git -C STM32H7xx checkout -q -- boards/btt_skr_v3.0_map.h
git -C STM32H7xx apply ../../k40.patch
cd ..
make -j"$(nproc)"
