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
void ir_process_one_second();
uint8_t ir_check_crc();
#define IR_PROTO_LISTEN 	0
#define IR_PROTO_HELLO_C 	1
#define IR_PROTO_ITP_C 		2
#define IR_PROTO_PAIRING_C 	3
#define IR_PROTO_PAIRED_C 	4
#define IR_PROTO_HELLO_S 	5
#define IR_PROTO_ITP_S 		6
#define IR_PROTO_PAIRING_S 	7
#define IR_PROTO_PAIRED_S 	8

#endif /* IR_H_ */
