#include "avr_compiler.h"
#include "MODBUS_Master.h"
#include "usart.h"
#include "modbus_defs.h"
#include "def.h"

#include <hardware.h>


uint8_t *ptr_to_data_read;
uint16_t lenght_read;

//#define MAX_HOLDING_REGISTER 100

//unsigned int holding_registers[MAX_HOLDING_REGISTER];

static uint16_t AddressReadHoldingRegisters=0;
static uint8_t modbus_address;



volatile uint8_t MODBUS_Master_timer_start_RTU; //отчита мълчание по линията за 3.5байта (3.5*520=301 +50 =351uS/50=7)
//75	9600
//7		115200
#define RTU_TIMEOUT	75	//6

/* \brief  Receive buffer size: 2,4,8,16,32,64,128 or 256 bytes. */
//#define MODBUS_RX_BUFFER_SIZE 512
/* \brief Transmit buffer size: 2,4,8,16,32,64,128 or 256 bytes */
//#define MODBUS_TX_BUFFER_SIZE 512
/* \brief Receive buffer mask. */
#define USART_RX_BUFFER_MASK ( MODBUS_RX_BUFFER_SIZE - 1 )
/* \brief Transmit buffer mask. */
#define USART_TX_BUFFER_MASK ( MODBUS_TX_BUFFER_SIZE - 1 )


#if ( MODBUS_RX_BUFFER_SIZE & USART_RX_BUFFER_MASK )
#error u8MODBUS_RX_Buf buffer size is not a power of 2
#endif
#if ( MODBUS_TX_BUFFER_SIZE & USART_TX_BUFFER_MASK )
#error u8MODBUS_TX_Buf buffer size is not a power of 2
#endif


/* \brief Receive buffer. */
//static volatile uint8_t u8MODBUS_RX_Buf[USART_RX_BUFFER_SIZE];
/* \brief Transmit buffer. */
//static volatile uint8_t u8MODBUS_TX_Buf[USART_TX_BUFFER_SIZE];
/* \brief Receive buffer head. */
static volatile uint8_t RX_Head;
/* \brief Receive buffer tail. */
static volatile uint8_t RX_Tail;
/* \brief Transmit buffer head. */
static volatile uint8_t TX_Head;
/* \brief Transmit buffer tail. */
static volatile uint8_t TX_Tail;


static uint8_t kb_hit(void){
	return (RX_Head != RX_Tail);
}

static uint8_t get_char(void){
	uint8_t ans;

	while( RX_Head == RX_Tail);
	ans = (u8MODBUS_RX_Buf[RX_Tail]);

	/* Advance buffer tail. */
	RX_Tail = (RX_Tail + 1) & USART_RX_BUFFER_MASK;

	return ans;
}

/*! \brief Test if there is data in the transmitter software buffer.
 *
 *  This function can be used to test if there is free space in the transmitter
 *  software buffer.
 *
 *  \param usart_data The USART_data_t struct instance.
 *
 *  \retval true      There is data in the receive buffer.
 *  \retval false     The receive buffer is empty.
 */
static bool USART_TXBuffer_FreeSpace(void)
{
	/* Make copies to make sure that volatile access is specified. */
	uint8_t tempHead = (TX_Head + 1) & USART_TX_BUFFER_MASK;
	uint8_t tempTail = TX_Tail;

	/* There are data left in the buffer unless Head and Tail are equal. */
	return (tempHead != tempTail);
}

static void put_char(uint8_t data){
	
	uint8_t tempTX_Head;

	while( !USART_TXBuffer_FreeSpace());

  u8MODBUS_TX_Buf[tempTX_Head=TX_Head]= data;
	/* Advance buffer head. */
	TX_Head = (tempTX_Head + 1) & USART_TX_BUFFER_MASK;

	/* Enable DRE interrupt. */
	RS485_uart.CTRLA = (RS485_uart.CTRLA & ~USART_DREINTLVL_gm) | USART_DREINTLVL_LO_gc;
}

/*! \brief Receive complete interrupt service routine.
 */
ISR(RS485_RX_vect)
{
	uint8_t tempRX_Head = (RX_Head + 1) & USART_RX_BUFFER_MASK;

	/* Check for overflow. */
	uint8_t data = RS485_uart.DATA;

	if (tempRX_Head == RX_Tail) {
	  ;
	}else{
		u8MODBUS_RX_Buf[RX_Head] = data;
		RX_Head = tempRX_Head;
	}
}

ISR(RS485_TX_vect)
{
	RS485_Port.OUTCLR = RS485_DIR_PIN;
}

/*! \brief Data register empty  interrupt service routine.
 */
ISR(RS485_DRE_vect)
{
	if (TX_Head == TX_Tail){
	    /* Disable DRE interrupts. */
		RS485_uart.CTRLA = (RS485_uart.CTRLA & ~USART_DREINTLVL_gm) | USART_DREINTLVL_OFF_gc;
	}else{
		RS485_Port.OUTSET = RS485_DIR_PIN;
		/* Start transmitting. */
		RS485_uart.DATA = u8MODBUS_TX_Buf[TX_Tail];

		/* Advance buffer tail. */
		TX_Tail = (TX_Tail + 1) & USART_TX_BUFFER_MASK;
	}
}

static uint16_t counter_data;
static uint16_t calc_checksum_lsb;  //shiftwa se beznakowo
static uint8_t flag_command;
static uint8_t ena_protokol = 0;
static uint8_t bufer[MAX_BUFFER_SIZE];
static uint8_t *ptr_to_data_in;
static uint8_t *ptr_to_data_out;
static uint16_t lenght_out;

void MODBUS_Master_init(void){
	{
		/* This PORT setting is only valid to USARTC0 if other USARTs is used a
		* different PORT and/or pins is used. */
		/* PIN3 (TXD0) as output. */
		RS485_Port.DIRSET = RS485_TX_PIN;

		/* PC2 (RXD0) as input. */
		RS485_Port.DIRCLR = RS485_RX_PIN;
	
		/* PC1 (DIR) as output */
		RS485_Port.OUTCLR = RS485_DIR_PIN;
		RS485_Port.DIRSET = RS485_DIR_PIN;

		/* USARTC0, 8 Data bits, No Parity, 1 Stop bit. */
		USART_Format_Set(&RS485_uart, USART_CHSIZE_8BIT_gc, USART_PMODE_EVEN_gc, false);
		USART_Baudrate_Set(&RS485_uart,(uint16_t)((F_CPU/16)/RS485_BOUDE-1), 0);
	
		/* Enable RXC interrupt. */
		RS485_uart.CTRLA = USART_RXCINTLVL_LO_gc | USART_TXCINTLVL_LO_gc;

		/* Enable both u8MODBUS_RX_Buf and u8MODBUS_TX_Buf. */
		RS485_uart.CTRLB = 0;
		USART_Rx_Enable(&RS485_uart);
		USART_Tx_Enable(&RS485_uart);
	}
}

static void Send(void){

	calc_checksum_lsb = 0xFFFF;
	ptr_to_data_out = bufer+MBAP_HEADER_SIZE;
	
	while( lenght_out ){
		uint8_t temp;
		lenght_out --;
		temp = *ptr_to_data_out;	//i1; m1 = 1
		ptr_to_data_out++;
		{	//MB_RTU_PROTOCOL
			put_char(temp);
			calc_checksum_lsb ^=temp;
			{
				int j;
				for (j=0; j<8; j++){
					if(calc_checksum_lsb & 0x1){
						calc_checksum_lsb >>=1;
						calc_checksum_lsb ^=0xA001;
					}else{
						calc_checksum_lsb >>=1;
					}			
				}
			}
		}
	}
	put_char(calc_checksum_lsb);
	put_char(calc_checksum_lsb>>8);
	
	calc_checksum_lsb = 0xFFFF;
	counter_data = 0;
  flag_command = 0;
  ena_protokol = 0;
  ptr_to_data_in = bufer + MBAP_HEADER_SIZE;
}

void WriteReadHldingRegisters(uint8_t SlaveAddress,uint16_t WriteAddress,uint16_t WriteSize,uint16_t * WriteBufer,uint16_t ReadAddress,uint16_t ReadSize){
	AddressReadHoldingRegisters = ReadAddress;

	bufer[MBAP_HEADER_SIZE] = modbus_address = SlaveAddress;
		
	bufer[1+MBAP_HEADER_SIZE] = MBF_READ_WRITE_MULTIPLE_REGISTERS;
	
	bufer[2+MBAP_HEADER_SIZE] = ReadAddress>>8;
	bufer[3+MBAP_HEADER_SIZE] = ReadAddress&0xFF;
	bufer[4+MBAP_HEADER_SIZE] = ReadSize>>8;
	bufer[5+MBAP_HEADER_SIZE] = ReadSize&0xFF;
	bufer[6+MBAP_HEADER_SIZE] = WriteAddress>>8;
	bufer[7+MBAP_HEADER_SIZE] = WriteAddress&0xFF;
	bufer[8+MBAP_HEADER_SIZE] = WriteSize>>8;
	bufer[9+MBAP_HEADER_SIZE] = WriteSize&0xFF;
	bufer[10+MBAP_HEADER_SIZE] = WriteSize<<1;
	lenght_out = 6+4+1;
	{
		int i=0;
		uint8_t * p = bufer + 11 + MBAP_HEADER_SIZE;
		for(i = 0 ; i < WriteSize ; i++){
			uint16_t temp;
			AVR_ENTER_CRITICAL_REGION( );
			temp=*WriteBufer;
			AVR_LEAVE_CRITICAL_REGION( );
			WriteBufer++;
			lenght_out += 2;
			*p++ = temp >> 8;
			*p++ = temp & 0xff;
		}
	}
	Send();
}


void ReadHldingRegisters(uint8_t SlaveAddress,uint16_t Address,uint16_t Size){

	AddressReadHoldingRegisters = Address;

	bufer[MBAP_HEADER_SIZE] = modbus_address = SlaveAddress;
		
	bufer[1+MBAP_HEADER_SIZE] = MBF_READ_HOLDING_REGISTERS;
	bufer[2+MBAP_HEADER_SIZE] = AddressReadHoldingRegisters>>8;
	bufer[3+MBAP_HEADER_SIZE] = AddressReadHoldingRegisters&0xFF;
	bufer[4+MBAP_HEADER_SIZE] = Size>>8;
	bufer[5+MBAP_HEADER_SIZE] = Size&0xFF;
	lenght_out = 6;
	
	Send();
}

void MODBUS_Master_Loop(void){
	uint8_t AX1; //simulator na registr
	
	while( kb_hit() ){
		AX1=get_char();
		
		if( ena_protokol == 0 ){
			if(MODBUS_Master_timer_start_RTU>RTU_TIMEOUT){	//6*60 = 360 mikro sec >4 byte 115200
				MODBUS_Master_timer_start_RTU=0;
				//start_condition MB_RTU_PROTOCOL
				calc_checksum_lsb = 0xFFFF;
				counter_data = 0;
				flag_command = 0;
				ena_protokol = 1;
				ptr_to_data_in = bufer + MBAP_HEADER_SIZE;
				goto handled_slave_data;		//zapazi za izwikwaneto AX1
			}
		}
		MODBUS_Master_timer_start_RTU=0;
		if(ena_protokol)
			goto handled_slave_data;
		return;
handled_slave_data:
		*ptr_to_data_in=AX1;
		if( ptr_to_data_in < (bufer + sizeof(bufer) -1 ) ){
			ptr_to_data_in++;
			if(counter_data>=2){
				calc_checksum_lsb ^=bufer[counter_data-2+MBAP_HEADER_SIZE];
				{
					int j;
					for (j=0; j<8; j++){
      			if(calc_checksum_lsb & 0x1){
							calc_checksum_lsb >>=1;
        			calc_checksum_lsb ^=0xA001;
						}else{
							calc_checksum_lsb >>=1;
						}			
					}
    		}
			}
		}else{	//ERROR_OVERFLOV BUFER 
			ena_protokol=0;
		}
		counter_data++;
	}

	if(ena_protokol){
		if(MODBUS_Master_timer_start_RTU>RTU_TIMEOUT){	//6*60 = 360 mikro sec >4 byte 115200
			ena_protokol=0;
			if(counter_data>2){
				if((calc_checksum_lsb&0x0ff) == bufer[counter_data-2+MBAP_HEADER_SIZE]){
					if(((calc_checksum_lsb>>8)&0x0ff) == bufer[counter_data-1+MBAP_HEADER_SIZE]){
						ptr_to_data_in--;
						goto end_condition;
					}
				}
			}
		}
	}
	return;


end_condition:
	if(bufer[MBAP_HEADER_SIZE]==modbus_address)
		goto my_recived_mesage;
	return;
my_recived_mesage:
	switch(bufer[1+MBAP_HEADER_SIZE]){
		case MBF_READ_WRITE_MULTIPLE_REGISTERS:
		case MBF_READ_HOLDING_REGISTERS:
			{
				uint8_t lenght = bufer[2+MBAP_HEADER_SIZE];
				if(ptr_to_data_in == (bufer+4+MBAP_HEADER_SIZE+lenght)){
					//uint16_t *p = holding_registers + AddressReadHoldingRegisters;
					ptr_to_data_read = ptr_to_data_in = bufer+3+MBAP_HEADER_SIZE;
					if( lenght&0x01 )
						return;
					lenght_read = lenght;
			/*		while(lenght){
						lenght -= 2;
						unsigned int temp = ((*ptr_to_data_in++)<<8);
						temp |= ((*ptr_to_data_in++)&0xFF);
						AVR_ENTER_CRITICAL_REGION( );
						*p = temp;
						AVR_LEAVE_CRITICAL_REGION( );
						p++;
					}*/
					goto ok_reciv;
				}
			}
	}
	return;
ok_reciv:
	MODBUS_Maser_Ok_reciv();
	return;
}