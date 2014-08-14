/*
 * clocks.h
 * (c) 2014 George Louthan
 * 3-clause BSD license; see license.md.
 */

#ifndef CLOCKS_H_
#define CLOCKS_H_

// For clock initialization:
#define UCS_XT1_TIMEOUT 50000
#define UCS_XT2_TIMEOUT 50000
#define UCS_XT1_CRYSTAL_FREQUENCY	32768

#if BADGE_TARGET

#define UCS_XT2_CRYSTAL_FREQUENCY 16000000
#define DCO_DESIRED_FREQUENCY_IN_KHZ 8000
#define DCO_FLLREF_RATIO 8000000 / UCS_REFOCLK_FREQUENCY
#define MCLK_DESIRED_FREQUENCY_IN_KHZ 1000

#define MCLK_FREQUENCY 1000000
#define SMCLK_FREQUENCY 8000000

#else

#define UCS_XT2_CRYSTAL_FREQUENCY 4000000

#define DCO_DESIRED_FREQUENCY_IN_KHZ 24000
#define DCO_FLLREF_RATIO 6
#define MCLK_DESIRED_FREQUENCY_IN_KHZ 24000

#define MCLK_FREQUENCY 24000000
#define SMCLK_FREQUENCY 12000000

#endif

uint8_t xt2_status;
uint8_t xt1_status; // 1 = working, 0 = fault condition

#define ALARM_NO_REINIT		BIT7
#define ALARM_STOP_LIGHT 	BIT6
#define ALARM_START_LIGHT 	BIT5
#define ALARM_NOW_MSG 		BIT4
#define ALARM_DISP_MSG 		BIT3

void init_clocks();
void init_alarms();
void init_rtc();
void init_watchdog();

extern char alarm_msg[];
extern uint8_t next_event_flag;

typedef struct {
	uint8_t day;
	uint8_t hour;
	uint8_t event_id;
	uint8_t light_length;
	uint8_t msg_index;
} alarm_time;

extern char *event_times[8];
extern char *event_messages[8];

extern volatile Calendar currentTime;

#endif /* CLOCKS_H_ */
