#include "cli.h"
#include "scheduler.h" /* For task_sleep_ticks */
#include "utils.h"     /* For strcmp, memset */
#include <stdarg.h>    /* For cli_printf variable args */
#include <project_config.h>
#include "arch_ops.h"
#include "queue.h"
#include "spinlock.h"


static struct {
    cli_getc_fn_t   getc;
    cli_puts_fn_t   puts;
    const char      *prompt;
    queue_t         *rx_queue;
    queue_t         *tx_queue;
    cli_command_t   commands[CLI_MAX_CMDS];
    uint32_t        cmd_count;
    uint32_t        line_pos;
    uint32_t        cursor_pos;     /* Current cursor position */
    spinlock_t      lock;
    uint8_t         esc_state;      /* 0: Normal, 1: ESC, 2: [ */
    char            line_buffer[CLI_MAX_LINE_LEN];
} cli_ctx;

/* Helper to send string to queue or puts callback */
static void cli_puts(const char *s) {
    if (cli_ctx.tx_queue) {
        /* Use bulk write for efficiency */
        size_t len = 0;
        const char *p = s;
        while (*s) {
            len++; s++;
        }
        queue_push_arr(cli_ctx.tx_queue, p, len);
    } else if (cli_ctx.puts) {
            cli_ctx.puts(s);
    }
}

/* Helper for integer formatting */
static void cli_fmt_int(char *buff, 
                        uint32_t *pos, 
                        uint32_t max, 
                        uintptr_t val, 
                        int width, 
                        char pad, 
                        int base, 
                        int neg,
                        int left_align) {
    char digits[12];
    int i = 0;
    do {
        int d = val % base;
        digits[i++] = (d < 10) ? ('0' + d) : ('a' + d - 10);
        val /= base;
    } while (val > 0 && i < 12);
    
    int actual_len = i + (neg ? 1 : 0);
    int padding = width - actual_len;
    
    if (!left_align) {
        /* If padding with '0', sign comes first (e.g. -001) */
        if (neg && pad == '0') {
            if (*pos < max - 1) buff[(*pos)++] = '-';
            neg = 0; 
        }
        
        while (padding > 0 && *pos < max - 1) {
            buff[(*pos)++] = pad;
            padding--;
        }
    }
    
    if (neg && *pos < max - 1) {
        buff[(*pos)++] = '-';
    }
    
    while (i > 0 && *pos < max - 1) {
        buff[(*pos)++] = digits[--i];
    }

    if (left_align) {
        while (padding > 0 && *pos < max - 1) {
            buff[(*pos)++] = ' ';
            padding--;
        }
    }
}

/* 
 * Tokenizer that splits the incoming string by spaces.
 * Replaces spaces with '\0' and saves the output in argv
 * Returns number of args (cmd is an arg).
 */
static uint32_t cli_tokenize(char *line, char **argv) {
    uint32_t argc = 0;
    uint8_t is_token = 0;
    while(*line && argc < CLI_MAX_ARGS) {
        /* skip spaces */
        while(*line == ' ' || *line == '\t') {
            *line++ = '\0';
            is_token = 0;
        }

        /* End of string */
        if(*line == '\0') {
            break;
        }

        /* Start of token */
        if(!is_token) {
            argv[argc++] = line; /* point to the start of the token */
            is_token = 1;
        }
        line++;
    }
    return argc;
}

/* Process the command line buffer */
static void cli_process_cmd(void) {
    char *argv[CLI_MAX_ARGS];
    int found = 0;
    int (*handler)(int, char**) = NULL;

    /* tokenize the buffer */
    int argc = cli_tokenize(cli_ctx.line_buffer, argv);

    if (argc == 0) { /* empty line */
        cli_puts(cli_ctx.prompt);
        return;
    } else {
        uint32_t flags = spin_lock(&cli_ctx.lock);
        for(uint32_t i = 0; i < cli_ctx.cmd_count; i++) {
            if(utils_strcmp(argv[0], cli_ctx.commands[i].name) == 0) {
                handler = cli_ctx.commands[i].handler;
                found = 1;
                break;
            }
        }
        spin_unlock(&cli_ctx.lock, flags);

        if (found && handler) {
            handler(argc, argv);
        }
    }

    if (!found) {
        cli_printf("Unknown command: %s\r\n", argv[0]);
        cli_printf("Type 'help' for list.\r\n");
    }

    /* Repost prompt */
    cli_puts(cli_ctx.prompt);
}

/* Print the list of registered commands */
static void cli_print_help(void) {
    cli_printf("Available commands:\r\n");
    
    uint32_t i = 0;
    while (1) {
        const char *name = NULL;
        const char *help = NULL;

        uint32_t flags = spin_lock(&cli_ctx.lock);
        if (i < cli_ctx.cmd_count) {
            name = cli_ctx.commands[i].name;
            help = cli_ctx.commands[i].help;
        }
        spin_unlock(&cli_ctx.lock, flags);

        if (!name) {
            break;
        }

        cli_printf("  %-10s %s\r\n", name, help);
        i++;
    }
}

/* Internal built-in help command handler */
static int cmd_help_handler(int argc, char **argv) {
    (void)argc; 
    (void)argv;
    cli_print_help();
    return 0;
}

static const cli_command_t help_cmd = {
    .name = "help",
    .help = "List commands",
    .handler = cmd_help_handler
};

/* 
 * Supports: %d (int), %u (unsigned), %x (hex), %s (string), %c (char), %% (literal %)
 */
uint32_t cli_printf(const char *text, ...) {
    char buff[CLI_MAX_LINE_LEN];
    uint32_t pos = 0;
    va_list args;
    va_start(args, text); /* initialize args to point to the first arg after text */

    while (*text && pos < CLI_MAX_LINE_LEN-1) {
        if (*text == '%') { /* need to insert an argument */
            text++;
            
            /* Parse flags */
            int left_align = 0;
            if (*text == '-') {
                left_align = 1;
                text++;
            }
            
            /* Parse width/padding */
            int width = 0;
            char pad_char = ' ';
            
            if (*text == '0') {
                pad_char = (left_align) ? ' ' : '0'; /* Ignore 0 if left align */
                text++;
            }
            while (*text >= '0' && *text <= '9') {
                width = width * 10 + (*text - '0');
                text++;
            }
            
            if(*text == 'd') { /* decimal */
                int arg = va_arg(args, int);
                unsigned int uval;
                int neg = 0;
                if(arg < 0) {
                    neg = 1;
                    uval = (unsigned int)0 - (unsigned int)arg; /* Safe negation for INT_MIN */
                } else {
                    uval = (unsigned int)arg;
                }
                cli_fmt_int(buff, &pos, CLI_MAX_LINE_LEN, (uintptr_t)uval, width, pad_char, 10, neg, left_align);
            }
            else if(*text == 'u') { /* unsigned */
                unsigned int arg = va_arg(args, unsigned int);
                cli_fmt_int(buff, &pos, CLI_MAX_LINE_LEN, (uintptr_t)arg, width, pad_char, 10, 0, left_align);
            }
            else if(*text == 'x') { /* hex */
                unsigned int arg = va_arg(args, unsigned int);
                cli_fmt_int(buff, &pos, CLI_MAX_LINE_LEN, (uintptr_t)arg, width, pad_char, 16, 0, left_align);
            }
            else if(*text == 'p') { /* pointer */
                void *ptr = va_arg(args, void *);
                uintptr_t val = (uintptr_t)ptr;
                
                if (pos < CLI_MAX_LINE_LEN - 2) {
                    buff[pos++] = '0';
                    buff[pos++] = 'x';
                }
                /* Format as hex. Width depends on platform (8 chars for 32-bit, 16 for 64-bit) */
                int ptr_width = sizeof(void*) * 2;
                cli_fmt_int(buff, &pos, CLI_MAX_LINE_LEN, val, ptr_width, '0', 16, 0, left_align);
            }
            else if(*text == 's') { /* string */
                const char *arg = va_arg(args, const char *);
                if(arg) {
                    size_t len = 0;
                    const char *temp = arg;
                    while(*temp++) len++;
                    
                    int padding = width - len;
                    if (!left_align) {
                        while (padding > 0 && pos < CLI_MAX_LINE_LEN-1) {
                            buff[pos++] = ' ';
                            padding--;
                        }
                    }
                    
                    while(*arg && pos < CLI_MAX_LINE_LEN-1) {
                        buff[pos++] = *arg++;
                    }
                    
                    if (left_align) {
                        while (padding > 0 && pos < CLI_MAX_LINE_LEN-1) {
                            buff[pos++] = ' ';
                            padding--;
                        }
                    }
                }
            }
            else if(*text == 'c') { /* char */
                if(pos < CLI_MAX_LINE_LEN-1) {
                    buff[pos++] = (char)va_arg(args, int);
                }
            }
            else if(*text == '%') { /* literal */
                if(pos < CLI_MAX_LINE_LEN-1) {
                    buff[pos++] = '%';
                }
            }
            text++;
        }
        else { /* no argument insertion. just append the text */
            buff[pos++] = *text++;
        }
    }
    buff[pos] = '\0'; /* add null terminate in the end*/
    va_end(args);
    if (pos > 0) {
        cli_puts(buff); /* write the text to cli */
    }
    return pos;
}


/* Register a new command in the cli */
int32_t cli_register_command(const cli_command_t *cmd) {
    uint32_t flags = spin_lock(&cli_ctx.lock);
    if(cli_ctx.cmd_count >= CLI_MAX_CMDS) {
        spin_unlock(&cli_ctx.lock, flags);
        return CLI_ERR;
    }
    if(!cmd || !cmd->name || !cmd->handler) {
        spin_unlock(&cli_ctx.lock, flags);
        return CLI_ERR;
    }

    cli_ctx.commands[cli_ctx.cmd_count++] = *cmd;
    spin_unlock(&cli_ctx.lock, flags);
    return CLI_OK;
}


/* Unregister a command in the cli */
int32_t cli_unregister_command(const char *name) {
    uint32_t flags = spin_lock(&cli_ctx.lock);
    for (uint32_t i = 0; i < cli_ctx.cmd_count; i++) {
        if(utils_strcmp(name, cli_ctx.commands[i].name) == 0) {
            cli_ctx.commands[i] = cli_ctx.commands[cli_ctx.cmd_count-1];
            cli_ctx.cmd_count--;
            spin_unlock(&cli_ctx.lock, flags);
            return CLI_OK;
        }
    }
    spin_unlock(&cli_ctx.lock, flags);
    return CLI_ERR_NOCMD;
}


/* Initialize the cli */
int32_t cli_init(const char *prompt, cli_getc_fn_t getc, cli_puts_fn_t puts) {
    utils_memset(&cli_ctx, 0, sizeof(cli_ctx));
    cli_ctx.prompt = prompt;
    cli_ctx.getc = getc;
    cli_ctx.puts = puts;
    spinlock_init(&cli_ctx.lock);

    cli_register_command(&help_cmd);

    return CLI_OK;
}

/* Set the input queue for the CLI */
void cli_set_rx_queue(queue_t *q) {
    cli_ctx.rx_queue = q;
}

/* Set the output queue for the CLI */
void cli_set_tx_queue(queue_t *q) {
    cli_ctx.tx_queue = q;
}

/* Main CLI task loop */
void cli_task_entry(void *arg) {
    (void)arg;
    char c;

    /* initial prompt */
    cli_puts("\r\n");
    cli_puts(cli_ctx.prompt);

    while(1) {
        if (cli_ctx.rx_queue) {
            /* Block on queue until a character arrives */
            if (queue_pop(cli_ctx.rx_queue, &c) != 0) {
                continue;
            }
        } else {
            /* Fallback to polling/notify if no queue set */
            int ret = cli_ctx.getc(&c); 
            if(ret == 0) {
                /* Block until notified by UART ISR (or timeout after 1s) */
                task_notify_wait(1, 1000); 
                continue;
            }
        }

        /* Escape Sequence Handling for Arrow Keys */
        if (cli_ctx.esc_state == 1) {
            if (c == '[') {
                cli_ctx.esc_state = 2;
            } else {
                cli_ctx.esc_state = 0;
            }
            continue;
        } else if (cli_ctx.esc_state == 2) {
            if (c == 'D') { /* Left Arrow */
                if (cli_ctx.cursor_pos > 0) {
                    cli_ctx.cursor_pos--;
                    cli_puts("\033[D");
                }
            } else if (c == 'C') { /* Right Arrow */
                if (cli_ctx.cursor_pos < cli_ctx.line_pos) {
                    cli_ctx.cursor_pos++;
                    cli_puts("\033[C");
                }
            }
            cli_ctx.esc_state = 0;
            continue;
        } else if (c == 0x1B) { /* ESC */
            cli_ctx.esc_state = 1;
            continue;
        }

        if (c == '\r' || c == '\n') { /* cmd line ended */
            cli_puts("\r\n"); /* start new line */
            cli_ctx.line_buffer[cli_ctx.line_pos] = '\0'; /* terminate string */
            cli_process_cmd();
            cli_ctx.line_pos = 0; /* reset buffer */
            cli_ctx.cursor_pos = 0;
        }
        /* Handle Backspace (ASCII 0x08 or DEL 0x7F) */
        else if (c == '\b' || c == 0x7F) {
            if (cli_ctx.cursor_pos > 0) {
                /* Shift buffer left */
                for (uint32_t i = cli_ctx.cursor_pos; i < cli_ctx.line_pos; i++) {
                    cli_ctx.line_buffer[i-1] = cli_ctx.line_buffer[i];
                }
                cli_ctx.line_pos--;
                cli_ctx.cursor_pos--;
                cli_ctx.line_buffer[cli_ctx.line_pos] = '\0';

                /* Visual update: Move back, print rest of line, erase last char */
                cli_puts("\b"); 
                cli_puts(&cli_ctx.line_buffer[cli_ctx.cursor_pos]); 
                cli_puts(" "); 
                
                /* Move cursor back to correct position */
                uint32_t diff = cli_ctx.line_pos - cli_ctx.cursor_pos + 1;
                while(diff--) {
                    cli_puts("\033[D");
                }
            }
        }
        /* Handle Standard Characters */
        else if (c >= ' ' && c <= '~') {
            if (cli_ctx.line_pos < CLI_MAX_LINE_LEN - 1) {
                /* Insert character */
                if (cli_ctx.cursor_pos < cli_ctx.line_pos) {
                    /* Shift right */
                    for (uint32_t i = cli_ctx.line_pos; i > cli_ctx.cursor_pos; i--) {
                        cli_ctx.line_buffer[i] = cli_ctx.line_buffer[i-1];
                    }
                    cli_ctx.line_buffer[cli_ctx.cursor_pos] = c;
                    cli_ctx.line_pos++;
                    cli_ctx.line_buffer[cli_ctx.line_pos] = '\0';
                    
                    /* Visual update: Print rest of line */
                    cli_puts(&cli_ctx.line_buffer[cli_ctx.cursor_pos]);
                    
                    /* Move cursor back to correct position */
                    cli_ctx.cursor_pos++;
                    uint32_t diff = cli_ctx.line_pos - cli_ctx.cursor_pos;
                    while(diff--) {
                        cli_puts("\033[D");
                    }
                } else {
                    /* Append at end */
                    cli_ctx.line_buffer[cli_ctx.line_pos++] = c;
                    cli_ctx.cursor_pos++;
                    char echo[2] = {c, '\0'};
                    cli_puts(echo);
                }
            } else {
                cli_puts("ERROR: BUFFER FULL\r\n");
            }
        }
    }
}
