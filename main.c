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

volatile uint8_t received_data = 0;

uint8_t frame_index = 0;
uint8_t ir_tx_frame[4] = {SYNC0, SYNC1, 0, 0};
uint8_t ir_rx_frame[4] = {0};

uint8_t ir_rx_index = 0;
uint8_t ir_rx_len = 4;

volatile uint8_t ir_xmit = 0;
volatile uint8_t ir_xmit_index = 0;
volatile uint8_t ir_xmit_len = 0;
volatile uint8_t ir_xmit_payload = 0;

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
	GPIO_setAsPeripheralModuleFunctionInputPin(GPIO_PORT_P4, GPIO_PIN5); // 4.5: RXD
	GPIO_setAsPeripheralModuleFunctionOutputPin(GPIO_PORT_P4, GPIO_PIN4); // 4.4: TXD
	///////////////////////////////////////////////////////////////////////////

	// Interrupt pin for radio:
	GPIO_setAsInputPin(GPIO_PORT_P2, GPIO_PIN0);
	GPIO_enableInterrupt(GPIO_PORT_P2, GPIO_PIN0);
	GPIO_interruptEdgeSelect(GPIO_PORT_P2, GPIO_PIN0, GPIO_LOW_TO_HIGH_TRANSITION);
	GPIO_clearInterruptFlag(GPIO_PORT_P2, GPIO_PIN0);
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
	USCI_A_UART_disable(USCI_A1_BASE);
	USCI_A_UART_initAdvance(
			USCI_A1_BASE,
			USCI_A_UART_CLOCKSOURCE_SMCLK,
			416,
			0,
			6,
			USCI_A_UART_NO_PARITY,
			USCI_A_UART_MSB_FIRST,
			USCI_A_UART_ONE_STOP_BIT,
			USCI_A_UART_MODE,
			USCI_A_UART_LOW_FREQUENCY_BAUDRATE_GENERATION
	);
	USCI_A_UART_disable(USCI_A1_BASE);

	UCA1IRTCTL = UCIREN + UCIRTXPL2 + UCIRTXPL0;
	UCA1IRRCTL |= UCIRRXPL;
#endif

	USCI_A_UART_enable(USCI_A1_BASE);

	USCI_A_UART_clearInterruptFlag(USCI_A1_BASE, USCI_A_UART_RECEIVE_INTERRUPT_FLAG);
	USCI_A_UART_enableInterrupt(
			USCI_A1_BASE,
			USCI_A_UART_RECEIVE_INTERRUPT
	);

	USCI_A_UART_clearInterruptFlag(USCI_A1_BASE, USCI_A_UART_TRANSMIT_INTERRUPT_FLAG);
	USCI_A_UART_enableInterrupt(
				USCI_A1_BASE,
				USCI_A_UART_TRANSMIT_INTERRUPT
		);
}

void write_ir_byte(uint8_t payload) {
//		while (!USCI_A_UART_getInterruptStatus(USCI_A1_BASE, UCTXIFG));
//		USCI_A_UART_transmitData(USCI_A1_BASE, SYNC2);
//		USCI_A_UART_transmitData(USCI_A1_BASE, SYNC1);
		USCI_A_UART_transmitData(USCI_A1_BASE, payload);
//		USCI_A_UART_transmitData(USCI_A1_BASE, 0);
//		while (!f_rx_ready);
//		f_rx_ready = 0;
}

void write_serial(uint8_t* text) {
	uint16_t sendchar = 0;
	do {
//		while (!USCI_A_UART_getInterruptStatus(USCI_A1_BASE, UCTXIFG));
		write_ir_byte(text[sendchar]);
//		while (!f_rx_ready);
//		f_rx_ready = 0;
	} while (text[++sendchar]);
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

	uint8_t test_char = 0;

	print("Startup");
	led_disp_bit_to_values(0, 0);
	led_display_bits(values);
	led_enable(1);
	delay(2000);

	char hex[4] = "AA";
//	delay(2000);

	uint8_t val;
	uint8_t seen_j = 255;
	while (1) {
		seen_j = 255;
		for (uint8_t j=1; j<20; j++) {
			for (uint16_t i=1; i!=0; i++)
				if (f_rx_ready) {
					seen_j = j;
					f_rx_ready = 0;
					val = ir_rx_frame[2];
					hex[0] = (val/16 < 10)? '0' + val/16 : 'A' - 10 + val/16;
					hex[1] = (val%16 < 10)? '0' + val%16 : 'A' - 10 + val%16;
					print(hex);
				}
			if (seen_j != j) {
				print("...");
			}
		}
//		write_ir_byte(test_char++);

		ir_tx_frame[2] = test_char++;
//		uint8_t ir_rx_frame[4] = {0};
		ir_xmit = 1;
		ir_xmit_index = 0;
		ir_xmit_len = 4;
		USCI_A_UART_transmitData(USCI_A1_BASE, ir_tx_frame[0]);
//		delay(2000);
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
		status = UCS_clearAllOscFlagsWithTimeout(1000);
	} while (status != 0);
}

#pragma vector=USCI_A1_VECTOR
__interrupt void USCI_A1_ISR(void)
{
	/*
	 * NOTE: The RX interrupt has priority over TX interrupt. As a result,
	 * although normally after transmitting over IR we'll see the TX interrupt
	 * first, then the corresponding RX interrupt (because the transceiver
	 * echoes TX to RX), when stepping through in debug mode it will often
	 * be the case the the order is reversed: RXI, followed by the corresponding
	 * TX interrupt.
	 */
	switch(__even_in_range(UCA1IV,4))
	{
	case 0:	// 0: No interrupt.
		break;
	case 2:	// RXIFG: RX buffer ready to read.
//		if (f_rx_ready == 2) {
//			f_rx_ready = 0;
//			// don't clobber what we may have actually read, because we just sent something:
//			USCI_A_UART_clearInterruptFlag(USCI_A1_BASE, USCI_A_UART_RECEIVE_INTERRUPT_FLAG);
//		}
//		else {
			received_data = USCI_A_UART_receiveData(USCI_A1_BASE);
			if (ir_rx_index == 0 && received_data == SYNC0) {
				// do stuff
			} else if (ir_rx_index == 1 && received_data == SYNC1) {
				// do stuff
			}
			else if (ir_rx_index == 2) {
				// do stuff, payload
			} else if (ir_rx_index == 3 && received_data==0) {
				f_rx_ready = 1;
				ir_rx_index = 0;
				// do stuff, successful receive
			} else {
				// malformed
				ir_rx_index = 0;
				break;
			}
			ir_rx_frame[ir_rx_index] = received_data;
			if (!f_rx_ready)
				ir_rx_index++;
//		}

		break;
	case 4:	// TXIFG: TX buffer is sent.
		ir_xmit_index++;
		if (ir_xmit_index >= ir_xmit_len) {
			ir_xmit = 0;
		}

		if (ir_xmit) {
			USCI_A_UART_transmitData(USCI_A1_BASE, ir_tx_frame[ir_xmit_index]);
		}
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
