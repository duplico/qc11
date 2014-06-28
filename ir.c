/*
 * ir.c
 *
 *  Created on: Jun 28, 2014
 *      Author: George
 */

#include "ir.h"
#include "qcxi.h"

#include "driverlib.h"
#include <string.h>

volatile uint8_t received_data = 0;

uint8_t frame_index = 0;
uint8_t ir_tx_frame[4] = {SYNC0, SYNC1, 0, 0};
uint8_t ir_rx_frame[4] = {0};

uint8_t ir_rx_index = 0;
uint8_t ir_rx_len = 4;

volatile uint8_t ir_xmit = 0;
volatile uint8_t ir_xmit_index = 0;
volatile uint8_t ir_xmit_len = 0;
volatile uint8_t ir_xmit_payload = 0;


void init_serial() {

	// We'll use SMCLK, which is 8 MHz.
	// See: http://software-dl.ti.com/msp430/msp430_public_sw/mcu/msp430/MSP430BaudRateConverter/index.html

	USCI_A_UART_disable(USCI_A1_BASE);

	// 19200 baud: non-oversampled.
	USCI_A_UART_initAdvance(
			USCI_A1_BASE,
			USCI_A_UART_CLOCKSOURCE_SMCLK,
			416,
			0,
			6,
			USCI_A_UART_NO_PARITY,
			USCI_A_UART_MSB_FIRST,
			USCI_A_UART_ONE_STOP_BIT,
			USCI_A_UART_MODE,
			USCI_A_UART_LOW_FREQUENCY_BAUDRATE_GENERATION
	);
	USCI_A_UART_disable(USCI_A1_BASE);

	UCA1IRTCTL = UCIREN + UCIRTXPL2 + UCIRTXPL0;
	UCA1IRRCTL |= UCIRRXPL;

	USCI_A_UART_enable(USCI_A1_BASE);

	USCI_A_UART_clearInterruptFlag(USCI_A1_BASE, USCI_A_UART_RECEIVE_INTERRUPT_FLAG);
	USCI_A_UART_enableInterrupt(
			USCI_A1_BASE,
			USCI_A_UART_RECEIVE_INTERRUPT
	);

	USCI_A_UART_clearInterruptFlag(USCI_A1_BASE, USCI_A_UART_TRANSMIT_INTERRUPT_FLAG);
	USCI_A_UART_enableInterrupt(
				USCI_A1_BASE,
				USCI_A_UART_TRANSMIT_INTERRUPT
		);
}

void write_ir_byte(uint8_t payload) {
//		while (!USCI_A_UART_getInterruptStatus(USCI_A1_BASE, UCTXIFG));
//		USCI_A_UART_transmitData(USCI_A1_BASE, SYNC2);
//		USCI_A_UART_transmitData(USCI_A1_BASE, SYNC1);
		USCI_A_UART_transmitData(USCI_A1_BASE, payload);
//		USCI_A_UART_transmitData(USCI_A1_BASE, 0);
//		while (!f_rx_ready);
//		f_rx_ready = 0;
}

void write_serial(uint8_t* text) {
	uint16_t sendchar = 0;
	do {
//		while (!USCI_A_UART_getInterruptStatus(USCI_A1_BASE, UCTXIFG));
		write_ir_byte(text[sendchar]);
//		while (!f_rx_ready);
//		f_rx_ready = 0;
	} while (text[++sendchar]);
}

#pragma vector=USCI_A1_VECTOR
__interrupt void ir_isr(void)
{
	/*
	 * NOTE: The RX interrupt has priority over TX interrupt. As a result,
	 * although normally after transmitting over IR we'll see the TX interrupt
	 * first, then the corresponding RX interrupt (because the transceiver
	 * echoes TX to RX), when stepping through in debug mode it will often
	 * be the case the the order is reversed: RXI, followed by the corresponding
	 * TX interrupt.
	 */
	switch(__even_in_range(UCA1IV,4))
	{
	case 0:	// 0: No interrupt.
		break;
	case 2:	// RXIFG: RX buffer ready to read.
//		if (f_rx_ready == 2) {
//			f_rx_ready = 0;
//			// don't clobber what we may have actually read, because we just sent something:
//			USCI_A_UART_clearInterruptFlag(USCI_A1_BASE, USCI_A_UART_RECEIVE_INTERRUPT_FLAG);
//		}
//		else {


		if (ir_xmit) {
			USCI_A_UART_clearInterruptFlag(USCI_A1_BASE, USCI_A_UART_RECEIVE_INTERRUPT_FLAG);
			break;
		}

		received_data = USCI_A_UART_receiveData(USCI_A1_BASE);

		if (ir_rx_index == 0 && received_data == SYNC0) {
			// do stuff
		} else if (ir_rx_index == 1 && received_data == SYNC1) {
			// do stuff
		}
		else if (ir_rx_index == 2) {
			// do stuff, payload
		} else if (ir_rx_index == 3 && received_data==0) {
			f_ir_rx_ready = 1;
			ir_rx_index = 0;
			// do stuff, successful receive
		} else {
			// malformed
			ir_rx_index = 0;
			break;
		}
		ir_rx_frame[ir_rx_index] = received_data;
		if (!f_ir_rx_ready)
			ir_rx_index++;
//		}

		break;
	case 4:	// TXIFG: TX buffer is sent.
		ir_xmit_index++;
		if (ir_xmit_index >= ir_xmit_len) {
			ir_xmit = 0;
		}

		if (ir_xmit) {
			USCI_A_UART_transmitData(USCI_A1_BASE, ir_tx_frame[ir_xmit_index]);
		}
		break;
	default: break;
	}
}
