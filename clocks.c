/*
 * clocks.c
 *
 *  Created on: Jun 12, 2014
 *      Author: George
 */

#include "qcxi.h"
#include "clocks.h"
#include <string.h>

volatile Calendar currentTime;
uint8_t next_event_flag = 0;

char *event_times[] = {
		"4pm!",
		"10pm!",
		"9pm!",
		"12am!"
};

char *event_messages[] = {
		"Mixer @ IBar ",
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
//	GPIO_setAsPeripheralModuleFunctionOutputPin(
//			GPIO_PORT_P5,
//			GPIO_PIN2 + GPIO_PIN3 // XT2
//			+ GPIO_PIN4 + GPIO_PIN5 // XT1
//	);
	P5SEL |= BIT2 + BIT3 + BIT4 + BIT5;

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
			UCS_CLOCK_DIVIDER_8); // TODO
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

//	volatile uint32_t clockValue;
//	clockValue = UCS_getMCLK();
//	clockValue = UCS_getACLK();
//	clockValue = UCS_getSMCLK();
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


// TODO: refactor alarms so that we can procedurally generate the reminders.
// event has a time;
// reminder: yes/no
// light: yes/no


#pragma DATA_SECTION (alarms, ".infoB");
/*
 * NB:
 *    The order of these items in this array MUST be chronological.
 *    This is because looping through this array is how we determine which
 *    event happens next. HOWEVER, note that the ID numbers of the events
 *    are NOT chronological. This is so I can play a little trick to make
 *    the ID of an event correspond with the ID of its respective light (if
 *    it gets one).
 */
const alarm_time alarms[] = {
		{8, 16, 1, 120, 0}, // Friday mixer at iBar
		{8, 22, 5, 225, 1}, // Now: Friday pool party at Palms
		{9, 16, 2, 120, 0}, // Now: Saturday mixer at iBar
		{9, 21, 6, 0, 2}, // Now: Saturday party at Piranha
		{10, 0, 7, 0, 3}, // Now: Saturday karaoke
		{10, 16, 3, 120, 0}, // Now: Sunday mixer at iBar
		{10, 21, 4, 225, 0}, // Now: Sunday after-party
};

uint8_t offsets[] = {0, 15, 20, 25, 30, 45, 60, 0};

char alarm_msg[32] = "";

void init_alarms() {
	if (!clock_is_set)
		return;

	currentTime = RTC_A_getCalendarTime(RTC_A_BASE);

	// Find the next alarm:
	uint8_t next_alarm = 0;
	uint8_t min;
	uint8_t hour;
	uint8_t day;
	uint8_t done = 0;
	memset(alarm_msg, 0, 32);

	while (!done && next_alarm < 7) {
		uint8_t light_length = alarms[next_alarm].light_length; // TODO: should be minutes
		if (light_length)
			offsets[7] = light_length + 30;
		for (uint8_t offset_index = 0; offset_index < 8; offset_index++) {
			min = (offsets[offset_index] + 30) % 60;
			hour = (alarms[next_alarm].hour + 23 + ((offsets[offset_index]+30)/60)) % 24;
			day = alarms[next_alarm].day + (alarms[next_alarm].hour + ((offsets[offset_index]+30)/60)) / 24;

			// If current alarm is in the past or less than a minute away, go to next.
			if (day < currentTime.DayOfMonth ||
			 (day == currentTime.DayOfMonth && hour < currentTime.Hours) ||
			 (day == currentTime.DayOfMonth && hour == currentTime.Hours && min < currentTime.Minutes)) { // TODO: re-add the +1
				continue;
			}

			next_event_flag = alarms[next_alarm].event_id;

			// index != 6 => message
			if (next_alarm != 6 && offset_index == 4) {
				// now
				next_event_flag |= ALARM_NOW_MSG+ALARM_DISP_MSG;
			} else if (next_alarm != 6) {
				// disp
				next_event_flag |= ALARM_DISP_MSG;
			}

			if (next_event_flag & ALARM_DISP_MSG) {
				if (next_event_flag & ALARM_NOW_MSG)
					strcat(alarm_msg, "!!! ");
				strcat(alarm_msg, event_messages[alarms[next_alarm].msg_index]);
				if (next_event_flag & ALARM_NOW_MSG)
					strcat(alarm_msg, "NOW!");
				else
					strcat(alarm_msg, event_times[alarms[next_alarm].msg_index]);
			}

			// index < 4 => lights <= light_length exists
			if (offset_index == 0 && light_length) {
				// start light
				next_event_flag |= ALARM_START_LIGHT;
			} else if (offset_index == 7 && light_length) {
				// no msg
				next_event_flag &= ~ALARM_DISP_MSG;
				// stop light
				next_event_flag |= ALARM_STOP_LIGHT;
			}

			// If we're in between a start light and a stop light, go ahead
			//  and take it upon ourselves to issue an f_alarm now.
			if (offset_index && light_length) {
				f_alarm = ALARM_NO_REINIT + ALARM_START_LIGHT + alarms[next_alarm].event_id;
			}

			// Set the next alarm!
			RTC_A_setCalendarAlarm(
					RTC_A_BASE,
					min,
					hour,
					RTC_A_ALARMCONDITION_OFF,
					day
			);
			done = 1;

			break;
		}
		next_alarm++;
		// TODO: set_event_occurred(uint8_t id);
	}
	if (next_alarm == 7) {
		// queercon is over.
		// TODO: set_event_occurred(uint8_t id)
		return;
	}
}

void init_rtc() {
	//Starting Time for Calendar: // TODO:
	currentTime.Seconds    = 40;
	currentTime.Minutes    = 59;
	currentTime.Hours      = 17;
	currentTime.DayOfWeek  = 6;
	currentTime.DayOfMonth = 8;
	currentTime.Month      = 8;
	currentTime.Year       = 2014;

	clock_is_set = 1; // TODO: remove.

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
