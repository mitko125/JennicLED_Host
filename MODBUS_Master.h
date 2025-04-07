#ifndef MODBUS_MASTER_HEADER
#define MODBUS_MASTER_HEADER

extern volatile uint8_t MODBUS_Master_timer_start_RTU;

extern uint8_t *ptr_to_data_read;
extern uint16_t lenght_read;

void MODBUS_Master_init(void);
void MODBUS_Master_Loop(void);

void ReadHldingRegisters(uint8_t SlaveAddress,uint16_t Address,uint16_t Size);
void WriteReadHldingRegisters(uint8_t SlaveAddress,uint16_t WriteAddress,uint16_t WriteSize,uint16_t * WriteBufer,uint16_t ReadAddress,uint16_t ReadSize);
void MODBUS_Maser_Ok_reciv(void);

#endif //MODBUS_MASTER_HEADER