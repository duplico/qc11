#include "main.h"
#include "driverlib.h"
#include "radio.h"

#define WRITE_IF(port, pin, val) if (val) GPIO_setOutputHighOnPin(port, pin); else GPIO_setOutputLowOnPin(port, pin);
#define GPIO_pulse(port, pin) do { GPIO_setOutputHighOnPin(port, pin); GPIO_setOutputLowOnPin(port, pin); } while (0);

// For clock initialization:
#define UCS_XT1_TIMEOUT 50000
#define UCS_XT2_TIMEOUT 50000
#define UCS_XT1_CRYSTAL_FREQUENCY	32768
#define UCS_XT2_CRYSTAL_FREQUENCY	16000000
#define MCLK_DESIRED_FREQUENCY_IN_KHZ 8000
#define MCLK_FLLREF_RATIO MCLK_DESIRED_FREQUENCY_IN_KHZ / (UCS_REFOCLK_FREQUENCY / 1024)
extern uint8_t returnValue;
uint32_t clockValue;
// Status of oscillator fault flags:
uint16_t status;

//Define our pins
#define LED_PORT	GPIO_PORT_P1
#define LED_DATA	GPIO_PIN5
#define LED_CLOCK	GPIO_PIN4
#define LED_LATCH	GPIO_PIN7
#define LED_BLANK	GPIO_PIN3

#define DEBUG_SERIAL 1

// Declare functions
void delay(unsigned int);
void led_enable(uint16_t);
void led_disable(void);
void led_display_bits(uint16_t*);

uint16_t values[5] = {65535, 65535, 65535, 65535, 65535};
uint16_t zeroes[5] = {0, 0, 0, 0, 0};

uint8_t disp[16][5] = { // column-major format (for some reason)
		{0}, {0}, {0}, {0}, {0},
		{0}, {0}, {0}, {0}, {0},
		{0}, {0}, {0}, {0}, {0}, {0}
};

void led_disp_to_values() {
	// row 1 : values[0]
	values[0] = 0;
	for (int bit_index=0; bit_index<16; bit_index++) {
		values[0] |= ((disp[bit_index][0] ? 1 : 0) << bit_index);
	}
	for (int led_segment = 1; led_segment<=4; led_segment++) {
		values[led_segment] = 0;
		for (int bit_index=0; bit_index<16; bit_index++) {
			values[led_segment] |= ((disp[(bit_index % 8) + (~(led_segment & 0b1) * 8)][led_segment + bit_index / 8] ? 1 : 0) << bit_index);
		}
	}
}

void init_watchdog() {
	WDT_A_hold(WDT_A_BASE);
}

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
	   // + GPIO_PIN4 + GPIO_PIN5 // XT1
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

	// PWM disable - TODO:
	GPIO_setAsOutputPin(LED_PORT, LED_BLANK);

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
	GPIO_setAsPeripheralModuleFunctionInputPin(GPIO_PORT_P4, GPIO_PIN5); // 4.5: RXD
	GPIO_setAsPeripheralModuleFunctionOutputPin(GPIO_PORT_P4, GPIO_PIN4); // 4.4: TXD

}

void init_clocks() {

	UCS_setExternalClockSource(
			UCS_XT1_CRYSTAL_FREQUENCY,
			UCS_XT2_CRYSTAL_FREQUENCY
	);

	// Initialize XT1 at 32.768KHz
//	returnValue = UCS_LFXT1StartWithTimeout(
//			UCS_XT1_DRIVE3, // MAX POWER
//			UCS_XCAP_1,		// 5.5 pF, closest to 4 pF of crystal.
//			UCS_XT1_TIMEOUT
//	);

	// Turn off XT1 because it's soldered on backwards. TODO
	UCS_XT1Off();

	// Init XT2:
	returnValue = UCS_XT2StartWithTimeout(
			UCS_XT2DRIVE_8MHZ_16MHZ,
			UCS_XT2_TIMEOUT
	);

	// Setup the clocks:

	// Select XT1 as ACLK source
//	UCS_clockSignalInit(
//			UCS_ACLK,
//			UCS_XT1CLK_SELECT,
//			UCS_CLOCK_DIVIDER_1
//	);

	// Never mind, actually use REFO because the crystal is in backwards.
	UCS_clockSignalInit(
			UCS_ACLK,
			UCS_REFOCLK_SELECT,
			UCS_CLOCK_DIVIDER_1
	);

	//Select XT2 as SMCLK source
	UCS_clockSignalInit(
			UCS_SMCLK,
			UCS_XT2CLK_SELECT,
			UCS_CLOCK_DIVIDER_2 // Divide by 2 to get 8 MHz.
	);

	// Select REFO as the input to the FLL reference.
	// TODO: once the crystal is in correctly, we can go back
	// to using UCS_XT1CLK_SELECT.
	UCS_clockSignalInit(UCS_FLLREF, UCS_REFOCLK_SELECT,
			UCS_CLOCK_DIVIDER_1);

	UCS_initFLLSettle(
			MCLK_DESIRED_FREQUENCY_IN_KHZ,
			MCLK_FLLREF_RATIO
	);

	// TODO: Configure DCO

	// Use the DCO paired with REFO+FLL (or XT1, later) as the master clock
	// This will run at around 1 MHz. This is the default.
	//                     (1,048,576 Hz)
	UCS_clockSignalInit(UCS_MCLK, UCS_DCOCLKDIV_SELECT,
			UCS_CLOCK_DIVIDER_1);

	// Enable global oscillator fault flag
	SFR_clearInterrupt(SFR_OSCILLATOR_FAULT_INTERRUPT);
	SFR_enableInterrupt(SFR_OSCILLATOR_FAULT_INTERRUPT);

	// Enable global interrupt:
	__bis_SR_register(GIE);

	// Verify if the clock settings are as expected:
	clockValue = UCS_getMCLK();
	clockValue = UCS_getACLK();
	clockValue = UCS_getSMCLK();

}

void init_timers() {

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

//	UCA1CTL0 = 0x0;
//	UCA1CTL1 = UCSSEL1;
//	UCA1BR0 = 0x41;
//	UCA1BR1 = 0x03;
//	UCA1MCTL = 0x04;

	UCA1IRTCTL = UCIREN + UCIRTXPL5;
#endif

	USCI_A_UART_enable(USCI_A1_BASE);

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
	} while (text[++sendchar]);
}

uint8_t reg_read = 0;
uint8_t reg_reads[2] = {0, 0};

uint8_t reg_data[65] = {0};
uint8_t test_data[65] = {0};

int main( void )
{
	// TODO: check to see what powerup mode we're in.
	led_disable();
	init_watchdog();
	init_power();
	init_gpio();
	init_clocks();
	init_timers();
	init_serial();
	init_radio();

	led_display_bits(values);
	led_enable(5);
//	led_disable();

	delay(500);

	for (int i=0; i<64; i++) {
		test_data[i] = (uint8_t)'Q';
	}
	write_serial("OK");

//	while (1) {
//
//#if !BADGE_TARGET
//		while (1) {
//			mode_tx_sync();
//			write_serial("Transmitting a thing.\r\n");
//			write_serial(test_data);
//			write_register(RFM_FIFO, test_data, 64);
//			delay(2500);
//		}
//#else
//		mode_rx_sync();
//		write_serial("Receive mode!");
//		while (!rfm_crcok());
//		write_serial("We seem to have gotten something!");
//		read_register_sync(RFM_FIFO, 64, reg_data);
//		write_serial(reg_data);
//#endif
//	}

	//write_single_register(RFM_OPMODE, 0b00010000);

	delay(5000);
	while (1) {
		for (int i=0; i<16; i++) {
			for (int j=0; j<5; j++) {
				disp[i][j] ^= 1;
				led_disp_to_values();
				led_display_bits(values);
				delay(250);
			}
		}
	}

	uint8_t receive_status = read_single_register_sync(RFM_IRQ1);
	while (1) {
		receive_status = read_single_register_sync(RFM_IRQ1);
		delay(100);
		write_single_register(RFM_OPMODE, 0b00010000);
		reg_data[0] = receive_status;
		write_serial(reg_data);
		receive_status &= BIT7;
		if (!receive_status) {
			write_serial("No receive");
		}
		else {
			write_serial("Receive.");
		}
		delay(1500);
	}

	while (1) {
		// LPM3
		// __bis_SR_register(LPM3_bits + GIE);
		reg_reads[0] = reg_read;
		write_serial(reg_reads); // Address
		read_register_sync(reg_read, 1, reg_data);
		write_serial(reg_data);
		delay(500);
		reg_read = (reg_read + 1) % 0x72;
		delay(500);
	}
}

void delay(uint16_t ms)
{
	while (ms--)
    {
        __delay_cycles(MCLK_DESIRED_FREQUENCY_IN_KHZ);
    }
}

void led_display_bits(uint16_t* val)
{
	//Set latch to low (should be already)
	GPIO_setOutputLowOnPin(LED_PORT, LED_LATCH);

	uint16_t i;
	uint8_t j;

	for (j=4; j>=0; j--) {
		// Iterate over each bit, set data pin, and pulse the clock to send it
		// to the shift register
		for (i = 0; i < 16; i++)  {
			WRITE_IF(LED_PORT, LED_DATA, (val[j] & (1 << i)));
			GPIO_pulse(LED_PORT, LED_CLOCK)
		}
	}

	// Pulse the latch pin to write the values into the display register
	GPIO_pulse(LED_PORT, LED_LATCH);
}

void led_enable(uint16_t duty_cycle) {
//	led_disable(); // TODO
	GPIO_setAsPeripheralModuleFunctionOutputPin(LED_PORT, LED_BLANK);

	TIMER_A_generatePWM(
		TIMER_A0_BASE,
		TIMER_A_CLOCKSOURCE_ACLK,
		TIMER_A_CLOCKSOURCE_DIVIDER_1,
		10, // period
		TIMER_A_CAPTURECOMPARE_REGISTER_2,
		TIMER_A_OUTPUTMODE_RESET_SET,
		10 - duty_cycle // duty cycle
	);

	TIMER_A_startCounter(TIMER_A0_BASE, TIMER_A_UP_MODE);

}

void led_disable( void )
{
	GPIO_setAsOutputPin(
			LED_PORT,
			LED_BLANK
	);

	GPIO_setOutputHighOnPin(LED_PORT, LED_BLANK);
}

inline void led_toggle( void ) {
	GPIO_toggleOutputOnPin(LED_PORT, LED_BLANK);
}

#pragma vector=UNMI_VECTOR
__interrupt void NMI_ISR(void)
{
	do {
		// If it still can't clear the oscillator fault flags after the timeout,
		// trap and wait here.
		status = UCS_clearAllOscFlagsWithTimeout(1000);
	} while (status != 0);
}

uint8_t received_data = 0;

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
		USCI_A_UART_transmitData(USCI_A1_BASE, received_data);

		break;
	case 4:                             // Vector 4 - TXIFG // Ready for another character...
		break;
	default: break;
	}
}
