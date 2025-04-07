
void GPRS_UartInit(void);

void GPRS_put_char(uint8_t data);
uint8_t GPRS_get_char(void);
uint8_t GPRS_kb_hit(void);
void GPRS_put_str(char *data);

int sim_serial_write(const unsigned char data);
int sim_serial_read(unsigned char *data);