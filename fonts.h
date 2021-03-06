/*
 * fonts.h
 *
 * Some code generated using The Dot Factory
 * 	<http://www.eran.io/the-dot-factory-an-lcd-font-and-image-generator/>
 *
 * (c) 2014 George Louthan and released under 3-clause BSD license;
 *  see license.md.
 */

#ifndef FONTS_H_
#define FONTS_H_

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
	const FONT_CHAR_INFO*	charInfo;		// pointer to array of char information
	const uint8_t*			data;			// pointer to generated array of character visual representation
} FONT_INFO;

/* Font data for D3 5pt */
extern uint8_t font_bits[];
extern FONT_INFO font_info;
extern FONT_CHAR_INFO d3_5ptDescriptors[];

#endif /* FONTS_H_ */
