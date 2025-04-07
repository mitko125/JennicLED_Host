/****************************************************************************
 *
 * MODULE:             Linux 6LoWPAN Routing daemon
 *
 * COMPONENT:          Interface to module
 *
 * REVISION:           $Revision: 37647 $
 *
 * DATED:              $Date: 2011-12-02 11:16:28 +0000 (Fri, 02 Dec 2011) $
 *
 * AUTHOR:             Matt Redfearn
 *
 ****************************************************************************
 *
 * This software is owned by NXP B.V. and/or its supplier and is protected
 * under applicable copyright laws. All rights are reserved. We grant You,
 * and any third parties, a license to use this software solely and
 * exclusively on NXP products [NXP Microcontrollers such as JN5148, JN5142, JN5139]. 
 * You, and any third parties must reproduce the copyright and warranty notice
 * and any other legend of ownership on each copy or partial copy of the 
 * software.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.

 * Copyright NXP B.V. 2012. All rights reserved
 *
 ***************************************************************************/

#ifdef WIN32
#include <windows.h>
#include <process.h>  
#endif	//WIN32

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#ifdef WIN32
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <sys/stat.h>
#include <stdint.h>
#include "windows_sub.h"
#else	//WIN32
#include "avr_compiler.h"
#include "hardware.h"
#endif	//WIN32

#include "log.h"
#include "def.h"
#include "sub.h"

#include "JennicModule.h"
#include "TunDevice.h"
#include "SerialLink.h"

#define PRINT_SECURITY false

#define JENNIC_VERSION_MAJOR(a) (a << 16)
#define JENNIC_VERSION_MINOR(a) (a << 8)
#define JENNIC_VERSION_REV(a)   (a << 0)
#define JENNIC_VERSION(a,b,c)   (JENNIC_VERSION_MAJOR(a) | JENNIC_VERSION_MINOR(b) | JENNIC_VERSION_REV(c))

/** Timeout comms after 60 seconds of no data */
#define MODULE_TIMEOUT 60

/** Structure of flags for state machine */
static struct
{
    unsigned    uVersionKnown           : 1;    /**< Version information has been received */
    unsigned    uAddressKnown           : 1;    /**< IPv6 address information has been received */
    unsigned    uConfigKnown            : 1;    /**< Configuration of node is known */
    unsigned    uSupportsPing           : 1;    /**< Node supports the ping message */
} sFlags;

enum
{
    E_STATE_IDLE,
    E_STATE_DETERMINE_VERSION,
    E_STATE_CONFIGURE_NETWORK,
    E_STATE_CONFIGURE_SECURITY,
    E_STATE_CONFIGURE_PROFILE,
    E_STATE_START_MODULE,
    E_STATE_CONFIGURE_FRONTEND,
    E_STATE_DETERMINE_CONFIGURATION,
    E_STATE_DETERMINE_ADDRESS,
    E_STATE_ACTIVITY_LED,
    E_STATE_RUNNING,
}JeModuleState;



uint8_t eModuleState;

#ifdef WIN32
/** RADIUS Packet codes */
typedef enum
{
#pragma pack(push, 1)
	E_RADIUS_ACCESS_REQUEST = 1,
	E_RADIUS_ACCESS_ACCEPT = 2,
	E_RADIUS_ACCESS_REJECT = 3,
#pragma pack(pop)
}teRADIUS_Packet_Code;

/** RADIUS Attribute-Value pair types */
typedef enum
{
#pragma pack(push, 1)
	E_RADIUS_USER_NAME = 1,
	E_RADIUS_USER_PASSWORD = 2,
	E_RADIUS_VENDOR_SPECIFIC = 26,
	E_RADIUS_802154_COMMISIONING_KEY = 100,
#pragma pack(pop)
}teRADIUS_AVP_Type;
#else //WIN32
/** RADIUS Packet codes */
typedef enum
{
	E_RADIUS_ACCESS_REQUEST = 1,
	E_RADIUS_ACCESS_ACCEPT = 2,
	E_RADIUS_ACCESS_REJECT = 3,
} __attribute__((__packed__)) teRADIUS_Packet_Code;

 /** RADIUS Attribute-Value pair types */
typedef enum
{
	E_RADIUS_USER_NAME = 1,
	E_RADIUS_USER_PASSWORD = 2,
	E_RADIUS_VENDOR_SPECIFIC = 26,
	E_RADIUS_802154_COMMISIONING_KEY = 100,
} __attribute__((__packed__))teRADIUS_AVP_Type;
#endif //WIN32

#define IANA_VENDOR_ID_NXP 28137L

/** Enumerated type of module modes */
typedef enum
{
	E_MODE_COORDINATOR = 0,        /**< Start module as a coordinator */
	E_MODE_ROUTER = 1,        /**< Start module as a router */
	E_MODE_COMMISSIONING = 2,        /**< Start module in commissioning mode */
} teModuleMode;

teModuleMode     eModuleMode        = E_MODE_COORDINATOR;

/** Enumerated type of Activity LED DIOs */
typedef enum
{
	E_ACTIVITY_LED_NONE = 0xFFFFFFFF,
} teActivityLED;

teActivityLED    eActivityLED = E_ACTIVITY_LED_NONE;

//tsConfigBorderRuter sModuleSetConfig;
tsConfigBorderRuter sModuleGetConfig;
struct in6_addr sRouterAddress;



/** Firmware version of the connected device */
static uint32_t u32JennicDeviceVersion = 0;


/** Time of last successful communications */
long  iLastSuccessfulComms = 0;



#define VERSION 0 

static uint8_t hexa_to_byte(uint8_t  *u8Data) {
	uint8_t data = 0;
	if (*u8Data <= '9')
		data = *u8Data - '0';
	else
		data = *u8Data + 10 - 'A';
	data <<= 4;
	u8Data++;
	if (*u8Data <= '9')
		data |= *u8Data - '0';
	else
		data |= *u8Data + 10 - 'A';
	return data;
}

static uint16_t CalculateChecsum(uint32_t u32Length, uint8_t *pu8Data)
{
	uint32_t checksum, lenght, data;
	uint8_t *p;
	
  //printf_P(PSTR("\n\r%u %u %04X %u \n\r"),(unsigned int)(u32Length>>16),(unsigned int)(u32Length&0xFFFF),(unsigned int)pu8Data,*pu8Data);
	
	if (u32Length & 1) {
		pu8Data[u32Length] = 0;
		u32Length++;
	}

	checksum = ((uint16_t)(*(pu8Data + 46))) << 8 | *(pu8Data + 47);
	lenght = ((uint16_t)(*(pu8Data + 4))) << 8 | *(pu8Data + 5);
	data = *(pu8Data + 6);

	checksum = lenght + data;
	for (p = pu8Data + 8; p < (pu8Data + u32Length); p += 2) {
		data = ((uint16_t)(*p)) << 8 | *(p + 1);
		checksum += data;
		while (checksum >> 16)
			checksum = (checksum & 0xFFFF) + (checksum >> 16);
	}

	checksum = (uint16_t)~checksum;
	return checksum;
}

#define SOURCE_PORT 0xD3ED


static teModuleStatus eJennicModuleSendMessageIPv6(struct in6_addr *source_addr,struct in6_addr *dest_addr,uint32_t u32Length, uint8_t *pu8Data)
{
	uint8_t buffer[200];
	uint16_t checksum;

	buffer[0] = 0x60;
	buffer[1] = 0;
	buffer[2] = 0;
	buffer[3] = 0;

	buffer[4] = ((u32Length + 8) >> 8) & 0xFF;
	buffer[5] = (u32Length + 8) & 0xFF;
	buffer[6] = 0x11;
	buffer[7] = 0x40;

	memcpy(buffer + 8, source_addr, sizeof(struct in6_addr));
	memcpy(buffer + 24, dest_addr, sizeof(struct in6_addr));


	buffer[40] = (SOURCE_PORT >> 8 ) & 0xFF;
	buffer[41] = SOURCE_PORT & 0xFF;
	buffer[42] = (JENNIC_PORT >> 8) & 0xFF;
	buffer[43] = JENNIC_PORT & 0xFF;

	buffer[44] = ((u32Length+8) >> 8) & 0xFF;
	buffer[45] = (u32Length+8) & 0xFF;
	buffer[46] = 0;
	buffer[47] = 0;

	memcpy(buffer + 48, pu8Data, u32Length);

	u32Length += 40 + 8;
	checksum = CalculateChecsum(u32Length, pu8Data);

	//if (checksum == 0)
	//	checksum = 0xffff;
	buffer[46] = checksum >> 8;
	buffer[47] = checksum & 0xff;

	if (verbosity >= LOG_DEBUG) {
		uint32_t i;

		for (i = 0; i < u32Length; i++)
			printf("%02X", *(buffer + i));
		putchar('\n');
	}
	
	if (eJennicModuleWriteIPv6(u32Length, buffer) != E_MODULE_OK)
	{
		daemon_log(LOG_ERR, "Error writing packet to module");
		return E_MODULE_ERROR;
	}

	return E_MODULE_OK;
}

static uint8_t my_local_address[16] = { 0xFE,0x80,0x00,0x00,0x00,0x00,0x00,0x00 ,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF };
static uint8_t my_sors_address[16] =  { 0xFD,0x04,0x0B,0xD3,0x80,0xE8,0xFF,0xFF ,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00 };
static uint8_t all_device_group[16] = { 0xFF,0x15,0x00,0x00,0x00,0x00,0x00,0x00 ,0x00,0x00,0x00,0x00,0x00,0x00,0xF0,0x0F };
//static uint8_t all_bulbs_group[16] = { 0xFF,0x15,0x00,0x00,0x00,0x00,0x00,0x00 ,0x00,0x00,0x00,0x00,0x00,0x00,0xFE,0x04 };
static uint8_t group_prefix[16]    = { 0xFF,0x15,0x00,0x00,0x00,0x00,0x00,0x00 ,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00 };

static uint8_t group_address[MAX_GROUP_TIMERS][2]={
	{ 0xF0,0x0F },
	{ 0xFE,0x04 },
	{ 0x1E,0x02 },
	{ 0x1E,0x03 },

	{ 0x1E,0x04 },
	{ 0x1E,0x05 },
	{ 0x1E,0x06 },
	{ 0x1E,0x07 },

	{ 0x1E,0x08 },
	{ 0x1E,0x09 },
	{ 0x1E,0x0A },
	{ 0x1E,0x0B },

	{ 0x1E,0x0C },
	{ 0x1E,0x0D },
	{ 0x1E,0x0E },
	{ 0x1E,0x0F },
};

teModuleStatus GlobalSetUint8ByModuleID(uint32_t ModuleID, uint8_t VariableIndex, uint8_t data)
{
	uint8_t buffer[20];

	if(eModuleState != E_STATE_RUNNING)
		return E_MODULE_ERROR;

	buffer[0] = VERSION;
	buffer[1] = 0x1D;
	buffer[2] = rand() & 0x7F;

	buffer[3] = (ModuleID >> 24) & 0xFF;
	buffer[4] = (ModuleID >> 16) & 0xFF;
	buffer[5] = (ModuleID >> 8) & 0xFF;
	buffer[6] = (ModuleID ) & 0xFF;

	buffer[7] = VariableIndex;

	buffer[8] = 0x00;
	buffer[9] = 0x04;
	buffer[10] = data;

	return eJennicModuleSendMessageIPv6((struct in6_addr*)&my_local_address,(struct in6_addr*)&all_device_group,11,buffer);
}

teModuleStatus GroupSetUint8ByModuleID(uint8_t group,uint32_t ModuleID, uint8_t VariableIndex, uint8_t data) {
	uint8_t buffer[20];

	if (eModuleState != E_STATE_RUNNING)
		return E_MODULE_ERROR;

	group_prefix[14] = group_address[group][0];
	group_prefix[15] = group_address[group][1];

	buffer[0] = VERSION;
	buffer[1] = 0x1D;
	buffer[2] = rand() & 0x7F;

	buffer[3] = (ModuleID >> 24) & 0xFF;
	buffer[4] = (ModuleID >> 16) & 0xFF;
	buffer[5] = (ModuleID >> 8) & 0xFF;
	buffer[6] = (ModuleID)& 0xFF;

	buffer[7] = VariableIndex;

	buffer[8] = 0x00;
	buffer[9] = 0x04;
	buffer[10] = data;

	return eJennicModuleSendMessageIPv6((struct in6_addr*)&my_local_address,(struct in6_addr*) &group_prefix, 11, buffer);
}

teModuleStatus GetSubTreeNodes(void){
	uint8_t buffer[20];

	if (eModuleState != E_STATE_RUNNING)
		return E_MODULE_ERROR;
	
	//JenNet Module NetworkTable blob table
	uint8_t mouleIndex = 0x01;
	uint8_t VariableIndex = 0x03;
	
	buffer[0] = VERSION;
	buffer[1] = 0x10;	//Get_request
	buffer[2] = rand() & 0x7F;

	buffer[3] = mouleIndex;

	buffer[4] = VariableIndex;

	buffer[5] = 1;
	
	return eJennicModuleSendMessageIPv6((struct in6_addr*)&my_sors_address, (struct in6_addr*) &sRouterAddress, 6, buffer);
}

teModuleStatus GetJenNetNetworkRouter(uint16_t u16FirstTableEntry, uint8_t u8EntryCount) {
	uint8_t buffer[20];

	if (eModuleState != E_STATE_RUNNING)
		return E_MODULE_ERROR;

/*	//JenNet Module NetworkTable blob table
	uint32_t ModuleID = 0xFFFFFF01;
	uint8_t VariableIndex = 0x04;
	
	buffer[0] = VERSION;
	buffer[1] = 0x1C;	//Get_by_ID_request
	buffer[2] = rand() & 0x7F;

	buffer[3] = (ModuleID >> 24) & 0xFF;
	buffer[4] = (ModuleID >> 16) & 0xFF;
	buffer[5] = (ModuleID >> 8) & 0xFF;
	buffer[6] = (ModuleID)& 0xFF;

	buffer[7] = VariableIndex;

	buffer[8] = (u16FirstTableEntry >> 8) & 0xFF;
	buffer[9] = (u16FirstTableEntry) & 0xFF;;

	buffer[10] = u8EntryCount;

	return eJennicModuleSendMessageIPv6((struct in6_addr*)&my_sors_address, (struct in6_addr*) &sRouterAddress, 11, buffer);*/
	
	//JenNet Module NetworkTable blob table
	uint8_t mouleIndex = 0x01;
	uint8_t VariableIndex = 0x04;
	
	buffer[0] = VERSION;
	buffer[1] = 0x10;	//Get_request
	buffer[2] = rand() & 0x7F;

	buffer[3] = mouleIndex;

	buffer[4] = VariableIndex;

	buffer[5] = (u16FirstTableEntry >> 8) & 0xFF;
	buffer[6] = (u16FirstTableEntry) & 0xFF;;

	buffer[7] = u8EntryCount;

	return eJennicModuleSendMessageIPv6((struct in6_addr*)&my_sors_address, (struct in6_addr*) &sRouterAddress, 8, buffer);
}

tsMAC_Address sMAC_Request;
tsMAC_Reject sRejectTable;


static teModuleStatus eJennicModuleProcessMessageIPv6(uint32_t u32Length, uint8_t *pu8Data)
{
	uint16_t checksum1,checksum, lenght, data;

	checksum1 = ((uint16_t)(*(pu8Data + 46))) << 8 | *(pu8Data + 47);
	lenght = ((uint16_t)(*(pu8Data + 4))) << 8 | *(pu8Data + 5);
	data = *(pu8Data + 6);

	if (verbosity >= LOG_DEBUG) {
		uint32_t i;

		daemon_log(LOG_DEBUG, "IPv6 from Jennic lenght %ld", u32Length);
		daemon_log(LOG_DEBUG, "Protcol %d Lenght %d Checksum %04X", data, lenght, checksum1);
		for (i = 0; i < u32Length; i++)
			printf("%02X", *(pu8Data + i));
		putchar('\n');
	}

	checksum = CalculateChecsum(u32Length, pu8Data);

	if (checksum && checksum1) {
		daemon_log(LOG_DEBUG, "BAD Calculated checksum %04X", checksum);
	}else if (*(pu8Data + 6) == 17) {	//UDP protocol
		if (memcmp(pu8Data + 24, &sModuleSetConfig.sSecurityConfig.uAuthSchemeData.sRadiusPAP.sAuthServerIP, sizeof(struct in6_addr)) == 0) {
			uint8_t temp[16];
			uint32_t i;

			sMAC_Request.MAC[0] = hexa_to_byte(pu8Data + 70);
			sMAC_Request.MAC[1] = hexa_to_byte(pu8Data + 72);
			sMAC_Request.MAC[2] = hexa_to_byte(pu8Data + 74);
			sMAC_Request.MAC[3] = hexa_to_byte(pu8Data + 76);
			sMAC_Request.MAC[4] = hexa_to_byte(pu8Data + 78);
			sMAC_Request.MAC[5] = hexa_to_byte(pu8Data + 80);
			sMAC_Request.MAC[6] = hexa_to_byte(pu8Data + 82);
			sMAC_Request.MAC[7] = hexa_to_byte(pu8Data + 84);
			daemon_log(LOG_DEBUG, "RADIUS Access Request %02X%02X%02X%02X%02X%02X%02X%02X"
				, sMAC_Request.MAC[0], sMAC_Request.MAC[1], sMAC_Request.MAC[2], sMAC_Request.MAC[3]
				, sMAC_Request.MAC[4], sMAC_Request.MAC[5], sMAC_Request.MAC[6], sMAC_Request.MAC[7]);
			
			memcpy(temp, pu8Data + 8, 16);	//change IPv6 address
			memcpy(pu8Data + 8, pu8Data + 24, 16);
			memcpy(pu8Data + 24, temp, 16);

			memcpy(temp, pu8Data + 40, 2);	//change UDP Ports
			memcpy(pu8Data + 40, pu8Data + 42, 2);
			memcpy(pu8Data + 42, temp, 2);

			*(pu8Data + 46) = 0;
			*(pu8Data + 47) = 0;
			
			uint8_t enable = 0;// = 1;

			for (i = 0;i < u16_LampsInTable;i++) {
				if (memcmp(&((psLampTable + i)->sLampStatus.sMAC_Address.MAC[0]), &sMAC_Request, sizeof(tsMAC_Address)) == 0) {
					enable = 1;
					break;
				}
			}
			if (enable || psModuleSetConfig->u8RadiusOff) {
				daemon_log(LOG_DEBUG, "Accses Accept");
				*(pu8Data + 48) = E_RADIUS_ACCESS_ACCEPT;
				*(pu8Data + 49) = rand();

				*(pu8Data + 68) = E_RADIUS_VENDOR_SPECIFIC;
				*(pu8Data + 69) = 24;
				*(pu8Data + 70) = (IANA_VENDOR_ID_NXP >> 24) & 0xff;
				*(pu8Data + 71) = (IANA_VENDOR_ID_NXP >> 16) & 0xff;
				*(pu8Data + 72) = (IANA_VENDOR_ID_NXP >> 8) & 0xff;
				*(pu8Data + 73) = (IANA_VENDOR_ID_NXP) & 0xff;

				*(pu8Data + 74) = E_RADIUS_802154_COMMISIONING_KEY;
				*(pu8Data + 75) = 18;
				for (i = 0; i < 8; i++) {
					*(pu8Data + 76 + (i << 1)) = 0;
					*(pu8Data + 76 + 1 + (i << 1)) = sMAC_Request.MAC[7-i];
				}

				checksum = 44;
			}else {
				daemon_log(LOG_DEBUG, "Accses Reject");
				uint8_t sMAC_zero[8] = { 0,0,0,0,0,0,0,0 };
				int i;
				for (i = 0; i < MAX_ACCESS_REJECT_TABLE; i++) {
					if (memcmp(&(sRejectTable.sReject[i]), &sMAC_zero, sizeof(tsMAC_Address)) == 0) {
						memcpy(&(sRejectTable.sReject[i]), &sMAC_Request, sizeof(tsMAC_Address));
						break;
					}else if (memcmp(&(sRejectTable.sReject[i]), &sMAC_Request, sizeof(tsMAC_Address)) == 0)
						break;
				}
				*(pu8Data + 48) = E_RADIUS_ACCESS_REJECT;
				*(pu8Data + 49) = rand();
				
				checksum = 20;
			}
			for (i = 0; i < 16; i++)
				*(pu8Data + 52 + i) = rand();

			*(pu8Data + 50) = checksum >> 8;
			*(pu8Data + 51) = checksum & 0xff;

			checksum += 8;

			*(pu8Data + 4) = checksum >> 8;
			*(pu8Data + 5) = checksum & 0xff;

			*(pu8Data + 7) = 0x40;	//Hop limit

			*(pu8Data + 44) = checksum >> 8;
			*(pu8Data + 45) = checksum & 0xff;


			u32Length = 40 + checksum;

			checksum = CalculateChecsum(u32Length, pu8Data);

			//if (checksum == 0)
			//	checksum = 0xffff;
			*(pu8Data + 46) = checksum >> 8;
			*(pu8Data + 47) = checksum & 0xff;

			if (verbosity >= LOG_DEBUG) {
				uint32_t i;

				for (i = 0; i < u32Length; i++)
					printf("%02X", *(pu8Data + i));
				putchar('\n');
			}
			if( eJennicModuleWriteIPv6(u32Length, pu8Data) != E_MODULE_OK)
			{
				daemon_log(LOG_ERR, "Error writing packet to module");
				return E_MODULE_ERROR;
			}
		} else if (*(pu8Data + 24) == 0xFF) {
			if (memcmp(my_local_address, pu8Data + 8, sizeof(struct in6_addr)) != 0) {
				daemon_log(LOG_DEBUG, "Multicast answer");
				daemon_log(LOG_DEBUG, "Send message");
				// Write the packet into the TUN device and let the kernel do it's stuff
				if (eTunDeviceWritePacket(u32Length, pu8Data) != E_TUN_OK)
				{
					daemon_log(LOG_ERR, "Error writing to tun device");
					return E_MODULE_ERROR;
				}
			} else {
				daemon_log(LOG_DEBUG, "Local multicast answer");
			}
		} else if ( (*(pu8Data + 8) == 0xFE) && (((*(pu8Data + 9))&0xC0) == 0x80)) {
			daemon_log(LOG_DEBUG, "Local message");
		}else if(memcmp(pu8Data + 24, &my_sors_address, sizeof(struct in6_addr)) == 0) {
			if (memcmp(pu8Data + 8, &sRouterAddress, sizeof(struct in6_addr)) == 0) {
				daemon_log(LOG_DEBUG, "To border from router answer");
				if (lenght >= 10) {
					switch (*(pu8Data + 48 + 1)) {
					case 0x11:	//Get Response
						{
							uint8_t JenNetNetworkBlobTable[] = { 0x01,0x04,0x00,0x4b };
							if (memcmp(pu8Data + 48 + 3, JenNetNetworkBlobTable, 4) == 0) {
								uint16_t u16NumberOffRemainingEntries;
								uint16_t u16TableVersion;

								u16NumberOffRemainingEntries = *(pu8Data + 48 + 7); u16NumberOffRemainingEntries <<= 8;
								u16NumberOffRemainingEntries |= *(pu8Data + 48 + 8);

								u16TableVersion = *(pu8Data + 48 + 9); u16TableVersion <<= 8;
								u16TableVersion |= *(pu8Data + 48 + 10);

								int16_t i16Lenght = lenght;
								i16Lenght -= 19;
								daemon_log(LOG_DEBUG, "JenNetNetworkBlobTable  NORE %d Version %d lenght %d", u16NumberOffRemainingEntries, u16TableVersion, i16Lenght);
								ProcesNetworkRouterTable(pu8Data + 40 + 19, i16Lenght);
							}
						}
						break;
					}
				}
			}else {
				daemon_log(LOG_DEBUG, "To border answer");
			}
		}else{
			daemon_log(LOG_DEBUG, "Send message");
			// Write the packet into the TUN device and let the kernel do it's stuff
			if (eTunDeviceWritePacket(u32Length, pu8Data) != E_TUN_OK)
			{
				daemon_log(LOG_ERR, "Error writing to tun device");
				return E_MODULE_ERROR;
			}
		}
	}else if (*(pu8Data + 6) == 58) {	//ICMP for IPv6 protocol
		daemon_log(LOG_DEBUG, "Send message");
		// Write the packet into the TUN device and let the kernel do it's stuff
		if (eTunDeviceWritePacket(u32Length, pu8Data) != E_TUN_OK)
		{
			daemon_log(LOG_ERR, "Error writing to tun device");
			return E_MODULE_ERROR;
		}
	}
	return E_MODULE_OK;
}

void inet_ntop(struct in6_addr * adr, char *buffer) {
	snprintf(buffer, INET6_ADDRSTRLEN, "%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x"
		, adr->u.Byte[0], adr->u.Byte[1], adr->u.Byte[2], adr->u.Byte[3]
		, adr->u.Byte[4], adr->u.Byte[5], adr->u.Byte[6], adr->u.Byte[7]
		, adr->u.Byte[8], adr->u.Byte[9], adr->u.Byte[10], adr->u.Byte[11]
		, adr->u.Byte[12], adr->u.Byte[13], adr->u.Byte[14], adr->u.Byte[15]);
}

static teModuleStatus eJennicModuleWriteConfig(void)
{
    if ((sFlags.uVersionKnown == 1) && (u32JennicDeviceVersion >= JENNIC_VERSION(1L,1,0)))
    {
   
        daemon_log(LOG_INFO, "Writing configuration to Module %d",sizeof(tsModule_ConfigV11));
        daemon_log(LOG_INFO, "Config 15.4 Region    : %d", sModuleSetConfig.sModuleConfigV11.u8Region);
        daemon_log(LOG_INFO, "Config 15.4 Channel   : %d", sModuleSetConfig.sModuleConfigV11.u8Channel);
        daemon_log(LOG_INFO, "Config 15.4 PAN ID    : 0x%x", htons(sModuleSetConfig.sModuleConfigV11.u16PanID));
        daemon_log(LOG_INFO, "Config JenNet ID      : 0x%lx", ntohl(sModuleSetConfig.sModuleConfigV11.u32NetworkID));
        daemon_log(LOG_INFO, "Config 6LoWPAN Prefix : 0x%08lx%08lx", ntohl(sModuleSetConfig.sModuleConfigV11.u64NetworkPrefixMSB), 
					ntohl(sModuleSetConfig.sModuleConfigV11.u64NetworkPrefixLSB));
		
        /* Send the module's configuration data */
        vSL_WriteMessage(E_SL_MSG_CONFIG, sizeof(tsModule_ConfigV11), (uint8_t*)&sModuleSetConfig.sModuleConfigV11);
    }
    else
    {
        daemon_log(LOG_ERR, "Cannot configure border router node version V%d.%d.%d", 
                   (int)((u32JennicDeviceVersion >> 16) & 0x0F), 
                   (int) ((u32JennicDeviceVersion >>  8) & 0x0F), 
                   (int) ((u32JennicDeviceVersion >>  0) & 0x0F));
        return E_MODULE_ERROR;
    }
    
    return E_MODULE_OK;
}


static teModuleStatus eJennicModuleWriteSecurityConfig(void)
{
    if(PRINT_SECURITY)
    {
        char buffer[INET6_ADDRSTRLEN] = "Could not determine Security Key";
		inet_ntop( &sModuleSetConfig.sSecurityConfig.sKey, buffer);

        daemon_log(LOG_INFO, "Enabling network security:");
        daemon_log(LOG_INFO, "Network Key           : %s", buffer);
        
        switch (ntohl(sModuleSetConfig.sSecurityConfig.eAuthScheme))
        {
            case(E_AUTH_SCHEME_NONE):
                daemon_log(LOG_INFO, "Authorisation Scheme  : None");
                break;
                
            case(E_AUTH_SCHEME_RADIUS_PAP):
            {
                char buffer[INET6_ADDRSTRLEN] = "Could not determine Security Key";
                inet_ntop( &sModuleSetConfig.sSecurityConfig.uAuthSchemeData.sRadiusPAP.sAuthServerIP, buffer);
                daemon_log(LOG_INFO, "Authorisation Scheme  : RADIUS server at %s using PAP", buffer);
                break;
            }
            
            default:
				daemon_log(LOG_ERR, "Authorisation Scheme  ERROR");
                break;
        }
    }
    
    //daemon_log(LOG_DEBUG, "Writing Module: Security Config");
		daemon_log(LOG_DEBUG, "Writing Module: Security Config %d", sizeof(tsSecurityConfig));
		
    /* Send security configuration data */
    vSL_WriteMessage(E_SL_MSG_SECURITY, sizeof(tsSecurityConfig), (uint8_t*)&sModuleSetConfig.sSecurityConfig);

    
    return E_MODULE_OK;
}


static teModuleStatus eJennicModuleWriteActivityLED(void)
{
    if (eActivityLED != E_ACTIVITY_LED_NONE)
    {
        uint8_t u8ActivityLED = (uint8_t)eActivityLED;

        daemon_log(LOG_DEBUG, "Writing Module: Activity LED: %d", u8ActivityLED);
        
        vSL_WriteMessage(E_SL_MSG_ACTIVITY_LED, sizeof(uint8_t), &u8ActivityLED);
    }
    return E_MODULE_OK;
}


teModuleStatus eJennicModuleWriteProfile(void)
{
    if (u32JennicDeviceVersion >= JENNIC_VERSION(1L,1,0))
    {
        /* Version 1.1 up supports profiles */
        daemon_log(LOG_DEBUG, "Writing Module: Set JenNet Profile (%d)", sModuleSetConfig.u8JenNetProfile & 0xff);
        
        vSL_WriteMessage(E_SL_MSG_PROFILE, sizeof(uint8_t), &sModuleSetConfig.u8JenNetProfile);
    }
    return E_MODULE_OK;
}


teModuleStatus eJennicModuleWriteFrontEndConfig(void)
{
    if (u32JennicDeviceVersion >= JENNIC_VERSION(1L,4,0))
    {
        /* Version 1.4 up support configuring radio frontend and antenna diversity*/
        daemon_log(LOG_DEBUG, "Writing Module: Set Frontend (%d)", sModuleSetConfig.eRadioFrontEnd);
        
        vSL_WriteMessage(E_SL_MSG_SET_RADIO_FRONTEND, sizeof(uint8_t),(uint8_t *) &sModuleSetConfig.eRadioFrontEnd);
        
        if (sModuleSetConfig.iAntennaDiversity)
        {
            daemon_log(LOG_DEBUG, "Writing Module: Enabling Antenna Diversity");
            
            vSL_WriteMessage(E_SL_MSG_ENABLE_DIVERSITY, 0, NULL);
        }
    }
    return E_MODULE_OK;
}


teModuleStatus eJennicModuleRun(void)
{
    if (eModuleMode == E_MODE_COORDINATOR)
    {
        daemon_log(LOG_DEBUG, "Writing Module: Run Coordinator");
        
        vSL_WriteMessage(E_SL_MSG_RUN_COORDINATOR, 0, NULL);
    }
    else if (eModuleMode == E_MODE_ROUTER)
    {
        daemon_log(LOG_DEBUG, "Writing Module: Run Router");
       
        vSL_WriteMessage(E_SL_MSG_RUN_ROUTER, 0, NULL);
    }
    else if (eModuleMode == E_MODE_COMMISSIONING)
    {
        daemon_log(LOG_DEBUG, "Writing Module: Run Commisioning");
        
        vSL_WriteMessage(E_SL_MSG_RUN_COMMISIONING, 0, NULL);
    }
    else
    {
        daemon_log(LOG_ERR, "Unknown module mode: %d", eModuleMode);
    }
    return E_MODULE_OK;
}


teModuleStatus eJennicModuleReset(void)
{
    daemon_log(LOG_DEBUG, "Writing Module: Reset");
    
    vSL_WriteMessage(E_SL_MSG_RESET, 0, NULL);
    return E_MODULE_OK;
}


teModuleStatus JennicModuleGetIPv6Address(void)
{
    daemon_log(LOG_DEBUG, "Writing Module: Get Address");
    
    sFlags.uAddressKnown = 0;
    vSL_WriteMessage(E_SL_MSG_ADDR, 0, NULL);
    return E_MODULE_OK;
}


teModuleStatus eJennicModuleWriteIPv6(uint32_t u32Length, uint8_t *pu8Data)
{
    vSL_WriteMessage(E_SL_MSG_IPV6, u32Length, pu8Data);
    return E_MODULE_OK;
}


teModuleStatus eJennicModuleWritePing(void)
{
    daemon_log(LOG_DEBUG, "Writing Module: Ping");
    
    vSL_WriteMessage(E_SL_MSG_PING, 0, NULL);
    return E_MODULE_OK;
}


teModuleStatus eJennicModuleWriteVersionRequest(void)
{
    daemon_log(LOG_DEBUG, "Writing Module: Get Version");
    
    vSL_WriteMessage(E_SL_MSG_VERSION_REQUEST , 0, NULL);
    return E_MODULE_OK;
}


teModuleStatus eJennicModuleWriteConfigRequest(void)
{
    daemon_log(LOG_DEBUG, "Writing Module: Get Config");
    
    vSL_WriteMessage(E_SL_MSG_CONFIG_REQUEST, 0, NULL);
    return E_MODULE_OK;
}


/** Detect communication failures with the border router by
 *  sending a regular ping message.
 *  Process incoming ping messages from the module.
 *  \param  u32Length   Length of received packet
 *  \param  pu8Data     Pointer to message. If NULL, this is called from state machine.
 *  \return E_MODULE_OK or E_MODULE_COMMS_FAILED on error
 */
static time_t   sLastPing = 0; 
		 
static teModuleStatus eJennicModulePing(uint32_t u32Length, uint8_t *pu8Data)
{
#define PING_INTERVAL   (10) /* Seconds between pings */
             /* Time last ping was sent */

    if (sFlags.uSupportsPing == 1)
    {
        /* Connected border router supports ping */

        if (pu8Data)
        {
            /* Ping received: time of last successful comms will be updated */
            daemon_log(LOG_DEBUG, "Pong");
 
        }
        else
        {
            if (difftime(time(NULL), sLastPing) > PING_INTERVAL)
            {
                daemon_log(LOG_DEBUG, "Ping");
                
                vSL_WriteMessage(E_SL_MSG_PING, 0, NULL);
                sLastPing = time(NULL);
            }
            else
            {
                /* Do nothing */
            }
        }
    }
    return E_MODULE_OK;
}


teModuleStatus eJennicModuleStateMachine(uint8_t bTimeout)
{
    static uint32_t u32Retries = 0;
#define MAX_VERSION_RETRIES 3
#define MAX_ADDRESS_RETRIES 10
    
	switch (eModuleState)
	{
	case (E_STATE_DETERMINE_VERSION) :
		if (sFlags.uVersionKnown == 0)
		{
			if (u32Retries)
			{
				daemon_log(LOG_DEBUG, "Timeout waiting for version");
			}
			if (++u32Retries < MAX_VERSION_RETRIES)
			{
				daemon_log(LOG_DEBUG, "Requesting version");

				eJennicModuleWriteVersionRequest();
			}
			else
			{
				u32Retries = 0;
				//eModuleState = E_STATE_CONFIGURE_NETWORK;
			}
			break;
		}
		else
		{
			u32Retries = 0;
			eModuleState = E_STATE_CONFIGURE_NETWORK;
		}
									 /* Fall through to next state if we know the version of border router node */

	case (E_STATE_CONFIGURE_NETWORK) :
			if (sModuleSetConfig.sModuleConfigV11.u64NetworkPrefixMSB == 0){
				eModuleState = E_STATE_IDLE;
				daemon_log(LOG_CRIT, "BAD config");
				break;
			}
            if( eJennicModuleWriteConfig() == E_MODULE_OK )
				eModuleState = E_STATE_CONFIGURE_SECURITY;
			else
				eModuleState = E_STATE_DETERMINE_VERSION;
            break;
        
        case (E_STATE_CONFIGURE_SECURITY):
            if (sModuleSetConfig.sSecurityConfig.eAuthScheme != htonl(E_AUTH_SCHEME_NONE))
            {
                eJennicModuleWriteSecurityConfig();
            }
            
            if (u32JennicDeviceVersion >= JENNIC_VERSION(1L,1,0))
            {
                /* Border router 1.1.0 and above support profiles */
                eModuleState = E_STATE_CONFIGURE_PROFILE;
            }
            else
            {
                eModuleState = E_STATE_START_MODULE;
            }
            break;
        
        case (E_STATE_CONFIGURE_PROFILE):
            eJennicModuleWriteProfile();
            eModuleState = E_STATE_START_MODULE;
            break;
            
        case (E_STATE_START_MODULE):
            eJennicModuleRun();
            
            if (u32JennicDeviceVersion >= JENNIC_VERSION(1L,4,0))
            {
                /* Border router 1.4.0 and above support configuring radio frontend */
                eModuleState = E_STATE_CONFIGURE_FRONTEND;
            }
            else if (u32JennicDeviceVersion >= JENNIC_VERSION(1L,1,0))
            {
                /* Version From version 1.1 on we can request the configuration from the node */
                /* It will ignore these requests until it's network is up. */
                eModuleState = E_STATE_DETERMINE_CONFIGURATION;
            }
            else
            {
                sFlags.uAddressKnown = 0;
                eModuleState = E_STATE_DETERMINE_ADDRESS;
            }
            break;

        case (E_STATE_CONFIGURE_FRONTEND):
            eJennicModuleWriteFrontEndConfig();
            eModuleState = E_STATE_DETERMINE_CONFIGURATION;
            break; 
            
        case (E_STATE_DETERMINE_CONFIGURATION):
            if (sFlags.uConfigKnown == 0)
            {
                if (bTimeout)
                {
                    /* Keep requesting configuration until the module responds */
                    daemon_log(LOG_DEBUG, "Requesting configuration");
                    
                    eJennicModuleWriteConfigRequest();
                }
                break;
            }
            else
            {
                /* Got configuration, now get the address */
                eModuleState = E_STATE_DETERMINE_ADDRESS;
                sFlags.uAddressKnown = 0;
            }
            /* Fall through to next state if we know the configuration of border router node */
            
        case (E_STATE_DETERMINE_ADDRESS):
            if (sFlags.uAddressKnown == 0)
            {
                if (bTimeout)
                {
                    /* Wait for a timeout */
                    if (u32Retries)
                    {
                        daemon_log(LOG_DEBUG, "Timeout waiting for address");
                    }
                    if (++u32Retries < MAX_ADDRESS_RETRIES)
                    {
                        daemon_log(LOG_DEBUG, "Requesting module address");
                        
                        JennicModuleGetIPv6Address();
                    }
                    else
                    {
                        daemon_log(LOG_ERR, "Cannot determine module address");
                        eModuleState    = E_STATE_DETERMINE_VERSION;
                        eJennicModuleReset();
                    }
                }
                break;
            }
            else
            {
                u32Retries = 0;
                eModuleState = E_STATE_ACTIVITY_LED;
            }
            /* Fall through to next state */
            
        case (E_STATE_ACTIVITY_LED):
            if (u32JennicDeviceVersion >= JENNIC_VERSION(1L,3,0))
            {
                /* Border router 1.3.0 and above support Activity LED */
                eJennicModuleWriteActivityLED();
            }
            eModuleState = E_STATE_RUNNING;
            break;
            
        case (E_STATE_RUNNING):
            if (eJennicModulePing(0, NULL) != E_MODULE_OK)
            {
                return E_MODULE_ERROR;
            }
            
            break;
    
            
        default:
            break;
        
    }
    
    if ((sFlags.uVersionKnown) && (sFlags.uSupportsPing))
    {
        /*struct sysinfo sSysInfo;
        if (sysinfo(&sSysInfo) != 0)
        {
            perror("sysinfo");
            return E_MODULE_ERROR;
        }*/
        if ((time(NULL)/*sSysInfo.uptime*/ - iLastSuccessfulComms) > MODULE_TIMEOUT)
        {
            daemon_log(LOG_ERR, "Node not responding (last comms %ld seconds ago)", (long)(time(NULL)/*sSysInfo.uptime*/ - iLastSuccessfulComms));
            return E_MODULE_COMMS_FAILED;
        }
    }
    return E_MODULE_OK;
}

teModuleStatus eJennicModuleStart(void)
{
    daemon_log(LOG_DEBUG, "Starting module");

	//if ( eModuleState >= E_STATE_START_MODULE)
		eJennicModuleReset();
	if( sModuleSetConfig.sModuleConfigV11.u64NetworkPrefixMSB == 0 )
		eModuleState = E_STATE_IDLE;
	else
		eModuleState = E_STATE_DETERMINE_VERSION;
    memset(&sFlags, 0, sizeof(sFlags));
    return eJennicModuleStateMachine(0);
}

static teModuleStatus eJennicModuleProcessMessageVersion(uint32_t u32Length, uint8_t *pu8Data)
{
    u32JennicDeviceVersion = 0;
    u32JennicDeviceVersion |= JENNIC_VERSION_MAJOR ((uint32_t)pu8Data[0]);
    u32JennicDeviceVersion |= JENNIC_VERSION_MINOR (pu8Data[1]);
    u32JennicDeviceVersion |= JENNIC_VERSION_REV   (pu8Data[2]);

	sRouterStatus.u32JennicDeviceVersion = htonl(u32JennicDeviceVersion);

    daemon_log(LOG_INFO, "Connected to Border router V%d.%d.%d", pu8Data[0], pu8Data[1], pu8Data[2]);

    sFlags.uVersionKnown = 1;
    
    if (u32JennicDeviceVersion >= JENNIC_VERSION(1L,1,0))
    {
        /* Version 1.1.0 and greater of the border router support ping */ 
        sFlags.uSupportsPing = 1;
    }
    
    return E_MODULE_OK;
}


static teModuleStatus eJennicModuleProcessMessageConfig(uint32_t u32Length, uint8_t *pu8Data)
{
    if (u32Length == 3)
    {
        /* This is actually a version packet in response to the config message */
        return eJennicModuleProcessMessageVersion(u32Length, pu8Data);
    }
    
    /* This is a configuration packet */
    
    if (u32JennicDeviceVersion >= JENNIC_VERSION(1L,1,0))
    {
        /* Configuration packet from 1.1 series border router */
                
		memcpy(&sModuleGetConfig.sModuleConfigV11, (tsModule_ConfigV11 *)pu8Data, sizeof(tsModule_ConfigV11));
            
		daemon_log(LOG_INFO, "Received configuration from Module");

        //if (memcmp(&sModuleGetConfig.sModuleConfigV11, &sModuleSetConfig.sModuleConfigV11, sizeof(tsModule_ConfigV11)))
        {
			uint64_t u64NewPrefix;

			u64NewPrefix = (((uint64_t)htonl(sModuleGetConfig.sModuleConfigV11.u64NetworkPrefixMSB)) << 32) | ((uint64_t)htonl(sModuleGetConfig.sModuleConfigV11.u64NetworkPrefixLSB));

           
            daemon_log(LOG_INFO, "Config 15.4 Region    : %d", sModuleGetConfig.sModuleConfigV11.u8Region);
            daemon_log(LOG_INFO, "Config 15.4 Channel   : %d", sModuleGetConfig.sModuleConfigV11.u8Channel);
            daemon_log(LOG_INFO, "Config 15.4 PAN ID    : 0x%x", ntohs(sModuleGetConfig.sModuleConfigV11.u16PanID));
            daemon_log(LOG_INFO, "Config JenNet ID      : 0x%lx", ntohl(sModuleGetConfig.sModuleConfigV11.u32NetworkID));
            daemon_log(LOG_INFO, "Config 6LoWPAN Prefix : 0x%08lx%08lx", htonl(sModuleGetConfig.sModuleConfigV11.u64NetworkPrefixMSB),
							htonl(sModuleGetConfig.sModuleConfigV11.u64NetworkPrefixLSB));
        }
        
        sFlags.uConfigKnown = 1;
    }

    return E_MODULE_OK;
}


static teModuleStatus eJennicModuleProcessMessageSecurity(uint32_t u32Length, uint8_t *pu8Data)
{
    /* This is a security configuration packet */
    
    if (u32JennicDeviceVersion >= JENNIC_VERSION(1L,1,0))
    {
        /* Configuration packet from 1.1 series border router */
        tsSecurityConfig *psSecurity = (tsSecurityConfig *)pu8Data;
        
        /* We MUST remember the configuration so that we can restart with the same network parameters.
         * Otherwise we might not be able to talk to devices again once they have joined to these parameters.
         */
        
       
        
        /* Running securely */
		
		sModuleGetConfig.sSecurityConfig.eAuthScheme = htonl(E_AUTH_SCHEME_RADIUS_PAP);
        memcpy(&sModuleGetConfig.sSecurityConfig, psSecurity, sizeof(tsSecurityConfig));
        
        daemon_log(LOG_INFO, "Received security configuration from Module");
		if (PRINT_SECURITY) {
			char buffer[INET6_ADDRSTRLEN] = "Could not determine address\n";
			inet_ntop(&sModuleGetConfig.sSecurityConfig.sKey, buffer);
			daemon_log(LOG_INFO, "Security key: %s", buffer);
		}
    }

    return E_MODULE_OK;
}


static teModuleStatus eJennicModuleProcessMessageConfigRequest(uint32_t u32Length, uint8_t *pu8Data)
{
    daemon_log(LOG_INFO, "Configuration request from module");

    /* Resend the configuration */
    eModuleState = E_STATE_CONFIGURE_NETWORK;
    
    return E_MODULE_OK;
}


static teModuleStatus eJennicModuleProcessMessageIPv6Address(uint32_t u32Length, uint8_t *pu8Data)
{
    char buffer[INET6_ADDRSTRLEN] = "Could not determine address";
    inet_ntop((struct in6_addr *)pu8Data, buffer);
	memcpy(&sRouterAddress, pu8Data, sizeof(struct in6_addr));
    daemon_log(LOG_INFO, "Module address: %s", buffer);
    
#ifdef USE_ZEROCONF
    {
        char acHostname[255];
        sprintf(acHostname, "BR_%s", cpTunDevice);
        ZC_RegisterService("JIP Border Router", acHostname, buffer);
    }
#endif /* USE_ZEROCONF */
    
	/* !!!
    {
        char acFileName[255];
        int fd;
        
        sprintf(acFileName, "/tmp/6LoWPANd.%s", cpTunDevice);
        
      
		fd = open(acFileName, O_CREAT | O_TRUNC | O_WRONLY, S_IRUSR | S_IWUSR);
        if (fd < 0)
        {
            daemon_log(LOG_ERR, "Error storing Module address: open (%s)", strerror(errno));
            return E_MODULE_ERROR;
        }
        
        if (write(fd, buffer, strlen(buffer)) < 0)
        {
            daemon_log(LOG_ERR, "Error storing Module address: write (%s)", strerror(errno));
            close(fd);
            return E_MODULE_ERROR;
        }
        
        if (write(fd, "\n", 1) < 0)
        {
            daemon_log(LOG_ERR, "Error storing Module address: write (%s)", strerror(errno));
            close(fd);
            return E_MODULE_ERROR;
        }
        
        close(fd);
    }	*/
    
    sFlags.uAddressKnown = 1;
    return E_MODULE_OK;
}


static teModuleStatus eJennicModuleProcessMessageLog(uint32_t u32Length, uint8_t *pu8Data)
{
    uint8_t u8Priority = pu8Data[0];
    
    if (u8Priority > LOG_DEBUG)
    {
        u8Priority = LOG_DEBUG;
    }
    
    /* Ensure the message is NULL terminated */
    pu8Data[u32Length] = '\0';
    
    /* Log the message */
    daemon_log(u8Priority, "Module: %s", &pu8Data[1]);
    
    return E_MODULE_OK;
}


teModuleStatus eJennicModuleProcessMessage(uint8_t u8Message, uint32_t u32Length, uint8_t *pu8Data)
{
    teModuleStatus eStatus = E_MODULE_ERROR;

    switch(u8Message)
    {
        // Handle each packet type appropriately
#define TEST(X) case (X): /*daemon_log(LOG_DEBUG, #X)*/
        TEST(E_SL_MSG_IPV6);                eStatus = eJennicModuleProcessMessageIPv6(u32Length, pu8Data);          break;
        TEST(E_SL_MSG_CONFIG);              eStatus = eJennicModuleProcessMessageConfig(u32Length, pu8Data);        break;
        TEST(E_SL_MSG_SECURITY);            eStatus = eJennicModuleProcessMessageSecurity(u32Length, pu8Data);      break;
        TEST(E_SL_MSG_ADDR);                eStatus = eJennicModuleProcessMessageIPv6Address(u32Length, pu8Data);   break;
        TEST(E_SL_MSG_CONFIG_REQUEST);      eStatus = eJennicModuleProcessMessageConfigRequest(u32Length, pu8Data); break;
        TEST(E_SL_MSG_LOG);                 eStatus = eJennicModuleProcessMessageLog(u32Length, pu8Data);           break;
        TEST(E_SL_MSG_VERSION);             eStatus = eJennicModuleProcessMessageVersion(u32Length, pu8Data);       break;
        TEST(E_SL_MSG_PING);                eStatus = eJennicModulePing(u32Length, pu8Data);                        break;
        default:                            return E_MODULE_OK; /* Move on to next message when an invalid type is received */
#undef TEST
    }
    
    // Update the time of the last successful comms with the border router
    {
       /* struct sysinfo sSysInfo;
        if (sysinfo(&sSysInfo) != 0)
        {
            perror("sysinfo");
            return E_MODULE_ERROR;
        }*/
        iLastSuccessfulComms = (long)time(NULL)/*sSysInfo.uptime*/;
    }
    
    if (eStatus == E_MODULE_OK)
    {
        eStatus = eJennicModuleStateMachine(0);
    }
    
    return eStatus;
}



