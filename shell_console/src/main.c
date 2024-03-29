/*
 * Copyright (c) 2017 Linaro Limited
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/console/console.h>
#include "shell_browser_console.h"
#include "shell_browser.h"
#include <zephyr/shell/shell.h>
#define OUTPUT_STACK_SIZE 500
#define OUTPUT_STACK_PRIORITY 5

#define THREAD_WAIT        -1

//K_THREAD_DEFINE( output_tid, OUTPUT_STACK_SIZE,
//                 shell_console_output_loop, NULL, NULL, NULL,
//                 OUTPUT_STACK_PRIORITY, 0, THREAD_WAIT);

/* static inline void _exit_qemu() { */
/*   register uint32_t r0 __asm__("r0"); */
/*   r0 = 0x18; */
/*   register uint32_t r1 __asm__("r1"); */
/*   r1 = 0x20026; */
/*   __asm__ volatile("bkpt #0xAB"); */
/* } */

SHELL_BROWSER_CONSOLE_DEFINE( sbc, "$", 128, 128, 128 );

void main(void)
{
  console_init();
  shell_browser_console_init( &sbc);

  printk("shell console\n");

  char c = '\0';
  while ( c != 'q') {
    c = console_getchar();
    //printk("%c",c);
   shell_browser_console_putc( &sbc, c);
  }
  //enable_shell_console( NULL);
  //k_thread_start( output_tid);
  //shell_console_input_loop();
  //k_thread_join( output_tid, K_FOREVER);
  printk( "leave main\n");
  //_exit_qemu();
}
