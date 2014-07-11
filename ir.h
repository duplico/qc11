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
uint8_t ir_check_crc();
void ir_process_one_second();
void ir_write_global();

extern uint8_t ir_cycle;

#endif /* IR_H_ */
