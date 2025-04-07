
#ifdef WIN32

#include <windows.h>
#include <process.h>
#include <fcntl.h>
#include <sys/types.h>
#include <errno.h>
#include <time.h>

#endif	//WIN32


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "log.h"
#include "def.h"

#include "TunDevice.h"
#include "JennicModule.h"

#ifndef WIN32
#include "avr_compiler.h"
#include "GPRS_Uart.h"
#include "defs.h"
#endif

uint8_t key_a,key_b;

#define SIM_900


static volatile int ipv6_len = 0;
static unsigned char ipv6_buf[100];
char pin[5];

void main_loop(void);

#ifndef SIM_900

static volatile unsigned char thread_ok = 0;

 

// макрос для печати количества активных пользователей
#define PRINTNUSERS if (nclients) {daemon_log(LOG_DEBUG,"%d user on-line\n", nclients);} \
        else {daemon_log(LOG_DEBUG,"No User on line\n");}
// глобальная переменная - количество активных пользователей
int nclients = 0;

// прототип функции, обслуживающий подключившихся пользователей
int SexToClient(LPVOID client_socket);


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
	// размер очереди - 0x100
	if (listen(mysocket, 0x100))
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

		_beginthread(SexToClient, 0, &client_socket);
	}
	return 0;
}


// Эта функция создается в отдельном потоке
// и обсуживает очередного подключившегося клиента независимо от остальных
int SexToClient(LPVOID client_socket)
{
	SOCKET my_sock;
	my_sock = ((SOCKET *)client_socket)[0];
	unsigned char buff[20 * 1024];
#define sHELLO "SOCKET PODKLUCHEN\r\n"

	// отправляем клиенту приветствие
	//	send(my_sock, sHELLO, sizeof(sHELLO), 0);

	// цикл эхо-сервера: прием строки от клиента и возвращение ее клиенту
	int bytes_recv;
	while ((bytes_recv = recv(my_sock, &buff[0], sizeof(buff), 0)) && bytes_recv != SOCKET_ERROR) {
		//send(my_sock, &buff[0], bytes_recv, 0);
		if (bytes_recv > HEADER_SIZE) {
			for (int i = 0; i < bytes_recv; i++)
				    printf("%02X", buff[i] & 0x000000FF);
				printf("\n");
			int len = buff[0];
			len <<= 8;
			len |= buff[1];
			if ((len + HEADER_SIZE) == bytes_recv) {
				if (buff[HEADER_SIZE] == IPv6_PACKET) {
					memcpy(ipv6_buf, buff + HEADER_SIZE + 1, len - 1);
					ipv6_len = len - 1;
				}
			}
		}
	}

	// если мы здесь, то произошел выход из цикла по причине
	// возращения функцией recv ошибки - соединение с клиентом разорвано
	nclients--; // уменьшаем счетчик активных клиентов
	daemon_log(LOG_DEBUG, "-disconnect\n"); 
	PRINTNUSERS

		// закрываем сокет
		closesocket(my_sock);
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

void SimWrite(const unsigned char * text) {
	if (verbosity >= LOG_DEBUG)
		daemon_log(LOG_DEBUG, "To SIM:%s", text);
	while (*text)
		sim_serial_write(*text++);
}

typedef enum {
	AT_test,
	ATE0,
	CIFSR_test,
	PIN_test,
	PIN_set,
	CIPMUX,
	CREG,
	CGATT,
	CSQ,
	CSTT,
	CIICR,
	BAD_PIN,
	TO_Open,
	Opened,
}sim900state;
sim900state state = AT_test;
static long time_sec;
#define T_STATE_WAIT_SIM 4
static uint8_t fl_read = 0;
static int recived = 0;
static int max_recived;
static unsigned char buffer[500];
static unsigned int to_reciv = 0;
static unsigned int client;
static unsigned char buffer_to_send[100];
static unsigned int to_send = 0;
void SimRead(unsigned char *pBuf, int lenght) {
	time_sec = (long)time(NULL);
	fl_read = 1;
	recived = 0;
	max_recived = lenght;
}

uint16_t sim_errors = 0;
uint8_t t_min_no_connect = 0;
#define MAX_TIME_NO_CONNECT 6
#define TIME_TEST_CONNECT (MAX_TIME_NO_CONNECT-1)
static uint8_t fl_test_connect = 0;

void On_off_SIM(void){

	daemon_log(LOG_DEBUG, "On/Off SIM");
	fl_test_connect = 0;
	t_min_no_connect = 0;
	
#ifndef WIN32
	PORT_GPRS_RST.OUTSET = GPRS_RST;
	
	time_sec = (long)time(NULL);
	while((time(NULL) - time_sec) < 2)
		main_loop();
	time_sec = (long)time(NULL);
	if(time_sec > 10)
		time_sec-=10;
	else
		time_sec = 0;
	
	PORT_GPRS_RST.OUTCLR = GPRS_RST;
			
#endif	//WIN32
		
}

void LoopRead(void) {
	if( t_min_no_connect >= MAX_TIME_NO_CONNECT ){
		state = AT_test;
		On_off_SIM();
		sim_errors ++;
		return;
	} 
	
	if ((time(NULL) - time_sec) >= T_STATE_WAIT_SIM) {
		if (state == Opened) {
			time_sec = (long)time(NULL);
			return;
		}
		fl_read = 0;
		if ((recived == 0) && (state < Opened)){
			state = AT_test;
			On_off_SIM();
			sim_errors ++;
		}else if (recived) {
			buffer[recived] = 0;
			if (verbosity >= LOG_DEBUG)
				daemon_log(LOG_DEBUG, "From SIM:%s", buffer);
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
				}
				else {
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
				}else 
					state = BAD_PIN;
				break;
			}
			case PIN_set:
			{
				if (recived == 6) {
					if (memcmp(buffer, "\r\nOK\r\n", recived) == 0) {
						state = PIN_test;
						break;
					}
				}else 
					state = BAD_PIN;
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
			buffer[recived] = u8Data;
			recived++;
			if (state == Opened) {
				if (recived && (recived == to_reciv)) {
					unsigned char b[100];
					buffer[recived] = 0;
					if (verbosity >= LOG_DEBUG)
						daemon_log(LOG_DEBUG, "From client SIM:%s", buffer);
					int i;
					for (i = 0;i < (to_reciv >> 1); i++) {
						unsigned char data;
						if (buffer[i << 1] >= 'A')
							data = buffer[i << 1] - 'A' + 10;
						else
							data = buffer[i << 1] - '0';
						data <<= 4;
						if (buffer[(i << 1)+1] >= 'A')
							data |= (buffer[(i << 1)+1] - 'A' + 10)&0x0F;
						else
							data |= (buffer[(i << 1)+1] - '0')&0x0F;
						b[i] = data;
					}
					int len = b[0];
					len <<= 8;
					len |= b[1];
					if ((len + HEADER_SIZE) == (to_reciv >> 1)) {
						switch(b[HEADER_SIZE]){
						case COMMAND_SET_HOST_DATA:
							break;
						case IPv6_PACKET:
							memcpy(ipv6_buf, b + HEADER_SIZE + 1, len - 1);
							ipv6_len = len - 1;
							break;
						case COMMAND_ON:
							OnLamp();
							break;
						case COMMAND_OFF:
							OffLamp();
							break;
						case COMMAND_TIME_ON_OFF:
							memcpy(psTimerOn,b + HEADER_SIZE + 1,sizeof(tsTimerHourMinute));
							memcpy(psTimerOff,b + HEADER_SIZE + 1 + sizeof(tsTimerHourMinute) ,sizeof(tsTimerHourMinute));
							break;
						case GET_STATUS_SIM_ERR:
							buffer_to_send[0] = 0;
							buffer_to_send[1] = 5;
							buffer_to_send[2] = VERSION;
							buffer_to_send[3] = SIM_ERRORS;
							buffer_to_send[4] = sim_errors>>8;
							buffer_to_send[5] = sim_errors;
							buffer_to_send[6] = (*on_counters)>>8;
							buffer_to_send[7] = *on_counters;
							to_send = buffer_to_send[1] + 3;
							sprintf((char*)b,"AT+CIPSEND=%d\r\n",client);
							SimWrite((unsigned char*)b);
							break;
						}
					}else{
						daemon_log(LOG_DEBUG, "BAD lenght from client:%d %d",len + HEADER_SIZE, to_reciv>>1);
					}
					to_reciv = 0;
					recived = 0;
				}
				switch (u8Data) {
				case '>':
					if( to_send ){
						int i;
						uint8_t data;
						for( i = 0 ; i< to_send ; i++ ){
							data = buffer_to_send[i];
							data >>= 4; data &= 0xF;
							if( data < 10 ) data += '0';	else	data += 'A' -10;
							sim_serial_write(data);
							
							data = buffer_to_send[i];
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
						daemon_log(LOG_DEBUG, "From SIM:%s", buffer);
					if ( (recived > 12) && (memcmp(buffer, "+RECEIVE,", 9) == 0 )) {
					
						t_min_no_connect = 0;
						fl_test_connect = 0;
						
						to_reciv = atoi((char*)(buffer + 11));
						client = atoi((char*)(buffer + 9));
						daemon_log(LOG_DEBUG, "Client % d to recived SIM:%d", client,to_reciv);
					}else if( (recived > 11) && (memcmp(buffer, "+PDP: DEACT", 11) == 0 )){
						state = AT_test;
						On_off_SIM();
						sim_errors ++;
					}else if( (recived > 13) && (memcmp(buffer, "+CIPSERVER: 0", 13) == 0 )){
						state = AT_test;
						On_off_SIM();
						sim_errors ++;
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

void SimLoop(void) {

	if (state < Opened) {
/*		pHC->fl_ready_to_open = false;
		pHC->fl_opened = false;
		pHC->fl_open = false;
		pHC->fl_close = false;
		pHC->fl_to_write = false;
		pHC->fl_write = false;*/
	}
	if (fl_read){
		if(key_a){
			key_a = 0;
			SimWrite((unsigned char*)"AT+CIPSERVER?\r\n");
			//SimRead(buffer, sizeof(buffer));
		}
		if(key_b){
			key_b = 0;
			SimWrite((unsigned char*)"AT+CIPSTATUS\r\n");
			//SimRead(buffer, sizeof(buffer));
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
	}else {
		switch (state) {
		case AT_test:
		{
			SimWrite((unsigned char*)"AT\r\n");
			SimRead(buffer, sizeof(buffer));
		}
		break;
		case ATE0:
		{
			SimWrite((unsigned char*)"ATE0\r\n");
			SimRead(buffer, sizeof(buffer));
		}
		break;
		case CIFSR_test:
		{
			SimWrite((unsigned char*)"AT+CIFSR\r\n");
			SimRead(buffer, sizeof(buffer));
		}
		break;
		case PIN_test:
		{
			SimWrite((unsigned char*)"AT+CPIN?\r\n");
			SimRead(buffer, sizeof(buffer));
		}
		break;
		case PIN_set:
		{
			char text[50];
			int old_ver = verbosity;
			verbosity = LOG_WARNING;
			#ifdef WIN32
			sprintf_s(text,sizeof(text), "AT+CPIN=\"%s\"\r\n", PIN);
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
			SimWrite((unsigned char*)"AT+CIPMUX=1\r\n");
			SimRead(buffer, sizeof(buffer));
		}
		break;
		case CREG:
		{
			SimWrite((unsigned char*)"AT+CREG?\r\n");
			SimRead(buffer, sizeof(buffer));
		}
		break;
		case CGATT:
		{
			SimWrite((unsigned char*)"AT+CGATT?\r\n");
			SimRead(buffer, sizeof(buffer));			
		}
		break;
		case CSQ:
		{
			SimWrite((unsigned char*)"AT+CSQ\r\n");
			SimRead(buffer, sizeof(buffer));
		}
		break;
		case CSTT:
		{
			SimWrite((unsigned char*)"AT+CSTT=\"tvulosv\"\r\n");
			SimRead(buffer, sizeof(buffer));
		}
		break;
		case CIICR:
		{
			SimWrite((unsigned char*)"AT+CIICR\r\n");
			SimRead(buffer, sizeof(buffer));
		}
		break;
		case TO_Open:
		{
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
		default:
			break;
		}
	}
}

#endif //SIM_900

void TunLoop(void) {
#ifdef SIM_900
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


teTunStatus eTunDeviceReadPacket(void)
{
//    unsigned char buf[2048];
    int len;
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
        if (eJennicModuleWriteIPv6(len, ipv6_buf) != E_MODULE_OK)
        {
            daemon_log(LOG_ERR, "Error writing packet to module");
            return E_TUN_ERROR;
        }
    }
    return E_TUN_OK;
}


teTunStatus eTunDeviceWritePacket(uint32_t u32Length, uint8_t *pu8Data)
{
    int len;

	len = 0;//write(tun_fd, pu8Data, u32Length);
    if (len == u32Length)
    {
        //printf("Data to TUN: %d bytes (%d)\n", len, psMsg->u16Length);
        return E_TUN_OK;
    }
    return E_TUN_ERROR;
}



