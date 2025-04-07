
//NOARK
//Smart Energy Meters
//Ex9EMS 3P 4M Direct connected - 3P 4W



#include "avr_compiler.h"
#include "def.h"
#include "sub.h"
#include "EnergyMeter.h"
#include "MODBUS_Master.h"
#include "FTDI_Uart.h"
#include "log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <stdio.h>
//#include <stdlib.h>
#include <string.h>
#include "hardware.h"

typedef unsigned char byte;

typedef struct byte2{
	unsigned char l;
	unsigned char h;
}byte2;

typedef union word_2{
	unsigned int u16w;
	byte2 bits2_8;
}word_2;

typedef struct byte4{
	unsigned char ll;
	unsigned char lh;
	unsigned char hl;
	unsigned char hh;
}byte4;

typedef struct byte2_2{
	unsigned int u16l;
	unsigned int u16h;
}byte2_2;

typedef union bits32{
	float float_data;
	unsigned long int u_long_data;
	long int long_data;
	byte2_2 byte2_2;
	byte4 byte4;
}bits32;


#define CYRRENT_ENRGY_ADDRESS (0x5000 + (1*2))
#define CYRRENT_ENRGY_LENGHT (50 - (1*2))

#define TOTAL_ENRGY_ADDRESS (0x6000)
#define TOTAL_ENRGY_LENGHT (50)	//един регистъ в повече но да се различава от CYRRENT_ENRGY_LENGHT

#define ENERGY_METER_MODBUS_ADDRESS 1

volatile uint8_t ENERGY_METER_time_wait_s = 2;
volatile uint8_t ENERGY_METER_time_ERR_s;

uint8_t enable_ENERGY_METER = 1;

void ENERGY_METER_init(void){
	MODBUS_Master_init();
}

static uint8_t fl_odd;

void ENERGY_METER_Loop(void){
	if( enable_ENERGY_METER ){
	
		MODBUS_Master_Loop();
		
		if( ENERGY_METER_time_ERR_s > 20 ){
			ENERGY_METER_time_ERR_s = 0;
			daemon_log(LOG_ERR, "Error read ENERGY METER");
		}
	
		if( ENERGY_METER_time_wait_s == 0 ){
			ENERGY_METER_time_wait_s = 2;
			if( ( ++fl_odd ) & 0x01 )
				ReadHldingRegisters(ENERGY_METER_MODBUS_ADDRESS,CYRRENT_ENRGY_ADDRESS,CYRRENT_ENRGY_LENGHT);
			else
				ReadHldingRegisters(ENERGY_METER_MODBUS_ADDRESS,TOTAL_ENRGY_ADDRESS,TOTAL_ENRGY_LENGHT);
		}
	}
}

static float GetDoubleFromMODBUS(uint8_t offset){
	bits32 data;
	
	data.byte4.hh = *(ptr_to_data_read + offset + 0 );
	data.byte4.hl = *(ptr_to_data_read + offset + 1 );
	data.byte4.lh = *(ptr_to_data_read + offset + 2 );
	data.byte4.ll = *(ptr_to_data_read + offset + 3 );
	
	return data.float_data;
}



//tsCurrentEnergy sCurrentEnergy;
//tsTotalEnergy sTotalEnergy;

void MODBUS_Maser_Ok_reciv(void){
	
/*	int i;
	for( i = 0 ; i < lenght_read ; i++ ){
		printf("%02X",*(ptr_to_data_read+i));
	}
	printf("\n\rLenght %d\n\r",lenght_read);*/
	
	
	if( lenght_read == ( CYRRENT_ENRGY_LENGHT * 2 ) ){
		ENERGY_METER_time_ERR_s = 0;
		
		memcpy(&(sCurrentEnergy.sLastContact), date_time, sizeof(tsDateTime));
		
		sCurrentEnergy.L1_Voltage = GetDoubleFromMODBUS(0);
		sCurrentEnergy.L2_Voltage = GetDoubleFromMODBUS(4);
		sCurrentEnergy.L3_Voltage = GetDoubleFromMODBUS(8);
	
		sCurrentEnergy.Grid_frequency = GetDoubleFromMODBUS(12);
																											//(16);	//Current*
		sCurrentEnergy.L1_Current = GetDoubleFromMODBUS(20);
		sCurrentEnergy.L2_Current = GetDoubleFromMODBUS(24);
		sCurrentEnergy.L3_Current = GetDoubleFromMODBUS(28);
	
		sCurrentEnergy.Active_power = GetDoubleFromMODBUS(32);
		sCurrentEnergy.L1_Active_power = GetDoubleFromMODBUS(36);
		sCurrentEnergy.L2_Active_power = GetDoubleFromMODBUS(40);
		sCurrentEnergy.L3_Active_power = GetDoubleFromMODBUS(44);
	
		sCurrentEnergy.Reactive_power = GetDoubleFromMODBUS(48);
		sCurrentEnergy.L1_Reactive_power = GetDoubleFromMODBUS(52);
		sCurrentEnergy.L2_Reactive_power = GetDoubleFromMODBUS(56);
		sCurrentEnergy.L3_Reactive_power = GetDoubleFromMODBUS(60);
	
		sCurrentEnergy.Apparent_power = GetDoubleFromMODBUS(64);
		sCurrentEnergy.L1_Apparent_power = GetDoubleFromMODBUS(68);
		sCurrentEnergy.L2_Apparent_power = GetDoubleFromMODBUS(72);
		sCurrentEnergy.L3_Apparent_power = GetDoubleFromMODBUS(76);
	
		sCurrentEnergy.Power_factor = GetDoubleFromMODBUS(80);
		sCurrentEnergy.L1_Power_factor = GetDoubleFromMODBUS(84);
		sCurrentEnergy.L2_Power_factor = GetDoubleFromMODBUS(88);
		sCurrentEnergy.L3_Power_factor = GetDoubleFromMODBUS(92);
	
		AddCurrentEnergyInArray();
		//PrintCurrentEnergy();
	}else if( lenght_read == ( TOTAL_ENRGY_LENGHT * 2 ) ){
		ENERGY_METER_time_ERR_s = 0;
	
		memcpy(&(sTotalEnergy.sLastContact), date_time, sizeof(tsDateTime));
		
		sTotalEnergy.Total_active_energy = GetDoubleFromMODBUS(0);
		sTotalEnergy.T1_Total_active_energy = GetDoubleFromMODBUS(4);
		sTotalEnergy.T2_Total_active_energy = GetDoubleFromMODBUS(8);
		
		sTotalEnergy.L1_Total_active_energy = GetDoubleFromMODBUS(12);
		sTotalEnergy.L2_Total_active_energy = GetDoubleFromMODBUS(16);
		sTotalEnergy.L3_Total_active_energy = GetDoubleFromMODBUS(20);
		//24	Forward active energy
		//28
		//32
		//36
		//40
		//44
		
		//48	Reverse active energy
		//52
		//56
		//60
		//64
		//68
		sTotalEnergy.Total_reactive_energy = GetDoubleFromMODBUS(72);
		sTotalEnergy.T1_Total_reactive_energy = GetDoubleFromMODBUS(76);
		sTotalEnergy.T2_Total_reactive_energy = GetDoubleFromMODBUS(80);
		
		sTotalEnergy.L1_Total_reactive_energy = GetDoubleFromMODBUS(84);
		sTotalEnergy.L2_Total_reactive_energy = GetDoubleFromMODBUS(88);
		sTotalEnergy.L3_Total_reactive_energy = GetDoubleFromMODBUS(92);
		
		//PrintTotalEnergy();
	}
}
