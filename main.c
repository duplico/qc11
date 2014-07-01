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
#if BADGE_TARGET
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
//	ir_reject_loopback = 1;
	// Radio - TODO
#if BADGE_TARGET
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
#endif

	return post_result;
}








#include <msp430f5529.h>
#include "ws2812.h"
#include "driverlib.h"

#define NUMBEROFLEDS	16
#define ENCODING 		3		// possible values 3 and 4

void sendBuffer(uint8_t* buffer, ledcount_t ledCount);
void sendBuffer(uint8_t* buffer, ledcount_t ledCount);
void shiftLed(ledcolor_t* leds, ledcount_t ledCount);


void shiftLed(ledcolor_t* leds, ledcount_t ledCount) {
	ledcolor_t tmpLed;
	ledcount_t ledIdx;

	tmpLed = leds[ledCount-1];
	for(ledIdx=(ledCount-1); ledIdx > 0; ledIdx--) {
		leds[ledIdx] = leds[ledIdx-1];
	}
	leds[0] = tmpLed;
}

// copy bytes from the buffer to SPI transmit register
// should be reworked to use DMA
void sendBuffer(uint8_t* buffer, ledcount_t ledCount) {
	uint16_t bufferIdx;
	__bic_SR_register(GIE); // TODO: need to make this interrupt based:
	for (bufferIdx=0; bufferIdx < (ENCODING * sizeof(ledcolor_t) * ledCount); bufferIdx++) {
		while (!(UCB0IFG & UCTXIFG));		// wait for TX buffer to be ready
		UCB0TXBUF = buffer[bufferIdx];
	}
	__bis_SR_register(GIE);
	__delay_cycles(300);
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
	init_radio(); // requires interrupts enabled.

	post();







	// buffer to store encoded transport data
	uint8_t frameBuffer[(ENCODING * sizeof(ledcolor_t) * NUMBEROFLEDS)] = { 0, };

	ledcolor_t leds[NUMBEROFLEDS] = {
			// rainbow colors
			{ 0x80, 0xf3, 0x1f },
			{ 0xa5, 0xde, 0xb },
			{ 0xc7, 0xc1, 0x1 },
			{ 0xe3, 0x9e, 0x3 },
			{ 0xf6, 0x78, 0xf },
			{ 0xfe, 0x53, 0x26 },
			{ 0xfb, 0x32, 0x44 },
			{ 0xed, 0x18, 0x68 },
			{ 0xd5, 0x7, 0x8e },
			{ 0xb6, 0x1, 0xb3 },
			{ 0x91, 0x6, 0xd3 },
			{ 0x6b, 0x16, 0xec },
			{ 0x47, 0x2f, 0xfa },
			{ 0x28, 0x50, 0xfe },
			{ 0x11, 0x75, 0xf7 },
			{ 0x3, 0x9b, 0xe5 },
	};

	uint8_t update;
	ledcolor_t blankLed = {0x00, 0x00, 0x00};
	uint8_t colorIdx;
	ledcolor_t led;



	USCI_B_SPI_masterInit(
			USCI_B0_BASE,
			USCI_B_SPI_CLOCKSOURCE_SMCLK,
			8000000,
			2666666,
			USCI_B_SPI_MSB_FIRST,
			USCI_B_SPI_PHASE_DATA_CHANGED_ONFIRST_CAPTURED_ON_NEXT,
			USCI_B_SPI_CLOCKPOLARITY_INACTIVITY_LOW
	);

	USCI_B_SPI_enable(USCI_B0_BASE);

	GPIO_setAsPeripheralModuleFunctionOutputPin(GPIO_PORT_P3, GPIO_PIN0);

	while(1) {
		// blank all LEDs
		fillFrameBufferSingleColor(&blankLed, NUMBEROFLEDS, frameBuffer, ENCODING);
		sendBuffer(frameBuffer, NUMBEROFLEDS);
		__delay_cycles(0x100000);

		// Animation - Part1
		// set one LED after an other (one more with each round) with the colors from the LEDs array
		fillFrameBuffer(leds, NUMBEROFLEDS, frameBuffer, ENCODING);
		for(update=1; update <= NUMBEROFLEDS; update++) {
			sendBuffer(frameBuffer, update);
			__delay_cycles(0xFFFFF);
		}
		__delay_cycles(0xFFFFFF);

		// Animation - Part2
		// shift previous LED pattern
		for(update=0; update < 15*8; update++) {
			shiftLed(leds, NUMBEROFLEDS);
			fillFrameBuffer(leds, NUMBEROFLEDS, frameBuffer, ENCODING);
			sendBuffer(frameBuffer, NUMBEROFLEDS);
			__delay_cycles(0x7FFFF);
		}
	}
	return 0;












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
		mode_sb_sync();
		led_print(" TX");
		write_single_register(0x25, 0b00000000); // GPIO map to default
		write_register(RFM_FIFO, test_data, 64);
		led_print("TX");
		f_rfm_job_done = 0;
		mode_tx_async();
		while (!f_rfm_job_done);
		f_rfm_job_done = 0;
		mode_sb_sync();
		//		write_single_register(0x29, 228); // RssiThreshold = -this/2 in dB
		led_print("...");
		delay(100);
		mode_rx_sync();
		delay(1000);
		if (f_rfm_job_done) {
			f_rfm_job_done = 0;
			val = read_single_register_sync(0x24);
			read_register_sync(RFM_FIFO, 64, test_data);
			mode_sb_sync();
			hex[0] = (val/16 < 10)? '0' + val/16 : 'A' - 10 + val/16;
			hex[1] = (val%16 < 10)? '0' + val%16 : 'A' - 10 + val%16;
			led_print((char *)test_data);
			delay(1000);
		}
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

#pragma vector=PORT2_VECTOR
__interrupt void radio_interrupt_0(void)
{
	f_rfm_job_done = 1;
	GPIO_clearInterruptFlag(GPIO_PORT_P2, GPIO_PIN0);
}
