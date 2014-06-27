#include "qcxi.h"
#include "radio.h"
#include "fonts.h"
#include "clocks.h"
#include "leds.h"

// Interrupt flags to signal the main thread:
volatile uint8_t f_new_minute = 0;
volatile uint8_t f_timer = 0;
volatile uint8_t f_tx_done = 0;
volatile uint8_t f_rx_ready = 0;
volatile uint8_t f_rfm_job_done = 0;

uint8_t received_data = 0;
uint8_t test_data[65] = {0};

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

	//   PWM-enabled BLANK pin:
//	GPIO_setAsPeripheralModuleFunctionOutputPin(LED_PORT, LED_BLANK);
	GPIO_setAsOutputPin(LED_PORT, LED_BLANK);
	// TODO: Also, there's an input FROM the LED controllers on pin 1.6
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

	// ALTERNATE FOR IR: serial debug interface ///////////////////////////////
	// serial debug:
//	GPIO_setAsPeripheralModuleFunctionInputPin(GPIO_PORT_P4, GPIO_PIN5); // 4.5: RXD
//	GPIO_setAsPeripheralModuleFunctionOutputPin(GPIO_PORT_P4, GPIO_PIN4); // 4.4: TXD
	///////////////////////////////////////////////////////////////////////////

	// Interrupt pin for radio:
	GPIO_setAsInputPin(GPIO_PORT_P2, GPIO_PIN0);
	GPIO_enableInterrupt(GPIO_PORT_P2, GPIO_PIN0);
	GPIO_interruptEdgeSelect(GPIO_PORT_P2, GPIO_PIN0, GPIO_LOW_TO_HIGH_TRANSITION);
	GPIO_clearInterruptFlag(GPIO_PORT_P2, GPIO_PIN0);

	for (int i=0; i<64; i++) { // TODO: Here's a test.
		test_data[i] = (uint8_t)'Q';
	}
}

void init_serial() {

#if DEBUG_SERIAL

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

#else


	// IR Interface ///////////////////////////////////////////////////////////
	//
//	USCI_A_UART_initAdvance(
//			USCI_A1_BASE,
//			USCI_A_UART_CLOCKSOURCE_SMCLK, // Use the 8 MHz clock
//			8,
//			0,
//			11, // 57600 bps
//			USCI_A_UART_EVEN_PARITY,
//			USCI_A_UART_MSB_FIRST,
//			USCI_A_UART_ONE_STOP_BIT,
//			USCI_A_UART_MODE, // UART mode
//			USCI_A_UART_OVERSAMPLING_BAUDRATE_GENERATION // UCOS = 1, oversample.
//	);
//
//	UCA1CTL0 = 0x0;
//	UCA1CTL1 = UCSSEL1;
//
//	UCA1IRTCTL |= UCIREN; // IR Enable
//	UCA1IRTCTL |= UCIRTXCLK; // TXCLK = 1, meaning
//	UCA1IRTCTL |= UCIRTXPL2 + UCIRTXPL0; // UCIRTXPL = 5, so this is 5 << 2 TODO.
//	UCA1IRRCTL |= UCIRRXFL1; // Filter length: 1 (see family guide p901)
//	UCA1IRRCTL |= UCIRRXPL; // Active low.
//	UCA1IRRCTL |= UCIRRXFE; // Use filter


	// msb first, no parity, one stop, 8 bit
//	UCA1CTL0 = 0x0;
//	UCA1CTL1 = UCSSEL1;
	// Set 9600 baud rate
	// UCBRx  = 833
	// UCBRSx = 2
	// UCBRFx = 0
	// SMCLK  = 8 MHz


//	UCA1BR0   = 0x41;
//	UCA1BR1   = 0x03;
//	UCA1MCTL  = 0x04;
//	UCA1IRTCTL = UCIREN + UCIRTXPL5;



//#endif
//
//	USCI_A_UART_enable(USCI_A1_BASE);
//
//	USCI_A_UART_enableInterrupt(
//			USCI_A1_BASE,
//			USCI_A_UART_RECEIVE_INTERRUPT
//	);


	// IR Interface ///////////////////////////////////////////////////////////
	//
	USCI_A_UART_initAdvance(
			USCI_A1_BASE,
			USCI_A_UART_CLOCKSOURCE_SMCLK,
			833,
			0,
			2,
			USCI_A_UART_NO_PARITY,
			USCI_A_UART_MSB_FIRST,
			USCI_A_UART_ONE_STOP_BIT,
			USCI_A_UART_MODE,
			USCI_A_UART_LOW_FREQUENCY_BAUDRATE_GENERATION
	);

	UCA1IRTCTL = UCIREN + UCIRTXPL2 + UCIRTXPL0;
#endif

	USCI_A_UART_enable(USCI_A1_BASE);

	USCI_A_UART_clearInterruptFlag(USCI_A1_BASE, USCI_A_UART_RECEIVE_INTERRUPT_FLAG);
	USCI_A_UART_enableInterrupt(
			USCI_A1_BASE,
			USCI_A_UART_RECEIVE_INTERRUPT
	);

}

void write_serial(uint8_t* text) {
	uint16_t sendchar = 0;
	do {
		while (!USCI_A_UART_getInterruptStatus(USCI_A1_BASE, UCTXIFG));
		USCI_A_UART_transmitData(USCI_A1_BASE, text[sendchar]);
//		if (f_rx_ready) while(!f_rx_ready);
//		f_rx_ready = 0;
	} while (text[++sendchar]);
}

uint8_t reg_read = 0;
uint8_t reg_reads[2] = {0, 0};

uint8_t reg_data[65] = {0};

char time[6] = "00:00";

uint8_t receive_status;

uint8_t packet_sent = 0;

uint16_t _rotl(uint16_t value, int shift) {
    if ((shift &= sizeof(value)*8 - 1) == 0)
      return value;
    return (value << shift) | (value >> (sizeof(value)*8 - shift));
}

int main( void )
{
	// TODO: check to see what powerup mode we're in.
	init_watchdog();
	init_power();
	init_gpio();
//	led_post(); // TODO: Let's try to incorporate this once we have
				//       hardware that supports it.
	init_clocks();
	init_timers();
	init_rtc();
	init_serial();
	__bis_SR_register(GIE); // enable interrupts
	init_radio(); // requires interrupts enabled.


	// TODO
	uint16_t i = 0b0000000000011111;
	uint16_t buffer_offset = 0;

	print("Startup");
	led_on();


	char hex[4] = "AA";
	uint8_t val;
	while (1) {
		if (f_rx_ready) {
			f_rx_ready = 0;
			val = received_data;
			hex[0] = (val/16 < 10)? '0' + val/16 : 'A' - 10 + val/16;
			hex[1] = (val%16 < 10)? '0' + val%16 : 'A' - 10 + val%16;
			print(hex);
			delay(100);
		}
		write_serial("QQQ");
		delay(1000);
	}



//	while (1) {
//		mode_sb_sync();
//		led_disp_bit_to_values(0, 0);
//		led_display_bits(values);
//		print(" TX");
//		led_disp_bit_to_values(0, 0);
//		led_display_bits(values);
//		write_single_register(0x25, 0b00000000); // GPIO map to default
//		write_register(RFM_FIFO, test_data, 64);
//		print("TX");
//		f_rfm_job_done = 0;
//		mode_tx_async();
//		while (!f_rfm_job_done);
//		f_rfm_job_done = 0;
//		mode_sb_sync();
//		//		write_single_register(0x29, 228); // RssiThreshold = -this/2 in dB
//		write_single_register(0x25, 0b00000000); // GPIO map
//		print("...");
////		delay(100);
////		mode_rx_sync();
//		delay(1000);
////		if (f_rfm_job_done) {
////			f_rfm_job_done = 0;
////			val = read_single_register_sync(0x24);
////			read_register_sync(RFM_FIFO, 64, test_data);
////			mode_sb_sync();
////			//			print("RX!");
////			hex[0] = (val/16 < 10)? '0' + val/16 : 'A' - 10 + val/16;
////			hex[1] = (val%16 < 10)? '0' + val%16 : 'A' - 10 + val%16;
////			print((char *)test_data);
////			led_disp_bit_to_values(0, 0);
////			led_display_bits(values);
////			led_disp_bit_to_values(0, 0);
////			led_display_bits(values);
////			delay(1000);
////		}
//	}


//	delay(2000);

//	while (1) {
//		mode_rx_sync();
//		print("RX mode");
//		led_disp_bit_to_values(0, 0);
//		led_display_bits(values);
//
//		if (f_new_minute) {
//			currentTime = RTC_A_getCalendarTime(RTC_A_BASE);
//			time[0] = '0' + ((currentTime.Hours & 0b11110000) >> 4);
//			time[1] = '0' + (currentTime.Hours & 0b1111);
//			time[3] = '0' + ((currentTime.Minutes & 0b11110000) >> 4);
//			time[4] = '0' + (currentTime.Minutes & 0b1111);
//			print(time);
//			f_new_minute = 0;
//		}
//		for (int i=0; i<BACK_BUFFER_WIDTH; i++) {
//			led_disp_bit_to_values(i, 0);
//			led_display_bits(values);
//			delay(100);
//		}
//		// TODO: "scroll" flag w/ configgable ISR
//	}
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
		status = UCS_clearAllOscFlagsWithTimeout(1000);
	} while (status != 0);
}

// Echo back RXed character, confirm TX buffer is ready first
#pragma vector=USCI_A1_VECTOR
__interrupt void USCI_A1_ISR(void)
{
	switch(__even_in_range(UCA1IV,4))
	{
	case 0:break;                             // Vector 0 - no interrupt
	case 2:                                   // Vector 2 - RXIFG
		while (!USCI_A_UART_getInterruptStatus(USCI_A1_BASE, UCTXIFG));
		received_data = USCI_A_UART_receiveData(USCI_A1_BASE);
		f_rx_ready = 1;

		break;
	case 4:                             // Vector 4 - TXIFG // Ready for another character...
		break;
	default: break;
	}
}

#pragma vector=PORT2_VECTOR
__interrupt void radio_interrupt_0(void)
{
	f_rfm_job_done = 1;
	GPIO_clearInterruptFlag(GPIO_PORT_P2, GPIO_PIN0);
}
