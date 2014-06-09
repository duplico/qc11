/*
 * main.h
 *
 *  Created on: May 26, 2014
 *      Author: George
 */

#ifndef MAIN_H_
#define MAIN_H_

#include <stdint.h>

#define BADGE_TARGET 1

#if BADGE_TARGET
#include <msp430f5308.h>
#define NSS_PORT GPIO_PORT_P4
#define NSS_PIN GPIO_PIN7
#else
#include <msp430f5529.h>
#define NSS_PORT GPIO_PORT_P3
#define NSS_PIN GPIO_PIN7
#endif


extern char received_data_str[2];
extern void init_radio();
extern void write_serial(uint8_t*);
extern void mode_rx_sync();
extern uint8_t rfm_crcok();


// This structure describes a single character's display information
typedef struct
{
	const uint8_t widthBits;					// width, in bits (or pixels), of the character
	const uint16_t offset;					// offset of the character's bitmap, in bytes, into the the FONT_INFO's data array

} FONT_CHAR_INFO;

// Describes a single font
typedef struct
{
	const uint8_t 			heightPages;	// height, in pages (8 pixels), of the font's characters
	const uint8_t 			startChar;		// the first character in the font (e.g. in charInfo and data)
	const uint8_t 			endChar;		// the last character in the font
	const uint8_t			spacePixels;	// number of pixels that a space character takes up
	const FONT_CHAR_INFO*	charInfo;		// pointer to array of char information
	const uint8_t*			data;			// pointer to generated array of character visual representation
} FONT_INFO;

#endif /* MAIN_H_ */
