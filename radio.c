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

void init_radio() {

	// SPI for radio //////////////////////////////////////////////////////////
	//
	// P4.1 ---- MOSI (TX) >>--|
	// P4.2 ---- MISO (RX) <<--| RFM69CW
	// P4.3 ----------- CLK ---|
	// P4.7 ----------- NSS ---|
	//                         |
	//                         |
	// P2.0 --------- DIO0 <<--| (TODO)
	//

	//P3.5,4,0 option select
	GPIO_setAsPeripheralModuleFunctionInputPin(
		GPIO_PORT_P4,
		GPIO_PIN2 // + GPIO_PIN7
	);

	GPIO_setAsPeripheralModuleFunctionOutputPin(
			GPIO_PORT_P4,
			GPIO_PIN1 + GPIO_PIN3
	);

	// TODO: Can probably do this in hardware instead?
	GPIO_setAsOutputPin(NSS_PORT, NSS_PIN);
	GPIO_setOutputHighOnPin(NSS_PORT, NSS_PIN); // NSS is active low.

	// SPI to RFM /////////////////////////////////////////////////////////////
	//
	// Initialize the SPI for talking to the radio
	//	USCI_B_SPI_disable(USCI_B1_BASE); // This wasn't in the example.

	//Initialize Master
	returnValue = USCI_B_SPI_masterInit(
		USCI_B1_BASE,
		USCI_B_SPI_CLOCKSOURCE_SMCLK, // selectClockSource
		UCS_getSMCLK(),
		SPICLK,
		USCI_B_SPI_MSB_FIRST,
		USCI_B_SPI_PHASE_DATA_CAPTURED_ONFIRST_CHANGED_ON_NEXT,
		USCI_B_SPI_CLOCKPOLARITY_INACTIVITY_LOW
	);

	if (STATUS_FAIL == returnValue)
		return;

	//Enable SPI module
	USCI_B_SPI_enable(USCI_B1_BASE);

	//Enable Receive interrupt
	USCI_B_SPI_clearInterruptFlag(USCI_B1_BASE, USCI_B_SPI_RECEIVE_INTERRUPT);
	USCI_B_SPI_enableInterrupt(USCI_B1_BASE, USCI_B_SPI_RECEIVE_INTERRUPT);
	//	USCI_B_SPI_clearInterruptFlag(USCI_B1_BASE, USCI_B_SPI_TRANSMIT_INTERRUPT);
	//	USCI_B_SPI_enableInterrupt(USCI_B1_BASE, USCI_B_SPI_TRANSMIT_INTERRUPT);

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

}

volatile uint8_t rfm_reg_data[64] = {0};
uint8_t rfm_zeroes[64] = {0};
uint8_t rfm_reg_data_index = 0;
uint8_t rfm_reg_data_length = 0;
volatile uint8_t rfm_reg_data_ready = 0;
uint8_t frame_bytes_remaining = 0;

void cmd_register(uint8_t cmd, uint8_t *data, uint8_t len) {
	frame_bytes_remaining = len;
	rfm_reg_data_length = len;

	while (!USCI_B_SPI_getInterruptStatus(USCI_B1_BASE,
		USCI_B_SPI_TRANSMIT_INTERRUPT)); // Make sure we can send
	rfm_reg_data_ready = 0;
	GPIO_setOutputLowOnPin(NSS_PORT, NSS_PIN); // Hold NSS low.
	USCI_B_SPI_transmitData(USCI_B1_BASE, cmd);
	for (int i=0; i<len; i++) {
		while (!USCI_B_SPI_getInterruptStatus(USCI_B1_BASE,
			USCI_B_SPI_TRANSMIT_INTERRUPT));
		USCI_B_SPI_transmitData(USCI_B1_BASE, data[i]);
	}
	while (!rfm_reg_data_ready);
	rfm_reg_data_ready = 0;
}

void write_register(uint8_t addr, uint8_t *data, uint8_t len) {
	/*
	 * You can use set_register like a blocking call.
	 */
	// MSB=1 is a write command:
	addr |= 0b10000000;
	cmd_register(addr, data, len);
}


void write_single_register(uint8_t addr, uint8_t data) {
	/*
	 * You can use set_register like a blocking call.
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
	memcpy(target, rfm_reg_data, len);
	return len;
}

uint8_t read_single_register_sync(uint8_t addr) {
	read_register(addr, 1);
	return rfm_reg_data[0];
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
	write_single_register(RFM_OPMODE, 0b00000100); // Receive mode.
	uint8_t reg_read;
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

uint8_t rfm_crcok() {
	uint8_t status = 0;
	read_register(RFM_IRQ2, 1);
	status = rfm_reg_data[0] & BIT1;
	return status ? 1 : 0;
}

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
			rfm_reg_data[rfm_reg_data_length - frame_bytes_remaining - 1] = \
					USCI_B_SPI_receiveData(USCI_B1_BASE);
		}
		if (!(frame_bytes_remaining--)) {
			GPIO_setOutputHighOnPin(NSS_PORT, NSS_PIN); // NSS high to end frame
			rfm_reg_data_ready = 1;
		}
		break;
	case 4: // Vector 4 - TXIFG // Ready for another character...
		break;
	default: break;
	}
}
