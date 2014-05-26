/*
 * radio.c
 *
 *  Created on: May 26, 2014
 *      Author: George
 */

#include <msp430f5308.h>
#include <stdint.h>
#include "driverlib.h"
#include "main.h"

#define SPICLK 1000000

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
	GPIO_setAsOutputPin(GPIO_PORT_P4, GPIO_PIN7);
	GPIO_setOutputHighOnPin(GPIO_PORT_P4, GPIO_PIN7); // NSS is active low.

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
}

uint8_t rfm_reg_data[64] = {0};
uint8_t rfm_reg_data_index = 0;
uint8_t rfm_reg_data_length = 0;
uint8_t rfm_reg_data_ready = 0;
uint8_t frame_bytes_remaining = 0;

void set_register(uint8_t addr, uint8_t data) {
	/*
	 * You can use set_register like a blocking call.
	 */
	// MSB=1 is a write command:
	addr = 0b10000000 | addr;

	while (!USCI_B_SPI_getInterruptStatus(USCI_B1_BASE,
		USCI_B_SPI_TRANSMIT_INTERRUPT)); // Make sure we can send
	GPIO_setOutputLowOnPin(GPIO_PORT_P4, GPIO_PIN7); // Hold NSS low.
	frame_bytes_remaining = 1;
	rfm_reg_data_length = 1;
	USCI_B_SPI_transmitData(USCI_B1_BASE, addr);
	while (!USCI_B_SPI_getInterruptStatus(USCI_B1_BASE,
		USCI_B_SPI_TRANSMIT_INTERRUPT));
	USCI_B_SPI_transmitData(USCI_B1_BASE, data);
}

void read_register(uint8_t addr, uint8_t len) {
	// MSB=0 is a read command:
	addr = 0b01111111 & addr;
	rfm_reg_data_length = len;
	frame_bytes_remaining = len;

	while (!USCI_B_SPI_getInterruptStatus(USCI_B1_BASE,
		USCI_B_SPI_TRANSMIT_INTERRUPT)); // Make sure we can send
	rfm_reg_data_ready = 0;
	GPIO_setOutputLowOnPin(GPIO_PORT_P4, GPIO_PIN7); // Hold NSS low.
	frame_bytes_remaining = 1;
	USCI_B_SPI_transmitData(USCI_B1_BASE, addr);
	while (!USCI_B_SPI_getInterruptStatus(USCI_B1_BASE,
		USCI_B_SPI_TRANSMIT_INTERRUPT));
	USCI_B_SPI_transmitData(USCI_B1_BASE, 0);
}

// TODO: Accept a buffer to memcpy into?
uint8_t read_register_sync(uint8_t addr, uint8_t len) {
	read_register(addr, len);
	while (!rfm_reg_data_ready);
	rfm_reg_data_ready = 0;
	return rfm_reg_data[0];
}

#pragma vector=USCI_B1_VECTOR
__interrupt void USCI_B1_ISR(void)
{
	switch (__even_in_range(UCB1IV, 4)) {
	//Vector 2 - RXIFG
	case 2:
		// TODO: This could break is we ask for a length of 0.
		// Don't ask for a length of 0.
		if (frame_bytes_remaining == rfm_reg_data_length) {
			USCI_B_SPI_receiveData(USCI_B1_BASE); // throw away the first byte
		} else { // Not the all-zero useless first byte:
			rfm_reg_data[rfm_reg_data_length - frame_bytes_remaining - 1] = \
					USCI_B_SPI_receiveData(USCI_B1_BASE);
		}
		if (!(frame_bytes_remaining--)) {
			GPIO_setOutputHighOnPin(GPIO_PORT_P4, GPIO_PIN7); // NSS high to end frame
			rfm_reg_data_ready = 1;
		}
		break;
	case 4: // Vector 4 - TXIFG // Ready for another character...
		break;
	default: break;
	}
}
