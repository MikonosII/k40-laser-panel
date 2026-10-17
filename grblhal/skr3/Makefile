# grblHAL for the K40: BTT SKR 3 / SKR 3 EZ with STM32H723, TMC2209 (UART), UART on the TFT header.
# Plain-make build of grblHAL's STM32H7xx driver (upstream uses PlatformIO). Run ./build.sh.

SRC_ROOT ?= work
DRV := $(SRC_ROOT)/STM32H7xx
OUT := build

CC      := arm-none-eabi-gcc
OBJCOPY := arm-none-eabi-objcopy
SIZE    := arm-none-eabi-size

# Upstream env "btt_skr_30_h723_tmc5160_bl128", with TMC2209 and the TFT-header UART instead of USB ...
DEFS := -DSTM32H723xx -DUSE_HAL_DRIVER -DHSE_VALUE=25000000 -DL1_CACHE_ENABLE=1 \
        -DOVERRIDE_MY_MACHINE '-DISR_CODE=__attribute__((section(".itcmram")))' \
        -DBOARD_BTT_SKR_30 -DTRINAMIC_ENABLE=2209 -DSDCARD_ENABLE=1 -DUSB_SERIAL_CDC=0
# ... plus the K40 setup (defaults apply on first boot or after $RST=*)
DEFS += -DK40_PWM_RGB -DK40_FEEDHOLD_PWRDET \
        -DDEFAULT_LASER_MODE=1 \
        -DDEFAULT_SPINDLE_RPM_MAX=1000.0f -DDEFAULT_SPINDLE_RPM_MIN=0.0f \
        -DDEFAULT_SPINDLE_PWM_FREQ=1000 \
        -DDEFAULT_CONTROL_SIGNALS_INVERT_MASK=68 \
        -DDEFAULT_PROBE_SIGNAL_INVERT=0 \
        -DDEFAULT_X_STEPS_PER_MM=157.48f -DDEFAULT_Y_STEPS_PER_MM=157.48f \
        -DDEFAULT_X_MAX_RATE=18000.0f -DDEFAULT_Y_MAX_RATE=18000.0f \
        -DDEFAULT_X_ACCELERATION=2000.0f -DDEFAULT_Y_ACCELERATION=2000.0f \
        -DDEFAULT_X_CURRENT=600.0f -DDEFAULT_Y_CURRENT=600.0f -DDEFAULT_Z_CURRENT=300.0f

LIBDIRS := grbl boards plugins motors trinamic sdcard FATFS/App FATFS/Target
INCS := -I$(DRV) -I$(DRV)/Inc $(addprefix -I$(DRV)/,$(LIBDIRS)) \
        -I$(DRV)/Middlewares/Third_Party/FatFs/src \
        -I$(DRV)/Drivers/STM32H7xx_HAL_Driver/Inc -I$(DRV)/Drivers/CMSIS/Device/ST/STM32H7xx/Include \
        -I$(DRV)/Drivers/CMSIS/Include

CPU    := -mcpu=cortex-m7 -mthumb -mfpu=fpv5-d16 -mfloat-abi=hard
CFLAGS := $(CPU) -Os -g -std=gnu11 -ffunction-sections -fdata-sections -fno-common \
          -Wall -Wno-unused-function -Wno-unused-variable -Wno-unused-but-set-variable \
          -Wno-missing-braces -Wno-switch $(DEFS) $(INCS)
LDFLAGS := $(CPU) -T$(DRV)/STM32H723VGTX_BL_FLASH.ld -Wl,--gc-sections -Wl,-Map=$(OUT)/grblHAL.map \
           --specs=nano.specs --specs=nosys.specs -Wl,-u,_printf_float -Wl,-u,_scanf_float \
           -Wl,--no-warn-rwx-segments

DRV_SRC := $(wildcard $(DRV)/Src/*.c)
LIB_SRC := $(shell find $(addprefix $(DRV)/,$(LIBDIRS)) -name '*.c' 2>/dev/null) \
           $(wildcard $(DRV)/Middlewares/Third_Party/FatFs/src/*.c)
HAL_SRC := $(filter-out %_template.c,$(wildcard $(DRV)/Drivers/STM32H7xx_HAL_Driver/Src/*.c))
ASM_SRC := $(DRV)/Startup723/startup_stm32h723vgtx.s

SRCS := $(DRV_SRC) $(LIB_SRC) $(HAL_SRC)
OBJS := $(patsubst $(SRC_ROOT)/%.c,$(OUT)/%.o,$(SRCS)) $(OUT)/startup.o

all: $(OUT)/firmware.bin

$(OUT)/%.o: $(SRC_ROOT)/%.c
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) -c $< -o $@

$(OUT)/startup.o: $(ASM_SRC)
	@mkdir -p $(dir $@)
	$(CC) $(CPU) -c $< -o $@

$(OUT)/grblHAL.elf: $(OBJS)
	@$(CC) $(LDFLAGS) $^ -lm -o $@
	$(SIZE) $@

# The BTT SD-card bootloader (128 KB) flashes firmware.bin at 0x08020000 (set by the linker script)
$(OUT)/firmware.bin: $(OUT)/grblHAL.elf
	$(OBJCOPY) -O binary $< $@
	@ls -l $@

clean:
	rm -rf $(OUT)

.PHONY: all clean
