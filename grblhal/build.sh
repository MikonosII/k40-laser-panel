#!/usr/bin/env bash
# Builds grblHAL for the K40's SKR Mini E3 V3 -> build/firmware.bin
# Needs: git, make, python3, arm-none-eabi-gcc (Debian/Ubuntu: apt install gcc-arm-none-eabi libnewlib-arm-none-eabi)
set -euo pipefail
cd "$(dirname "$0")"
mkdir -p work && cd work

fetch() {  # repo dir commit [--recursive]
  if [ ! -d "$2" ]; then git clone -q ${4:-} "https://github.com/$1" "$2"; fi
  git -C "$2" fetch -q --depth 1 origin "$3" 2>/dev/null || true
  git -C "$2" checkout -q "$3"
  if [ "${4:-}" = "--recursive" ]; then git -C "$2" submodule update -q --init --recursive; fi
}
fetch mtaylor819/STM32G0xx STM32G0xx 804c4f4d8b83fe5276bb55c7bad7870aa0f83399 --recursive
fetch STMicroelectronics/stm32g0xx_hal_driver stm32g0xx_hal_driver 289ef6c8ff0922878e99d253f29be76851f8f28e
fetch STMicroelectronics/cmsis_device_g0 cmsis_device_g0 7c77b7cbcee497dfbb5cc08f9562a80515e78013
fetch STMicroelectronics/cmsis_core cmsis_core 01d18147527e30ed69ace1cb03b818de78bc7062

# K40 change: laser PWM on the NEOPIXEL (RGB) connector instead of SERVOS
git -C STM32G0xx checkout -q -- boards/btt_skr_mini_e3_3.0_map.h
git -C STM32G0xx apply ../../k40.patch

cd ..
make -j"$(nproc)"
