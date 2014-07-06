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

volatile uint8_t ir_rx_frame[64] = {0};
volatile uint8_t ir_rx_index = 0;
volatile uint8_t ir_rx_len = 0;

uint8_t ir_reject_loopback = 0;

// Protocol: SYNC0, SYNC1, LEN, DATA, CRC_MSB, CRC_LSB, SYNC1, SYNC0
//  Max length: 56 bytes
uint8_t ir_tx_frame[64] = {SYNC0, SYNC1, 1, 0, 0, 0, SYNC1, SYNC0, 0};
volatile uint8_t ir_xmit = 0;
volatile uint8_t ir_xmit_index = 0;
volatile uint8_t ir_xmit_len = 0;


void init_ir() {
	// TX for IR
	GPIO_setAsPeripheralModuleFunctionOutputPin(
			IR_TXRX_PORT,
			IR_TX_PIN
	);
	// RX for IR
	GPIO_setAsPeripheralModuleFunctionInputPin(
			IR_TXRX_PORT,
			IR_RX_PIN
	);

	// Shutdown (SD) for IR
	GPIO_setAsOutputPin(IR_SD_PORT, IR_SD_PIN);
	GPIO_setOutputLowOnPin(IR_SD_PORT, IR_SD_PIN); // shutdown low = on

	// We'll use SMCLK, which is 8 MHz.
	// See: http://software-dl.ti.com/msp430/msp430_public_sw/mcu/msp430/MSP430BaudRateConverter/index.html

	USCI_A_UART_disable(IR_USCI_BASE);

#if BADGE_TARGET
	// 19200 baud: non-oversampled.
	USCI_A_UART_initAdvance(
			IR_USCI_BASE,
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
#else
	// Infrared:
	// 19200 baud: non-oversampled.
	USCI_A_UART_initAdvance(
			IR_USCI_BASE,
			USCI_A_UART_CLOCKSOURCE_SMCLK,
			625,
			0,
			0,
			USCI_A_UART_NO_PARITY,
			USCI_A_UART_MSB_FIRST,
			USCI_A_UART_ONE_STOP_BIT,
			USCI_A_UART_MODE,
			USCI_A_UART_LOW_FREQUENCY_BAUDRATE_GENERATION
	);
#endif
	USCI_A_UART_disable(IR_USCI_BASE);

	IRTCTL = UCIREN + UCIRTXPL2 + UCIRTXPL0;
	IRRCTL |= UCIRRXPL;

	USCI_A_UART_enable(IR_USCI_BASE);

	USCI_A_UART_clearInterruptFlag(IR_USCI_BASE, USCI_A_UART_RECEIVE_INTERRUPT_FLAG);
	USCI_A_UART_enableInterrupt(
			IR_USCI_BASE,
			USCI_A_UART_RECEIVE_INTERRUPT
	);

	USCI_A_UART_clearInterruptFlag(IR_USCI_BASE, USCI_A_UART_TRANSMIT_INTERRUPT_FLAG);
	USCI_A_UART_enableInterrupt(
			IR_USCI_BASE,
			USCI_A_UART_TRANSMIT_INTERRUPT
	);
}

uint8_t ir_check_crc() {
	uint16_t crc = 0;

	CRC_setSeed(CRC_BASE, 0xBEEF);

	for (uint8_t i=0; i<ir_rx_len; i++) {
		CRC_set8BitData(CRC_BASE, ir_rx_frame[i]);
	}

	crc = CRC_getResult(CRC_BASE);

	return ir_rx_frame[ir_rx_len] == (crc & 0xFF) &&
			ir_rx_frame[ir_rx_len+1] == ((crc & 0xFF00) >> 8);
}


// Single byte, encapsulated in our IR datagram:
void ir_write_single_byte(uint8_t payload) {
	ir_write(&payload, 1);
}

void ir_write(uint8_t* payload, uint8_t len) {
	if (len==0) {
		while (payload[len++]); // If len=0, it's a null-termed string
		// It's len++ instead of ++len because we DO want to send the
		// terminator character, so we can just print it on the other side
		// without any extra processing.
	}

	if (len>56) {
		len=56;
	}

	uint16_t crc = 0;

	// Packet header:
	ir_tx_frame[0] = SYNC0;
	ir_tx_frame[1] = SYNC1;
	ir_tx_frame[2] = len;

	// Packet payload & CRC:
	CRC_setSeed(CRC_BASE, 0xBEEF);
	for (uint8_t i=0; i<len; i++) {
		CRC_set8BitData(CRC_BASE, payload[i]);
		ir_tx_frame[3+i] = payload[i];
	}

	crc = CRC_getResult(CRC_BASE);

	ir_tx_frame[3+len] = (uint8_t) (crc & 0xFF);
	ir_tx_frame[4+len] = (uint8_t) ((crc & 0xFF00) >> 8);

	// Packet footer:
	ir_tx_frame[5 + len] = SYNC1;
	ir_tx_frame[6 + len] = SYNC0;

	// Start the transmission:
	ir_xmit = 1;
	ir_xmit_index = 0;
	ir_xmit_len = len;

	USCI_A_UART_transmitData(IR_USCI_BASE, ir_tx_frame[0]);

}


volatile uint8_t ir_rx_state = 0;

/*
 * 0 - base state, waiting for sync0
 * 1 = sync0 received, waiting for sync1
 * 2 = sync1 received, waiting for len
 * 3 = len received, listening to payload
 * 4 = len payload received, waiting for crc
 * 5 = len payload received, waiting for sync0
 * 6 = waiting for sync1
 * return to 0 on receive of sync0
 */

#pragma vector=IR_USCI_VECTOR
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
	switch(__even_in_range(IRIV,4))
	{
	case 0:	// 0: No interrupt.
		break;
	case 2:	// RXIFG: RX buffer ready to read.

		if (ir_reject_loopback && ir_xmit) {
			USCI_A_UART_clearInterruptFlag(IR_USCI_BASE, USCI_A_UART_RECEIVE_INTERRUPT_FLAG);
			break;
		}

		received_data = USCI_A_UART_receiveData(IR_USCI_BASE);

		switch (ir_rx_state) {
		case 0: // IDLE, this should be SYNC0
			if (received_data == SYNC0) {
				ir_rx_state++;
			}
			break;
		case 1: // Have SYNC0, this should be SYNC1
			if (received_data == SYNC1) {
				ir_rx_state++;
			} else {
				ir_rx_state = 0;
			}
			break;
		case 2: // SYNC, this should be len
			ir_rx_len = received_data;
			ir_rx_index = 0;
			ir_rx_state++;
			break;
		case 3: // LISTEN, this should be part of the payload
			ir_rx_frame[ir_rx_index] = received_data;
			ir_rx_index++;
			if (ir_rx_index == ir_rx_len) { // Received len bytes (ir frames):
				ir_rx_state++;
			}
			break;
		case 4: // Payload received, waiting for CRC
			// TODO: get the checksum, but don't actually verify it.
			//  We'll set the f_ir_rx_ready flag, and it will be the
			//  responsibility of the main thread to verify the checksum.
			ir_rx_frame[ir_rx_index] = received_data;
			ir_rx_index++; // the crc will be the last two bytes of the buffer
			if (ir_rx_index == ir_rx_len+2) { // crc is 2 bytes
				ir_rx_state++;
			}
			break;
			// fall through to next case:
		case 5: // CRC received and checked, waiting for SYNC1
			if (received_data == SYNC1)
				ir_rx_state++;
			else
				ir_rx_state = 0;
			break;
		case 6: // SYNC1 received, waiting for SYNC0
			if (received_data == SYNC0) {
				ir_rx_state = 0;
				f_ir_rx_ready = 1; // Successful receive
			} else {
				ir_rx_state = 0;
			}
		} // switch (ir_rx_state)

		break;
	case 4:	// TXIFG: TX buffer is sent.
		ir_xmit_index++;
		if (ir_xmit_index >= ir_xmit_len+7) {
			ir_xmit = 0;
		}

		if (ir_xmit) {
			USCI_A_UART_transmitData(IR_USCI_BASE, ir_tx_frame[ir_xmit_index]);
		}
		break;
	default: break;
	}
}
