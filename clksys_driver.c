#include "avr_compiler.h"
#include "clksys_driver.h"

/*! \brief This function configures the internal high-frequency PLL.
 *
 *  Configuration of the internal high-frequency PLL to the correct
 *  values. It is used to define the input of the PLL and the factor of
 *  multiplication of the input clock source.
 *
 *  \note Note that the oscillator cannot be used as a main system clock
 *        source without being enabled and stable first. Check the ready flag
 *        before using the clock. The macro CLKSYS_IsReady( _oscSel )
 *        can be used to check this.
 *
 *  \param  clockSource Reference clock source for the PLL,
 *                      must be above 0.4MHz.
 *  \param  factor      PLL multiplication factor, must be
 *                      from 1 to 31, inclusive.
 */
void CLKSYS_PLL_Config( OSC_PLLSRC_t clockSource, uint8_t factor )
{
	factor &= OSC_PLLFAC_gm;
	OSC.PLLCTRL = (uint8_t) clockSource | ( factor << OSC_PLLFAC_gp );
}


uint8_t CLKSYS_Disable( uint8_t oscSel )
{
	OSC.CTRL &= ~oscSel;
	uint8_t clkEnabled = OSC.CTRL & oscSel;
	return clkEnabled;
}

/*! \brief CCP write helper function written in assembly.
 *
 *  This function is written in assembly because of the timecritial
 *  operation of writing to the registers.
 *
 *  \param address A pointer to the address to write to.
 *  \param value   The value to put in to the register.
 */
void CCPWrite( volatile uint8_t * address, uint8_t value )
{
#ifdef __ICCAVR__

	// Store global interrupt setting in scratch register and disable interrupts.
        asm("in  R1, 0x3F \n"
	    "cli"
	    );

	// Move destination address pointer to Z pointer registers.
	asm("movw r30, r16");
#ifdef RAMPZ
	asm("ldi  R16, 0 \n"
            "out  0x3B, R16"
	    );

#endif
	asm("ldi  r16,  0xD8 \n"
	    "out  0x34, r16  \n"
#if (__MEMORY_MODEL__ == 1)
	    "st     Z,  r17  \n");
#elif (__MEMORY_MODEL__ == 2)
	    "st     Z,  r18  \n");
#else /* (__MEMORY_MODEL__ == 3) || (__MEMORY_MODEL__ == 5) */
	    "st     Z,  r19  \n");
#endif /* __MEMORY_MODEL__ */

	// Restore global interrupt setting from scratch register.
        asm("out  0x3F, R1");

#elif defined __GNUC__
	AVR_ENTER_CRITICAL_REGION( );
	volatile uint8_t * tmpAddr = address;
#ifdef RAMPZ
	RAMPZ = 0;
#endif
	asm volatile(
		"movw r30,  %0"	      "\n\t"
		"ldi  r16,  %2"	      "\n\t"
		"out   %3, r16"	      "\n\t"
		"st     Z,  %1"       "\n\t"
		:
		: "r" (tmpAddr), "r" (value), "M" (CCP_IOREG_gc), "i" (&CCP)
		: "r16", "r30", "r31"
		);

	AVR_LEAVE_CRITICAL_REGION( );
#endif
}

/*! \brief This function changes the prescaler configuration.
 *
 *  Change the configuration of the three system clock
 *  prescaler is one single operation. The user must make sure that
 *  the main CPU clock does not exceed recommended limits.
 *
 *  \param  PSAfactor   Prescaler A division factor, OFF or 2 to 512 in
 *                      powers of two.
 *  \param  PSBCfactor  Prescaler B and C division factor, in the combination
 *                      of (1,1), (1,2), (4,1) or (2,2).
 */
void CLKSYS_Prescalers_Config( CLK_PSADIV_t PSAfactor,
                               CLK_PSBCDIV_t PSBCfactor )
{
	uint8_t PSconfig = (uint8_t) PSAfactor | PSBCfactor;
	CCPWrite( &CLK.PSCTRL, PSconfig );
}

/*! \brief This function selects the main system clock source.
 *
 *  Hardware will disregard any attempts to select a clock source that is not
 *  enabled or not stable. If the change fails, make sure the source is ready
 *  and running and try again.
 *
 *  \param  clockSource  Clock source to use as input for the system clock
 *                       prescaler block.
 *
 *  \return  Non-zero if change was successful.
 */
uint8_t CLKSYS_Main_ClockSource_Select( CLK_SCLKSEL_t clockSource )
{
	uint8_t clkCtrl = ( CLK.CTRL & ~CLK_SCLKSEL_gm ) | clockSource;
	CCPWrite( &CLK.CTRL, clkCtrl );
	clkCtrl = ( CLK.CTRL & clockSource );
	return clkCtrl;
}

/*! \brief This function configures the external oscillator.
 *
 *  \note Note that the oscillator cannot be used as a main system clock
 *        source without being enabled and stable first. Check the ready flag
 *        before using the clock. The macro CLKSYS_IsReady( _oscSel )
 *        can be used to check this.
 *
 *  \param  freqRange          Frequency range for high-frequency crystal, does
 *                             not apply for external clock or 32kHz crystals.
 *  \param  lowPower32kHz      True of high-quality watch crystals are used and
 *                             low-power oscillator is desired.
 *  \param  xoscModeSelection  Combined selection of oscillator type (or
 *                             external clock) and startup times.
 */
void CLKSYS_XOSC_Config( OSC_FRQRANGE_t freqRange,
                         bool lowPower32kHz,
                         OSC_XOSCSEL_t xoscModeSelection )
{
	OSC.XOSCCTRL = (uint8_t) freqRange |
	               ( lowPower32kHz ? OSC_X32KLPM_bm : 0 ) |
	               xoscModeSelection;
}

void CLOCK_Init(void){
		/* Enable for external 12-16 MHz crystal with quick startup time
		 * (256CLK). Check if it's stable and set the external
		 * oscillator as the main clock source. Wait for user input
		 * while the LEDs toggle.
		 */
		CLKSYS_XOSC_Config( OSC_FRQRANGE_12TO16_gc,
		                    false,
		                    OSC_XOSCSEL_XTAL_256CLK_gc );
		CLKSYS_Enable( OSC_XOSCEN_bm );
		do {} while ( CLKSYS_IsReady( OSC_XOSCRDY_bm ) == 0 );
		
		
		/*  Configure PLL external 12-16 MHz crystal oscillator as source and
		 *  multiply by 2 to get 294,91200 MHz PLL clock and enable it. Wait
		 *  for it to be stable. Disable unused clock 
		 */
		CLKSYS_PLL_Config( OSC_PLLSRC_XOSC_gc, 2 );
		CLKSYS_Enable( OSC_PLLEN_bm );
		do {} while ( CLKSYS_IsReady( OSC_PLLRDY_bm ) == 0 );
		CLKSYS_Main_ClockSource_Select( CLK_SCLKSEL_PLL_gc );
		CLKSYS_Disable( OSC_RC2MEN_bm );
}