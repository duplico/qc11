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

uint8_t rfm_zeroes[64] = {0};
uint8_t rfm_reg_data_index = 0;
uint8_t rfm_reg_data_length = 0;
uint8_t frame_bytes_remaining = 0;

void rfm_init() {

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

	GPIO_setAsOutputPin(NSS_PORT, NSS_PIN);
	GPIO_setOutputHighOnPin(NSS_PORT, NSS_PIN); // NSS is active low.

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

	//Enable Receive interrupt
	USCI_B_SPI_clearInterruptFlag(USCI_B1_BASE, USCI_B_SPI_RECEIVE_INTERRUPT);
	USCI_B_SPI_enableInterrupt(USCI_B1_BASE, USCI_B_SPI_RECEIVE_INTERRUPT);
	USCI_B_SPI_clearInterruptFlag(USCI_B1_BASE, USCI_B_SPI_TRANSMIT_INTERRUPT);
	USCI_B_SPI_enableInterrupt(USCI_B1_BASE, USCI_B_SPI_TRANSMIT_INTERRUPT);

	// init radio to recommended "defaults" per datasheet:
	rfm_write_single_register_sync(0x18, 0x88);
	rfm_write_single_register_sync(0x19, 0x55);
	rfm_write_single_register_sync(0x1a, 0x8b);
	rfm_write_single_register_sync(0x26, 0x07);
	rfm_write_single_register_sync(0x29, 0xe0);
//	write_single_register(0x29, 0xd0);

	for (uint8_t sync_addr=0x2f; sync_addr<=0x36; sync_addr++) {
		rfm_write_single_register_sync(sync_addr, 0x01);
	}

	rfm_write_single_register_sync(0x3c, 0x8f);
	rfm_write_single_register_sync(0x6f, 0x30);

	rfm_write_single_register_sync(0x25, 0b00000000); // GPIO map to default

}

///// Primitive communications with radio:

void rfm_cmd_register_async(uint8_t cmd, uint8_t *data, uint8_t len) {
	frame_bytes_remaining = len;
	rfm_reg_data_length = len;

	// Load up our outgoing buffer:
	rfm_write_len = len;
	rfm_write_index = 0;
	memcpy((void *) rfm_write_reg_data, (void *) data, rfm_write_len);
	rfm_writing = 1;

	f_rfm_reg_finished = 0;

	// NSS low and deliver the command.
	GPIO_setOutputLowOnPin(NSS_PORT, NSS_PIN);
	USCI_B_SPI_transmitData(USCI_B1_BASE, cmd);
}

void rfm_cmd_register_sync(uint8_t cmd, uint8_t *data, uint8_t len) {
	rfm_cmd_register_async(cmd, data, len);
	while (!f_rfm_reg_finished);
	f_rfm_reg_finished = 0;
}

void rfm_write_register_sync(uint8_t addr, uint8_t *data, uint8_t len) {
	/*
	 * You can use this like a blocking call.
	 */
	// MSB=1 is a write command:
	addr |= 0b10000000;
	rfm_cmd_register_sync(addr, data, len);
}

void rfm_write_single_register_sync(uint8_t addr, uint8_t data) {
	/*
	 * You can use set_register like a blocking call.
	 */
	// MSB=1 is a write command:
	uint8_t buf[1] = {data};
	rfm_write_register_sync(addr, buf, 1);
}

void rfm_read_register_sync(uint8_t addr, uint8_t len) {
	// MSB=0 is a read command:
	addr = 0b01111111 & addr;
	rfm_cmd_register_sync(addr, rfm_zeroes, len);
}

uint8_t rfm_read_single_register_sync(uint8_t addr) {
	rfm_read_register_sync(addr, 1);
	return rfm_read_reg_data[0];
}

uint8_t rfm_copy_register_sync(uint8_t addr, uint8_t len, uint8_t *target) { // TODO: Refactor to be the same as write.
	rfm_read_register_sync(addr, len);
	memcpy(target, rfm_read_reg_data, len);
	return len;
}

////// Radio modes:
volatile uint8_t rfm_mode = 0;

void rfm_mode_async(uint8_t mode) {
	// In some sense this is not so much asynchronous as it doesn't check to
	// make sure it took. It actually blocks while sending the two bytes to
	// the RFM module. TODO: let's see if we need to optimize this later.
	rfm_write_single_register_sync(RFM_OPMODE, mode);
	rfm_mode = mode;
}

void rfm_mode_sync(uint8_t mode) {
	rfm_mode_async(mode);
	uint8_t reg_read;
	do {
		reg_read = rfm_read_single_register_sync(RFM_IRQ1);
	}
	while (!(BIT7 & reg_read));
}

/////// Radio send/receive commands:

void rfm_send_sync(uint8_t * data, uint8_t len) {
	rfm_mode_sync(RFM_MODE_SB); // Enter standby to load up the FIFO
	rfm_write_single_register_sync(0x25, 0b00000000); // GPIO map to default
	rfm_write_register_sync(RFM_FIFO, data, len); // Load up the FIFO
	f_rfm_job_done = 0;
	rfm_mode_async(RFM_MODE_TX);
	while (!f_rfm_job_done); // Busywait until an interrupt that we've sent it. TODO
	f_rfm_job_done = 0;
	rfm_mode_sync(RFM_MODE_SB); // Back to standby.
}

volatile uint8_t rfm_mode_after_send = 0;
volatile uint8_t rfm_mode_after_reg = 0;

// TODO: state machine.

void rfm_send_async(uint8_t * data, uint8_t len, uint8_t mode_after_send) {
	rfm_mode_sync(RFM_MODE_SB); // Enter standby to load up the FIFO
	rfm_write_single_register_sync(0x25, 0b00000000); // GPIO map to default
	rfm_mode_after_reg = RFM_MODE_TX;
	rfm_write_register_sync(RFM_FIFO, data, len); // Load up the FIFO
	f_rfm_job_done = 0;
	rfm_mode_after_send = mode_after_send;
	rfm_mode_async(RFM_MODE_TX);
}

uint8_t rfm_crcok() {
	uint8_t status = 0;
	rfm_read_register_sync(RFM_IRQ2, 1);
	status = rfm_read_reg_data[0] & BIT1;
	return status ? 1 : 0;
}

uint8_t rfm_process_async() {
	if (rfm_mode == RFM_MODE_TX) {
		if (f_rfm_job_done && rfm_mode_after_send != 0) {
			rfm_mode_sync(rfm_mode_after_send);
			f_rfm_job_done = 0;
		}
	} else if (rfm_mode == RFM_MODE_RX) {

	}
}

volatile uint8_t temp_debug_flag = 0;

#pragma vector=USCI_B1_VECTOR
__interrupt void USCI_B1_ISR(void)
{
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
			f_rfm_reg_finished = 1;
		}
		break;
	case 4: // Vector 4 - TXIFG // Ready for another character...
		if (rfm_writing = 1 && rfm_write_index < rfm_write_len) {
			USCI_B_SPI_transmitData(USCI_B1_BASE, rfm_write_reg_data[rfm_write_index]);
			rfm_write_index++;
		}
		break;
	default: break;
	}
	if (f_rfm_reg_finished) {
		rfm_writing = 0;
		GPIO_setOutputHighOnPin(NSS_PORT, NSS_PIN); // NSS high to end frame
	}
}

#pragma vector=PORT2_VECTOR
__interrupt void radio_interrupt_0(void)
{
	// Called on CrcOK and on PacketSent.
	f_rfm_job_done = 1;

	if (rfm_mode == RFM_MODE_TX) {
		// Just finished sending a packet
	} else if (rfm_mode == RFM_MODE_RX) {
		// Just received a packet.
	}

	GPIO_clearInterruptFlag(GPIO_PORT_P2, GPIO_PIN0);
	__bic_SR_register_on_exit(LPM3_bits);
}
