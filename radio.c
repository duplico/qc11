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

// The register-reading machine:
volatile uint8_t rfm_reg_tx_index = 0;
volatile uint8_t rfm_reg_rx_index = 0;
volatile uint8_t rfm_reg_len = 64;
volatile uint8_t rfm_begin = 0;
volatile uint8_t rfm_rw_reading  = 0; // 0- read, 1- write
volatile uint8_t rfm_rw_single = 0; // 0- single, 1- fifo
volatile uint8_t rfm_single_msg = 0;
volatile uint8_t rfm_fifo[64] = {0};

#define RFM_REG_IDLE			0
#define RFM_REG_RX_SINGLE_CMD	1
#define RFM_REG_RX_SINGLE_DAT	2
#define RFM_REG_TX_SINGLE_CMD	3
#define RFM_REG_TX_SINGLE_DAT	4
#define RFM_REG_RX_FIFO_CMD		5
#define RFM_REG_RX_FIFO_DAT		6
#define RFM_REG_TX_FIFO_CMD		7
#define RFM_REG_TX_FIFO_DAT		8

volatile uint8_t rfm_reg_ifgs = 0;
volatile uint8_t rfm_reg_state = RFM_REG_IDLE;

// The protocol machine:
volatile uint8_t rfm_proto_state = 0;

#define RFM_PROTO_RX_IDLE 0
#define RFM_PROTO_RX_FIFO 1
#define RFM_PROTO_SB_UNSET_CMD 2
#define RFM_PROTO_SB_UNSET_DAT 3
#define RFM_PROTO_SB_FIFO 4
#define RFM_PROTO_TX 5
#define RFM_PROTO_RX_UNSET_CMD 6
#define RFM_PROTO_RX_UNSET_DAT 7

void init_radio() {

	// SPI for radio //////////////////////////////////////////////////////////
	//
	// P4.1 ---- MOSI (TX) >>--|
	// P4.2 ---- MISO (RX) <<--| RFM69CW
	// P4.3 ----------- CLK ---|
	// P4.7 ----------- NSS ---|
	//                         |
	// P6.0 --------- RESET->>-|
	// P2.0 --------- DIO0 <<--|
	//

	// DIO0 (interrupt pin):
	GPIO_setAsInputPin(GPIO_PORT_P2, GPIO_PIN0);

	GPIO_enableInterrupt(GPIO_PORT_P2, GPIO_PIN0);
	GPIO_interruptEdgeSelect(GPIO_PORT_P2, GPIO_PIN0, GPIO_LOW_TO_HIGH_TRANSITION);
	GPIO_clearInterruptFlag(GPIO_PORT_P2, GPIO_PIN0);

	// RESET:
	GPIO_setAsOutputPin(GPIO_PORT_P6, GPIO_PIN0);

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

	// Radio reboot procedure:
	//  hold RESET high for > 100 us
	//  pull RESET low, wait 5 ms
	//  module is ready

	GPIO_setOutputHighOnPin(GPIO_PORT_P6, GPIO_PIN0);
	delay(1);
	GPIO_setOutputLowOnPin(GPIO_PORT_P6, GPIO_PIN0);
	delay(10);

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

inline void write_single_register_async(uint8_t addr, uint8_t data) {
	if (rfm_reg_state != RFM_REG_IDLE)
		return; // TODO: flag a fault?
	rfm_reg_state = RFM_REG_TX_SINGLE_CMD;
	rfm_single_msg = data;
	addr = addr | 0b10000000; // MSB=1 => write command
	GPIO_setOutputLowOnPin(RFM_NSS_PORT, RFM_NSS_PIN); // Hold NSS low to begin frame.
	USCI_B_SPI_transmitData(USCI_B1_BASE, addr); // Send our command.
}

inline void write_single_register(uint8_t addr, uint8_t data) {
	/*
	 * This blocks.
	 */
	while (rfm_reg_state != RFM_REG_IDLE); // Block until ready to write.
	write_single_register_async(addr, data);
	while (rfm_reg_state != RFM_REG_IDLE); // Block until written.
}

inline void read_single_register_async(uint8_t addr) {
	if (rfm_reg_state != RFM_REG_IDLE)
			return; // TODO: flag a fault?
	rfm_reg_state = RFM_REG_RX_SINGLE_CMD;
	addr = 0b01111111 & addr; // MSB=0 => write command
	GPIO_setOutputLowOnPin(RFM_NSS_PORT, RFM_NSS_PIN); // Hold NSS low to begin frame.
	USCI_B_SPI_transmitData(USCI_B1_BASE, addr); // Send our command.
}

inline uint8_t read_single_register_sync(uint8_t addr) {
	while (rfm_reg_state != RFM_REG_IDLE); // Block until ready to read.
	read_single_register_async(addr);
	while (rfm_reg_state != RFM_REG_IDLE); // Block until read finished.
	return rfm_single_msg;
}

inline void mode_rx_async() {
	write_single_register_async(RFM_OPMODE, 0b00010000);
}

inline void mode_rx_sync() {
	while (rfm_reg_state != RFM_REG_IDLE);
	mode_rx_async(); // Receive mode.
	while (rfm_reg_state != RFM_REG_IDLE);
	uint8_t reg_read;
	do {
		reg_read = read_single_register_sync(RFM_IRQ1);
	}
	while (!(BIT7 & reg_read) || !(BIT6 & reg_read));
}

inline void mode_sb_async() {
	write_single_register_async(RFM_OPMODE, 0b00000100);
}

inline void mode_sb_sync() {
	while (rfm_reg_state != RFM_REG_IDLE);
	mode_sb_async();
	while (rfm_reg_state != RFM_REG_IDLE);
	uint8_t reg_read;
	do {
		reg_read = read_single_register_sync(RFM_IRQ1);
	}
	while (!(BIT7 & reg_read));
}

void mode_tx_async() {
	write_single_register_async(RFM_OPMODE, 0b00001100); // TX mode.
}

inline void mode_tx_sync() {
	while (rfm_reg_state != RFM_REG_IDLE);
	mode_tx_async(); // TX mode.
	while (rfm_reg_state != RFM_REG_IDLE);
	uint8_t reg_read;
	do {
		reg_read = read_single_register_sync(RFM_IRQ1);
	}
	while (!(BIT7 & reg_read) || !(BIT5 & reg_read));
}

inline void radio_send_dispatch(uint8_t len) {
	if (rfm_reg_state != RFM_REG_IDLE) return; // TODO
	rfm_reg_state = RFM_REG_TX_FIFO_CMD;
	GPIO_setOutputLowOnPin(RFM_NSS_PORT, RFM_NSS_PIN); // Hold NSS low to begin frame.
	USCI_B_SPI_transmitData(USCI_B1_BASE, RFM_FIFO | 0b10000000); // Send write command.
}

// TODO: This currently blocks.
void radio_send(uint8_t *data, uint8_t len) {
	memcpy((void *)rfm_fifo, (void *)data, len);
	while (rfm_reg_state != RFM_REG_IDLE || rfm_proto_state != RFM_PROTO_RX_IDLE); // Block until ready to write.
//	radio_send_dispatch(len);
	rfm_proto_state = RFM_PROTO_SB_UNSET_CMD;
	mode_sb_async();
	while (rfm_reg_state != RFM_REG_IDLE || rfm_proto_state != RFM_PROTO_RX_IDLE); // Block until written.
}

inline void radio_recv_start() {
	if (rfm_reg_state != RFM_REG_IDLE)
			return; // TODO: flag a fault?
	rfm_reg_state = RFM_REG_RX_FIFO_CMD;
	GPIO_setOutputLowOnPin(RFM_NSS_PORT, RFM_NSS_PIN); // Hold NSS low to begin frame.
	USCI_B_SPI_transmitData(USCI_B1_BASE, RFM_FIFO); // Send our read command.
}

/*
 * ISR for the SPI interface to the radio.
 *
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
 */
#pragma vector=USCI_B1_VECTOR
__interrupt void USCI_B1_ISR(void)
{
	switch (__even_in_range(UCB1IV, 4)) {
	//Vector 2 - RXIFG
	case 2:
		switch(rfm_reg_state) {
		case RFM_REG_IDLE:
			// WTF?
			break;
		case RFM_REG_RX_SINGLE_DAT:
			// We just got the value. We're finished.
			rfm_single_msg = USCI_B_SPI_receiveData(USCI_B1_BASE);
			rfm_reg_ifgs++; // RX thread is ready to go IDLE.
			break;
		case RFM_REG_TX_SINGLE_DAT:
			// We just got the old value. It's stale, because we're setting it.
			USCI_B_SPI_receiveData(USCI_B1_BASE); // Throw it away.
			rfm_reg_ifgs++; // RX thread is ready to go IDLE.
			break;
		case RFM_REG_RX_FIFO_DAT:
			// Got a data byte from the FIFO. Put it into its proper place.
			rfm_fifo[rfm_reg_rx_index] = USCI_B_SPI_receiveData(USCI_B1_BASE);
			rfm_reg_rx_index++;
			if (rfm_reg_rx_index == rfm_reg_len) {
				// That was the last one we were expecting.
				rfm_reg_ifgs++; // RX thread is ready to go IDLE.
			}
			break;
		case RFM_REG_TX_FIFO_DAT:
			// Got a data byte from the FIFO, but we're writing so it's stale garbage.
			USCI_B_SPI_receiveData(USCI_B1_BASE); // Throw it away.
			rfm_reg_rx_index++;
			if (rfm_reg_rx_index == rfm_reg_len) {
				// That was the last one we were expecting.
				rfm_reg_ifgs++; // RX thread is ready to go IDLE.
			}
			break;
		default:
			// This covers all the CMD cases.
			// We received some garbage sent to us while we were sending the command.
			USCI_B_SPI_receiveData(USCI_B1_BASE); // Throw it away.
			rfm_reg_ifgs++; // RX thread is ready to go to the DAT state.
			rfm_reg_rx_index = 0;
		} // end of state machine (RX thread)
		break; // End of RXIFG ///////////////////////////////////////////////////////

	case 4: // Vector 4 - TXIFG : I just sent a byte.
		switch(rfm_reg_state) {
		case RFM_REG_IDLE:
			// WTF?
			break;
		case RFM_REG_RX_SINGLE_CMD:
			// Just finished sending the command. Now we need to send a 0 so the
			// clock keeps going and we can receive the data.
			USCI_B_SPI_transmitData(USCI_B1_BASE, 0);
			rfm_reg_ifgs++; // TX thread is ready to go to RFM_REG_RX_SINGLE_DAT.
			break;
		case RFM_REG_RX_SINGLE_DAT:
			// Done.
			rfm_reg_ifgs++; // TX thread is ready to go IDLE.
			break;
		case RFM_REG_TX_SINGLE_CMD:
			// Just finished sending the command. Now we need to send
			// rfm_single_msg.
			USCI_B_SPI_transmitData(USCI_B1_BASE, rfm_single_msg);
			rfm_reg_ifgs++; // TX thread is ready to go to RFM_REG_TX_SINGLE_DAT
			break;
		case RFM_REG_TX_SINGLE_DAT:
			// Just finished sending the value. We don't need to send anything else.
			rfm_reg_ifgs++; // TX thread is ready to go IDLE.
			break;
		case RFM_REG_RX_FIFO_CMD:
			// Just finished sending the FIFO read command.
			rfm_reg_tx_index = 0;
			rfm_reg_ifgs++; // TX thread is ready to go to RFM_REG_RX_FIFO_DAT.
			// Fall through and send the first data byte's corresponsing 0 as below:
		case RFM_REG_RX_FIFO_DAT:
			// We just finished sending the blank message of index rfm_reg_tx_index-1.
			if (rfm_reg_tx_index == rfm_reg_len) {
				// We just finished sending the last one.
				rfm_reg_ifgs++; // TX thread is ready to go IDLE.
			} else {
				// We have more to send.
				USCI_B_SPI_transmitData(USCI_B1_BASE, 0);
				rfm_reg_tx_index++;
			}
			break;
		case RFM_REG_TX_FIFO_CMD:
			// Just finished sending the FIFO write command.
			rfm_reg_tx_index = 0;
			rfm_reg_ifgs++; // TX thread is ready to go to RFM_REG_TX_FIFO_DAT.
			// Fall through and send the first data byte as below:
		case RFM_REG_TX_FIFO_DAT:
			// We just finished sending the message of index rfm_reg_tx_index-1.
			if (rfm_reg_tx_index == rfm_reg_len) {
				// We just finished sending the last one.
				rfm_reg_ifgs++; // TX thread is ready to go IDLE.
			} else {
				// We have more to send.
				USCI_B_SPI_transmitData(USCI_B1_BASE, rfm_fifo[rfm_reg_tx_index]);
				rfm_reg_tx_index++;
			}
			break;
		default: break;
			// WTF?
		} // end of state machine (TX thread)
		break; // End of TXIFG /////////////////////////////////////////////////////

	default: break;
	} // End of ISR flag switch ////////////////////////////////////////////////////

	// If it's time to switch states:
	if (rfm_reg_ifgs == 2) {
		rfm_reg_ifgs = 0;
		switch(rfm_reg_state) {
		case RFM_REG_IDLE:
			// WTF?
			break;
		case RFM_REG_RX_SINGLE_CMD:
			rfm_reg_state = RFM_REG_RX_SINGLE_DAT;
			break;
		case RFM_REG_RX_SINGLE_DAT:
			rfm_reg_state = RFM_REG_IDLE;
			break;
		case RFM_REG_TX_SINGLE_CMD:
			rfm_reg_state = RFM_REG_TX_SINGLE_DAT;
			break;
		case RFM_REG_TX_SINGLE_DAT:
			rfm_reg_state = RFM_REG_IDLE;
			break;
		case RFM_REG_RX_FIFO_CMD:
			rfm_reg_state = RFM_REG_RX_FIFO_DAT;
			break;
		case RFM_REG_RX_FIFO_DAT:
			rfm_reg_state = RFM_REG_IDLE;
			break;
		case RFM_REG_TX_FIFO_CMD:
			rfm_reg_state = RFM_REG_TX_FIFO_DAT;
			break;
		case RFM_REG_TX_FIFO_DAT:
			rfm_reg_state = RFM_REG_IDLE;
			break;
		default:
			// WTF?
			break;
		}

	} // end of state machine (transitions)

	if (rfm_reg_state == RFM_REG_IDLE) {
		GPIO_setOutputHighOnPin(RFM_NSS_PORT, RFM_NSS_PIN); // NSS high to end frame

		// It also might be time to transition our state in the protocol machine:
		switch(rfm_proto_state) {
		case RFM_PROTO_RX_FIFO:
			rfm_proto_state = RFM_PROTO_RX_IDLE;
			f_rfm_rx_done = 1; // Just received something.
			__bic_SR_register(LPM3_bits);
			break;
		case RFM_PROTO_SB_UNSET_CMD:
			rfm_proto_state = RFM_PROTO_SB_UNSET_DAT;
			// Start reading the register that says whether we're
			read_single_register_async(RFM_IRQ1);
			break;
		case RFM_PROTO_SB_UNSET_DAT:
			if (rfm_single_msg & BIT7) {
				// mode change went well
				rfm_proto_state = RFM_PROTO_SB_FIFO;
				// time to start filling the FIFO:
				radio_send_dispatch(64);
			} else {
				read_single_register_async(RFM_IRQ1);
			}
			break;
		case RFM_PROTO_SB_FIFO:
			rfm_proto_state = RFM_PROTO_TX;
			mode_tx_async();
			break;
		case RFM_PROTO_RX_UNSET_CMD:
			rfm_proto_state = RFM_PROTO_RX_UNSET_DAT;
			read_single_register_async(RFM_IRQ1);
			break;
		case RFM_PROTO_RX_UNSET_DAT:
			if (rfm_single_msg & (BIT7 + BIT5)) {
				rfm_proto_state = RFM_PROTO_RX_IDLE;
			} else {
				read_single_register_async(RFM_IRQ1);
			}
			break;
		}
	}
}

/*
 * ISR for DIO0 from the RFM module. It's asserted when a job (TX or RX) is finished.
 */
#pragma vector=PORT2_VECTOR
__interrupt void radio_interrupt_0(void)
{
	switch (rfm_proto_state) {
	case RFM_PROTO_RX_IDLE:
		// We just received something from the radio. Let's read it.
		rfm_proto_state = RFM_PROTO_RX_FIFO;
		radio_recv_start();
		break;
	case RFM_PROTO_TX:
		// We just finished sending something.
		rfm_proto_state = RFM_PROTO_RX_UNSET_CMD;
		mode_rx_async();
		// TODO: enter mode.
		break;
	default:
		// WTF?
		break;
	}
	GPIO_clearInterruptFlag(GPIO_PORT_P2, GPIO_PIN0);
}
