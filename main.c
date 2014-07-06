#include <string.h>

#include "qcxi.h"
#include "radio.h"
#include "fonts.h"
#include "clocks.h"
#include "leds.h"
#include "ir.h"
#include "ws2812.h"

#pragma DATA_SECTION (my_conf, ".infoA");
qcxiconf my_conf;

// Interrupt flags to signal the main thread:
volatile uint8_t f_new_minute = 0;
volatile uint8_t f_timer = 0;
volatile uint8_t f_rfm_job_done = 0;
volatile uint8_t f_rfm_rx_done = 0;
volatile uint8_t f_ir_tx_done = 0;
volatile uint8_t f_ir_rx_ready = 0;
uint8_t f_config_clobbered = 0;

#if !BADGE_TARGET
volatile uint8_t f_ser_rx = 0;
#endif

void init_power() {
#if BADGE_TARGET
	// Set Vcore to 1.8 V - NB: allows MCLK up to 8 MHz only
	PMM_setVCore(PMM_CORE_LEVEL_0);
#else
	PMM_setVCore(PMM_CORE_LEVEL_3);
#endif
}

void init_gpio() {

	// Start out by turning off all the pins.
	P1DIR = 0xFF;
	P1OUT = LED_BLANK;
	P2DIR = 0xFF;
	P2OUT = 0x00;
	P3DIR = 0xFF;
	P3OUT = 0x00;
	P4DIR = 0xFF;
	P4OUT = 0x00;
	P5DIR = 0xFF;
	P5OUT = 0x00;
	P6DIR = 0xFF;
	P6OUT = 0x00;

	// External crystal pins //////////////////////////////////////////////////
	//  __ X1
	// |  |----P5.4
	// |__|----P5.5
	//
	//  __ X2
	// |  |----P5.2
	// |__|----P5.3
	//
	GPIO_setAsPeripheralModuleFunctionOutputPin(
			GPIO_PORT_P5,
			GPIO_PIN2 + GPIO_PIN3 // XT2
	   // + GPIO_PIN4 + GPIO_PIN5 // XT1 // TODO
	);

#if BADGE_TARGET

	// Setup LED module pins //////////////////////////////////////////////////
	//   bit-banged serial data output:
	//
	// LED_PORT.LED_DATA, LED_CLOCK, LED_LATCH
	//
	GPIO_setAsOutputPin(
			LED_PORT,
			LED_DATA + LED_CLOCK + LED_LATCH // + LED_BLANK
	);

	// BLANK pin (we turn on PWM later as needed):
	GPIO_setAsOutputPin(LED_PORT, LED_BLANK);
	// Shift register input from LED controllers:
	GPIO_setAsInputPin(LED_PORT, GPIO_PIN6);
#endif
	// IR pins ////////////////////////////////////////////////////////////////
	//
	// P4.4, 4.5, 4.6
	//
	// TX for IR
	GPIO_setAsPeripheralModuleFunctionOutputPin(
			IR_TXRX_PORT,
			IR_TX_PIN
	);
	// RX for IR
	GPIO_setAsPeripheralModuleFunctionInputPin(
			IR_TXRX_PORT,
			IR_RX_PIN
	);

	// Shutdown (SD) for IR
	GPIO_setAsOutputPin(IR_SD_PORT, IR_SD_PIN);
	GPIO_setOutputLowOnPin(IR_SD_PORT, IR_SD_PIN); // shutdown low = on

	// Interrupt pin for radio:
	GPIO_setAsInputPin(GPIO_PORT_P2, GPIO_PIN0);
	GPIO_enableInterrupt(GPIO_PORT_P2, GPIO_PIN0);
	GPIO_interruptEdgeSelect(GPIO_PORT_P2, GPIO_PIN0, GPIO_LOW_TO_HIGH_TRANSITION);
	GPIO_clearInterruptFlag(GPIO_PORT_P2, GPIO_PIN0);
}

uint8_t reg_read = 0;
uint8_t reg_reads[2] = {0, 0};

uint8_t reg_data[65] = {0};
uint8_t test_data[65] = {'q', 'c', 'x', 'i', 0};

char time[6] = "00:00";

uint8_t receive_status;

uint8_t packet_sent = 0;

uint16_t _rotl(uint16_t value, int shift) {
    if ((shift &= sizeof(value)*8 - 1) == 0)
      return value;
    return (value << shift) | (value >> (sizeof(value)*8 - shift));
}

uint8_t post() {
	__bic_SR_register(GIE);
	uint8_t post_result = 0;

	// Clocks
	if (xt1_status == STATUS_FAIL) {
		post_result |= POST_XT1F;
	}
	if (xt2_status == STATUS_FAIL) {
		post_result |= POST_XT2F;
	}
#if BADGE_TARGET

	uint16_t tp0[5] = {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF};
	uint16_t tp1[5] = {0xFF00, 0xFF00, 0xFF00, 0xFF00, 0xFF00};
	uint16_t tp2[5] = {0x00FF, 0x00FF, 0x00FF, 0x00FF, 0x00FF};

	if (led_post() == STATUS_FAIL) {
		post_result |= POST_SHIFTF;
	}
	// LED test pattern
	led_display_bits(tp0);
	for (uint8_t i=LED_PERIOD; i>0; i--) {
		led_enable(i);
		delay(8);
	}
	led_display_bits(tp1);
	for (uint8_t i=LED_PERIOD; i>0; i--) {
		led_enable(i);
		delay(8);
	}
	led_display_bits(tp2);
	for (uint8_t i=LED_PERIOD; i>0; i--) {
		led_enable(i);
		delay(8);
	}
	led_disable();
	delay(500);
#endif
	__bis_SR_register(GIE);

	ir_reject_loopback = 0;
	// IR loopback
	char test_str[] = "qcxi";
	ir_write((uint8_t *) test_str, 0);
	delay(200);
	if (f_ir_rx_ready) {
		f_ir_rx_ready = 0;
		if (!ir_check_crc()) {
			// IR integrity fault
			post_result |= POST_IRIF;
		} else {
			if (strcmp(test_str, (char *) ir_rx_frame) != 0)
				post_result |= POST_IRVF; // IR value fault
		}
	} else {
		post_result |= POST_IRGF; // IR general fault
	}
	ir_reject_loopback = 1;
	// Radio - TODO

	return post_result;
}

void check_config() {
	WDT_A_hold(WDT_A_BASE);

	uint16_t crc = 0;
	uint8_t* config_bytes = (uint8_t *) &my_conf;

	CRC_setSeed(CRC_BASE, 0xBEEF);

	for (uint8_t i=0; i<sizeof(my_conf) - 2; i++) {
		CRC_set8BitData(CRC_BASE, config_bytes[i]);
	}

	crc = CRC_getResult(CRC_BASE);

	if (crc != my_conf.crc) {
		qcxiconf new_conf;
		uint8_t* new_config_bytes = (uint8_t *) &new_conf;
		for (uint8_t i=0; i<25; i++) {
			new_config_bytes[i] = 0;
			// paired_ids, seen_ids, scores, events occurred and attended.
		}
		// TODO: set self to seen/paired, I guess.
		new_conf.badge_id = 100;
		new_conf.datetime[0] = 0; // TODO: Pre-con party time.
		new_conf.datetime[1] = 0;
		strcpy(my_conf.handle, "person");
		strcpy(my_conf.message, "Hi new person.");

		CRC_setSeed(CRC_BASE, 0xBEEF);

		for (uint8_t i=0; i<sizeof(my_conf) - 2; i++) {
			CRC_set8BitData(CRC_BASE, new_config_bytes[i]);
		}

		new_conf.crc = CRC_getResult(CRC_BASE);

		FLASH_unlockInfoA();
		uint8_t flash_status = 0;
		do {
			FLASH_segmentErase((uint8_t *)INFOA_START);
			flash_status = FLASH_eraseCheck((uint8_t *)INFOA_START, 128);
		} while (flash_status == STATUS_FAIL);

		FLASH_write8(new_config_bytes, (uint8_t *)INFOA_START, sizeof(qcxiconf));
		FLASH_lockInfoA();
	}
	// TODO: the opposite of WDT_A_hold(WDT_A_BASE);
}

int main( void )
{
	// TODO: check to see what powerup mode we're in.
	init_watchdog();
	init_power();
	init_gpio();
	init_clocks();
	init_timers();
	init_rtc();
	check_config();
	init_ir();
	__bis_SR_register(GIE);
	init_radio(); // requires interrupts enabled.
#if !BADGE_TARGET
	ws2812_init();
	ser_init();
#endif

	volatile uint8_t post_result = post();

	if (post_result != 0) {
#if BADGE_TARGET
		// Display error code:
		char hex[4] = "AA";
		hex[0] = (post_result/16 < 10)? '0' + post_result/16 : 'A' - 10 + post_result/16;
		hex[1] = (post_result%16 < 10)? '0' + post_result%16 : 'A' - 10 + post_result%16;
		led_print(hex);
		for (uint8_t i=LED_PERIOD; i>0; i--) {
			led_enable(i);
			delay(25);
		}
#else
		fillFrameBufferSingleColor(&leds[6], NUMBEROFLEDS, ws_frameBuffer, ENCODING);
		ws_set_colors_async(NUMBEROFLEDS);
		delay(1000);
#endif
	}

	mode_sb_sync();
	led_print("...");

#if !BADGE_TARGET
	while(1) {

		for (uint8_t color = 0; color<21; color++) {
			fillFrameBufferSingleColor(&leds[color], NUMBEROFLEDS, ws_frameBuffer, ENCODING);
			ws_set_colors_async(NUMBEROFLEDS);
			if (f_ir_rx_ready) {
				f_ir_rx_ready = 0;
				if (!ir_check_crc()) {
					continue;
				}
				fillFrameBufferSingleColor(&leds[1], NUMBEROFLEDS, ws_frameBuffer, ENCODING);
				ws_set_colors_async(NUMBEROFLEDS);
				delay(1000);
				break;
			}

			if (f_ser_rx) {
				ser_print(ser_buffer_rx);
				f_ser_rx = 0;
			}

			__delay_cycles(0x7FFFF);
		}

		fillFrameBufferSingleColor(&leds[2], NUMBEROFLEDS, ws_frameBuffer, ENCODING);
		ws_set_colors_async(NUMBEROFLEDS);

		ir_write("qcxi", 0);

		fillFrameBufferSingleColor(&leds[8], NUMBEROFLEDS, ws_frameBuffer, ENCODING);
		ws_set_colors_async(NUMBEROFLEDS);

		write_single_register(0x25, 0b00000000); // GPIO map to default
		radio_send(test_data, 64);
		f_rfm_job_done = 0;
		mode_tx_async();

		while (!f_rfm_job_done);

		fillFrameBufferSingleColor(&leds[12], NUMBEROFLEDS, ws_frameBuffer, ENCODING);
		ws_set_colors_async(NUMBEROFLEDS);

		f_rfm_job_done = 0;
		mode_sb_sync();

		fillFrameBufferSingleColor(&leds[15], NUMBEROFLEDS, ws_frameBuffer, ENCODING);
		ws_set_colors_async(NUMBEROFLEDS);

		delay(1207);

		ser_print("Hello!");

	}
#endif


	led_on();
	uint8_t startup_test = 0;

	led_anim_init(); // start a-blinkin.
	f_animation_done = 1;

	char hex[4] = "AA";
	uint8_t val;

	uint8_t seen_j = 255;
	while (1) {
		seen_j = 255;
		for (uint8_t j=1; j<3; j++) {
			for (uint16_t i=1; i!=0; i++)
				if (f_ir_rx_ready) {
					if (!ir_check_crc()) {
						f_ir_rx_ready = 0;
						continue;
					}
					seen_j = j;
					f_ir_rx_ready = 0;
					led_print((char *)ir_rx_frame);
				}
			if (seen_j != j) {
				led_print("...");
			}
		}
		ir_write("qcxi", 0);
	}

	while (1) {
		if (f_animate) {
			f_animate = 0;
			led_animate();
		}

		if (f_animation_done) {
			f_animation_done = 0;
			led_print_scroll("Startup", (startup_test & 0b10) >> 1, startup_test & 0b01, 1);
			startup_test++;
		}
		__bis_SR_register(LPM3_bits + GIE);
	}
}

void delay(uint16_t ms)
{
	while (ms--)
    {
        __delay_cycles(MCLK_DESIRED_FREQUENCY_IN_KHZ);
    }
}

#pragma vector=UNMI_VECTOR
__interrupt void NMI_ISR(void)
{
	static uint16_t status;
	do {
		// If it still can't clear the oscillator fault flags after the timeout,
		// trap and wait here.
		// TODO: We now should not be able to reach this point.
		status = UCS_clearAllOscFlagsWithTimeout(1000);
	} while (status != 0);
}

#pragma vector=PORT2_VECTOR
__interrupt void radio_interrupt_0(void)
{
	f_rfm_job_done = 1;
	GPIO_clearInterruptFlag(GPIO_PORT_P2, GPIO_PIN0);
}
