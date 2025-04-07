
#ifdef WIN32

#include <windows.h>
#include <process.h>
#include <fcntl.h>
#include <sys/types.h>
#include <errno.h>
#include <time.h>
#include <stdint.h>
#include "windows_sub.h"
#else	//WIN32
#include "avr_compiler.h"
#include "GPRS_Uart.h"
#include "hardware.h"
#endif	//WIN32


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "log.h"
#include "def.h"
#include "sub.h"

#include "TunDevice.h"
#include "JennicModule.h"

uint8_t key_a,key_b,key_c;

#undef SIM_900

#ifndef WIN32
#define SIM_900
#endif

#define MAX_CLIENTS 7
#ifdef WIN32
SOCKET my_sock[MAX_CLIENTS];
#endif //WIN32
static volatile unsigned int butes_reciv = 0;
static volatile int ipv6_len = 0;
static unsigned char ipv6_buf[2048];
char pin[5];


uint8_t t_min_no_connect = 0;
#define MAX_TIME_NO_CONNECT 6
#define TIME_TEST_CONNECT (MAX_TIME_NO_CONNECT-1)


#ifndef SIM_900

static volatile unsigned char thread_ok = 0;

 

// макрос для печати количества активных пользователей
#define PRINTNUSERS if (nclients) {daemon_log(LOG_DEBUG,"%d user on-line\n", nclients);} \
        else {daemon_log(LOG_DEBUG,"No User on line\n");}
// глобальная переменная - количество активных пользователей
int nclients = 0;
int last_clients = 0;

// прототип функции, обслуживающий подключившихся пользователей
int SexToClient(int * client);

int MyThread(void *p) {
	char buff[1024]; // Буфер для различных нужд

	// Шаг 1 - Инициализация Библиотеки Сокетов
	// т.к. возвращенная функцией информация не используется
	// ей передается указатель на рабочий буфер, преобразуемый к указателю
	// на структуру WSADATA.
	// Такой прием позволяет сэкономить одну переменную, однако, буфер
	// должен быть не менее полкилобайта размером (структура WSADATA
	// занимает 400 байт)
	if (WSAStartup(0x0202, (WSADATA *)&buff[0]))
	{
		// Ошибка!
		daemon_log(LOG_ERR, "Error WSAStartup %d", WSAGetLastError());
		return -1;
	}

	// Шаг 2 - создание сокета
	SOCKET mysocket;
	// AF_INET - сокет Интернета
	// SOCK_STREAM - потоковый сокет (с установкой соединения)
	// 0 - по умолчанию выбирается TCP протокол
	if ((mysocket = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		// Ошибка!
		daemon_log(LOG_ERR, "Error socket %d", WSAGetLastError());
		WSACleanup(); // Деиницилизация библиотеки Winsock
		return -1;
	}

	// Шаг 3 - связывание сокета с локальным адресом
	struct sockaddr_in local_addr;
	local_addr.sin_family = AF_INET;
	local_addr.sin_port = htons(JENNIC_PORT); // не забываем о сетевом порядке!!!
	local_addr.sin_addr.s_addr = 0; // сервер принимает подключения
									// на все свои IP-адреса

									// вызываем bind для связывания
	if (bind(mysocket, (struct sockaddr *)&local_addr, sizeof(local_addr)))
	{
		// Ошибка
		daemon_log(LOG_ERR, "Error bind %d", WSAGetLastError());
		closesocket(mysocket); // закрываем сокет!
		WSACleanup();
		return -1;
	}

	// Шаг 4 - ожидание подключений
	// размер очереди - MAX_CLIENTS
	if (listen(mysocket, MAX_CLIENTS))
	{
		// Ошибка
		daemon_log(LOG_ERR, "Error listen %d", WSAGetLastError());
		closesocket(mysocket);
		WSACleanup();
		return -1;
	}

	daemon_log(LOG_DEBUG, "Wait clients");

	// Шаг 5 - извлекаем сообщение из очереди
	SOCKET client_socket; // сокет для клиента
	struct sockaddr_in client_addr; // адрес клиента (заполняется системой)

									// функции accept необходимо передать размер структуры
	int client_addr_size = sizeof(client_addr);

	thread_ok = 1;

	// цикл извлечения запросов на подключение из очереди
	while ((client_socket = accept(mysocket, (struct sockaddr *)&client_addr, \
		&client_addr_size)))
	{
		my_sock[last_clients = nclients] = client_socket;
		nclients++; // увеличиваем счетчик подключившихся клиентов

					// пытаемся получить имя хоста
		HOSTENT *hst;
		hst = gethostbyaddr((char *)&client_addr.sin_addr.s_addr, 4, AF_INET);

		// вывод сведений о клиенте
		daemon_log(LOG_DEBUG,"+%s [%s] new connect!\n",
			(hst) ? hst->h_name : "", inet_ntoa(client_addr.sin_addr));
		PRINTNUSERS

			// Вызов нового потока для обслужвания клиента
			// Да, для этого рекомендуется использовать _beginthreadex
			// но, поскольку никаких вызовов функций стандартной Си библиотеки
			// поток не делает, можно обойтись и CreateThread

		_beginthread(SexToClient, 0, &last_clients);
	}
	return 0;
}


// Эта функция создается в отдельном потоке
// и обсуживает очередного подключившегося клиента независимо от остальных
int SexToClient(int * client)
{
	int num_client = *client;

	// цикл эхо-сервера: прием строки от клиента и возвращение ее клиенту
	int bytes_r;
	while ((bytes_r = recv(my_sock[num_client], &ipv6_buf[0], sizeof(ipv6_buf), 0)) && bytes_r != SOCKET_ERROR) {
		//send(my_sock, &buff[0], bytes_recv, 0);
		ipv6_buf[bytes_r] = 0;
		if (verbosity >= LOG_DEBUG)
			daemon_log(LOG_DEBUG, "From client TCP/IP:%s", ipv6_buf);
		butes_reciv = bytes_r;
	}

	// если мы здесь, то произошел выход из цикла по причине
	// возращения функцией recv ошибки - соединение с клиентом разорвано
	nclients--; // уменьшаем счетчик активных клиентов
	daemon_log(LOG_DEBUG, "-disconnect\n"); 
	PRINTNUSERS

	// закрываем сокет
	closesocket(my_sock[num_client]);
	my_sock[num_client] = 0;
	return 0;
}
#else	//SIM_900

int sim_serial_fd;

int sim_serial_open(int port, uint32_t baud);
int sim_serial_read(unsigned char *data);
int sim_serial_write(const unsigned char data);

int sim_serial_open(int port, uint32_t baud)
{
#ifdef WIN32
	int fd;
	char device[80];
	DCB dcb;


	sprintf_s(device, 80, "\\\\.\\COM%d", port);


	daemon_log(LOG_INFO, "Opening serial device '%s' at baud rate %ubps", device, baud);


	fd = (int)CreateFile(device,
		GENERIC_READ | GENERIC_WRITE,
		0,
		0,
		OPEN_EXISTING,
		0,//FILE_FLAG_OVERLAPPED, 
		0);

	if ((HANDLE)fd == INVALID_HANDLE_VALUE) {
		daemon_log(LOG_ERR, "Couldn't open serial device %s", device, 0);
		return -1;
	}



	dcb.DCBlength = sizeof(DCB);
	if (!GetCommState((HANDLE)fd, &dcb)) {
		CloseHandle((HANDLE)fd);
		return -1;
	}
	dcb.BaudRate = baud;

	dcb.ByteSize = 8;
	if ((dcb.Parity = NOPARITY) != NOPARITY)
		dcb.fParity = TRUE;
	else
		dcb.fParity = FALSE;
	dcb.StopBits = ONESTOPBIT;


	dcb.fDsrSensitivity = FALSE;
	dcb.fAbortOnError = FALSE;
	dcb.fOutxCtsFlow = FALSE;
	dcb.fOutX = FALSE;
	dcb.fInX = FALSE;

	if (!SetCommState((HANDLE)fd, &dcb)) {
		daemon_log(LOG_ERR, "Error setting port settings ");
		return -1;
	}


	sim_serial_fd = fd;
	return fd;
#else	//WIN32
	return 0;
#endif
}

#ifdef WIN32
int sim_serial_read(unsigned char *data)
{
	COMSTAT comstat;
	DWORD   err = 0;
	int res = 0;
	int bytes = 0;


	ClearCommError((HANDLE)sim_serial_fd, &err, &comstat);
	if (comstat.cbInQue) {
		;
	}
	else {
		//printf("Serial read: %d\n", res);
		if (res == 0)
		{
			//daemon_log(LOG_ERR, "Serial connection to module interrupted");
			//bRunning = 0;
		}
		return 0;
	}

	ReadFile((HANDLE)sim_serial_fd, data, 1, &res, NULL);

	if (res > 0) {
#if DEBUG
		if (verbosity >= LOG_DEBUG) daemon_log(LOG_DEBUG, "RX %02x", *data);
#endif /* DEBUG */
	}

	return res;
}

int sim_serial_write(const unsigned char data)
{
	int err, attempts = 0;

	DWORD wLength;

#if DEBUG
	if (verbosity >= LOG_DEBUG) daemon_log(LOG_DEBUG, "TX %02x", data);
#endif /* DEBUG */


	err = WriteFile((HANDLE)sim_serial_fd, &data, 1, &wLength, NULL);
	if (err < 0)
	{
		if (errno == EAGAIN)
		{
			for (attempts = 0; attempts <= 5; attempts++)
			{
				Sleep(1000);
				err = WriteFile((HANDLE)sim_serial_fd, &data, 1, &wLength, NULL);
				if (err < 0)
				{
					if ((errno == EAGAIN) && (attempts == 5))
					{
						daemon_log(LOG_ERR, "Error writing to module after %d", attempts);
						exit(-1);
					}
				}
				else
				{
					break;
				}
			}
		}
		else
		{
			daemon_log(LOG_ERR, "Error writing to module");
			exit(-1);
		}
	}
	return 0;
}

#endif //WIN32











#define T_WAIT_PACKET	4000	//ms
#define T_WAIT_BUTE 300	//ms

typedef enum {
	AT_test = 0,
	ATE0,	//1
	CIFSR_test,	//2
	PIN_test,	//3
	PIN_set,	//4
	CIPMUX,	//5
	CREG,	//6
	CGATT,	//7
	CSQ,	//8
	CSTT,	//9
	CIICR,	//10
	BAD_PIN,	//11
	TO_Open,	//12
	Opened,	//13
}sim900state;

sim900state state = AT_test;
static uint8_t fl_read = 0;	//при state = Opened е = 1 и прескача автомата за стартиране на SIM
static uint8_t no_SimLoop = 0;
volatile uint16_t time_sleep_SIM = T_WAIT_PACKET;

static void SleepSIM(uint16_t sleep_time){
	cli();	//__disable_interrupt();
	time_sleep_SIM = sleep_time;
	sei();	//__enable_interrupt();
	no_SimLoop = 1;
	while(time_sleep_SIM)
		main_loop();
	no_SimLoop = 0;
}

void SimWrite(const unsigned char * text) {
	if (verbosity >= LOG_DEBUG)
		//daemon_log(LOG_DEBUG, "To %d %d %d SIM:%s", state, fl_read,no_SimLoop,text);
		daemon_log(LOG_DEBUG, "To SIM:%s",text);
	while (*text)
		sim_serial_write(*text++);
}

#define T_STATE_WAIT_SIM 5
static int recived = 0;
static int max_recived;
static unsigned int to_reciv = 0;
#define MAX_RECIV 200
static unsigned char buffer[MAX_RECIV];
static unsigned int client;
static unsigned int to_send = 0;
void SimRead(unsigned char *pBuf, int lenght) {
	cli();	//__disable_interrupt();
	time_sleep_SIM = T_WAIT_PACKET;
	sei();	//__enable_interrupt();
	fl_read = 1;
	recived = 0;
	max_recived = lenght;
}

static uint8_t fl_test_connect = 0;
static uint8_t fl_reject_resetSIM = 0;

static uint8_t first_time = 1;

void ResetSIM(void){
	if (verbosity >= LOG_DEBUG) {
		printf_P(PSTR("\n\rReset SIM in Time: "));
		PrintDateTime(date_time);
		printf_P(PSTR("\n\r"));
	}
	if( fl_reject_resetSIM ){
		fl_reject_resetSIM = 0;
		if (verbosity >= LOG_DEBUG) {
			printf_P(PSTR("Reject last client in Time: "));
			PrintDateTime(&sRouterStatus.sDateTimeLastClient.date_time[0]);
			printf_P(PSTR("\n\r"));
		}
		return;
	}
/*	if( state < BAD_PIN ){
		daemon_log(LOG_DEBUG, "Reject state=%d < BAD_PIN",state);
		return;
	}*/
	{
		daemon_log(LOG_DEBUG, "Off SIM");
		PORT_GPRS_RST.OUTSET = GPRS_RST;
		SleepSIM(1000);
		PORT_GPRS_RST.OUTCLR = GPRS_RST;
	
		cli();	//__disable_interrupt();
		time_sleep_SIM = 10000;
		sei();	//__enable_interrupt();
		no_SimLoop = 1;
		recived = 0;
		while(time_sleep_SIM){
			main_loop();
			uint8_t u8Data;
			while (sim_serial_read(&u8Data)) {
				if( recived < MAX_RECIV )
					buffer[recived] = u8Data;
				recived++;
				cli();	//__disable_interrupt();
				time_sleep_SIM = 2000;
				sei();	//__enable_interrupt();
			}
		}
		if (recived) {
			buffer[recived] = 0;
			daemon_log(LOG_DEBUG, "From SIM:%s",buffer);
			recived = 0;
		}
		no_SimLoop = 0;
		
		daemon_log(LOG_DEBUG, "On SIM");
		PORT_GPRS_RST.OUTSET = GPRS_RST;
		SleepSIM(1000);
		PORT_GPRS_RST.OUTCLR = GPRS_RST;
		
		cli();	//__disable_interrupt();
		time_sleep_SIM = 10000;
		sei();	//__enable_interrupt();
		no_SimLoop = 1;
		recived = 0;
		while(time_sleep_SIM){
			main_loop();
			uint8_t u8Data;
			while (sim_serial_read(&u8Data)) {
				if( recived < MAX_RECIV )
					buffer[recived] = u8Data;
				recived++;
				cli();	//__disable_interrupt();
				time_sleep_SIM = 200;
				sei();	//__enable_interrupt();
			}
		}
		if (recived) {
			buffer[recived] = 0;
			daemon_log(LOG_DEBUG, "From SIM:%s",buffer);
			recived = 0;
		}
		no_SimLoop = 0;
		
		cli();	//__disable_interrupt();
		time_sleep_SIM = T_WAIT_PACKET;
		sei();	//__enable_interrupt();
	
		fl_test_connect = 0;
		t_min_no_connect = 0;
		fl_read = 0;
	
		state = AT_test;
	}
}

void On_off_SIM(void){
	
	if( first_time )
		first_time = 0;
	else{
		sRouterStatus.u16SimErrors = htons(ntohs(sRouterStatus.u16SimErrors) + 1);
		memcpy(&(sRouterStatus.sDateTimeResetGPRS), date_time, sizeof(tsDateTime));
	}
	
	daemon_log(LOG_DEBUG, "On/Off SIM");
	fl_test_connect = 0;
	t_min_no_connect = 0;
	fl_read = 0;
	
	state = AT_test;
	
	PORT_GPRS_RST.OUTSET = GPRS_RST;
	SleepSIM(1000);
	PORT_GPRS_RST.OUTCLR = GPRS_RST;
	
	cli();	//__disable_interrupt();
	time_sleep_SIM = T_WAIT_PACKET;
	sei();	//__enable_interrupt();

}

void LoopRead(void) {
	if( t_min_no_connect >= MAX_TIME_NO_CONNECT ){
		//printf("                                             t_min_no_connect >= MAX_TIME_NO_CONNECT\n\r");
		On_off_SIM();
		return;
	} 
	
	if ( ( time_sleep_SIM == 0 ) ){
		if ( state == Opened ) {
			cli();	//__disable_interrupt();
			time_sleep_SIM = T_WAIT_PACKET;
			sei();	//__enable_interrupt();
			return;
		}
		fl_read = 0;
		if ( (recived == 0) && (state < Opened) ){
			//printf("                                               (recived == 0) && (state < Opened)\n\r");
			On_off_SIM();
		}else if (recived) {
			buffer[recived] = 0;
			if (verbosity >= LOG_DEBUG)
				//daemon_log(LOG_DEBUG, "From %d %d %d SIM:%s", state ,fl_read,no_SimLoop, buffer);
				daemon_log(LOG_DEBUG, "From SIM:%s",buffer);
			switch (state) {
			case AT_test:
			{
				if (recived == 10) {
					if (memcmp(buffer, "AT\r\n\r\nOK\r\n", recived) == 0) {
						state = ATE0;
					}
				}
				else if (recived == 6) {
					if (memcmp(buffer, "\r\nOK\r\n", recived) == 0) {
						state = CIFSR_test;
					}
				}
				break;
			}
			case ATE0:
			{
				if (recived == 12) {
					if (memcmp(buffer, "ATE0\r\n\r\nOK\r\n", recived) == 0) {
						state = CIFSR_test;
					}
				}
				break;
			}
			case CIFSR_test:
			{
				if (recived >10) {
					state = TO_Open;
				}else if( recived == 0 ){
					state = AT_test;
				}else {
					state = PIN_test;
				}
				break;
			}
			case PIN_test:
			{
				if (recived == 24) {
					if (memcmp(buffer, "\r\n+CPIN: SIM PIN\r\n\r\nOK\r\n", recived) == 0) {
						state = PIN_set;
					}
				}
				else if (recived == 22) {
					if (memcmp(buffer, "\r\n+CPIN: READY\r\n\r\nOK\r\n", recived) == 0) {
						state = CIPMUX;
					}
				}else{ 
					state = BAD_PIN;
					daemon_log(LOG_CRIT,"Error PIN test %s",buffer);
				}
				break;
			}
			case PIN_set:
			{
				if (recived == 6) {
					if (memcmp(buffer, "\r\nOK\r\n", recived) == 0) {
						state = PIN_test;
						break;
					}
				}else{
					state = BAD_PIN;
					daemon_log(LOG_CRIT,"Error SET PIN %s",buffer);
				}
				break;
			}
			case CIPMUX:
			{
				if (recived > 5) {
					{
						state = CREG;
					}
				}
				break;
			}
			case CREG:
			{
				if (recived == 20) {
					if (memcmp(buffer, "\r\n+CREG: 0,1\r\n\r\nOK\r\n", recived) == 0) {
						state = CGATT;
					}
				}
				break;
			}
			case CGATT:
			{
				if (recived == 19) {
					if (memcmp(buffer, "\r\n+CGATT: 1\r\n\r\nOK\r\n", recived) == 0) {
						state = CSQ;
					}
				}
				break;
			}
			case CSQ:
			{
				if (recived > 10) {
					sRouterStatus.u8CSQ = atoi((char*)(buffer + 7));
					{
						state = CSTT;
					}
				}
				break;
			}
			case CSTT:
			{
				if (recived > 5) {
					{
						state = CIICR;
					}
				}
				break;
			}
			case CIICR:
			{
				if (recived > 5) {
					{
						state = CIFSR_test;
					}
				}
				break;
			}
			case TO_Open:
			{
				if (recived > 5) {
					{
						state = Opened;
						t_min_no_connect = 0;
					}
				}
				break;
			}
			case BAD_PIN:
			case Opened:
				break;
			}
		}
	}	else {
		uint8_t u8Data;
		while (sim_serial_read(&u8Data)) {
			if( to_reciv )
				ipv6_buf[recived] = u8Data;
			if( recived < MAX_RECIV )
				buffer[recived] = u8Data;
			recived++;
			cli();	//__disable_interrupt();
			time_sleep_SIM = T_WAIT_BUTE;
			sei();	//__enable_interrupt();
			if (state == Opened) {
				if (recived && (recived == to_reciv)) {
					ipv6_buf[recived] = 0;
					if (verbosity >= LOG_DEBUG)
						daemon_log(LOG_DEBUG, "From client SIM:%s", ipv6_buf);
					butes_reciv = 	to_reciv;
					memcpy(&(sRouterStatus.sDateTimeLastClient), date_time, sizeof(tsDateTime));
					fl_reject_resetSIM = 1;
	
					to_reciv = 0;
					recived = 0;
				}
				switch (u8Data) {
				case '>':
					if( to_send ){
						unsigned int i;
						uint8_t data;
						for( i = 0 ; i< to_send ; i++ ){
							data = ipv6_buf[i];
							data >>= 4; data &= 0xF;
							if( data < 10 ) data += '0';	else	data += 'A' -10;
							sim_serial_write(data);
							
							data = ipv6_buf[i];
							data &= 0xF;
							if( data < 10 ) data += '0';	else	data += 'A' -10;
							sim_serial_write(data);
						}
						sim_serial_write(0x1A);
						daemon_log(LOG_DEBUG, "To client %d sended %d * 2 bytes", client,to_send);
						to_send=0;
					}
					break;
				case '+':
					buffer[0] = u8Data;
					recived = 1;
					break;
				case 0x0A:
				{
					buffer[recived] = 0;
					if (verbosity >= LOG_DEBUG)
						//daemon_log(LOG_DEBUG, "From %d %d %d SIM:%s", state ,fl_read,no_SimLoop, buffer);
						daemon_log(LOG_DEBUG, "From SIM:%s", buffer);
					if ( (recived > 12) && (memcmp(buffer, "+RECEIVE,", 9) == 0 )) {
					
						t_min_no_connect = 0;
						fl_test_connect = 0;
						
						to_reciv = atoi((char*)(buffer + 11));
						client = atoi((char*)(buffer + 9));
						daemon_log(LOG_DEBUG, "Client % d to recived SIM:%d", client,to_reciv);
					}else if( (recived > 11) && (memcmp(buffer, "+PDP: DEACT", 11) == 0 )){
						//printf("                                               (recived > 11) && (memcmp(buffer, +PDP: DEACT, 11) == 0 \n\r");
						On_off_SIM();
					}else if( (recived > 13) && (memcmp(buffer, "+CIPSERVER: 0", 13) == 0 )){
						//printf("                                                (recived > 13) && (memcmp(buffer, +CIPSERVER: 0, 13) == 0  \n\r");
						On_off_SIM();
					}else if( (recived > 13) && (memcmp(buffer, "+CIPSERVER: 1", 13) == 0 )){
						t_min_no_connect = 0;
						fl_test_connect = 0;
					}
					recived = 0;
					break;
				}
				}
			}
		}
	}
}

void SendOnlyCommand(uint8_t new_state){
	if( new_state > Opened ){
		printf("\n\rError state\n\r");
		return;
	}else
		printf("\n\r");
	switch( new_state ){
		case AT_test:
		{
			//SleepSIM(3000);
			SimWrite((unsigned char*)"AT\r\n");
			SimRead(buffer, sizeof(buffer));
		}
		break;
		case ATE0:
		{
			//SleepSIM(1000);
			SimWrite((unsigned char*)"ATE0\r\n");
			SimRead(buffer, sizeof(buffer));
		}
		break;
		case CIFSR_test:
		{
			//SleepSIM(1000);
			SimWrite((unsigned char*)"AT+CIFSR\r\n");
			SimRead(buffer, sizeof(buffer));
		}
		break;
		case PIN_test:
		{
			//SleepSIM(1000);
			SimWrite((unsigned char*)"AT+CPIN?\r\n");
			SimRead(buffer, sizeof(buffer));
		}
		break;
		case PIN_set:
		{
			//SleepSIM(3000);
			char text[50];
			int old_ver = verbosity;
			verbosity = LOG_WARNING;
			#ifdef WIN32
			sprintf_s(text,sizeof(text), "AT+CPIN=\"%s\"\r\n", pin);
			#else
			sprintf(text, "AT+CPIN=\"%s\"\r\n", pin);
			#endif
			SimWrite((unsigned char*)text);
			verbosity = old_ver;
			SimRead(buffer, sizeof(buffer));
		}
		break;
		case CIPMUX:
		{
			//SleepSIM(1000);
			SimWrite((unsigned char*)"AT+CIPMUX=1\r\n");
			SimRead(buffer, sizeof(buffer));
		}
		break;
		case CREG:
		{
			//SleepSIM(1000);
			SimWrite((unsigned char*)"AT+CREG?\r\n");
			SimRead(buffer, sizeof(buffer));
		}
		break;
		case CGATT:
		{
			//SleepSIM(4000);
			SimWrite((unsigned char*)"AT+CGATT?\r\n");
			SimRead(buffer, sizeof(buffer));		
		}
		break;
		case CSQ:
		{
			//SleepSIM(1000);
			SimWrite((unsigned char*)"AT+CSQ\r\n");
			SimRead(buffer, sizeof(buffer));
		}
		break;
		case CSTT:
		{
			//SleepSIM(1000);
			SimWrite((unsigned char*)"AT+CSTT=\"tvulosv\"\r\n");
			SimRead(buffer, sizeof(buffer));
		}
		break;
		case CIICR:
		{
			//SleepSIM(1000);
			SimWrite((unsigned char*)"AT+CIICR\r\n");
			SimRead(buffer, sizeof(buffer));
		}
		break;
		case BAD_PIN:
			printf("Not to sim write\n\r");
			break;
		case TO_Open:
		{
			//SleepSIM(1000);
			SimWrite((unsigned char*)"AT+CIPSERVER=1,1873\r\n");
			SimRead(buffer, sizeof(buffer));
		}
		break;
		case Opened:
		{
			printf("Not to sim write\n\r");
			SimRead(buffer, sizeof(buffer));
		}
		break;
	}
}

void SetSimState(uint8_t new_state){
	if( new_state > Opened ){
		printf("\n\rError state\n\r");
		return;
	}else
		printf("\n\r");
	switch( state = new_state ){
		case AT_test:
		{
			//SleepSIM(3000);
			SimWrite((unsigned char*)"AT\r\n");
			SimRead(buffer, sizeof(buffer));
		}
		break;
		case ATE0:
		{
			//SleepSIM(1000);
			SimWrite((unsigned char*)"ATE0\r\n");
			SimRead(buffer, sizeof(buffer));
		}
		break;
		case CIFSR_test:
		{
			//SleepSIM(1000);
			SimWrite((unsigned char*)"AT+CIFSR\r\n");
			SimRead(buffer, sizeof(buffer));
		}
		break;
		case PIN_test:
		{
			//SleepSIM(1000);
			SimWrite((unsigned char*)"AT+CPIN?\r\n");
			SimRead(buffer, sizeof(buffer));
		}
		break;
		case PIN_set:
		{
			//SleepSIM(3000);
			char text[50];
			int old_ver = verbosity;
			verbosity = LOG_WARNING;
			#ifdef WIN32
			sprintf_s(text,sizeof(text), "AT+CPIN=\"%s\"\r\n", pin);
			#else
			sprintf(text, "AT+CPIN=\"%s\"\r\n", pin);
			#endif
			SimWrite((unsigned char*)text);
			verbosity = old_ver;
			SimRead(buffer, sizeof(buffer));
		}
		break;
		case CIPMUX:
		{
			//SleepSIM(1000);
			SimWrite((unsigned char*)"AT+CIPMUX=1\r\n");
			SimRead(buffer, sizeof(buffer));
		}
		break;
		case CREG:
		{
			//SleepSIM(1000);
			SimWrite((unsigned char*)"AT+CREG?\r\n");
			SimRead(buffer, sizeof(buffer));
		}
		break;
		case CGATT:
		{
			//SleepSIM(4000);
			SimWrite((unsigned char*)"AT+CGATT?\r\n");
			SimRead(buffer, sizeof(buffer));		
		}
		break;
		case CSQ:
		{
			//SleepSIM(1000);
			SimWrite((unsigned char*)"AT+CSQ\r\n");
			SimRead(buffer, sizeof(buffer));
		}
		break;
		case CSTT:
		{
			//SleepSIM(1000);
			SimWrite((unsigned char*)"AT+CSTT=\"tvulosv\"\r\n");
			SimRead(buffer, sizeof(buffer));
		}
		break;
		case CIICR:
		{
			//SleepSIM(1000);
			SimWrite((unsigned char*)"AT+CIICR\r\n");
			SimRead(buffer, sizeof(buffer));
		}
		break;
		case BAD_PIN:
			printf("Not to sim write\n\r");
			break;
		case TO_Open:
		{
			//SleepSIM(1000);
			SimWrite((unsigned char*)"AT+CIPSERVER=1,1873\r\n");
			SimRead(buffer, sizeof(buffer));
		}
		break;
		case Opened:
		{
			printf("Not to sim write\n\r");
			SimRead(buffer, sizeof(buffer));
		}
		break;
	}
}

void PrintSimState(void)
{
	char text_state[20] = "Unknow";
	
	switch( state ){
		case AT_test:	strcpy_P(text_state,PSTR("0 AT_test"));	break;
		case ATE0:	strcpy_P(text_state,PSTR("1 ATE0"));	break;
		case CIFSR_test:	strcpy_P(text_state,PSTR("2 CIFSR_test"));	break;
		case PIN_test:	strcpy_P(text_state,PSTR("3 PIN_test"));	break;
		case PIN_set:	strcpy_P(text_state,PSTR("4 PIN_set"));	break;
		case CIPMUX:	strcpy_P(text_state,PSTR("5 CIPMUX"));	break;
		case CREG:	strcpy_P(text_state,PSTR("6 CREG"));	break;
		case CGATT:	strcpy_P(text_state,PSTR("7 CGATT"));	break;
		case CSQ:	strcpy_P(text_state,PSTR("8 CSQ"));	break;
		case CSTT:	strcpy_P(text_state,PSTR("9 CSTT"));	break;
		case CIICR:	strcpy_P(text_state,PSTR("10 CIICR"));	break;
		case BAD_PIN:	strcpy_P(text_state,PSTR("11 BAD_PIN"));	break;
		case TO_Open:	strcpy_P(text_state,PSTR("12 TO_Open"));	break;
		case Opened:	strcpy_P(text_state,PSTR("13 Opened"));	break;
	}
	printf_P(PSTR("\n\rSim status: %s fl_read = %d t_min_no_connect = %d\n\r\n\r"),text_state,fl_read,t_min_no_connect);
}

void SimLoop(void) {

	if (state < Opened) {
/*		pHC->fl_ready_to_open = false;
		pHC->fl_opened = false;
		pHC->fl_open = false;
		pHC->fl_close = false;
		pHC->fl_to_write = false;
		pHC->fl_write = false;*/
	}
	
	if(key_a){
		key_a = 0;
		//printf("opi2 $d\n\r",key_a);
		SimWrite((unsigned char*)"AT\r\n");
		SimRead(buffer, sizeof(buffer));
	}
		
	if (fl_read){
		if(key_b){
			key_b = 0;
			//printf("opi2 $d\n\r",key_b);
			SimWrite((unsigned char*)"AT+CIPSERVER?\r\n");
			SimRead(buffer, sizeof(buffer));
		}
		if(key_c){
			key_c = 0;
			//printf("opi2 $d\n\r",key_b);
			SimWrite((unsigned char*)"AT+CIPSTATUS\r\n");
			SimRead(buffer, sizeof(buffer));
		}
		if( state == Opened ){
			if( fl_test_connect == 0 ){
				if( t_min_no_connect >= TIME_TEST_CONNECT ){
					fl_test_connect = 1;
					SimWrite((unsigned char*)"AT+CIPSERVER?\r\n");
				}
			}
		}
		LoopRead();
	}else{
		switch (state) {
		case AT_test:
		{
			SleepSIM(3000);
			SimWrite((unsigned char*)"AT\r\n");
			SimRead(buffer, sizeof(buffer));
		}
		break;
		case ATE0:
		{
			SleepSIM(1000);
			SimWrite((unsigned char*)"ATE0\r\n");
			SimRead(buffer, sizeof(buffer));
		}
		break;
		case CIFSR_test:
		{
			SleepSIM(1000);
			SimWrite((unsigned char*)"AT+CIFSR\r\n");
			SimRead(buffer, sizeof(buffer));
		}
		break;
		case PIN_test:
		{
			SleepSIM(1000);
			SimWrite((unsigned char*)"AT+CPIN?\r\n");
			SimRead(buffer, sizeof(buffer));
		}
		break;
		case PIN_set:
		{
			SleepSIM(3000);
			char text[50];
			int old_ver = verbosity;
			verbosity = LOG_WARNING;
			#ifdef WIN32
			sprintf_s(text,sizeof(text), "AT+CPIN=\"%s\"\r\n", pin);
			#else
			sprintf(text, "AT+CPIN=\"%s\"\r\n", pin);
			#endif
			SimWrite((unsigned char*)text);
			verbosity = old_ver;
			SimRead(buffer, sizeof(buffer));
		}
		break;
		case CIPMUX:
		{
			SleepSIM(1000);
			SimWrite((unsigned char*)"AT+CIPMUX=1\r\n");
			SimRead(buffer, sizeof(buffer));
		}
		break;
		case CREG:
		{
			SleepSIM(1000);
			SimWrite((unsigned char*)"AT+CREG?\r\n");
			SimRead(buffer, sizeof(buffer));
		}
		break;
		case CGATT:
		{
			SleepSIM(4000);
			SimWrite((unsigned char*)"AT+CGATT?\r\n");
			SimRead(buffer, sizeof(buffer));		
		}
		break;
		case CSQ:
		{
			SleepSIM(1000);
			SimWrite((unsigned char*)"AT+CSQ\r\n");
			SimRead(buffer, sizeof(buffer));
		}
		break;
		case CSTT:
		{
			SleepSIM(1000);
			SimWrite((unsigned char*)"AT+CSTT=\"tvulosv\"\r\n");
			SimRead(buffer, sizeof(buffer));
		}
		break;
		case CIICR:
		{
			SleepSIM(1000);
			SimWrite((unsigned char*)"AT+CIICR\r\n");
			SimRead(buffer, sizeof(buffer));
		}
		break;
		case TO_Open:
		{
			SleepSIM(1000);
			SimWrite((unsigned char*)"AT+CIPSERVER=1,1873\r\n");
			SimRead(buffer, sizeof(buffer));
		}
		break;
		case Opened:
		{
			SimRead(buffer, sizeof(buffer));
		}
		break;
		case BAD_PIN:
			break;
		//default:
		//	break;
		}
	}
}

#endif //SIM_900

void TunLoop(void) {
#ifdef SIM_900
	if( no_SimLoop == 0 )
		SimLoop();
#endif
}

teTunStatus eTunDeviceOpen(int port, uint32_t baud)
{
  
#ifdef SIM_900
	if( sim_serial_open(port,baud) <0){
		daemon_log(LOG_ERR, "Error Opened COM in tun device");
		return E_TUN_ERROR;
	}
#else	//SIM_900
	_beginthread(MyThread,0, NULL);
	while (thread_ok == 0);
#endif
    daemon_log(LOG_DEBUG, "Opened COM in tun device");

    return E_TUN_OK;
}

static void SendPacage(int len){
#ifdef SIM_900
	to_send = len;
#ifdef WIN32
	sprintf_s((char*)buffer,sizeof(buffer), "AT+CIPSEND=%d\r\n", client);
#else //WIN32
	sprintf((char*)buffer,"AT+CIPSEND=%d\r\n",client);
#endif	//WIN32
	SimWrite((unsigned char*)buffer);
#else //SIM_900
#ifdef WIN32
	if (my_sock[last_clients]) {
		unsigned char buff[2048 * 2], data;
		int i;
		for (i = 0; i < len; i++) {
			data = ipv6_buf[i];
			data >>= 4; data &= 0xF;
			if (data < 10) data += '0';	else	data += 'A' - 10;
			buff[(i << 1)] = data;

			data = ipv6_buf[i];
			data &= 0xF;
			if (data < 10) data += '0';	else	data += 'A' - 10;
			buff[(i << 1) + 1] = data;
		}
		int sended = 0;
		do {
			sended = send(my_sock[last_clients], buff + sended, (len << 1) - sended, 0);
			if (sended <= 0) {
				daemon_log(LOG_ERR, "Error send socket %d", WSAGetLastError());
				return;
			}
		} while (sended != (len << 1));
		daemon_log(LOG_DEBUG, "To client %d sended %d * 2 bytes", last_clients, len);
	}
#endif //WIN32
#endif //SIM_900
}

static void SendACK(void){
	ipv6_buf[0] = (1) >> 8;
	ipv6_buf[1] = (1) & 0xFF;
	ipv6_buf[2] = VERSION;
	ipv6_buf[3] = ACK;
	SendPacage(HEADER_SIZE + 1);
}

teTunStatus eTunDeviceReadPacket(void)
{
	int len;

	if( butes_reciv ){

		unsigned int i;
		for (i = 0;i < (butes_reciv >> 1); i++) {
			unsigned char data;
			if (ipv6_buf[i << 1] >= 'A')
				data = ipv6_buf[i << 1] - 'A' + 10;
			else
				data = ipv6_buf[i << 1] - '0';
			data <<= 4;
			if (ipv6_buf[(i << 1)+1] >= 'A')
				data |= (ipv6_buf[(i << 1)+1] - 'A' + 10)&0x0F;
			else
				data |= (ipv6_buf[(i << 1)+1] - '0')&0x0F;
			ipv6_buf[i] = data;
		}
		
		len = ipv6_buf[0];
		len <<= 8;
		len |= ipv6_buf[1];
		if ((len + HEADER_SIZE) == (butes_reciv >> 1)) {
			switch(ipv6_buf[HEADER_SIZE]){
			case SET_WORK_HOURS:
				{
					tsSetWorkHours * psSetWorkHours = (tsSetWorkHours*)(ipv6_buf + HEADER_SIZE + 1);
					for (i = 0;i < u16_LampsInTable;i++) {
						if (memcmp(&((psLampTable + i)->sLampStatus.sMAC_Address.MAC[0]), psSetWorkHours->sMAC_Address.MAC, sizeof(tsMAC_Address)) == 0) {
							(psLampTable + i)->sLampStatus.u32WorkHours = psSetWorkHours->u32WorkHours;
							break;
						}
					}
					SendACK();
				}
				break;
			case SEND_LAMPS_MAC_TABLE:
				{
					uint16_t u16FirstTableEntry;
					uint16_t u16EntryCount;
					int i;

					tsSendTable * psSendLamsMAC = (tsSendTable*)(ipv6_buf + HEADER_SIZE + 1);
					tsMAC_Address * psMAC_Address = (tsMAC_Address*)(psSendLamsMAC + 1);

					u16_LampsInTable = 0;
					u16_LampsConnected = 0;

					u16FirstTableEntry = ntohs(psSendLamsMAC->u16FirstTableEntry);
					u16EntryCount = ntohs(psSendLamsMAC->u16EntryCount);

					for (i = 0; i < u16EntryCount;i++) {
						if( ( u16FirstTableEntry + i ) < ROUTE_TABLE_ENTRIES ){
							memcpy(&((psLampTable + u16FirstTableEntry + i)->sLampStatus.sMAC_Address.MAC[0]), psMAC_Address, sizeof(tsMAC_Address));
							memset(&((psLampTable + u16FirstTableEntry + i)->sLampStatus.sLastContacts), 0, sizeof(tsDateTime));
							memset(&((psLampTable + u16FirstTableEntry + i)->sLampStatus.u32WorkHours), 0, sizeof(uint32_t));
						}
						psMAC_Address++;
					}
					if (psSendLamsMAC->u8FlagEnd){
						if( ( u16_LampsInTable = u16FirstTableEntry + u16EntryCount ) > ROUTE_TABLE_ENTRIES )
							u16_LampsInTable = ROUTE_TABLE_ENTRIES;
					}
					SendACK();
				}
				break;
			case GET_LAMPS_STATUS:
				{
					uint16_t u16FirstTableEntry;
					uint16_t u16EntryCount;
					int i,cou=0;

					tsSendTable * psSendLamsStatus = (tsSendTable*)(ipv6_buf + HEADER_SIZE + 1);
					tsLampStatus * psLampStatus = (tsLampStatus*)(psSendLamsStatus + 1);

					u16FirstTableEntry = ntohs(psSendLamsStatus->u16FirstTableEntry);
					u16EntryCount = ntohs(psSendLamsStatus->u16EntryCount);

					for (i = u16FirstTableEntry; (i < u16_LampsInTable) && (u16EntryCount); i++) {
						u16EntryCount--;
						cou++;
						memcpy(psLampStatus, &((psLampTable + i)->sLampStatus), sizeof(tsLampStatus));
						psLampStatus++;
					}
					psSendLamsStatus->u16FirstTableEntry = htons(u16FirstTableEntry);
					psSendLamsStatus->u16EntryCount = htons(cou);
					if (i >= u16_LampsInTable)
						psSendLamsStatus->u8FlagEnd = 1;

					ipv6_buf[0] = (sizeof(tsSendTable) + (sizeof(tsLampStatus)*cou) + 1) >> 8;
					ipv6_buf[1] = (sizeof(tsSendTable) + (sizeof(tsLampStatus)*cou) + 1) & 0xFF;
					ipv6_buf[2] = VERSION;
					ipv6_buf[3] = SEND_LAMPS_STATUS;
					SendPacage(HEADER_SIZE + 1 + sizeof(tsSendTable) + (sizeof(tsLampStatus)*cou));
				}
				break;
			case COMMAND_GET_REJECT:
				ipv6_buf[0] = (sizeof(sRejectTable) + 1) >> 8;
				ipv6_buf[1] = (sizeof(sRejectTable) + 1) & 0xFF;
				ipv6_buf[2] = VERSION;
				ipv6_buf[3] = SEND_REJECT_TABLE;
				memcpy(ipv6_buf + HEADER_SIZE + 1, (unsigned char*)&sRejectTable, sizeof(sRejectTable));
				SendPacage(HEADER_SIZE + 1 + sizeof(sRejectTable));
				break;
			case COMMAND_GET_TIMERS:
				memcpy(&(psTimers->sDateTime), date_time, sizeof(tsDateTime));
				make_crc((unsigned char*)psTimers, sizeof(tsTimers));
				ipv6_buf[0] = (sizeof(tsTimers) + 1) >> 8;
				ipv6_buf[1] = (sizeof(tsTimers) + 1) & 0xFF;
				ipv6_buf[2] = VERSION;
				ipv6_buf[3] = COMMAND_SET_TIMERS;
				memcpy(ipv6_buf + HEADER_SIZE + 1, (unsigned char*)psTimers, sizeof(tsTimers));
				SendPacage(HEADER_SIZE + 1 + sizeof(tsTimers));
				break;
			case COMMAND_SET_TIMERS:
				memset(psTimers, 0, sizeof(tsTimers));	//V5 при SET... първо нулираме а после взимаме len-1 данни
				memcpy(psTimers, ipv6_buf + HEADER_SIZE + 1, len-1);
				make_crc((unsigned char*)psTimers, sizeof(tsTimers));
				SetDateTime();
				get_time();
				ipv6_buf[0] = (6 + 1) >> 8;
				ipv6_buf[1] = (6 + 1) & 0xFF;
				ipv6_buf[2] = VERSION;
				ipv6_buf[3] = SEND_DATE_TIME;
				memcpy(ipv6_buf + HEADER_SIZE + 1,date_time, 6);
				SendPacage(HEADER_SIZE + 1 + 6);
				break;
			case COMMAND_MAC_ADDRESS:
				ipv6_buf[0] = (8 + 1) >> 8;
				ipv6_buf[1] = (8 + 1) & 0xFF;
				ipv6_buf[2] = VERSION;
				ipv6_buf[3] = COMMAND_MAC_ADDRESS;
				memcpy(ipv6_buf + HEADER_SIZE + 1, (unsigned char*)(&sRouterAddress) + 8 , 8);

				ipv6_buf[4] ^= 0x02;

				SendPacage(HEADER_SIZE + 1 + 8);
				break;
			case COMMAND_GET_HOST_DATA:
				sModuleGetConfig.eRadioFrontEnd = psModuleSetConfig->eRadioFrontEnd;
				sModuleGetConfig.u8JenNetProfile = psModuleSetConfig->u8JenNetProfile;
				sModuleGetConfig.iAntennaDiversity = psModuleSetConfig->iAntennaDiversity;
				sModuleGetConfig.u8RadiusOff = psModuleSetConfig->u8RadiusOff;
				sModuleGetConfig.u16LampsInTable = htons(u16_LampsInTable);
				sModuleGetConfig.u16LampsConnected = htons(u16_LampsConnected);
				sModuleGetConfig.u8INT_resetGPRShours = psModuleSetConfig->u8INT_resetGPRShours;
				sModuleGetConfig.u8INT_resetGPRSminuts = psModuleSetConfig->u8INT_resetGPRSminuts;
				sModuleGetConfig.u8EnableEnergyMeter = psModuleSetConfig->u8EnableEnergyMeter;
				ipv6_buf[0] = (sizeof(tsConfigBorderRuter)+1) >> 8;
				ipv6_buf[1] = (sizeof(tsConfigBorderRuter)+1) & 0xFF;
				ipv6_buf[2] = VERSION;
				ipv6_buf[3] = COMMAND_SET_HOST_DATA;
				memcpy(ipv6_buf + HEADER_SIZE + 1, (unsigned char*)&sModuleGetConfig, sizeof(tsConfigBorderRuter));
				SendPacage( HEADER_SIZE + 1 + sizeof(tsConfigBorderRuter) );
				break;
			case COMMAND_SET_HOST_DATA:
				{
					memset(psModuleSetConfig, 0, sizeof(tsConfigBorderRuter));	//V5 при SET... първо нулираме а после взимаме len-1 данни
					memcpy(psModuleSetConfig, ipv6_buf + HEADER_SIZE + 1, len-1);
					make_crc((unsigned char*)psModuleSetConfig, sizeof(tsConfigBorderRuter));
					//int i;
					//for ( i = 0; i < sizeof(tsConfigBorderRuter); i++)printf("%02X", *(((unsigned char*)psModuleSetConfig) + i));
					//printf("\n\r sizeof(tsConfigBorderRuter) %d len - 1 %d\n\r",sizeof(tsConfigBorderRuter),len-1);
					#ifndef NO_COORDINATOR
					eJennicModuleStart();
					#endif
					SendACK();
				}
				break;
			case IPv6_PACKET:
				//memcpy(ipv6_buf, b + HEADER_SIZE + 1, len - 1);
				ipv6_len = len - 1;
				break;
			case COMMAND_ON:
				OnLamp();
				SendACK();
				break;
			case COMMAND_OFF:
				OffLamp();
				SendACK();
				break;
			case COMMAND_CLEAR_RAM:
				ClearRam();
				#ifndef NO_COORDINATOR
				eJennicModuleStart();
				#endif
				SendACK();
				break;
			case COMMAND_TIME_ON_OFF:
				memcpy(&(psTimers->sTimerOn1),ipv6_buf + HEADER_SIZE + 1,sizeof(tsTimerHourMinute));
				memcpy(&(psTimers->sTimerOff1),ipv6_buf + HEADER_SIZE + 1 + sizeof(tsTimerHourMinute) ,sizeof(tsTimerHourMinute));
				SendACK();
				break;
			case GET_STATUS_ROUTER:
				ipv6_buf[0] = (sizeof(tsRouterStatus) + 1) >> 8;
				ipv6_buf[1] = (sizeof(tsRouterStatus) + 1) & 0xFF;
				ipv6_buf[2] = VERSION;
				ipv6_buf[3] = SEND_STATUS_ROUTER;
				sRouterStatus.u8JenniceModuleState = eModuleState;
				sRouterStatus.u8Inputs = 0;
				sRouterStatus.u8Outputs = 0;
				#ifndef WIN32
				if( (PORT_INPUTS.IN & INP1) == 0) sRouterStatus.u8Inputs |= 0x01;
				if( (PORT_INPUTS.IN & INP2) == 0) sRouterStatus.u8Inputs |= 0x02;
				if( (PORT_INPUTS.IN & INP3) == 0) sRouterStatus.u8Inputs |= 0x04;
				if( (PORT_INPUTS.IN & INP4) == 0) sRouterStatus.u8Inputs |= 0x08;
				if( PORT_RELAY.IN & RELAY1 ) sRouterStatus.u8Outputs |= 0x01;
				if( PORT_RELAY.IN & RELAY2 ) sRouterStatus.u8Outputs |= 0x02;
				if( PORT_RELAY.IN & RELAY3 ) sRouterStatus.u8Outputs |= 0x04;
				if( PORT_RELAY.IN & RELAY4 ) sRouterStatus.u8Outputs |= 0x08;
				#else //WIN32
				if( on_relay )	sRouterStatus.u8Outputs |= 0x01;
				#endif	//WIN32
				memcpy(ipv6_buf + HEADER_SIZE + 1, (unsigned char*)&sRouterStatus, sizeof(tsRouterStatus));
				SendPacage(HEADER_SIZE + 1 + sizeof(tsRouterStatus));
				break;
			case COMMAND_READ_CURRENT_ENERGY:
				printf("\n\r%d %d %d\n\r\n\r",
					sizeof(float),sizeof(double),sizeof(float));	
				ipv6_buf[0] = (sizeof(tsCurrentEnergy) + 1) >> 8;
				ipv6_buf[1] = (sizeof(tsCurrentEnergy) + 1) & 0xFF;
				ipv6_buf[2] = VERSION;
				ipv6_buf[3] = COMMAND_READ_CURRENT_ENERGY;
				memcpy(ipv6_buf + HEADER_SIZE + 1, (unsigned char*)&sCurrentEnergy, sizeof(tsCurrentEnergy));
				SendPacage(HEADER_SIZE + 1 + sizeof(tsCurrentEnergy));
				break;
			case COMMAND_READ_TOTAL_ENERGY:
				ipv6_buf[0] = (sizeof(tsTotalEnergy) + 1) >> 8;
				ipv6_buf[1] = (sizeof(tsTotalEnergy) + 1) & 0xFF;
				ipv6_buf[2] = VERSION;
				ipv6_buf[3] = COMMAND_READ_TOTAL_ENERGY;
				memcpy(ipv6_buf + HEADER_SIZE + 1, (unsigned char*)&sTotalEnergy, sizeof(tsTotalEnergy));
				SendPacage(HEADER_SIZE + 1 + sizeof(tsTotalEnergy));
				break;
			case COOMAND_GET_CURRENT_ENERGY_ARRAY:
				{
					static uint16_t i_array;
					static char flag_arrray;
					static uint16_t NextInArray;
					uint16_t u16EntryArrayCount;
					int cou=0;

					tsSendEnergyArray * psSendEnergyArray = (tsSendEnergyArray*)(ipv6_buf + HEADER_SIZE + 1);
					tsCurrentEnergySmall * psCurrentEnergySmall = (tsCurrentEnergySmall*)(psSendEnergyArray + 1);

					if( ntohs(psSendEnergyArray->u16FirstArrayEntry) == 0 ){	//Init send array
					/*	printf("INIT %02X-%02X-%02X %02X:%02X:%02X     %02X-%02X-%02X %02X:%02X:%02X \n\r",
							psSendEnergyArray->reversDateTimeStart.date_time[0],psSendEnergyArray->reversDateTimeStart.date_time[1],psSendEnergyArray->reversDateTimeStart.date_time[2],
							psSendEnergyArray->reversDateTimeStart.date_time[3],psSendEnergyArray->reversDateTimeStart.date_time[4],psSendEnergyArray->reversDateTimeStart.date_time[5],
							psSendEnergyArray->reversDateTimeEnd.date_time[0],psSendEnergyArray->reversDateTimeEnd.date_time[1],psSendEnergyArray->reversDateTimeEnd.date_time[2],
							psSendEnergyArray->reversDateTimeEnd.date_time[3],psSendEnergyArray->reversDateTimeEnd.date_time[4],psSendEnergyArray->reversDateTimeEnd.date_time[5]);
							*/
						NextInArray = sCurrenEnegryArray.NextInArray;
						if( memcmp(&(sCurrenEnegryArray.sCurrentEnergySmall[NextInArray].DateTime),&DateTimeCleared,sizeof(tsDateTime)) != 0){
							flag_arrray = 1;
							i_array = NextInArray;
						}else{
							flag_arrray = 0;
							i_array = 0;
						}
					}
					u16EntryArrayCount = ntohs(psSendEnergyArray->u16EntryArrayCount);
					//printf("i %d, flag %d\n\r",i_array,flag_arrray);
					for( ; ( i_array < NextInArray ) || flag_arrray ; ){
						unsigned char reverse_time[6];
						reverse_time[0] = sCurrenEnegryArray.sCurrentEnergySmall[i_array].DateTime.date_time[5];
						reverse_time[1] = sCurrenEnegryArray.sCurrentEnergySmall[i_array].DateTime.date_time[4];
						reverse_time[2] = sCurrenEnegryArray.sCurrentEnergySmall[i_array].DateTime.date_time[3];
						reverse_time[3] = sCurrenEnegryArray.sCurrentEnergySmall[i_array].DateTime.date_time[2];
						reverse_time[4] = sCurrenEnegryArray.sCurrentEnergySmall[i_array].DateTime.date_time[1];
						reverse_time[5] = sCurrenEnegryArray.sCurrentEnergySmall[i_array].DateTime.date_time[0];
						/*printf("Current %02X-%02X-%02X %02X:%02X:%02X %d %d %d %d\n\r",
							reverse_time[0],reverse_time[1],reverse_time[2],
							reverse_time[3],reverse_time[4],reverse_time[5],
							sizeof(float),sizeof(tsSendEnergyArray),sizeof(tsCurrentEnergySmall),sizeof(tsCurrenEnegryArray));			*/			
						if( memcmp( reverse_time , &(psSendEnergyArray->reversDateTimeStart) , sizeof(tsDateTime)) < 0 )
							goto no_add_data;
						if( memcmp( reverse_time , &(psSendEnergyArray->reversDateTimeEnd) , sizeof(tsDateTime)) > 0 )
							goto no_add_data;	
						{	
							u16EntryArrayCount--;
							cou++;
							memcpy(psCurrentEnergySmall, &(sCurrenEnegryArray.sCurrentEnergySmall[i_array]), sizeof(tsCurrentEnergySmall));
							psCurrentEnergySmall++;
						}
				no_add_data:
						if( ++i_array >= MAX_CURRENT_ENERGY ){
							i_array = 0;
							flag_arrray = 0;
						}
						if( u16EntryArrayCount == 0)
							break;
					}
					psSendEnergyArray->u16EntryArrayCount = htons(cou);
					if (i_array == NextInArray){
						psSendEnergyArray->u8FlagEnd = 1;
						//printf("END\n\r");
					}

					ipv6_buf[0] = (sizeof(tsSendEnergyArray) + (sizeof(tsCurrentEnergySmall)*cou) + 1) >> 8;
					ipv6_buf[1] = (sizeof(tsSendEnergyArray) + (sizeof(tsCurrentEnergySmall)*cou) + 1) & 0xFF;
					ipv6_buf[2] = VERSION;
					ipv6_buf[3] = COOMAND_GET_CURRENT_ENERGY_ARRAY;
					SendPacage(HEADER_SIZE + 1 + sizeof(tsSendEnergyArray) + (sizeof(tsCurrentEnergySmall)*cou));
				}
				break;
			default:
				daemon_log(LOG_ERR, "Error unknow teCommandsPC");
				//Send NACK ???
				break;
			}
		}else{
			daemon_log(LOG_DEBUG, "BAD lenght from client:%d %d",len + HEADER_SIZE, butes_reciv>>1);
		}
		butes_reciv = 0;
	}
					
					
					
					
					
  
	len = 0;//read(tun_fd, buf, sizeof(buf));
  if (ipv6_len > 0)
    {	
		len = ipv6_len;
		ipv6_len = 0;
        // If there's data waiting for us on the TUN device, write it to the Jennic chip.
        //printf("Data from TUN: %d bytes\n", len);
        
        //for (i = 0; i < len; i++)
        //    printf("%x ", buf[i] & 0x000000FF);
        //printf("\n");
        
        // Send data to Jennic chip
        if (eJennicModuleWriteIPv6(len, ipv6_buf+HEADER_SIZE+1) != E_MODULE_OK)
        {
            daemon_log(LOG_ERR, "Error writing packet to module");
            return E_TUN_ERROR;
        }
    }
    return E_TUN_OK;
}


teTunStatus eTunDeviceWritePacket(uint32_t u32Length, uint8_t *pu8Data)
{
	ipv6_buf[0] = (u32Length + 1) >> 8;
	ipv6_buf[1] = (u32Length + 1) & 0xFF;
	ipv6_buf[2] = VERSION;
	ipv6_buf[3] = IPv6_PACKET;
	memcpy( ipv6_buf + HEADER_SIZE + 1, pu8Data, u32Length);
	SendPacage(u32Length + HEADER_SIZE + 1);

	return E_TUN_OK;
    
    return E_TUN_ERROR;
}



