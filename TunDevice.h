#ifndef  TUNDEVICE_H_INCLUDED
#define  TUNDEVICE_H_INCLUDED

#include <stdint.h>
//#include <netinet/in.h>

#define PIN_STR "9971"
//#define PIN_STR "5357"
//#define PIN_STR "4095"

extern char pin[];

#define SIZE_ipv6_buf 2048
extern unsigned char ipv6_buf[SIZE_ipv6_buf];

extern volatile unsigned int butes_reciv;

#if defined __cplusplus
extern "C" {
#endif

/****************************************************************************/
/***        Include files                                                 ***/
/****************************************************************************/

/****************************************************************************/
/***        Macro Definitions                                             ***/
/****************************************************************************/

/****************************************************************************/
/***        Type Definitions                                              ***/
/****************************************************************************/


/** Enumerated type of status codes */
typedef enum
{
    E_TUN_OK,
    E_TUN_ERROR,
} teTunStatus;


/****************************************************************************/
/***        Local Function Prototypes                                     ***/
/****************************************************************************/

/****************************************************************************/
/***        Exported Variables                                            ***/
/****************************************************************************/


/****************************************************************************/
/***        Local Variables                                               ***/
/****************************************************************************/

/****************************************************************************/
/***        Exported Functions                                            ***/
/****************************************************************************/


/** Read available data from the tun device
 *  \return E_TUN_OK if all ok
 */
teTunStatus eTunDeviceReadPacket(void);


/** Write available data to the tun device
 *  \param u32Length    Amount of data available
 *  \param pu8Data      Data to write
 *  \return E_TUN_OK if data written ok
 */
teTunStatus eTunDeviceWritePacket(uint32_t u32Length, uint8_t *pu8Data);

/****************************************************************************/
/***        Local Functions                                               ***/
/****************************************************************************/

#if defined __cplusplus
}
#endif

#endif  /* TUNDEVICE_H_INCLUDED */

/****************************************************************************/
/***        END OF FILE                                                   ***/
/****************************************************************************/

