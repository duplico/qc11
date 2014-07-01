/*
 * clocks.c
 *
 *  Created on: Jun 12, 2014
 *      Author: George
 */

#include "qcxi.h"
#include "clocks.h"

volatile Calendar currentTime;

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


	if (xt1_status == STATUS_FAIL) {
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
		// REFO is automatically disabled when not sourcing anything.
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

	// Initializes the DCO to operate at the given frequency below,
	//  using the FLL (note XT1 is its input above)
	//  (this will set SMCLK and MCLK to use DCO, so we'll need to reinitialize
	//   them after we setup the DCO/FLL)
	UCS_initFLLSettle(
			DCO_DESIRED_FREQUENCY_IN_KHZ, // 8000
			DCO_FLLREF_RATIO			   // 8 MHz / 32KHz
	);

	// Use the DCO as the master clock.
	// Divide by 8 to get a MCLK of 1 MHz
	// TODO: Decide if this is the right frequency or not.
#if BADGE_TARGET
	UCS_clockSignalInit(UCS_MCLK, UCS_DCOCLKDIV_SELECT,
			UCS_CLOCK_DIVIDER_8);
#else
	UCS_clockSignalInit(UCS_MCLK, UCS_DCOCLKDIV_SELECT,
			UCS_CLOCK_DIVIDER_1);
#endif
	// if not badge_target we'll need to use DCO for SMCLK too, probably:

#if BADGE_TARGET
	// Init XT2:
	xt2_status = UCS_XT2StartWithTimeout(
			UCS_XT2DRIVE_8MHZ_16MHZ,
			UCS_XT2_TIMEOUT
	);
	if (xt2_status == STATUS_FAIL) {
		// XT2 is broken.
		// Fall back to using the DCO at 8 MHz (ish)
		UCS_clockSignalInit(
			UCS_SMCLK,
			UCS_DCOCLKDIV_SELECT,
			UCS_CLOCK_DIVIDER_1
		);
	}
	else {
		// XT2 is not broken:
		// Select XT2 as SMCLK source
		UCS_clockSignalInit(
			UCS_SMCLK,
			UCS_XT2CLK_SELECT,
			UCS_CLOCK_DIVIDER_2 // Divide by 2 to get 8 MHz.
		);
	}
#else
	xt2_status = STATUS_SUCCESS;
#endif

	// Enable global oscillator fault flag
	SFR_clearInterrupt(SFR_OSCILLATOR_FAULT_INTERRUPT);
	SFR_enableInterrupt(SFR_OSCILLATOR_FAULT_INTERRUPT);

	volatile uint32_t clockValue;
	// TODO: Verify if the clock settings are as expected:
	clockValue = UCS_getMCLK();
	clockValue = UCS_getACLK();
	clockValue = UCS_getSMCLK();

}

void init_timers() {

}

void init_rtc() {

	//Starting Time for Calendar:
	currentTime.Seconds    = 0x00;
	currentTime.Minutes    = 0x19;
	currentTime.Hours      = 0x18;
	currentTime.DayOfWeek  = 0x03;
	currentTime.DayOfMonth = 0x20;
	currentTime.Month      = 0x07;
	currentTime.Year       = 0x2011;

	//Initialize Calendar Mode of RTC
	RTC_A_calendarInit(RTC_A_BASE,
			currentTime,
			RTC_A_FORMAT_BCD);

	//Interrupt to every minute with a CalendarEvent
	RTC_A_setCalendarEvent(RTC_A_BASE,
			RTC_A_CALENDAREVENT_MINUTECHANGE);

	//Enable interrupt for RTC Ready Status, which asserts when the RTC
	//Calendar registers are ready to read.
	RTC_A_clearInterrupt(RTC_A_BASE,
			RTCRDYIFG + RTCTEVIFG + RTCAIFG);
	RTC_A_enableInterrupt(RTC_A_BASE,
			RTCRDYIE + RTCTEVIE + RTCAIE);

	//Start RTC Clock
	RTC_A_startClock(RTC_A_BASE);
}


void init_watchdog() {
	WDT_A_hold(WDT_A_BASE);
}
