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

#define BACK_BUFFER_HEIGHT 16
#define BACK_BUFFER_WIDTH 255

#define SCREEN_HEIGHT 5
#define SCREEN_WIDTH 14

extern uint16_t led_values[5];
extern uint16_t led_zeroes[5];
extern uint8_t led_animating;

void led_print(char* text);
void led_print_scroll(char*, uint8_t, uint8_t, uint8_t);

void led_set_rainbow(uint16_t value);
void led_disp_bit_to_values(uint8_t left, uint8_t top);
void led_display_bits(uint16_t* val);
void led_enable(uint16_t duty_cycle);
void led_on();
void led_disable( void );
void led_post();
void led_anim_init();
void led_animate();
inline void led_toggle( void );

#endif /* LEDS_H_ */
