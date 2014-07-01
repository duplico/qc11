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

#define RFM_MODE_TX 0b00001100
#define RFM_MODE_RX 0b00010000
#define RFM_MODE_SB 0b00000100

void rfm_write_register_sync(uint8_t, uint8_t*, uint8_t);
void rfm_write_single_register_sync(uint8_t, uint8_t);
void rfm_read_register_sync(uint8_t, uint8_t);
uint8_t rfm_copy_register_sync(uint8_t, uint8_t, uint8_t*);
uint8_t rfm_read_single_register_sync(uint8_t);
void rfm_mode_rx_sync();
void rfm_mode_sb_sync();
void rfm_mode_tx_sync();
void rfm_mode_tx_async();
void rfm_mode_async(uint8_t);
void rfm_mode_sync(uint8_t);
uint8_t rfm_process_async();

void rfm_send_sync(uint8_t *, uint8_t);
void rfm_send_async(uint8_t *, uint8_t, uint8_t);

uint8_t rfm_crcok();
void rfm_init();

#endif /* RADIO_H_ */
