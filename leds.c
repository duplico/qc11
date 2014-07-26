/*
 * leds.c
 *
 *  Created on: May 26, 2014
 *      Author: George
 */

#include "qcxi.h"
#include "leds.h"
#include "fonts.h"
#include "anim.h"

#define DISPLAY_OFF 	0
#define DISPLAY_ON  	1
#define DISPLAY_ANIMATE BIT1
#define DISPLAY_ON_ANIMATE 3

#define DISPLAY_MIRROR_BIT BIT3

uint8_t led_display_left = 0;
uint8_t led_display_right = 0;
uint8_t led_display_full = 0;
uint8_t led_display_text = 0;

uint8_t led_display_left_frame = 0;
uint8_t led_display_right_frame = 0;
uint8_t led_display_full_frame = 0;

uint8_t led_display_text_character = 0;
uint8_t led_display_text_cursor = 0;

uint8_t led_display_left_len = 0;
uint8_t led_display_right_len = 0;
uint8_t led_display_full_len = 0;
uint8_t led_display_text_len = 0;

uint8_t led_display_anim_skip = 0;
uint8_t led_display_text_skip = 0;

uint8_t led_display_anim_skip_index = 0;
uint8_t led_display_text_skip_index = 0;

spriteframe* led_display_left_sprite = 0;
spriteframe* led_display_right_sprite = 0;
fullframe* led_display_full_anim = 0;

char* led_display_text_string = "";

uint16_t disp_buffer[10] = { 0 }; // 10-row buffer - bottom is animation, top is text. MS 2 bits unused.
uint16_t led_values[5] = {0};
uint8_t led_display_bottom = 5;

volatile uint8_t f_time_loop = 0;

#define DISP_MODE_ANIM  -1
#define DISP_MODE_SCROLL 0
#define DISP_MODE_TEXT   1

volatile int8_t disp_mode = DISP_MODE_TEXT;
volatile int8_t disp_mode_target = DISP_MODE_TEXT;

void led_init() {

	// Setup LED module pins //////////////////////////////////////////////////
	// LED_PORT.LED_DATA, LED_CLOCK, LED_LATCH, LED_BLANK
	P1DIR |= LED_DATA + LED_CLOCK + LED_LATCH + LED_BLANK;

	// Shift register input from LED controllers:
	// LEDPORT.PIN6 as input:
	P1DIR &= ~BIT6;
}

void left_sprite_animate(spriteframe* animation, uint8_t frameskip) {
	led_display_left = DISPLAY_ON_ANIMATE;
	led_display_left_frame = 0;
	led_display_anim_skip = frameskip;
	led_display_left_sprite = animation;
}

void full_animate(fullframe* animation, uint8_t frameskip) {
	led_display_full = DISPLAY_ON_ANIMATE;
	led_display_full_frame = 0;
	led_display_anim_skip = frameskip;
	led_display_full_anim = animation;
}

void right_sprite_animate(spriteframe* animation, uint8_t frameskip, uint8_t flip) {
	led_display_right = DISPLAY_ON_ANIMATE;
	led_display_right_frame = 0;
	led_display_anim_skip = frameskip;
	led_display_right_sprite = animation;
	if (flip)
		led_display_right |= DISPLAY_MIRROR_BIT;
}

void led_print_scroll(char* text, uint8_t frameskip) {
	led_display_text_string = text;
	led_display_text_character = 0;
	led_display_text_cursor = 0;
	led_display_text_skip = frameskip;
	led_display_text = DISPLAY_ANIMATE;
	led_display_text_len = strlen(text);

	disp_mode_target = DISP_MODE_TEXT; // Scroll to text mode if necessary.
}

void clear_anim() {
	for (uint8_t i=0; i<5; i++)
		disp_buffer[i] = 0;
}

void clear_text() {
	for (uint8_t i=5; i<10; i++)
		disp_buffer[i] = 0;
}

void led_clear() {
	clear_anim();
	clear_text();
	led_update_display();
}

// This puts the animations in rows i=0,1,2,3,4, with 0 being the bottom of the sprite.
void draw_animations() {
	clear_anim();
	for (uint8_t i=0; i<5; i++) {
		if (led_display_left)
			disp_buffer[i] |= (led_display_left_sprite[led_display_left_frame].rows[i] << 6);

		if (led_display_right & DISPLAY_MIRROR_BIT)
			disp_buffer[i] |= (uint8_t)(((led_display_left_sprite[led_display_left_frame].rows[i] * 0x0802LU & 0x22110LU) | (led_display_left_sprite[led_display_left_frame].rows[i] * 0x8020LU & 0x88440LU)) * 0x10101LU >> 16);
		else if (led_display_right)
			disp_buffer[i] |= (led_display_right_sprite[led_display_right_frame].rows[i]);

		if (led_display_full) {
			disp_buffer[i] |= (led_display_full_anim[led_display_full_frame].rows[i]);
		}
	}
}

// This puts the text in rows i=5,6,7,8,9
// with row 5 as the base of the text.
void draw_text() {
	// Character.Cursor is at the far right of the display. If Character == length, then we're filling the screen up with Cursor blank spaces.
	// This has the happy effect that the first bits we're setting are the LSBs.
	uint8_t buffer_location = 0;
	uint8_t current_char = led_display_text_character;
	uint8_t current_cursor = led_display_text_cursor;
	clear_text();

	if (current_char == led_display_text_len) {
		buffer_location += current_cursor;
		current_char--;
		// set cursor to last column of font bitmap:
		current_cursor = font_info.charInfo[led_display_text_string[current_char] - font_info.startChar].widthBits - 1;
	}
	// Else we'll just use what we've got in the global settings.

	uint16_t font_bits_index = font_info.charInfo[led_display_text_string[current_char] - font_info.startChar].offset;

	while (buffer_location < 14) {
		// Current column in the buffer is buffer_location.
		// Current column in the font is current_cursor
		// Current character is current_char.

		// Apply the current column of the current font bitmap:
		for (uint8_t row=0; row<5; row++) {
			// In the font bitmap, bit0 is the top
			// bitmap starts at: font_bits[font_bits_index]
			// column is: font_bits[font_bits_index + current_cursor]
			// bit to copy is: font_bits[font_bits_index + current_cursor] & (1 << (4-row))
			// destination is: disp_buffer[row+5] & (1 << buffer_location)
			if (font_bits[font_bits_index + current_cursor] & (1 << (4-row))) {
				disp_buffer[row+5] |= (1 << buffer_location);
			}
		}
		buffer_location++;

		if (current_cursor == 0) {
			// We just finished with a character
			// And we need to go on to the next character.
			if (current_char == 0) {
				break; // This was the last character to print, so we're done.
			}
			current_char--;
			font_bits_index = font_info.charInfo[led_display_text_string[current_char] - font_info.startChar].offset;
			current_cursor = font_info.charInfo[led_display_text_string[current_char] - font_info.startChar].widthBits - 1;
			// So next we need a blank column too.
			buffer_location++;
		} else {
			current_cursor--;
		}
	}
}

// NB: Don't call this function when we're in text mode. TODO.
void animation_timestep() {
	// Return if we're not animating or if we have to skip a frame:
	if (!(led_display_left | led_display_right | led_display_full & DISPLAY_ANIMATE)) {
		return;
	} else if (led_display_anim_skip_index) {
		led_display_anim_skip_index--; // If we're not animating, this is DONTCARE because it's set when animations start.
		return;
	}
	led_display_anim_skip_index = led_display_anim_skip;
	draw_animations();
	// end animation if done:
	if (led_display_full & DISPLAY_ANIMATE) {
		led_display_full_frame++;
		if (led_display_full_frame == led_display_full_len) {
			led_display_full &= ~DISPLAY_ANIMATE;
			f_animation_done = 1;
		}
	}
	if (led_display_left & DISPLAY_ANIMATE) {
		led_display_left_frame++;
		// end animation if done::
		if (led_display_left_sprite == stand && led_display_left_frame == led_display_left_len) {
			led_display_left &= ~DISPLAY_ANIMATE;
			f_animation_done = 1;
		} else if (led_display_left_frame == led_display_left_len) {
			left_sprite_animate(stand, led_display_anim_skip);
		}
	}
	if (led_display_right & DISPLAY_ANIMATE) {
		led_display_right_frame++;
		if (led_display_right_frame == led_display_right_len) {
			// end animation
			led_display_right &= ~DISPLAY_ANIMATE;
			f_animation_done = 1;
		}
	}
}

void text_timestep() {
	// Return if we're not animating or if we have to skip a frame:
	if (!(led_display_text & DISPLAY_ANIMATE) || led_display_text_skip_index) {
		led_display_text_skip_index--; // If we're not animating, this is DONTCARE because it's set when animations start.
		return;
	}

	led_display_text_skip_index = led_display_text_skip;
	draw_text();

	if (led_display_text_character == led_display_text_len && led_display_text_cursor == 14) {
		// done animating. TODO: go back to anim
		led_display_text &= ~DISPLAY_ANIMATE;
		f_animation_done = 1;
		disp_mode_target = DISP_MODE_ANIM;
		return;
	} else if (led_display_text_character == led_display_text_len) {
		// Done with the text, now we're just scrolling.
		led_display_text_cursor++;
	} else {
		// Need to go to the next bit of text:
		led_display_text_cursor++;
		if (led_display_text_cursor >= font_info.charInfo[led_display_text_string[led_display_text_character] - font_info.startChar].widthBits) {
			led_display_text_character++;
			led_display_text_cursor = 0;
			// TODO: Need to make sure we're handling the spaces between letters.
		}
	}
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
	led_values[0] &= 0b0111111111111110;
	led_values[1] &= 0b0111111101111111;
	led_values[2] &= 0b1111111011111110;
	led_values[3] &= 0b0111111101111111;
	led_values[4] &= 0b1111111011111110;

	led_values[0] |= (value & BIT9)? BIT15 : 0;
	led_values[1] |= (value & BIT8)? BIT15 : 0;
	led_values[1] |= (value & BIT7)? BIT7 : 0;
	led_values[3] |= (value & BIT6)? BIT15 : 0;
	led_values[3] |= (value & BIT5)? BIT7 : 0;
	led_values[0] |= (value & BIT4)? BIT0 : 0;
	led_values[2] |= (value & BIT3)? BIT8 : 0;
	led_values[2] |= (value & BIT2)? BIT0 : 0;
	led_values[4] |= ((value & BIT1)? BIT8 : 0) | ((value & BIT0)? BIT0 : 0);
}

void led_update_display() {
	// Clear everything but the rainbows on the end:
	led_values[0] &= 0b1000000000000001;

	led_values[1] &= 0b1000000010000000;
	led_values[3] &= 0b1000000010000000;

	led_values[2] &= 0b0000000100000001;
	led_values[4] &= 0b0000000100000001;

	// Top row:
	led_values[0] |= (disp_buffer[led_display_bottom+4] << 1) & 0b0111111111111110;

	// Left halves:
	led_values[1] |= (disp_buffer[led_display_bottom+3] & 0b11111110000000) << 1; // should look like 0b R 1111111 R 0000000
	led_values[1] |= (disp_buffer[led_display_bottom+2] & 0b11111110000000) >> 7; // should look like 0b R 0000000 R 1111111
	led_values[3] |= (disp_buffer[led_display_bottom+1] & 0b11111110000000) << 1;
	led_values[3] |= (disp_buffer[led_display_bottom]   & 0b11111110000000) >> 7;

	// Right halves:
	led_values[2] |= (disp_buffer[led_display_bottom+3] & 0b1111111) << 9; // should look like 0b 1111111 R 0000000 R
	led_values[2] |= (disp_buffer[led_display_bottom+2] & 0b1111111) << 1; // should look like 0b 0000000 R 1111111 R
	led_values[4] |= (disp_buffer[led_display_bottom+1] & 0b1111111) << 9;
	led_values[4] |= (disp_buffer[led_display_bottom]   & 0b1111111) << 1;

	//Set latch to low (should be already)
	P1OUT &= ~LED_LATCH;

	for (uint8_t j=0; j<5; j++) {
		// Iterate over each bit, set data pin, and pulse the clock to send it
		// to the shift register
		for (uint8_t i = 0; i < 16; i++)  {
			WRITE_IF(LED_PORT, LED_DATA, (led_values[4-j] & (1 << i)));
			GPIO_pulse(LED_PORT, LED_CLOCK);
		}
	}
	GPIO_pulse(LED_PORT, LED_LATCH);

}

uint8_t led_post()
{
	//Set latch to low (should be already)
	GPIO_setOutputLowOnPin(LED_PORT, LED_LATCH);

	uint16_t test_pattern = 0b1111101010100001;
	uint16_t test_response = 0;
	for (uint8_t j=0; j<6; j++) { // Fill all the registers with the test pattern.
		for (uint8_t i = 0; i < 16; i++)  {
			test_response |= (P1IN & GPIO_PIN6)? 1 << i : 0;
			WRITE_IF(LED_PORT, LED_DATA, (test_pattern & (1 << i)));
			GPIO_pulse(LED_PORT, LED_CLOCK);
		}
	}
	return test_response == test_pattern;
}

void led_enable(uint16_t duty_cycle) {
//	led_disable(); // TODO
	GPIO_setAsPeripheralModuleFunctionOutputPin(LED_PORT, LED_BLANK);

	TIMER_A_generatePWM(
		TIMER_A0_BASE,
		TIMER_A_CLOCKSOURCE_ACLK,
		TIMER_A_CLOCKSOURCE_DIVIDER_1,
		LED_PERIOD, // period
		TIMER_A_CAPTURECOMPARE_REGISTER_2,
		TIMER_A_OUTPUTMODE_RESET_SET,
		LED_PERIOD - duty_cycle // duty cycle
	);

	TIMER_A_startCounter(TIMER_A0_BASE, TIMER_A_UP_MODE);

}

void led_anim_init() {
	// In RTC calendar mode (which we're using), PRESCALE_1 starts sourced with
	// a 128 Hz signal.
	RTC_A_definePrescaleEvent(
		RTC_A_BASE,
		RTC_A_PRESCALE_1,
		TIME_LOOP_SCALER
	);

	// Interrupt 8 times per second for animation purposes:
	RTC_A_enableInterrupt(RTC_A_BASE, RTC_A_PRESCALE_TIMER1_INTERRUPT);
}

void led_timestep() {
	if (disp_mode != disp_mode_target) {
		// we're scrolling between display modes.
		// anim means bottom=5
		// text means bottom=0
		if (disp_mode_target == DISP_MODE_TEXT && led_display_bottom < 5) {
			led_display_bottom++;
		} else if (disp_mode_target == DISP_MODE_TEXT) {
			led_display_bottom = 5;
			disp_mode = disp_mode_target;
		} else if (disp_mode_target == DISP_MODE_ANIM && led_display_bottom > 0) {
			led_display_bottom--;
		} else if (disp_mode_target == DISP_MODE_ANIM) {
			led_display_bottom = 0; // TODO?
			disp_mode = disp_mode_target;
		}
	} else {
		animation_timestep();
		text_timestep();

		draw_animations();
		draw_text();
	}
	// TODO: Maybe only do this if we know something is changing.
	led_update_display();
}

//void led_animate() {
//
//	static uint8_t led_skip_frame;
//
//	// Check to see if we're transitioning display modes:
//	if (vscroll_to_text && vertical_mode == DISP_MODE_ANIM) {
//		disp_left = 0;
//		if (disp_top > 0) {
//			disp_top--;
//		} else {
//			// done...
//			vscroll_to_text = 0;
//			disp_top = 0; // just in case
//			vertical_mode = TEXT;
//		}
//		led_disp_bit_to_values(disp_left, disp_top);
//		led_display_bits(led_values);
//		return;
//	} else if (vscroll_to_anim && vertical_mode == TEXT) {
//		disp_left = 0;
//		if (disp_top < 8) {
//			disp_top++;
//		} else {
//			// done...
//			vscroll_to_anim = 0;
//			disp_top = 8;
//			vertical_mode = ANIMATION;
//		}
//		led_disp_bit_to_values(disp_left, disp_top);
//		led_display_bits(led_values);
//		return;
//	}
//
//	// Check to see if we need to skip this frame.
//	led_skip_frame = led_text_scrolling? led_skip_frame_text : led_skip_frame_anim;
//	if (led_skip_frame && led_frames_skipped >= led_skip_frame) {
//		led_frames_skipped = 0;
//		// DISPLAY this frame.
//	} else if (led_skip_frame) {
//		led_frames_skipped++;
//		return;
//	}
//
//	if (led_text_scrolling) {
//		if (disp_left < print_pixel_len) {
//			led_disp_bit_to_values(disp_left, disp_top);
//			led_display_bits(led_values);
//			disp_left++;
//		} else {
//			f_animation_done = !sprite_animate;
//			if (sprite_display) {
//				vscroll_to_anim = 1;
//			}
//			led_text_scrolling = 0;
//		}
//	} else if (sprite_display) {
//		draw_row_major_sprite();
//		if (sprite_animate) {
//			sprite_next_frame();
//			if (!sprite_animate) {
//				f_animation_done = 1;
//			}
//		}
//		led_disp_bit_to_values(disp_left, disp_top);
//		led_display_bits(led_values);
//	}
//}

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
