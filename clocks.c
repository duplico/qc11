/*
 * clocks.c
 *
 *  Created on: Jun 12, 2014
 *      Author: George
 */

#include "qcxi.h"
#include "clocks.h"

volatile Calendar currentTime;
uint8_t next_event_flag = 0;

char *event_times[8] = {
		"",
		"4pm!",
		"4pm!",
		"4pm!",
		"",
		"10pm!",
		"9pm!",
		"12am!"
};

char *event_messages[8] = {
		"",
		"Mixer @ IBar ",
		"Mixer @ IBar ",
		"Mixer @ IBar ",
		"",
		"Pool party @ Palms ",
		"Party @ Piranha ",
		"Karaoke ",
};


void init_clocks() {

	UCS_setExternalClockSource(
			UCS_XT1_CRYSTAL_FREQUENCY,
			UCS_XT2_CRYSTAL_FREQUENCY
	);


	// External crystal pins //////////////////////////////////////////////////
	//  __ X1 (32768 Hz)
	// |  |----P5.4
	// |__|----P5.5
	//
	//  __ X2 (16 MHz (badge) or 4 MHz (LP)
	// |  |----P5.2
	// |__|----P5.3
	//
	GPIO_setAsPeripheralModuleFunctionOutputPin(
			GPIO_PORT_P5,
			GPIO_PIN2 + GPIO_PIN3 // XT2
			+ GPIO_PIN4 + GPIO_PIN5 // XT1
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

	// Use the DCO as the master clock.
	// Divide by 8 to get a MCLK of 1 MHz
#if BADGE_TARGET

	// Init XT2:
	xt2_status = UCS_XT2StartWithTimeout(
			UCS_XT2DRIVE_8MHZ_16MHZ,
			UCS_XT2_TIMEOUT
	);

	// Initializes the DCO to operate at the given frequency below,
	//  using the FLL (note XT1 is its input above)
	//  (this will set SMCLK and MCLK to use DCO, so we'll need to reinitialize
	//   them after we setup the DCO/FLL)
	UCS_initFLLSettle(
			DCO_DESIRED_FREQUENCY_IN_KHZ, // 8000
			DCO_FLLREF_RATIO			   // 8 MHz / 32KHz
	);
	UCS_clockSignalInit(UCS_MCLK, UCS_DCOCLKDIV_SELECT,
			UCS_CLOCK_DIVIDER_8);
#else

	// Init XT2:
	xt2_status = UCS_XT2StartWithTimeout(
			UCS_XT2DRIVE_4MHZ_8MHZ,
			UCS_XT2_TIMEOUT
	);

	UCS_clockSignalInit(UCS_FLLREF, UCS_XT2CLK_SELECT,
			UCS_CLOCK_DIVIDER_1);
	// Initializes the DCO to operate at the given frequency below,
	//  using the FLL (note XT1 is its input above)
	//  (this will set SMCLK and MCLK to use DCO, so we'll need to reinitialize
	//   them after we setup the DCO/FLL)
	UCS_initFLLSettle(
			24000,
			6
	);
//	UCS_clockSignalInit(UCS_MCLK, UCS_DCOCLKDIV_SELECT,
//			UCS_CLOCK_DIVIDER_1);
	UCS_clockSignalInit(UCS_SMCLK, UCS_DCOCLKDIV_SELECT,
			UCS_CLOCK_DIVIDER_1);
#endif
	// if not badge_target we'll need to use DCO for SMCLK too, probably:

#if BADGE_TARGET
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
	clockValue = UCS_getMCLK();
	clockValue = UCS_getACLK();
	clockValue = UCS_getSMCLK();

}

// We should get the following data in a flag to main from the alarm:
// LSB
//	3 bits: next event ID (starts with 1 because the opening party is 0)
//	1 bit: display a message (0 for no, 1 for yes)
//  1 bit: which message (0 for reminder, 1 for now)
//	1 bit: start blinking light (0 for no, 1 for yes)
//  1 bit: stop blinking light (0 for no, 1 for yes)
//  1 bit: Prop
// MSB

/*
 * NB:
 *    The order of these items in this array MUST be chronological.
 *    This is because looping through this array is how we determine which
 *    event happens next. HOWEVER, note that the ID numbers of the events
 *    are NOT chronological. This is so I can play a little trick to make
 *    the ID of an event correspond with the ID of its respective light (if
 *    it gets one).
 */
const alarm_time alarms[49] = {
		{0x08, 0x15, 0x30, 1 + ALARM_DISP_MSG + ALARM_START_LIGHT}, // Reminder: Friday mixer at iBar
		{0x08, 0x15, 0x45, 1 + ALARM_DISP_MSG}, // Reminder: Friday mixer at iBar
		{0x08, 0x15, 0x50, 1 + ALARM_DISP_MSG}, // Reminder: Friday mixer at iBar
		{0x08, 0x15, 0x55, 1 + ALARM_DISP_MSG}, // Reminder: Friday mixer at iBar
		{0x08, 0x16, 0x00, 1 + ALARM_DISP_MSG + ALARM_NOW_MSG}, // Now: Friday mixer at iBar
		{0x08, 0x16, 0x15, 1 + ALARM_DISP_MSG + ALARM_NOW_MSG}, // Now: Friday mixer at iBar
		{0x08, 0x16, 0x30, 1 + ALARM_DISP_MSG + ALARM_NOW_MSG}, // Now: Friday mixer at iBar
		{0x08, 0x17, 0x00, 1 + ALARM_STOP_LIGHT}, // End: Friday mixer at iBar

		{0x08, 0x21, 0x30, 5 + ALARM_DISP_MSG + ALARM_START_LIGHT}, // Reminder: Friday pool party at Palms
		{0x08, 0x21, 0x45, 5 + ALARM_DISP_MSG}, // Reminder: Friday pool party at Palms
		{0x08, 0x21, 0x50, 5 + ALARM_DISP_MSG}, // Reminder: Friday pool party at Palms
		{0x08, 0x21, 0x55, 5 + ALARM_DISP_MSG}, // Reminder: Friday pool party at Palms
		{0x08, 0x22, 0x00, 5 + ALARM_DISP_MSG + ALARM_NOW_MSG}, // Now: Friday pool party at Palms
		{0x08, 0x22, 0x15, 5 + ALARM_DISP_MSG + ALARM_NOW_MSG}, // Now: Friday pool party at Palms
		{0x08, 0x22, 0x30, 5 + ALARM_DISP_MSG + ALARM_NOW_MSG}, // Now: Friday pool party at Palms
		{0x09, 0x03, 0x00, 5 + ALARM_STOP_LIGHT}, // End: Friday pool party at Palms

		{0x09, 0x15, 0x30, 2 + ALARM_DISP_MSG + ALARM_START_LIGHT}, // Reminder: Saturday mixer at iBar
		{0x09, 0x15, 0x45, 2 + ALARM_DISP_MSG}, // Reminder: Saturday mixer at iBar
		{0x09, 0x15, 0x50, 2 + ALARM_DISP_MSG}, // Reminder: Saturday mixer at iBar
		{0x09, 0x15, 0x55, 2 + ALARM_DISP_MSG}, // Reminder: Saturday mixer at iBar
		{0x09, 0x16, 0x00, 2 + ALARM_DISP_MSG + ALARM_NOW_MSG}, // Now: Saturday mixer at iBar
		{0x09, 0x16, 0x15, 2 + ALARM_DISP_MSG + ALARM_NOW_MSG}, // Now: Saturday mixer at iBar
		{0x09, 0x16, 0x30, 2 + ALARM_DISP_MSG + ALARM_NOW_MSG}, // Now: Saturday mixer at iBar
		{0x09, 0x17, 0x00, 2 + ALARM_STOP_LIGHT}, // End: Saturday mixer at iBar

		{0x09, 0x20, 0x30, 6 + ALARM_DISP_MSG}, // Reminder: Saturday party at Piranha
		{0x09, 0x20, 0x45, 6 + ALARM_DISP_MSG}, // Reminder: Saturday party at Piranha
		{0x09, 0x20, 0x50, 6 + ALARM_DISP_MSG}, // Reminder: Saturday party at Piranha
		{0x09, 0x20, 0x55, 6 + ALARM_DISP_MSG}, // Reminder: Saturday party at Piranha
		{0x09, 0x21, 0x00, 6 + ALARM_DISP_MSG + ALARM_NOW_MSG}, // Now: Saturday party at Piranha
		{0x09, 0x21, 0x15, 6 + ALARM_DISP_MSG + ALARM_NOW_MSG}, // Now: Saturday party at Piranha
		{0x09, 0x21, 0x30, 6 + ALARM_DISP_MSG + ALARM_NOW_MSG}, // Now: Saturday party at Piranha

		{0x09, 0x23, 0x30, 7 + ALARM_DISP_MSG}, // Reminder: Saturday karaoke
		{0x09, 0x23, 0x45, 7 + ALARM_DISP_MSG}, // Reminder: Saturday karaoke
		{0x09, 0x23, 0x50, 7 + ALARM_DISP_MSG}, // Reminder: Saturday karaoke
		{0x09, 0x23, 0x55, 7 + ALARM_DISP_MSG}, // Reminder: Saturday karaoke
		{0x10, 0x00, 0x00, 7 + ALARM_DISP_MSG + ALARM_NOW_MSG}, // Now: Saturday karaoke
		{0x10, 0x00, 0x15, 7 + ALARM_DISP_MSG + ALARM_NOW_MSG}, // Now: Saturday karaoke
		{0x10, 0x00, 0x30, 7 + ALARM_DISP_MSG + ALARM_NOW_MSG}, // Now: Saturday karaoke

		{0x10, 0x15, 0x30, 3 + ALARM_DISP_MSG + ALARM_START_LIGHT}, // Reminder: Sunday mixer at iBar
		{0x10, 0x15, 0x45, 3 + ALARM_DISP_MSG}, // Reminder: Sunday mixer at iBar
		{0x10, 0x15, 0x50, 3 + ALARM_DISP_MSG}, // Reminder: Sunday mixer at iBar
		{0x10, 0x15, 0x55, 3 + ALARM_DISP_MSG}, // Reminder: Sunday mixer at iBar
		{0x10, 0x16, 0x00, 3 + ALARM_DISP_MSG + ALARM_NOW_MSG}, // Now: Sunday mixer at iBar
		{0x10, 0x16, 0x15, 3 + ALARM_DISP_MSG + ALARM_NOW_MSG}, // Now: Sunday mixer at iBar
		{0x10, 0x16, 0x30, 3 + ALARM_DISP_MSG + ALARM_NOW_MSG}, // Now: Sunday mixer at iBar
		{0x10, 0x17, 0x00, 3 + ALARM_STOP_LIGHT}, // End: Sunday mixer at iBar

		{0x10, 0x09, 0x30, 4 + ALARM_START_LIGHT}, // Reminder: Sunday after-party
		{0x10, 0x09, 0x00, 4}, // Now: Sunday after-party
		{0x11, 0x03, 0x00, 4 + ALARM_STOP_LIGHT}, // End: Sunday after-party
};

void init_alarms() {
	if (!clock_is_set)
		return;

	currentTime = RTC_A_getCalendarTime(RTC_A_BASE);

	// Find the next alarm:
	uint8_t next_alarm = 0;
	while (next_alarm < 49) {
		// If current alarm is in the past or less than a minute away, go to next.
		if (alarms[next_alarm].day < currentTime.DayOfMonth &&
				alarms[next_alarm].hour < currentTime.Hours &&
				alarms[next_alarm].min < currentTime.Minutes + 1) { // TODO: I think there's some wrong math here...
			next_alarm++;
		} else {
			next_event_flag = alarms[next_alarm].flag;
			break;
		}
		// TODO: If we're in between a start light and a stop light, go ahead
		//  and take it upon ourselves to issue an f_alarm now.
	}
	if (next_alarm == 49) {
		// queercon is over.
		// TODO
		return;
	}

	// Set the next alarm!
	RTC_A_setCalendarAlarm(
			RTC_A_BASE,
			alarms[next_alarm].min,
			alarms[next_alarm].hour,
			RTC_A_ALARMCONDITION_OFF,
			alarms[next_alarm].day
	);
}


void init_rtc() {
	//Starting Time for Calendar: // TODO:
	currentTime.Seconds    = 00;
	currentTime.Minutes    = 19;
	currentTime.Hours      = 18;
	currentTime.DayOfWeek  = 03;
	currentTime.DayOfMonth = 20;
	currentTime.Month      = 07;
	currentTime.Year       = 2011;

	//Initialize Calendar Mode of RTC
	RTC_A_calendarInit(RTC_A_BASE,
			currentTime,
			RTC_A_FORMAT_BINARY);

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

#pragma vector=RTC_VECTOR
__interrupt
void RTC_A_ISR(void)
{
	switch (__even_in_range(RTCIV, 16)) {
	case 0: break;  //No interrupts
	case 2:         //RTCRDYIFG // TODO: make sure this is on
		f_new_second = 1;
		__bic_SR_register_on_exit(LPM3_bits);
		break;
	case 4:         //RTCEVIFG
		//Interrupts every minute // TODO: make sure this is on if we need it
		f_new_minute = 1;
		__bic_SR_register_on_exit(LPM3_bits);
		break;
	case 6:         //RTCAIFG
		f_alarm = next_event_flag;
		break;
	case 8: break;  //RT0PSIFG
	case 10:		// Rollover of prescale counter
		f_time_loop = 1; // We know what it does! It's a TIME LOOP MACHINE.
		// ...who would build a device that loops time every 32 milliseconds?
		// WHO KNOWS. But that's what it does.
		__bic_SR_register_on_exit(LPM3_bits);
		break; //RT1PSIFG
	case 12: break; //Reserved
	case 14: break; //Reserved
	case 16: break; //Reserved
	default: break;
	}
}
