extern unsigned char date_time[];
extern unsigned char error_clock;
extern uint8_t on_relay;

void text(void);
void main_loop(void);
void OnLamp(void);
void OffLamp(void);
uint8_t get_digits(void);
void TestExtRAM(void);
void ClearRam(void);
void make_crc(unsigned char *p, int len);
unsigned char check_crc(unsigned char *p, int len);
void ProcesNetworkRouterTable(uint8_t * pu8Data, int16_t i16Lenght);
void TestNetworkRouterTable(void);
void TestSubTreeNodes(void);