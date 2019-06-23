#include <asf.h>
#include <conf_demo.h>
#include <stdio.h>
#include <string.h>
#include <sd_mmc_mem.h>
#include "demotasks.h"

//mutexes
static xSemaphoreHandle terminal_mutex;
static xSemaphoreHandle queue_mutex;
//message queues
static QueueHandle_t QMessage;

//constant definitions
#define BUFFER_SIZE 100
#define RECEVER_TASK_PRIORITY (tskIDLE_PRIORITY + 5)
#define WRITER_TASK_PRIORITY (tskIDLE_PRIORITY + 5)
#define RECEVER_TASK_DELAY	(10 / portTICK_RATE_MS)
#define WRITER_TASK_DELAY	(10 / portTICK_RATE_MS)

//function prototypes
/*
void set_usart_config(int32_t baudrate);
void parse_command(char * cmd);
void print_usage();
void write_buffer(char * string);
void read_cmd();
static void receiver(void * param);
static void writer(void * param);
void write_string(char * string);
int mount_fs();
*/
//global variables
static char g_buffer[BUFFER_SIZE] = {0};
static const char filename[] = "ed.txt";


//mount a FAT file system from a flash drive connected to EXT1
int mount_fs()
{
	FATFS fs;
	FRESULT res;
	Ctrl_status status;
	char test_file_name[] = "0:ola.txt";
	FIL file_object;
	
    delay_init();
    irq_initialize_vectors();
	cpu_irq_enable();
	
    /* Initialize SD MMC stack */
	sd_mmc_init();
	
	do {
		status = sd_mmc_test_unit_ready(0);
		if (CTRL_FAIL == status) {
			printf("Card install FAIL\n\r");
			printf("Please unplug and re-plug the card.\n\r");
			while (CTRL_NO_PRESENT != sd_mmc_check(0)) {
			}
		}
	} while (CTRL_GOOD != status);
	
	printf("Mounting FATfs...\n");
    memset(&fs, 0, sizeof(FATFS));
    res = f_mount(LUN_ID_SD_MMC_0_MEM, &fs);
    if (FR_OK != res) {
        printf("[FAIL] res %d\r\n", res);
        return 1;
    }
    printf("[OK]\r\n");
	test_file_name[0] = LUN_ID_SD_MMC_0_MEM + '0';
	printf("%s\n", test_file_name);
	res = f_open(&file_object,
			(char const *)test_file_name,
			FA_OPEN_ALWAYS | FA_WRITE);
	if (res != FR_OK) {
		printf("[FAIL] res %d\r\n", res);
	}
	f_puts("ola", &file_object);
	f_close(&file_object);
	return 0;
}


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


void print_usage()
{
	printf("Invalid command\nUse -r to read file\n-i message to write message\n");
}


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
	f_close(&fd);
}

void write_string(char * string)
{
	FIL fd;
	int res;
	res = f_open(&fd, filename, FA_OPEN_ALWAYS | FA_WRITE);
	if(res != FR_OK)
	{
		printf("Erro abrindo o arquivo para escrita: %d\n", res);
		return;
	}
	if(f_lseek(&fd, f_size(&fd)) != FR_OK)
	{
		printf("Erro no lseek()\n");
	}
	f_puts(string, &fd);
	f_close(&fd);
}

void write_buffer(char * string)
{
	//if message is just space, write nothing
	if(sizeof(string) < 4)
	{
		printf("Message is too short\n");
		return;
	}
	char * message = string + 3;        //skip the first 3 characters that are not the message ("-i ")
	//verify if the next character is not terminator
	if(*message == '\0')
	{
		printf("Message is invalid\n");
		return;
	}
	printf("Writing to buffer: %s", message);
	strcpy(g_buffer, message);
	return;
}


//GLOBAL SCOPE VARIABLES RESPOSABLE FOR USART CONFIGURATIONS
struct usart_module usart_instance;
struct usart_config usart_conf;

//configures USART comunication with \param baudrate bps
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


void receiver(void * param)
{
	char input_buffer[BUFFER_SIZE];
	for(;;)
	{
		xSemaphoreTake(terminal_mutex, portMAX_DELAY);
		printf("Escreva um comando:\n");
		fgets(input_buffer, BUFFER_SIZE, stdin);
		parse_command(input_buffer);
		xSemaphoreGive(terminal_mutex);
		vTaskDelay(RECEVER_TASK_DELAY);
	}
}



void writer(void * param)
{
	for(;;)
	{
		xSemaphoreTake(terminal_mutex, portMAX_DELAY);
		printf("Dentro do semafaro\n");
		if(*g_buffer == 0)
		{
			printf("buffer vazio\n");
		}
		else
		{
			write_string(g_buffer);
			memset(g_buffer, 0, BUFFER_SIZE);
		}
		xSemaphoreGive(terminal_mutex);
		vTaskDelay(WRITER_TASK_DELAY);
	}
}

/**
 * \brief Initialize tasks and resources for demo
 */
void demotasks_init(void)
{
	int error_return = 0;
	//configure USART
	terminal_mutex = xSemaphoreCreateMutex();
	queue_mutex = xSemaphoreCreateMutex();
	QMessage = xQueueCreate(1, sizeof(char *));
	if(QMessage == 0)
	{
		printf("Failed to create Queue\n");
	}
	
	error_return = xTaskCreate(receiver,
			(const char *) "RX",
			1024,
			NULL,
			RECEVER_TASK_PRIORITY,
			NULL);
	if(error_return == errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY)
	{
		printf("Erro criando receiver\n");
	}
	error_return = xTaskCreate(writer,
			(const char *) "WR",
			1024,
			NULL,
			WRITER_TASK_PRIORITY,
			NULL);
	if(error_return == errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY)
	{
		printf("Erro criando writer\n");
	}
}
