#include <asf.h>
#include <conf_demo.h>
#include <stdio.h>
#include <string.h>
#include <sd_mmc_mem.h>
#include "demotasks.h"

//mutexes
static xSemaphoreHandle terminal_mutex;

//constant definitions
#define BUFFER_SIZE 50
#define RECEVER_TASK_PRIORITY (tskIDLE_PRIORITY + 5)
#define WRITER_TASK_PRIORITY (tskIDLE_PRIORITY + 5)
#define RECEVER_TASK_DELAY	(10 / portTICK_RATE_MS)
#define WRITER_TASK_DELAY	(10 / portTICK_RATE_MS)


/* global variables */
static char g_buffer[BUFFER_SIZE] = {0};
static const char filename[] = "ed.txt";
struct usart_module usart_instance;
struct usart_config usart_conf;


/**
* \brief mount a FAT file system from a flash drive connected to EXT1
*/
int mount_fs()
{
	FATFS fs;
	FRESULT res;
	Ctrl_status status;
	
    delay_init();
    irq_initialize_vectors();
	cpu_irq_enable();
	
    /* Initialize SD MMC stack */
	sd_mmc_init();
	
    /* wait until SD card is ready and plugged in */
	do {
		status = sd_mmc_test_unit_ready(0);
		if (CTRL_FAIL == status) {
			printf("Card install FAIL\n\r");
			printf("Please unplug and re-plug the card.\n\r");
			while (CTRL_NO_PRESENT != sd_mmc_check(0)) {
			}
		}
	} while (CTRL_GOOD != status);
	
    /* mount fs */
	printf("Mounting FATfs...\n");
    memset(&fs, 0, sizeof(FATFS));
    res = f_mount(LUN_ID_SD_MMC_0_MEM, &fs);
    if (FR_OK != res) {
        printf("[FAIL] res %d\r\n", res);
        return 1;
    }
	printf("[OK]\r\n");
	return 0;
}

/**
* \brief parses command and calls the appropriate function to treat it
    Available commands:
        -r            reads the contents of the file
        -i [message]  writes [message] to the file
*/
void parse_command(char * cmd)
{
	if(sizeof(cmd) < 3 || cmd[0] != '-')
	{
		print_usage();
		return;
	}
	if(cmd[1] == 'r')
	{
		read_cmd();
	}
	else if(cmd[1] == 'i')
	{
		write_buffer(cmd);
	}
	else
	{
		print_usage();
	}
}


/**
* \brief prints how to use the commands to write and read the file 
*/
void print_usage()
{
	printf("Invalid command\nUse -r to read file\n-i message to write message\n");
}


/**
* \brief read the text file 
*/
void read_cmd()
{
	char line[BUFFER_SIZE];
	int res = FR_OK;
	printf("Reading the file...\n");
	FIL fd; 
	res = f_open(&fd, filename, FA_READ | FA_OPEN_ALWAYS);
	if( res != FR_OK)
	{
		printf("Erro abrindo o arquivo para leitura: %d\n", res);
		return;
	}
	while(f_gets(line, BUFFER_SIZE, &fd)) 
	{
		printf(line);
	}
	printf("Closing after reading...");
	res = f_close(&fd);
	if(res != FR_OK)
	{
		printf("\t[ERROR %d]\n", res);
		return;
	}
	printf("\t[OK]\n");
	return;
}

/**
* \brief writes string to text file
*/
void write_string(char * string)
{
	FIL fd;
	int res;
	res = f_open(&fd, filename, FA_OPEN_ALWAYS | FA_WRITE);
	if(res != FR_OK)
	{
		printf("Error opening file reading: %d\n", res);
		return;
	}
	if(f_lseek(&fd, f_size(&fd)) != FR_OK)
	{
		printf("Error using f_lseek()!\n");
	}
	res = f_puts(string, &fd);
	if(res == EOF)
	{
		printf("Error writing to file\n");
	}
	printf("Closing after writing...");
	res = f_close(&fd);
	if(res != FR_OK)
	{
		printf("\t[ERROR %d]\n", res);
		return;
	}
	printf("\t[OK]\n");
	return;
}

/** 
* \brief writes received message to buffer to be acessed by @ref writer() 
*/
void write_buffer(char * string)
{
	/* if message is just space, write nothing */
	if(sizeof(string) < 4)
	{
		printf("Message is too short\n");
		return;
	}
    /* skip the first 3 characters that are not the message ("-i ") */
	char * message = string + 3;        
	/* verify if the next character is not terminator */
	if(*message == '\0')
	{
		printf("Message is invalid\n");
		return;
	}
	printf("Writing to buffer: %s", message);
	/* send message to write buffer (this functions does not write, but send the data to writer task) */
	strcpy(g_buffer, message);
	return;
}


/**
* \brief configures USART comunication with \param baudrate bps 
*/
void set_usart_config(int32_t baudrate)
{
	usart_get_config_defaults(&usart_conf);
	usart_conf.baudrate    = baudrate;
	usart_conf.mux_setting = EDBG_CDC_SERCOM_MUX_SETTING;
	usart_conf.pinmux_pad0 = EDBG_CDC_SERCOM_PINMUX_PAD0;
	usart_conf.pinmux_pad1 = EDBG_CDC_SERCOM_PINMUX_PAD1;
	usart_conf.pinmux_pad2 = EDBG_CDC_SERCOM_PINMUX_PAD2;
	usart_conf.pinmux_pad3 = EDBG_CDC_SERCOM_PINMUX_PAD3;
	stdio_serial_init(&usart_instance, EDBG_CDC_MODULE, &usart_conf);
	usart_enable(&usart_instance);
}


/**
* \brief main task that receives data from USART and interprets commands, executing them 
*/
void receiver(void * param)
{
	char input_buffer[BUFFER_SIZE];
	for(;;)
	{
		xSemaphoreTake(terminal_mutex, portMAX_DELAY);
		printf("Input command:\n");
		fgets(input_buffer, BUFFER_SIZE, stdin);
		parse_command(input_buffer);
		xSemaphoreGive(terminal_mutex);
		vTaskDelay(RECEVER_TASK_DELAY);
	}
}


/**
* \brief main task that receives data from @ref receiver() and writes it to file 
*/
void writer(void * param)
{
	for(;;)
	{
		xSemaphoreTake(terminal_mutex, portMAX_DELAY);
		/* Check if anything is available on buffer */
		if(*g_buffer == 0)
		{
			/* if there is nothing on the buffer, simply exit */
		}
		else
		{
			/* if buffer is available, write the contents of the buffer to file */
			write_string(g_buffer);
			memset(g_buffer, 0, BUFFER_SIZE);
		}
		xSemaphoreGive(terminal_mutex);
		vTaskDelay(WRITER_TASK_DELAY);
	}
}

/**
 * \brief Initialize tasks and mutexes
 */
void demotasks_init(void)
{
	int error_return = 0;

	terminal_mutex = xSemaphoreCreateMutex();
	
	error_return = xTaskCreate(receiver,
			(const char *) "RX",
			2048,
			NULL,
			RECEVER_TASK_PRIORITY,
			NULL);
	if(error_return == errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY)
	{
		printf("Error creating task: receiver\n");
	}
	error_return = xTaskCreate(writer,
			(const char *) "WR",
			2048,
			NULL,
			WRITER_TASK_PRIORITY,
			NULL);
	if(error_return == errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY)
	{
		printf("Error creating task: writer\n");
	}
}
