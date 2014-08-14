/*
 * (c) 2014 George Louthan
 * 3-clause BSD license; see license.md.
 *
 */
#include <stdint.h>

#define NUMBEROFLEDS	1
#define ENCODING 		3		// possible values 3 and 4

typedef struct {
	uint8_t red;
	uint8_t green;
	uint8_t blue;
} ledcolor_t;

typedef uint16_t ledcount_t;

void ws2812_init();

void fillFrameBuffer(ledcolor_t* leds, ledcount_t ledCount, uint8_t* buffer, uint8_t encoding);
void fillFrameBufferSingleColor(ledcolor_t* led, ledcount_t ledCount, uint8_t* buffer, uint8_t encoding);
void encodeData3bit(ledcolor_t* led, uint8_t* output);
void encodeData4bit(ledcolor_t* led, uint8_t* output);

void ws_set_colors_blocking(uint8_t* buffer, ledcount_t ledCount);
void ws_rotate(ledcolor_t* leds, ledcount_t ledCount);
void ws_set_colors_async(ledcount_t);

extern ledcolor_t leds[];
extern ledcolor_t blankLed;
