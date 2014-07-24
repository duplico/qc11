/*
 * fonts.h
 *
 *  Created on: Jun 9, 2014
 *      Author: George
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
extern const uint8_t font_bits[];
extern const FONT_INFO font_info;
extern const FONT_CHAR_INFO d3_5ptDescriptors[];

extern const uint8_t microsoftSansSerif_5ptBitmaps[];
extern const FONT_INFO microsoftSansSerif_5ptFontInfo;
extern const FONT_CHAR_INFO microsoftSansSerif_5ptDescriptors[];

#endif /* FONTS_H_ */
