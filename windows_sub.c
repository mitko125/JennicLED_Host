#include <windows.h>
#include <process.h> 
#include <time.h>
#include <conio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <errno.h>
#include <sys/stat.h>
#include <signal.h>
#include "Serial.h"
#include <stdint.h>
//#include <afxdisp.h>
#include "windows_sub.h"

#include "def.h"
#include "log.h"
#include "sub.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


unsigned char * p_E_RAM = NULL;


void InitHardware(void) {
	//Init

	p_E_RAM = malloc(SIZE_RAM);

	if (p_E_RAM == NULL) {
		daemon_log(LOG_EMERG, "Err create Data Memory");
		while (1);
	}
	
	FILE *fp;
	fp = fopen("backup.bin", "rb");
	if (fp == NULL ) {
		daemon_log(LOG_EMERG, "Not backub.bin file, clear RAM");
		ClearRam();
	} else {
		uint16_t i = (uint16_t)fread(p_E_RAM, sizeof(unsigned char), SIZE_RAM , fp);
		if (i != SIZE_RAM) {
			daemon_log(LOG_ERR, "Bad lenght %u backub.bin file, clear RAM", i);
			ClearRam();
		}
		fclose(fp);
	}
	ENERGY_METER_init();
}

void save_RAM(void)
{
	FILE *fp;
	fp = fopen("backup.bin", "wb");
	if (fp != NULL) {
		fwrite(p_E_RAM, sizeof(unsigned char), SIZE_RAM, fp);
		fclose(fp);
	}else{
		daemon_log(LOG_ERR, "Error write backub.bin file");
	}
		
}

#undef FAST_GET_TIME

#ifdef FAST_GET_TIME
static uint16_t INT_sim_hour = 0, INT_sim_minute = 0, old_sec = 100;
void first_get_time(void)
#else
void get_time(void)
#endif
{
	SYSTEMTIME lpSystemTime;
	GetLocalTime(&lpSystemTime);	//GetSystemTime - по Гринуич  GetLocalTime - местно време
	date_time[5] = INT_BCD((lpSystemTime.wYear % 100));
	date_time[4] = INT_BCD(lpSystemTime.wMonth);
	date_time[3] = INT_BCD(lpSystemTime.wDay);
	date_time[2] = INT_BCD(lpSystemTime.wHour);// +3));
	date_time[1] = INT_BCD(lpSystemTime.wMinute);
	date_time[0] = INT_BCD(lpSystemTime.wSecond);
#ifdef FAST_GET_TIME
	INT_sim_hour = lpSystemTime.wHour;
	INT_sim_minute = lpSystemTime.wMinute;
	old_sec = lpSystemTime.wSecond;
/*	printf_P(PSTR("\n\rTime %02d:%02d  "),INT_sim_hour, INT_sim_minute);
	PrintDateTime(date_time);
	printf_P(PSTR("\n\r"));*/
#endif
}

#ifdef FAST_GET_TIME
static uint8_t fl_first_get_time = 1;
void get_time(void)
{
	if (fl_first_get_time) {
		fl_first_get_time = 0;
		first_get_time();
	}
	SYSTEMTIME lpSystemTime;
	GetLocalTime(&lpSystemTime);
	uint16_t sec = lpSystemTime.wSecond;
	if( old_sec != sec ){
		old_sec = sec;
		if( ++INT_sim_minute  >= 60 ){
			INT_sim_minute = 0;
			if( ++INT_sim_hour >= 24 ){
				INT_sim_hour = 0;
			}
		}
		//printf("%02d:%02d\n\r", INT_sim_hour, INT_sim_minute);
		date_time[2] = INT_BCD(INT_sim_hour);
		date_time[1] = INT_BCD(INT_sim_minute);
		date_time[0] = INT_BCD(sec);
	}
}
#endif

void ResetSIM(void){
	if (verbosity >= LOG_DEBUG) {
		printf_P(PSTR("\n\rReset SIM in Time: "));
		PrintDateTime(date_time);
		printf_P(PSTR("\n\r"));
	}
}

void SetDateTime(void){
	error_clock = 0;
}

static time_t  sLastLoop = 0; 

void ENERGY_METER_init(void){

	get_time();

	sLastLoop = time(NULL);
	
	memcpy(&(sCurrentEnergy.sLastContact), date_time, sizeof(tsDateTime));
		
	sCurrentEnergy.L1_Voltage = (float)220.1;
	sCurrentEnergy.L2_Voltage = (float)220.2;
	sCurrentEnergy.L3_Voltage = (float)220.3;
	
	sCurrentEnergy.Grid_frequency = (float)50.0;
	
	sCurrentEnergy.L1_Current = (float)1.1;
	sCurrentEnergy.L2_Current = (float)1.2;
	sCurrentEnergy.L3_Current = (float)1.3;
	
	sCurrentEnergy.Active_power = (float)2.0;
	sCurrentEnergy.L1_Active_power = (float)2.1;
	sCurrentEnergy.L2_Active_power = (float)2.2;
	sCurrentEnergy.L3_Active_power = (float)2.3;
	
	sCurrentEnergy.Reactive_power = (float)3;
	sCurrentEnergy.L1_Reactive_power = (float)3.1;
	sCurrentEnergy.L2_Reactive_power = (float)3.2;
	sCurrentEnergy.L3_Reactive_power = (float)3.3;
	
	sCurrentEnergy.Apparent_power = (float)4;
	sCurrentEnergy.L1_Apparent_power = (float)4.1;
	sCurrentEnergy.L2_Apparent_power = (float)4.2;
	sCurrentEnergy.L3_Apparent_power = (float)4.3;
	
	sCurrentEnergy.Power_factor = (float)1.0;
	sCurrentEnergy.L1_Power_factor = (float)0.91;
	sCurrentEnergy.L2_Power_factor = (float)0.92;
	sCurrentEnergy.L3_Power_factor = (float)0.93;
	
	
	memcpy(&(sTotalEnergy.sLastContact), date_time, sizeof(tsDateTime));
		
	sTotalEnergy.Total_active_energy = (float)6.0;
	sTotalEnergy.T1_Total_active_energy = (float)4.0;
	sTotalEnergy.T2_Total_active_energy = (float)2.0;
		
	sTotalEnergy.L1_Total_active_energy = (float)1.0;
	sTotalEnergy.L2_Total_active_energy = (float)2.0;
	sTotalEnergy.L3_Total_active_energy = (float)3.0;
		
		
	sTotalEnergy.Total_reactive_energy = (float)0.6;
	sTotalEnergy.T1_Total_reactive_energy = (float)0.4;
	sTotalEnergy.T2_Total_reactive_energy = (float)0.2;
		
	sTotalEnergy.L1_Total_reactive_energy = (float)0.1;
	sTotalEnergy.L2_Total_reactive_energy = (float)0.2;
	sTotalEnergy.L3_Total_reactive_energy = (float)0.3;
}

void ENERGY_METER_Loop(void){
	if (difftime(time(NULL), sLastLoop) > 4){
		sLastLoop = time(NULL);
		if( psModuleSetConfig->u8EnableEnergyMeter ){
			memcpy(&(sCurrentEnergy.sLastContact), date_time, sizeof(tsDateTime));
			sCurrentEnergy.Active_power += (float)0.1;
			AddCurrentEnergyInArray();
		}
	}
}