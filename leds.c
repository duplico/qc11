/*
 * leds.c
 *
 *  Created on: May 26, 2014
 *      Author: George
 */

#include "qcxi.h"
#include "leds.h"
#include "fonts.h"

uint16_t values[5] = {65535, 65535, 65535, 65535, 65535};

uint16_t zeroes[5] = {0, 0, 0, 0, 0};

uint16_t disp_bit_buffer[BACK_BUFFER_WIDTH] = { 0 };

void print(char* text) {
	uint8_t character = 0;
	uint8_t cursor = 0;
	do {
		for (uint16_t i = d3_5ptFontInfo.charInfo[text[character] - d3_5ptFontInfo.startChar].offset; i < d3_5ptFontInfo.charInfo[text[character] - d3_5ptFontInfo.startChar].offset + d3_5ptFontInfo.charInfo[text[character] - d3_5ptFontInfo.startChar].widthBits; i++) {
			disp_bit_buffer[cursor++] = d3_5ptFontInfo.data[i];
			if (cursor == BACK_BUFFER_WIDTH)
				return;
		}
		disp_bit_buffer[cursor++] = 0; // gap between letters
		if (cursor == BACK_BUFFER_WIDTH)
			return;
		character++;

	} while (text[character]);

	while (cursor < BACK_BUFFER_WIDTH) { // empty everything else.
		disp_bit_buffer[cursor++] = 0;
	}
	led_disp_bit_to_values(0, 0);
	led_display_bits(values);
}

void led_set_rainbow(uint16_t value) {
	value &= 0b01111111111;
	// 0.15 = value.9,
	// 1.15 = value.8
	// 1.7 = value.7
	// 3.15 = value.6
	// 3.7 = value.5
	// 0.0 = value.4
	// 2.8 = value.3
	// 2.0 = value.2
	// 4.8 = value.1
	// 4.0 = value.0
	values[0] &= 0b0111111111111110;
	values[1] &= 0b0111111101111111;
	values[2] &= 0b1111111011111110;
	values[3] &= 0b0111111101111111;
	values[4] &= 0b1111111011111110;

	values[0] |= (value & BIT9)? BIT15 : 0;
	values[1] |= (value & BIT8)? BIT15 : 0;
	values[1] |= (value & BIT7)? BIT7 : 0;
	values[3] |= (value & BIT6)? BIT15 : 0;
	values[3] |= (value & BIT5)? BIT7 : 0;
	values[0] |= (value & BIT4)? BIT0 : 0;
	values[2] |= (value & BIT3)? BIT8 : 0;
	values[2] |= (value & BIT2)? BIT0 : 0;
	values[4] |= ((value & BIT1)? BIT8 : 0) | ((value & BIT0)? BIT0 : 0);
}

uint16_t rainbow_values;

void led_disp_bit_to_values(uint8_t left, uint8_t top) {

	// TODO: This will be handled in another routine later. But for
	//  now, we'll light the whole rainbow in this one:
	values[0] &= 0b1000000000000001;
	for (int i=1; i<5; i++) {
		if (i & 1) {
			values[i] &= 0b1000000010000000;
		} else {
			values[i] &= 0b0000000100000001;
		}
	}

	int x_offset = 0;

	uint8_t led_segment = 1;
	uint8_t led_index = 0;
	for (uint8_t x=0; x<14; x++) {
		for (uint8_t y=0; y<5; y++) {
			if (y == 0) {
				led_segment = 0;
				x_offset = 1;
			} else {
				if (x<7) {
					led_segment = 1; // LED segment is odd  (left side)
					// left side is x = [0 .. 6]
					// so x_offset is set to skip the leftmost LEDs only,
					// leading to x=0->out=1
					x_offset = 1;
				}
				else {
					led_segment = 2; // LED segment is even (right side)
					// right side is x = [7 .. 13], skipping the far right LED
					// so x_offset is set to make x=7->out=0
					x_offset = -7;
				}
				if (y>2) {
					led_segment+=2;
				}
				if (!(y % 2)) {
					// if Y is even (second row of a controller's segment)
					// then we need the offset to change. i.e. x=0->out=1->out'=9
					// or x=7->out=0->out'=8
					x_offset += 8;
				}
			}

			led_index = x_offset + x;

			// now we need to write to:
			// segment: 	led_segment
			// bit: 		x_offset + x
			if (disp_bit_buffer[(x + left) % BACK_BUFFER_WIDTH] & (1 << ((y + top) % BACK_BUFFER_HEIGHT)))
				values[led_segment] |= (1 << (15 - led_index));

		}
	}
}

void led_display_bits(uint16_t* val)
{
	//Set latch to low (should be already)
	GPIO_setOutputLowOnPin(LED_PORT, LED_LATCH);

	uint16_t i;
	uint8_t j;
	for (j=0; j<5; j++) {
		// Iterate over each bit, set data pin, and pulse the clock to send it
		// to the shift register
		for (i = 0; i < 16; i++)  {
			WRITE_IF(LED_PORT, LED_DATA, (val[4-j] & (1 << i)));
			GPIO_pulse(LED_PORT, LED_CLOCK);
		}
	}
	GPIO_pulse(LED_PORT, LED_LATCH);
}

void led_enable(uint16_t duty_cycle) {
//	led_disable(); // TODO
	GPIO_setAsPeripheralModuleFunctionOutputPin(LED_PORT, LED_BLANK);

	TIMER_A_generatePWM(
		TIMER_A0_BASE,
		TIMER_A_CLOCKSOURCE_ACLK,
		TIMER_A_CLOCKSOURCE_DIVIDER_1,
		20, // period
		TIMER_A_CAPTURECOMPARE_REGISTER_2,
		TIMER_A_OUTPUTMODE_RESET_SET,
		20 - duty_cycle // duty cycle
	);

	TIMER_A_startCounter(TIMER_A0_BASE, TIMER_A_UP_MODE);

}

void led_on()
{
	GPIO_setAsOutputPin(
			LED_PORT,
			LED_BLANK
	);

	GPIO_setOutputLowOnPin(LED_PORT, LED_BLANK);
}

void led_disable( void )
{
	GPIO_setAsOutputPin(
			LED_PORT,
			LED_BLANK
	);

	GPIO_setOutputHighOnPin(LED_PORT, LED_BLANK);
}

inline void led_toggle( void ) {
	GPIO_toggleOutputOnPin(LED_PORT, LED_BLANK);
}
