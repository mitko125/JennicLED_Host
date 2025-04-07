#include <windows.h>
#include <process.h>
#include <fcntl.h>
#include <sys/types.h>
#include <errno.h>
#include <time.h>
#include <stdint.h>
#include <conio.h>
#include <sys/stat.h>
#include <signal.h>
#include "Serial.h"
#include <winsock.h>
//#include <winsock2.h>
//#include <afxdisp.h>
#include "windows_sub.h"

#include "def.h"
#include "log.h"
#include "sub.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "TunDevice.h"
#include "JennicModule.h"


unsigned char * p_E_RAM = NULL;


void InitHardware(void) {
	//Init

	p_E_RAM = malloc(SIZE_RAM);

	if (p_E_RAM == NULL) {
		daemon_log(LOG_EMERG, "Err create Data Memory");
		while (1);
	}
	
	FILE *fp;
	fp = fopen("backup.bin", "rb");
	if (fp == NULL ) {
		daemon_log(LOG_EMERG, "Not backub.bin file, clear RAM");
		ClearRam();
	} else {
		uint16_t i = (uint16_t)fread(p_E_RAM, sizeof(unsigned char), SIZE_RAM , fp);
		if (i != SIZE_RAM) {
			daemon_log(LOG_ERR, "Bad lenght %u backub.bin file, clear RAM", i);
			ClearRam();
		}
		fclose(fp);
	}
	ENERGY_METER_init();
}

void save_RAM(void)
{
	FILE *fp;
	fp = fopen("backup.bin", "wb");
	if (fp != NULL) {
		fwrite(p_E_RAM, sizeof(unsigned char), SIZE_RAM, fp);
		fclose(fp);
	}else{
		daemon_log(LOG_ERR, "Error write backub.bin file");
	}
		
}

#undef FAST_GET_TIME

#ifdef FAST_GET_TIME
static uint16_t INT_sim_hour = 0, INT_sim_minute = 0, old_sec = 100;
void first_get_time(void)
#else
void get_time(void)
#endif
{
	SYSTEMTIME lpSystemTime;
	GetLocalTime(&lpSystemTime);	//GetSystemTime - по Гринуич  GetLocalTime - местно време
	date_time[5] = INT_BCD((lpSystemTime.wYear % 100));
	date_time[4] = INT_BCD(lpSystemTime.wMonth);
	date_time[3] = INT_BCD(lpSystemTime.wDay);
	date_time[2] = INT_BCD(lpSystemTime.wHour);// +3));
	date_time[1] = INT_BCD(lpSystemTime.wMinute);
	date_time[0] = INT_BCD(lpSystemTime.wSecond);
#ifdef FAST_GET_TIME
	INT_sim_hour = lpSystemTime.wHour;
	INT_sim_minute = lpSystemTime.wMinute;
	old_sec = lpSystemTime.wSecond;
/*	printf_P(PSTR("\n\rTime %02d:%02d  "),INT_sim_hour, INT_sim_minute);
	PrintDateTime(date_time);
	printf_P(PSTR("\n\r"));*/
#endif
}

#ifdef FAST_GET_TIME
static uint8_t fl_first_get_time = 1;
void get_time(void)
{
	if (fl_first_get_time) {
		fl_first_get_time = 0;
		first_get_time();
	}
	SYSTEMTIME lpSystemTime;
	GetLocalTime(&lpSystemTime);
	uint16_t sec = lpSystemTime.wSecond;
	if( old_sec != sec ){
		old_sec = sec;
		if( ++INT_sim_minute  >= 60 ){
			INT_sim_minute = 0;
			if( ++INT_sim_hour >= 24 ){
				INT_sim_hour = 0;
			}
		}
		//printf("%02d:%02d\n\r", INT_sim_hour, INT_sim_minute);
		date_time[2] = INT_BCD(INT_sim_hour);
		date_time[1] = INT_BCD(INT_sim_minute);
		date_time[0] = INT_BCD(sec);
	}
}
#endif

void ResetSIM(void){
	if (verbosity >= LOG_DEBUG) {
		printf_P(PSTR("\n\rReset SIM in Time: "));
		PrintDateTime(date_time);
		printf_P(PSTR("\n\r"));
	}
}

void SetDateTime(void){
	error_clock = 0;
}

static time_t  sLastLoop = 0; 

void ENERGY_METER_init(void){

	get_time();

	sLastLoop = time(NULL);
	
	memcpy(&(sCurrentEnergy.sLastContact), date_time, sizeof(tsDateTime));
		
	sCurrentEnergy.L1_Voltage = (float)220.1;
	sCurrentEnergy.L2_Voltage = (float)220.2;
	sCurrentEnergy.L3_Voltage = (float)220.3;
	
	sCurrentEnergy.Grid_frequency = (float)50.0;
	
	sCurrentEnergy.L1_Current = (float)1.1;
	sCurrentEnergy.L2_Current = (float)1.2;
	sCurrentEnergy.L3_Current = (float)1.3;
	
	sCurrentEnergy.Active_power = (float)2.0;
	sCurrentEnergy.L1_Active_power = (float)2.1;
	sCurrentEnergy.L2_Active_power = (float)2.2;
	sCurrentEnergy.L3_Active_power = (float)2.3;
	
	sCurrentEnergy.Reactive_power = (float)3;
	sCurrentEnergy.L1_Reactive_power = (float)3.1;
	sCurrentEnergy.L2_Reactive_power = (float)3.2;
	sCurrentEnergy.L3_Reactive_power = (float)3.3;
	
	sCurrentEnergy.Apparent_power = (float)4;
	sCurrentEnergy.L1_Apparent_power = (float)4.1;
	sCurrentEnergy.L2_Apparent_power = (float)4.2;
	sCurrentEnergy.L3_Apparent_power = (float)4.3;
	
	sCurrentEnergy.Power_factor = (float)1.0;
	sCurrentEnergy.L1_Power_factor = (float)0.91;
	sCurrentEnergy.L2_Power_factor = (float)0.92;
	sCurrentEnergy.L3_Power_factor = (float)0.93;
	
	
	memcpy(&(sTotalEnergy.sLastContact), date_time, sizeof(tsDateTime));
		
	sTotalEnergy.Total_active_energy = (float)6.0;
	sTotalEnergy.T1_Total_active_energy = (float)4.0;
	sTotalEnergy.T2_Total_active_energy = (float)2.0;
		
	sTotalEnergy.L1_Total_active_energy = (float)1.0;
	sTotalEnergy.L2_Total_active_energy = (float)2.0;
	sTotalEnergy.L3_Total_active_energy = (float)3.0;
		
		
	sTotalEnergy.Total_reactive_energy = (float)0.6;
	sTotalEnergy.T1_Total_reactive_energy = (float)0.4;
	sTotalEnergy.T2_Total_reactive_energy = (float)0.2;
		
	sTotalEnergy.L1_Total_reactive_energy = (float)0.1;
	sTotalEnergy.L2_Total_reactive_energy = (float)0.2;
	sTotalEnergy.L3_Total_reactive_energy = (float)0.3;
}

void ENERGY_METER_Loop(void){
	if (difftime(time(NULL), sLastLoop) > 4){
		sLastLoop = time(NULL);
		if( psModuleSetConfig->u8EnableEnergyMeter ){
			memcpy(&(sCurrentEnergy.sLastContact), date_time, sizeof(tsDateTime));
			sCurrentEnergy.Active_power += (float)0.1;
			AddCurrentEnergyInArray();
		}
	}
}



// макрос для печати количества активных пользователей
#define PRINTNUSERS if (nclients) {daemon_log(LOG_DEBUG,"%d user on-line\n", nclients);} \
        else {daemon_log(LOG_DEBUG,"No User on line\n");}
// глобальная переменная - количество активных пользователей
volatile int nclients = 0;
volatile int last_clients = 0;

SOCKET my_sock[MAX_TCP_IP4_CLIENTS];

static volatile unsigned char thread_ok = 0;
// прототип функции, обслуживающий подключившихся пользователей
int SexToClient(int * client);

int MyThread(void *p) {
	{
		WSADATA WSAData;
		
		// Initialize winsock dll
		if(WSAStartup(MAKEWORD(1, 0), &WSAData))//if (::WSAStartup(MAKEWORD(1, 0), &WSAData))
		{
			// Error handling
			daemon_log(LOG_ERR, "Error WSAStartup");
		}
		
		// Get local host name
		char szHostName[128] = "";
		
		if(gethostname(szHostName, sizeof(szHostName))) //if(::gethostname(szHostName, sizeof(szHostName)))
		{
			// Error handling -> call 'WSAGetLastError()'
			daemon_log(LOG_ERR, "Error gethostname");
		}
		
		// Get local IP addresses
		struct sockaddr_in SocketAddress;
		struct hostent     *pHost        = 0;
		
		pHost = gethostbyname(szHostName);//::gethostbyname(szHostName);
		if(!pHost)
		{
			// Error handling -> call 'WSAGetLastError()'
			daemon_log(LOG_ERR, "Error gethostbyname");
		}
		
		char aszIPAddresses[16]; // maximum of ten IP addresses
		
		int iCnt;
		for(iCnt = 0; ((pHost->h_addr_list[iCnt]) && (iCnt < 10)); ++iCnt)
		{
			memcpy(&SocketAddress.sin_addr, pHost->h_addr_list[iCnt], pHost->h_length);
			strcpy(aszIPAddresses, inet_ntoa(SocketAddress.sin_addr));
			daemon_log(LOG_DEBUG, "My IP %d:%s",iCnt,aszIPAddresses);
		}
		
		// Cleanup
		WSACleanup();
	}
	
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
	// размер очереди - MAX_TCP_IP4_CLIENTS
	if (listen(mysocket, MAX_TCP_IP4_CLIENTS))
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
		if( nclients < MAX_TCP_IP4_CLIENTS ){
			my_sock[last_clients = nclients] = client_socket;
			nclients++; // увеличиваем счетчик подключившихся клиентов

					// пытаемся получить имя хоста
			HOSTENT *hst;
			hst = gethostbyaddr((char *)&client_addr.sin_addr.s_addr, 4, AF_INET);

			// вывод сведений о клиенте
			Clients_sIP4addres[last_clients].S_un.S_addr = my_inet_addr(inet_ntoa(client_addr.sin_addr));
			
			daemon_log(LOG_DEBUG,"+%s [%s] new connect!\n",
				(hst) ? hst->h_name : "", inet_ntoa(client_addr.sin_addr));
			PRINTNUSERS

			// Вызов нового потока для обслужвания клиента
			// Да, для этого рекомендуется использовать _beginthreadex
			// но, поскольку никаких вызовов функций стандартной Си библиотеки
			// поток не делает, можно обойтись и CreateThread

			_beginthread(SexToClient, 0, &last_clients);
		}else{
			// закрываем сокет
			closesocket(client_socket);
			daemon_log(LOG_ERR, "Too much TCP clients > %d\n", MAX_TCP_IP4_CLIENTS);
		}
	}
	return 0;
}



 






// Эта функция создается в отдельном потоке
// и обсуживает очередного подключившегося клиента независимо от остальных
int SexToClient(int * client)
{
	int num_client = *client;

	// цикл эхо-сервера: прием строки от клиента и возвращение ее клиенту
	int bytes_r = 0;
	while ((bytes_r = recv(my_sock[num_client], &ipv6_buf[0], SIZE_ipv6_buf , 0)) && bytes_r != SOCKET_ERROR) {
		//send(my_sock, &buff[0], bytes_recv, 0);
		ipv6_buf[bytes_r] = 0;
		daemon_log(LOG_DEBUG, "From client TCP/IP:%s", ipv6_buf);
		butes_reciv = bytes_r;
	}

	// если мы здесь, то произошел выход из цикла по причине
	// возращения функцией recv ошибки - соединение с клиентом разорвано
 	nclients--; // уменьшаем счетчик активных клиентов
	Clients_sIP4addres[num_client].S_un.S_addr = 0;

	daemon_log(LOG_DEBUG, "-disconnect\n"); 
	PRINTNUSERS

	// закрываем сокет
	closesocket(my_sock[num_client]);
	my_sock[num_client] = 0;
	return 0;
}

uint8_t StartWinMyThread(void)
{
	_beginthread(MyThread,0, NULL);
	while (thread_ok == 0);

  daemon_log(LOG_DEBUG, "Opened COM in tun device");

  return 0;	//>=0 O'K
}

void SendPacageToPC_Client(int len)
{
	last_clients = 7;
	SendPacage(len);
}

void SendPacage(int len)
{
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
}

void SimLoop(void){
	;
}


//Фиктивни от SIM900
uint8_t t_min_no_connect = 0;