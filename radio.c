/*
 * radio.c
 *
 *  Created on: May 26, 2014
 *      Author: George
 */

#include "radio.h"
#include "qcxi.h"
#include "leds.h" // TODO

#include <stdint.h>
#include "driverlib.h"
#include <string.h>

#define SPICLK 8000000

uint8_t returnValue = 0;

// The register-reading machine:
volatile uint8_t rfm_reg_tx_index = 0;
volatile uint8_t rfm_reg_rx_index = 0;
volatile uint8_t rfm_begin = 0;
volatile uint8_t rfm_rw_reading  = 0; // 0- read, 1- write
volatile uint8_t rfm_rw_single = 0; // 0- single, 1- fifo
volatile uint8_t rfm_single_msg = 0;

volatile uint8_t rfm_reg_ifgs = 0;
volatile uint8_t rfm_reg_state = RFM_REG_IDLE;

// The protocol machine:
volatile uint8_t rfm_proto_state = 0;

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
//	GPIO_setAsInputPin(GPIO_PORT_P2, GPIO_PIN0);
	P2DIR &= ~BIT0;
	P2SEL &= ~BIT0;

	//P3.5,4,0 option select
//	GPIO_setAsPeripheralModuleFunctionInputPin(
//		GPIO_PORT_P4,
//		GPIO_PIN2
//	);
	P4SEL |= BIT2;

//	GPIO_setAsPeripheralModuleFunctionOutputPin(
//			GPIO_PORT_P4,
//			GPIO_PIN1 + GPIO_PIN3
//	);
	P4SEL |= GPIO_PIN1 + GPIO_PIN3;

//	GPIO_setAsOutputPin(RFM_NSS_PORT, RFM_NSS_PIN);
	RFM_NSS_PORT_DIR |= RFM_NSS_PIN;

//	GPIO_setOutputHighOnPin(RFM_NSS_PORT, RFM_NSS_PIN); // NSS is active low.
	RFM_NSS_PORT_OUT |= RFM_NSS_PIN;

	// SPI to RFM /////////////////////////////////////////////////////////////
	//
	// Initialize the SPI for talking to the radio

	returnValue = USCI_B_SPI_masterInit(
		USCI_B1_BASE,
		USCI_B_SPI_CLOCKSOURCE_SMCLK, // selectClockSource
#if BADGE_TARGET
		8000000,
#else
		UCS_getSMCLK(),
#endif
		SPICLK,
		USCI_B_SPI_MSB_FIRST,
		USCI_B_SPI_PHASE_DATA_CAPTURED_ONFIRST_CHANGED_ON_NEXT,
		USCI_B_SPI_CLOCKPOLARITY_INACTIVITY_LOW
	);

	if (STATUS_FAIL == returnValue) // TODO: flag
		return;

	//Enable SPI module
//	USCI_B_SPI_enable(USCI_B1_BASE);
	UCB1CTL1 &= ~UCSWRST;

	// Radio reboot procedure:
	//  hold RESET high for > 100 us
	//  pull RESET low, wait 5 ms
	//  module is ready

//	GPIO_setOutputHighOnPin(GPIO_PORT_P6, GPIO_PIN0);
	P6OUT |= BIT0;
	delay(1);
//	GPIO_setOutputLowOnPin(GPIO_PORT_P6, GPIO_PIN0);
	P6OUT &= ~BIT0;
	delay(10);

	//Enable Receive interrupt
	USCI_B_SPI_clearInterruptFlag(USCI_B1_BASE, USCI_B_SPI_RECEIVE_INTERRUPT);
	USCI_B_SPI_enableInterrupt(USCI_B1_BASE, USCI_B_SPI_RECEIVE_INTERRUPT);
	USCI_B_SPI_clearInterruptFlag(USCI_B1_BASE, USCI_B_SPI_TRANSMIT_INTERRUPT);
	USCI_B_SPI_enableInterrupt(USCI_B1_BASE, USCI_B_SPI_TRANSMIT_INTERRUPT);

	rfm_reg_state = RFM_REG_IDLE;
	mode_sb_sync();

	// init radio to recommended "defaults" (seriously, wtf are they
	//  calling them defaults for if they're not set BY DEFAULT?????
	//  Sheesh.), per datasheet:
	write_single_register(0x18, 0b00001000); // Low-noise amplifier
	write_single_register(0x19, 0b01010101); // Bandwidth control "default"
//	write_single_register(0x1a, 0b10001011); // Auto Frequency Correction BW
	write_single_register(0x1a, 0x8b); // Auto Frequency Correction BW "default"
	write_single_register(0x26, 0x07); // Disable ClkOut
	write_single_register(0x29, 210); // RSSI Threshold

	// Other configuration:

	write_single_register(0x3c, sizeof(qcxipayload));

	/// Output configuration:
	write_single_register(0x11, 0b10011100); // Output power
//	write_single_register(0x12, 0b00001111); // PA0 ramp time

	write_single_register(0x25, 0b00000000); // GPIO map to default

	// Setup addresses and length:
	write_single_register(0x37, 0b00110100); // Packet configuration (see DS)
	write_single_register(0x38, sizeof(qcxipayload)); // PayloadLength
	write_single_register(0x39, my_conf.badge_id); // NodeAddress
	write_single_register(0x3A, RFM_BROADCAST); // BroadcastAddress

	write_single_register(0x3c, 0x8f); // TxStartCondition - FifoNotEmpty

	// Bandwidth settings we're currently mucking around with:
	// Bandwidth (see pg 26, 3.4.6):
	// BW > 1/2 BR
	// Our BW needs to be >= 1/2 of bitrate
//	write_single_register(0x19, 0b01001011);
	// Beta = 2 * FDEV / BR
	// Set Fdev, so 2xFDEV/BR \in [0.5,10]
//	write_single_register(0x06, 0x52); // FDEV = 61*this value.
	// Bitrate:
//	write_single_register(0x03, 0x06);
//	write_single_register(0x04, 0x83);
	// Auto frequency correction settings:
//	write_single_register(0x0b, 0b00100000); // Special AFC for low-beta
//	write_single_register(0x71, 1); // Low-beta AFC offset / 488 Hz
//	write_single_register(0x6f, 0x20); // Fading margin improvement // 0x20 for low beta, 0x30 for high beta
//	write_single_register(0x1a, 0b10000101); // AFC bandwidth
//	write_single_register(0x1e, 0b00001100); // Restart every time we hit RX mode
	// Preamble LSB:
//	write_single_register(0x2d, 0x10); // 16 preamble bytes

	for (uint8_t sync_addr=0x2f; sync_addr<=0x36; sync_addr++) {
		write_single_register(sync_addr, 0x01);
	}

	// Now that we're done with this setup business, we can enable the
	// DIO interrupts. We have to wait until now because otherwise if
	// the is radio activity during setup it will enter our protocol
	// state machine way too early, which can cause the system to hang
	// indefinitely.

	// Auto packet mode: RX->SB->RX on receive.
	mode_rx_sync();
	write_single_register(0x3b, RFM_AUTOMODE_RX);
	volatile uint8_t ret = 0;

	GPIO_enableInterrupt(GPIO_PORT_P2, GPIO_PIN0);
	GPIO_interruptEdgeSelect(GPIO_PORT_P2, GPIO_PIN0, GPIO_LOW_TO_HIGH_TRANSITION);
	GPIO_clearInterruptFlag(GPIO_PORT_P2, GPIO_PIN0);
	ret = read_single_register_sync(0x01);

}

void write_single_register_async(uint8_t addr, uint8_t data) {
	if (rfm_reg_state != RFM_REG_IDLE)
		return; // TODO: flag a fault?
	rfm_reg_state = RFM_REG_TX_SINGLE_CMD;
	rfm_single_msg = data;
	addr = addr | 0b10000000; // MSB=1 => write command
//	GPIO_setOutputLowOnPin(RFM_NSS_PORT, RFM_NSS_PIN); // Hold NSS low to begin frame.
	RFM_NSS_PORT_OUT &= ~RFM_NSS_PIN;
	USCI_B_SPI_transmitData(USCI_B1_BASE, addr); // Send our command.
}

void write_single_register(uint8_t addr, uint8_t data) {
	/*
	 * This blocks.
	 */
	while (rfm_reg_state != RFM_REG_IDLE); // Block until ready to write.
	write_single_register_async(addr, data);
	while (rfm_reg_state != RFM_REG_IDLE); // Block until written.
}

void read_single_register_async(uint8_t addr) {
	if (rfm_reg_state != RFM_REG_IDLE)
		return; // TODO: flag a fault?
	rfm_reg_state = RFM_REG_RX_SINGLE_CMD;
	addr = 0b01111111 & addr; // MSB=0 => write command
//	GPIO_setOutputLowOnPin(RFM_NSS_PORT, RFM_NSS_PIN); // Hold NSS low to begin frame.
	RFM_NSS_PORT_OUT &= ~RFM_NSS_PIN;
	USCI_B_SPI_transmitData(USCI_B1_BASE, addr); // Send our command.
}

uint8_t read_single_register_sync(uint8_t addr) {
	while (rfm_reg_state != RFM_REG_IDLE); // Block until ready to read.
	read_single_register_async(addr);
	while (rfm_reg_state != RFM_REG_IDLE); // Block until read finished.
	return rfm_single_msg;
}

void mode_rx_async() {
	write_single_register_async(RFM_OPMODE, 0b00010000);
}

void mode_rx_sync() {
	while (rfm_reg_state != RFM_REG_IDLE);
	mode_rx_async(); // Receive mode.
	while (rfm_reg_state != RFM_REG_IDLE);
	uint8_t reg_read;
	do {
		reg_read = read_single_register_sync(RFM_IRQ1);
	}
	while (!(BIT7 & reg_read));
}

void mode_sb_async() {
	write_single_register_async(RFM_OPMODE, 0b00000100);
}

void mode_sb_sync() {
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

void mode_tx_sync() {
	while (rfm_reg_state != RFM_REG_IDLE);
	mode_tx_async(); // TX mode.
	while (rfm_reg_state != RFM_REG_IDLE);
	uint8_t reg_read;
	do {
		reg_read = read_single_register_sync(RFM_IRQ1);
	}
	while (!(BIT7 & reg_read));
}

uint8_t expected_dio_interrupt = 0;

void radio_send_sync() {
	// Wait for, e.g., completion of receiving something.
	while (rfm_reg_state != RFM_REG_IDLE);
	mode_sb_sync(); // Enter standby mode.
	// Intermediate mode is TX
	// Enter condition is FIFO level
	// Exit condition is PacketSent.
	// During sending, let's set the end mode to RX
	write_single_register(0x3b, RFM_AUTOMODE_TX);

	expected_dio_interrupt = 1; // will be xmit finished.

	rfm_reg_state = RFM_REG_TX_FIFO_CMD;
//	GPIO_setOutputLowOnPin(RFM_NSS_PORT, RFM_NSS_PIN); // Hold NSS low to begin frame.
	RFM_NSS_PORT_OUT &= ~RFM_NSS_PIN;
	USCI_B_SPI_transmitData(USCI_B1_BASE, RFM_FIFO | 0b10000000); // Send write command.
	while (rfm_reg_state != RFM_REG_IDLE);
	mode_rx_async(); // Set the mode so we'll re-enter RX mode once xmit is done.
}

 void radio_recv_start() {
	if (rfm_reg_state != RFM_REG_IDLE) {
		led_print_scroll("??!!", 1);
		return; // TODO: flag a fault?
	}
	rfm_reg_state = RFM_REG_RX_FIFO_CMD;
//	GPIO_setOutputLowOnPin(RFM_NSS_PORT, RFM_NSS_PIN); // Hold NSS low to begin frame.
	RFM_NSS_PORT_OUT &= ~RFM_NSS_PIN;
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
			((uint8_t *) &in_payload)[rfm_reg_rx_index] = USCI_B_SPI_receiveData(USCI_B1_BASE);
			rfm_reg_rx_index++;
			if (rfm_reg_rx_index == sizeof(qcxipayload)) {
				// That was the last one we were expecting.
				rfm_reg_ifgs++; // RX thread is ready to go IDLE.
			}
			break;
		case RFM_REG_TX_FIFO_DAT:
			// Got a data byte from the FIFO, but we're writing so it's stale garbage.
			USCI_B_SPI_receiveData(USCI_B1_BASE); // Throw it away.
			rfm_reg_rx_index++;
			if (rfm_reg_rx_index == sizeof(qcxipayload)) {
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
			if (rfm_reg_tx_index == sizeof(qcxipayload)) {
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
			if (rfm_reg_tx_index == sizeof(qcxipayload)) {
				// We just finished sending the last one.
				rfm_reg_ifgs++; // TX thread is ready to go IDLE.
			} else {
				// We have more to send.
				USCI_B_SPI_transmitData(USCI_B1_BASE, ((uint8_t *) &out_payload)[rfm_reg_tx_index]);
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
		case RFM_REG_RX_SINGLE_DAT:
			rfm_reg_state = RFM_REG_IDLE;
			break;
		case RFM_REG_TX_SINGLE_DAT:
			rfm_reg_state = RFM_REG_IDLE;
			break;
		case RFM_REG_RX_FIFO_DAT:
			rfm_reg_state = RFM_REG_IDLE;
			f_rfm_rx_done = 1;
			break;
		case RFM_REG_TX_FIFO_DAT:
			// After we send the FIFO, we need to set the mode to RX so the
			// thing will automagically return to the RX mode once we're done.
			rfm_reg_state = RFM_REG_TX_FIFO_AM;
			break;
		default: // Covers all the CMD cases:
			rfm_reg_state++;
			break;
		}

	} // end of state machine (transitions)

	if (rfm_reg_state == RFM_REG_IDLE) {
//		GPIO_setOutputHighOnPin(RFM_NSS_PORT, RFM_NSS_PIN); // NSS high to end frame
		RFM_NSS_PORT_OUT |= RFM_NSS_PIN;
	} else if (rfm_reg_state == RFM_REG_TX_FIFO_AM) { // Automode:
//		GPIO_setOutputHighOnPin(RFM_NSS_PORT, RFM_NSS_PIN); // NSS high to end frame
		RFM_NSS_PORT_OUT |= RFM_NSS_PIN;
		rfm_reg_state = RFM_REG_IDLE;
		mode_rx_async();
	}
}

/*
 * ISR for DIO0 from the RFM module. It's asserted when a job (TX or RX) is finished.
 */
#pragma vector=PORT2_VECTOR
__interrupt void radio_interrupt_0(void) {
	if (expected_dio_interrupt) { // tx finished.
		// Auto packet mode: RX->SB->RX on receive.
		f_rfm_tx_done = 1;
		expected_dio_interrupt = 0;
		__bic_SR_register_on_exit(LPM3_bits);
	} else { // rx
		radio_recv_start();
	}
	GPIO_clearInterruptFlag(GPIO_PORT_P2, GPIO_PIN0); // TODO?
}
