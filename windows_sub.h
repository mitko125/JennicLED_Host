#define kb_hit _kbhit
#define get_char _getch
#define put_char putchar

extern unsigned char * p_E_RAM;

void get_time(void);
void InitHardware(void);
void save_RAM(void);
void SetDateTime(void);