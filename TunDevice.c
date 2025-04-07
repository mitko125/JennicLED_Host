#ifdef WIN32

#include <windows.h>
#include <process.h>
#include <fcntl.h>
#include <sys/types.h>
#include <errno.h>
#include <time.h>
#include <stdint.h>
#include "windows_sub.h"
#else	//WIN32
#include "avr_compiler.h"
//#include "GPRS_Uart.h"
#include "hardware.h"
#include "SIM900.h"
#endif	//WIN32


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "log.h"
#include "def.h"
#include "sub.h"

#include "TunDevice.h"
#include "JennicModule.h"

char pin[5];

volatile unsigned int butes_reciv = 0;
static volatile int ipv6_len = 0;
unsigned char ipv6_buf[SIZE_ipv6_buf];

static void SendACK(void){
	ipv6_buf[0] = (1) >> 8;
	ipv6_buf[1] = (1) & 0xFF;
	ipv6_buf[2] = VERSION;
	ipv6_buf[3] = ACK;
	SendPacage(HEADER_SIZE + 1);
}

teTunStatus eTunDeviceReadPacket(void)
{
	int len;

	if( butes_reciv ){

		unsigned int i;
		for (i = 0;i < (butes_reciv >> 1); i++) {
			unsigned char data;
			if (ipv6_buf[i << 1] >= 'A')
				data = ipv6_buf[i << 1] - 'A' + 10;
			else
				data = ipv6_buf[i << 1] - '0';
			data <<= 4;
			if (ipv6_buf[(i << 1)+1] >= 'A')
				data |= (ipv6_buf[(i << 1)+1] - 'A' + 10)&0x0F;
			else
				data |= (ipv6_buf[(i << 1)+1] - '0')&0x0F;
			ipv6_buf[i] = data;
		}
		
		len = ipv6_buf[0];
		len <<= 8;
		len |= ipv6_buf[1];
		if ((len + HEADER_SIZE) == (butes_reciv >> 1)) {
			switch(ipv6_buf[HEADER_SIZE]){
			case SET_WORK_HOURS:
				{
					tsSetWorkHours * psSetWorkHours = (tsSetWorkHours*)(ipv6_buf + HEADER_SIZE + 1);
					for (i = 0;i < u16_LampsInTable;i++) {
						if (memcmp(&((psLampTable + i)->sLampStatus.sMAC_Address.MAC[0]), psSetWorkHours->sMAC_Address.MAC, sizeof(tsMAC_Address)) == 0) {
							(psLampTable + i)->sLampStatus.u32WorkHours = psSetWorkHours->u32WorkHours;
							break;
						}
					}
					SendACK();
				}
				break;
			case SEND_LAMPS_MAC_TABLE:
				{
					uint16_t u16FirstTableEntry;
					uint16_t u16EntryCount;
					int i;

					tsSendTable * psSendLamsMAC = (tsSendTable*)(ipv6_buf + HEADER_SIZE + 1);
					tsMAC_Address * psMAC_Address = (tsMAC_Address*)(psSendLamsMAC + 1);

					u16_LampsInTable = 0;
					u16_LampsConnected = 0;

					u16FirstTableEntry = ntohs(psSendLamsMAC->u16FirstTableEntry);
					u16EntryCount = ntohs(psSendLamsMAC->u16EntryCount);

					for (i = 0; i < u16EntryCount;i++) {
						if( ( u16FirstTableEntry + i ) < ROUTE_TABLE_ENTRIES ){
							memcpy(&((psLampTable + u16FirstTableEntry + i)->sLampStatus.sMAC_Address.MAC[0]), psMAC_Address, sizeof(tsMAC_Address));
							memset(&((psLampTable + u16FirstTableEntry + i)->sLampStatus.sLastContacts), 0, sizeof(tsDateTime));
							memset(&((psLampTable + u16FirstTableEntry + i)->sLampStatus.u32WorkHours), 0, sizeof(uint32_t));
						}
						psMAC_Address++;
					}
					if (psSendLamsMAC->u8FlagEnd){
						if( ( u16_LampsInTable = u16FirstTableEntry + u16EntryCount ) > ROUTE_TABLE_ENTRIES )
							u16_LampsInTable = ROUTE_TABLE_ENTRIES;
					}
					SendACK();
				}
				break;
			case GET_LAMPS_STATUS:
				{
					uint16_t u16FirstTableEntry;
					uint16_t u16EntryCount;
					int i,cou=0;

					tsSendTable * psSendLamsStatus = (tsSendTable*)(ipv6_buf + HEADER_SIZE + 1);
					tsLampStatus * psLampStatus = (tsLampStatus*)(psSendLamsStatus + 1);

					u16FirstTableEntry = ntohs(psSendLamsStatus->u16FirstTableEntry);
					u16EntryCount = ntohs(psSendLamsStatus->u16EntryCount);

					for (i = u16FirstTableEntry; (i < u16_LampsInTable) && (u16EntryCount); i++) {
						u16EntryCount--;
						cou++;
						memcpy(psLampStatus, &((psLampTable + i)->sLampStatus), sizeof(tsLampStatus));
						psLampStatus++;
					}
					psSendLamsStatus->u16FirstTableEntry = htons(u16FirstTableEntry);
					psSendLamsStatus->u16EntryCount = htons(cou);
					if (i >= u16_LampsInTable)
						psSendLamsStatus->u8FlagEnd = 1;

					ipv6_buf[0] = (sizeof(tsSendTable) + (sizeof(tsLampStatus)*cou) + 1) >> 8;
					ipv6_buf[1] = (sizeof(tsSendTable) + (sizeof(tsLampStatus)*cou) + 1) & 0xFF;
					ipv6_buf[2] = VERSION;
					ipv6_buf[3] = SEND_LAMPS_STATUS;
					SendPacage(HEADER_SIZE + 1 + sizeof(tsSendTable) + (sizeof(tsLampStatus)*cou));
				}
				break;
			case COMMAND_GET_REJECT:
				ipv6_buf[0] = (sizeof(sRejectTable) + 1) >> 8;
				ipv6_buf[1] = (sizeof(sRejectTable) + 1) & 0xFF;
				ipv6_buf[2] = VERSION;
				ipv6_buf[3] = SEND_REJECT_TABLE;
				memcpy(ipv6_buf + HEADER_SIZE + 1, (unsigned char*)&sRejectTable, sizeof(sRejectTable));
				SendPacage(HEADER_SIZE + 1 + sizeof(sRejectTable));
				break;
			case COMMAND_GET_TIMERS:
				memcpy(&(psTimers->sDateTime), date_time, sizeof(tsDateTime));
				make_crc((unsigned char*)psTimers, sizeof(tsTimers));
				ipv6_buf[0] = (sizeof(tsTimers) + 1) >> 8;
				ipv6_buf[1] = (sizeof(tsTimers) + 1) & 0xFF;
				ipv6_buf[2] = VERSION;
				ipv6_buf[3] = COMMAND_SET_TIMERS;
				memcpy(ipv6_buf + HEADER_SIZE + 1, (unsigned char*)psTimers, sizeof(tsTimers));
				SendPacage(HEADER_SIZE + 1 + sizeof(tsTimers));
				break;
			case COMMAND_SET_TIMERS:
				memset(psTimers, 0, sizeof(tsTimers));	//V5 при SET... първо нулираме а после взимаме len-1 данни
				memcpy(psTimers, ipv6_buf + HEADER_SIZE + 1, len-1);
				make_crc((unsigned char*)psTimers, sizeof(tsTimers));
				SetDateTime();
				get_time();
				ipv6_buf[0] = (6 + 1) >> 8;
				ipv6_buf[1] = (6 + 1) & 0xFF;
				ipv6_buf[2] = VERSION;
				ipv6_buf[3] = SEND_DATE_TIME;
				memcpy(ipv6_buf + HEADER_SIZE + 1,date_time, 6);
				SendPacage(HEADER_SIZE + 1 + 6);
				break;
			case COMMAND_MAC_ADDRESS:
				ipv6_buf[0] = (8 + 1) >> 8;
				ipv6_buf[1] = (8 + 1) & 0xFF;
				ipv6_buf[2] = VERSION;
				ipv6_buf[3] = COMMAND_MAC_ADDRESS;
				memcpy(ipv6_buf + HEADER_SIZE + 1, (unsigned char*)(&sRouterAddress) + 8 , 8);

				ipv6_buf[4] ^= 0x02;

				SendPacage(HEADER_SIZE + 1 + 8);
				break;
			case COMMAND_GET_HOST_DATA:
				sModuleGetConfig.eRadioFrontEnd = psModuleSetConfig->eRadioFrontEnd;
				sModuleGetConfig.u8JenNetProfile = psModuleSetConfig->u8JenNetProfile;
				sModuleGetConfig.iAntennaDiversity = psModuleSetConfig->iAntennaDiversity;
				sModuleGetConfig.u8RadiusOff = psModuleSetConfig->u8RadiusOff;
				sModuleGetConfig.u16LampsInTable = htons(u16_LampsInTable);
				sModuleGetConfig.u16LampsConnected = htons(u16_LampsConnected);
				sModuleGetConfig.u8INT_resetGPRShours = psModuleSetConfig->u8INT_resetGPRShours;
				sModuleGetConfig.u8INT_resetGPRSminuts = psModuleSetConfig->u8INT_resetGPRSminuts;
				sModuleGetConfig.u8EnableEnergyMeter = psModuleSetConfig->u8EnableEnergyMeter;
				ipv6_buf[0] = (sizeof(tsConfigBorderRuter)+1) >> 8;
				ipv6_buf[1] = (sizeof(tsConfigBorderRuter)+1) & 0xFF;
				ipv6_buf[2] = VERSION;
				ipv6_buf[3] = COMMAND_SET_HOST_DATA;
				memcpy(ipv6_buf + HEADER_SIZE + 1, (unsigned char*)&sModuleGetConfig, sizeof(tsConfigBorderRuter));
				SendPacage( HEADER_SIZE + 1 + sizeof(tsConfigBorderRuter) );
				break;
			case COMMAND_SET_HOST_DATA:
				{
					memset(psModuleSetConfig, 0, sizeof(tsConfigBorderRuter));	//V5 при SET... първо нулираме а после взимаме len-1 данни
					memcpy(psModuleSetConfig, ipv6_buf + HEADER_SIZE + 1, len-1);
					make_crc((unsigned char*)psModuleSetConfig, sizeof(tsConfigBorderRuter));
					//int i;
					//for ( i = 0; i < sizeof(tsConfigBorderRuter); i++)printf("%02X", *(((unsigned char*)psModuleSetConfig) + i));
					//printf("\n\r sizeof(tsConfigBorderRuter) %d len - 1 %d\n\r",sizeof(tsConfigBorderRuter),len-1);
					#ifndef NO_COORDINATOR
					eJennicModuleStart();
					#endif
					SendACK();
				}
				break;
			case IPv6_PACKET:
				//memcpy(ipv6_buf, b + HEADER_SIZE + 1, len - 1);
				ipv6_len = len - 1;
				break;
			case COMMAND_ON:
				OnLamp();
				SendACK();
				break;
			case COMMAND_OFF:
				OffLamp();
				SendACK();
				break;
			case COMMAND_CLEAR_RAM:
				ClearRam();
				#ifndef NO_COORDINATOR
				eJennicModuleStart();
				#endif
				SendACK();
				break;
			case COMMAND_TIME_ON_OFF:
				memcpy(&(psTimers->sTimerOn1),ipv6_buf + HEADER_SIZE + 1,sizeof(tsTimerHourMinute));
				memcpy(&(psTimers->sTimerOff1),ipv6_buf + HEADER_SIZE + 1 + sizeof(tsTimerHourMinute) ,sizeof(tsTimerHourMinute));
				SendACK();
				break;
			case GET_STATUS_ROUTER:
				ipv6_buf[0] = (sizeof(tsRouterStatus) + 1) >> 8;
				ipv6_buf[1] = (sizeof(tsRouterStatus) + 1) & 0xFF;
				ipv6_buf[2] = VERSION;
				ipv6_buf[3] = SEND_STATUS_ROUTER;
				sRouterStatus.u8JenniceModuleState = eModuleState;
				sRouterStatus.u8Inputs = 0;
				sRouterStatus.u8Outputs = 0;
				#ifndef WIN32
				if( (PORT_INPUTS.IN & INP1) == 0) sRouterStatus.u8Inputs |= 0x01;
				if( (PORT_INPUTS.IN & INP2) == 0) sRouterStatus.u8Inputs |= 0x02;
				if( (PORT_INPUTS.IN & INP3) == 0) sRouterStatus.u8Inputs |= 0x04;
				if( (PORT_INPUTS.IN & INP4) == 0) sRouterStatus.u8Inputs |= 0x08;
				if( PORT_RELAY.IN & RELAY1 ) sRouterStatus.u8Outputs |= 0x01;
				if( PORT_RELAY.IN & RELAY2 ) sRouterStatus.u8Outputs |= 0x02;
				if( PORT_RELAY.IN & RELAY3 ) sRouterStatus.u8Outputs |= 0x04;
				if( PORT_RELAY.IN & RELAY4 ) sRouterStatus.u8Outputs |= 0x08;
				#else //WIN32
				if( on_relay )	sRouterStatus.u8Outputs |= 0x01;
				#endif	//WIN32
				memcpy(ipv6_buf + HEADER_SIZE + 1, (unsigned char*)&sRouterStatus, sizeof(tsRouterStatus));
				SendPacage(HEADER_SIZE + 1 + sizeof(tsRouterStatus));
				break;
			case COMMAND_READ_CURRENT_ENERGY:
				//printf("\n\r%d %d %d\n\r\n\r",
				//	sizeof(float),sizeof(double),sizeof(float));	
				ipv6_buf[0] = (sizeof(tsCurrentEnergy) + 1) >> 8;
				ipv6_buf[1] = (sizeof(tsCurrentEnergy) + 1) & 0xFF;
				ipv6_buf[2] = VERSION;
				ipv6_buf[3] = COMMAND_READ_CURRENT_ENERGY;
				memcpy(ipv6_buf + HEADER_SIZE + 1, (unsigned char*)&sCurrentEnergy, sizeof(tsCurrentEnergy));
				SendPacage(HEADER_SIZE + 1 + sizeof(tsCurrentEnergy));
				break;
			case COMMAND_READ_TOTAL_ENERGY:
				ipv6_buf[0] = (sizeof(tsTotalEnergy) + 1) >> 8;
				ipv6_buf[1] = (sizeof(tsTotalEnergy) + 1) & 0xFF;
				ipv6_buf[2] = VERSION;
				ipv6_buf[3] = COMMAND_READ_TOTAL_ENERGY;
				memcpy(ipv6_buf + HEADER_SIZE + 1, (unsigned char*)&sTotalEnergy, sizeof(tsTotalEnergy));
				SendPacage(HEADER_SIZE + 1 + sizeof(tsTotalEnergy));
				break;
			case COOMAND_GET_CURRENT_ENERGY_ARRAY:
				{
					static uint16_t i_array;
					static char flag_arrray;
					static uint16_t NextInArray;
					uint16_t u16EntryArrayCount;
					int cou=0;

					tsSendEnergyArray * psSendEnergyArray = (tsSendEnergyArray*)(ipv6_buf + HEADER_SIZE + 1);
					tsCurrentEnergySmall * psCurrentEnergySmall = (tsCurrentEnergySmall*)(psSendEnergyArray + 1);

					if( ntohs(psSendEnergyArray->u16FirstArrayEntry) == 0 ){	//Init send array
					/*	printf("INIT %02X-%02X-%02X %02X:%02X:%02X     %02X-%02X-%02X %02X:%02X:%02X \n\r",
							psSendEnergyArray->reversDateTimeStart.date_time[0],psSendEnergyArray->reversDateTimeStart.date_time[1],psSendEnergyArray->reversDateTimeStart.date_time[2],
							psSendEnergyArray->reversDateTimeStart.date_time[3],psSendEnergyArray->reversDateTimeStart.date_time[4],psSendEnergyArray->reversDateTimeStart.date_time[5],
							psSendEnergyArray->reversDateTimeEnd.date_time[0],psSendEnergyArray->reversDateTimeEnd.date_time[1],psSendEnergyArray->reversDateTimeEnd.date_time[2],
							psSendEnergyArray->reversDateTimeEnd.date_time[3],psSendEnergyArray->reversDateTimeEnd.date_time[4],psSendEnergyArray->reversDateTimeEnd.date_time[5]);
							*/
						NextInArray = sCurrenEnegryArray.NextInArray;
						if( memcmp(&(sCurrenEnegryArray.sCurrentEnergySmall[NextInArray].DateTime),&DateTimeCleared,sizeof(tsDateTime)) != 0){
							flag_arrray = 1;
							i_array = NextInArray;
						}else{
							flag_arrray = 0;
							i_array = 0;
						}
					}
					u16EntryArrayCount = ntohs(psSendEnergyArray->u16EntryArrayCount);
					//printf("i %d, flag %d\n\r",i_array,flag_arrray);
					for( ; ( i_array < NextInArray ) || flag_arrray ; ){
						unsigned char reverse_time[6];
						reverse_time[0] = sCurrenEnegryArray.sCurrentEnergySmall[i_array].DateTime.date_time[5];
						reverse_time[1] = sCurrenEnegryArray.sCurrentEnergySmall[i_array].DateTime.date_time[4];
						reverse_time[2] = sCurrenEnegryArray.sCurrentEnergySmall[i_array].DateTime.date_time[3];
						reverse_time[3] = sCurrenEnegryArray.sCurrentEnergySmall[i_array].DateTime.date_time[2];
						reverse_time[4] = sCurrenEnegryArray.sCurrentEnergySmall[i_array].DateTime.date_time[1];
						reverse_time[5] = sCurrenEnegryArray.sCurrentEnergySmall[i_array].DateTime.date_time[0];
						/*printf("Current %02X-%02X-%02X %02X:%02X:%02X %d %d %d %d\n\r",
							reverse_time[0],reverse_time[1],reverse_time[2],
							reverse_time[3],reverse_time[4],reverse_time[5],
							sizeof(float),sizeof(tsSendEnergyArray),sizeof(tsCurrentEnergySmall),sizeof(tsCurrenEnegryArray));			*/			
						if( memcmp( reverse_time , &(psSendEnergyArray->reversDateTimeStart) , sizeof(tsDateTime)) < 0 )
							goto no_add_data;
						if( memcmp( reverse_time , &(psSendEnergyArray->reversDateTimeEnd) , sizeof(tsDateTime)) > 0 )
							goto no_add_data;	
						{	
							u16EntryArrayCount--;
							cou++;
							memcpy(psCurrentEnergySmall, &(sCurrenEnegryArray.sCurrentEnergySmall[i_array]), sizeof(tsCurrentEnergySmall));
							psCurrentEnergySmall++;
						}
				no_add_data:
						if( ++i_array >= MAX_CURRENT_ENERGY ){
							i_array = 0;
							flag_arrray = 0;
						}
						if( u16EntryArrayCount == 0)
							break;
					}
					psSendEnergyArray->u16EntryArrayCount = htons(cou);
					if (i_array == NextInArray){
						psSendEnergyArray->u8FlagEnd = 1;
						//printf("END\n\r");
					}

					ipv6_buf[0] = (sizeof(tsSendEnergyArray) + (sizeof(tsCurrentEnergySmall)*cou) + 1) >> 8;
					ipv6_buf[1] = (sizeof(tsSendEnergyArray) + (sizeof(tsCurrentEnergySmall)*cou) + 1) & 0xFF;
					ipv6_buf[2] = VERSION;
					ipv6_buf[3] = COOMAND_GET_CURRENT_ENERGY_ARRAY;
					SendPacage(HEADER_SIZE + 1 + sizeof(tsSendEnergyArray) + (sizeof(tsCurrentEnergySmall)*cou));
				}
				break;
			case GET_MY_IP_FROM_SERVER:
				{
					ipv6_buf[0] = (sizeof(sin_addr) + 1) >> 8;
					ipv6_buf[1] = (sizeof(sin_addr) + 1) & 0xFF;
					ipv6_buf[2] = VERSION;
					ipv6_buf[3] = GET_MY_IP_FROM_SERVER;
					sin_addr my_ip;
					my_ip.S_un.S_addr = htonl(Clients_sIP4addres[client_number].S_un.S_addr);
					memcpy(ipv6_buf + HEADER_SIZE + 1, (unsigned char*)&my_ip,sizeof(sin_addr));
					SendPacage(HEADER_SIZE + 1 + sizeof(sin_addr));
				}
				break;
			case SET_MY_IP_TO_CLIENT:
				{
					psServerIP->S_un.S_addr = Clients_sIP4addres[client_number].S_un.S_addr;
					SendACK();
					data_to_server = SET_MY_IP_TO_CLIENT;
				}
				break;
			default:
				daemon_log(LOG_DEBUG, "Error unknow teCommandsPC %d",ipv6_buf[HEADER_SIZE]);
				//Send NACK ???
				break;
			}
		}else{
			daemon_log(LOG_DEBUG, "BAD lenght from client:%d %d",len + HEADER_SIZE, butes_reciv>>1);
		}
		butes_reciv = 0;
	}
					
					
					
					
					
  
	len = 0;//read(tun_fd, buf, sizeof(buf));
  if (ipv6_len > 0)
    {	
		len = ipv6_len;
		ipv6_len = 0;
        // If there's data waiting for us on the TUN device, write it to the Jennic chip.
        //printf("Data from TUN: %d bytes\n", len);
        
        //for (i = 0; i < len; i++)
        //    printf("%x ", buf[i] & 0x000000FF);
        //printf("\n");
        
        // Send data to Jennic chip
        if (eJennicModuleWriteIPv6(len, ipv6_buf+HEADER_SIZE+1) != E_MODULE_OK)
        {
            daemon_log(LOG_ERR, "Error writing packet to module");
            return E_TUN_ERROR;
        }
    }
    return E_TUN_OK;
}


teTunStatus eTunDeviceWritePacket(uint32_t u32Length, uint8_t *pu8Data)
{
	ipv6_buf[0] = (u32Length + 1) >> 8;
	ipv6_buf[1] = (u32Length + 1) & 0xFF;
	ipv6_buf[2] = VERSION;
	ipv6_buf[3] = IPv6_PACKET;
	memcpy( ipv6_buf + HEADER_SIZE + 1, pu8Data, u32Length);
	SendPacage(u32Length + HEADER_SIZE + 1);

	return E_TUN_OK;
    
    return E_TUN_ERROR;
}



