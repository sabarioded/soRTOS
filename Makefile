# --- Project Configuration ---
# Default platform (can be overridden: make PLATFORM=native)
PLATFORM    ?= stm32l476rg
TARGET      = soRTOS
BUILD_DIR   = build/$(PLATFORM)

# --- Directories ---
APP_DIR     = app
KERNEL_DIR  = kernel
DRIVERS_DIR = drivers
PLATFORM_DIR= platform
ARCH_DIR    = arch
CONFIG_DIR  = config

# --- Common Sources ---
C_SRCS += \
	$(APP_DIR)/main.c \
	$(APP_DIR)/app_commands.c \
	$(KERNEL_DIR)/src/utils.c \
	$(KERNEL_DIR)/src/scheduler.c \
	$(KERNEL_DIR)/src/cli.c \
	$(KERNEL_DIR)/src/allocator.c \
	$(KERNEL_DIR)/src/system_clock.c

# --- Common Includes ---
INCLUDES += \
	-I$(APP_DIR) \
	-I$(KERNEL_DIR)/include \
	-I$(DRIVERS_DIR)/interface \
	-I$(PLATFORM_DIR) \
	-I$(CONFIG_DIR)

# --- Platform Specific Configuration ---

ifeq ($(PLATFORM), stm32l476rg)
	# Toolchain
	CC = arm-none-eabi-gcc
	
	# Architecture Flags
	MCU_FLAGS = -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard
	
	# Platform Sources
	C_SRCS += \
		$(PLATFORM_DIR)/stm32l476rg/platform.c \
		$(PLATFORM_DIR)/stm32l476rg/memory_map.c \
		$(DRIVERS_DIR)/stm32/uart.c \
		$(DRIVERS_DIR)/stm32/gpio.c \
		$(DRIVERS_DIR)/stm32/led.c \
		$(DRIVERS_DIR)/stm32/button.c \
		$(ARCH_DIR)/arm/cortex_m4/systick.c \
		$(ARCH_DIR)/arm/cortex_m4/arch_ops.c \
		startup/stm32l476_startup.c

	ASM_SRCS += \
		$(ARCH_DIR)/arm/cortex_m4/context_switch.S

	# Platform Includes
	INCLUDES += \
		-I$(PLATFORM_DIR)/stm32l476rg \
		-I$(ARCH_DIR)/arm/cortex_m4

	# Linker
	LDSCRIPT = $(CONFIG_DIR)/stm32l476rg_flash.ld
	LDFLAGS  = -nostdlib -T $(LDSCRIPT) -Wl,-Map=$(BUILD_DIR)/$(TARGET).map -Wl,--gc-sections
	
	# Compiler Flags
	CFLAGS = $(MCU_FLAGS) -std=gnu11 -g -O0 -Wall -Wextra -ffreestanding \
	         $(INCLUDES) -DSTM32L476xx

	# Flash Tool Configuration
	OPENOCD_BOARD = board/st_nucleo_l4.cfg
endif

# --- Native (Host) Platform Configuration ---
ifeq ($(PLATFORM), native)
	CC = gcc
	CFLAGS = -std=gnu11 -g -O0 -Wall -Wextra $(INCLUDES) -DHOST_PLATFORM
	
	# Note: You would need to implement platform/native/platform.c for this to link
	# C_SRCS += $(PLATFORM_DIR)/native/platform.c
	
	LDFLAGS = 
endif

# --- Build Rules ---

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

# --- Unit Tests (Native) ---
NATIVE_CC     = gcc
NATIVE_CFLAGS = -std=gnu11 -g -Wall $(INCLUDES) -Iexternal/unity/src -DUNIT_TESTING
UNITY_SRC     = external/unity/src/unity.c
TEST_SRCS     = tests/test_allocator.c $(KERNEL_DIR)/src/allocator.c $(UNITY_SRC)
TEST_BIN      = $(BUILD_DIR)/test_runner

test:
	@mkdir -p $(dir $(TEST_BIN))
	@echo "--- RUNNING UNIT TESTS (NATIVE) ---"
	$(NATIVE_CC) $(NATIVE_CFLAGS) $(TEST_SRCS) -o $(TEST_BIN)
	./$(TEST_BIN)

-include $(DEPS)
