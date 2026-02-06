# soRTOS Overview

## Summary

soRTOS is a lightweight, preemptive RTOS written from scratch in C with a strong emphasis on separation of concerns:

* Application layer (`app/`)
* Platform-agnostic kernel (`kernel/`)
* Platform-agnostic drivers (`drivers/`)
* Platform HALs and board setup (`platform/`)
* Architecture-specific low-level code (`arch/`)

The project is designed to run both on embedded hardware (STM32L476RG) and as a native host simulation for development and unit testing.

---

## Architecture

```mermaid
graph TD
    subgraph App["Application Layer"]
        AppMain["app/src/main.c"]
        AppAPI["app/interface"]
        AppTasks["App Tasks"]
    end

    subgraph Kernel["Kernel Layer"]
        Scheduler["Scheduler"]
        Allocator["Allocator (TLSF)"]
        IPC["IPC (Mutex/Sem/Queue/EG)"]
        Services["Timers / Logger / CLI"]
    end

    subgraph Drivers["Driver Layer (Platform-Agnostic)"]
        DIf["drivers/interface"]
        DCore["drivers/src"]
    end

    subgraph HAL["HAL / Platform Drivers"]
        HalIf["platform/*/drivers/*_hal.h"]
    end

    subgraph Platform["Platform + Arch"]
        Startup["Startup / Linker"]
        ArchOps["arch/*"]
        Clock["platform/*/system_clock"]
    end

    subgraph HW["Hardware"]
        Periph["MCU Peripherals"]
    end

    AppMain --> Kernel
    AppTasks --> Kernel
    Kernel --> Drivers
    Drivers --> HAL
    HAL --> Periph
    Kernel --> ArchOps
    Startup --> ArchOps
```

---

## Build and Test

Native build and run:

```bash
make
./build/native/soRTOS.elf
```

Unit tests (native):

```bash
make test
```

STM32 build:

```bash
gmake PLATFORM=stm32l476rg
```

---

## Design Notes

* Kernel APIs are platform-agnostic; only HALs are platform-specific.
* Drivers expose clean interfaces in `drivers/interface` and delegate hardware access to HALs.
* The app layer mirrors this structure with `app/interface` and `app/src`.
