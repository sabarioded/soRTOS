# 1. Connect to the OpenOCD GDB Server (usually port 3333)
target remote localhost:3333

# 2. Reset the device and halt execution
monitor reset halt

# 3. Load the compiled program (.elf) into the chip's flash
load

# 4. Enable "Pretty Printing" (makes structures easier to read)
set print pretty on

# 5. Define a custom command to inspect your OS state
#    Typing 'os_info' in GDB will run this sequence
define os_info
    printf "--- System Tick: %u ---\n", systick_ticks
    printf "--- Current Task ---\n"
    print *task_current
    printf "--- Task List Summary ---\n"
    # Print the first 5 tasks to see what's happening
    print task_list[0]
    print task_list[1]
    print task_list[2]
end

# 6. Set a breakpoint at the scheduler context switch
#    This lets you inspect the OS every time it switches tasks
break schedule_next_task

# 7. Start execution
continue


# 1. start openocd: openocd -f board/st_nucleo_l4.cfg
# 2. run gdb with my script arm-none-eabi-gdb -x debug.gdb build/stm32_cli_os.elf
# 3. it will hit breakpoint in schedule_next_task. can os_info to get current state.