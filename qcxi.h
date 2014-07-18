/*
 * qcxi.h
 *
 *  Created on: May 26, 2014
 *      Author: George
 */

#ifndef QCXI_H_
#define QCXI_H_

#include <stdint.h>
#include "driverlib.h"

// Configuration flags
#define BADGE_TARGET 1
#define DEBUG_SERIAL 0

// Memory organization (same for F5529 and F5308)
#define INFOA_START 0x001980

// Configuration of pins for the badge and launchpad
#if BADGE_TARGET
	// Target is the actual badge:
	#include <msp430f5308.h>

	// IR:
	#define IR_USCI_BASE USCI_A1_BASE
	#define IR_USCI_VECTOR USCI_A1_VECTOR
	#define IRIV UCA1IV
	#define IRTCTL UCA1IRTCTL
	#define IRRCTL UCA1IRRCTL
	#define IR_TXRX_PORT GPIO_PORT_P4
	#define IR_SD_PORT GPIO_PORT_P4
	#define IR_TX_PIN GPIO_PIN4
	#define IR_RX_PIN GPIO_PIN5
	#define IR_SD_PIN GPIO_PIN6

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
	#define IRTCTL UCA0IRTCTL
	#define IRRCTL UCA0IRRCTL
	#define IR_TXRX_PORT GPIO_PORT_P3
	#define IR_SD_PORT GPIO_PORT_P8
	#define IR_TX_PIN GPIO_PIN3
	#define IR_RX_PIN GPIO_PIN4
	#define IR_SD_PIN GPIO_PIN1

	// Radio:
	#define RFM_NSS_PORT GPIO_PORT_P3
	#define RFM_NSS_PIN GPIO_PIN7

#endif

// Useful defines:
#define BIT15 0x8000
#define WRITE_IF(port, pin, val) if (val) GPIO_setOutputHighOnPin(port, pin); else GPIO_setOutputLowOnPin(port, pin)
#define GPIO_pulse(port, pin) do { GPIO_setOutputHighOnPin(port, pin); GPIO_setOutputLowOnPin(port, pin); } while (0)

// The delay function, which we don't really want to use much, please.
void delay(unsigned int);

// For setting up our time-based loop:
// 128 Hz / 4 = 32 Hz
#define TIME_LOOP_HZ 32
#define TIME_LOOP_SCALER RTC_A_PSEVENTDIVIDER_4


extern uint8_t clock_is_set;

// Power-on self test result codes:
#define POST_XT1F 	0b1
#define POST_XT2F 	0b10
#define POST_SHIFTF 0b100
#define POST_IRGF	0b1000
#define POST_IRIF 	0b10000
#define POST_IRVF 	0b100000
#define POST_RRF	0b1000000
#define POST_RTF	0b10000000

// Infrared communication:
#define SYNC0 0xAD
#define SYNC1 0xFB
#define SYNC2 0xCA
#define SYNC3 0xDE

extern volatile uint8_t ir_rx_frame[]; // Used in POST
extern uint8_t ir_reject_loopback;
extern uint8_t ir_proto_state;
extern uint8_t ir_proto_seqnum;
extern volatile uint8_t loops_to_ir_timestep;

#define IR_LOOPS_PER_SEC 8
#define IR_LOOPS TIME_LOOP_HZ / IR_LOOPS_PER_SEC

#define IR_SECONDS_PER_BEACON 2
#define IR_LOOPS_PER_BEACON IR_LOOPS * IR_SECONDS_PER_BEACON

#define IR_SECONDS_TO_TIMEOUT 3
#define IR_PROTO_TTO IR_LOOPS * IR_SECONDS_TO_TIMEOUT

#define SECONDS_TO_PAIR 5
#define ITPS_TO_PAIR IR_LOOPS_PER_SEC * SECONDS_TO_PAIR
#define ITPS_TO_SHOW_PAIRING ITPS_TO_PAIR / 2

// Persistent configuration:
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

// Flags for interrupt service routines to signal the main thread:
extern volatile uint8_t f_new_minute;
extern volatile uint8_t f_ir_tx_done;
extern volatile uint8_t f_ir_rx_ready;
extern volatile uint8_t f_time_loop;
extern volatile uint8_t f_rfm_rx_done;
extern volatile uint8_t f_new_second;
extern volatile uint8_t f_alarm;

// Additional flags for signaling the main thread, not from interrupts:
extern uint8_t f_animation_done;
extern uint8_t f_paired;
extern uint8_t f_unpaired;
extern uint8_t f_paired_new_person;
extern uint8_t f_paired_new_trick;
extern uint8_t f_ir_itp_step;
extern uint8_t f_ir_pair_abort;

typedef struct {
	uint8_t to_addr, from_addr, base_id, puppy_flags, clock_authority,
			seconds, minutes, hours, day, month;
	uint16_t year, clock_age_seconds;
	uint8_t prop_id, prop_time_loops_before_start, prop_from;
} qcxipayload;
extern qcxipayload in_payload, out_payload;

// Extra features for the Launchpad version - serial and LED chains:
#if !BADGE_TARGET
	// Debug serial
	extern volatile char ser_buffer_rx[255];
	extern volatile char ser_buffer_tx[255];
	extern volatile uint8_t f_ser_rx;
	void ser_print(char*);
	void ser_init();
	// WS2812 LEDs:
	extern uint8_t ws_frameBuffer[];
#endif

#endif /* QCXI_H_ */
