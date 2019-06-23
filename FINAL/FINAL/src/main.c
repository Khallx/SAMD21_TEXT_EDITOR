#include <asf.h>
#include "demotasks.h"


int main (void)
{
	system_init();
	//initialize tasks
	set_usart_config(9600);
	mount_fs();
	demotasks_init();
	printf("Welcome to microTextEditor!\n");
	//start scheduler
	vTaskStartScheduler();

	do {
		// Intentionally left empty
	} while (true);
}
