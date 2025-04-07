#include "avr_compiler.h"
#include "RS485_Uart.h"
#include "usart.h"


#include <defs.h>


/* \brief  Receive buffer size: 2,4,8,16,32,64,128 or 256 bytes. */
#define USART_RX_BUFFER_SIZE 256
/* \brief Transmit buffer size: 2,4,8,16,32,64,128 or 256 bytes */
#define USART_TX_BUFFER_SIZE 256
/* \brief Receive buffer mask. */
#define USART_RX_BUFFER_MASK ( USART_RX_BUFFER_SIZE - 1 )
/* \brief Transmit buffer mask. */
#define USART_TX_BUFFER_MASK ( USART_TX_BUFFER_SIZE - 1 )


#if ( USART_RX_BUFFER_SIZE & USART_RX_BUFFER_MASK )
#error RX buffer size is not a power of 2
#endif
#if ( USART_TX_BUFFER_SIZE & USART_TX_BUFFER_MASK )
#error TX buffer size is not a power of 2
#endif


/* \brief Receive buffer. */
static volatile uint8_t RX[USART_RX_BUFFER_SIZE];
/* \brief Transmit buffer. */
static volatile uint8_t TX[USART_TX_BUFFER_SIZE];
/* \brief Receive buffer head. */
static volatile uint8_t RX_Head;
/* \brief Receive buffer tail. */
static volatile uint8_t RX_Tail;
/* \brief Transmit buffer head. */
static volatile uint8_t TX_Head;
/* \brief Transmit buffer tail. */
static volatile uint8_t TX_Tail;



uint8_t RS485_kb_hit(void){
	return (RX_Head != RX_Tail);
}

uint8_t RS485_get_char(void){
	uint8_t ans;

	while( RX_Head == RX_Tail);
	ans = (RX[RX_Tail]);

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

void RS485_put_char(uint8_t data){
	
	uint8_t tempTX_Head;

	while( !USART_TXBuffer_FreeSpace());

  TX[tempTX_Head=TX_Head]= data;
	/* Advance buffer head. */
	TX_Head = (tempTX_Head + 1) & USART_TX_BUFFER_MASK;

	/* Enable DRE interrupt. */
	RS485_uart.CTRLA = (RS485_uart.CTRLA & ~USART_DREINTLVL_gm) | USART_DREINTLVL_LO_gc;
}

void RS485_put_str(char *data){
	while( *data )
		RS485_put_char( *data++ );
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
		RX[RX_Head] = data;
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
		RS485_uart.DATA = TX[TX_Tail];

		/* Advance buffer tail. */
		TX_Tail = (TX_Tail + 1) & USART_TX_BUFFER_MASK;
	}
}

void RS485_UartInit(void){
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
		USART_Format_Set(&RS485_uart, USART_CHSIZE_8BIT_gc, USART_PMODE_DISABLED_gc, false);
		USART_Baudrate_Set(&RS485_uart,(uint16_t)((F_CPU/16)/RS485_BOUDE-1), 0);
	
		/* Enable RXC interrupt. */
		RS485_uart.CTRLA = USART_RXCINTLVL_LO_gc | USART_TXCINTLVL_LO_gc;

		/* Enable both RX and TX. */
		RS485_uart.CTRLB = 0;
		USART_Rx_Enable(&RS485_uart);
		USART_Tx_Enable(&RS485_uart);
	}
}