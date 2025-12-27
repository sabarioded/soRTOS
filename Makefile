# --- Project Configuration ---
TARGET      = stm32_cli_os
LDSCRIPT    = config/stm32l476rg_flash.ld

# --- STM32 Toolchain (Cross-Compiler) ---
CC          = arm-none-eabi-gcc
CFLAGS      = -mcpu=cortex-m4 -mthumb -std=gnu11 -g -O0 -Wall -Wextra -ffreestanding \
	          -Iinclude -Icore -Idrivers -Iapp -Iconfig
LDFLAGS     = -nostdlib -T $(LDSCRIPT) -Wl,-Map=$(TARGET).map -Wl,--gc-sections

# --- Native Toolchain (For TDD on your PC) ---
NATIVE_CC     = gcc
NATIVE_CFLAGS = -std=gnu11 -g -Wall -Iinclude -Icore -Idrivers -Iapp -Iconfig -Iexternal/unity/src -DUNIT_TESTING
UNITY_SRC     = external/unity/src/unity.c
TEST_SRCS     = tests/test_allocator.c core/allocator.c $(UNITY_SRC)
TEST_BIN      = test_runner

# --- STM32 Source Files ---
C_SRCS = \
	app/main.c \
	app/app_commands.c \
	core/utils.c \
	core/scheduler.c \
	core/system_clock.c \
	core/cli.c \
	core/allocator.c \
	core/stm32_alloc.c \
	drivers/led.c \
	drivers/button.c \
	drivers/uart.c \
	drivers/systick.c \
	startup/stm32l476_startup.c

ASM_SRCS = \
	core/context_switch.S

OBJS = $(C_SRCS:.c=.o) $(ASM_SRCS:.S=.o)

# --- Targets ---

.PHONY: all clean load test

# Build for STM32
all: $(TARGET).elf

$(TARGET).elf: $(OBJS) $(LDSCRIPT)
	$(CC) $(CFLAGS) $(OBJS) $(LDFLAGS) -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.S
	$(CC) $(CFLAGS) -c $< -o $@

# Build and Run Tests on Host PC
test:
	@echo "--- RUNNING UNIT TESTS (NATIVE) ---"
	$(NATIVE_CC) $(NATIVE_CFLAGS) $(TEST_SRCS) -o $(TEST_BIN)
	./$(TEST_BIN)
	@rm -f $(TEST_BIN)

# Clean build files
clean:
	rm -f $(OBJS) $(TARGET).elf $(TARGET).map $(TEST_BIN)

# Load to STM32 Hardware
load: $(TARGET).elf
	openocd -f board/st_nucleo_l4.cfg -c "program $(TARGET).elf verify reset exit"
