/*
 * ir.c
 *
 *  Created on: Jun 28, 2014
 *      Author: George
 */

#include "ir.h"
#include "leds.h"
#include "qcxi.h"

#include "driverlib.h"
#include <string.h>

volatile uint8_t received_data = 0;

volatile uint8_t ir_rx_frame[38] = {0};
volatile uint16_t ir_rx_crc = 0;
volatile uint8_t ir_rx_index = 0;
volatile uint8_t ir_rx_len = 0;
volatile uint8_t ir_rx_from = 0;

volatile uint8_t loops_to_ir_timestep = IR_LOOPS;
uint8_t ir_timesteps_to_beacon = IR_LOOPS_PER_BEACON;

uint8_t ir_reject_loopback = 0;

#define MAX_IR_LEN 31
// Protocol: SYNC0, SYNC1, FROM, TO, LEN, DATA, CRC_MSB, CRC_LSB, SYNC2, SYNC3
// CRC16 of:              |_____|        |.....|
//  Max length: 31 bytes
uint8_t ir_tx_frame[MAX_IR_LEN + 9] = {SYNC0, SYNC1, 0, 0xFF, 1, 0, 0, 0, SYNC2, SYNC3, 0};
volatile uint8_t ir_xmit = 0;
volatile uint8_t ir_xmit_index = 0;
volatile uint8_t ir_xmit_len = 0;

uint8_t ir_pair_payload[30] = {IR_OP_PAIRACC, 0, 0};

char ir_rx_handle[11] = "";
char ir_rx_message[17] = "";

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
}

// Single byte, encapsulated in our IR datagram:
void ir_write_single_byte(uint8_t payload) {
	ir_write(&payload, 0xff, 1);
}

void ir_proto_setup(uint8_t to_addr, uint8_t opcode, uint8_t seqnum) {
	uint16_t crc = 0;
	uint8_t len = 2;

	if (opcode == IR_OP_PAIRACC || opcode == IR_OP_PAIRACK) {
		// this is the one where we send our message...
		len = 30;
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
		memcpy(&(ir_tx_frame[7]), ir_pair_payload, 30);
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
	return (ir_proto_state & 0b1111) == 4;
}

void ir_pair_setstate(uint8_t state) {
	ir_proto_tto = IR_PROTO_TTO;
	ir_proto_state = state;
}

#define IR_PAIR_SETSTATE(STATE) ir_pair_setstate(STATE);
#define IR_ASSERT_PARTNER if (ir_partner != ir_rx_from) IR_PAIR_SETSTATE(IR_PROTO_LISTEN)

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
	case IR_PROTO_PAIRED_C: // TODO: Anything special here? // fall through
	case IR_PROTO_PAIRED_S: // TODO: Anything special here? // fall through
	default:
		if (ir_proto_tto--) {
			// re-send, don't time out
			if (ir_proto_state >= IR_PROTO_HELLO_C && ir_proto_state <= IR_PROTO_PAIRED_C) {
				ir_write_global();
			}
		} else {
			// time out
			if (ir_paired()) {
				f_unpaired = 1;
			}
			IR_PAIR_SETSTATE(IR_PROTO_LISTEN);
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

	switch(ir_proto_state) {
	case IR_PROTO_LISTEN:
		if (opcode == IR_OP_BEACON) {
			// We'll be the client.
			ir_partner = ir_rx_from;
			IR_PAIR_SETSTATE(IR_PROTO_HELLO_C);
			ir_proto_setup(ir_partner, IR_OP_HELLO, 0);
			ir_write_global();
		} else if (opcode == IR_OP_HELLO) {
			// We'll be the server.
			ir_partner = ir_rx_from;
			ir_proto_seqnum = 0;
			IR_PAIR_SETSTATE(IR_PROTO_HELLO_S);
			ir_proto_setup(ir_partner, IR_OP_HELLOACK, 0);
			ir_write_global();
		} else {
			// do nothing; we don't care about this message.
		}
		break;
	case IR_PROTO_HELLO_C:
		IR_ASSERT_PARTNER
		if (opcode == IR_OP_HELLOACK) { // as expected
			IR_PAIR_SETSTATE(IR_PROTO_ITP_C);
			ir_proto_seqnum = 0;
			ir_proto_setup(ir_partner, IR_OP_ITP, 0);
			ir_write_global();
		} else {
			IR_PAIR_SETSTATE(IR_PROTO_LISTEN);
		}
		break;
	case IR_PROTO_ITP_C:
		IR_ASSERT_PARTNER
		// For the SERVER: ir_proto_seqnum is what we are SENDING.
		if (opcode == IR_OP_PAIRREQ && ir_proto_seqnum == ITPS_TO_PAIR) {
			IR_PAIR_SETSTATE(IR_PROTO_PAIRING_C);
			ir_proto_setup(ir_partner, IR_OP_PAIRACC, 0);
			ir_write_global();
			f_ir_itp_step = 1;
		} else if (opcode == IR_OP_ITP && seqnum == ir_proto_seqnum+1) {
			ir_proto_seqnum += 2;
			if (ir_proto_seqnum > ITPS_TO_SHOW_PAIRING)
				f_ir_itp_step = 1;
			// Specifically DON'T send anything at this point.
			ir_proto_setup(ir_partner, IR_OP_ITP, ir_proto_seqnum);
			IR_PAIR_SETSTATE(IR_PROTO_ITP_C);
		} else {
			IR_PAIR_SETSTATE(IR_PROTO_LISTEN);
			f_ir_pair_abort = 1;
		}
		break;
	case IR_PROTO_PAIRING_C:
		IR_ASSERT_PARTNER
		if (opcode == IR_OP_PAIRACK) {
			// decide we're paired.

			strcpy(ir_rx_handle, (char *) &(ir_rx_frame[2]));
			strcpy(ir_rx_message, (char *) &(ir_rx_frame[2+11]));

			// See if this is a new pair.
			if (!paired_badge(ir_partner)) {
				f_paired_new_person = 1;
				if (!have_trick(ir_partner % TRICK_COUNT)) {
					// new trick
					f_paired_new_trick = (ir_partner % TRICK_COUNT) + 1;
				}
			}
			f_paired = 1;
			IR_PAIR_SETSTATE(IR_PROTO_PAIRED_C);
			ir_proto_setup(ir_partner, IR_OP_KEEPALIVE, 0);
		} else {
			IR_PAIR_SETSTATE(IR_PROTO_LISTEN);
			f_ir_pair_abort = 1;
		}
		break;
	case IR_PROTO_PAIRED_C:
		if (ir_rx_from != ir_partner) {
			break; // just ignore messages from others now that we're paired
		}
		if (opcode == IR_OP_STILLALIVE) {
			IR_PAIR_SETSTATE(IR_PROTO_PAIRED_C);
		} else { // fault of some kind:
			IR_PAIR_SETSTATE(IR_PROTO_LISTEN);
			f_unpaired = 1;
		}
		break;
	case IR_PROTO_HELLO_S: // can receive either HELLO (resend) or ITP
		IR_ASSERT_PARTNER
		if (opcode == IR_OP_HELLO) {
			// resend HELLOACK
			ir_proto_setup(ir_partner, IR_OP_HELLOACK, 0);
			ir_write_global();
		} else if (opcode == IR_OP_ITP && seqnum==0) {
			IR_PAIR_SETSTATE(IR_PROTO_ITP_S);
			// send ITP(1)
			ir_proto_setup(ir_partner, IR_OP_ITP, 1);
			ir_proto_seqnum = 2; // we're expecting 2 as the response.
			ir_write_global();
		} else {
			IR_PAIR_SETSTATE(IR_PROTO_LISTEN);
		}
		break;
	case IR_PROTO_ITP_S: // can receive either ITP(##) or ITP(16)
		IR_ASSERT_PARTNER
		// For the SERVER: ir_proto_seqnum is what we are EXPECTING.
		if (opcode == IR_OP_ITP && seqnum == ir_proto_seqnum && seqnum == ITPS_TO_PAIR) {
			// TODO: send PAIRREQ
			// send our message, etc
			IR_PAIR_SETSTATE(IR_PROTO_PAIRING_S);
			ir_proto_setup(ir_partner, IR_OP_PAIRREQ, 0);
			f_ir_itp_step = 1;
			ir_write_global();
		} else if (opcode == IR_OP_ITP) {
			if (ir_proto_seqnum > ITPS_TO_SHOW_PAIRING)
				f_ir_itp_step = 1;
			ir_proto_setup(ir_partner, IR_OP_ITP, seqnum+1);
			ir_write_global();
			if (seqnum == ir_proto_seqnum) {
				// not resend:
				ir_proto_seqnum+=2;
				IR_PAIR_SETSTATE(IR_PROTO_ITP_S);
			} else {
				// resend: no resetting of anything.
			}
		} else {
			f_ir_pair_abort = 1;
			IR_PAIR_SETSTATE(IR_PROTO_LISTEN)
		}
		break;
	case IR_PROTO_PAIRING_S: // can receive either ITP(16) (resend) or PAIRACC
		IR_ASSERT_PARTNER
		if (opcode == IR_OP_PAIRACC) {
			IR_PAIR_SETSTATE(IR_PROTO_PAIRED_S);
			f_paired = 1;
			strcpy(ir_rx_handle, &(ir_rx_frame[2]));
			strcpy(ir_rx_message, &(ir_rx_frame[2+11]));

			if (my_conf.met_ids[ir_partner/16] & (1 << ir_partner % 16)) {
				// new person
				f_paired_new_person = 1;
				// TODO: see if it's a new trick.
			}
			ir_proto_setup(ir_partner, IR_OP_PAIRACK, 0);
			ir_write_global();
		} else if (opcode == IR_OP_ITP && seqnum == ITPS_TO_PAIR) {
			// resend:
			// TODO: send PAIRREQ. Send message, etc; check if this is a new
			// person.
			ir_proto_setup(ir_partner, IR_OP_PAIRREQ, 0); // TODO: probably already done.
			ir_write_global();
		} else {
			f_ir_pair_abort = 1;
			IR_PAIR_SETSTATE(IR_PROTO_LISTEN);
		}
		break;
	case IR_PROTO_PAIRED_S: // can receive either PAIRACC (resend) or KEEPALIVE
		if (ir_rx_from != ir_partner) {
			break; // just ignore messages from others now that we're paired
		}
		if (opcode == IR_OP_PAIRACC) {
			// resend:
			ir_proto_setup(ir_partner, IR_OP_PAIRACK, 0);
			ir_write_global();
		} else if (opcode == IR_OP_KEEPALIVE) {
			IR_PAIR_SETSTATE(IR_PROTO_PAIRED_S);
			ir_proto_setup(ir_partner, IR_OP_STILLALIVE, 0);
			ir_write_global();
		} else {
			IR_PAIR_SETSTATE(IR_PROTO_LISTEN);
			f_unpaired = 1;
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
