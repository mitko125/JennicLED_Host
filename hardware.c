#include "avr_compiler.h"
#include <clksys_driver.h>
#include "ebi_driver.h"
#include <eeprom_driver.h>

#include "CRD2_Uart.h"
#include "GPRS_Uart.h"
#include "FTDI_Uart.h"
#include "twi_master_driver.h"
#include "MODBUS_Master.h"
#include "EnergyMeter.h"

#include "log.h"
#include "def.h"
#include "sub.h"


#include <stdio.h>
//#include <stdlib.h>
#include <string.h>
#include "hardware.h"

#include "TunDevice.h"

#define TIME 50	//50uS
static uint8_t cou_1ms = 0;
unsigned int cou_1s = 0;

ISR(TCC0_OVF_vect)
{
	if(MODBUS_Master_timer_start_RTU < 255)	//da ne se prewarta
		MODBUS_Master_timer_start_RTU++;
		
	//if( PORT_TSTLED.IN & TSTLED )	PORT_TSTLED.OUTCLR = TSTLED;	else	PORT_TSTLED.OUTSET = TSTLED;
		
	if( cou_1ms == 20 ){
		cou_1ms = 0;
			
		if( time_sleep_SIM )
			time_sleep_SIM--;
			
		if(	 cou_1s == 1000 ){
			cou_1s = 0;
			
			if( ENERGY_METER_time_wait_s )
				ENERGY_METER_time_wait_s --;
				
			if( ENERGY_METER_time_ERR_s < 200 )
				ENERGY_METER_time_ERR_s ++;
				
		}else
			cou_1s ++;
	}else
		cou_1ms ++ ;
}

int ser0Put(char c,FILE *stream)
{
    if (c == '\n')
      ser0Put('\r',stream);
    put_char(c);
    return 0;
}

static FILE mystdout = FDEV_SETUP_STREAM(ser0Put, NULL,_FDEV_SETUP_WRITE);

TWI_Master_t twiMaster;    /*!< TWI master module. */
/*! CPU speed 2MHz, BAUDRATE 100kHz and Baudrate Register Settings */
//#define CPU_SPEED       2000000
#define BAUDRATE	100000	//100000
#define TWI_BAUDSETTING TWI_BAUD(F_CPU, BAUDRATE)


/*! TWIC Master Interrupt vector. */
ISR(TWIF_TWIM_vect)
{
	TWI_MasterInterruptHandler(&twiMaster);
}


void InitHardware(void){
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
	
	ENERGY_METER_init();
	
	//прекъсване на 50us
  TCC0.PER = (uint16_t)((((F_CPU/64)*TIME)/1000000)-1);
	TCC0.CTRLA = ( TCC0.CTRLA & ~TC0_CLKSEL_gm ) | TC_CLKSEL_DIV64_gc;
	TCC0.INTCTRLA = TC_OVFINTLVL_LO_gc;
	
	sei();	//__enable_interrupt();
	/* Enable low interrupt level in PMIC. */
	PMIC.CTRL |= PMIC_LOLVLEN_bm;
}

void set_pin(void){
	char new_pin[5];
	uint8_t i = 0;
	
	new_pin[0]=0;
	
	while( i < 4 ){
		char c;
		printf_P(PSTR("Set PIN : %s\n\r"),new_pin);
		c = get_char();
		if( (c >='0') && (c<='9') ){
			new_pin[i++] = c;
			new_pin[i]=0;
		}
	}
	printf_P(PSTR("Set PIN : %s\n\r"),new_pin);
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

static unsigned char clock_set = 0;

void get_time(void){
	uint8_t reg = 0;
	
	if( PORT_TSTLED.IN & TSTLED )
		PORT_TSTLED.OUTCLR = TSTLED;
	else
		PORT_TSTLED.OUTSET = TSTLED;
		
	TWI_MasterWriteRead(&twiMaster,	SLAVE_ADDRESS,&reg,	1,0);
	while (twiMaster.status != TWIM_STATUS_READY);
	{	
		int i;
		for( i=1 ; i ; i++ );
	}
	TWI_MasterWriteRead(&twiMaster,SLAVE_ADDRESS,	0,0,7);
	while (twiMaster.status != TWIM_STATUS_READY);
	
	if( ( twiMaster.readData[0] & 0x80 ) == 0 ){
		error_clock = 1;
		daemon_log(LOG_ERR, "Clock is reset");
	}
	date_time[5] = twiMaster.readData[6];
	date_time[4] = twiMaster.readData[5] & 0x1F;
	date_time[3] = twiMaster.readData[4] & 0x3F;
	date_time[2] = twiMaster.readData[2] & 0x3F;
	date_time[1] = twiMaster.readData[1] & 0x7F;
	date_time[0] = twiMaster.readData[0] & 0x7F;
	//printf_P(PSTR("%02X %02X %02X %02X %02X %02X %02X"),twiMaster.readData[0],twiMaster.readData[1],twiMaster.readData[2],twiMaster.readData[3],twiMaster.readData[4],twiMaster.readData[5],twiMaster.readData[6]);
	//get_char();
	
	if( error_clock && ( clock_set == 0 ) ){
		uint8_t reg[3];
		
		clock_set = 1;
		
		{	int i;	for( i=1 ; i ; i++ ); }	{	int i;	for( i=1 ; i ; i++ ); }
		reg[0] = 6;	reg[1] = 0x15;
		TWI_MasterWriteRead(&twiMaster,	SLAVE_ADDRESS,reg,2,0);		while (twiMaster.status != TWIM_STATUS_READY);
		
		{	int i;	for( i=1 ; i ; i++ ); }	{	int i;	for( i=1 ; i ; i++ ); }
		reg[0] = 5;	reg[1] = 0x08;
		TWI_MasterWriteRead(&twiMaster,	SLAVE_ADDRESS,reg,2,0);		while (twiMaster.status != TWIM_STATUS_READY);
		
		{	int i;	for( i=1 ; i ; i++ ); }	{	int i;	for( i=1 ; i ; i++ ); }
		reg[0] = 4;	reg[1] = 0x01;
		TWI_MasterWriteRead(&twiMaster,	SLAVE_ADDRESS,reg,2,0);		while (twiMaster.status != TWIM_STATUS_READY);
			
		{	int i;	for( i=1 ; i ; i++ ); }	{	int i;	for( i=1 ; i ; i++ ); }
		reg[0] = 2;	reg[1] = 0x08; 	reg[2] = 0x09;	//Enable Vbat
		TWI_MasterWriteRead(&twiMaster,	SLAVE_ADDRESS,reg,3,0);		while (twiMaster.status != TWIM_STATUS_READY);
		
		{	int i;	for( i=1 ; i ; i++ ); }	{	int i;	for( i=1 ; i ; i++ ); }
		reg[0] = 0;	 reg[1] = 0x80; reg[2] = 0x33;	//Enable RTC
		TWI_MasterWriteRead(&twiMaster,	SLAVE_ADDRESS,reg,3,0);		while (twiMaster.status != TWIM_STATUS_READY);
		
		{	int i;	for( i=1 ; i ; i++ ); }	{	int i;	for( i=1 ; i ; i++ ); }
	}
}

void SetDateTime(void){
	{
		uint8_t reg[3];
		
		clock_set = 1;
		
		{	int i;	for( i=1 ; i ; i++ ); }	{	int i;	for( i=1 ; i ; i++ ); }
		reg[0] = 6;	reg[1] = psTimers->sDateTime.date_time[5];
		TWI_MasterWriteRead(&twiMaster,	SLAVE_ADDRESS,reg,2,0);		while (twiMaster.status != TWIM_STATUS_READY);
		
		{	int i;	for( i=1 ; i ; i++ ); }	{	int i;	for( i=1 ; i ; i++ ); }
		reg[0] = 5;	reg[1] = psTimers->sDateTime.date_time[4];
		TWI_MasterWriteRead(&twiMaster,	SLAVE_ADDRESS,reg,2,0);		while (twiMaster.status != TWIM_STATUS_READY);
		
		{	int i;	for( i=1 ; i ; i++ ); }	{	int i;	for( i=1 ; i ; i++ ); }
		reg[0] = 4;	reg[1] = psTimers->sDateTime.date_time[3];
		TWI_MasterWriteRead(&twiMaster,	SLAVE_ADDRESS,reg,2,0);		while (twiMaster.status != TWIM_STATUS_READY);
			
		{	int i;	for( i=1 ; i ; i++ ); }	{	int i;	for( i=1 ; i ; i++ ); }
		reg[0] = 2;	reg[1] = psTimers->sDateTime.date_time[2]; 	reg[2] = 0x09;	//Enable Vbat
		TWI_MasterWriteRead(&twiMaster,	SLAVE_ADDRESS,reg,3,0);		while (twiMaster.status != TWIM_STATUS_READY);
		
		{	int i;	for( i=1 ; i ; i++ ); }	{	int i;	for( i=1 ; i ; i++ ); }
		reg[0] = 0;	 reg[1] = 0x80; reg[2] = psTimers->sDateTime.date_time[1];	//Enable RTC
		TWI_MasterWriteRead(&twiMaster,	SLAVE_ADDRESS,reg,3,0);		while (twiMaster.status != TWIM_STATUS_READY);
		
		{	int i;	for( i=1 ; i ; i++ ); }	{	int i;	for( i=1 ; i ; i++ ); }
	}
	error_clock = 0;
}