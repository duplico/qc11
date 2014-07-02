/*
 * qcxi.h
 *
 *  Created on: May 26, 2014
 *      Author: George
 */

#ifndef MAIN_H_
#define MAIN_H_

// Configuration flags (TODO: should probably be in a make file):
#define BADGE_TARGET 0
#define DEBUG_SERIAL 0

#define SYNC0 0b11001100
#define SYNC1 0b01010101

#include <stdint.h>
#include "driverlib.h"

#define BIT15 0x8000

#define WRITE_IF(port, pin, val) if (val) GPIO_setOutputHighOnPin(port, pin); else GPIO_setOutputLowOnPin(port, pin)
#define GPIO_pulse(port, pin) do { GPIO_setOutputHighOnPin(port, pin); GPIO_setOutputLowOnPin(port, pin); } while (0)

#if BADGE_TARGET
// Target is the actual badge:
#include <msp430f5308.h>

// IR:
#define IR_USCI_BASE USCI_A1_BASE
#define IR_USCI_VECTOR USCI_A1_VECTOR
#define IR_TXRX_PORT GPIO_PORT_P4
#define IR_SD_PORT GPIO_PORT_P4
#define IR_TX_PIN GPIO_PIN4
#define IR_RX_PIN GPIO_PIN5
#define IR_SD_PIN GPIO_PIN6
#define IRIV UCA1IV

#define IRTCTL UCA1IRTCTL
#define IRRCTL UCA1IRRCTL

// Radio:
#define RFM_NSS_PORT GPIO_PORT_P4
#define RFM_NSS_PIN GPIO_PIN7

// Crystal:
#define UCS_XT2_CRYSTAL_FREQUENCY 16000000

#else
// Target is the Launchpad+shield:
#include <msp430f5329.h>

// IR:
#define IR_USCI_BASE USCI_A0_BASE
#define IR_USCI_VECTOR USCI_A0_VECTOR
#define IRIV UCA0IV
#define IR_TXRX_PORT GPIO_PORT_P3
#define IR_SD_PORT GPIO_PORT_P8
#define IR_TX_PIN GPIO_PIN3
#define IR_RX_PIN GPIO_PIN4
#define IR_SD_PIN GPIO_PIN1

#define IRTCTL UCA0IRTCTL
#define IRRCTL UCA0IRRCTL

// Radio:
#define RFM_NSS_PORT GPIO_PORT_P3
#define RFM_NSS_PIN GPIO_PIN7

// Crystal:
#define UCS_XT2_CRYSTAL_FREQUENCY 4000000

// WS2812 LEDs:
extern uint8_t ws_frameBuffer[];

#endif

void delay(unsigned int);


extern uint8_t ir_tx_frame[];

extern volatile uint8_t ir_rx_frame[];
extern volatile uint8_t ir_rx_len;

extern volatile uint8_t ir_rx_index;

extern volatile uint8_t ir_xmit;
extern volatile uint8_t ir_xmit_index;
extern volatile uint8_t ir_xmit_len;
extern uint8_t ir_reject_loopback;

// Interrupt flags:
extern volatile uint8_t f_new_minute;
extern volatile uint8_t f_ir_tx_done;
extern volatile uint8_t f_ir_rx_ready;
extern volatile uint8_t f_animate;
extern uint8_t f_animation_done; // not actually set from interrupt

#endif /* MAIN_H_ */
