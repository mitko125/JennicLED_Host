
void RS485_UartInit(void);

void RS485_put_char(uint8_t data);
uint8_t RS485_get_char(void);
uint8_t RS485_kb_hit(void);
void RS485_put_str(char *data);