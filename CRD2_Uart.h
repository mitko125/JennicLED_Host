
void CRD2_UartInit(void);

int serial_write(uint8_t data);
int serial_read(unsigned char *data);
uint8_t CRD2_get_char(void);
uint8_t CRD2_kb_hit(void);
void CRD2_put_str(char *data);