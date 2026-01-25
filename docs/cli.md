# Command Line Interface (CLI)

## Overview
The soRTOS CLI provides a lightweight, interactive shell for the system. It allows developers to interact with the running kernel and applications via a serial terminal (UART).

It runs as a dedicated task (`cli_task_entry`) that processes input, handles line editing, and executes registered commands.

## Features

*   **Dynamic Command Registration:** Commands can be registered and unregistered at runtime in a thread-safe manner.
*   **Line Editing:** Supports VT100/ANSI escape sequences for a better user experience:
    *   **Left/Right Arrows:** Move cursor for insertion/editing.
    *   **Backspace:** Delete characters.
*   **Thread-Safe Output:** `cli_printf` allows multiple tasks to print to the console without output corruption.
*   **Asynchronous I/O:** Uses the kernel's **Queue** mechanism for non-blocking transmission and reception (when configured).

## Architecture

### 1. The CLI Task
The CLI operates as a standalone task. Its main loop:
1.  **Waits for Input:** Blocks on the RX Queue (or polls via callback) until a character arrives.
2.  **Parses Input:**
    *   Standard characters are added to the line buffer.
    *   Escape sequences (e.g., `\033[D`) are decoded for cursor movement.
    *   Backspace/Delete modifies the buffer and updates the display.
3.  **Executes Command:** When `ENTER` is pressed:
    *   The line is tokenized (split by spaces).
    *   The command list is searched for a matching name.
    *   The handler function is executed.

### 2. Thread Safety
*   **Command List:** Protected by a **spinlock**. This ensures that tasks can safely register or unregister commands even while the CLI task is processing input.
*   **Output:** `cli_printf` formats the string into a local stack buffer and then pushes the entire buffer to the TX Queue in a single atomic operation (using `queue_push_arr`). This prevents messages from different tasks being interleaved character-by-character.

### 3. Input/Output Abstraction
The CLI can operate in two modes:
*   **Queue Mode (Recommended):** Uses `queue_t` for RX and TX. This allows the CLI to block efficiently when idle and allows ISRs (like UART) to push data without complex logic.
*   **Callback Mode:** Uses direct function pointers (`getc`, `puts`) for simple polling implementations or debugging.

## API Reference

| Function | Description |
| :--- | :--- |
| `cli_init` | Initializes the CLI context and registers the built-in `help` command. |
| `cli_register_command` | Adds a new command to the list. |
| `cli_unregister_command` | Removes a command from the list. |
| `cli_set_rx_queue` | Assigns the input queue (from UART ISR). |
| `cli_set_tx_queue` | Assigns the output queue (to UART Driver). |
| `cli_printf` | Thread-safe, formatted print function (supports `%d`, `%x`, `%s`, `%p`, etc.). |
| `cli_task_entry` | The main loop function (pass this to `task_create`). |

## Configuration

The CLI is configured via `project_config.h`:

| Macro | Description | Default |
| :--- | :--- | :--- |
| `CLI_MAX_LINE_LEN` | Maximum characters in a command line. | 128 |
| `CLI_MAX_ARGS` | Maximum number of arguments (tokens). | 16 |
| `CLI_MAX_CMDS` | Maximum number of registered commands. | 32 |

## Supported Format Specifiers
`cli_printf` supports a subset of standard printf specifiers:
*   `%d`: Signed Integer
*   `%u`: Unsigned Integer
*   `%x`: Hexadecimal (lowercase)
*   `%p`: Pointer address
*   `%s`: String
*   `%c`: Character
*   Padding: `%5d`, `%05d`, `%-10s` supported.