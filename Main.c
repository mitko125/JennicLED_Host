#ifndef WIN32

#include "avr_compiler.h"
#include <clksys_driver.h>
#include "ebi_driver.h"

#include "CRD2_Uart.h"
#include "GPRS_Uart.h"
#include "FTDI_Uart.h"
#include "twi_master_driver.h"
#include "MODBUS_Master.h"
#include "hardware.h"
extern TWI_Master_t twiMaster;

#else //WIN32

#include <windows.h>
#include <process.h> 
#include <time.h>
#include <conio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <errno.h>
#include <sys/stat.h>
#include <signal.h>
#include "Serial.h"
#include <stdint.h>

#include "windows_sub.h"

#endif	//WIN32

#include "log.h"
#include "def.h"
#include "sub.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include "JennicModule.h"
#include "TunDevice.h"
#include "SerialLink.h"

int num_pin = 0;


//char enable_pin[5]="0000";
//int verbosity = LOG_DEBUG;


char enable_pin[5]="";
int verbosity = LOG_NOTICE;//LOG_INFO;       /** Default log level */


#ifdef WIN32

void get_pin(void){
	strcpy_s(pin,5,PIN_STR);
}

#define HOST_VERSION 0x00020000UL


#else	// WIN32



#endif	//WIN32


#ifndef WIN32
void MODBUS_Maser_Ok_reciv(void){
}
#endif //WIN32

int main(void){

#ifdef WIN32
	verbosity = LOG_DEBUG;//LOG_ERR;//LOG_DEBUG;
#endif

	InitHardware();
	
	get_time();
	
	get_pin();

//	printf_P(PSTR("Read PIN : %s\n\r"),pin);
//	printf_P(PSTR("Verbosity %d enable_pin %s pin %s\n\r"),verbosity,enable_pin,pin);
			
	
	unsigned char BAD_RAM = 0;

	if (check_crc((unsigned char*)psModuleSetConfig, sizeof(tsConfigBorderRuter))) {
		daemon_log(LOG_ERR, "BAD crc in RAM, psModuleSetConfig");
		BAD_RAM = 1;
	}

	if (check_crc((unsigned char*)psTimers, sizeof(tsTimers))) {
		daemon_log(LOG_ERR, "BAD crc in RAM, psTimers");
		BAD_RAM = 1;
	}
		
	if (BAD_RAM) {
		daemon_log(LOG_ERR, "BAD clear RAM");
		ClearRam();
	}

	sRouterStatus.u16OnCounter = htons(ntohs(sRouterStatus.u16OnCounter)+1);
	sRouterStatus.u16SimErrors = 0;
	sRouterStatus.u32HostVersion = htonl(HOST_VERSION);
	sRouterStatus.u32JennicDeviceVersion = 0;
	memcpy(&(sRouterStatus.sDateTimeOn), date_time, sizeof(tsDateTime));
	memcpy(&(sRouterStatus.sDateTimeOff), &(sRouterStatus.sLastDateTime), sizeof(tsDateTime));
	






	//get_char();











    /* Wait up to five seconds. */
#ifdef WIN32
	if ((serial_open(15, 1000000) < 0) || (eTunDeviceOpen(13/*8*/,115200L) != E_TUN_OK))
  {
    goto finish;
  }
#endif

	#ifndef NO_COORDINATOR
  eJennicModuleStart();
	#endif
	time_sec_sub = (long)time(NULL);
		
	while(1){
		
		main_loop();
		
		if(kb_hit()){
			char c = get_char();

			text();
			
#ifndef WIN32
			if( strcmp(enable_pin,pin)!= 0){
				if(num_pin < 4){
					if( (c >='0') && (c<='9')){
						enable_pin[num_pin++] = c;
						enable_pin[num_pin]=0;
					}else{
						enable_pin[num_pin=0] =0;
					}
				}else{
					enable_pin[num_pin=0] =0;
				}
			}else
#endif //WIN32
			{
			switch(c){
			case '1':
				//GetJenNetNetworkRouter(0, 10);
				GlobalSetUint8ByModuleID(0xFFFFFE04, 0x02, 1);
				break;
			case '2':
				//GetJenNetNetworkRouter(1, 10);
				GlobalSetUint8ByModuleID(0xFFFFFE04, 0x02, 127);
				break;
			case '3':
				//GetJenNetNetworkRouter(2, 10);
				GlobalSetUint8ByModuleID(0xFFFFFE04, 0x02, 255);
				break;
			case '4':
				if(on_relay)
					OffLamp();
				else
					OnLamp();
				break;
#ifndef WIN32
			case '5':
				if( PORT_GPRS_RST.IN & GPRS_RST )
					PORT_GPRS_RST.OUTCLR = GPRS_RST;
				else
					PORT_GPRS_RST.OUTSET = GPRS_RST;
				break;
			case '6':
				GPRS_put_str("0123456789ABCDEF");
				printf_P(PSTR("To GPRS Uart    sendet:0123456789ABCDEF\n\r"));
				printf_P(PSTR("Press any key !\n\r"));
				get_char();
				printf_P(PSTR("From GPRS uart recived:"));
				while( GPRS_kb_hit() )
					put_char( GPRS_get_char() );
				printf_P(PSTR("\n\r"));
				break;
			case '7':
				CRD2_put_str("FEDCBA9876543210");
				printf_P(PSTR("To CRD2 Uart    sendet:FEDCBA9876543210\n\r"));
				printf_P(PSTR("Press any key !\n\r"));
				get_char();
				printf_P(PSTR("From CRD2 uart recived:"));
				while( CRD2_kb_hit() )
					put_char( CRD2_get_char() );
				printf_P(PSTR("\n\r"));
				break;
#endif	//WIN32
			case '8':
				TestExtRAM();
				break;
#ifndef WIN32
			case '9':
				{
					uint8_t reg[3],data;
					
					printf_P(PSTR("\n\rSet Year:"));
					data = get_digits();
					reg[0] = 6;	reg[1] = data;
					TWI_MasterWriteRead(&twiMaster,	SLAVE_ADDRESS,reg,2,0);		while (twiMaster.status != TWIM_STATUS_READY);
					
					printf_P(PSTR("\n\rSet Month:"));
					data = get_digits();
					reg[0] = 5;	reg[1] = data&0x1F;
					TWI_MasterWriteRead(&twiMaster,	SLAVE_ADDRESS,reg,2,0);		while (twiMaster.status != TWIM_STATUS_READY);
					
					printf_P(PSTR("\n\rSet Date:"));
					data = get_digits();
					reg[0] = 4;	reg[1] = data&0x3F;
					TWI_MasterWriteRead(&twiMaster,	SLAVE_ADDRESS,reg,2,0);		while (twiMaster.status != TWIM_STATUS_READY);
					
					printf_P(PSTR("\n\rSet Hour:"));
					data = get_digits();
					reg[0] = 2;	reg[1] = data&0x3F; 	reg[2] = 0x08;	//Enable Vbat
					TWI_MasterWriteRead(&twiMaster,	SLAVE_ADDRESS,reg,3,0);		while (twiMaster.status != TWIM_STATUS_READY);
					
					printf_P(PSTR("\n\rSet Minute:"));
					data = get_digits();
					reg[0] = 0;	 reg[1] = 0x80; reg[2] = data&0x7F;	//Enable RTC
					TWI_MasterWriteRead(&twiMaster,	SLAVE_ADDRESS,reg,3,0);		while (twiMaster.status != TWIM_STATUS_READY);
					
					error_clock = 0;
				}
				break;
#endif //WIN32
			case '0':
				#ifndef NO_COORDINATOR
				eJennicModuleStart();
				#endif
				break;
			case 'l':
			case 'L':
				TestNetworkRouterTable();
				break;
			case 'k':
			case 'K':
				TestSubTreeNodes();
				break;
#ifndef WIN32
			case 'p':
			case 'P':
				set_pin();
				break;
#endif //WIN32
			case 't':
			case 'T':
				{
					printf_P(PSTR("\n\rSet On1 Hour:"));
					psTimers->sTimerOn1.u8Hour = get_digits();
					make_crc((unsigned char*)psTimers, sizeof(tsTimers));
					printf_P(PSTR("\n\rSet On1 Minute:"));
					psTimers->sTimerOn1.u8Minute = get_digits();
					make_crc((unsigned char*)psTimers, sizeof(tsTimers));
					printf_P(PSTR("\n\rSet Off1 Hour:"));
					psTimers->sTimerOff1.u8Hour = get_digits();
					make_crc((unsigned char*)psTimers, sizeof(tsTimers));
					printf_P(PSTR("\n\rSet Off1 Minute:"));
					psTimers->sTimerOff1.u8Minute = get_digits();
					make_crc((unsigned char*)psTimers, sizeof(tsTimers));
					
					printf_P(PSTR("\n\rSet On2 Hour:"));
					psTimers->sTimerOn2.u8Hour = get_digits();
					make_crc((unsigned char*)psTimers, sizeof(tsTimers));
					printf_P(PSTR("\n\rSet On2 Minute:"));
					psTimers->sTimerOn2.u8Minute = get_digits();
					make_crc((unsigned char*)psTimers, sizeof(tsTimers));
					printf_P(PSTR("\n\rSet Off2 Hour:"));
					psTimers->sTimerOff2.u8Hour = get_digits();
					make_crc((unsigned char*)psTimers, sizeof(tsTimers));
					printf_P(PSTR("\n\rSet Off2 Minute:"));
					psTimers->sTimerOff2.u8Minute = get_digits();
					make_crc((unsigned char*)psTimers, sizeof(tsTimers));
				}
				break;
			case 'v':
			case 'V':
				printf_P(PSTR("\n\rSet verbosity:"));
				verbosity = get_digits();
				break;
			case 'x':
			case 'X':
				printf_P(PSTR("\n\rCRC tsConfigBorderRuter = %d    psTimers = %d\n\r"),
					check_crc((unsigned char*)psModuleSetConfig, sizeof(tsConfigBorderRuter)),
					check_crc((unsigned char*)psTimers, sizeof(tsTimers)));
				break;
			case 'r':
			case 'R':
				{
					int i;
					for (i = 0;i < u16_LampsInTable;i++) {
						printf_P(PSTR("Lamp % 3d %02X%02X%02X%02X%02X%02X%02X%02X\n"),i
						, (psLampTable + i)->sLampStatus.sMAC_Address.MAC[0], (psLampTable + i)->sLampStatus.sMAC_Address.MAC[1]
						, (psLampTable + i)->sLampStatus.sMAC_Address.MAC[2], (psLampTable + i)->sLampStatus.sMAC_Address.MAC[3]
						, (psLampTable + i)->sLampStatus.sMAC_Address.MAC[4], (psLampTable + i)->sLampStatus.sMAC_Address.MAC[5]
						, (psLampTable + i)->sLampStatus.sMAC_Address.MAC[6], (psLampTable + i)->sLampStatus.sMAC_Address.MAC[7]);
					}
				}
				break;
			case 'a':
			case 'A':
				key_a = 1;
				break;
			case 'b':
			case 'B':
				key_b = 1;
				break;
			case ' ':
#ifdef WIN32
				goto end;
#endif //WIN32
				break;
			}
		}
		}
	}
#ifdef WIN32
end:
	#ifndef NO_COORDINATOR
	daemon_log(LOG_INFO, "Resetting Coordinator Module");
	eJennicModuleReset();
	#endif
finish:
	save_RAM();
#endif //WIN32
	return -1;
}