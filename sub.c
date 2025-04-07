#ifndef WIN32
#include "FTDI_Uart.h"
#include "twi_master_driver.h"
#include "hardware.h"
#include "MODBUS_Master.h"
#else	//WIN32
#include <windows.h>
#include <conio.h>
#include <stdint.h>
#include <time.h>
#include "windows_sub.h"
#endif //WIN32

#include "log.h"
#include "def.h"
#include "sub.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "JennicModule.h"
#include "TunDevice.h"
#include "SerialLink.h"

//#undef LOG_DEBUG
//#define LOG_DEBUG 6

unsigned char date_time[6];
unsigned char error_clock;
uint32_t time_1s = 0;
uint32_t time_1m = 0;


typedef struct
{
    uint8_t     u8Type;
    uint16_t    u16Length;
    uint8_t     u8Message[2048];
} sJennicModuleMsg;

static sJennicModuleMsg sIncomingMsg;

long time_sec_sub;
#define T_STATE_MASHINE 3

void ClearRam(void) {
	uint16_t i;
	for (i = 0; i < SIZE_RAM; i++) {
		*(p_E_RAM + i) = 0;
	}
	memcpy(&(sRouterStatus.sDateTimeClearRAM), date_time, sizeof(tsDateTime));
}

uint8_t get_digits(void) {
	uint8_t data, c;

	data = 0;

	while (1) {
		c = get_char();
		if ((c >= '0') && (c <= '9')) {
			data <<= 4;
			data += c - '0';
			put_char(c);
		}
		else
			break;
	}

	return data;
}

void text(void){
	printf_P(PSTR("\n\rJennic HOST V%d.%d.%d  Size NVM = %u bytes Free = %d bytes\n\r"),(unsigned int)(ntohl(sRouterStatus.u32HostVersion)>>16),
		(unsigned int)((ntohl(sRouterStatus.u32HostVersion)>>8)&0xFF),(unsigned int)(ntohl(sRouterStatus.u32HostVersion)&0xFF),END_NVM,SIZE_RAM-END_NVM);
	
	printf_P(PSTR("1 Broadcast 1\n\r"));
	printf_P(PSTR("2 Broadcast 127\n\r"));
	printf_P(PSTR("3 Broadcast 255\n\r"));
	
	printf_P(PSTR("4 On/Off Relay 1\n\r"));

#ifndef WIN32
	printf_P(PSTR("5 GPRS JP6 RST\n\r"));
	printf_P(PSTR("6 Test GPRS Uart JP6\n\r"));
	printf_P(PSTR("7 Test CRD2 Uart JP4\n\r"));
#endif //WIN32
	printf_P(PSTR("8 Test External RAM\n\r"));
	
	printf_P(PSTR("9 Set time & data\n\r"));
	
	printf_P(PSTR("0 Jennic reset\n\r"));
	
	printf_P(PSTR("p Set PIN\n\r"));
	printf_P(PSTR("t Set On/Off\n\r"));
	printf_P(PSTR("a,b test GPRS\n\r"));
	printf_P(PSTR("r RADIUS table\n\r"));
	printf_P(PSTR("v Verbosity\n\r"));
	printf_P(PSTR("x test CRC\n\r"));
	printf_P(PSTR("k SubTreeNodes, l NetworkTable\n\r"));
	
#ifndef WIN32
	printf_P(PSTR("Inputs: "));
	if( PORT_INPUTS.IN & INP1 ) put_char('0'); else put_char('1');
	if( PORT_INPUTS.IN & INP2 ) put_char('0'); else put_char('1');
	if( PORT_INPUTS.IN & INP3 ) put_char('0'); else put_char('1');
	if( PORT_INPUTS.IN & INP4 ) put_char('0'); else put_char('1');
	printf_P(PSTR("\n\r"));
#else //WIN32
	printf_P(PSTR("tab exit in Windows simulator\n\r"));
#endif //WIN32

	printf_P(PSTR("\n\rTime "));
	put_char((date_time[5]>>4)+'0');put_char(((date_time[5])&0x0f)+'0'); put_char('-');
	put_char((date_time[4]>>4)+'0');put_char(((date_time[4])&0x0f)+'0'); put_char('-');
	put_char((date_time[3]>>4)+'0');put_char(((date_time[3])&0x0f)+'0'); put_char(' ');
	
	put_char((date_time[2]>>4)+'0');put_char(((date_time[2])&0x0f)+'0'); put_char(':');
	put_char((date_time[1]>>4)+'0');put_char(((date_time[1])&0x0f)+'0'); put_char(':');
	put_char((date_time[0]>>4)+'0');put_char(((date_time[0])&0x0f)+'0');
	
	printf_P(PSTR(" On1 %2X:%02X  Off1 %2X:%02X    On2 %2X:%02X  Off2 %2X:%02X\n\r"), psTimers->sTimerOn1.u8Hour, psTimers->sTimerOn1.u8Minute, psTimers->sTimerOff1.u8Hour, psTimers->sTimerOff1.u8Minute,
		psTimers->sTimerOn2.u8Hour, psTimers->sTimerOn2.u8Minute, psTimers->sTimerOff2.u8Hour, psTimers->sTimerOff2.u8Minute);
}

static uint8_t old_sec = 0;
static uint8_t old_min = 0xff;
static uint8_t on_lamps = 0;

uint8_t on_relay = 0;

uint8_t current_light = 255;
uint8_t next_light = 255;
uint8_t next_group = 255;

static uint16_t u16Lamps=0;

#define TIME_NETWROR_ROUTER 121	//на 2 минути
#define BAD_TIME_WORK 50
#define START_TIME_NETWROR_ROUTER (5*60)	//5min
#define MAX_BLOB_NETWROR_ROUTER 255

static uint16_t u16TimeNetworkRouter = START_TIME_NETWROR_ROUTER;

uint16_t u16FirstTableEntry = 0;

static void OnRelay(void) {
	if (on_relay == 0) {
		int i;
		for (i = 0;i < u16_LampsInTable;i++) {
			(psLampTable + i)->u32OldMinutes = time_1m;
			(psLampTable + i)->u8FlSee = 0;
		}
		u16Lamps = 0;
		u16TimeNetworkRouter = START_TIME_NETWROR_ROUTER;
	}
	on_relay = 1;

	memset(&sRejectTable, 0, sizeof(sRejectTable));

	current_light = 255;
	next_light = 255;
	next_group = 255;
#ifndef WIN32
	PORT_RELAY.OUTSET = RELAY1;
#endif
} 

static void AddWorkTime(tsLamp * pLamp) {
	uint32_t u32t = time_1m - pLamp->u32OldMinutes;
	if (u32t < BAD_TIME_WORK) {
		if ((pLamp->u32Minutes += u32t)>60) {
			u32t = pLamp->u32Minutes/60;
			pLamp->u32Minutes = pLamp->u32Minutes % 60;
			pLamp->sLampStatus.u32WorkHours = htonl(ntohl(pLamp->sLampStatus.u32WorkHours) + u32t);
		}
	}
}

static void OffRelay(void){
	if (on_relay) {
		int i;
		for (i = 0;i < u16_LampsInTable;i++) {
			AddWorkTime(psLampTable + i);
		}
	}
	on_relay = 0;

	current_light = 255;
	next_light = 255;
	next_group = 255;
#ifndef WIN32
	PORT_RELAY.OUTCLR = RELAY1;
#endif
}

void OnTimer(void){
	if( on_lamps == 0 ){
		daemon_log(LOG_DEBUG, "OnTimer");
		on_lamps = 1;
		OnRelay();
	}
}

void OffTimer(void){
	if( on_lamps == 1 ){
		daemon_log(LOG_DEBUG, "OffTimer");
		on_lamps = 0;
		OffRelay();
	}
}

void OnLamp(void){
	daemon_log(LOG_DEBUG, "OnLamp");
	OnRelay();
}

void OffLamp(void){
	daemon_log(LOG_DEBUG, "OffLamp");
	OffRelay();
}

#define COU_BROADCAST 2
#define TIME_BROADCAST 15
static uint8_t curren_time[2], on[2], off[2];
static uint8_t broadcast_group;
static uint8_t cou_broadcast = 0;
static uint8_t time_broadcast = 0;

static int test_time(void) {
	int flag_on = 0;

	if (memcmp(on, off, 2) != 0) {
		if (memcmp(on, off, 2) > 0) {
			if (memcmp(curren_time, on, 2) >= 0) {
				flag_on = 1;
			}else if (memcmp(off, curren_time, 2) > 0) {
				flag_on = 1;
			}else {
				flag_on = -1;
			}
		}else if ((memcmp(curren_time, on, 2) >= 0) && (memcmp(curren_time, off, 2) < 0)) {
			flag_on = 1;
		}else {
			flag_on = -1;
		}
	}

//	daemon_log(LOG_DEBUG, "%2X:%02X %2X:%02X %2X:%02X %d",curren_time[0],curren_time[1],on[0],on[1],off[0],off[1],flag_on);

	return flag_on;
}

void ProcesNetworkRouterTable(uint8_t * pu8Data, int16_t i16Lenght) {
	if (on_relay) {
		uint8_t fl_ok = 1;
#if LOG_DEBUG == 6
		daemon_log(LOG_DEBUG, "Recived lenght %d", i16Lenght);
#endif
		if (i16Lenght <= 0) {
			fl_ok=0;
			//u16FirstTableEntry = ROUTE_TABLE_ENTRIES + (2 * MAX_BLOB_NETWROR_ROUTER);
			int i;
			for (i = 0; i < u16_LampsInTable; i++) {
				(psLampTable + i)->u8FlSee = 0;
			}
			u16FirstTableEntry = 0;
			u16_LampsConnected = u16Lamps;
			u16Lamps = 0;
			daemon_log(LOG_DEBUG, "Connected %d of %d", u16_LampsConnected, u16_LampsInTable);
		}
		while (i16Lenght > 0) {
			int i;
			pu8Data += 3;
			*(pu8Data + 0) ^= 0x02;
			daemon_log(LOG_DEBUG, "MAC %02X%02X%02X%02X%02X%02X%02X%02X", *(pu8Data + 0), *(pu8Data + 1), *(pu8Data + 2), *(pu8Data + 3)
				, *(pu8Data + 4), *(pu8Data + 5), *(pu8Data + 6), *(pu8Data + 7));

			for (i = 0;i < u16_LampsInTable;i++) {
				if (memcmp(&((psLampTable + i)->sLampStatus.sMAC_Address.MAC[0]), pu8Data, sizeof(tsMAC_Address)) == 0) {
					
					memcpy((psLampTable + i)->sLampStatus.sLastContacts.date_time, date_time, sizeof(tsDateTime));
					AddWorkTime(psLampTable + i);
					(psLampTable + i)->u32OldMinutes = time_1m;
					if ((psLampTable + i)->u8FlSee == 0) {
						(psLampTable + i)->u8FlSee = 1;
						u16Lamps++;
						daemon_log(LOG_DEBUG, "Ok");
					}else
						daemon_log(LOG_DEBUG, "Double report");
					break;
				}
			}
			pu8Data += 12;
			i16Lenght -= 15;
			
			u16FirstTableEntry++;
		}
		if(fl_ok){
			//u16FirstTableEntry += MAX_BLOB_NETWROR_ROUTER;
			daemon_log(LOG_DEBUG, "Test at address %d", u16FirstTableEntry);
			GetJenNetNetworkRouter( u16FirstTableEntry, MAX_BLOB_NETWROR_ROUTER);
		}
	}
}

void TestNetworkRouterTable(void){
	u16FirstTableEntry=0;
	daemon_log(LOG_DEBUG, "Test at address %d", u16FirstTableEntry);
	GetJenNetNetworkRouter( u16FirstTableEntry, MAX_BLOB_NETWROR_ROUTER);
}

void TestSubTreeNodes(void){
	GetSubTreeNodes();
}

void main_loop(void){

#ifndef WIN32
	wdt_reset();
#endif
	
	TunLoop();

#ifndef NO_COORDINATOR
  if(bSL_ReadMessage(&sIncomingMsg.u8Type, &sIncomingMsg.u16Length, sizeof(sIncomingMsg.u8Message), sIncomingMsg.u8Message)) {
    if (eJennicModuleProcessMessage(sIncomingMsg.u8Type, sIncomingMsg.u16Length, sIncomingMsg.u8Message) != E_MODULE_OK) {
      daemon_log(LOG_ERR, "Error communicating with border router module");
			eJennicModuleStart();
    }
  }
#endif //NO_COORDINATOR
	if (eTunDeviceReadPacket() != E_TUN_OK) {
		daemon_log(LOG_ERR, "Error handling tun packet");
	}
#ifndef NO_COORDINATOR
  // Select timeout 
	if ( (time(NULL) - time_sec_sub) >= T_STATE_MASHINE ) {
		time_sec_sub = (long)time(NULL);
		if (eJennicModuleStateMachine(1) != E_MODULE_OK){
			eJennicModuleStart();
		}
	}
#endif //NO_COORDINATOR


#ifndef WIN32
//	MODBUS_Master_Loop();
#endif
	
	get_time();
	
	if( old_sec != date_time[0] ){
		old_sec = date_time[0];
		time_1s ++;
		if (--u16TimeNetworkRouter == 0) {
			u16TimeNetworkRouter = TIME_NETWROR_ROUTER;
			if (on_relay) {
				/*if (u16FirstTableEntry > ROUTE_TABLE_ENTRIES){//u16_LampsInTable) {
					int i;
					for (i = 0; i < u16_LampsInTable; i++) {
						(psLampTable + i)->u8FlSee = 0;
					}
					u16FirstTableEntry = 0;
					u16_LampsConnected = u16Lamps;
					u16Lamps = 0;
					daemon_log(LOG_DEBUG, "Connected %d of %d", u16_LampsConnected, u16_LampsInTable);
				}
				else
					u16FirstTableEntry += MAX_BLOB_NETWROR_ROUTER;
				daemon_log(LOG_DEBUG, "Test at address %d", u16FirstTableEntry);
				GetJenNetNetworkRouter( u16FirstTableEntry, MAX_BLOB_NETWROR_ROUTER);*/
				
				u16FirstTableEntry=0;
				daemon_log(LOG_DEBUG, "Test at address %d", u16FirstTableEntry);
				GetJenNetNetworkRouter( u16FirstTableEntry, MAX_BLOB_NETWROR_ROUTER);
			}
		}
		if (cou_broadcast) {
			if (--time_broadcast == 0) {
				cou_broadcast--;
				time_broadcast = TIME_BROADCAST;
				if (E_MODULE_OK == GroupSetUint8ByModuleID(broadcast_group, 0xFFFFFE04, 0x02, next_light))
					current_light = next_light;
			}
		}
	}
		
	if( old_min != date_time[1] ){
		old_min = date_time[1];
		time_1m++;
		t_min_no_connect++;
		
		memcpy(&(sRouterStatus.sLastDateTime), date_time, sizeof(tsDateTime));

		if( error_clock == 0 ){
			int result;
			curren_time[0] = date_time[2]; curren_time[1] = date_time[1];

			on[0]= psTimers->sTimerOn1.u8Hour; on[1]= psTimers->sTimerOn1.u8Minute;
			off[0]= psTimers->sTimerOff1.u8Hour; off[1]= psTimers->sTimerOff1.u8Minute;
			if( (result = test_time()) < 0 ){
				on[0]= psTimers->sTimerOn2.u8Hour; on[1]= psTimers->sTimerOn2.u8Minute;
				off[0]= psTimers->sTimerOff2.u8Hour; off[1]= psTimers->sTimerOff2.u8Minute;
				if( (result = test_time()) == 0 )
					result = -1;
			}
			
			if (result > 0)	OnTimer();	else if (result < 0)	OffTimer();

			if (on_relay) {
				int i;
				for (i = 0;i < MAX_GROUP_TIMERS;i++) {
					on[0] = psTimers->sGroupTimer[i].sTimerOn.u8Hour; on[1] = psTimers->sGroupTimer[i].sTimerOn.u8Minute;
					off[0] = psTimers->sGroupTimer[i].sTimerOff.u8Hour; off[1] = psTimers->sGroupTimer[i].sTimerOff.u8Minute;
					result = test_time();

					if (result > 0) {
						next_light = psTimers->sGroupTimer[i].u8Lights;
						next_group = i;
					}else if (result < 0) {
						if ((255 != current_light)) {
							if (next_group == 255) {
								next_light = 255;
								next_group = i;
							}
						}
					}
				}
				//next_group = 1;
				//next_light = 10;
				if (next_group != 255) {
					if (current_light != next_light) {
						daemon_log(LOG_DEBUG, "%2X:%02X Group %d Light %d", curren_time[0], curren_time[1], next_group + 1, next_light);
						broadcast_group = next_group;
						cou_broadcast = COU_BROADCAST;
						time_broadcast = TIME_BROADCAST;
						if(E_MODULE_OK == GroupSetUint8ByModuleID(broadcast_group, 0xFFFFFE04, 0x02, next_light))
							current_light = next_light;
					}
					next_group = 255;
				}
			}
		}
	}
}


void TestExtRAM(void)
{
	uint16_t i,error,address;
	uint16_t data;
	
	for( i = 0 ; i < SIZE_RAM; i ++ ){
		//data = __far_mem_read(EXY_RAM_ADD+i);
		data = *(p_E_RAM+i);
		if( data )
			break;
	}
	if( i != SIZE_RAM){
		printf_P(PSTR("\n\rExernal RAM not clear at Address:0x%X"),i);
	}
	
	error = 0;
	address = 0;
	for( i = 0 ; i < SIZE_RAM; i ++ ){
		if( i & 0x01 )
			data = ((i>>8)&0xFF);
		else
			data = (i&0xff);
		//__far_mem_write(EXY_RAM_ADD+i,data);
		*(p_E_RAM+i) = (unsigned char)data;
	}

	for( i = 0 ; i < SIZE_RAM; i ++ ){
		//data = __far_mem_read(EXY_RAM_ADD+i);
		data = *(p_E_RAM+i);
		if( i & 0x01 ){
			if( data != ((i>>8)&0xFF)){
				error ++;
				if(address == 0)
					address = i;
			}
		}else{
			if( data != (i&0xff)){
				error ++;
				if(address == 0)
					address = i;
			}
		}
	}
	
	if( error )
		printf_P(PSTR("\n\rExernal RAM ERRORS:%u at Address:0x%X\n\r"),error , address);
	else
		printf_P(PSTR("\n\rExternal RAM OK\n\r"));
		
	ClearRam();
}

void make_crc(unsigned char *p, int len)
{
	int i;
	unsigned char crc = 0;
	for (i = 0; i < len; i++)
		crc += *p++;
	*p = crc;
}

unsigned char check_crc(unsigned char *p, int len)
{
	int i;
	unsigned char crc = 0;
	for (i = 0; i < len; i++)
		crc += *p++;
	if (*p == crc)
		return 0;
	return 1;
}