/*
 * clocks.c
 *
 *  Created on: Jun 12, 2014
 *      Author: George
 */

#include "qcxi.h"
#include "clocks.h"

void init_clocks() {

	UCS_setExternalClockSource(
			UCS_XT1_CRYSTAL_FREQUENCY,
			UCS_XT2_CRYSTAL_FREQUENCY
	);

	//Port select XT1
	GPIO_setAsPeripheralModuleFunctionInputPin(
			GPIO_PORT_P5,
			GPIO_PIN4 + GPIO_PIN5
	);

	xt1_status = UCS_LFXT1StartWithTimeout(
		UCS_XT1_DRIVE0,
		UCS_XCAP_3,
		65535
	);


	if (xt1_status == STATUS_FAIL) { // TODO remove compare and switch if around
		// XT1 is broken.
		// Fall back to REFO ////////////////////////////////////
		UCS_XT1Off();                                          //
		UCS_clockSignalInit(                                   //
				UCS_ACLK,                                      //
				UCS_REFOCLK_SELECT,                            //
				UCS_CLOCK_DIVIDER_1                            //
		);                                                     //
		UCS_clockSignalInit(UCS_FLLREF, UCS_REFOCLK_SELECT,    //
				UCS_CLOCK_DIVIDER_1);                          //
		                                                       //
		/////////////////////////////////////////////////////////
	}
	else { // XT1 is not broken:
		// Select XT1 as ACLK source
		UCS_clockSignalInit(
			UCS_ACLK,
			UCS_XT1CLK_SELECT,
			UCS_CLOCK_DIVIDER_1
		);
		// Select XT1 as the input to the FLL reference.
		UCS_clockSignalInit(UCS_FLLREF, UCS_XT1CLK_SELECT,
				UCS_CLOCK_DIVIDER_1);
	}

	// Init XT2:
	xt2_status = UCS_XT2StartWithTimeout(
			UCS_XT2DRIVE_8MHZ_16MHZ,
			UCS_XT2_TIMEOUT
	);

	// Setup the clocks:


	//Select XT2 as SMCLK source
	UCS_clockSignalInit(
			UCS_SMCLK,
			UCS_XT2CLK_SELECT,
			UCS_CLOCK_DIVIDER_2 // Divide by 2 to get 8 MHz.
	);


	UCS_initFLLSettle(
			MCLK_DESIRED_FREQUENCY_IN_KHZ,
			MCLK_FLLREF_RATIO
	);

	// Use the DCO paired with XT1+FLL as the master clock
	// This will run at around 1 MHz. This is the default.
	//                     (1,048,576 Hz)
	UCS_clockSignalInit(UCS_MCLK, UCS_DCOCLKDIV_SELECT,
			UCS_CLOCK_DIVIDER_1);

	// Enable global oscillator fault flag
	SFR_clearInterrupt(SFR_OSCILLATOR_FAULT_INTERRUPT);
	SFR_enableInterrupt(SFR_OSCILLATOR_FAULT_INTERRUPT);

	uint32_t clockValue;
	// TODO: Verify if the clock settings are as expected:
	clockValue = UCS_getMCLK();
	clockValue = UCS_getACLK();
	clockValue = UCS_getSMCLK();

}

void init_timers() {

}

void init_rtc() {

}


void init_watchdog() {
	WDT_A_hold(WDT_A_BASE);
}
