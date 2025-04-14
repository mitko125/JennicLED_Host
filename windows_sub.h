#define kb_hit _kbhit
#define get_char _getch
#define put_char putchar

extern unsigned char * p_E_RAM;

extern uint8_t t_min_no_connect;
extern uint8_t client_number;
extern uint8_t data_to_server;

void get_time(void);
void InitHardware(void);
void save_RAM(void);
void SetDateTime(void);
void ENERGY_METER_init(void);
void ENERGY_METER_Loop(void);
void ResetSIM(void);
uint8_t StartWinMyThread(void);
void SendPacage(int len);
void SimLoop(void);