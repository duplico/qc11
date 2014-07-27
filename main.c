#pragma FUNC_ALWAYS_INLINE (UCS_clearAllOscFlagsWithTimeout);

#include "qcxi.h"
#include <string.h>
#include <stdlib.h>

#include "radio.h"
#include "fonts.h"
#include "clocks.h"
#include "leds.h"
#include "ir.h"
#include "ws2812.h"
#include "anim.h"
#include "main.h"

#pragma DATA_SECTION (my_conf, ".infoA");
qcxiconf my_conf;

// Interrupt flags to signal the main thread:
volatile uint8_t f_new_minute = 0;
volatile uint8_t f_timer = 0;
volatile uint8_t f_rfm_rx_done = 0;
volatile uint8_t f_rfm_tx_done = 0;
volatile uint8_t f_ir_tx_done = 0;
volatile uint8_t f_ir_rx_ready = 0;
uint8_t f_config_clobbered = 0;
volatile uint8_t f_new_second = 0;
volatile uint8_t f_alarm = 0;
uint8_t f_paired = 0;
uint8_t f_unpaired = 0;
uint8_t f_paired_new_person = 0;
uint8_t f_paired_new_trick = 0;
uint8_t f_animation_done = 0;
uint8_t f_ir_itp_step = 0;
uint8_t f_ir_pair_abort = 0;

// Global state:
uint8_t clock_is_set = 0;
uint8_t my_clock_authority = 0;
uint16_t clock_setting_age = 0;
uint16_t loops_to_rf_beacon = 10 * TIME_LOOP_HZ;
#define MTS_LEN 255
char message_to_send[MTS_LEN] = "";
uint8_t my_trick = 0;
uint16_t known_tricks = 0;
uint8_t known_trick_count = 0;
qcxipayload in_payload, out_payload;
uint16_t rainbow_lights = 0;

// My general app-level status:
uint8_t badge_status = 0;
uint8_t am_idle = 1;

#define PAIR_INIT 0
#define PAIR_ONSCREEN 1
#define PAIR_WAVE 2
#define PAIR_GREETING 3
#define PAIR_MESSAGE 4
#define PAIR_IDLE 5

uint8_t pair_state = 0;

// Gaydar - Stolen from QC10:
uint8_t neighbor_counts[RECEIVE_WINDOW] = {0};
uint8_t window_position = 0;
uint8_t badges_seen[BADGES_IN_SYSTEM];
uint8_t total_badges_seen = 0;
uint8_t uber_badges_seen = 0;
uint8_t last_neighbor_count = 0;
uint8_t neighbor_count = 0;
uint8_t neighbor_count_cycle = 0;
uint8_t window_seconds = RECEIVE_WINDOW_LENGTH_SECONDS;
uint8_t trick_seconds = TRICK_INTERVAL_SECONDS;
uint8_t target_gaydar_index = 0;
uint8_t gaydar_index = 0;

uint8_t my_score = 0;

#if !BADGE_TARGET
volatile uint8_t f_ser_rx = 0;
#endif

char time[6] = "00:00";

void init_power() {
#if BADGE_TARGET
	// Set Vcore to 1.8 V - NB: allows MCLK up to 8 MHz only
//	PMM_setVCore(PMM_CORE_LEVEL_0);
//	PMMCTL0 &= 0b11111100;
//	PMMCTL0 |= 0b11;
#else
	PMM_setVCore(PMM_CORE_LEVEL_3);
//	PMMCTL0 |= 0b11;
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

void set_badge_seen(uint8_t id) {
	uint8_t badge_frame = id / 16;
	uint8_t badge_bit = 1 << (id % 16);
	if (!(~(my_conf.met_ids[badge_frame]) & badge_bit)) {
		// haven't seen it, so we need to set its 1 to a 0.
		uint16_t new_config_word = my_conf.met_ids[badge_frame] & ~(badge_bit);
		FLASH_unlockInfoA();// TODO
		FLASH_write16(&new_config_word, &(my_conf.met_ids[badge_frame]), 1);
		FLASH_lockInfoA();
	} // otherwise, nothing to do.
}

uint8_t paired_badge(uint8_t id) {
	uint8_t badge_frame = id / 16;
	uint8_t badge_bit = 1 << (id % 16);
	return (~(my_conf.paired_ids[badge_frame]) & badge_bit)? 1: 0;
}

void set_badge_paired(uint8_t id) {
	strcpy(ir_rx_handle, (char *) &(ir_rx_frame[2]));
	strcpy(ir_rx_message, (char *) &(ir_rx_frame[2+11]));

	uint8_t badge_frame = id / 16;
	uint8_t badge_bit = 1 << (id % 16);

	// See if this is a new pair.
	if (!(~(my_conf.paired_ids[badge_frame]) & badge_bit)) {
		f_paired_new_person = 1;
		// haven't seen it, so we need to set its 1 to a 0.
		uint16_t new_config_word = my_conf.paired_ids[badge_frame] & ~(badge_bit);
		FLASH_unlockInfoA();// TODO
		FLASH_write16(&new_config_word, &(my_conf.paired_ids[badge_frame]), 1);
		FLASH_lockInfoA();

		if (!have_trick(id % TRICK_COUNT)) {
			// new trick
			f_paired_new_trick = (id % TRICK_COUNT);
			known_trick_count++;
			known_tricks |= (1 << f_paired_new_trick);
			f_paired_new_trick++; // because this flag is trick_id+1
		}

	} // otherwise, nothing to do.
	f_paired = 1;
	ir_pair_setstate(ir_proto_state+1);
}

uint8_t have_trick(uint8_t trick_id) {
	return known_tricks & (1 << trick_id);
}

void set_gaydar_target() {
	if (neighbor_count > 3)
		target_gaydar_index = 3;
	else
		target_gaydar_index = neighbor_count;
}

/*
 * So here's the flow of this thing.
 *
 * * STARTUP (POST, message, etc)
 * ------ block until finished ----
 *
 * Here are the things that can be flagged:
 *
 * Time based:
 * * TODO: Event alert raised (interrupt flag)
 * * DONE It's been long enough that we can do a trick (set flag from time loop)
 * ** TODO  (maybe the trick is a prop)
 * * DONE Time to beacon the radio (set flag from time loop)
 * * DONE Time to beacon the IR (set flag from time loop)
 *
 * From the radio:
 * * DONE Receive a beacon (at some point, we need to decide if it means we:)
 * ** TODO adjust neighbor count
 * ** TODO are near a base station (arrive event)
 * ** DONE should schedule a prop
 * ** TODO get the puppy
 * ** DONE set our clock
 * ** TODO confirms we should give up the puppy
 *
 * From the IR
 * * TODO Docking with base station
 * * DONE Pairing
 * ** DONE possibly new person
 * *** DONE possibly new person with a new trick
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

	srand(my_conf.badge_id);

	// Power-on self test:
	uint8_t post_result = post();
	if (post_result != 0) {
#if BADGE_TARGET
		// Display error code:
		char hex[4] = {0, 0, 0, 0};
		hex[0] = (post_result/16 < 10)? '0' + post_result/16 : 'A' - 10 + post_result/16;
		hex[1] = (post_result%16 < 10)? '0' + post_result%16 : 'A' - 10 + post_result%16;
		led_print_scroll(hex, 4);
#else
		fillFrameBufferSingleColor(&leds[6], NUMBEROFLEDS, ws_frameBuffer, ENCODING);
		ws_set_colors_async(NUMBEROFLEDS);
		delay(1000);
#endif
	} else {
		led_print_scroll("qcxi", 0);
	}
#if BADGE_TARGET
	led_set_rainbow(0);
	led_clear();
	led_enable(LED_PERIOD/2);
#else
	uint8_t color = 0;
#endif
	led_anim_init();

	static uint8_t s_prop_id = 0,
				   s_prop_cycles = 0,
				   s_prop_authority = 0,
				   s_propped = 0;

	// Signals within the main thread:
	static uint8_t s_event_arrival = 0,
				   s_on_bus = 0,
				   s_off_bus = 0,
				   s_event_alert = 0,
				   s_need_rf_beacon = 0,
				   s_rf_retransmit = 0,
				   s_pair = 0, // f_pair
				   s_new_pair = 0,
				   s_new_trick = 0,
				   s_new_score = 0,
				   s_new_prop = 0,
				   s_unpair = 0, // f_unpair
				   s_trick = 0,
				   s_prop = 0,
				   s_prop_animation_length = 0,
				   s_get_puppy = 0,
				   s_lose_puppy = 0,
				   s_update_rainbow = 0;

	static uint8_t itps_pattern = 0;

	// Main sequence:
	while (1) {
		if (f_rfm_tx_done) {
			f_rfm_tx_done = 0;
			// Back to normal RX automode:
			write_single_register(0x3b, RFM_AUTOMODE_RX);
		}

		if (f_rfm_rx_done) {
			f_rfm_rx_done = 0;
			if (in_payload.beacon) {
				// It's a beacon (one per cycle).
				// Increment our beacon count in the current position in our
				// sliding window.
				neighbor_counts[window_position]+=1;

				if (neighbor_counts[window_position] > neighbor_count) {
					neighbor_count = neighbor_counts[window_position];
					neighbor_count_cycle = window_position;
					set_gaydar_target();
				} else if (neighbor_counts[window_position] == neighbor_count) {
					neighbor_count_cycle = window_position;
				}
				led_print_scroll("beacon", 2);
			}
		}

		if (f_new_second) {
			f_new_second = 0;

			// BIT7 of events_occurred is !defcon_over:
			if (!clock_is_set && (BIT7 & my_conf.events_occurred)) {
				// TODO
				rainbow_lights ^= BIT9;
				s_update_rainbow = 1;
			}

			clock_setting_age++;
			currentTime.Seconds++;
			if (currentTime.Seconds >= 60) {
				currentTime = RTC_A_getCalendarTime(RTC_A_BASE);
			}

			window_seconds--;
			if (!window_seconds) {
				window_seconds = RECEIVE_WINDOW_LENGTH_SECONDS;
				s_need_rf_beacon = 1;

				window_position = (window_position + 1) % RECEIVE_WINDOW;
				neighbor_counts[window_position] = 0;

				// If the cycle we're currently clobbering is the high water
				// mark for the gaydar count, we need to loop through the
				// cycles and find the next-highest:
				if (neighbor_count_cycle == window_position) {
					neighbor_count = 0;
					for (uint8_t i=0; i<RECEIVE_WINDOW; i++) {
						if (neighbor_counts[i] > neighbor_count) {
							neighbor_count = neighbor_counts[i];
						}
					}
				}

				set_gaydar_target();
			}
		}

		if (f_time_loop) {
			f_time_loop = 0;
#if BADGE_TARGET
			led_timestep();
#else
			fillFrameBufferSingleColor(&leds[color], NUMBEROFLEDS, ws_frameBuffer, ENCODING);
			ws_set_colors_async(NUMBEROFLEDS);
			color++;
			if (color==21) f_animation_done = 1;
#endif
		}

		// This is background:
		if (s_need_rf_beacon && rfm_reg_state == RFM_REG_IDLE) {
			out_payload.beacon = 1;
			radio_send_sync();
			s_need_rf_beacon = 0;
		} else if (s_rf_retransmit && rfm_proto_state == RFM_PROTO_RX_IDLE && rfm_reg_state == RFM_REG_IDLE) {
			out_payload.beacon = 0;
			radio_send_sync();
			s_rf_retransmit = 0;
		}
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

	led_set_rainbow(0b1111111111);

	if (led_post() == STATUS_FAIL) {
		post_result |= POST_SHIFTF;
	}
	// LED test pattern
	memset(disp_buffer, 0xff, sizeof disp_buffer);
	led_update_display();
	for (uint8_t i=LED_PERIOD; i>0; i--) {
		led_enable(i);
		delay(20);
	}
	led_disable();
	delay(500);
#endif
	__bis_SR_register(GIE);

	ir_reject_loopback = 0;
	// IR loopback
	ir_write("test", 0xff, 0);
#if BADGE_TARGET
	uint16_t spin = 65535;
#else
	uint32_t spin = 1572840;
#endif
	while (spin-- && !f_ir_rx_ready);
	if (f_ir_rx_ready) {
		f_ir_rx_ready = 0;
		if (!ir_check_crc()) {
			// IR integrity fault
			post_result |= POST_IRIF;
		} else {
			if (strcmp("test", (char *) ir_rx_frame) != 0)
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

	for (uint8_t i=0; i<sizeof(qcxiconf) - 2; i++) {
		CRC_set8BitData(CRC_BASE, config_bytes[i]);
	}

	crc = CRC_getResult(CRC_BASE);

	if (crc != my_conf.crc || 1) { // TODO!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
		qcxiconf new_conf;
		uint8_t* new_config_bytes = (uint8_t *) &new_conf;
		for (uint8_t i=0; i<50; i++) {
			new_config_bytes[i] = 0xff;
			// paired_ids, seen_ids, scores, events occurred and attended.
		}
		// TODO: set self to seen/paired, I guess.
		new_conf.badge_id = 102;
		// new_conf.datetime is a DONTCARE because the clock's damn well not
		// going to be set anyway, and we have no idea what time it is,
		// and this section should (c) never be reached.
		strcpy((char *) new_conf.handle, "person");
		strcpy((char *) new_conf.message, "Hi new person.");

		CRC_setSeed(CRC_BASE, 0xBEEF);

		for (uint8_t i=0; i<sizeof(qcxiconf) - 2; i++) {
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

	// Decide which tricks we know:
	my_trick = my_conf.badge_id % TRICK_COUNT;
	known_tricks = 1 << my_trick;
	known_trick_count = 1;

	for (uint8_t trick_id = 0; trick_id < TRICK_COUNT; trick_id++) {
		if (trick_id == my_trick) continue;
		for (uint8_t badge_id = trick_id; badge_id < BADGES_IN_SYSTEM; badge_id+=TRICK_COUNT) {
			if (paired_badge(badge_id) && !(known_tricks & 1<<(trick_id))) {
				known_tricks |= 1 << trick_id;
				known_trick_count++;
				break;
			}
		}
	}

	// Time:
	memcpy(&currentTime, &my_conf.datetime, sizeof (Calendar));

	// Setup our IR pairing payload:
	strcpy(&(ir_pair_payload[0]), my_conf.handle);
	strcpy(&(ir_pair_payload[11]), my_conf.message);

//	uint8_t to_addr, from_addr, base_id, puppy_flags, clock_authority,
//			seconds, minutes, hours, day, month;
//	uint16_t year, clock_age_seconds;
//	uint8_t prop_id, prop_time_loops_before_start, prop_from;

	out_payload.to_addr = RFM_BROADCAST;
	out_payload.from_addr = my_conf.badge_id;
	out_payload.base_id = 0xFF; // TODO: unless I'm a base
	out_payload.puppy_flags = 0;
	out_payload.clock_authority = 0xFF; // UNSET
//	memcpy(&out_payload.time, &currentTime, sizeof out_payload.time); // I think I don't care about this.
	out_payload.clock_age_seconds = 0;
	out_payload.prop_id = 0;
	out_payload.prop_time_loops_before_start = 0;
	out_payload.prop_from = 0xFF;
	out_payload.beacon = 0;

	// TODO: the opposite of WDT_A_hold(WDT_A_BASE);
	// probably.
}

void update_clock() {

	if (memcmp(&currentTime.Minutes,
				&in_payload.time.Minutes,
				sizeof currentTime - sizeof currentTime.Seconds) ||
		in_payload.time.Seconds < ((currentTime.Seconds-1) % 60) ||
		in_payload.time.Seconds > ((currentTime.Seconds+1) % 60))
	{
		memcpy(&currentTime, &in_payload.time, sizeof (Calendar));
		clock_is_set = 1;
		clock_setting_age = 0;
		my_clock_authority = in_payload.clock_authority;
		out_payload.clock_authority = my_clock_authority;
		init_alarms();
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
