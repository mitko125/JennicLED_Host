#include "avr_compiler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "log.h"
#include "def.h"
#include "sub.h"
#include "TunDevice.h"
#include "JennicModule.h"
#include "GPRS_Uart.h"
#include "hardware.h"
#include "SIM900.h"


uint8_t t_min_no_connect = 0;
uint8_t client_number;
uint8_t data_to_server = 0;
#define MAX_TIME_NO_CONNECT 6
#define TIME_TEST_CONNECT (MAX_TIME_NO_CONNECT-1)

#define T_WAIT_PACKET	4000	//ms
#define T_WAIT_BUTE 300	//ms

char text_to_CIPSTART[20];

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

static sim900state state = AT_test;
static uint8_t fl_read = 0;	//при state = Opened е = 1 и прескача автомата за стартиране на SIM,
	// вдига се в 1 при SimRead и пада в 0 time_sleep_SIM ако не е Opened
static uint8_t no_SimLoop = 0;
volatile uint16_t time_sleep_SIM = T_WAIT_PACKET;
uint8_t key_a,key_b,key_c;
static int recived = 0;
static unsigned int to_reciv = 0;
#define MAX_RECIV 200
static unsigned char buffer_SIM900[MAX_RECIV];
static unsigned int to_send = 0;
static uint8_t fl_test_connect = 0;
static uint8_t fl_reject_resetSIM = 0;
static uint8_t to_close_client7 = 0;

static void SleepSIM(uint16_t sleep_time);
static void SimRead(void);
static void SimWrite(const unsigned char * text);
static void On_off_SIM(void);

void LoopRead(void) {
	if( t_min_no_connect >= MAX_TIME_NO_CONNECT ){	//ако 6 минути не е имало връзка със SIM ресет на сима
		On_off_SIM();
		return;
	} 
	
	if ( ( time_sleep_SIM == 0 ) ){		//край на приемането на данни
		if ( state == Opened ) {				//прескачаме обработката на прочетеното от SIM от крайния автомат за запускане на SIM (симът е запуснат)
			cli();	//__disable_interrupt();
			time_sleep_SIM = T_WAIT_PACKET;
			sei();	//__enable_interrupt();
			return;
		}
		fl_read = 0;
		if ( (recived == 0) && (state < Opened) ){	//ако 4 секунди не е дошло нищо от SIM при запускането му, го ресетваме
			On_off_SIM();
		}else if (recived) {						// обработва прочетеното от SIM по време на работа на крайния автомат за запускането му
			buffer_SIM900[recived] = 0;
			daemon_log(LOG_DEBUG, "From SIM C <% 3d>:%s",recived,buffer_SIM900);	//state no Opened
			switch (state) {
			case AT_test:
			{
				if (recived == 10) {
					if (memcmp(buffer_SIM900, "AT\r\n\r\nOK\r\n", recived) == 0)
						state = ATE0;
				}else if (recived == 6) {
					if (memcmp(buffer_SIM900, "\r\nOK\r\n", recived) == 0)
						state = CIFSR_test;
				}
				break;
			}
			case ATE0:
			{
				if (recived == 12) {
					if (memcmp(buffer_SIM900, "ATE0\r\n\r\nOK\r\n", recived) == 0)
						state = CIFSR_test;
				}else if (recived == 6) {
					if (memcmp(buffer_SIM900, "\r\nOK\r\n", recived) == 0)
						state = CIFSR_test;
				}
				break;
			}
			case CIFSR_test:
			{
				if (recived >10) {
					My_sIP4addres.S_un.S_addr = my_inet_addr(buffer_SIM900);
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
					if (memcmp(buffer_SIM900, "\r\n+CPIN: SIM PIN\r\n\r\nOK\r\n", recived) == 0)
						state = PIN_set;
				}else if (recived == 22) {
					if (memcmp(buffer_SIM900, "\r\n+CPIN: READY\r\n\r\nOK\r\n", recived) == 0)
						state = CIPMUX;
				}else{ 
					state = BAD_PIN;
					daemon_log(LOG_CRIT,"Error PIN test %s",buffer_SIM900);
				}
				break;
			}
			case PIN_set:
			{
				if (recived == 6) {
					if (memcmp(buffer_SIM900, "\r\nOK\r\n", recived) == 0) {
						state = PIN_test;
						break;
					}
				}else{
					state = BAD_PIN;
					daemon_log(LOG_CRIT,"Error SET PIN %s",buffer_SIM900);
				}
				break;
			}
			case CIPMUX:
			{
				if (recived > 5) {
					state = CREG;
				}
				break;
			}
			case CREG:
			{
				if (recived == 20) {
					if (memcmp(buffer_SIM900, "\r\n+CREG: 0,1\r\n\r\nOK\r\n", recived) == 0)
						state = CGATT;
				}
				break;
			}
			case CGATT:
			{
				if (recived == 19) {
					if (memcmp(buffer_SIM900, "\r\n+CGATT: 1\r\n\r\nOK\r\n", recived) == 0)
						state = CSQ;
				}
				break;
			}
			case CSQ:
			{
				if (recived > 10) {
					sRouterStatus.u8CSQ = atoi((char*)(buffer_SIM900 + 7));
					state = CSTT;
				}
				break;
			}
			case CSTT:
			{
				if (recived > 5) {
					state = CIICR;
				}
				break;
			}
			case CIICR:
			{
				if (recived > 5) {
					state = CIFSR_test;
				}
				break;
			}
			case TO_Open:
			{
				if( ( recived == 19 ) && ( memcmp(buffer_SIM900, "\r\nOK\r\n\r\nSERVER OK\r\n", recived) == 0 ) ) {
					uint8_t i;
					for( i = 0 ; i < MAX_TCP_IP4_CLIENTS ; i++ )
						Clients_sIP4addres[i].S_un.S_addr = 0;
					state = Opened;
					t_min_no_connect = 0;
					daemon_log(LOG_DEBUG, "Server is opened");
				}else if( ( recived == 9 ) && ( memcmp(buffer_SIM900, "\r\nERROR\r\n", recived) == 0 ) ) {
					uint8_t i;
					for( i = 0 ; i < MAX_TCP_IP4_CLIENTS ; i++ )
						Clients_sIP4addres[i].S_un.S_addr = 0;
					state = Opened;
					t_min_no_connect = 0;
					daemon_log(LOG_DEBUG, "Server ALREADY OPEN");
					SimWrite((unsigned char*)"AT+CIPSTATUS\r\n");
					SimRead();
				}
				break;
			}
			case BAD_PIN:
			case Opened:
				break;
			}
		}
	}	else {	//четене от SIM
		uint8_t u8Data;
		while (sim_serial_read(&u8Data)) {	//чете докато има данни
			if( to_reciv )
				ipv6_buf[recived] = u8Data;
			if( recived < MAX_RECIV )
				buffer_SIM900[recived] = u8Data;
			recived++;
			cli();	//__disable_interrupt();
			time_sleep_SIM = T_WAIT_BUTE;			//отрязва дългото (4 секунди) чакане на данни от SIM и прави очакването на 300ms за пореден символ
			sei();	//__enable_interrupt();
			if (state == Opened) {						//обработват се данни от SIM само ако е запуснат, иначе се оставят за крайния автомат за запускането му
				if (recived && (recived == to_reciv)) {	//взима се данните от TCP пакет приет от SIM
					ipv6_buf[recived] = 0;
					daemon_log(LOG_DEBUG, "From TCP client SIM<% 3d>:%s",recived, ipv6_buf);
					butes_reciv = 	to_reciv;		//казва на TunDevice че има пакет за обработка
					memcpy(&(sRouterStatus.sDateTimeLastClient), date_time, sizeof(tsDateTime));
					fl_reject_resetSIM = 1;
	
					to_reciv = 0;
					recived = 0;
				}
				switch (u8Data) {
				case '>':
					if( to_send ){					//изпраща се TCP пакет през SIM
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
						daemon_log(LOG_DEBUG, "To TCP client %d sended %d * 2 bytes", client_number,to_send);
						to_send=0;
					}
					break;
				case '+':			//започва се нов пакет обикновено с "+RECEIVE" започва приемането на нов TCP пакет от SIM
					buffer_SIM900[0] = u8Data;
					recived = 1;
					break;
				case 0x0A:		//край на приет пакет от SIM
					{
						buffer_SIM900[recived] = 0;
						daemon_log(LOG_DEBUG, "From SIM O <% 3d>:%s",recived,buffer_SIM900);	//state Opened
						if ( (recived > 12) && (memcmp(buffer_SIM900, "+RECEIVE,", 9) == 0 )) {	//само при state == Opened се стартира приемането на TCP пакет от SIM
							t_min_no_connect = 0;
							fl_test_connect = 0;
							
							to_reciv = atoi((char*)(buffer_SIM900 + 11));
							client_number = atoi((char*)(buffer_SIM900 + 9));
							daemon_log(LOG_DEBUG, "Client %d to recived SIM:%d", client_number,to_reciv);
						}else if( (recived > 13) && (memcmp(buffer_SIM900+1, ", REMOTE IP: ", 13) == 0 )){		//само при state == Opened Open TCP client
							client_number = atoi((char*)(buffer_SIM900));
							Clients_sIP4addres[client_number].S_un.S_addr = my_inet_addr(buffer_SIM900+14);
							daemon_log(LOG_DEBUG, "Open TCP client %d",client_number);
						}else if( (recived == 11) && (memcmp(buffer_SIM900+1, ", CLOSED", 8) == 0 )){		//само при state == Opened Close TCP client
							client_number = atoi((char*)(buffer_SIM900));
							if( Clients_sIP4addres[client_number].S_un.S_addr ){
								Clients_sIP4addres[client_number].S_un.S_addr = 0;
								daemon_log(LOG_DEBUG, "Close TCP client %d",client_number);
							}
						}else if( (recived > 11) && (memcmp(buffer_SIM900, "+PDP: DEACT", 11) == 0 )){		//само при state == Opened
							On_off_SIM();
						}else if( (recived > 13) && (memcmp(buffer_SIM900, "+CIPSERVER: 0", 13) == 0 )){		//само при state == Opened
							On_off_SIM();
						}else if( (recived > 13) && (memcmp(buffer_SIM900, "+CIPSERVER: 1", 13) == 0 )){		//само при state == Opened
							t_min_no_connect = 0;
							fl_test_connect = 0;
						}else if( (recived == 26) && (memcmp(buffer_SIM900+15, "\"INITIAL\"\r\n", 11) == 0 )){
							client_number = atoi((char*)(buffer_SIM900+3));
							if( Clients_sIP4addres[client_number].S_un.S_addr ){
								Clients_sIP4addres[client_number].S_un.S_addr = 0;
								daemon_log(LOG_DEBUG, "Close TCP client %d",client_number);
							}
						}else if( (recived == 13) && (memcmp(buffer_SIM900+1, ", CLOSE OK\r\n", 12) == 0 )){	//затваря TCP клиент от AT+CIPCLOSE=7
							/*client_number = atoi((char*)(buffer_SIM900));
							if( Clients_sIP4addres[client_number].S_un.S_addr ){
								Clients_sIP4addres[client_number].S_un.S_addr = 0;
								daemon_log(LOG_DEBUG, "Close TCP client %d",client_number);
							}*/
							if( Clients_sIP4addres[7].S_un.S_addr ){
								Clients_sIP4addres[7].S_un.S_addr = 0;
								daemon_log(LOG_DEBUG, "Close TCP client 7 to PC server");
							}
						}else if( (recived == 12) && (memcmp(buffer_SIM900, "7, SEND OK\r\n", 12) == 0 )){	//затваря TCP клиент със AT+CIPCLOSE=7 (приключва сесията му)
							to_close_client7 = 1;
						}else if( (recived == 15) && (memcmp(buffer_SIM900+1, ", CONNECT OK\r\n", 14) == 0 )){	//отваря TCP клиент от AT+CIPSTART=7
							/*client_number = atoi((char*)(buffer_SIM900));
							Clients_sIP4addres[client_number].S_un.S_addr = my_inet_addr((unsigned char*)text_to_CIPSTART);
							daemon_log(LOG_DEBUG, "Open TCP client %d \"%s\"",client_number,text_to_CIPSTART);*/
							Clients_sIP4addres[7].S_un.S_addr = my_inet_addr((unsigned char*)text_to_CIPSTART);
							daemon_log(LOG_DEBUG, "Open TCP client 7 to PC server \"%s\"",text_to_CIPSTART);
						}else if( recived > 43 ){
							if( memcmp(buffer_SIM900 + recived - 10, "\"CLOSED\"\r\n", 10) == 0){
								client_number = atoi((char*)(buffer_SIM900+3));
								if( Clients_sIP4addres[client_number].S_un.S_addr ){
									Clients_sIP4addres[client_number].S_un.S_addr = 0;
									daemon_log(LOG_DEBUG, "Close TCP client %d",client_number);
								}
							}else if( memcmp(buffer_SIM900 + recived - 13,"\"CONNECTED\"\r\n", 13) == 0){
								client_number = atoi((char*)(buffer_SIM900+3));
								Clients_sIP4addres[client_number].S_un.S_addr = my_inet_addr(buffer_SIM900+14);
								daemon_log(LOG_DEBUG, "Open TCP client %d",client_number);
							}else if( memcmp(buffer_SIM900 + recived - 18,"\"REMOTE CLOSING\"\r\n", 18) == 0){
								client_number = atoi((char*)(buffer_SIM900+3));
								if( Clients_sIP4addres[client_number].S_un.S_addr ){
									Clients_sIP4addres[client_number].S_un.S_addr = 0;
									daemon_log(LOG_DEBUG, "Close TCP client %d",client_number);
								}
							}else 
								daemon_log(LOG_DEBUG, "Ko da go prawq ? <<%s>>",buffer_SIM900);
						}
						recived = 0;
					}
					break;
				}
			}
		}
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

static uint8_t to_close = 0;
static uint8_t to_open = 0;
static void SendPacageToPC_Client(int len);

void SimLoop(void) {
	if( no_SimLoop )
		return;
		
	//Съобщение от концентратора къ PC сървър	
	if( psServerIP->S_un.S_addr ){	//ако въобще е програмиран PC сървър
		if( to_send == 0 ){	//ако няма текущи съобюения
			if( data_to_server ){	//ако има данни към PC сървъра
				if( psServerIP->S_un.S_addr != Clients_sIP4addres[7].S_un.S_addr ){	//отворен ли е правилния клиент към PC сървъра
					if( Clients_sIP4addres[7].S_un.S_addr ){	
						to_open = 0;
						if( to_close == 0 ){
							to_close = 1;
							SimWrite((unsigned char*)"AT+CIPCLOSE=7\r\n");
							SimRead();
						}
					}else {
						to_close = 0;
						if( to_open == 0 ){
							to_open = 1;
							char text[50];
							sprintf_P(text_to_CIPSTART,PSTR("%d.%d.%d.%d"),psServerIP->S_un.S_un_b.s_b1,psServerIP->S_un.S_un_b.s_b2
								,psServerIP->S_un.S_un_b.s_b3,psServerIP->S_un.S_un_b.s_b4);
							sprintf_P(text,PSTR("AT+CIPSTART=7,\"TCP\",\"%s\",\"1873\"\r\n"),text_to_CIPSTART);
							SimWrite((unsigned char*)text);
							SimRead();
						}
					}
				}else{
					to_open = 0;
					to_close = 0;
					switch( data_to_server ){
						case SET_MY_IP_TO_CLIENT:
							{
								ipv6_buf[0] = (sizeof(sin_addr) + 1) >> 8;
								ipv6_buf[1] = (sizeof(sin_addr) + 1) & 0xFF;
								ipv6_buf[2] = VERSION;
								ipv6_buf[3] = SET_MY_IP_TO_CLIENT;
								sin_addr my_ip;
								my_ip.S_un.S_addr = htonl(psServerIP->S_un.S_addr);
								memcpy(ipv6_buf + HEADER_SIZE + 1, (unsigned char*)&my_ip,sizeof(sin_addr));
								SendPacageToPC_Client(HEADER_SIZE + 1 + sizeof(sin_addr));
							}
							break;
						default:
							daemon_log(LOG_DEBUG, "Error unknow message to PC server %d",ipv6_buf[HEADER_SIZE]);
							break;
					}
					data_to_server = 0;
				}
			}
			if( to_close_client7 ){
				to_close_client7 = 0;
				SimWrite((unsigned char*)"AT+CIPCLOSE=7\r\n");
				SimRead();
			}
		}
	}
	
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
		SimWrite((unsigned char*)"AT\r\n");
		SimRead();
	}
		
	if (fl_read){		//не се интерисува от автомата за стартиране на SIM
		if(key_b){
			key_b = 0;
			SimWrite((unsigned char*)"AT+CIPSERVER?\r\n");
			SimRead();
		}
		if(key_c){
			key_c = 0;
			SimWrite((unsigned char*)"AT+CIPSTATUS\r\n");
			SimRead();
		}
		if( state == Opened ){
			if( fl_test_connect == 0 ){
				if( t_min_no_connect >= TIME_TEST_CONNECT ){
					fl_test_connect = 1;
					SimWrite((unsigned char*)"AT+CIPSERVER?\r\n");	//
				}
			}
		}
		LoopRead();
	}else{			// съобщения към SIM от автомата за стартиране на SIM
		switch (state) {
		case AT_test:
		{
			SleepSIM(3000);
			SimWrite((unsigned char*)"AT\r\n");
			SimRead();
		}
		break;
		case ATE0:
		{
			SleepSIM(1000);
			SimWrite((unsigned char*)"ATE0\r\n");
			SimRead();
		}
		break;
		case CIFSR_test:
		{
			SleepSIM(1000);
			SimWrite((unsigned char*)"AT+CIFSR\r\n");
			SimRead();
		}
		break;
		case PIN_test:
		{
			SleepSIM(1000);
			SimWrite((unsigned char*)"AT+CPIN?\r\n");
			SimRead();
		}
		break;
		case PIN_set:
		{
			SleepSIM(3000);
			char text[50];
			int old_ver = verbosity;
			verbosity = LOG_WARNING;
			sprintf(text, "AT+CPIN=\"%s\"\r\n", pin);
			SimWrite((unsigned char*)text);
			verbosity = old_ver;
			SimRead();
		}
		break;
		case CIPMUX:
		{
			SleepSIM(1000);
			SimWrite((unsigned char*)"AT+CIPMUX=1\r\n");
			SimRead();
		}
		break;
		case CREG:
		{
			SleepSIM(1000);
			SimWrite((unsigned char*)"AT+CREG?\r\n");
			SimRead();
		}
		break;
		case CGATT:
		{
			SleepSIM(4000);
			SimWrite((unsigned char*)"AT+CGATT?\r\n");
			SimRead();		
		}
		break;
		case CSQ:
		{
			SleepSIM(1000);
			SimWrite((unsigned char*)"AT+CSQ\r\n");
			SimRead();
		}
		break;
		case CSTT:
		{
			SleepSIM(1000);
			SimWrite((unsigned char*)"AT+CSTT=\"tvulosv\"\r\n");
			SimRead();
		}
		break;
		case CIICR:
		{
			SleepSIM(1000);
			SimWrite((unsigned char*)"AT+CIICR\r\n");
			SimRead();
		}
		break;
		case TO_Open:
		{
			SleepSIM(1000);
			SimWrite((unsigned char*)"AT+CIPSERVER=1,1873\r\n");
			SimRead();
		}
		break;
		case Opened:
		{
			SimRead();
		}
		break;
		case BAD_PIN:
			break;
		//default:
		//	break;
		}
	}
}

static void SendPacageToPC_Client(int len)
{
	client_number = 7;
	SendPacage(len);
}

void SendPacage(int len){
	to_send = len;
	sprintf((char*)buffer_SIM900,"AT+CIPSEND=%d\r\n",client_number);
	SimWrite((unsigned char*)buffer_SIM900);
}

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
		SimWrite((unsigned char*)"AT+CIPSHUT\r\n");
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
					buffer_SIM900[recived] = u8Data;
				recived++;
				cli();	//__disable_interrupt();
				time_sleep_SIM = 2000;
				sei();	//__enable_interrupt();
			}
		}
		if (recived) {
			buffer_SIM900[recived] = 0;
			daemon_log(LOG_DEBUG, "From SIM in reset:%s",buffer_SIM900);
			recived = 0;
		}
		no_SimLoop = 0;
		
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
					buffer_SIM900[recived] = u8Data;
				recived++;
				cli();	//__disable_interrupt();
				time_sleep_SIM = 2000;
				sei();	//__enable_interrupt();
			}
		}
		if (recived) {
			buffer_SIM900[recived] = 0;
			daemon_log(LOG_DEBUG, "From SIM in reset:%s",buffer_SIM900);
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
					buffer_SIM900[recived] = u8Data;
				recived++;
				cli();	//__disable_interrupt();
				time_sleep_SIM = 200;
				sei();	//__enable_interrupt();
			}
		}
		if (recived) {
			buffer_SIM900[recived] = 0;
			daemon_log(LOG_DEBUG, "From SIM in reset:%s",buffer_SIM900);
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

void SendTextCommand(uint8_t *text){
	SimWrite(text);
	SimRead();
}

void SendOnlyCommand(uint8_t new_state){
	if( new_state > 24 ){
		printf_P(PSTR("\n\rError state\n\r"));
		return;
	}else
		printf_P(PSTR("\n\r"));
	switch( new_state ){
		case AT_test:
		{
			//SleepSIM(3000);
			SimWrite((unsigned char*)"AT\r\n");
			SimRead();
		}
		break;
		case ATE0:
		{
			//SleepSIM(1000);
			SimWrite((unsigned char*)"ATE0\r\n");
			SimRead();
		}
		break;
		case CIFSR_test:
		{
			//SleepSIM(1000);
			SimWrite((unsigned char*)"AT+CIFSR\r\n");
			SimRead();
		}
		break;
		case PIN_test:
		{
			//SleepSIM(1000);
			SimWrite((unsigned char*)"AT+CPIN?\r\n");
			SimRead();
		}
		break;
		case PIN_set:
		{
			//SleepSIM(3000);
			char text[50];
			int old_ver = verbosity;
			verbosity = LOG_WARNING;
			sprintf(text, "AT+CPIN=\"%s\"\r\n", pin);
			SimWrite((unsigned char*)text);
			verbosity = old_ver;
			SimRead();
		}
		break;
		case CIPMUX:
		{
			//SleepSIM(1000);
			SimWrite((unsigned char*)"AT+CIPMUX=1\r\n");
			SimRead();
		}
		break;
		case CREG:
		{
			//SleepSIM(1000);
			SimWrite((unsigned char*)"AT+CREG?\r\n");
			SimRead();
		}
		break;
		case CGATT:
		{
			//SleepSIM(4000);
			SimWrite((unsigned char*)"AT+CGATT?\r\n");
			SimRead();		
		}
		break;
		case CSQ:
		{
			//SleepSIM(1000);
			SimWrite((unsigned char*)"AT+CSQ\r\n");
			SimRead();
		}
		break;
		case CSTT:
		{
			//SleepSIM(1000);
			SimWrite((unsigned char*)"AT+CSTT=\"tvulosv\"\r\n");
			SimRead();
		}
		break;
		case CIICR:
		{
			//SleepSIM(1000);
			SimWrite((unsigned char*)"AT+CIICR\r\n");
			SimRead();
		}
		break;
		case BAD_PIN:
			printf_P(PSTR("Not to sim write\n\r"));
			break;
		case TO_Open:
		{
			//SleepSIM(1000);
			SimWrite((unsigned char*)"AT+CIPSERVER=1,1873\r\n");
			SimRead();
		}
		break;
		case 13:
		{
			SimWrite((unsigned char*)"AT+CIPSERVER=0\r\n");
			SimRead();
		}
		break;
		case 14:
		{	
			char text[50];
			//strcpy(text_to_CIPSTART,"87.121.77.236");//От офиса постоянно IP с платка USB/TCP NO:9700 00-02-19-0E-20-0E
			//strcpy(text_to_CIPSTART,"87.121.77.8");//От офиса
			strcpy(text_to_CIPSTART,"79.100.162.29");//Повредена карта сложена е от 15 Vivacom, но после заработи със SIM към PC и сега е там
			//strcpy(text_to_CIPSTART,"79.100.161.227");//Средец ТП П.училище
			//strcpy(text_to_CIPSTART,"83.228.109.121");//SIM test ELL
			sprintf_P(text,PSTR("AT+CIPSTART=7,\"TCP\",\"%s\",\"1873\"\r\n"),text_to_CIPSTART);
			SimWrite((unsigned char*)text);
			//SimWrite((unsigned char*)"AT+CIPSTART=7,\"TCP\",\"79.100.162.29\",\"1873\"\r\n");//Повредена карта сложена е от 15 Vivacom, но псле заработи със SIM към PC и сега е там
			SimRead();
		}
		break;
		case 15:
		{
			SimWrite((unsigned char*)"AT+CIPCLOSE=7\r\n");
			SimRead();
		}
		break;
		case 16:
		{
			SimWrite((unsigned char*)"AT+CIPSEND=7\r\n");
			SimRead();
		}
		break;
		case 17:
		{
			SimWrite((unsigned char*)"\32");	//$1A
			SimRead();
		}
		break;
		case 18:
		{
			SimWrite((unsigned char*)"AT+CIPSEND?\r\n");
			SimRead();
		}
		break;
		case 19:
		{
			SimWrite((unsigned char*)"00010006\32");	//00010006$1A
			SimRead();
		}break;
		case 20:
		{
			SimWrite((unsigned char*)"AT\r\n");
			SimRead();
		}
		break;
		case 22:
		{
			SimWrite((unsigned char*)"AT+CIPSHUT\r\n");
			SimRead();
		}
		break;
		case 23:
		{
			SimWrite((unsigned char*)"AT+CIPSTATUS\r\n");
			SimRead();
		}
		break;
		case 24:
		{
			SimWrite((unsigned char*)"AT+CIPSERVER?\r\n");
			SimRead();
		}
		break;
		default:
		{
			printf_P(PSTR("Not to sim write\n\r"));
		}
		break;
	}
}

void SetSimState(uint8_t new_state){
	if( new_state > Opened ){
		printf_P(PSTR("\n\rError state\n\r"));
		return;
	}else
		printf_P(PSTR("\n\r"));
	switch( state = new_state ){
		case AT_test:
		{
			//SleepSIM(3000);
			SimWrite((unsigned char*)"AT\r\n");
			SimRead();
		}
		break;
		case ATE0:
		{
			//SleepSIM(1000);
			SimWrite((unsigned char*)"ATE0\r\n");
			SimRead();
		}
		break;
		case CIFSR_test:
		{
			//SleepSIM(1000);
			SimWrite((unsigned char*)"AT+CIFSR\r\n");
			SimRead();
		}
		break;
		case PIN_test:
		{
			//SleepSIM(1000);
			SimWrite((unsigned char*)"AT+CPIN?\r\n");
			SimRead();
		}
		break;
		case PIN_set:
		{
			//SleepSIM(3000);
			char text[50];
			int old_ver = verbosity;
			verbosity = LOG_WARNING;
			sprintf(text, "AT+CPIN=\"%s\"\r\n", pin);
			SimWrite((unsigned char*)text);
			verbosity = old_ver;
			SimRead();
		}
		break;
		case CIPMUX:
		{
			//SleepSIM(1000);
			SimWrite((unsigned char*)"AT+CIPMUX=1\r\n");
			SimRead();
		}
		break;
		case CREG:
		{
			//SleepSIM(1000);
			SimWrite((unsigned char*)"AT+CREG?\r\n");
			SimRead();
		}
		break;
		case CGATT:
		{
			//SleepSIM(4000);
			SimWrite((unsigned char*)"AT+CGATT?\r\n");
			SimRead();		
		}
		break;
		case CSQ:
		{
			//SleepSIM(1000);
			SimWrite((unsigned char*)"AT+CSQ\r\n");
			SimRead();
		}
		break;
		case CSTT:
		{
			//SleepSIM(1000);
			SimWrite((unsigned char*)"AT+CSTT=\"tvulosv\"\r\n");
			SimRead();
		}
		break;
		case CIICR:
		{
			//SleepSIM(1000);
			SimWrite((unsigned char*)"AT+CIICR\r\n");
			SimRead();
		}
		break;
		case BAD_PIN:
			printf_P(PSTR("Not to sim write\n\r"));
			break;
		case TO_Open:
		{
			//SleepSIM(1000);
			SimWrite((unsigned char*)"AT+CIPSERVER=1,1873\r\n");
			SimRead();
		}
		break;
		case Opened:
		{
			printf_P(PSTR("Not to sim write\n\r"));
			SimRead();
		}
		break;
	}
}

static void SimRead(void) {
	cli();	//__disable_interrupt();
	time_sleep_SIM = T_WAIT_PACKET;
	sei();	//__enable_interrupt();
	fl_read = 1;
	recived = 0;
}

static void SimWrite(const unsigned char * text) {
	daemon_log(LOG_DEBUG, "To SIM:%s",text);
	while (*text)
		sim_serial_write(*text++);
}

static uint8_t first_time = 1;
static void On_off_SIM(void){
	
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

static void SleepSIM(uint16_t sleep_time){
	cli();	//__disable_interrupt();
	time_sleep_SIM = sleep_time;
	sei();	//__enable_interrupt();
	no_SimLoop = 1;
	while(time_sleep_SIM)
		main_loop();
	no_SimLoop = 0;
}