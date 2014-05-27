/*
 * main.h
 *
 *  Created on: May 26, 2014
 *      Author: George
 */

#ifndef MAIN_H_
#define MAIN_H_

extern char received_data_str[2];
extern void init_radio();
extern void write_serial(uint8_t*);
extern void set_register(uint8_t, uint8_t);
extern void read_register(uint8_t, uint8_t);
extern uint8_t read_register_sync(uint8_t, uint8_t, uint8_t*);

#endif /* MAIN_H_ */
