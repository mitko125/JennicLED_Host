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

void get_time(void)
{
	SYSTEMTIME lpSystemTime;
	GetSystemTime(&lpSystemTime);
	date_time[5] = INT_BCD((lpSystemTime.wYear % 100));
	date_time[4] = INT_BCD(lpSystemTime.wMonth);
	date_time[3] = INT_BCD(lpSystemTime.wDay);
	date_time[2] = INT_BCD((lpSystemTime.wHour + 3));
	date_time[1] = INT_BCD(lpSystemTime.wMinute);
	date_time[0] = INT_BCD(lpSystemTime.wSecond);
}

void SetDateTime(void){
	error_clock = 0;
}