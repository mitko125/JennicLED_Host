#undef NO_COORDINATOR



struct in6_addr {
	union {
		uint8_t  Byte[16];
		uint16_t Word[8];
	} u;
};

#ifndef true
#define true 1
#define false 0
#endif

#define INT_BCD(A) ( ((A/10)<<4) | (A%10) )
#define BCD_INT(A) ( ((A>>4)*10) + (A&0x0F) )

#define SIZE_RAM 0x8000

#ifndef WIN32

//#ifdef __BYTE_ORDER__
//  #if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    #define HTONS(x) ((uint16_t) (((uint16_t) (x) << 8) | ((uint16_t) (x) >> 8)))
    #define HTONL(x) ((uint32_t) (((uint32_t) HTONS(x) << 16) | HTONS((uint32_t) (x) >> 16)))
//  #elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
 //   #define HTONS(x) ((uint16_t) (x))
 //   #define HTONL(x) ((uint32_t) (x))
 // #else
 //   #error Byte order not supported!
 // #endif
//#else
//  #error Byte order not defined!
//#endif
 
#define NTOHS(x) HTONS(x)
#define NTOHL(x) HTONL(x)
 
static inline uint16_t htons(uint16_t x) {
  return HTONS(x);
}
static inline uint32_t htonl(uint32_t x) {
  return HTONL(x);
}
static inline uint16_t ntohs(uint16_t x) {
  return htons(x);
}
static inline uint32_t ntohl(uint32_t x) {
  return htonl(x);
}

typedef uint32_t time_t;
#define difftime(A,B) (A-B)
extern uint32_t time_1s;
#define time(A) time_1s
#endif	//WIN32

#ifdef __GNUC__
#define PACKED( class_to_pack ) class_to_pack __attribute__((__packed__))
#else
#define PACKED( class_to_pack ) __pragma( pack(push, 1) ) class_to_pack __pragma( pack(pop) )
#endif

#ifndef INET6_ADDRSTRLEN
#define INET6_ADDRSTRLEN 46
#endif

/** Enumerated type of statuses */
typedef enum
{
	E_MODULE_OK,
	E_MODULE_ERROR,
	E_MODULE_COMMS_FAILED,
} teModuleStatus;

/** Enumerated type of allowable certification regions */
typedef enum
{
	E_REGION_EUROPE,
	E_REGION_USA,
	E_REGION_JAPAN,

	E_REGION_MAX
} teRegion;


/** Enumerated type of allowable channels */
typedef enum
{
	E_CHANNEL_AUTOMATIC = 0,
	E_CHANNEL_MINIMUM = 11,
	E_CHANNEL_MAXIMUM = 26
} teChannel;


/** Enumerated type of supported authorisation schemes */
typedef enum
{
	E_AUTH_SCHEME_NONE,
	E_AUTH_SCHEME_RADIUS_PAP,

	E_AUTH_SCHEME_DUMMY = 2147483647,	/**< Force this emumeration to be 4 bytes as sent by the host. */
} teAuthScheme;


/** Per authorisation scheme union of required configuration data */
typedef union
{
	struct
	{
		struct in6_addr sAuthServerIP;
	} sRadiusPAP;
} tuAuthSchemeData;


/** Enumerated type of supported radio front ends */
#ifdef WIN32
typedef enum
{
	E_FRONTEND_STANDARD_POWER,          /**< No frontend - just a standard power device */
	E_FRONTEND_HIGH_POWER,              /**< High power module - enable PA and LNA */
	E_FRONTEND_ETSI,                    /**< Enable ETSI compliant mode */
}teRadioFrontEnd;
#else //WIN32
typedef enum
{
	E_FRONTEND_STANDARD_POWER,          /**< No frontend - just a standard power device */
	E_FRONTEND_HIGH_POWER,              /**< High power module - enable PA and LNA */
	E_FRONTEND_ETSI,                    /**< Enable ETSI compliant mode */
} __attribute__((__packed__)) teRadioFrontEnd;
#endif //WIN32

/** Structure definition to configure the operating parameters of the network
*  This verison of the structure is used for the 1.1.X series border routers
*/
PACKED(
	typedef struct
{
	uint8_t     u8Region;
	uint8_t     u8Channel;
	uint16_t    u16PanID;
	uint32_t    u32NetworkID;
	uint32_t    u64NetworkPrefixMSB;
	uint32_t    u64NetworkPrefixLSB;
})tsModule_ConfigV11;

/** Structure definition to configure the security parameters of the network */
PACKED(
	typedef struct
{
	struct in6_addr  sKey;                      /**< Store key like an IPv6 address. That gets us round the endianness issues */

	teAuthScheme eAuthScheme;
	tuAuthSchemeData uAuthSchemeData;
})tsSecurityConfig;

PACKED(
	typedef struct
{
	tsModule_ConfigV11	sModuleConfigV11;
	tsSecurityConfig	sSecurityConfig;
	uint8_t				eRadioFrontEnd;
	uint8_t				u8JenNetProfile;
	uint8_t			  iAntennaDiversity;
	uint8_t			  u8RadiusOff;
	uint16_t			u16LampsInTable;
	uint16_t			u16LampsConnected;
	//V5
	uint8_t				u8INT_resetGPRShours;
	uint8_t				u8INT_resetGPRSminuts;
	uint8_t				u8EnableEnergyMeter;
})tsConfigBorderRuter;

PACKED(
	typedef struct
{
	uint8_t     u8Hour;
	uint8_t     u8Minute;
})tsTimerHourMinute;

PACKED(
typedef struct
{
	tsTimerHourMinute sTimerOn;
	tsTimerHourMinute sTimerOff;
	uint8_t     u8Lights;
} )tsGrourTimer;

PACKED(
typedef struct
{
	unsigned char date_time[6];
}) tsDateTime;

#define MAX_GROUP_TIMERS 16

PACKED(
	typedef struct
{
	tsDateTime sDateTime;
	tsTimerHourMinute sTimerOn1;
	tsTimerHourMinute sTimerOff1;
	tsGrourTimer sGroupTimer[MAX_GROUP_TIMERS];
	//V2
	tsTimerHourMinute sTimerOn2;
	tsTimerHourMinute sTimerOff2;
})tsTimers;

PACKED(
	typedef struct
{
	uint8_t MAC[8];
})tsMAC_Address;

#define MAX_ACCESS_REJECT_TABLE 16	//25
PACKED(
	typedef struct
{
	tsMAC_Address sReject[MAX_ACCESS_REJECT_TABLE];
})tsMAC_Reject;

PACKED(
	typedef struct
{
	tsMAC_Address	sMAC_Address;
	tsDateTime	sLastContacts;
	uint32_t	u32WorkHours;
})tsLampStatus;

PACKED(
	typedef struct
{
	tsLampStatus	sLampStatus;
	uint32_t u32Minutes;
	uint32_t u32OldMinutes;
	uint8_t u8FlSee;
})tsLamp;

PACKED(
typedef struct
{
	uint16_t u16FirstTableEntry;
	uint16_t u16EntryCount;
	uint8_t u8FlagEnd;
})tsSendTable;

#define ROUTE_TABLE_ENTRIES		300	//300	//Profile 4 е за 50-150 устройсва, но го оставяме на 300
#define MAX_SEND_MAC			20
#define MAX_SEND_LAMP_STATUS	10				

PACKED(
	typedef struct
{
	uint16_t u16SimErrors;
	uint16_t u16OnCounter;
	tsDateTime sDateTimeClearRAM;
	tsDateTime sDateTimeOn;
	tsDateTime sDateTimeOff;
	tsDateTime sLastDateTime;
	uint8_t u8CSQ;
	uint8_t u8JenniceModuleState;
	uint32_t u32HostVersion;
	uint32_t u32JennicDeviceVersion;
	uint8_t u8Inputs;
	uint8_t u8Outputs;
	tsDateTime sDateTimeResetGPRS;	// V4 > V3.0.0
	tsDateTime sDateTimeLastClient;	// V4 > V3.0.0 PC няма нужда да го чете,чете се само от Концентратора
})tsRouterStatus;

PACKED(
	typedef struct
{
	tsMAC_Address	sMAC_Address;
	uint32_t	u32WorkHours;
})tsSetWorkHours;

PACKED(
	typedef struct
{
	tsDateTime	sLastContact;
	
	float L1_Voltage;
	float L2_Voltage;
	float L3_Voltage;
	float Grid_frequency;
	float L1_Current;
	float L2_Current;
	float L3_Current;
	float Active_power;
	float L1_Active_power;
	float L2_Active_power;
	float L3_Active_power;
	float Reactive_power;
	float L1_Reactive_power;
	float L2_Reactive_power;
	float L3_Reactive_power;
	float Apparent_power;
	float L1_Apparent_power;
	float L2_Apparent_power;
	float L3_Apparent_power;
	float Power_factor;
	float L1_Power_factor;
	float L2_Power_factor;
	float L3_Power_factor;
})tsCurrentEnergy;

PACKED(
	typedef struct
{
	tsDateTime	sLastContact;
	
	float Total_active_energy;
	float T1_Total_active_energy;
	float T2_Total_active_energy;
	float L1_Total_active_energy;
	float L2_Total_active_energy;
	float L3_Total_active_energy;
	
	float Total_reactive_energy;
	float T1_Total_reactive_energy;
	float T2_Total_reactive_energy;
	float L1_Total_reactive_energy;
	float L2_Total_reactive_energy;
	float L3_Total_reactive_energy;
	
})tsTotalEnergy;


PACKED(
	typedef struct
{
	tsDateTime	DateTime;
	
	/*float L1_Voltage;
	float L2_Voltage;
	float L3_Voltage;
	float Grid_frequency;
	float L1_Current;
	float L2_Current;
	float L3_Current;*/
	float Active_power;
	float L1_Active_power;
	float L2_Active_power;
	float L3_Active_power;
/*	float Reactive_power;
	float L1_Reactive_power;
	float L2_Reactive_power;
	float L3_Reactive_power;
	float Apparent_power;
	float L1_Apparent_power;
	float L2_Apparent_power;
	float L3_Apparent_power;
	float Power_factor;
	float L1_Power_factor;
	float L2_Power_factor;
	float L3_Power_factor;*/
	
})tsCurrentEnergySmall;

#define MAX_CURRENT_ENERGY 450
#define MAX_SEND_CURRENT_ENEGY_ARRAY 10

PACKED(
	typedef struct
{
	uint16_t	NextInArray;
	tsCurrentEnergySmall sCurrentEnergySmall[MAX_CURRENT_ENERGY];
})tsCurrenEnegryArray;

PACKED(
typedef struct
{
	tsDateTime	reversDateTimeStart;
	tsDateTime	reversDateTimeEnd;
	uint16_t u16FirstArrayEntry;
	uint16_t u16EntryArrayCount;
	uint8_t u8FlagEnd;
})tsSendEnergyArray;

#define CRD2_RX_BUFFER_SIZE 1024
#define CRD2_TX_BUFFER_SIZE 1024

#define GPRS_RX_BUFFER_SIZE 1024
#define GPRS_TX_BUFFER_SIZE 1024

#define MODBUS_RX_BUFFER_SIZE 512
#define MODBUS_TX_BUFFER_SIZE 512

#define OFFSET_psModuleSetConfig 0
#define psModuleSetConfig ((tsConfigBorderRuter *)(p_E_RAM+OFFSET_psModuleSetConfig))
#define sModuleSetConfig (*psModuleSetConfig)

#define OFFSET_psTimers (OFFSET_psModuleSetConfig + sizeof(tsConfigBorderRuter) + 1 )	// + 1	crc psModuleSetConfig
#define psTimers ((tsTimers*)(p_E_RAM+OFFSET_psTimers))

#define OFFSET_u16LampsInTable (OFFSET_psTimers + sizeof(tsTimers) + 1 )	// + 1	crc psTimers
#define u16_LampsInTable (*((uint16_t*)(p_E_RAM+OFFSET_u16LampsInTable)))

#define OFFSET_u16LampsConnected (OFFSET_u16LampsInTable + sizeof(uint16_t))
#define u16_LampsConnected (*((uint16_t*)(p_E_RAM+OFFSET_u16LampsConnected)))

#define OFFSET_psLampTable (OFFSET_u16LampsConnected + sizeof(uint16_t))
#define psLampTable ((tsLamp*)(p_E_RAM+OFFSET_psLampTable))

#define OFFSET_sRouterStatus (OFFSET_psLampTable + (sizeof(tsLamp)*ROUTE_TABLE_ENTRIES))
#define sRouterStatus (*((tsRouterStatus*)(p_E_RAM+OFFSET_sRouterStatus)))

#define OFFSET_u8CRD2_RX_Buf (OFFSET_sRouterStatus + sizeof(tsRouterStatus))
#define u8CRD2_RX_Buf ((uint8_t*)(p_E_RAM+OFFSET_u8CRD2_RX_Buf))

#define OFFSET_u8CRD2_TX_Buf (OFFSET_u8CRD2_RX_Buf + CRD2_RX_BUFFER_SIZE)
#define u8CRD2_TX_Buf ((uint8_t*)(p_E_RAM+OFFSET_u8CRD2_TX_Buf))

#define OFFSET_u8GPRS_RX_Buf (OFFSET_u8CRD2_TX_Buf + CRD2_TX_BUFFER_SIZE)
#define uGPRS_RX_Buf ((uint8_t*)(p_E_RAM+OFFSET_u8GPRS_RX_Buf))

#define OFFSET_u8GPRS_TX_Buf (OFFSET_u8GPRS_RX_Buf + GPRS_RX_BUFFER_SIZE)
#define u8GPRS_TX_Buf ((uint8_t*)(p_E_RAM+OFFSET_u8GPRS_TX_Buf))

#define OFFSET_u8MODBUS_RX_Buf (OFFSET_u8GPRS_TX_Buf + GPRS_TX_BUFFER_SIZE)
#define u8MODBUS_RX_Buf ((uint8_t*)(p_E_RAM+OFFSET_u8MODBUS_RX_Buf))

#define OFFSET_u8MODBUS_TX_Buf (OFFSET_u8MODBUS_RX_Buf + MODBUS_RX_BUFFER_SIZE)
#define u8MODBUS_TX_Buf ((uint8_t*)(p_E_RAM+OFFSET_u8MODBUS_TX_Buf))

#define OFFSET_sCurrentEnergy (OFFSET_u8MODBUS_TX_Buf + MODBUS_TX_BUFFER_SIZE)
#define sCurrentEnergy (*((tsCurrentEnergy*)(p_E_RAM+OFFSET_sCurrentEnergy)))

#define OFFSET_sTotalEnergy (OFFSET_sCurrentEnergy + sizeof(tsCurrentEnergy))
#define sTotalEnergy (*((tsTotalEnergy*)(p_E_RAM+OFFSET_sTotalEnergy)))

#define OFFSET_sCurrentEnergyArray (OFFSET_sTotalEnergy + sizeof(tsTotalEnergy))
#define sCurrenEnegryArray (*((tsCurrenEnegryArray*)(p_E_RAM+OFFSET_sCurrentEnergyArray)))

#define END_NVM (OFFSET_sCurrentEnergyArray + sizeof(tsCurrenEnegryArray))

