/*
 * leds.h
 * (c) 2014 George Louthan
 * 3-clause BSD license; see license.md.
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
	uint8_t rows[5];
	uint8_t lastframe;
} spriteframe;

typedef struct {
	uint16_t rows[5];
	uint8_t lastframe;
} fullframe;

extern uint16_t led_values[5];
extern uint16_t led_zeroes[5];
extern uint16_t disp_buffer[10];

extern uint8_t led_display_left;
extern uint8_t led_display_text;

void led_init();

void led_print_scroll(char*, uint8_t);
void left_sprite_animate(spriteframe*, uint8_t);
void full_animate(fullframe*, uint8_t);
void right_sprite_animate(spriteframe*, uint8_t, uint8_t, int8_t, uint8_t);
void led_timestep();

void led_set_rainbow(uint16_t value);
void led_update_display();
void led_enable(uint16_t);
void led_disable( void );
uint8_t led_post();
void led_anim_init();
void begin_sprite_animation(spriteframe*, uint8_t);

void stickman_wave();
void led_clear();

#endif /* LEDS_H_ */
