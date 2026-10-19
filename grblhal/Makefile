# grblHAL for the K40: BTT SKR Mini E3 V3.0 (STM32G0B1), UART on the TFT header.
# Plain-make build of the STM32G0xx driver (the upstream uses PlatformIO; this needs only
# arm-none-eabi-gcc). Run ./build.sh, which fetches the sources and calls this.

SRC_ROOT ?= work
DRV  := $(SRC_ROOT)/STM32G0xx
HAL  := $(SRC_ROOT)/stm32g0xx_hal_driver
DEV  := $(SRC_ROOT)/cmsis_device_g0
CORE := $(SRC_ROOT)/cmsis_core
OUT  := build

CC      := arm-none-eabi-gcc
OBJCOPY := arm-none-eabi-objcopy
SIZE    := arm-none-eabi-size

# Same options as the upstream PlatformIO env "BTT_SKR_MINI_E3_V30" ...
DEFS := -DSTM32G0B1xx -DUSE_HAL_DRIVER \
        -DBOARD_BTT_SKR_MINI_E3_V30= -DUSB_SERIAL_CDC=0 -DHAS_BOOTLOADER= \
        -DRX_BUFFER_SIZE=512 -DNVS_SIZE=1536 -DOVERRIDE_MY_MACHINE
# ... plus the K40 setup (defaults apply on first boot or after $RST=*)
DEFS += -DK40_PWM_NEOPIXEL -DK40_ENABLE_ON_HB \
        -DDEFAULT_LASER_MODE=1 \
        -DDEFAULT_SPINDLE_RPM_MAX=1000.0f -DDEFAULT_SPINDLE_RPM_MIN=0.0f \
        -DDEFAULT_SPINDLE_PWM_FREQ=1000 \
        -DDEFAULT_CONTROL_SIGNALS_INVERT_MASK=70 \
        -DDEFAULT_PROBE_SIGNAL_INVERT=1 \
        -DDEFAULT_HOMING_ENABLE=1 -DDEFAULT_HOMING_DIR_MASK=1 \
        -DDEFAULT_HOMING_CYCLE_0=3 -DDEFAULT_HOMING_CYCLE_1=0 -DDEFAULT_HOMING_CYCLE_2=0 \
        -DDEFAULT_LIMIT_SIGNALS_INVERT_MASK=4 \
        -DDEFAULT_X_MAX_TRAVEL=316.0f -DDEFAULT_Y_MAX_TRAVEL=221.0f \
        -DDEFAULT_STEPPER_IDLE_LOCK_TIME=255 -DDEFAULT_REPORT_MACHINE_POSITION=0 \
        -DDEFAULT_SOFT_LIMIT_ENABLE=1 -DDEFAULT_JOG_LIMIT_ENABLE=1 \
        -DDEFAULT_HOMING_SEEK_RATE=1000.0f -DDEFAULT_HOMING_PULLOFF=3.0f \
        -DDEFAULT_X_STEPS_PER_MM=157.48f -DDEFAULT_Y_STEPS_PER_MM=157.48f \
        -DDEFAULT_X_MAX_RATE=6000.0f -DDEFAULT_Y_MAX_RATE=6000.0f \
        -DDEFAULT_X_ACCELERATION=1000.0f -DDEFAULT_Y_ACCELERATION=1000.0f \
        -DDEFAULT_X_CURRENT=800.0f -DDEFAULT_Y_CURRENT=600.0f -DDEFAULT_Z_CURRENT=300.0f

INCS := -I$(DRV) -I$(DRV)/Inc -I$(DRV)/boards -I$(DRV)/grbl -I$(DRV)/keypad -I$(DRV)/motors \
        -I$(DRV)/odometer -I$(DRV)/plugins -I$(DRV)/trinamic \
        -I$(HAL)/Inc -I$(DEV)/Include -I$(CORE)/CMSIS/Core/Include

CPU    := -mcpu=cortex-m0plus -mthumb
CFLAGS := $(CPU) -MMD -MP -Os -g -std=gnu11 -ffunction-sections -fdata-sections -fno-common \
          -Wall -Wno-unused-function -Wno-unused-variable -Wno-unused-but-set-variable \
          -Wno-missing-braces -Wno-switch $(DEFS) $(INCS)
LDFLAGS := $(CPU) -T$(DRV)/STM32G0B1RETX_FLASH.ld -Wl,--gc-sections -Wl,-Map=$(OUT)/grblHAL.map \
           --specs=nano.specs --specs=nosys.specs -Wl,--no-warn-rwx-segments

# Sources (same set PlatformIO compiles: driver minus USB/I2C/disk, core + plugins, HAL, startup)
DRV_SRC := $(filter-out %/usb_serial.c %/i2c.c %/diskio.c,$(wildcard $(DRV)/Src/*.c))
LIB_SRC := $(shell find $(DRV)/grbl $(DRV)/boards $(DRV)/keypad $(DRV)/motors $(DRV)/odometer \
                        $(DRV)/plugins $(DRV)/trinamic -name '*.c' 2>/dev/null)
HAL_SRC := $(filter-out %_template.c,$(wildcard $(HAL)/Src/*.c))
SYS_SRC := $(DEV)/Source/Templates/system_stm32g0xx.c
ASM_SRC := $(DEV)/Source/Templates/gcc/startup_stm32g0b1xx.s

SRCS := $(DRV_SRC) $(LIB_SRC) $(HAL_SRC) $(SYS_SRC)
OBJS := $(patsubst $(SRC_ROOT)/%.c,$(OUT)/%.o,$(SRCS)) $(OUT)/startup.o

all: $(OUT)/firmware.bin

$(OUT)/%.o: $(SRC_ROOT)/%.c
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) -c $< -o $@

$(OUT)/startup.o: $(ASM_SRC)
	@mkdir -p $(dir $@)
	$(CC) $(CPU) -c $< -o $@

$(OUT)/grblHAL.elf: $(OBJS)
	$(CC) $(LDFLAGS) $^ -lm -o $@
	$(SIZE) $@

# The BTT SD-card bootloader flashes firmware.bin at 0x08002000 (kept by the linker script).
# STM32G0 flash is written 8 bytes at a time, so pad the image to a multiple of 8.
$(OUT)/firmware.bin: $(OUT)/grblHAL.elf
	$(OBJCOPY) -O binary $< $@
	@python3 -c "import os,sys;p=sys.argv[1];n=os.path.getsize(p);open(p,'ab').write(b'\xff'*((8-n%8)%8))" $@
	@ls -l $@

clean:
	rm -rf $(OUT)

.PHONY: all clean

-include $(OBJS:.o=.d)
