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
		s_need_rf_beacon = 0,
		s_rf_retransmit = 0,
		s_pair = 0, // f_pair
		s_new_pair = 0,
		s_new_trick = 0,
		s_new_score = 0,
		s_new_prop = 0,
		s_trick = 0,
		s_prop = 0,
		s_update_rainbow = 0;

uint8_t itps_pattern = 0;

// Scores:
uint8_t my_score = 0;
uint8_t shown_score = 0;
uint8_t s_count_score = 0;
#define COUNT_SCORE_CYCLES 4
uint8_t s_count_score_cycles = 0;

void set_known_tricks() {
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
}

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

void set_score(uint8_t id, uint8_t value) {
	uint8_t score_frame = id / 16;
	uint16_t score_bits = 0;
	while (value--) {
		score_bits |= 1 << (id+value % 16);
	}
	if (!(~(my_conf.scores[score_frame]) & score_bits)) {
		// haven't seen it, so we need to set its 1 to a 0.
		uint16_t new_config_word = my_conf.scores[score_frame] & ~(score_bits);
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

		if (id < 12) {
			set_score(id, 1); // seen each uber
			// all zeroes means all seen:
			if (!(~my_conf.scores[0] & 0b111111111111)) {
				set_score(12, 3); // seen all ubers
			}
		}
		set_score(16, 1); // first pair

		f_paired_new_person = 1;
		// haven't seen it, so we need to set its 1 to a 0.
		uint16_t new_config_word = my_conf.paired_ids[badge_frame] & ~(badge_bit);
		FLASH_unlockInfoA();
		FLASH_write16(&new_config_word, &(my_conf.paired_ids[badge_frame]), 1);
		FLASH_lockInfoA();

		// TODO: if space: f_paired_new_trick
		set_known_tricks();

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
uint8_t seconds_to_tx = 4;
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
	led_enable(LED_PERIOD/16);
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
	am_idle = 1;
	while (1) {

#if !BADGE_TARGET
		// New serial message?
		if (f_ser_rx) {
			ser_print((uint8_t *) ser_buffer_rx);
			f_ser_rx = 0;
		}
#endif

		if (f_new_second) {
			f_new_second = 0;
			if (!seconds_to_tx--) {
				s_need_rf_beacon = 1;
				seconds_to_tx = 4;
			}
		}

		if (f_rfm_tx_done) {
			f_rfm_tx_done = 0;
			// Back to normal RX automode:
			write_single_register(0x3b, RFM_AUTOMODE_RX);
		}

		if (f_rfm_rx_done) {
			am_idle = 0;
			led_print_scroll("rx", 0);
			f_rfm_rx_done = 0;
		}

		if (f_time_loop) {
			f_time_loop = 0;
#if BADGE_TARGET
			if (!am_idle)
				led_timestep();
			else {
				memset(disp_buffer, 0xff, sizeof disp_buffer);
				led_set_rainbow(0xffff);
				led_update_display();
			}
#else
			fillFrameBufferSingleColor(&leds[color], NUMBEROFLEDS, ws_frameBuffer, ENCODING);
			ws_set_colors_async(NUMBEROFLEDS);
			color++;
			if (color==21) f_animation_done = 1;
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
			am_idle = 0;
			led_print_scroll("tx", 0);
			s_need_rf_beacon = 0;
		}


		// Is an animation finished?
		if (f_animation_done) {
			f_animation_done = 0;
			led_update_display();
//			led_display_left |= BIT0;
			am_idle = 1;
			#if !BADGE_TARGET
			color = 0;
			#endif
		}

		// Going to sleep... mode...
//		__bis_SR_register(LPM3_bits + GIE);
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
	set_known_tricks();

	set_my_score_from_config();
	s_new_score = 0;

	// Setup our IR pairing payload:
	memcpy(&(ir_pair_payload[0]), my_conf.handle, 11);
	memcpy(&(ir_pair_payload[11]), my_conf.message, 17);
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
		set_score(48, 1); // clock setting
	}
}

void delay(uint16_t ms)
{
	while (ms--)
    {
        __delay_cycles(MCLK_DESIRED_FREQUENCY_IN_KHZ);
    }
}
