/*
 * ir.h
 *
 *  Created on: Jun 28, 2014
 *      Author: George
 */

#ifndef IR_H_
#define IR_H_

#include <stdint.h>

void init_ir();
void ir_write_single_byte(uint8_t);
void ir_write(uint8_t*, uint8_t, uint8_t);
void ir_process_rx_ready();
void ir_process_timestep();
void ir_pair_setstate(uint8_t);

uint8_t ir_check_crc();
inline uint8_t ir_paired();
extern uint8_t ir_pair_payload[];
extern uint8_t ir_partner;

extern char ir_rx_handle[11];
extern char ir_rx_message[17];

#define IR_PROTO_LISTEN 	0x0
#define IR_PROTO_HELLO_C 	0x11
#define IR_PROTO_ITP_C 		0x12
#define IR_PROTO_PAIRING_C 	0x13
#define IR_PROTO_PAIRED_C 	0x14
#define IR_PROTO_HELLO_S 	0x21
#define IR_PROTO_ITP_S 		0x22
#define IR_PROTO_PAIRING_S 	0x23
#define IR_PROTO_PAIRED_S 	0x24

#define IR_OP_BEACON 	 100
#define IR_OP_HELLO 	 101
#define IR_OP_HELLOACK 	 102
#define IR_OP_ITP		 103
#define IR_OP_PAIRREQ	 104
#define IR_OP_PAIRACC	 105
#define IR_OP_PAIRACK	 106
#define IR_OP_KEEPALIVE	 107
#define IR_OP_STILLALIVE 108

#endif /* IR_H_ */
