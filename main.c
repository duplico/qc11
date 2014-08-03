//#pragma FUNC_ALWAYS_INLINE (UCS_clearAllOscFlagsWithTimeout);

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
#pragma DATA_SECTION (backup_conf, ".infoB");

const qcxiconf my_conf;
//const qcxiconf backup_conf = {
//		{0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
//		{0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
//		{0xffff, 0xffff, 0xffff, 0xffff},
//		0xff, 0xff,
//		101,
//		"",
//		"",
//		0xffff
//}; // TODO

#if BADGE_TARGET
const qcxiconf backup_conf = {
		{0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
		{0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
		{0xffff, 0xffff, 0xffff, 0xffff},
		0xff, 0xff,
		0x55,
		"George",
		"0xDECAFBAD!",
		0xffff
};
#else
const qcxiconf backup_conf = {
		{0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
		{0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff},
		{0xffff, 0xffff, 0xffff, 0xffff},
		0xff, 0xff,
		0xff,
		"",
		"",
		0xffff
};
#endif

// Interrupt flags to signal the main thread:
volatile uint8_t f_rfm_rx_done = 0;
volatile uint8_t f_rfm_tx_done = 0;
volatile uint8_t f_ir_tx_done = 0;
volatile uint8_t f_ir_rx_ready = 0;
volatile uint8_t f_new_second = 0;
volatile uint8_t f_alarm = 0;

uint8_t f_paired = 0;
uint8_t f_unpaired = 0;
uint8_t f_paired_new_person = 0;
uint8_t f_paired_new_trick = 0;
uint8_t f_animation_done = 0;
uint8_t f_ir_itp_step = 0;
uint8_t f_ir_pair_abort = 0;
uint8_t f_paired_trick = 0;

// Global state:
uint8_t clock_is_set = 0;
uint8_t my_clock_authority = 0;
uint16_t loops_to_rf_beacon = 10 * TIME_LOOP_HZ;
char pairing_message[20] = "";
char event_message[40] = "";
uint8_t my_trick = 0;
uint16_t known_tricks = 0;
uint8_t known_trick_count = 0;
uint8_t known_props = 0;
qcxipayload in_payload;

qcxipayload out_payload = {
		RFM_BROADCAST, 0xff, 0xff, 0xff, 0xff, 0};

uint16_t rainbow_lights = 0;
uint8_t light_blink = 0;

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

// Gaydar:
uint8_t window_position = 0; // Currently only used for restarting radio & skipping windows.
uint8_t neighbor_count = 0;
uint8_t window_seconds = RECEIVE_WINDOW_LENGTH_SECONDS;
uint8_t trick_seconds = TRICK_INTERVAL_SECONDS;
uint8_t target_gaydar_index = 0;
uint8_t gaydar_index = 0;
uint8_t neighbor_badges[BADGES_IN_SYSTEM] = {0};

#if !BADGE_TARGET
volatile uint8_t f_ser_rx = 0;
#endif

char time[6] = "00:00";

// Main thread signals:

uint16_t s_prop_cycles = 0,
		 s_prop_animation_length = 0;

uint8_t s_prop_id = 0,
		s_prop_authority = 0,
		s_propped = 0,
		s_event_arrival = 0,
		s_on_bus = 0,
		s_off_bus = 0,
		s_need_rf_beacon = 0,
		s_rf_retransmit = 0,
		s_pair = 0, // f_pair
		s_new_pair = 0,
		s_new_trick = 0,
		s_new_score = 0,
		s_new_prop = 0,
		s_trick = 0,
		s_prop = 0,
		s_get_puppy = 0,
		s_lose_puppy = 0,
		s_update_rainbow = 0;

uint8_t itps_pattern = 0;

// Scores:
uint8_t my_score = 0;
uint8_t shown_score = 0;
uint8_t s_count_score = 0;
#define COUNT_SCORE_CYCLES 4
uint8_t s_count_score_cycles = 0;


void set_my_score_from_config() {
	my_score = 0;
	// Count the bits set in score:
	uint16_t v = 0;
	for (uint8_t i=0; i<4; i++) {
		v = ~my_conf.scores[i];
		for (;v;my_score++) {
			v &= v - 1;
		}
	}

	s_count_score_cycles = COUNT_SCORE_CYCLES;
	shown_score = 0;
	s_count_score = 1;

	known_props = 0;
	known_props += (my_score>=3);
	known_props += (my_score>=6);
	known_props += (my_score>=11);
	known_props += (my_score>=17);
	known_props += (my_score>=24);
	known_props += (my_score>=31);
}

void set_score(uint8_t id) {
	uint8_t score_frame = id / 16;
	uint8_t score_bit = 1 << (id % 16);
	if (!(~(my_conf.scores[score_frame]) & score_bit)) {
		// haven't seen it, so we need to set its 1 to a 0.
		uint16_t new_config_word = my_conf.scores[score_frame] & ~(score_bit);
		FLASH_unlockInfoA();
		FLASH_write16(&new_config_word, &(my_conf.scores[score_frame]), 1);
		FLASH_lockInfoA();
		s_new_score = 1;
		set_my_score_from_config();
	}
}

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
		FLASH_unlockInfoA();
		FLASH_write16(&new_config_word, &(my_conf.met_ids[badge_frame]), 1);
		FLASH_lockInfoA();
	} // otherwise, nothing to do.
}

uint8_t event_attended(uint8_t id) {
	return ~my_conf.events_attended & (1 << id);
}

void set_event_attended(uint8_t id) {
	// No if event_attended() is needed here because this is only called once,
	//  and that call is wrapped in an if.
	if (light_blink == 128 + id) {
		light_blink = 0;
		s_update_rainbow = 1;
	}
	uint8_t new_event_attended = my_conf.events_attended & ~(1 << id);
	FLASH_unlockInfoA();
	FLASH_write8(&new_event_attended, &my_conf.events_attended, 1);
	FLASH_lockInfoA();
}

void set_event_occurred(uint8_t id) {
	if (~my_conf.events_occurred & (1 << id))
		return;
	uint8_t new_event_occurred = my_conf.events_occurred & ~(1 << id);
	FLASH_unlockInfoA();
	FLASH_write8(&new_event_occurred, &my_conf.events_occurred, 1);
	FLASH_lockInfoA();
}

uint8_t paired_badge(uint8_t id) {
	uint8_t badge_frame = id / 16;
	uint8_t badge_bit = 1 << (id % 16);
	return (~(my_conf.paired_ids[badge_frame]) & badge_bit)? 1: 0;
}

void set_badge_paired(uint8_t id) {
	uint8_t badge_frame = id / 16;
	uint8_t badge_bit = 1 << (id % 16);

	// See if this is a new pair.
	if (id != 0xff && !(~(my_conf.paired_ids[badge_frame]) & badge_bit)) {
		f_paired_new_person = 1;
		// haven't seen it, so we need to set its 1 to a 0.
		uint16_t new_config_word = my_conf.paired_ids[badge_frame] & ~(badge_bit);
		FLASH_unlockInfoA();
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
	if (id != 0xff) {
		f_paired = 1;
	}
	ir_proto_seqnum = 0;
	ir_pair_setstate(IR_PROTO_PAIRED);

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
 * * Event alert raised (interrupt flag)
 * * DONE It's been long enough that we can do a trick (set flag from time loop)
 * ** DONE  (maybe the trick is a prop)
 * * DONE Time to beacon the radio (set flag from time loop)
 * * DONE Time to beacon the IR (set flag from time loop)
 *
 * From the radio:
 * * DONE Receive a beacon (at some point, we need to decide if it means we:)
 * ** adjust neighbor count
 * ** DONE Are near a base station (arrive event)
 * ** DONE should schedule a prop
 * ** DONE set our clock
 *
 * From the IR
 * * DONE Docking with base station - don't REALLY dock...
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

uint8_t skip_window = 1;

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

#if BADGE_TARGET
	// Startup sequence:
	uint8_t startup_seq_index = 0;

	while (startup_seq_index<3) {
		// Time to do something because of time?
		if (f_time_loop) {
			f_time_loop = 0;
			led_timestep();
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
			case 1:
				left_sprite_animate((spriteframe *) anim_sprite_walkin, 4);
				break;
			case 2:
				left_sprite_animate((spriteframe *) anim_sprite_wave, 4);
				break;

			}
		}
	}
	delay(750);
#endif

	// Main sequence:
	f_rfm_rx_done = 0;
	while (1) {

#if !BADGE_TARGET
		// New serial message?
		if (f_ser_rx) {
			ser_print((uint8_t *) ser_buffer_rx);
			f_ser_rx = 0;
		}
#endif

		if (f_rfm_tx_done) {
			f_rfm_tx_done = 0;
			// Back to normal RX automode:
			write_single_register(0x3b, RFM_AUTOMODE_RX);
		}

		/*
		 * Process link-layer IR messages if needed.
		 */
		if (f_ir_rx_ready) {
			f_ir_rx_ready = 0;
			ir_process_rx_ready();
		}

		if (f_ir_pair_abort) {
			f_ir_pair_abort = 0;
#if BADGE_TARGET
			if (itps_pattern) {
				s_count_score_cycles = (my_score > 31) ? 1 : COUNT_SCORE_CYCLES;
				shown_score = 0;
				s_count_score = 1;
			}
			itps_pattern = 0;
#endif
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
			f_rfm_rx_done = 0;
			if (in_payload.clock_authority != 0xff &&
					(!clock_is_set ||
							in_payload.clock_authority < my_clock_authority)) {
				am_idle = 0;
				update_clock();
			}
#if BADGE_TARGET
			if (in_payload.base_id == BUS_BASE_ID && in_payload.from_addr == 0xff) {
				// Bus
				// TODO: Score.
			}
			else if (in_payload.base_id < 7 && in_payload.from_addr == 0xff) {
				// Base station, may need to check in.
				// base_id = event_id, should be 0..7
				if (!event_attended(in_payload.base_id)) {
					set_event_attended(in_payload.base_id);
					s_event_arrival = BIT7 + in_payload.base_id;
				}
			}

			if (in_payload.prop_from != 0xFF && in_payload.prop_from != my_conf.badge_id) {
				// It's a prop notification.
				// If we don't currently have a prop scheduled, or if this prop is
				// more authoritative than our currently scheduled prop, it's time
				// to do a prop.
				// If we're paired, and this is from the person
				// we're paired with, it's the most authoritative prop possible.
				uint8_t prop_authority = in_payload.prop_from;
				if (((!s_propped && !s_prop) || prop_authority < s_prop_authority) && in_payload.prop_time_loops_before_start) {
					s_propped = 1;
					s_prop = 0;
					s_prop_authority = in_payload.prop_from;
					s_prop_cycles = in_payload.prop_time_loops_before_start;
					s_prop_id = in_payload.prop_id;
					out_payload.prop_from = s_prop_authority;
					out_payload.prop_time_loops_before_start = s_prop_cycles;
					out_payload.prop_id = s_prop_id;
				} else if (s_propped) {
					// If we're already propped, and our current prop has higher authority,
					// retransmit it.
					s_rf_retransmit = 1;
				}
			}

			if (in_payload.beacon && in_payload.from_addr != 0xff) {
				// It's a beacon (one per cycle).
				// Increment our beacon count in the current position in our
				// sliding window.
				neighbor_badges[in_payload.from_addr] = RECEIVE_WINDOW;
				set_badge_seen(in_payload.from_addr);
			}
#endif
		}
#if BADGE_TARGET
		static uint8_t event_id = 0;
		/*
		 * Calendar interrupts:
		 *
		 * * Event alert raised (interrupt flag)
		 *
		 */
		if (f_alarm) { // needs to be before f_new_second?
			event_id = f_alarm & 0b0111;
			if (clock_is_set  && !event_attended(event_id)) {
				if (f_alarm & ALARM_START_LIGHT) {
					light_blink = 128 + event_id;
				}
				if (f_alarm & ALARM_STOP_LIGHT) {
					light_blink = 0;
					s_update_rainbow = 1;
				}
				if (f_alarm & ALARM_DISP_MSG) {
					strcpy(event_message, alarm_msg);
					if (!led_display_text) {
						led_print_scroll(event_message, 1);
						am_idle = 0;
					}
				}
			}
			if (!(f_alarm & ALARM_NO_REINIT)) {
				init_alarms();
			}
			f_alarm = 0;
		}
#endif

		if (f_new_second) {
			f_new_second = 0;
#if BADGE_TARGET
			// BIT7 of events_occurred is !defcon_over:
			if (BIT7 & my_conf.events_occurred) {
				if (light_blink) {
					rainbow_lights ^= 1 << (9-(light_blink & 127));
					s_update_rainbow = 1;
				}
				if (!clock_is_set) {
					rainbow_lights ^= BIT9;
					s_update_rainbow = 1;
				}
			}

			if (!s_count_score && !(rand() % 30)) {
				s_count_score_cycles = (my_score > 31) ? 1 : COUNT_SCORE_CYCLES;
				shown_score = 0;
				s_count_score = 1;
				if (BIT7 & ~my_conf.events_occurred && my_conf.handle[0] && !led_display_text) { // defcon is over:
					am_idle = 0;
					led_print_scroll(my_conf.handle, 0);
				}
			}
#endif

			currentTime.Seconds++;
			if (currentTime.Seconds >= 60) {
				currentTime = RTC_A_getCalendarTime(RTC_A_BASE);
				out_payload.time.Hours = currentTime.Hours;
				out_payload.time.Minutes = currentTime.Minutes;
				out_payload.time.DayOfMonth = currentTime.DayOfMonth;
				out_payload.time.DayOfWeek = currentTime.DayOfWeek;
				out_payload.time.Month = currentTime.Month;
				out_payload.time.Year = currentTime.Year;
			}
			out_payload.time.Seconds = currentTime.Seconds;
#if BADGE_TARGET
			if (!trick_seconds) {
				trick_seconds = TRICK_INTERVAL_SECONDS-1 + (rand()%3);
				if (rand() % 3) {
					// wave
					s_trick = TRICK_COUNT+1;
				} else if (!s_prop && !s_propped && neighbor_count && known_props) { // && !(rand() % 4)) { // TODO
					// prop
					// TODO:
					s_prop = 1;
					s_prop_authority = my_conf.badge_id;

					s_prop_id = rand() % known_props; // TODO: for now. later maybe have a prob. dist.

					// We need to convert from generated-prop-id to actual-prop-id. This is because
					//  some of them are full-width and some of them are sprites. Yay!
					// We'll need to SEND the "effect" version of the id, and we'll keep the "use"
					//  version for ourselves.

					// Happily, for effects, 0 and 1 are FULL; and the rest are SPRITES, in order.
					// So we can keep it the same, and we'll just subtract 2 at the other end if
					// it's a full one.
					out_payload.prop_id = s_prop_id;

					// For ourselves, #0 can stay 0 (it's full[0])
					// but #4 will need to become 1 (it's full[1]).
					// therefore, #1 will be 2 (so we can just do full[#-2])
					// #2 will be #4, #4 will be #4, and #5 will be #5
					// Then 1 will need to be 2 (prop 1 is sprite).
					if (s_prop_id == 4) {
						s_prop_id = 1;
					} else if (s_prop_id != 5 && s_prop_id > 0) {
						s_prop_id+=1;
					}

					s_prop_animation_length = 0;

					if (s_prop_id < 2) {
						while(!(prop_uses[s_prop_id][s_prop_animation_length++].lastframe & BIT7));
					} else { // the rest are sprites:
						while(!(prop_uses_sprites[s_prop_id-2][s_prop_animation_length++].lastframe & BIT7));
					}

					s_prop_animation_length *= PROP_FRAMESKIP;

					s_prop_cycles = ANIM_DELAY + s_prop_animation_length;
					out_payload.prop_from = my_conf.badge_id;
					out_payload.prop_time_loops_before_start = s_prop_cycles;
					s_rf_retransmit = 1;
					// Then we're testing whether s_prop_cycles == s_prop_animation_length
				} else {
					// trick
					static uint8_t known_trick_to_do;
					known_trick_to_do = rand() % known_trick_count;
					// start with the first known trick:

					while (!(known_tricks & 1<<s_trick)) {
						s_trick++;
					}

					while (known_trick_to_do) {
						s_trick++;
						if (known_tricks & 1<<s_trick) {
							// if and only if we know the candidate trick, do
							// we decrement known_trick_to_do.
							known_trick_to_do--;
						}
					}
					// doing s_trick...
					s_trick++; // because the s_trick flag is trick_id+1
				}
			} else { // if (!sprite_animate && !led_text_scrolling && !s_propped) {
				trick_seconds--;
			}

			if (s_trick && pair_state == PAIR_IDLE) {
				ir_proto_seqnum = s_trick;
			}
#endif

			window_seconds--;
			if (!window_seconds) {
				window_seconds = RECEIVE_WINDOW_LENGTH_SECONDS;
				if (skip_window != window_position) {
					s_need_rf_beacon = 1;
				}
				neighbor_count = 0;
				for (uint8_t i=0; i<BADGES_IN_SYSTEM; i++) {
					if (neighbor_badges[i]) {
						neighbor_count++;
						neighbor_badges[i]--;
					}
				}

				window_position = (window_position + 1) % RECEIVE_WINDOW;
				if (!window_position) {
					skip_window = rand() % RECEIVE_WINDOW;
				}
				// If we're rolling over the window and have no neighbors,
				// try a radio reboot, in case that can gin up some neighbors
				// for some reason.
				if (!window_position && neighbor_count == 0) {
					init_radio();
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

			if (loops_to_ir_timestep) {
				loops_to_ir_timestep--;
			} else {
				loops_to_ir_timestep = IR_LOOPS;
				ir_process_timestep();
			}

#if BADGE_TARGET
			// number of ITPs to display data for: (ITPS_TO_PAIR-ITPS_TO_SHOW_PAIRING)
			// (ITPS_TO_PAIR-ITPS_TO_SHOW_PAIRING) / 5: ITPS per light

			if (ir_proto_state == IR_PROTO_ITP && ir_proto_seqnum > ITPS_TO_SHOW_PAIRING) {
				itps_pattern = 0;
				for (uint8_t i=0; i <= (ir_proto_seqnum - ITPS_TO_SHOW_PAIRING) / ((ITPS_TO_PAIR - ITPS_TO_SHOW_PAIRING) / 5); i++) {
					itps_pattern |= (1 << i);
				}
				s_update_rainbow = 1;
			} else if (itps_pattern) {
				itps_pattern = 0;
				s_update_rainbow = 1;
			} else if (s_count_score && !s_count_score_cycles) {
				s_count_score_cycles = (my_score > 31) ? 1 : COUNT_SCORE_CYCLES;;
				if (shown_score == my_score || shown_score == 31)
					s_count_score = 0;
				else {
					shown_score++;
					s_update_rainbow = 1;
				}
			} else if (s_count_score) {
				s_count_score_cycles--;
			}


			if (s_prop_cycles) {
				s_prop_cycles--;
				out_payload.prop_time_loops_before_start = s_prop_cycles;
			}
#endif
		}

		// This is background:
		if (s_need_rf_beacon && rfm_reg_state == RFM_REG_IDLE  && !(read_single_register_sync(0x27) & (BIT1+BIT0))) {
#if BADGE_TARGET
			out_payload.beacon = 1;
#else
			out_payload.beacon = 0;
			out_payload.base_id = 3;
#endif
			if (!clock_is_set)
				out_payload.clock_authority = 0xff;
#if !BADGE_TARGET
			else
				out_payload.clock_authority = 0;
#endif
			radio_send_sync();
			s_need_rf_beacon = 0;
		} else if (s_rf_retransmit && rfm_reg_state == RFM_REG_IDLE) {
			out_payload.beacon = 0;

			if (!clock_is_set)
				out_payload.clock_authority = 0xff;
#if !BADGE_TARGET
			else
				out_payload.clock_authority = 0;
#endif
			radio_send_sync();
			s_rf_retransmit = 0;
		}

		// Is an animation finished?
		if (f_animation_done) {
			f_animation_done = 0;
			led_display_left |= BIT0;
			am_idle = 1;
			#if !BADGE_TARGET
			color = 0;
			#endif
		}

#if BADGE_TARGET

		// Pre-emptive:

		if (am_idle) { // Can do another action now.
			switch(badge_status) {
			case BSTAT_GAYDAR:
				if (s_prop && s_prop_cycles <= s_prop_animation_length) {
					// Do a prop use:
					am_idle = 0;
					s_prop = 0;
					out_payload.prop_from = 0xff;
					out_payload.prop_time_loops_before_start = 0;
					 // 0,1 uses are full; the rest are sprites:
					if (s_prop_id <= 1) {
						led_display_left &= ~BIT0;
						full_animate(prop_uses[s_prop_id], PROP_FRAMESKIP);
					} else {
						left_sprite_animate(prop_uses_sprites[s_prop_id-2], PROP_FRAMESKIP);
					}
					s_trick = 0;
				} else if (s_propped && !s_prop_cycles) {
					// Do a prop effect:
					am_idle = 0;
					s_propped = 0;
					out_payload.prop_from = 0xff;
					out_payload.prop_time_loops_before_start = 0;

					if (s_prop_id <= 1) { // 0, 1 are full:
						if (s_prop_id != 1)
							led_display_left &= ~BIT0;
						full_animate(prop_effects[s_prop_id], PROP_FRAMESKIP);
					} else { // the rest are full:
						left_sprite_animate(prop_effects_sprites[s_prop_id-2], PROP_FRAMESKIP);
					}
					s_trick = 0;

				} else if (f_paired) { // TODO: This might should be pre-emptive?
					f_paired = 0;
					s_prop = 0;
					s_propped = 0;
					s_prop_cycles = 0;
					out_payload.prop_from = 0xff;
					out_payload.prop_time_loops_before_start = 0;
					pair_state = PAIR_INIT;
					itps_pattern = 0;
					s_update_rainbow = 1;
					badge_status = BSTAT_PAIR;
					am_idle = 0;
					gaydar_index = 0;
					right_sprite_animate(anim_sprite_walkin, 2, 1, 1, 1);
				} else if (target_gaydar_index > gaydar_index) {
					am_idle = 0;
					right_sprite_animate(gaydar[gaydar_index], 4, 0, 1, 1);
					left_sprite_animate(anim_sprite_wave, 4);
					gaydar_index++;
				} else if (target_gaydar_index < gaydar_index) {
					am_idle = 0;
					gaydar_index--;
					right_sprite_animate(gaydar[gaydar_index], 4, 0, -1, gaydar_index!=0);
					left_sprite_animate(anim_sprite_wave, 4);
				} else if (s_event_arrival) {
					// TODO: score.
					s_event_arrival = 0;
				}
				break;
			case BSTAT_PAIR:
				if (f_unpaired) {
					f_unpaired = 0;
					itps_pattern = 0;
					s_update_rainbow = 1;
					badge_status = BSTAT_GAYDAR;
					am_idle = 0;
					right_sprite_animate(anim_sprite_walkin, 2, 1, -1, 0);
					left_sprite_animate(anim_sprite_wave, 2);
					break;
				}
				switch(pair_state) {
				case PAIR_INIT: // Pat just walked on
					am_idle = 0;
					pair_state = PAIR_WAVE;
					right_sprite_animate(anim_sprite_wave, 5, 1, 1, 1);
					left_sprite_animate(anim_sprite_wave, 5);
					break;
				case PAIR_WAVE:
					am_idle = 0;
					memset(pairing_message, 0, 20);
					strcat(pairing_message, "Hi ");
					strcat(pairing_message, ir_rx_handle);
					led_print_scroll(pairing_message, 1);
					pair_state = PAIR_GREETING;
					break;
				case PAIR_GREETING:
					if (!ir_rx_message[0]) {
						pair_state = PAIR_IDLE;
					} else {
						am_idle = 0;
						led_print_scroll(ir_rx_message, 1);
						pair_state = PAIR_MESSAGE;
						break;
					}
					break;
				case PAIR_MESSAGE:
					pair_state = PAIR_IDLE;
					break;
				}
			} // end switch(badge_status)
		}

		// Any status, if we have no more idle-status-change processing to do:
		if (am_idle) {
			if (s_new_score) {
				am_idle = 0;
				led_print_scroll("Score!", 1); // TODO: if time, add custom messages.
				s_new_score = 0;
				s_update_rainbow = 1;
			}
			if (s_trick) {
				uint16_t trick_len = 0;
				while (!(tricks[s_trick-1][trick_len++].lastframe & BIT7));
				trick_len *= 4; // TODO.
				if (!(s_propped || s_prop) ||
						(s_propped && trick_len+TIME_LOOP_HZ < s_prop_cycles) ||
						(trick_len+TIME_LOOP_HZ < s_prop_cycles - s_prop_animation_length))
				{
					am_idle = 0;
					left_sprite_animate((spriteframe *)tricks[s_trick-1], 4);
					s_trick = 0; // this needs to be after the above statement. Duh.
				}
			}
		}

		if (pair_state == PAIR_IDLE && f_paired_trick) {
			right_sprite_animate((spriteframe *)tricks[f_paired_trick-1], 4, 1, 1, 0xff);
			f_paired_trick = 0;
		}

		// Background:
		if (s_update_rainbow) {
			s_update_rainbow = 0;
			rainbow_lights &= 0b1111111111100000;
			if (clock_is_set)
				rainbow_lights &= ~BIT9;
			if (itps_pattern) {
				rainbow_lights |= itps_pattern;
			} else if (s_count_score) {
				rainbow_lights |= (shown_score & 0b11111);
			} else {
				if (my_score >= 31) {
					rainbow_lights |= 0b11111;
				} else {
					rainbow_lights |= (my_score & 0b11111);
				}
			}

			if (!light_blink) {
				rainbow_lights &= 0b1111111000011111;
				// set according to events attended...
				uint8_t reverse_events = ((my_conf.events_attended * 0x0802LU & 0x22110LU) | (my_conf.events_attended * 0x8020LU & 0x88440LU)) * 0x10101LU >> 16;
				rainbow_lights |= ((uint16_t) ~reverse_events & 0b11111000) << 2;
			}

			led_set_rainbow(rainbow_lights);
		}
#endif

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

uint16_t config_crc(qcxiconf conf) {
	CRC_setSeed(CRC_BASE, 0xBEEF);
	CRC_set8BitData(CRC_BASE, conf.badge_id);
	for (uint8_t i=0; i<sizeof(conf.handle); i++) {
		CRC_set8BitData(CRC_BASE, conf.handle[i]);
	}

	for (uint8_t i=0; i<sizeof(conf.message); i++) {
		CRC_set8BitData(CRC_BASE, conf.message[i]);
	}
	return CRC_getResult(CRC_BASE);
}

void check_config() {
	WDT_A_hold(WDT_A_BASE);

	uint16_t crc = config_crc(my_conf);

	if (crc != my_conf.crc) { // TODO!!!!!!!!!!!!!1
		// this means we need to load the backup conf:
		// we ignore the CRC of the backup conf.
		uint8_t* new_config_bytes = (uint8_t *) &backup_conf;

		FLASH_unlockInfoA();
		uint8_t flash_status = 0;
		do {
			FLASH_segmentErase((uint8_t *)INFOA_START);
			flash_status = FLASH_eraseCheck((uint8_t *)INFOA_START, 128);
		} while (flash_status == STATUS_FAIL);

		FLASH_write8(new_config_bytes, (uint8_t *)INFOA_START, sizeof(qcxiconf) - sizeof my_conf.crc);

		crc = config_crc(backup_conf);
		FLASH_write16(&crc, &my_conf.crc, 1);

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
	set_my_score_from_config();
	s_new_score = 0;

	// Setup our IR pairing payload:
	strcpy(&(ir_pair_payload[0]), my_conf.handle);
	strcpy(&(ir_pair_payload[11]), my_conf.message);
	out_payload.from_addr = my_conf.badge_id;

}

void update_clock() {
	if (!clock_is_set ||
		memcmp(&currentTime.Minutes,
				&in_payload.time.Minutes,
				sizeof currentTime - sizeof currentTime.Seconds) ||
		in_payload.time.Seconds < ((currentTime.Seconds-1) % 60) ||
		in_payload.time.Seconds > ((currentTime.Seconds+1) % 60))
	{
		memcpy(&currentTime, &in_payload.time, sizeof (Calendar));
		clock_is_set = 1;
		my_clock_authority = in_payload.clock_authority;
		out_payload.clock_authority = 1;
		if (!out_payload.clock_authority)
			out_payload.clock_authority = 1;
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
