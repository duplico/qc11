/*
 * leds.h
 *
 *  Created on: May 26, 2014
 *      Author: George
 */

#ifndef LEDS_H_
#define LEDS_H_

#define LED_PORT	GPIO_PORT_P1
#define LED_DATA	GPIO_PIN5
#define LED_CLOCK	GPIO_PIN4
#define LED_LATCH	GPIO_PIN7
#define LED_BLANK	GPIO_PIN3

#define LED_PERIOD 64

#define BACK_BUFFER_HEIGHT 16
#define BACK_BUFFER_WIDTH 255

#define SCREEN_HEIGHT 5
#define SCREEN_WIDTH 14

typedef struct {
	uint8_t columns[8];
	uint8_t movement; // bit4 is set if it's the last frame.
} spriteframe;

extern uint16_t led_values[5];
extern uint16_t led_zeroes[5];
extern uint8_t led_text_scrolling;

extern const spriteframe anim_wave[];
extern const spriteframe anim_walkin[];

void led_init();

void led_print(char* text);
void led_print_scroll(char*, uint8_t, uint8_t, uint8_t);

void led_set_rainbow(uint16_t value);
void led_disp_bit_to_values(uint8_t, uint8_t);
void led_display_bits(uint16_t*);
void led_enable(uint16_t);
void led_on();
void led_disable( void );
uint8_t led_post();
void led_anim_init();
void led_animate();
inline void led_toggle( void );
void begin_sprite_animation(spriteframe*, uint8_t);

void stickman_wave();
void led_clear();

#endif /* LEDS_H_ */
