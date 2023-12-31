/*
 * "Hello World" example.
 *
 * This example prints 'Hello from Nios II' to the STDOUT stream. It runs on
 * the Nios II 'standard', 'full_featured', 'fast', and 'low_cost' example
 * designs. It runs with or without the MicroC/OS-II RTOS and requires a STDOUT
 * device in your system's hardware.
 * The memory footprint of this hosted application is ~69 kbytes by default
 * using the standard reference design.
 *
 * For a reduced footprint version of this template, and an explanation of how
 * to reduce the memory footprint for a given application, see the
 * "small_hello_world" template.
 *
 */

#include <stdio.h>
#include "system.h"
#include "altera_avalon_pio_regs.h"




int main()
{
	while (1){
//		printf("start \n");
		int buttons;
		buttons = IORD(BUTTON_PIO_BASE,0);
		int egm_base = IOWR(EGM_BASE,0,1);
		if(buttons == 7){
			printf("%d \n",buttons);
		}
		IOWR(LED_PIO_BASE,0,0X0A);
		printf("This is the egm base %d \n",egm_base);

	}
	return 0;
}
