#include <asf.h>
#include <conf_demo.h>
#include <stdio.h>
#include <string.h>
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
void set_usart_config(int32_t baudrate);
void parse_command(char * cmd);
void print_usage();
void write_buffer(char * string);
void read_cmd();
static void receiver(void * param);
static void writer(void * param);

//global variables
static char g_buffer[BUFFER_SIZE] = {0};

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
	printf("Reading the file...\n");
	//MUTEX DO ARQUIVO
	//ABRIR ARQUIVO
	//LER ELE DO COMEÇO
	//FECHAR ARQUIVO
	//FECHAR MUTEX
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
	char * write_buffer;
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
			printf("%s", g_buffer);
			memset(g_buffer, 0, BUFFER_SIZE);
		}
		/*if(xQueueReceive(QMessage, write_buffer, 0))
		{
			printf("Recebi: %s", write_buffer);
		}
		else
		{
			printf("nada no queue\n");
		}*/
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
	set_usart_config(9600);
	terminal_mutex = xSemaphoreCreateMutex();
	queue_mutex = xSemaphoreCreateMutex();
	QMessage = xQueueCreate(1, sizeof(char *));
	if(QMessage == 0)
	{
		printf("Failed to create Queue\n");
	}
	
	error_return = xTaskCreate(receiver,
			(const char *) "RX",
			configMINIMAL_STACK_SIZE + 514,
			NULL,
			RECEVER_TASK_PRIORITY,
			NULL);
	if(error_return == errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY)
	{
		printf("Erro criando receiver\n");
	}
	error_return = xTaskCreate(writer,
			(const char *) "WR",
			configMINIMAL_STACK_SIZE + 128,
			NULL,
			WRITER_TASK_PRIORITY,
			NULL);
	if(error_return == errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY)
	{
		printf("Erro criando writer\n");
	}
}
