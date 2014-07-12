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
#define DEBUG_SERIAL 0


// Memory organization (same for F5529 and F5308)
#define INFOA_START 0x001980

#define SYNC0 0xAD
#define SYNC1 0xFB
#define SYNC2 0xCA
#define SYNC3 0xDE

#include <stdint.h>
#include "driverlib.h"

#define BIT15 0x8000

#define WRITE_IF(port, pin, val) if (val) GPIO_setOutputHighOnPin(port, pin); else GPIO_setOutputLowOnPin(port, pin)
#define GPIO_pulse(port, pin) do { GPIO_setOutputHighOnPin(port, pin); GPIO_setOutputLowOnPin(port, pin); } while (0)

typedef struct {
	uint16_t paired_ids[10];
	uint16_t met_ids[10];
	uint16_t scores[4];
	uint8_t events_occurred;
	uint8_t events_attended;
	uint8_t badge_id;
	uint16_t datetime[2];
	uint8_t handle[11];
	uint8_t message[17];
	uint16_t crc;
} qcxiconf;
extern qcxiconf my_conf;

void check_config();
uint8_t post();

#define POST_XT1F 	0b1
#define POST_XT2F 	0b10
#define POST_SHIFTF 0b100
#define POST_IRGF	0b1000
#define POST_IRIF 	0b10000
#define POST_IRVF 	0b100000
#define POST_RRF	0b1000000
#define POST_RTF	0b10000000

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

// Debug serial
extern volatile char ser_buffer_rx[255];
extern volatile char ser_buffer_tx[255];
extern volatile uint8_t f_ser_rx;
void ser_print(char*);
void ser_init();

// Radio:
#define RFM_NSS_PORT GPIO_PORT_P3
#define RFM_NSS_PIN GPIO_PIN7

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

extern uint8_t ir_proto_state;

// Interrupt flags:
extern volatile uint8_t f_new_minute;
extern volatile uint8_t f_ir_tx_done;
extern volatile uint8_t f_ir_rx_ready;
extern volatile uint8_t f_animate;
extern uint8_t f_animation_done; // not actually set from interrupt
extern volatile uint8_t f_rfm_rx_done;
extern uint8_t f_config_clobbered; // not actually set from interrupt
extern volatile uint8_t f_new_second;
extern uint8_t f_paired; // not actually set from interrupt

#endif /* MAIN_H_ */
