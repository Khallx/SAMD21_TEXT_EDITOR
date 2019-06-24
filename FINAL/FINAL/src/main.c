#include <asf.h>
#include "demotasks.h"


int main (void)
{
	system_init();
    /* initialize USART configuration */ 
	set_usart_config(9600);
    printf("Welcome to microTextEditor!\n");
    /* Mount SD card FATfs */
	mount_fs();
    /* create tasks */
	demotasks_init();
	
	/*start scheduler */
	vTaskStartScheduler();

	do {
		// Intentionally left empty
	} while (true);
}
