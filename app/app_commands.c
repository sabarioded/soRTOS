#include "app_commands.h"
#include "cli.h"
#include "scheduler.h"
#include "stm32_alloc.h"
#include "systick.h"
#include "utils.h"

/* Forward declarations */
static int cmd_heap_stats_handler(int argc, char **argv);
static int cmd_task_list_handler(int argc, char **argv);
static int cmd_uptime_handler(int argc, char **argv);
static int cmd_kill_handler(int argc, char **argv);
static int cmd_reboot_handler(int argc, char **argv);

#if TASK_STACK_ALLOC_MODE == TASK_ALLOC_DYNAMIC
/******************* For heap test *******************/
static int cmd_heap_test_handler(int argc, char **argv);
/* Pseudo-random number generator for stress testing */
static uint32_t prng_state = 1234;
static uint32_t mini_rand(void) {
    prng_state = prng_state * 1664525 + 1013904223;
    return prng_state;
}

/* Fills memory with a verifiable pattern (Index based) */
static void fill_pattern(uint8_t *ptr, size_t size) {
    for (size_t i = 0; i < size; i++) {
        ptr[i] = (uint8_t)(i & 0xFF);
    }
}

/* Verifies the pattern. Returns 0 on success, -1 on error */
static int verify_pattern(uint8_t *ptr, size_t size) {
    for (size_t i = 0; i < size; i++) {
        if (ptr[i] != (uint8_t)(i & 0xFF)) {
            return -1;
        }
    }
    return 0;
}

#define TEST_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        cli_printf("[FAIL] %s\r\n", msg); \
        return -1; \
    } \
} while(0)
/****************************************************/
#endif

/* Command definitions */
static const cli_command_t heap_stats_cmd = {
    .name = "heap",
    .help = "Show heap statistics (dynamic mode only)",
    .handler = cmd_heap_stats_handler
};

static const cli_command_t task_list_cmd = {
    .name = "tasks",
    .help = "List all tasks",
    .handler = cmd_task_list_handler
};

static const cli_command_t uptime_cmd = {
    .name = "uptime",
    .help = "How long the system is up",
    .handler = cmd_uptime_handler
};

static const cli_command_t kill_cmd = {
    .name = "kill",
    .help = "kill <task_id> : kill a task",
    .handler = cmd_kill_handler
};

static const cli_command_t reboot_cmd = {
    .name = "reboot",
    .help = "reboot the system",
    .handler = cmd_reboot_handler
};

#if TASK_STACK_ALLOC_MODE == TASK_ALLOC_DYNAMIC
static const cli_command_t heap_test_cmd = {
    .name = "heaptest",
    .help = "Stress test heap: heaptest <basic|frag|stress> [size]",
    .handler = cmd_heap_test_handler
};
#endif

/* Command handlers */
static int cmd_heap_stats_handler(int argc, char **argv)
{
    (void)argc;
    (void)argv;

#if TASK_STACK_ALLOC_MODE == TASK_ALLOC_DYNAMIC
    heap_stats_t stats;
    if (stm32_allocator_get_stats(&stats) == 0) {
        cli_printf("Heap Statistics:\r\n");
        cli_printf("  Total size:     %u bytes\r\n", (unsigned int)stats.total_size);
        cli_printf("  Used:           %u bytes\r\n", (unsigned int)stats.used_size);
        cli_printf("  Free:           %u bytes\r\n", (unsigned int)stats.free_size);
        cli_printf("  Largest block:  %u bytes\r\n", (unsigned int)stats.largest_free_block);
        cli_printf("  Allocated blocks: %u\r\n", (unsigned int)stats.allocated_blocks);
        cli_printf("  Free fragments:   %u\r\n", (unsigned int)stats.free_blocks);
        
        if (stats.total_size > 0) {
            unsigned int percent = (stats.used_size * 100) / stats.total_size;
            cli_printf("  Usage:           %u%%\r\n", percent);
        }
        
        /* Check integrity */
        if (stm32_allocator_check_integrity() == 0) {
            cli_printf("  Status:          OK\r\n");
        } else {
            cli_printf("  Status:          CORRUPTED!\r\n");
        }
    } else {
        cli_printf("Heap not initialized\r\n");
    }
#else
    cli_printf("Heap statistics only available in dynamic allocation mode\r\n");
    cli_printf("Current mode: STATIC (stacks embedded in task_list[])\r\n");
#endif

    return 0;
}

static int cmd_task_list_handler(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    extern task_t task_list[MAX_TASKS];

    cli_printf("Task List:\r\n");
    cli_printf("ID   State      Stack Location\r\n");
    cli_printf("---  ---------  --------------\r\n");

    /* Count active tasks */
    uint32_t count = 0;
    for (uint32_t i = 0; i < MAX_TASKS; i++) {
        if (task_list[i].state != TASK_UNUSED) {
            const char *state_str;
            switch (task_list[i].state) {
                case TASK_READY:    state_str = "READY"; break;
                case TASK_RUNNING:  state_str = "RUNNING"; break;
                case TASK_BLOCKED:  state_str = "BLOCKED"; break;
                case TASK_ZOMBIE:   state_str = "ZOMBIE"; break;
                default:            state_str = "UNKNOWN"; break;
            }

#if TASK_STACK_ALLOC_MODE == TASK_ALLOC_STATIC
            cli_printf("%u   %s      STATIC\r\n", task_list[i].task_id, state_str);
#else
            if (task_list[i].stack_ptr != NULL) {
                cli_printf("%u   %s      %x\r\n", task_list[i].task_id, state_str, (unsigned int)task_list[i].stack_ptr);
            } else {
                cli_printf("Error loacting memory");
            }
#endif
            count++;
        }
    }

    cli_printf("\r\nTotal tasks: %u\r\n", (unsigned int)count);

    return 0;
}

static int cmd_uptime_handler(int argc, char **argv) {
    (void)argc;
    (void)argv;

    /* every tick is 1ms */
    uint32_t ticks = systick_ticks;
    uint32_t seconds = ticks / 1000;
    uint32_t mili_sec = ticks % 1000;

    /* every 60sec is a minute*/
    uint32_t minutes = seconds / 60;
    seconds = seconds % 60;

    /* every 60 minutes is an hour */
    uint32_t hours = minutes / 60;
    minutes = minutes % 60;

    /* every 24 hours is a day*/
    uint32_t days = hours / 24;
    hours = hours % 24;

    cli_printf("Uptime: %u Days, %u Hours, %u Minutes, %u Seconds.%u\r\n",
                        days,    hours,    minutes,    seconds,   mili_sec);

    return 0;
}


static int cmd_kill_handler(int argc, char **argv) {
    if (argc < 2) {
        cli_printf("Usage: kill <id>\r\n");
        return -1;
    }

    uint16_t task_id = (uint16_t)atoi(argv[1]);
    int32_t result = task_delete(task_id);

    /* 2. User Feedback */
    if (result == TASK_DELETE_SUCCESS) {
        cli_printf("Task %u killed.\r\n", task_id);
    } else if (result == TASK_DELETE_TASK_NOT_FOUND) {
        cli_printf("Error: Task %u not found.\r\n", task_id);
    } else {
        cli_printf("Error: Could not kill task %u (Code %d).\r\n", task_id, result);
    }
    return 0;
}


static int cmd_reboot_handler(int argc, char **argv) {
    (void)argc; (void)argv;
    cli_printf("Rebooting system...\r\n");
    
    /* Standard Cortex-M reset: Write 0x5FA to AIRCR with SYSRESETREQ bit */
    #define SCB_AIRCR_SYSRESETREQ_MASK (1 << 2)
    #define SCB_AIRCR_VECTKEY_POS      16U
    #define SCB_AIRCR_VECTKEY_VAL      0x05FA
    
    SCB->AIRCR = (SCB_AIRCR_VECTKEY_VAL << SCB_AIRCR_VECTKEY_POS) | 
                 SCB_AIRCR_SYSRESETREQ_MASK;
                 
    while(1); /* Wait for hardware reset */
    return 0;
}

#if TASK_STACK_ALLOC_MODE == TASK_ALLOC_DYNAMIC
static int cmd_heap_test_handler(int argc, char **argv) {
    if (argc < 2) {
        cli_printf("Usage: heaptest <mode> [size]\r\n");
        cli_printf("Modes:\r\n");
        cli_printf("  basic <size>   : Malloc, Write, Realloc, Verify, Free\r\n");
        cli_printf("  frag           : Frag/Defrag test (Swiss Cheese)\r\n");
        cli_printf("  stress         : Randomized torture test\r\n");
        return -1;
    }

    const char *mode = argv[1];

    /* Capture baseline stats to account for system tasks */
    heap_stats_t base_stats;
    stm32_allocator_get_stats(&base_stats);
    size_t baseline_blocks = base_stats.allocated_blocks;

    /* ==========================================
     * MODE: BASIC (Integrity & Realloc)
     * ========================================== */
    if (strcmp(mode, "basic") == 0) {
        if (argc < 3) { cli_printf("Size required.\r\n"); return -1; }
        size_t size = (size_t)atoi(argv[2]);
        if (size == 0) {
             cli_printf("Invalid size (0 or not a number): %s\r\n", argv[2]);
             return -1;
        }
        
        cli_printf("1. Allocating %u bytes...\r\n", size);
        uint8_t *ptr = stm32_allocator_malloc(size);
        TEST_ASSERT(ptr != NULL, "Malloc returned NULL");
        
        cli_printf("2. Writing pattern...\r\n");
        fill_pattern(ptr, size);
        
        cli_printf("3. Verifying pattern...\r\n");
        if (verify_pattern(ptr, size) != 0) {
            cli_printf("[FAIL] Data corruption detected!\r\n");
            stm32_allocator_free(ptr);
            return -1;
        }

        cli_printf("4. Reallocating to %u bytes (Growing)...\r\n", size * 2);
        uint8_t *new_ptr = stm32_allocator_realloc(ptr, size * 2);
        TEST_ASSERT(new_ptr != NULL, "Realloc returned NULL");
        
        /* Check if old data is still valid */
        if (verify_pattern(new_ptr, size) != 0) {
             cli_printf("[FAIL] Realloc corrupted old data!\r\n");
             stm32_allocator_free(new_ptr);
             return -1;
        }
        
        cli_printf("5. Freeing memory...\r\n");
        stm32_allocator_free(new_ptr);

        /* Final Integrity Check */
        TEST_ASSERT(stm32_allocator_check_integrity() == 0, "Heap corrupted after free");
        cli_printf("[PASS] Basic test passed.\r\n");
    }

    /* ==========================================
     * MODE: FRAG (Fragmentation & Coalescing)
     * ========================================== */
    else if (strcmp(mode, "frag") == 0) {
        #define FRAG_BLOCKS 5
        #define FRAG_SIZE 64
        void *ptrs[FRAG_BLOCKS];

        cli_printf("1. Allocating %d blocks of %d bytes...\r\n", FRAG_BLOCKS, FRAG_SIZE);
        for(int i=0; i<FRAG_BLOCKS; i++) {
            ptrs[i] = stm32_allocator_malloc(FRAG_SIZE);
            TEST_ASSERT(ptrs[i] != NULL, "Alloc failed");
            memset(ptrs[i], 0xAA, FRAG_SIZE); /* Poison memory */
        }

        cli_printf("2. Creating holes (Freeing index 1 and 3)...\r\n");
        /* Before: [0][1][2][3][4] */
        stm32_allocator_free(ptrs[1]);
        stm32_allocator_free(ptrs[3]);
        ptrs[1] = NULL;
        ptrs[3] = NULL;
        /* After:  [0]...[2]...[4] */

        TEST_ASSERT(stm32_allocator_check_integrity() == 0, "Integrity check failed after holes");
        
        /* Get stats to see if we have fragments */
        heap_stats_t stats;
        stm32_allocator_get_stats(&stats);
        cli_printf("   Fragments: %u (Expect > 1)\r\n", (unsigned int)stats.free_blocks);

        cli_printf("3. Freeing remaining blocks to force coalescing...\r\n");
        stm32_allocator_free(ptrs[0]);
        stm32_allocator_free(ptrs[2]);
        stm32_allocator_free(ptrs[4]);

        TEST_ASSERT(stm32_allocator_check_integrity() == 0, "Integrity check failed after full free");
        
        /* Verify everything merged back */
        stm32_allocator_get_stats(&stats);
        
        /* We expect allocated_blocks to return to baseline, and free_blocks to be 1 */
        if (stats.allocated_blocks == baseline_blocks && stats.free_blocks == 1) {
             cli_printf("[PASS] Coalescing working (1 large free block).\r\n");
        } else {
             cli_printf("[FAIL] Coalescing failed! Blocks: %u (Base: %u), Frags: %u\r\n", 
                        (unsigned int)stats.allocated_blocks, (unsigned int)baseline_blocks, (unsigned int)stats.free_blocks);
        }
    }

    /* ==========================================
     * MODE: STRESS (Randomized Torture)
     * ========================================== */
    else if (strcmp(mode, "stress") == 0) {
        #define STRESS_MAX_PTRS 32
        void *ptrs[STRESS_MAX_PTRS] = {0};
        int alloc_count = 0;
        
        cli_printf("Starting stress test (Loop 100 times)...\r\n");
        
        for (int i = 0; i < 100; i++) {
            /* Randomly decide to Malloc or Free */
            int action = mini_rand() % 2; // 0 = Alloc, 1 = Free
            
            /* Bias towards Alloc if empty */
            if (alloc_count == 0) action = 0;
            /* Bias towards Free if full */
            if (alloc_count >= STRESS_MAX_PTRS) action = 1;

            if (action == 0) {
                /* ALLOCATE */
                /* Find empty slot */
                int slot = -1;
                for(int k=0; k<STRESS_MAX_PTRS; k++) if(ptrs[k] == NULL) { slot=k; break; }
                
                if (slot != -1) {
                    size_t sz = (mini_rand() % 128) + 8; /* Random size 8-136 bytes */
                    ptrs[slot] = stm32_allocator_malloc(sz);
                    if (ptrs[slot]) {
                        memset(ptrs[slot], 0x55, sz); /* Touch memory */
                        alloc_count++;
                    }
                }
            } else {
                /* FREE */
                /* Find used slot */
                int slot = -1;
                // Try to find a non-null pointer
                for(int k=0; k<STRESS_MAX_PTRS; k++) {
                    if(ptrs[k] != NULL) {
                        if ((mini_rand() % 3) == 0) { // Random chance to pick this one
                            slot = k;
                            break;
                        }
                        slot = k; // Default to last found
                    }
                }

                if (slot != -1) {
                    stm32_allocator_free(ptrs[slot]);
                    ptrs[slot] = NULL;
                    alloc_count--;
                }
            }
            
            /* Periodic Integrity Check (every 10 ops) */
            if (i % 10 == 0) {
                if (stm32_allocator_check_integrity() != 0) {
                    cli_printf("[FAIL] Heap corrupted at iteration %d\r\n", i);
                    return -1;
                }
                cli_printf(".");
            }
        }
        
        /* Cleanup: Free everything left */
        cli_printf("\r\nCleaning up...\r\n");
        for (int i = 0; i < STRESS_MAX_PTRS; i++) {
            if (ptrs[i]) stm32_allocator_free(ptrs[i]);
        }
        
        TEST_ASSERT(stm32_allocator_check_integrity() == 0, "Final integrity check failed");
        cli_printf("[PASS] Stress test survived.\r\n");
    }
    
    else {
        cli_printf("Unknown mode: %s\r\n", mode);
        return -1;
    }

    return 0;
}
#endif

/* Register all commands */
void app_commands_register_all(void)
{
    cli_register_command(&heap_stats_cmd);
    cli_register_command(&task_list_cmd);
    cli_register_command(&uptime_cmd);
    cli_register_command(&kill_cmd);
    cli_register_command(&reboot_cmd);

#if TASK_STACK_ALLOC_MODE == TASK_ALLOC_DYNAMIC
    cli_register_command(&heap_test_cmd);
#endif
}

