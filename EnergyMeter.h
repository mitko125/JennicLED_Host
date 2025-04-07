#ifndef ENERGY_METER_HEADER
#define ENERGY_METER_HEADER

extern volatile uint8_t ENERGY_METER_time_wait_s;
extern volatile uint8_t ENERGY_METER_time_ERR_s;

void ENERGY_METER_init(void);
void ENERGY_METER_Loop(void);


#endif //ENERGY_METER_HEADER