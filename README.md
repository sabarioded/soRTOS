# stm32_cli_os

A small, approachable STM32L4 bare-metal project that started as a learning project and is intended to become a tiny, usable RTOS playground.

What you get here (today)
- A compact scheduler and task system (`core/scheduler.c`) with task creation, deletion, blocking, and simple garbage collection.
- Assembly context switch using PendSV (`core/context_switch.asm`). SysTick is configured one level higher than PendSV so SysTick completes its tick handling before a context switch is serviced.
- Utilities and helpers (`core/utils.c`), system clock management (`core/system_clock.c`) and SysTick driver (`drivers/systick.c`).
- Basic drivers for UART (buffered RX), LED and button in `drivers/`.
- A small RTOS demo in `app/main.c` that initializes the scheduler and SysTick, creates example tasks (blink and button logger), and starts a UART-based CLI task (`cli_task_entry`). The CLI is implemented in `core/cli.c` and offers a `help` command plus application commands (`app/app_commands.c`) such as `tasks`, `heap`, and `heaptest` (dynamic heap mode).

Quick start
1. Install the ARM toolchain (`arm-none-eabi-gcc`) and `openocd`.
2. Build the current demo:

```bash
make         # builds the default polling demo
make load    # flash the demo using OpenOCD
./scripts/monitor.sh <tty> 115200   # open serial console
```

Using the built-in CLI
- Connect a serial console (e.g., `./scripts/monitor.sh /dev/tty.usbserial-XXXX 115200`).
- When the target boots you'll see the `OS> ` prompt from the CLI.
- Type `help` to list available commands (e.g., `tasks`, `heap`, `heaptest` when using dynamic stacks), and `tasks` to inspect running tasks.

The CLI uses a non-blocking `getc`/`puts` abstraction (`cli_init` takes wrappers defined in `app/main.c`), a `cli_printf` formatter, and a simple `cli_register_command` API for application commands.

Short-term roadmap (what I recommend we do next)
- Add a `make rtos` target and a dedicated RTOS demo entry so it’s easy to build and flash the RTOS demo.
- Make CLI test helpers (`cli_execute_line`, `cli_vprintf`) so the CLI can be exercised from unit tests and host-side runners.
- Harden CLI registration (reject duplicate command names) and add small unit tests for the tokenizer and `cli_printf`.
- Add host unit tests for non-ASM code and a GitHub Actions pipeline for builds and tests.
- Make priority left-alignment configurable from `config/project_config.h` (derive the byte values from `NVIC_PRIO_BITS`) so the code works on other Cortex-M targets.

Limitations (important)
- The project is evolving: a small RTOS demo and a UART CLI are now present, but more testing and CI are needed.
- No CI / automated tests yet; parts of the code are still untested on host.

Configuration notes
- `config/project_config.h` contains priority macros (`SYSTICK_PRIORITY`, `PENDSV_PRIORITY`) which are currently set to 14 and 15 respectively (left-aligned to 0xE0 and 0xF0 in `core/context_switch.asm`). Adjust `NVIC_PRIO_BITS`/priority macros if targeting a different Cortex-M implementation.

If you want, I can implement one of the short-term roadmap items for you (I can add `app/rtos_main.c` + Makefile changes, or a minimal CLI). Tell me which and I’ll open a PR with code, tests, and README notes.