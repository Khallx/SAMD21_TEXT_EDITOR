#include <asf.h>
#include "demotasks.h"


int main (void)
{
	system_init();
	//initialize tasks
	demotasks_init();
	printf("Welcome to microTextEditor!\n");
	//start scheduler
	vTaskStartScheduler();

	do {
		// Intentionally left empty
	} while (true);
}
