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

#define MAX_IR_LEN 31

volatile uint8_t ir_rx_frame[MAX_IR_LEN+7] = {0};
volatile uint16_t ir_rx_crc = 0;
volatile uint8_t ir_rx_index = 0;
volatile uint8_t ir_rx_len = 0;
volatile uint8_t ir_rx_from = 0;

uint8_t ir_pair_role = 0;

volatile uint8_t loops_to_ir_timestep = IR_LOOPS;
uint8_t ir_timesteps_to_beacon = IR_LOOPS_PER_BEACON;

uint8_t ir_reject_loopback = 0;

// Protocol: SYNC0, SYNC1, FROM, TO, LEN, DATA, CRC_MSB, CRC_LSB, SYNC2, SYNC3
// CRC16 of:              |_____|        |.....|
//  Max length: 31 bytes (including opcode and seqnum)
uint8_t ir_tx_frame[MAX_IR_LEN + 9] = {SYNC0, SYNC1, 0, 0xFF, 1, 0, 0, 0, SYNC2, SYNC3, 0};
volatile uint8_t ir_xmit = 0;
volatile uint8_t ir_xmit_index = 0;
volatile uint8_t ir_xmit_len = 0;

uint8_t ir_pair_payload[30] = {IR_OP_PAIRACC, 0, 0};

char ir_rx_handle[11] = "";
char ir_rx_message[17] = "";

void init_ir() {
	// TX for IR
//	GPIO_setAsPeripheralModuleFunctionOutputPin(
//			IR_TXRX_PORT,
//			IR_TX_PIN
//	);
//
//	// RX for IR
//	GPIO_setAsPeripheralModuleFunctionInputPin(
//			IR_TXRX_PORT,
//			IR_RX_PIN
//	);

	IR_TXRX_PORT_SEL |= IR_TX_PIN + IR_RX_PIN;
	IR_TXRX_PORT_DIR &= ~IR_RX_PIN;

	// Shutdown (SD) for IR
//	GPIO_setAsOutputPin(IR_SD_PORT, IR_SD_PIN);// already output
//	GPIO_setOutputLowOnPin(IR_SD_PORT, IR_SD_PIN); // shutdown low = on
#if BADGE_TARGET
	IR_SD_PORT_OUT &= ~IR_SD_PIN;
#else
	GPIO_setAsOutputPin(IR_SD_PORT, IR_SD_PIN);// already output
	GPIO_setOutputLowOnPin(IR_SD_PORT, IR_SD_PIN); // shutdown low = on
#endif

	// We'll use SMCLK, which is 8 MHz.
	// See: http://software-dl.ti.com/msp430/msp430_public_sw/mcu/msp430/MSP430BaudRateConverter/index.html

//	USCI_A_UART_disable(IR_USCI_BASE);
	IR_USCI_CTL |= UCSWRST;

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
	IR_USCI_CTL |= UCSWRST;

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
	CRC_set8BitData(CRC_BASE, ir_rx_from);
	CRC_set8BitData(CRC_BASE, ir_rx_len);

	for (uint8_t i=0; i<ir_rx_len; i++) {
		CRC_set8BitData(CRC_BASE, ir_rx_frame[i]);
	}

	crc = CRC_getResult(CRC_BASE);

	return ir_rx_crc == crc;
}

void ir_setup_global(uint8_t* payload, uint8_t to_addr, uint8_t len) {
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
	ir_tx_frame[2] = my_conf.badge_id;
	ir_tx_frame[3] = to_addr;
	ir_tx_frame[4] = len;

	// Packet payload & CRC:
	CRC_setSeed(CRC_BASE, 0xBEEF);
	CRC_set8BitData(CRC_BASE, my_conf.badge_id);
	CRC_set8BitData(CRC_BASE, len);
	for (uint8_t i=0; i<len; i++) {
		CRC_set8BitData(CRC_BASE, payload[i]);
		ir_tx_frame[5+i] = payload[i];
	}

	crc = CRC_getResult(CRC_BASE);

	ir_tx_frame[5+len] = (uint8_t) (crc & 0xFF);
	ir_tx_frame[6+len] = (uint8_t) ((crc & 0xFF00) >> 8);

	// Packet footer:
	ir_tx_frame[7 + len] = SYNC2;
	ir_tx_frame[8 + len] = SYNC3;

	ir_xmit_len = len;
}

void ir_write_global() {
	// Start the transmission:
	ir_xmit = 1;
	ir_xmit_index = 0;
	USCI_A_UART_transmitData(IR_USCI_BASE, ir_tx_frame[0]);
	if (ir_proto_state == IR_PROTO_PAIRED && ir_proto_seqnum) {
		ir_proto_seqnum = 0;
	}
}

// Single byte, encapsulated in our IR datagram:
void ir_write_single_byte(uint8_t payload) {
	ir_write(&payload, 0xff, 1);
}

void ir_proto_setup(uint8_t to_addr, uint8_t opcode, uint8_t seqnum) {
	uint16_t crc = 0;
	uint8_t len = 2;

	if (opcode == IR_OP_ITP) {
		// this is the one where we send our message...
		len = 30;
		memcpy(&(ir_tx_frame[7]), ir_pair_payload, 30);
	} else if (opcode == IR_OP_KEEPALIVE || opcode == IR_OP_STILLALIVE) {
		memcpy(&(ir_tx_frame[7]), my_conf.paired_ids, 28); // paired_ids and scores
		ir_tx_frame[35] = my_conf.events_attended;
		len = 31;
	}

	// Packet header:
	ir_tx_frame[0] = SYNC0;
	ir_tx_frame[1] = SYNC1;
	ir_tx_frame[2] = my_conf.badge_id;
	ir_tx_frame[3] = to_addr;
	ir_tx_frame[4] = len;
	ir_tx_frame[5] = opcode;
	ir_tx_frame[6] = seqnum;

	if (len>2) {
	}

	CRC_setSeed(CRC_BASE, 0xBEEF);
	CRC_set8BitData(CRC_BASE, my_conf.badge_id);
	CRC_set8BitData(CRC_BASE, len);
	for (uint8_t i=0; i<len; i++) {
		CRC_set8BitData(CRC_BASE, ir_tx_frame[5+i]);
	}

	crc = CRC_getResult(CRC_BASE);

	ir_tx_frame[5+len] = (uint8_t) (crc & 0xFF);
	ir_tx_frame[6+len] = (uint8_t) ((crc & 0xFF00) >> 8);

	// Packet footer:
	ir_tx_frame[7 + len] = SYNC2;
	ir_tx_frame[8 + len] = SYNC3;

	ir_xmit_len = len;
}

void ir_write(uint8_t* payload, uint8_t to_addr, uint8_t len) {
	ir_setup_global(payload, to_addr, len);
	ir_write_global();
}

uint8_t ir_proto_state = IR_PROTO_LISTEN;
uint8_t ir_proto_tto = IR_PROTO_TTO; // tries to timeout
uint8_t ir_partner = 0;


// index 0 : OPCODE
// index 1 : ir_seqnum

uint8_t ir_proto_seqnum = 0;

inline uint8_t ir_paired() {
//	return (ir_proto_state & 0b1111) == 4;
	return ir_proto_state == IR_PROTO_PAIRED;
}

void ir_pair_setstate(uint8_t state) {
	ir_proto_tto = IR_PROTO_TTO;
	if (state == IR_PROTO_LISTEN && ir_proto_state == IR_PROTO_ITP) {
		f_ir_pair_abort = 1;
		ir_proto_seqnum = 0;
	}
	ir_proto_state = state;
}

#define IR_ASSERT_PARTNER if (ir_partner != ir_rx_from) { ir_pair_setstate(IR_PROTO_LISTEN); break; }

void ir_process_timestep() {
	if (ir_xmit)
		return;
	switch (ir_proto_state) {
	case IR_PROTO_LISTEN:
		if (!ir_timesteps_to_beacon) {
			ir_timesteps_to_beacon = IR_LOOPS_PER_BEACON;
			ir_proto_setup(0xff, IR_OP_BEACON, 0);
			ir_write_global();
		} else {
			ir_timesteps_to_beacon--;
		}
		break;
	case IR_PROTO_PAIRED:
		if (ir_pair_role == IR_ROLE_C) {
			ir_proto_setup(ir_partner, IR_OP_KEEPALIVE, ir_proto_seqnum);
		}
	default:
		if (ir_proto_tto--) {
			// re-send, don't time out
			if (ir_pair_role == IR_ROLE_C) {
				ir_write_global();
			}
		} else {
			// time out
			if (ir_paired()) {
				f_unpaired = (ir_partner != 0xff);
			}
			ir_pair_setstate(IR_PROTO_LISTEN);
		}
	}
}

void ir_process_rx_ready() {
	if (!ir_check_crc()) {
		return;
	}

	uint8_t opcode = ir_rx_frame[0];
	// Assert 100 <= ir_op <= 108
	if (opcode < 100 || opcode > 108) {
		return;
	}
	uint8_t seqnum = ir_rx_frame[1];

	switch(opcode) {
	case IR_OP_BEACON:
		if (ir_proto_state != IR_PROTO_LISTEN) {
			break;
		}
		ir_pair_role = IR_ROLE_C;
		ir_partner = ir_rx_from;
		ir_pair_setstate(IR_PROTO_ITP);
		ir_proto_seqnum = 0;
		// prep an ITP; fall through:
	case IR_OP_ITP:
		switch(ir_proto_state) {
		case IR_PROTO_LISTEN:
			ir_pair_role = IR_ROLE_S;
			ir_pair_setstate(IR_PROTO_ITP);
			ir_partner = ir_rx_from;
			// fall through:
		case IR_PROTO_ITP: // IR_ROLE_C falls through to here:
			IR_ASSERT_PARTNER
			ir_proto_seqnum = seqnum + !ir_pair_role; // +1 for client, +0 for server
			ir_proto_setup(ir_partner, IR_OP_ITP, ir_proto_seqnum);
			ir_pair_setstate(IR_PROTO_ITP);

			if (ir_proto_seqnum > ITPS_TO_SHOW_PAIRING) {
				f_ir_itp_step = 1;
			}

			if (ir_proto_seqnum == ITPS_TO_PAIR) {
				strcpy(ir_rx_handle, (char *) &(ir_rx_frame[2]));
				strcpy(ir_rx_message, (char *) &(ir_rx_frame[2+11]));
			}

			if (ir_pair_role == IR_ROLE_S) {
				ir_write_global();
			} else if (ir_proto_seqnum == ITPS_TO_PAIR) { // client is implicit here:
				// this means we can pair:
				ir_proto_setup(ir_partner, IR_OP_KEEPALIVE, 0);
				set_badge_paired(ir_partner);
			}
			break;
		case IR_PROTO_PAIRED:
			break; // ignore if paired.
		default:
			ir_pair_setstate(IR_PROTO_LISTEN);
		}
		break;

	case IR_OP_KEEPALIVE:
		if (ir_pair_role == IR_ROLE_S && ir_rx_from == ir_partner) {
			switch(ir_proto_state) {
			case IR_PROTO_ITP:
				set_badge_paired(ir_partner);
				// fall through:
			case IR_PROTO_PAIRED:
				if (seqnum) {
					f_paired_trick = seqnum;
				}
				ir_pair_setstate(IR_PROTO_PAIRED);
				ir_proto_setup(ir_partner, IR_OP_STILLALIVE, ir_proto_seqnum);
				ir_write_global();
				break;
			}
		}
		break;
	case IR_OP_STILLALIVE:
		if (ir_proto_state == IR_PROTO_PAIRED && ir_pair_role == IR_ROLE_C && ir_rx_from == ir_partner) {
			// Got a stillalive.
			if (seqnum) {
				f_paired_trick = seqnum;
			}
			ir_pair_setstate(IR_PROTO_PAIRED);
		}
		break;
	}
}

volatile uint8_t ir_rx_state = 0;

/*
 * 0 - base state, waiting for sync0
 * 1 = sync0 received, waiting for sync1
 * 2 = sync1 received, waiting for from
 * 3 = from received, waiting for to
 * 4 = to received, waiting for len
 * 5 = len received, listening to payload
 * 6 = len payload received, waiting for crc
 * 7 = len payload received, waiting for sync2
 * 8 = waiting for sync3
 * return to 0 on receive of sync3
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
		case 2: // SYNC, this should be from
			ir_rx_from = received_data;
			ir_rx_state++;
			break;
		case 3: // from, this should be to
			if (received_data == 0xFF || received_data == my_conf.badge_id) {
				// it's to me!
				ir_rx_state++;
			} else {
				// not to me. :-(
				ir_rx_state = 0;
			}
			break;
		case 4: // to, this should be len
			ir_rx_len = received_data;
			ir_rx_index = 0;
			ir_rx_state++;
			if (ir_rx_len > MAX_IR_LEN) {
				ir_rx_state = 0;
				ir_rx_len = 0;
			}
			break;
		case 5: // LISTEN, this should be part of the payload
			ir_rx_frame[ir_rx_index] = received_data;
			ir_rx_index++;
			if (ir_rx_index == ir_rx_len) { // Received len bytes (ir frames):
				ir_rx_state++;
			}
			break;
		case 6: // Payload received, waiting for CRC
			// Get the checksum, but don't actually verify it.
			//  We'll set the f_ir_rx_ready flag, and it will be the
			//  responsibility of the main thread to verify the checksum.
			if (ir_rx_index == ir_rx_len) {
				ir_rx_crc = received_data;
				ir_rx_index++;
			} else {
				ir_rx_crc |= ((uint16_t) received_data) << 8;
				ir_rx_state++;
			}
			break;
		case 7: // CRC received and checked, waiting for SYNC2
			if (received_data == SYNC2)
				ir_rx_state++;
			else
				ir_rx_state = 0;
			break;
		case 8: // SYNC2 received, waiting for SYNC3
			if (received_data == SYNC3) {
				ir_rx_state = 0;
				f_ir_rx_ready = 1; // Successful receive
				__bic_SR_register_on_exit(LPM3_bits);
			} else {
				ir_rx_state = 0;
			}
		} // switch (ir_rx_state)

		break;
	case 4:	// TXIFG: TX buffer is sent.
		ir_xmit_index++;
		if (ir_xmit_index >= ir_xmit_len+9) {
			ir_xmit = 0;
		}

		if (ir_xmit) {
			USCI_A_UART_transmitData(IR_USCI_BASE, ir_tx_frame[ir_xmit_index]);
		}
		break;
	default: break;
	}
}
