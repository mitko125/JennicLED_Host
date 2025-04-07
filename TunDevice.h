#ifndef  TUNDEVICE_H_INCLUDED
#define  TUNDEVICE_H_INCLUDED

#include <stdint.h>
//#include <netinet/in.h>

#define PIN_STR "9971"
//#define PIN_STR "5357"
//#define PIN_STR "4095"

extern char pin[];

extern uint8_t key_a,key_b;
extern uint8_t t_min_no_connect;

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


/** Open tun device
 *  \param dev          Name of device to create
 *  \return E_TUN_OK if opened ok
 */
teTunStatus eTunDeviceOpen(int port, uint32_t baud);


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

void TunLoop(void);

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

