
#if __BYTE_ORDER == __BIG_ENDIAN
#define htons(a)	(a)
#define htonl(a)	(a)
#define ntohs(a)	(a)
#define ntohl(a)	(a)
#else
#error Little endian not implemented
#endif /// BIG ENDIAN 

//FTDI uart
#define FTDI_uart USARTE0
#define FTDI_Port PORTE
#define	FTDI_RX_vect USARTE0_RXC_vect
#define FTDI_TX_vect	USARTE0_TXC_vect
#define FTDI_DRE_vect	USARTE0_DRE_vect
#define FTDI_TX_PIN PIN3_bm
#define FTDI_RX_PIN PIN2_bm
//#define FTDI_DIR_PIN PIN1_bm
#define FTDI_BOUDE 115200UL


//GPRS SIM 900
#define PORT_GPRS_RST	PORTC
#define GPRS_RST PIN5_bm

#define GPRS_uart USARTC1
#define GPRS_Port PORTC
#define	GPRS_RX_vect USARTC1_RXC_vect
#define GPRS_TX_vect	USARTC1_TXC_vect
#define GPRS_DRE_vect	USARTC1_DRE_vect
#define GPRS_TX_PIN PIN7_bm
#define GPRS_RX_PIN PIN6_bm
//#define GPRS_DIR_PIN PIN1_bm
#define GPRS_BOUDE 115200UL 

//COORDINATOR 2 uart JP4
#define CRD2_uart USARTD0
#define CRD2_Port PORTD
#define	CRD2_RX_vect USARTD0_RXC_vect
#define CRD2_TX_vect	USARTD0_TXC_vect
#define CRD2_DRE_vect	USARTD0_DRE_vect
#define CRD2_TX_PIN PIN3_bm
#define CRD2_RX_PIN PIN2_bm
//#define CRD2_DIR_PIN PIN0_bm
#define CRD2_BOUDE 115200UL 

//RS485 uart JP8
#define RS485_uart USARTF0
#define RS485_Port PORTF
#define	RS485_RX_vect USARTF0_RXC_vect
#define RS485_TX_vect	USARTF0_TXC_vect
#define RS485_DRE_vect	USARTF0_DRE_vect
#define RS485_TX_PIN PIN3_bm
#define RS485_RX_PIN PIN2_bm
#define RS485_DIR_PIN PIN4_bm
#define RS485_BOUDE 115200UL 

//Relay
#define PORT_RELAY	PORTA
#define RELAY1 PIN4_bm
#define RELAY2 PIN5_bm
#define RELAY3 PIN6_bm
#define RELAY4 PIN7_bm

//Inputs
#define PORT_INPUTS	PORTB
#define INP1 PIN4_bm
#define INP2 PIN5_bm
#define INP3 PIN6_bm
#define INP4 PIN7_bm

//Test LED
#define PORT_TSTLED	PORTD
#define TSTLED PIN1_bm

//External RAM
#define p_E_RAM 	((unsigned char *)(EXTERNAL_SRAM_START))	