#pragma FUNC_ALWAYS_INLINE (UCS_clearAllOscFlagsWithTimeout);

#include "qcxi.h"
#include <string.h>

#include "radio.h"
#include "fonts.h"
#include "clocks.h"
#include "leds.h"
#include "ir.h"
#include "ws2812.h"
#include "main.h"

#pragma DATA_SECTION (my_conf, ".infoA");
qcxiconf my_conf;

// Interrupt flags to signal the main thread:
volatile uint8_t f_new_minute = 0;
volatile uint8_t f_timer = 0;
volatile uint8_t f_rfm_rx_done = 0;
volatile uint8_t f_ir_tx_done = 0;
volatile uint8_t f_ir_rx_ready = 0;
uint8_t f_config_clobbered = 0;
volatile uint8_t f_new_second = 0;
volatile uint8_t f_alarm = 0;
uint8_t f_paired = 0;
uint8_t f_unpaired = 0;
uint8_t f_paired_new_person = 0;
uint8_t f_paired_new_trick = 0;
uint8_t start_new_animation = 0;

// Global state:
uint8_t clock_is_set = 0;
uint16_t loops_to_rf_beacon = 10 * TIME_LOOP_HZ;
#define MTS_LEN 255
char message_to_send[MTS_LEN] = "";

#if !BADGE_TARGET
volatile uint8_t f_ser_rx = 0;
#endif

char time[6] = "00:00";

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
}

/*
 * So here's the flow of this thing.
 *
 * * STARTUP (POST, message, etc)
 * ------ block until finished ----
 *
 * Here are the things that can happen:
 *
 * Time based:
 * * Event alert raised (interrupt flag)
 * * It's been long enough that we can do a trick (set flag from time loop)
 * **  (maybe the trick is a prop)
 * * Time to beacon the radio (set flag from time loop)
 * * Time to beacon the IR (set flag from time loop)
 *
 * From the radio:
 * * Receive a beacon (at some point, we need to decide if it means we:)
 * ** adjust neighbor count
 * ** are near a base station (arrive event)
 * ** should schedule a prop
 * ** get the puppy
 * ** set our clock
 * ** confirms we should give up the puppy
 *
 * From the IR
 * * Docking with base station
 * * Pairing
 * ** possibly new person
 * *** possibly new person with a new trick
 *
 * So for the setup in the loop, we should maybe do the following:
 *
 * * First process interrupts
 * ** f_new_minute;
 * ** f_ir_tx_done;
 * ** f_ir_rx_ready;
 * ** f_time_loop;
 * ** f_rfm_rx_done;
 * ** f_new_second;
 * * Then process second-order flags
 * ** f_animation_done;
 * ** f_paired;
 * ** f_unpaired;
 * * Then do the animation things.
 *
 * Looping - here are the priorities:
 *
 * * Set clock (pre-empts)
 * * Arrived at event (animation, don't wait for previous to finish)
 * * Event alert (animation, wait for idle)
 * * Pair begins (animation and behavior pre-empts, don't wait to finish)
 * * New pairing person (wait for idle)
 * * New trick learned (wait for idle)
 * * Score earned (wait for idle)
 * * Prop earned (wait for idle)
 * * Pair expires (wait for idle)
 * * Get/lose puppy (wait for idle)
 * * Get propped (from radio beacon)
 * * Do a trick or prop (idle only)
 *
 * * Adjust neighbor count (from radio beacon)
 * *
 *
 */

int main( void )
{
	init_watchdog();
	init_power();
	init_gpio();
	init_clocks();
	check_config();
	led_init();
	init_rtc();
	init_ir();
	init_alarms();
#if !BADGE_TARGET
	ws2812_init();
	ser_init();
#endif
	__bis_SR_register(GIE);
	init_radio(); // requires interrupts enabled.

	// Power-on self test:
	uint8_t post_result = post();
	if (post_result != 0) {
#if BADGE_TARGET
		// Display error code:
		char hex[4] = {0, 0, 0, 0};
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
#if BADGE_TARGET
	led_clear();
	led_anim_init();
	led_enable(LED_PERIOD/2);
#else
	uint8_t color = 0;
#endif

#if BADGE_TARGET
	// Startup sequence:
	uint8_t startup_seq_index = 0;
	led_clear();
	led_print_scroll("qcxi", 1, 1, 0);

	while (startup_seq_index<1) {
		// Time to do something because of time?
		if (f_time_loop) {
			f_time_loop = 0;
			led_animate();
		}

		// Is an animation finished?
		if (f_animation_done) {
			f_animation_done = 0;
			startup_seq_index++;
			switch(startup_seq_index) {
//			case 1:
//				led_print_scroll("Hello NAMENAME. I am your badge.", 1, 1, 0);
//				break;
//			case 2:
//				led_print_scroll("There are many like me, but I am yours.", 1, 1, 0);
//				break;
//			case 3:
//				led_print_scroll("Please leave my batteries in!", 1, 1, 0);
//				break;
			}
		}
	}
	delay(750);
#endif

	// Signals within the main thread:
	static uint8_t s_event_arrival = 0,
				   s_on_bus = 0,
				   s_event_alert = 0,
				   s_pair = 0, // f_pair
				   s_new_pair = 0,
				   s_new_trick = 0,
				   s_new_score = 0,
				   s_new_prop = 0,
				   s_unpair = 0, // f_unpair
				   s_propped = 0,
				   s_trick = 0,
				   s_prop = 0,
				   s_get_puppy = 0,
				   s_lose_puppy = 0;

	// Main sequence:
	begin_sprite_animation((spriteframe *) anim_walkin, 4);
	while (1) {

#if !BADGE_TARGET
		// New serial message?
		if (f_ser_rx) {
//			ser_print((uint8_t *) ser_buffer_rx);
			f_ser_rx = 0;
		}
#endif

		/*
		 * From the IR
		 * * Docking with base station
		 * * Pairing
		 * ** possibly new person
		 * *** possibly new person with a new trick
		 *
		 * ir_process_rx_ready() will do all this processing for us, and
		 *  if necessary set the flags for:
		 *  f_paired
		 *  f_unpaired
		 *  f_paired_new_person
		 *  f_paired_new_trick
		 *
		 * Here we can set:
		 *
		 *	   s_pair = 0, // f_pair
		 *	   s_new_pair = 0,
		 *	   s_new_trick = 0,
		 *	   s_new_score = 0,
		 *	   s_new_prop = 0,
		 *	   s_unpair = 0, // f_unpair
		 *
		 *
		 */
		if (f_ir_rx_ready) {
			f_ir_rx_ready = 0;
			ir_process_rx_ready();
		}

		if (f_paired) {
			f_paired = 0;
			s_pair = 1;
			led_print_scroll("pair", 1, 1, 0);
		} else if (f_unpaired) {
			led_print_scroll("unpair", 1, 1, 0);
			f_unpaired = 0;
			s_unpair = 1;
		}

		/*
		 * Unlike with the IR pairing mechanism, there is very little state
		 *  in the RF system. So we just need to load up the beacon into a
		 *  buffer, and decide if we:
		 *   * adjust neighbor count
		 *   * are near a base station (arrive event)
		 *   * should schedule a prop
		 *   * set our clock
		 *   * get the puppy
		 *   * got confirmation we should give up the puppy (this will have
		 *      some state)
		 *
		 *
		 *  Here we can set:
		 *   s_event_arrival = 0,
		 *	 s_on_bus = 0;
		 *	 s_new_score = 0,
		 *	 s_new_prop = 0,
		 *	 s_get_puppy;
		 *	 s_lose_puppy;
		 *
		 *
		 */
		if (f_rfm_rx_done) {
			// do something
		}

		/*
		 * We just got a time loop interrupt, which happens roughly
		 * TIME_LOOP_HZ times per second.
		 *
		 *  * Time loop based activities:
		 * * It's been long enough that we can do a trick (set flag from time loop)
		 * **  (maybe the trick is a prop)
		 * * Time to beacon the radio (set flag from time loop)
		 * * Time to beacon the IR (set flag from time loop)
		 *
	     * s_trick = 0,
		 * s_prop = 0;
		 *
		 */
		if (f_time_loop) {
			f_time_loop = 0;
#if BADGE_TARGET
			led_animate();
#else
			fillFrameBufferSingleColor(&leds[color], NUMBEROFLEDS, ws_frameBuffer, ENCODING);
			ws_set_colors_async(NUMBEROFLEDS);
			color++;
			if (color==21) f_animation_done = 1;
#endif
			if (loops_to_ir_timestep) {
				loops_to_ir_timestep--;
			} else {
				loops_to_ir_timestep = 8;
				ir_process_timestep();
			}

			if (0) {
				// TODO: radio
			}
		}


		static uint8_t event_id = 0;
		/*
		 * Calendar interrupts:
		 *
		 * * Event alert raised (interrupt flag)
		 * * Time to do a prop.
		 *
		 * s_event_alert = 0,
		 * s_propped = 0,
		 *
		 */
		if (f_alarm & BIT7) {
			// TODO: prop
		} else if (f_alarm) {
			event_id = f_alarm & 0b0111;
			if (f_alarm & ALARM_START_LIGHT) {
				// TODO: setup a light blink for light number event_id.
			}
			if (f_alarm & ALARM_STOP_LIGHT) {
				// TODO: stop the blinking if applicable.
			}
			if (f_alarm & ALARM_DISP_MSG) {
				memset(message_to_send, 0, MTS_LEN);
				if (f_alarm & ALARM_NOW_MSG)
					strcat(message_to_send, "!!! ");
				strcat(message_to_send, event_messages[event_id]);
				if (f_alarm & ALARM_NOW_MSG)
					strcat(message_to_send, "NOW!");
				else {
					strcat(message_to_send, event_times[event_id]);
				}
				led_print_scroll(message_to_send, 1, 1, 1);
			}
		}
		if (f_alarm) {
			f_alarm = 0;
			init_alarms();
		}

		/*
		 * Animation related activities:
		 *
		 * * Set clock (pre-empts)
		 * * Arrived at event (animation, don't wait for previous to finish)
		 * * Event alert (animation, wait for idle)
		 * * Pair begins (animation and behavior pre-empts, don't wait to finish)
		 * * New pairing person (wait for idle)
		 * * New trick learned (wait for idle)
		 * * Score earned (wait for idle)
		 * * Prop earned (wait for idle)
		 * * Pair expires (wait for idle)
		 * * Get/lose puppy (wait for idle)
		 * * Get propped (from radio beacon)
		 * * Do a trick or prop (idle only)
		 */

		// Time to handle signals.

		// Is an animation finished?
		if (f_animation_done || start_new_animation) {
			f_animation_done = 0;
#if BADGE_TARGET
#else
			color = 0;
#endif
		}

		// Going to sleep... mode...
		__bis_SR_register(LPM3_bits + GIE);
	}
} // end main()

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

	static const uint16_t tp0[5] = {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF};
	static const uint16_t tp1[5] = {0xFF00, 0xFF00, 0xFF00, 0xFF00, 0xFF00};
	static const uint16_t tp2[5] = {0x00FF, 0x00FF, 0x00FF, 0x00FF, 0x00FF};

	if (led_post() == STATUS_FAIL) {
		post_result |= POST_SHIFTF;
	}
	// LED test pattern
	led_display_bits((uint16_t *) tp0);
	for (uint8_t i=LED_PERIOD; i>0; i--) {
		led_enable(i);
		delay(8);
	}
	led_display_bits((uint16_t *) tp1);
	for (uint8_t i=LED_PERIOD; i>0; i--) {
		led_enable(i);
		delay(8);
	}
	led_display_bits((uint16_t *) tp2);
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
	static const char test_str[] = "qcxi";
	ir_write((uint8_t *) test_str, 0xff, 0);
	uint16_t spin = 65535;
	while (spin-- && !f_ir_rx_ready);
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
		strcpy((char *) my_conf.handle, "person");
		strcpy((char *) my_conf.message, "Hi new person.");

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
	strcpy(ir_pair_payload+2, my_conf.handle);
	strcpy(ir_pair_payload+2+11, my_conf.message);
	// TODO: the opposite of WDT_A_hold(WDT_A_BASE);
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
