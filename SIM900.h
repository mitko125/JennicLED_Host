#ifndef  SIM900_H_INCLUDED
#define  SIM900_H_INCLUDED

extern uint8_t key_a,key_b,key_c;
extern uint8_t client_number;
extern uint8_t data_to_server;
void PrintSimState(void);
void SetSimState(uint8_t new_state);
void SendOnlyCommand(uint8_t new_state);
void SendTextCommand(uint8_t *text);
void ResetSIM(void);
extern uint8_t t_min_no_connect;
extern volatile uint16_t time_sleep_SIM;

void SimLoop(void);
void SendPacage(int len);
void SendPacageToPC_Client(int len);

#endif //SIM900_H_INCLUDED