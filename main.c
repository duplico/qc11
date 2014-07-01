#include <string.h>

#include "qcxi.h"
#include "radio.h"
#include "fonts.h"
#include "clocks.h"
#include "leds.h"
#include "ir.h"

// Interrupt flags to signal the main thread:
volatile uint8_t f_new_minute = 0;
volatile uint8_t f_timer = 0;
volatile uint8_t f_rfm_job_done = 0;
volatile uint8_t f_ir_tx_done = 0;
volatile uint8_t f_ir_rx_ready = 0;
volatile uint8_t f_rfm_reg_finished = 0;

void init_power() {
	// Set Vcore to 1.8 V - NB: allows MCLK up to 8 MHz only
	PMM_setVCore(PMM_CORE_LEVEL_0);
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

	// IR pins ////////////////////////////////////////////////////////////////
	//
	// P4.4, 4.5, 4.6
	//
	// TX for IR
	GPIO_setAsPeripheralModuleFunctionOutputPin(
			GPIO_PORT_P4,
			GPIO_PIN4
	);
	// RX for IR
	GPIO_setAsPeripheralModuleFunctionInputPin(
			GPIO_PORT_P4,
			GPIO_PIN5
	);

	// Shutdown (SD) for IR
	GPIO_setAsOutputPin(GPIO_PORT_P4, GPIO_PIN6);
	GPIO_setOutputLowOnPin(GPIO_PORT_P4, GPIO_PIN6); // shutdown low = on

	// Interrupt pin for radio:
	GPIO_setAsInputPin(GPIO_PORT_P2, GPIO_PIN0);
	GPIO_enableInterrupt(GPIO_PORT_P2, GPIO_PIN0);
	GPIO_interruptEdgeSelect(GPIO_PORT_P2, GPIO_PIN0, GPIO_LOW_TO_HIGH_TRANSITION);
	GPIO_clearInterruptFlag(GPIO_PORT_P2, GPIO_PIN0);
}

uint8_t reg_read = 0;
uint8_t reg_reads[2] = {0, 0};

uint8_t reg_data[65] = {0};
uint8_t test_data[65] = {0};

char time[6] = "00:00";

uint8_t receive_status;

uint8_t packet_sent = 0;

uint16_t _rotl(uint16_t value, int shift) {
    if ((shift &= sizeof(value)*8 - 1) == 0)
      return value;
    return (value << shift) | (value >> (sizeof(value)*8 - shift));
}

#define POST_XT1F 	0b1
#define POST_XT2F 	0b10
#define POST_SHIFTF 0b100
#define POST_IRIF 	0b1000
#define POST_IRVF 	0b10000
#define POST_RRF	0b100000
#define POST_RTF	0b1000000

uint8_t post() {
	__bic_SR_register(GIE);
	uint8_t post_result = 0;
	uint16_t tp0[5] = {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF};
	uint16_t tp1[5] = {0xFF00, 0xFF00, 0xFF00, 0xFF00, 0xFF00};
	uint16_t tp2[5] = {0x00FF, 0x00FF, 0x00FF, 0x00FF, 0x00FF};
	// Clocks
	if (xt1_status == STATUS_FAIL) {
		post_result |= POST_XT1F;
	}
	if (xt2_status == STATUS_FAIL) {
		post_result |= POST_XT2F;
	}
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
	__bis_SR_register(GIE);

	ir_reject_loopback = 0;
	// IR loopback
	char test_str[] = "qcxi";
	ir_write((uint8_t *) test_str, 0);
	delay(100);
	if (f_ir_rx_ready) {
		f_ir_rx_ready = 0;
		if (!ir_check_crc()) {
			// IR loopback integrity fault
			post_result |= POST_IRIF;
		} else {
			if (strcmp(test_str, (char *) ir_rx_frame) != 0)
				post_result |= POST_IRVF; // IR value fault
		}
	}
	// Radio - TODO

	// Display error code:
	if (post_result != 0) {
		char hex[4] = "AA";
		hex[0] = (post_result/16 < 10)? '0' + post_result/16 : 'A' - 10 + post_result/16;
		hex[1] = (post_result%16 < 10)? '0' + post_result%16 : 'A' - 10 + post_result%16;
		led_print(hex);
		for (uint8_t i=LED_PERIOD; i>0; i--) {
			led_enable(i);
			delay(25);
		}
	}

	return post_result;
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
	init_serial();
	__bis_SR_register(GIE);
	rfm_init(); // requires interrupts enabled.

	post();

	led_on();
	uint8_t startup_test = 0;

	led_anim_init(); // start a-blinkin.
	f_animation_done = 1;

	char hex[4] = "AA";
	uint8_t val;

	uint8_t accum = 0;

	rfm_mode_sync(RFM_MODE_RX);
	while (1) {
		accum++;
		rfm_process_async();

		if (accum==16) {
//			rfm_send_sync("Queercon 2014 qcxi", 64);
//			rfm_mode_sync(RFM_MODE_RX);
			rfm_send_async("Queercon 2014 qcxi", 64, RFM_MODE_RX);
//			led_print_scroll("xmit", 0, 1, 1);
			accum = 0;
		}
		if (f_rfm_job_done) { // received something
			f_rfm_job_done = 0;
			rfm_copy_register_sync(RFM_FIFO, 64, test_data);
			led_print_scroll((char *) test_data, 0, 1, 1);
		}
		if (f_animate) {
			f_animate = 0;
			led_animate();
		}

		if (f_animation_done) {
			f_animation_done = 0;
//			led_print_scroll("Startup", (startup_test & 0b10) >> 1, startup_test & 0b01, 1);
//			startup_test++;
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


#if DEBUG_SERIAL
void init_usb() {

	// UART Serial to PC //////////////////////////////////////////////////////
	//
	// Initialize the UART serial, used to speak over USB.
	// NB: This clobbers the IR interface.
	USCI_A_UART_disable(USCI_A1_BASE);

	USCI_A_UART_initAdvance(
			USCI_A1_BASE,
			USCI_A_UART_CLOCKSOURCE_ACLK,
			3,
			0,
			3,
			USCI_A_UART_NO_PARITY,
			USCI_A_UART_LSB_FIRST,
			USCI_A_UART_ONE_STOP_BIT,
			USCI_A_UART_MODE,
			USCI_A_UART_LOW_FREQUENCY_BAUDRATE_GENERATION
	);
}
#endif


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

