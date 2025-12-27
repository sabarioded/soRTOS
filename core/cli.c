#include "cli.h"
#include "scheduler.h" /* For task_sleep_ticks */
#include <string.h>    /* For strcmp, strlen, memset */
#include <stdarg.h>    /* For cli_printf variable args */
#include <project_config.h>


static struct {
    cli_command_t   commands[CLI_MAX_CMDS];
    uint32_t        cmd_count;
    char            line_buffer[CLI_MAX_LINE_LEN];
    uint32_t        line_pos;

    cli_getc_fn_t   getc;
    cli_puts_fn_t   puts;
    const char      *prompt;
} cli_ctx;


/* Tockenizer that splits the incoming string by spaces
 * Replaces spaces with '\0' and saves the output in argv
 * Returns number of args (cmd is an arg)
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


static void cli_process_cmd(void) {
    char *argv[CLI_MAX_ARGS];
    int found = 0;

    /* tokenize the buffer */
    int argc = cli_tokenize(cli_ctx.line_buffer, argv);

    if (argc == 0) { /* empty line */
        cli_ctx.puts(cli_ctx.prompt);
        return;
    } else {
        for(uint32_t i = 0; i < cli_ctx.cmd_count; i++) {
            if(strcmp(argv[0], cli_ctx.commands[i].name) == 0) {
                cli_ctx.commands[i].handler(argc, argv);
                found = 1;
                break;
            }
        }
    }

    if (!found) {
        cli_printf("Unknown command: %s\r\n", argv[0]);
        cli_printf("Type 'help' for list.\r\n");
    }

    /* Repost prompt */
    cli_ctx.puts(cli_ctx.prompt);
}


static void cli_print_help(void) {
    cli_printf("Available commands:\r\n");
    for (uint32_t i = 0; i < cli_ctx.cmd_count; i++) {
        cli_printf("  %-10s %s\r\n", cli_ctx.commands[i].name, cli_ctx.commands[i].help);
    }
}

/* Internal built-in help command handler */
static int cmd_help_handler(int argc, char **argv) {
    (void)argc; (void)argv;
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
            if(*text == 'd') { /* decimal */
                int arg = va_arg(args, int);
                unsigned int uval;
                /* determine the sign */
                if(arg < 0) {
                    buff[pos++] = '-';
                    uval = (unsigned int)(-arg);  // Safe conversion
                } else {
                    uval = (unsigned int)arg;
                }

                char digit_buff[12]; /* buffer for digits. 10 digits (32bit) + null + sign */
                int i = 0;
                do {
                    digit_buff[i++] = '0' + (uval % 10);
                    uval = uval / 10;
                } while(uval > 0 && i < 12);

                /* reverse the digits to the result buffer */
                while(i > 0 && pos < CLI_MAX_LINE_LEN-1) {
                    buff[pos++] = digit_buff[--i];
                }
            }
            else if(*text == 'u') { /* unsigned */
                unsigned int arg = va_arg(args, unsigned int);
                char digit_buff[12]; /* buffer for digits. 11 digits + null */
                int i = 0;
                do {
                    digit_buff[i++] = '0' + (arg % 10);
                    arg = arg / 10;
                } while(arg > 0 && i < 12);

                /* reverse the digits to the result buffer */
                while(i > 0 && pos < CLI_MAX_LINE_LEN-1) {
                    buff[pos++] = digit_buff[--i];
                }
            }
            else if(*text == 'x') { /* hex */
                unsigned int arg = va_arg(args, unsigned int);
                /* add "0x" prefix */
                if (pos < CLI_MAX_LINE_LEN - 2) {
                    buff[pos++] = '0';
                    buff[pos++] = 'x';
                }
                char digit_buff[10]; /* buffer for 8 digits + safety */
                int i = 0;
                do {
                    unsigned int digit = (arg % 16);
                    digit_buff[i++] =  digit < 10 ? '0' + digit : 'a' + digit;
                    arg = arg / 16;
                } while (arg > 0 && i < 10);

                while(i > 0 && pos < CLI_MAX_LINE_LEN-1) {
                    buff[pos++] = digit_buff[--i];
                }
            }
            else if(*text == 's') { /* string */
                const char *arg = va_arg(args, const char *);
                if(arg) {
                    while(*arg && pos < CLI_MAX_LINE_LEN-1) {
                    buff[pos++] = *arg++;
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
    if (pos > 0 && cli_ctx.puts) {
        cli_ctx.puts(buff); /* write the text to cli */
    }
    return pos;
}


/* register a new command in the cli */
int32_t cli_register_command(const cli_command_t *cmd) {
    if(cli_ctx.cmd_count >= CLI_MAX_CMDS) return CLI_ERR;
    if(!cmd || !cmd->name || !cmd->handler) return CLI_ERR;

    cli_ctx.commands[cli_ctx.cmd_count++] = *cmd;
    return CLI_OK;
}


/* unregister a new command in the cli */
int32_t cli_unregister_command(const char *name) {
    for (uint32_t i = 0; i < cli_ctx.cmd_count; i++) {
        if(strcmp(name, cli_ctx.commands[i].name) == 0) {
            cli_ctx.commands[i] = cli_ctx.commands[cli_ctx.cmd_count-1];
            cli_ctx.cmd_count--;
            return CLI_OK;
        }
    }
    return CLI_ERR_NOCMD;
}


/* Initialize the cli */
int32_t cli_init(const char *prompt, cli_getc_fn_t getc, cli_puts_fn_t puts) {
    memset(&cli_ctx, 0, sizeof(cli_ctx));
    cli_ctx.prompt = prompt;
    cli_ctx.getc = getc;
    cli_ctx.puts = puts;

    cli_register_command(&help_cmd);

    return CLI_OK;
}


void cli_task_entry(void *arg) {
    (void)arg;
    char c;

    /* initial prompt */
    cli_ctx.puts("\r\n");
    cli_ctx.puts(cli_ctx.prompt);

    while(1) {
        int ret = cli_ctx.getc(&c); /* 1 if char is read, 0 otherwise */

        if(ret == 0) {
            /* if no char read wait for 20 ticks */
            task_sleep_ticks(20); 
            continue;
        }

        if (c == '\r' || c == '\n') { /* cmd line ended */
            cli_ctx.puts("\r\n"); /* start new line */
            cli_ctx.line_buffer[cli_ctx.line_pos] = '\0'; /* terminate string */
            cli_process_cmd();
            cli_ctx.line_pos = 0; /* reset buffer */
        }
        /* Handle Backspace (ASCII 0x08 or DEL 0x7F) */
        else if (c == '\b' || c == 0x7F) {
            if (cli_ctx.line_pos > 0) {
                cli_ctx.line_pos--;
                cli_ctx.puts("\b \b"); /* Visual erase: back, space, back */
            }
        }
        /* Handle Standard Characters */
        else if (c >= ' ' && c <= '~') {
            if (cli_ctx.line_pos < CLI_MAX_LINE_LEN - 1) {
                cli_ctx.line_buffer[cli_ctx.line_pos++] = c;
                char echo[2] = {c, '\0'};
                cli_ctx.puts(echo); /* Echo back to user */
            } else {
                /* Buffer full - optional: beep or ignore */
            }
        }
    }
}
