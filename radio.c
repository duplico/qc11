/*
 * radio.c
 *
 *  Created on: May 26, 2014
 *      Author: George
 */

#include "radio.h"
#include "qcxi.h"

#include <stdint.h>
#include "driverlib.h"
#include <string.h>

#define SPICLK 9600

uint8_t returnValue = 0;

volatile uint8_t rfm_read_reg_data[64] = {0};

volatile uint8_t rfm_write_reg_data[64] = {0};
volatile uint8_t rfm_writing = 0;
volatile uint8_t rfm_write_len = 0;
volatile uint8_t rfm_write_index = 0;

volatile uint8_t rfm_reg_index = 0;
volatile uint8_t rfm_reg_len = 0;
volatile uint8_t rfm_begin = 0;
volatile uint8_t rfm_rw_reading  = 0; // 0- read, 1- write
volatile uint8_t rfm_rw_single = 0; // 0- single, 1- fifo
volatile uint8_t rfm_single_msg = 0;
volatile uint8_t rfm_fifo[64] = {0};

// We can be doing:
//  read single
//  write single
//  read fifo
//  write fifo

uint8_t rfm_zeroes[64] = {0};
uint8_t rfm_reg_data_index = 0;
uint8_t rfm_reg_data_length = 0;
volatile uint8_t rfm_reg_data_ready = 0;
uint8_t frame_bytes_remaining = 0;

void init_radio() {

	// SPI for radio //////////////////////////////////////////////////////////
	//
	// P4.1 ---- MOSI (TX) >>--|
	// P4.2 ---- MISO (RX) <<--| RFM69CW
	// P4.3 ----------- CLK ---|
	// P4.7 ----------- NSS ---|
	//                         |
	//                         |
	// P2.0 --------- DIO0 <<--|
	//

	//P3.5,4,0 option select
	GPIO_setAsPeripheralModuleFunctionInputPin(
		GPIO_PORT_P4,
		GPIO_PIN2
	);

	GPIO_setAsPeripheralModuleFunctionOutputPin(
			GPIO_PORT_P4,
			GPIO_PIN1 + GPIO_PIN3
	);

	GPIO_setAsOutputPin(RFM_NSS_PORT, RFM_NSS_PIN);
	GPIO_setOutputHighOnPin(RFM_NSS_PORT, RFM_NSS_PIN); // NSS is active low.

	// SPI to RFM /////////////////////////////////////////////////////////////
	//
	// Initialize the SPI for talking to the radio

	// TODO: clock for radio for Launchpad? I don't think it actually matters...

	returnValue = USCI_B_SPI_masterInit(
		USCI_B1_BASE,
		USCI_B_SPI_CLOCKSOURCE_SMCLK, // selectClockSource
		UCS_getSMCLK(),
		SPICLK,
		USCI_B_SPI_MSB_FIRST,
		USCI_B_SPI_PHASE_DATA_CAPTURED_ONFIRST_CHANGED_ON_NEXT,
		USCI_B_SPI_CLOCKPOLARITY_INACTIVITY_LOW
	);

	if (STATUS_FAIL == returnValue) // TODO: flag
		return;

	//Enable SPI module
	USCI_B_SPI_enable(USCI_B1_BASE);

	//Enable Receive interrupt
	USCI_B_SPI_clearInterruptFlag(USCI_B1_BASE, USCI_B_SPI_RECEIVE_INTERRUPT);
	USCI_B_SPI_enableInterrupt(USCI_B1_BASE, USCI_B_SPI_RECEIVE_INTERRUPT);
	USCI_B_SPI_clearInterruptFlag(USCI_B1_BASE, USCI_B_SPI_TRANSMIT_INTERRUPT);
	USCI_B_SPI_enableInterrupt(USCI_B1_BASE, USCI_B_SPI_TRANSMIT_INTERRUPT);

	// init radio to recommended "defaults" per datasheet:
	write_single_register(0x18, 0x88);
	write_single_register(0x19, 0x55);
	write_single_register(0x1a, 0x8b);
	write_single_register(0x26, 0x07);
	write_single_register(0x29, 0xe0);
//	write_single_register(0x29, 0xd0);

	for (uint8_t sync_addr=0x2f; sync_addr<=0x36; sync_addr++) {
		write_single_register(sync_addr, 0x01);
	}

	write_single_register(0x3c, 0x8f);
	write_single_register(0x6f, 0x30);

	write_single_register(0x25, 0b00000000); // GPIO map to default

}

volatile uint8_t f_radio_xmitted = 0;

void cmd_register(uint8_t cmd, uint8_t *data, uint8_t len) {
	frame_bytes_remaining = len;
	rfm_reg_data_length = len;

	// Load up our outgoing buffer:
	rfm_write_len = len;
	rfm_write_index = 0;
	memcpy((void *) rfm_write_reg_data, (void *) data, rfm_write_len);
	rfm_writing = 1;

	// Remove if TX interrupts are enabled:
//	while (!USCI_B_SPI_getInterruptStatus(USCI_B1_BASE,
//		USCI_B_SPI_TRANSMIT_INTERRUPT)); // Make sure we can send
	f_radio_xmitted = 0;

	rfm_reg_data_ready = 0;

	// NSS low and deliver the command.
	GPIO_setOutputLowOnPin(RFM_NSS_PORT, RFM_NSS_PIN);
	USCI_B_SPI_transmitData(USCI_B1_BASE, cmd);

	while (!rfm_reg_data_ready);
	rfm_reg_data_ready = 0;

//
//	GPIO_setOutputLowOnPin(RFM_NSS_PORT, RFM_NSS_PIN); // Hold NSS low.
//	USCI_B_SPI_transmitData(USCI_B1_BASE, cmd); // Send our command.
//
//	// Now busy-wait while the ISR takes care of writing this command for us.
//	while (rfm_writing);
}

void write_register(uint8_t addr, uint8_t *data, uint8_t len) {
	/*
	 * You can use this like a blocking call.
	 */
	// MSB=1 is a write command:
	addr |= 0b10000000;
	cmd_register(addr, data, len);
}

void write_single_register(uint8_t addr, uint8_t data) {
	/*
	 * You can use this like a blocking call.
	 */
	// MSB=1 is a write command:
	uint8_t buf[1] = {data};
	write_register(addr, buf, 1);
}

void read_register(uint8_t addr, uint8_t len) {
	// MSB=0 is a read command:
	addr = 0b01111111 & addr;
	cmd_register(addr, rfm_zeroes, len);
}

uint8_t read_register_sync(uint8_t addr, uint8_t len, uint8_t *target) { // TODO: Refactor to be the same as write.
	read_register(addr, len);
	memcpy(target, rfm_read_reg_data, len);
	return len;
}

uint8_t read_single_register_sync(uint8_t addr) {
	read_register(addr, 1);
	return rfm_read_reg_data[0];
}

void mode_rx_sync() {
	write_single_register(RFM_OPMODE, 0b00010000); // Receive mode.
	uint8_t reg_read;
	do {
		reg_read = read_single_register_sync(RFM_IRQ1);
	}
	while (!(BIT7 & reg_read) || !(BIT6 & reg_read));
}

void mode_sb_sync() {
	uint8_t reg_read;
	write_single_register(RFM_OPMODE, 0b00000100);
	do {
		reg_read = read_single_register_sync(RFM_IRQ1);
	}
	while (!(BIT7 & reg_read));
}

void mode_tx_sync() {
	write_single_register(RFM_OPMODE, 0b00001100); // TX mode.
	uint8_t reg_read;
	do {
		reg_read = read_single_register_sync(RFM_IRQ1);
	}
	while (!(BIT7 & reg_read) || !(BIT5 & reg_read));
}

void mode_tx_async() {
	write_single_register(RFM_OPMODE, 0b00001100); // TX mode.
}

void radio_send(uint8_t *data, uint8_t len) {
	write_register(RFM_FIFO, data, len);
}

uint8_t rfm_crcok() {
	uint8_t status = 0;
	read_register(RFM_IRQ2, 1);
	status = rfm_read_reg_data[0] & BIT1;
	return status ? 1 : 0;
}

//volatile uint8_t rfm_reg_index = 0;
//volatile uint8_t rfm_reg_len = 0;
//volatile uint8_t rfm_begin = 0;
//volatile uint8_t rfm_rw_reading  = 0; // 0- read, 1- write
//volatile uint8_t rfm_rw_single = 0; // 0- single, 1- fifo
//volatile uint8_t rfm_single_msg = 0;
//volatile uint8_t rfm_fifo[64] = {0};

// rfm_reg_state:

#define RFM_REG_IDLE			0
#define RFM_REG_RX_SINGLE_CMD	1
#define RFM_REG_RX_SINGLE_DAT	2
#define RFM_REG_TX_SINGLE_CMD	3
#define RFM_REG_TX_SINGLE_DAT	4
#define RFM_REG_RX_FIFO_CMD			5
#define RFM_REG_RX_FIFO_DAT			6
#define RFM_REG_TX_FIFO_CMD			7
#define RFM_REG_TX_FIFO_DAT			8

volatile uint8_t rfm_reg_state = RFM_REG_IDLE;

// because we need to have RX and TX -IFGs process each state:
volatile uint8_t rfm_state_ifgs = 0;

#pragma vector=USCI_B1_VECTOR
__interrupt void USCI_B1_ISR(void)
{
	/*
	 * We either just sent or just received something.
	 * Here's how this goes down.
	 *
	 * (NB: all bets are off in the debugger: the order of RXIFG and TXIFG tend
	 *      to reverse when stepping through line by line. Doh.)
	 *
	 * First RXIFG is always ignored
	 * First TXIFG is always the command
	 *
	 * We can either be reading/writing a single register, in which case:
	 *
	 *    If READ:
	 *    	RXIFG: Second byte goes into rfm_single_msg
	 *    	TXIFG: Second byte is 0
	 *
	 * 	  If WRITE:
	 * 	  	RXIFG: Second byte is ignored
	 * 	  	TXIFG: Second byte is rfm_single_msg
	 *
	 * Or we can be reading/writing the FIFO, in which case:
	 *
	 *    If READ:
	 *    	Until index==len:
	 *    		RXIFG: put the read message into rfm_fifo
	 *    		TXIFG: send 0
	 *    If WRITE:
	 *    	Until index==len:
	 *    		RXIFG: ignore
	 *    		TXIFG: send the message from rfm_fifo
	 *
	 *
	 *
	 * You know - if we've received the last value, then TXIFG doesn't matter.
	 *  We know that the RFM has received whatever we sent, because it's sent
	 *  the last bit simultaneously with it. So RXIFG can take care of setting
	 *  IDLE. If state is idle and TXIFG fires, we just ignore it.
	 *
	 * Maybe RX does all the state change...?
	 */


	// TXIFG (just sent something):
	switch(rfm_reg_state) {
	case RFM_REG_IDLE:
		// WTF?
		break;
	case RFM_REG_RX_SINGLE_CMD:

		// Just finished sending the command. Now we need to send a 0 so the
		// clock keeps going and we can receive the data.
		// rfm_state_ifgs++; RFM_REG_RX_SINGLE_DAT next.
	case RFM_REG_RX_SINGLE_DAT:
		// Done. Don't do anything. RXIFG can handle it.
		break;
	case RFM_REG_TX_SINGLE_CMD:
		// Just finished sending the command. Now we need to send
		// rfm_single_msg.
		// rfm_state_ifgs++; RFM_REG_TX_SINGLE_DAT next.
		break;
	case RFM_REG_TX_SINGLE_DAT:
		// Just finished sending the value. We don't need to send anything else.
		// rfm_state_ifgs++; RFM_REG_IDLE next.
		break;
	case RFM_REG_RX_FIFO_CMD:
		// Just finished sending the FIFO read command.
		// Send 0. rfm_state_ifgs++; RFM_REG_RF_DAT next.
	case RFM_REG_RX_FIFO_DAT:
		// If there's more to send, send 0.
		// If not, rfm_state_ifgs++; RFM_REG_IDLE next.
		break;
	case RFM_REG_TX_FIFO_CMD:
		// If there's more to send, send rfm_fifo[index]. Inc index.
		// If no more to send, rfm_state_ifgs++; RFM_REG_DAT next.
		break;
	case RFM_REG_TX_FIFO_DAT:
		// If there's more to send, send the FIFO data. Inc index.
		// If not, rfm_state_ifgs++; RFM_REG_IDLE next.
		break;
	default:
		// This covers all the CMD cases.
		// I just send CMD. Time to send the next thing.
		// If we're
	}

	// RX:
	switch(rfm_reg_state) {
	case RFM_REG_IDLE:
		// WTF?
		break;
	case RFM_REG_RX_SINGLE_DAT:
		// We just got the value. We're finished.
		// It goes into rfm_single_msg. Set IDLE.
		break;
	case RFM_REG_TX_SINGLE_DAT:
		// We just got the old value. It's stale, because we're setting it.
		// Ignore it.
		// rfm_state_ifgs++; RFM_REG_IDLE is next.
		break;
	case RFM_REG_RX_FIFO_DAT:
		// Data byte, so put it into rfm_fifo.
		// If that was the last one, rfm_state_ifgs++; next: idle + flag received
		// inc index
		break;
	case RFM_REG_TX_FIFO_DAT:
		// data byte, but we're TXing. Ignore it.
		// check if receiving is done.
		// If so, rfm_state_ifgs++; RFM_REG_IDLE next.
		break;
	default:
		// This covers all the CMD cases.
		// We need to ignore the received garbage from when we were
		//  sending the command. And increment the state so we enter a
		//	DAT state.
		// rfm_state_ifgs++; state++ next.
	}





	switch (__even_in_range(UCB1IV, 4)) {
	//Vector 2 - RXIFG
	case 2:


		// TODO: This could break if we ask for a length of 0.
		// Don't ask for a length of 0.
		if (frame_bytes_remaining == rfm_reg_data_length) {
			USCI_B_SPI_receiveData(USCI_B1_BASE); // throw away the first byte
		} else { // Not the all-zero useless first byte:
			rfm_read_reg_data[rfm_reg_data_length - frame_bytes_remaining - 1] = \
					USCI_B_SPI_receiveData(USCI_B1_BASE);
		}
		if (!(frame_bytes_remaining--)) {
			rfm_reg_data_ready = 1;
		}
		break;
	case 4: // Vector 4 - TXIFG // Ready for another character...













		f_radio_xmitted = 1;
		if (rfm_writing = 1 && rfm_write_index < rfm_write_len) {
			USCI_B_SPI_transmitData(USCI_B1_BASE, rfm_write_reg_data[rfm_write_index]);
			rfm_write_index++;
		}
		break;
	default: break;
	}
	if (rfm_reg_data_ready) {
		rfm_writing = 0;
		GPIO_setOutputHighOnPin(RFM_NSS_PORT, RFM_NSS_PIN); // NSS high to end frame
	}

}
