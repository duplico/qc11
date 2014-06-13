/*
 * qcxi.h
 *
 *  Created on: May 26, 2014
 *      Author: George
 */

#ifndef MAIN_H_
#define MAIN_H_

// Configuration flags (TODO: should probably be in a make file):
#define BADGE_TARGET 1
#define DEBUG_SERIAL 1

#include <stdint.h>
#include "driverlib.h"

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

void write_serial(uint8_t*);
void delay(unsigned int);

// Interrupt flags:
extern volatile uint8_t f_new_minute;

#endif /* MAIN_H_ */
