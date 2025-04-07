#include <inttypes.h> 

void FTDI_UartInit(void);

void put_char(uint8_t data);
uint8_t get_char(void);
uint8_t kb_hit(void);
void put_str(char *data);