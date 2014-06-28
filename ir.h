/*
 * ir.h
 *
 *  Created on: Jun 28, 2014
 *      Author: George
 */

#ifndef IR_H_
#define IR_H_

#include <stdint.h>

void init_serial();
void write_ir_byte(uint8_t);
void write_serial(uint8_t*);
uint8_t check_crc();

#endif /* IR_H_ */
