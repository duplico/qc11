/*
 * qcxi.h
 *
 *  Created on: May 26, 2014
 *      Author: George
 */

#ifndef MAIN_H_
#define MAIN_H_

#include <stdint.h>
#include "driverlib.h"

#define BADGE_TARGET 1
#define BIT15 0x8000

#define WRITE_IF(port, pin, val) if (val) GPIO_setOutputHighOnPin(port, pin); else GPIO_setOutputLowOnPin(port, pin)
#define GPIO_pulse(port, pin) do { GPIO_setOutputHighOnPin(port, pin); GPIO_setOutputLowOnPin(port, pin); } while (0)

#if BADGE_TARGET
#include <msp430f5308.h>
#define NSS_PORT GPIO_PORT_P4
#define NSS_PIN GPIO_PIN7
#else
#include <msp430f5529.h>
#define NSS_PORT GPIO_PORT_P3
#define NSS_PIN GPIO_PIN7
#endif


extern char received_data_str[2];
extern void init_radio();
extern void write_serial(uint8_t*);
extern void mode_rx_sync();
extern uint8_t rfm_crcok();

#endif /* MAIN_H_ */
