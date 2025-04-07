#ifndef WIN32

#include "avr_compiler.h"
#include <clksys_driver.h>
#include "ebi_driver.h"
#include <eeprom_driver.h>
#include "CRD2_Uart.h"
#include "GPRS_Uart.h"
#include "FTDI_Uart.h"
#include "twi_master_driver.h"
#include "defs.h"

#else //WIN32

#include <stdio.h>
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

#define kb_hit _kbhit
#define get_char _getch
#endif	//WIN32

#include "log.h"
#include "def.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include "JennicModule.h"
#include "TunDevice.h"
#include "SerialLink.h"

uint8_t on_lamps = 0;


#ifdef WIN32

void main_loop(void){
	;
}

void get_pin(void){
	strcpy(pin,PIN_STR);
}

#else	// WIN32

TWI_Master_t twiMaster;    /*!< TWI master module. */
/*! CPU speed 2MHz, BAUDRATE 100kHz and Baudrate Register Settings */
//#define CPU_SPEED       2000000
#define BAUDRATE	100000	//100000
#define TWI_BAUDSETTING TWI_BAUD(F_CPU, BAUDRATE)
#define SLAVE_ADDRESS (0xDE>>1)

/*! TWIC Master Interrupt vector. */
ISR(TWIF_TWIM_vect)
{
	TWI_MasterInterruptHandler(&twiMaster);
}


static void text(void){
	char str[40];

	put_str("\n\rJennic HOST Hardware test\n\r");
	
	put_str("1 Broadcast 1\n\r");
	put_str("2 Broadcast 127\n\r");
	put_str("3 Broadcast 255\n\r");
	
	put_str("4 On/Off Relay 1\n\r");
		
	put_str("5 GPRS JP6 RST\n\r");
	put_str("6 Test GPRS Uart JP6\n\r");
	put_str("7 Test CRD2 Uart JP4\n\r");
	put_str("8 Test External RAM\n\r");
	
	put_str("9 Set time & data\n\r");
	
	put_str("0 Jennic reset\n\r");
	
	put_str("p Set PIN\n\r");
	put_str("t Set On/Off\n\r");
	
	put_str("Inputs: ");
	if( PORT_INPUTS.IN & INP1 ) put_char('0'); else put_char('1');
	if( PORT_INPUTS.IN & INP2 ) put_char('0'); else put_char('1');
	if( PORT_INPUTS.IN & INP3 ) put_char('0'); else put_char('1');
	if( PORT_INPUTS.IN & INP4 ) put_char('0'); else put_char('1');
	put_str("\n\r");
	
	put_str("\n\rTime ");
	put_char(((twiMaster.readData[6]>>4)&0xff)+'0');put_char(((twiMaster.readData[6])&0x0f)+'0'); put_char('-');
	put_char(((twiMaster.readData[5]>>4)&0x01)+'0');put_char(((twiMaster.readData[5])&0x0f)+'0'); put_char('-');
	put_char(((twiMaster.readData[4]>>4)&0x03)+'0');put_char(((twiMaster.readData[4])&0x0f)+'0'); put_char(' ');
	
	put_char(((twiMaster.readData[2]>>4)&0x03)+'0');put_char(((twiMaster.readData[2])&0x0f)+'0'); put_char(':');
	put_char(((twiMaster.readData[1]>>4)&0x07)+'0');put_char(((twiMaster.readData[1])&0x0f)+'0'); put_char(':');
	put_char(((twiMaster.readData[0]>>4)&0x07)+'0');put_char(((twiMaster.readData[0])&0x0f)+'0');
	
	sprintf(str," On %2X:%02X  Off %2X:%02X\n\r",psTimerOn->u8Hour,psTimerOn->u8Minute,psTimerOff->u8Hour,psTimerOff->u8Minute);
	put_str(str);
}

static uint8_t get_digits(void){
	uint8_t data,c;
					
	data = 0; 
	
	while(1){
		c = get_char();
		if( ( c >= '0' ) && ( c <='9' ) ){
			data<<=4;
			data+=c-'0';
			put_char(c);
		}else
			break;
	} 
	
	return data;
}

static void TestExtRAM(void)
{
	uint16_t i,error,address;
	uint16_t data;
	
	for( i = 0 ; i < 0x8000 ; i ++ ){
		//data = __far_mem_read(EXY_RAM_ADD+i);
		data = *(p_E_RAM+i);
		if( data )
			break;
	}
	if( i != 0x8000 ){
		char str[50];
		sprintf(str,"\n\rExernal RAM not clear at Address:0x%X",i);
		put_str(str);
	}
	
	error = 0;
	address = 0;
	for( i = 0 ; i < 0x8000 ; i ++ ){
		if( i & 0x01 )
			data = ((i>>8)&0xFF);
		else
			data = (i&0xff);
		//__far_mem_write(EXY_RAM_ADD+i,data);
		*(p_E_RAM+i) = data;
	}

	for( i = 0 ; i < 0x8000 ; i ++ ){
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
	
	if( error ){
		char str[50];
		sprintf(str,"\n\rExernal RAM ERRORS:%u at Address:0x%X\n\r",error , address);
		put_str(str);
	}else
		put_str("\n\rExternal RAM OK\n\r");
		
	for( i = 0 ; i < 0x8000 ; i ++ ){
		//__far_mem_write(EXY_RAM_ADD+i,0);
		*(p_E_RAM+i) = 0;
	}
}

uint8_t old_sec = 0;
uint8_t old_min = 0xff;

int ser0Put(char c,FILE *stream)
{
    if (c == '\n')
      ser0Put('\r',stream);
    put_char(c);
    return 0;
}

static FILE mystdout = FDEV_SETUP_STREAM(ser0Put, NULL,_FDEV_SETUP_WRITE);

void OnTimer(void){
	if( on_lamps == 0 ){
		on_lamps = 1;
		PORT_RELAY.OUTSET = RELAY1;
	}
}

void OffTimer(void){
	if( on_lamps == 1 ){
		on_lamps = 0;
		PORT_RELAY.OUTCLR = RELAY1;
	}
}

void main_loop(void){
	
	if( PORT_TSTLED.IN & TSTLED )
		PORT_TSTLED.OUTCLR = TSTLED;
	else
		PORT_TSTLED.OUTSET = TSTLED;
	{
		uint8_t reg = 0;
		TWI_MasterWriteRead(&twiMaster,	SLAVE_ADDRESS,&reg,	1,0);
		while (twiMaster.status != TWIM_STATUS_READY);
		{	
			int i;
			for( i=1 ; i ; i++ );
		}
		TWI_MasterWriteRead(&twiMaster,SLAVE_ADDRESS,	0,0,7);
		while (twiMaster.status != TWIM_STATUS_READY);
		if( old_sec != (twiMaster.readData[0]&0x7F) ){
			old_sec = (twiMaster.readData[0]&0x7F);
			time_1s ++;
		}
		if( old_min != (twiMaster.readData[1]&0x7F) ){
			uint8_t time[2],on[2],off[2];
			
			old_min = (twiMaster.readData[1]&0x7F);
			
			t_min_no_connect++;
			
			on[0]=psTimerOn->u8Hour; on[1]=psTimerOn->u8Minute;
			off[0]=psTimerOff->u8Hour; off[1]=psTimerOff->u8Minute;
			if( memcmp(on,off,2) != 0 ){
				time[0]=(twiMaster.readData[2]&0x3F); time[1]=(twiMaster.readData[1]&0x7F);
				if( memcmp(on,off,2) > 0 ){
					if( memcmp(time,on,2) >= 0 ){
						OnTimer();
					}else if( memcmp(off,time,2) > 0 ){
						OnTimer();
					}else{
						OffTimer();
					}
				}else if( (memcmp(time,on,2) >= 0) && (memcmp(time,off,2) < 0) ){
					OnTimer();
				}else{
					OffTimer();
				}
			}
		}
	}
}

void set_pin(void){
	char new_pin[5];
	char str[20];
	uint8_t i = 0;
	
	new_pin[0]=0;
	
	while( i < 4 ){
		char c;
		sprintf(str,"Set PIN : %s\n\r",new_pin);
		put_str(str);
		c = get_char();
		if( (c >='0') && (c<='9') ){
			new_pin[i++] = c;
			new_pin[i]=0;
		}
	}
	sprintf(str,"Set PIN : %s\n\r",new_pin);
	put_str(str);
	strcpy(pin,new_pin);
	
	EEPROM_WaitForNVM();
	EEPROM( 0 , 0 ) = pin[0];
	EEPROM( 0 , 1 ) = pin[1] ;
	EEPROM( 0 , 2 ) = pin[2] ;
	EEPROM( 0 , 3 ) = pin[3] ;
	
	EEPROM_ErasePage(0);
	EEPROM_SplitWritePage(0);
	EEPROM_WaitForNVM();

}

void get_pin(void){
	uint8_t i;
	
	EEPROM_FlushBuffer();
	EEPROM_EnableMapping();
	
	pin[0] = EEPROM( 0 , 0 );
	pin[1] = EEPROM( 0 , 1 );
	pin[2] = EEPROM( 0 , 2 );
	pin[3] = EEPROM( 0 , 3 );
	
	pin[4] = 0;

	for( i = 0 ; i < 4 ; i++ ){
		if( (pin[i] >='0') && (pin[i]<='9') )
			;
		else{
			set_pin();
			return;
		}
	}
}

#endif	//WIN32

void OnLamp(void){
#ifndef WIN32
	PORT_RELAY.OUTSET = RELAY1;
#endif
}

void OffLamp(void){
#ifndef WIN32
	PORT_RELAY.OUTCLR = RELAY1;
#endif
}

typedef struct
{
    uint8_t     u8Type;
    uint16_t    u16Length;
    uint8_t     u8Message[1024];
} sJennicModuleMsg;

static sJennicModuleMsg sIncomingMsg;

int verbosity = LOG_INFO;       /** Default log level */

int iResetCoordinator = 1;      /** Reset the coordinator at exit */

/** Main loop running flag */
volatile unsigned char bRunning = 1;


static long time_sec;
#define T_STATE_MASHINE 3


int main(void){
#ifndef WIN32
	//Init

	CLOCK_Init();
	//Disable JTAG
	CCPWrite( &MCU.MCUCR, MCU_JTAGD_bm );
	
	{		//Бави SPI заради програматора	
		int i;
		for( i=1 ; i ; i++ );
  }
	
	//External memory
	PORTH.OUTSET = 0x03;
	PORTH.DIRSET = 0x07;
	PORTJ.DIRCLR = 0x00;
	PORTK.DIRSET = 0xFF;
	
	//printf
	stdout = &mystdout;  
	
	// Initialize EBI. 
	EBI_Enable( EBI_SDDATAW_8BIT_gc,
	            EBI_LPCMODE_ALE1_gc,
	            EBI_SRMODE_NOALE_gc,	//EBI_SRMODE_ALE1_gc
	            EBI_IFMODE_3PORT_gc );

	// Initialize SRAM 
	EBI_EnableLPC( &EBI.CS0,               	// Chip Select 0. 
	                EBI_CS_ASPACE_64KB_gc, 	//  Address space. 
	                EXTERNAL_SRAM_START,    // Base address. 
	                EBI_CS_SRWS_0CLK_gc);//EBI_CS_SRWS_0CLK_gc );  // 0 wait states. EBI_CS_SRWS_7CLK_gc
	
	PORTQ.OUTSET = 0x01;	//Enable Ext RAM
	PORTQ.DIRSET = 0x01;

	PORT_RELAY.OUTCLR = RELAY1 | RELAY2 | RELAY3 | RELAY4;
	PORT_RELAY.DIRSET = RELAY1 | RELAY2 | RELAY3 | RELAY4;
	
	PORT_INPUTS.DIRCLR = INP1 | INP2 | INP3 | INP4;
	
	PORT_TSTLED.OUTCLR = TSTLED;
	PORT_TSTLED.DIRSET = TSTLED;
	
	PORT_GPRS_RST.OUTCLR = GPRS_RST;
	PORT_GPRS_RST.DIRSET = GPRS_RST;
	
	CRD2_UartInit();
	
	GPRS_UartInit();
	
	FTDI_UartInit();
	
		/* Initialize TWI master. */
	TWI_MasterInit(&twiMaster,
	               &TWIF,
	               TWI_MASTER_INTLVL_LO_gc,
	               TWI_BAUDSETTING);
	
	
	sei();	//__enable_interrupt();
	/* Enable low interrupt level in PMIC. */
	PMIC.CTRL |= PMIC_LOLVLEN_bm;
	
#endif	//WIN32
	
	get_pin();
	(*on_counters)++;
	
	verbosity = LOG_DEBUG;//LOG_ERR;//LOG_DEBUG;

	

	sModuleSetConfig.sModuleConfigV11.u8Region = CONFIG_DEFAULT_REGION;
	sModuleSetConfig.sModuleConfigV11.u8Channel = 17;//CONFIG_DEFAULT_CHANNEL;
	sModuleSetConfig.sModuleConfigV11.u16PanID = htons(0x1111);//CONFIG_DEFAULT_PAN_ID);
	sModuleSetConfig.sModuleConfigV11.u32NetworkID = htonl(0x11111111);//CONFIG_DEFAULT_NETWORK_ID;);
	sModuleSetConfig.sModuleConfigV11.u64NetworkPrefixMSB = htonl((CONFIG_DEFAULT_PREFIX >> 32) & 0xFFFFFFFF);
	sModuleSetConfig.sModuleConfigV11.u64NetworkPrefixLSB = htonl((CONFIG_DEFAULT_PREFIX >> 0) & 0xFFFFFFFF);

	memset(&sModuleSetConfig.sSecurityConfig.uAuthSchemeData.sRadiusPAP.sAuthServerIP, 0, sizeof(struct in6_addr));
	sModuleSetConfig.sSecurityConfig.uAuthSchemeData.sRadiusPAP.sAuthServerIP.u.Byte[0] = 0xfd;
	sModuleSetConfig.sSecurityConfig.uAuthSchemeData.sRadiusPAP.sAuthServerIP.u.Byte[1] = 0x04;
	sModuleSetConfig.sSecurityConfig.uAuthSchemeData.sRadiusPAP.sAuthServerIP.u.Byte[2] = 0x0b;
	sModuleSetConfig.sSecurityConfig.uAuthSchemeData.sRadiusPAP.sAuthServerIP.u.Byte[3] = 0xd3;
	sModuleSetConfig.sSecurityConfig.uAuthSchemeData.sRadiusPAP.sAuthServerIP.u.Byte[4] = 0x80;
	sModuleSetConfig.sSecurityConfig.uAuthSchemeData.sRadiusPAP.sAuthServerIP.u.Byte[5] = 0xe8;
	sModuleSetConfig.sSecurityConfig.uAuthSchemeData.sRadiusPAP.sAuthServerIP.u.Byte[7] = 0x01;
	sModuleSetConfig.sSecurityConfig.uAuthSchemeData.sRadiusPAP.sAuthServerIP.u.Byte[15] = 0x2;

	sModuleSetConfig.sSecurityConfig.eAuthScheme = htonl(E_AUTH_SCHEME_RADIUS_PAP);
	memset(&sModuleSetConfig.sSecurityConfig.sKey, 0, sizeof(struct in6_addr));
	sModuleSetConfig.sSecurityConfig.sKey.u.Byte[15] = 2;
#ifndef WIN32
	sModuleSetConfig.sSecurityConfig.sKey.u.Byte[12] = pin[0];
	sModuleSetConfig.sSecurityConfig.sKey.u.Byte[13] = pin[1];
	sModuleSetConfig.sSecurityConfig.sKey.u.Byte[14] = pin[2];
	sModuleSetConfig.sSecurityConfig.sKey.u.Byte[15] = pin[3];
#endif

	sModuleSetConfig.u8JenNetProfile = CONFIG_DEFAULT_PROFILE;
	sModuleSetConfig.iAntennaDiversity = 0;
	sModuleSetConfig.eRadioFrontEnd = E_FRONTEND_STANDARD_POWER;
	
    /* Wait up to five seconds. */
#ifdef WIN32
	if ((serial_open(15, 1000000) < 0) || (eTunDeviceOpen(13/*8*/,115200L) != E_TUN_OK))
  {
    goto finish;
  }
#endif

  eJennicModuleStart();
	time_sec = (long)time(NULL);
		
	while(1){
		
		main_loop();
		
		TunLoop();
		
    if(bSL_ReadMessage(&sIncomingMsg.u8Type, &sIncomingMsg.u16Length, sizeof(sIncomingMsg.u8Message), sIncomingMsg.u8Message)) {
      if (eJennicModuleProcessMessage(sIncomingMsg.u8Type, sIncomingMsg.u16Length, sIncomingMsg.u8Message) != E_MODULE_OK) {
        daemon_log(LOG_ERR, "Error communicating with border router module");
				//bRunning = FALSE;
				eJennicModuleStart();
      }
    }
			
    if (eTunDeviceReadPacket() != E_TUN_OK) {
			daemon_log(LOG_ERR, "Error handling tun packet");
		}
      
    // Select timeout 
		if ( (time(NULL) - time_sec) >= T_STATE_MASHINE ) {
			time_sec = (long)time(NULL);
			if (eJennicModuleStateMachine(1) != E_MODULE_OK){
				daemon_log(LOG_ERR, "Error communicating with border router module");
				//bRunning = FALSE;
				eJennicModuleStart();
			}
		}
			
		if(kb_hit()){
			char c = get_char();
			text();
			printf("Key %c\n\r",c);
			switch(c){
			case '1':
				GlobalSetUint8ByModuleID(0xFFFFFE04, 0x02, 1);
				break;
			case '2':
				GlobalSetUint8ByModuleID(0xFFFFFE04, 0x02, 127);
				break;
			case '3':
				GlobalSetUint8ByModuleID(0xFFFFFE04, 0x02, 255);
				break;
			case '4':
				if( PORT_RELAY.IN & RELAY1 )
					OffLamp();
				else
					OnLamp();
				break;
			case '5':
				if( PORT_GPRS_RST.IN & GPRS_RST )
					PORT_GPRS_RST.OUTCLR = GPRS_RST;
				else
					PORT_GPRS_RST.OUTSET = GPRS_RST;
				break;
			case '6':
				GPRS_put_str("0123456789ABCDEF");
				put_str("To GPRS Uart    sendet:0123456789ABCDEF\n\r");
				put_str("Press any key !\n\r");
				get_char();
				put_str("From GPRS uart recived:");
				while( GPRS_kb_hit() )
					put_char( GPRS_get_char() );
				put_str("\n\r");
				break;
			case '7':
				CRD2_put_str("FEDCBA9876543210");
				put_str("To CRD2 Uart    sendet:FEDCBA9876543210\n\r");
				put_str("Press any key !\n\r");
				get_char();
				put_str("From CRD2 uart recived:");
				while( CRD2_kb_hit() )
					put_char( CRD2_get_char() );
				put_str("\n\r");
				break;
			case '8':
				TestExtRAM();
				break;
			case '9':
				{
					uint8_t reg[3],data;
					
					put_str("\n\rSet Year:");
					data = get_digits();
					reg[0] = 6;	reg[1] = data;
					TWI_MasterWriteRead(&twiMaster,	SLAVE_ADDRESS,reg,2,0);		while (twiMaster.status != TWIM_STATUS_READY);
					
					put_str("\n\rSet Month:");
					data = get_digits();
					reg[0] = 5;	reg[1] = data&0x1F;
					TWI_MasterWriteRead(&twiMaster,	SLAVE_ADDRESS,reg,2,0);		while (twiMaster.status != TWIM_STATUS_READY);
					
					put_str("\n\rSet Date:");
					data = get_digits();
					reg[0] = 4;	reg[1] = data&0x3F;
					TWI_MasterWriteRead(&twiMaster,	SLAVE_ADDRESS,reg,2,0);		while (twiMaster.status != TWIM_STATUS_READY);
					
					put_str("\n\rSet Hour:");
					data = get_digits();
					reg[0] = 2;	reg[1] = data&0x3F; 	reg[2] = 0x08;	//Enable Vbat
					TWI_MasterWriteRead(&twiMaster,	SLAVE_ADDRESS,reg,3,0);		while (twiMaster.status != TWIM_STATUS_READY);
					
					put_str("\n\rSet Minute:");
					data = get_digits();
					reg[0] = 0;	 reg[1] = 0x80; reg[2] = data&0x7F;	//Enable RTC
					TWI_MasterWriteRead(&twiMaster,	SLAVE_ADDRESS,reg,3,0);		while (twiMaster.status != TWIM_STATUS_READY);
				}
				break;
			case '0':
				eJennicModuleStart();
				break;
			case 'p':
			case 'P':
				set_pin();
				break;
			case 't':
			case 'T':
				{
					put_str("\n\rSet On Hour:");
					psTimerOn->u8Hour = get_digits();
					put_str("\n\rSet On Minute:");
					psTimerOn->u8Minute = get_digits();
					put_str("\n\rSet Off Hour:");
					psTimerOff->u8Hour = get_digits();
					put_str("\n\rSet Off Minute:");
					psTimerOff->u8Minute = get_digits();
				}
				break;
			case 'a':
				key_a = 1;
				break;
			case 'b':
				key_b = 1;
				break;
			}
		}
	}
	
	return -1;
}