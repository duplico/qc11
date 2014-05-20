//***************************************************************************************
// MSP430 Driver for 74HC595 Shift Register
//
// Description; Drives 8 LED's with 3 digital pins of the MSP430, via a shift register
//
// MSP430x2xx
//
//***************************************************************************************
#include <msp430f5308.h>
#include <stdint.h>
#include "driverlib.h"

#define WRITE_IF(port, pin, val) if (val) GPIO_setOutputHighOnPin(port, pin); else GPIO_setOutputLowOnPin(port, pin);
#define GPIO_pulse(port, pin) do { GPIO_setOutputHighOnPin(port, pin); GPIO_setOutputLowOnPin(port, pin); } while (0);

// For clock initialization:
#define UCS_XT1_TIMEOUT 50000
#define UCS_XT2_TIMEOUT 50000
#define UCS_XT1_CRYSTAL_FREQUENCY	32768
#define UCS_XT2_CRYSTAL_FREQUENCY	16000000
#define MCLK_DESIRED_FREQUENCY_IN_KHZ 8000
#define MCLK_FLLREF_RATIO MCLK_DESIRED_FREQUENCY_IN_KHZ / (UCS_REFOCLK_FREQUENCY / 1024)
uint8_t returnValue = 0;
uint32_t clockValue;
// Status of oscillator fault flags:
uint16_t status;

//Define our pins
#define LED_PORT	GPIO_PORT_P1
#define LED_DATA	GPIO_PIN5
#define LED_CLOCK	GPIO_PIN4
#define LED_LATCH	GPIO_PIN7
#define LED_BLANK	GPIO_PIN3

// Declare functions
void delay(unsigned int);
void led_enable(uint16_t);
void led_disable(void);
void led_display_bits(uint16_t*);

uint16_t values[5] = {65535, 65535, 65535, 65535, 65535};
uint16_t zeroes[5] = {0, 0, 0, 0, 0};

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
	P1OUT = 0x00;
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

	GPIO_setAsPeripheralModuleFunctionOutputPin(
			GPIO_PORT_P5,
			GPIO_PIN2 + GPIO_PIN3 // XT2
	   // + GPIO_PIN4 + GPIO_PIN5 // XT1
	);

	GPIO_setAsOutputPin(
			LED_PORT,
			LED_DATA + LED_CLOCK + LED_LATCH // + LED_BLANK
	);

	GPIO_setAsPeripheralModuleFunctionOutputPin(LED_PORT, LED_BLANK);

	GPIO_setAsPeripheralModuleFunctionOutputPin(
			GPIO_PORT_P4,
			GPIO_PIN4
	);
	GPIO_setAsPeripheralModuleFunctionInputPin(
			GPIO_PORT_P4,
			GPIO_PIN5
	);
}

void init_clocks() {

	UCS_setExternalClockSource(
			UCS_XT1_CRYSTAL_FREQUENCY,
			UCS_XT2_CRYSTAL_FREQUENCY
	);

	// Initialize XT1 at 32.768KHz
//	returnValue = UCS_LFXT1StartWithTimeout(
//			UCS_XT1_DRIVE3, // MAX POWER
//			UCX_XCAP_1,		// 5.5 pF, closest to 4 pF of crystal.
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

		USCI_A_UART_enable(USCI_A1_BASE);

		USCI_A_UART_enableInterrupt(
				USCI_A1_BASE,
				USCI_A_UART_RECEIVE_INTERRUPT
		);
}


int main( void )
{
	// TODO: check to see what powerup mode we're in.
	init_watchdog();
	init_power();
	init_gpio();
	init_clocks();
	init_timers();
	init_serial();

//	led_display_bits(values);
	//led_enable(5);
	led_disable();

	while (1) {
		// LPM3
		// __bis_SR_register(LPM3_bits + GIE);
//		led_enable(duty++);
//		duty %= 10;
//		delay(500);
//		led_enable(5);
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

void led_display_bits(uint16_t* val)
{
	//Set latch to low (should be already)
	GPIO_setOutputLowOnPin(LED_PORT, LED_LATCH);

	uint16_t i;
	uint8_t j;

	for (j=5; j; j--) {
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
//	GPIO_setAsPeripheralModuleFunctionOutputPin(LED_PORT, LED_BLANK);
//
//	TIMER_A_generatePWM(
//		TIMER_A0_BASE,
//		TIMER_A_CLOCKSOURCE_ACLK,
//		TIMER_A_CLOCKSOURCE_DIVIDER_1,
//		10, // period
//		TIMER_A_CAPTURECOMPARE_REGISTER_2,
//		TIMER_A_OUTPUTMODE_RESET_SET,
//		10 - duty_cycle // duty cycle
//	);
//
//	TIMER_A_startCounter(TIMER_A0_BASE, TIMER_A_UP_MODE);

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

#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector=UNMI_VECTOR
__interrupt
#elif defined(__GNUC__)
__attribute__((interrupt(UNMI_VECTOR)))
#endif
void NMI_ISR(void)
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
  case 4:break;                             // Vector 4 - TXIFG // Ready for another character...
  default: break;
  }
}
