#include "unity.h"
#include "cli.h"
#include "queue.h"
#include "allocator.h"
#include "scheduler.h"
#include "test_common.h"
#include <string.h>
#include <stdio.h>
#include "spinlock.h"
#include <project_config.h>

static uint8_t heap[8192];
static queue_t *rx_q;
static queue_t *tx_q;


/* Mock callbacks (unused when queues are set) */
static int mock_getc(char *c) { 
    (void)c; 
    return 0; 
}

static int mock_puts(const char *s) { 
    (void)s; 
    return 0; 
}

static void dummy_task(void *arg) { 
    (void)arg; 
}

/* Drain TX queue into a buffer for verification */
static void get_tx_string(char *buf, size_t max_len) {
    size_t i = 0;
    char c;
    while(i < max_len - 1 && queue_pop_from_isr(tx_q, &c) == 0) {
        buf[i++] = c;
    }
    buf[i] = '\0';
}

/* Push string to RX queue */
static void push_rx_string(const char *str) {
    while(*str) {
        queue_push_from_isr(rx_q, str);
        str++;
    }
}

/* Run the CLI task until it blocks (yields) */
static void run_cli_until_block(void) {
    if (setjmp(yield_jump) == 0) {
        cli_task_entry(NULL);
    }
}

/* Custom command handler for testing */
static int test_cmd_called = 0;
static int test_cmd_argc = 0;
static char test_cmd_last_arg[32];

static int my_cmd_handler(int argc, char **argv) {
    test_cmd_called++;
    test_cmd_argc = argc;
    if (argc > 1) {
        strncpy(test_cmd_last_arg, argv[1], sizeof(test_cmd_last_arg)-1);
    }
    return 0;
}

static void setUp_local(void) {
    allocator_init(heap, sizeof(heap));
    scheduler_init();
    
    /* Create a dummy task and set it as current so queue operations don't crash */
    task_create(dummy_task, NULL, 512, 1);
    task_set_current(scheduler_get_task_by_index(0));
    
    rx_q = queue_create(1, 128);
    tx_q = queue_create(1, 1024);
    
    cli_init("TEST> ", mock_getc, mock_puts);
    cli_set_rx_queue(rx_q);
    cli_set_tx_queue(tx_q);
    
    mock_yield_count = 0;
}

static void tearDown_local(void) {
    queue_delete(rx_q);
    queue_delete(tx_q);
}

/* Verify CLI initialization */
void test_cli_init_should_register_help(void) {
    /* Help command should be registered by default */
    push_rx_string("help\r");
    run_cli_until_block();
    
    char output[512];
    get_tx_string(output, sizeof(output));
    TEST_ASSERT_NOT_NULL(strstr(output, "Available commands"));
}

/* Verify command registration and execution */
void test_cli_register_and_execute_command(void) {
    cli_command_t cmd = { 
        .name = "test", 
        .help = "Test cmd", 
        .handler = my_cmd_handler 
    };
    TEST_ASSERT_EQUAL(CLI_OK, cli_register_command(&cmd));
    
    test_cmd_called = 0;
    push_rx_string("test arg1\r");
    run_cli_until_block();
    
    TEST_ASSERT_EQUAL(1, test_cmd_called);
    TEST_ASSERT_EQUAL(2, test_cmd_argc); /* cmd + arg1 */
    TEST_ASSERT_EQUAL_STRING("arg1", test_cmd_last_arg);
}

/* Verify unregistering a command */
void test_cli_unregister_command(void) {
    cli_command_t cmd = { 
        .name = "temp", 
        .help = "Temp", 
        .handler = my_cmd_handler 
    };
    cli_register_command(&cmd);
    
    TEST_ASSERT_EQUAL(CLI_OK, cli_unregister_command("temp"));
    TEST_ASSERT_EQUAL(CLI_ERR_NOCMD, cli_unregister_command("temp")); /* Should fail 2nd time */
    
    /* Try to run it */
    push_rx_string("temp\r");
    run_cli_until_block();
    
    char output[256];
    get_tx_string(output, sizeof(output));
    TEST_ASSERT_NOT_NULL(strstr(output, "Unknown command"));
}

/* Verify cli_printf formatting - Integers */
void test_cli_printf_integers(void) {
    cli_printf("Int: %d, Neg: %d, Zero: %d", 123, -456, 0);
    
    char output[128];
    get_tx_string(output, sizeof(output));
    TEST_ASSERT_EQUAL_STRING("Int: 123, Neg: -456, Zero: 0", output);
}

/* Verify cli_printf formatting - Padding */
void test_cli_printf_padding(void) {
    cli_printf("|%5d|%05d|%-5d|", 12, 12, 12); 
    
    char output[128];
    get_tx_string(output, sizeof(output));
    /* Right align space, Right align zero, Left align space */
    TEST_ASSERT_EQUAL_STRING("|   12|00012|12   |", output);
}

/* Verify cli_printf formatting - Negative Zero Padding */
void test_cli_printf_negative_padding(void) {
    cli_printf("%05d", -1);
    
    char output[64];
    get_tx_string(output, sizeof(output));
    TEST_ASSERT_EQUAL_STRING("-0001", output);
}

/* Verify cli_printf formatting - Hex and Strings */
void test_cli_printf_hex_string(void) {
    cli_printf("Hex: %x, Str: %s", 0xAB, "Hello");
    
    char output[128];
    get_tx_string(output, sizeof(output));
    TEST_ASSERT_EQUAL_STRING("Hex: ab, Str: Hello", output);
}

/* Verify cli_printf formatting - Unsigned */
void test_cli_printf_unsigned(void) {
    cli_printf("%u", 0xFFFFFFFF);
    
    char output[64];
    get_tx_string(output, sizeof(output));
    TEST_ASSERT_EQUAL_STRING("4294967295", output);
}

/* Verify cli_printf formatting - Pointers */
void test_cli_printf_pointer(void) {
    void *ptr = (void*)0x1234ABCD;
    cli_printf("Ptr: %p", ptr);
    
    char output[64];
    get_tx_string(output, sizeof(output));
    
    if (sizeof(void *) == 8) {
        TEST_ASSERT_EQUAL_STRING("Ptr: 0x000000001234abcd", output);
    } else {
        TEST_ASSERT_EQUAL_STRING("Ptr: 0x1234abcd", output);
    }
}

/* Verify cli_printf formatting - String Padding */
void test_cli_printf_string_padding(void) {
    cli_printf("|%10s|%-10s|", "foo", "bar");
    
    char output[64];
    get_tx_string(output, sizeof(output));
    TEST_ASSERT_EQUAL_STRING("|       foo|bar       |", output);
}

/* Verify Backspace editing */
void test_cli_editing_backspace(void) {
    /* Type "he", then backspace, then "elp" -> "help" */
    push_rx_string("he\belp\r");
    
    /* Drain initial prompt from setup/run */
    char dummy[128];
    get_tx_string(dummy, sizeof(dummy));
    
    run_cli_until_block();
    
    char output[512];
    get_tx_string(output, sizeof(output));
    
    /* Should see the help menu */
    TEST_ASSERT_NOT_NULL(strstr(output, "Available commands"));
}

/* Verify Arrow Key editing (Insertion) */
void test_cli_editing_arrows_insert(void) {
    /* 
     * Sequence:
     * 1. "hlp"
     * 2. Left Arrow x 2 (cursor at 'l')
     * 3. 'e' (insert 'e' -> "help")
     * 4. Enter
     */
    push_rx_string("hlp");
    push_rx_string("\x1B[D"); /* Left */
    push_rx_string("\x1B[D"); /* Left */
    push_rx_string("e");
    push_rx_string("\r");
    
    /* Drain prompt */
    char dummy[128];
    get_tx_string(dummy, sizeof(dummy));
    
    run_cli_until_block();
    
    char output[512];
    get_tx_string(output, sizeof(output));
    
    /* Should execute help */
    TEST_ASSERT_NOT_NULL(strstr(output, "Available commands"));
}

/* Verify Arrow Key editing (Right Arrow) */
void test_cli_editing_arrows_right(void) {
    /* 
     * Sequence:
     * 1. "hel"
     * 2. Left (at 'l')
     * 3. Right (at end)
     * 4. 'p'
     * 5. Enter
     */
    push_rx_string("hel");
    push_rx_string("\x1B[D");
    push_rx_string("\x1B[C");
    push_rx_string("p\r");
    
    run_cli_until_block();
    
    char output[512];
    get_tx_string(output, sizeof(output));
    TEST_ASSERT_NOT_NULL(strstr(output, "Available commands"));
}

/* Verify buffer overflow protection */
void test_cli_buffer_overflow(void) {
    /* Fill buffer beyond limit */
    for(int i=0; i < CLI_MAX_LINE_LEN + 2; i++) {
        push_rx_string("a");
    }
    push_rx_string("\r");
    
    run_cli_until_block();
    
    char output[1024];
    get_tx_string(output, sizeof(output));
    TEST_ASSERT_NOT_NULL(strstr(output, "ERROR: BUFFER FULL"));
}

/* Verify empty line handling */
void test_cli_empty_line(void) {
    push_rx_string("\r");
    run_cli_until_block();
    
    char output[128];
    get_tx_string(output, sizeof(output));
    /* Should just reprint prompt */
    TEST_ASSERT_NOT_NULL(strstr(output, "TEST>"));
}

/* Handler that registers another command (verifies lock is released during execution) */
static int recursive_cmd_handler(int argc, char **argv) {
    (void)argc; (void)argv;
    cli_command_t new_cmd = { 
        .name = "inner", 
        .help = "Inner", 
        .handler = my_cmd_handler 
    };
    return cli_register_command(&new_cmd);
}

void test_cli_handler_can_register_command(void) {
    cli_command_t cmd = { 
        .name = "outer", 
        .help = "Outer", 
        .handler = recursive_cmd_handler 
    };
    cli_register_command(&cmd);
    
    push_rx_string("outer\r");
    run_cli_until_block();
    
    /* If we are here, it didn't deadlock. Now verify "inner" exists. */
    test_cmd_called = 0;
    push_rx_string("inner\r");
    run_cli_until_block();
    
    TEST_ASSERT_EQUAL(1, test_cmd_called);
}

void run_cli_tests(void) {
    printf("\n=== Starting CLI Tests ===\n");
    test_setUp_hook = setUp_local;
    test_tearDown_hook = tearDown_local;
    UnitySetTestFile("tests/test_cli.c");
    
    RUN_TEST(test_cli_init_should_register_help);
    RUN_TEST(test_cli_register_and_execute_command);
    RUN_TEST(test_cli_unregister_command);
    RUN_TEST(test_cli_printf_integers);
    RUN_TEST(test_cli_printf_padding);
    RUN_TEST(test_cli_printf_negative_padding);
    RUN_TEST(test_cli_printf_hex_string);
    RUN_TEST(test_cli_printf_unsigned);
    RUN_TEST(test_cli_printf_pointer);
    RUN_TEST(test_cli_printf_string_padding);
    RUN_TEST(test_cli_editing_backspace);
    RUN_TEST(test_cli_editing_arrows_insert);
    RUN_TEST(test_cli_editing_arrows_right);
    RUN_TEST(test_cli_buffer_overflow);
    RUN_TEST(test_cli_empty_line);
    RUN_TEST(test_cli_handler_can_register_command);
    
    printf("=== CLI Tests Complete ===\n");
}