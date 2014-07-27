/*
 * radio.h
 *
 *  Created on: May 26, 2014
 *      Author: George
 */

#ifndef RADIO_H_
#define RADIO_H_

#include <stdint.h>

#define RFM_FIFO 	0x00
#define RFM_OPMODE 	0x01

#define RFM_IRQ1 0x27
#define RFM_IRQ2 0x28

#define RFM_BROADCAST 0xff

// The protocol machine:
extern volatile uint8_t rfm_proto_state;
extern volatile uint8_t rfm_reg_state;

#define RFM_PROTO_RX_IDLE 0
#define RFM_PROTO_RX_FIFO 1
#define RFM_PROTO_SB_UNSET_CMD 2
#define RFM_PROTO_SB_UNSET_DAT 3
#define RFM_PROTO_SB_FIFO 4
#define RFM_PROTO_TX 5
#define RFM_PROTO_RX_UNSET_CMD 6
#define RFM_PROTO_RX_UNSET_DAT 7

#define RFM_REG_IDLE			0
#define RFM_REG_RX_SINGLE_CMD	1
#define RFM_REG_RX_SINGLE_DAT	2
#define RFM_REG_TX_SINGLE_CMD	3
#define RFM_REG_TX_SINGLE_DAT	4
#define RFM_REG_RX_FIFO_CMD		5
#define RFM_REG_RX_FIFO_DAT		6
#define RFM_REG_TX_FIFO_CMD		7
#define RFM_REG_TX_FIFO_DAT		8
#define RFM_REG_TX_FIFO_AM		9

#define RFM_AUTOMODE_RX 0b01100101
#define RFM_AUTOMODE_TX 0b01011011

void init_radio();

void write_single_register(uint8_t, uint8_t);
uint8_t read_single_register_sync(uint8_t);
void mode_rx_sync();
void mode_sb_sync();
void mode_tx_sync();
void mode_tx_async();

void radio_send_sync();
void radio_send_half_async();
void radio_send_async();

uint8_t rfm_crcok();

#endif /* RADIO_H_ */
