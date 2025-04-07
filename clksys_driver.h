/*! \brief This macro check if selected oscillator is ready.
 *
 *  This macro will return non-zero if is is running, regardless if it is
 *  used as a main clock source or not.
 *
 *  \param _oscSel Bitmask of selected clock. Can be one of the following
 *                 OSC_RC2MEN_bm, OSC_RC32MEN_bm, OSC_RC32KEN_bm, OSC_XOSCEN_bm,
 *                 OSC_PLLEN_bm.
 *
 *  \return  Non-zero if oscillator is ready and running.
 */
#define CLKSYS_IsReady( _oscSel ) ( OSC.STATUS & (_oscSel) )

/*! \brief This macro enables the selected oscillator.
 *
 *  \note Note that the oscillator cannot be used as a main system clock
 *        source without being enabled and stable first. Check the ready flag
 *        before using the clock. The function CLKSYS_IsReady( _oscSel )
 *        can be used to check this.
 *
 *  \param  _oscSel Bitmask of selected clock. Can be one of the following
 *                  OSC_RC2MEN_bm, OSC_RC32MEN_bm, OSC_RC32KEN_bm, OSC_XOSCEN_bm,
 *                  OSC_PLLEN_bm.
 */
#define CLKSYS_Enable( _oscSel ) ( OSC.CTRL |= (_oscSel) )

/*! \brief This function disables the selected oscillator.
 *
 *  This function will disable the selected oscillator if possible.
 *  If it is currently used as a main system clock source, hardware will
 *  disregard the disable attempt, and this function will return zero.
 *  If it fails, change to another main system clock source and try again.
 *
 *  \param oscSel  Bitmask of selected clock. Can be one of the following
 *                 OSC_RC2MEN_bm, OSC_RC32MEN_bm, OSC_RC32KEN_bm,
 *                 OSC_XOSCEN_bm, OSC_PLLEN_bm.
 *
 *  \return  Non-zero if oscillator was disabled successfully.
 */
 
 void CCPWrite( volatile uint8_t * address, uint8_t value );
 
 void CLOCK_Init(void);