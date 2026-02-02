# Project Configuration
# Default platform (can be overridden: make PLATFORM=stm32l476rg)
PLATFORM    ?= native
TARGET      = soRTOS
BUILD_DIR   = build/$(PLATFORM)

# Directories
APP_DIR     = app
KERNEL_DIR  = kernel
DRIVERS_DIR = drivers
PLATFORM_DIR= platform
ARCH_DIR    = arch
CONFIG_DIR  = config

# Common Sources
C_SRCS += \
	$(APP_DIR)/main.c \
	$(APP_DIR)/app_commands.c \
	$(KERNEL_DIR)/src/utils.c \
	$(KERNEL_DIR)/src/scheduler.c \
	$(KERNEL_DIR)/src/cli.c \
	$(KERNEL_DIR)/src/allocator.c \
	$(KERNEL_DIR)/src/mutex.c \
	$(KERNEL_DIR)/src/semaphore.c \
	$(KERNEL_DIR)/src/queue.c \
	$(KERNEL_DIR)/src/timer.c \
	$(KERNEL_DIR)/src/event_group.c \
	$(KERNEL_DIR)/src/mempool.c \
	$(KERNEL_DIR)/src/logger.c \


# Common Includes
INCLUDES += \
	-I$(APP_DIR) \
	-I$(KERNEL_DIR)/include \
	-I$(DRIVERS_DIR)/interface \
	-I$(PLATFORM_DIR) \
	-I$(CONFIG_DIR)

# Platform Specific Configuration
ifeq ($(PLATFORM), stm32l476rg)
	# Toolchain
	CC = arm-none-eabi-gcc
	
	# Architecture Flags
	FPU ?= 1
	MCU_FLAGS = -mcpu=cortex-m4 -mthumb
ifeq ($(FPU), 1)
	MCU_FLAGS += -mfpu=fpv4-sp-d16 -mfloat-abi=hard
else
	MCU_FLAGS += -mfloat-abi=soft
endif
	
	# Platform Sources
	C_SRCS += \
		$(PLATFORM_DIR)/stm32l476rg/platform.c \
		$(PLATFORM_DIR)/stm32l476rg/memory_map.c \
		$(PLATFORM_DIR)/stm32l476rg/system_clock.c \
		$(PLATFORM_DIR)/stm32l476rg/stm32l476_startup.c \
		$(ARCH_DIR)/arm/cortex_m4/arch_ops.c \
		$(DRIVERS_DIR)/src/gpio.c \
		$(DRIVERS_DIR)/src/uart.c \
		$(DRIVERS_DIR)/src/led.c \
		$(DRIVERS_DIR)/src/button.c \
		$(DRIVERS_DIR)/src/systick.c \
		$(DRIVERS_DIR)/src/i2c.c \
		$(DRIVERS_DIR)/src/spi.c \
		$(DRIVERS_DIR)/src/dma.c \
		$(DRIVERS_DIR)/src/adc.c \
		$(DRIVERS_DIR)/src/exti.c \
		$(DRIVERS_DIR)/src/watchdog.c \
		$(DRIVERS_DIR)/src/flash.c \
		$(DRIVERS_DIR)/src/dac.c \
		$(DRIVERS_DIR)/src/pwm.c \
		$(DRIVERS_DIR)/src/rtc.c

	ASM_SRCS += \
		$(ARCH_DIR)/arm/cortex_m4/context_switch.S

	# Platform Includes
	INCLUDES += \
		-I$(PLATFORM_DIR)/stm32l476rg \
		-I$(PLATFORM_DIR)/stm32l476rg/drivers \
		-I$(ARCH_DIR)/arm/cortex_m4 \
		-I$(ARCH_DIR)/arm/cortex_m4/drivers

	# Linker
	LDSCRIPT = $(PLATFORM_DIR)/stm32l476rg/stm32l476rg_flash.ld
	LDFLAGS  = -nostdlib -T $(LDSCRIPT) -Wl,-Map=$(BUILD_DIR)/$(TARGET).map -Wl,--gc-sections -lgcc
	
	# Compiler Flags
	CFLAGS = $(MCU_FLAGS) -std=gnu11 -g -O0 -Wall -Wextra -ffreestanding \
	         $(INCLUDES) -DSTM32L476xx

	# Flash Tool Configuration
	OPENOCD_BOARD = board/st_nucleo_l4.cfg
endif

# Native (Host) Platform Configuration
ifeq ($(PLATFORM), native)
	CC = gcc
	CFLAGS = -std=gnu11 -g -O0 -Wall -Wextra -I$(ARCH_DIR)/native -I$(PLATFORM_DIR)/native -I$(PLATFORM_DIR)/native/drivers $(INCLUDES) -DHOST_PLATFORM
	
	# Native platform implementation
	C_SRCS += $(PLATFORM_DIR)/native/platform.c \
	          $(PLATFORM_DIR)/native/memory_map.c \
	          $(PLATFORM_DIR)/native/drivers/native_hal.c \
	          $(ARCH_DIR)/native/arch_ops.c \
	          $(DRIVERS_DIR)/src/systick.c \
	          $(DRIVERS_DIR)/src/button.c \
	          $(DRIVERS_DIR)/src/led.c \
	          $(DRIVERS_DIR)/src/uart.c \
	          $(DRIVERS_DIR)/src/i2c.c \
	          $(DRIVERS_DIR)/src/spi.c \
	          $(DRIVERS_DIR)/src/dma.c \
	          $(DRIVERS_DIR)/src/adc.c \
	          $(DRIVERS_DIR)/src/exti.c \
	          $(DRIVERS_DIR)/src/watchdog.c \
	          $(DRIVERS_DIR)/src/flash.c \
	          $(DRIVERS_DIR)/src/dac.c \
	          $(DRIVERS_DIR)/src/pwm.c \
	          $(DRIVERS_DIR)/src/rtc.c
	
	LDFLAGS = 
endif

# Build Rules
OBJS = $(addprefix $(BUILD_DIR)/, $(C_SRCS:.c=.o) $(ASM_SRCS:.S=.o))
DEPS = $(OBJS:.o=.d)

.PHONY: all clean load test

all: $(BUILD_DIR)/$(TARGET).elf

$(BUILD_DIR)/$(TARGET).elf: $(OBJS) $(LDSCRIPT)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(OBJS) $(LDFLAGS) -o $@
	@echo "Build complete: $@"

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/%.o: %.S
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf build

load: $(BUILD_DIR)/$(TARGET).elf
ifdef OPENOCD_BOARD
	openocd -f $(OPENOCD_BOARD) -c "program $< verify reset exit"
else
	@echo "Load command not supported/defined for platform: $(PLATFORM)"
endif

# Unit Tests (Native)
NATIVE_CC     = gcc
NATIVE_CFLAGS = -std=gnu11 -g -Wall -Itests -I$(ARCH_DIR)/native -I$(PLATFORM_DIR)/native -I$(PLATFORM_DIR)/native/drivers $(INCLUDES) -Iexternal/unity/src -DUNIT_TESTING -DHOST_PLATFORM
UNITY_SRC     = external/unity/src/unity.c

TEST_SRCS     = tests/test_common.c \
                tests/test_main.c \
                tests/test_queue.c \
                tests/test_scheduler.c \
                tests/test_mutex.c \
                tests/test_semaphore.c \
                tests/test_allocator.c \
                tests/test_timer.c \
				tests/test_logger.c \
				tests/test_utils.c \
				tests/test_cli.c \
				tests/test_event_group.c \
				tests/test_mempool.c \
				tests/test_drivers.c \
				tests/test_watchdog.c \
				tests/test_systick.c \
				tests/test_button.c \
				tests/test_led.c \
				tests/test_uart.c \
				tests/test_i2c.c \
				tests/test_spi.c \
				tests/test_dma.c \
				tests/test_adc.c \
				tests/test_exti.c \
				tests/test_dac.c \
				tests/test_pwm.c \
				tests/test_rtc.c \
				tests/test_flash.c \
                $(ARCH_DIR)/native/arch_ops.c \
                $(KERNEL_DIR)/src/queue.c \
                $(KERNEL_DIR)/src/scheduler.c \
                $(KERNEL_DIR)/src/allocator.c \
                $(KERNEL_DIR)/src/utils.c \
                $(KERNEL_DIR)/src/logger.c \
                $(KERNEL_DIR)/src/cli.c \
                $(KERNEL_DIR)/src/mutex.c \
                $(KERNEL_DIR)/src/semaphore.c \
                $(KERNEL_DIR)/src/timer.c \
                $(KERNEL_DIR)/src/event_group.c \
				$(KERNEL_DIR)/src/mempool.c \
				$(DRIVERS_DIR)/src/systick.c \
				$(DRIVERS_DIR)/src/button.c \
				$(DRIVERS_DIR)/src/led.c \
				$(DRIVERS_DIR)/src/uart.c \
				$(DRIVERS_DIR)/src/i2c.c \
				$(DRIVERS_DIR)/src/spi.c \
				$(DRIVERS_DIR)/src/dma.c \
				$(DRIVERS_DIR)/src/adc.c \
				$(DRIVERS_DIR)/src/exti.c \
				$(DRIVERS_DIR)/src/watchdog.c \
				$(DRIVERS_DIR)/src/dac.c \
				$(DRIVERS_DIR)/src/pwm.c \
				$(DRIVERS_DIR)/src/rtc.c \
                $(UNITY_SRC)
TEST_BIN      = $(BUILD_DIR)/test_runner

test:
	@mkdir -p $(dir $(TEST_BIN))
	@echo "--- RUNNING UNIT TESTS (NATIVE) ---"
	$(NATIVE_CC) $(NATIVE_CFLAGS) $(TEST_SRCS) -o $(TEST_BIN)
	./$(TEST_BIN)

-include $(DEPS)
