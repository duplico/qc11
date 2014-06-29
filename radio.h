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


void write_register(uint8_t, uint8_t*, uint8_t);
void write_single_register(uint8_t, uint8_t);
void read_register(uint8_t, uint8_t);
uint8_t read_register_sync(uint8_t, uint8_t, uint8_t*);
uint8_t read_single_register_sync(uint8_t);
void mode_rx_sync();
void mode_sb_sync();
void mode_tx_sync();
void mode_tx_async();

void radio_send(uint8_t*, uint8_t);

uint8_t rfm_crcok();
void init_radio();

#endif /* RADIO_H_ */
