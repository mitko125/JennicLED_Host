#include "avr_compiler.h"
#include <clksys_driver.h>
#include "ebi_driver.h"
#include "CRD2_Uart.h"
#include "GPRS_Uart.h"
#include "FTDI_Uart.h"
#include "twi_master_driver.h"
#include "defs.h"
#include <stdio.h>

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

	put_str("\n\rJennic HOST Hardware test\n\r");
	
	put_str("1,2,3,4 Test RELAYS\n\r");
		
	put_str("5 GPRS JP6 RST\n\r");
	put_str("6 Test GPRS Uart JP6\n\r");
	put_str("7 Test CRD2 Uart JP4\n\r");
	put_str("8 Test External RAM\n\r");
	
	put_str("9 Set time & data\n\r");
	
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
	put_str("\n\r");
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

int main(void){
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
	
	
	
	/*{	//Enable RTC
			uint8_t reg[2];
			reg[0] = 0;
			reg[1] = 0x80;
			
			TWI_MasterWriteRead(&twiMaster,	SLAVE_ADDRESS,reg,2,0);


			while (twiMaster.status != TWIM_STATUS_READY) {
				// Wait until transaction is complete. 
			}
	}
	*/
	while(1){
		
		if( PORT_TSTLED.IN & TSTLED )
			PORT_TSTLED.OUTCLR = TSTLED;
		else
			PORT_TSTLED.OUTSET = TSTLED;
		{
			uint8_t reg = 0;
			TWI_MasterWriteRead(&twiMaster,	SLAVE_ADDRESS,&reg,	1,0);
			while (twiMaster.status != TWIM_STATUS_READY) {
				/* Wait until transaction is complete. */
			}
			{	
				int i;
				for( i=1 ; i ; i++ );
			}
			TWI_MasterWriteRead(&twiMaster,SLAVE_ADDRESS,	0,0,7);
			while (twiMaster.status != TWIM_STATUS_READY) {
				/* Wait until transaction is complete. */
			}
		}
		
		text();
		
		switch(get_char()){
			case '1':
				if( PORT_RELAY.IN & RELAY1 )
					PORT_RELAY.OUTCLR = RELAY1;
				else
					PORT_RELAY.OUTSET = RELAY1;
				{		//Бави заради inputs	
					int i;
					for( i=1 ; i ; i++ );
				}
				break;
			case '2':
				if( PORT_RELAY.IN & RELAY2 )
					PORT_RELAY.OUTCLR = RELAY2;
				else
					PORT_RELAY.OUTSET = RELAY2;
				{		//Бави заради inputs	
					int i;
					for( i=1 ; i ; i++ );
				}
				break;
			case '3':
				if( PORT_RELAY.IN & RELAY3 )
					PORT_RELAY.OUTCLR = RELAY3;
				else
					PORT_RELAY.OUTSET = RELAY3;
				{		//Бави заради inputs	
					int i;
					for( i=1 ; i ; i++ );
				}
				break;
			case '4':
				if( PORT_RELAY.IN & RELAY4 )
					PORT_RELAY.OUTCLR = RELAY4;
				else
					PORT_RELAY.OUTSET = RELAY4;
				{		//Бави заради inputs	
					int i;
					for( i=1 ; i ; i++ );
				}
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
		}
	}
	
	return -1;
}