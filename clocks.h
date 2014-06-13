/*
 * clocks.h
 *
 *  Created on: Jun 12, 2014
 *      Author: George
 */

#ifndef CLOCKS_H_
#define CLOCKS_H_

// For clock initialization:
#define UCS_XT1_TIMEOUT 50000
#define UCS_XT2_TIMEOUT 50000
#define UCS_XT1_CRYSTAL_FREQUENCY	32768
#define UCS_XT2_CRYSTAL_FREQUENCY	16000000
#define MCLK_DESIRED_FREQUENCY_IN_KHZ 8000
#define MCLK_FLLREF_RATIO MCLK_DESIRED_FREQUENCY_IN_KHZ / (UCS_REFOCLK_FREQUENCY / 1024)

uint8_t xt2_status;
uint8_t xt1_status; // 1 = working, 0 = fault condition

void init_clocks();
void init_timers();
void init_rtc();
void init_watchdog();

extern volatile Calendar currentTime;

#endif /* CLOCKS_H_ */
