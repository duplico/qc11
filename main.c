#include "qcxi.h"
#include "radio.h"
#include "fonts.h"
#include "clocks.h"
#include "leds.h"

extern uint8_t returnValue; // TODO: remove

#define DEBUG_SERIAL 1

// Declare functions
void delay(unsigned int);
void led_enable(uint16_t);
void led_disable(void);
void led_display_bits(uint16_t*);
void led_on();

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


volatile Calendar currentTime;
char time[6] = "00:00";

volatile uint8_t f_new_minute = 0;

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
	init_serial();
	init_radio();

	// Enable global interrupt:
	__bis_SR_register(GIE);

	led_disable();
	print("qcxi  QCXI");
	print("Hello Evan!");
	led_disp_bit_to_values(0, 0);
	led_display_bits(values);
//	led_on();
	led_enable(1);

	// TODO
	uint16_t i = 0b0000000000011111;
	uint16_t buffer_offset = 0;
	while (1) {
		led_set_rainbow(i);
		led_disp_bit_to_values(buffer_offset, 0);
		led_display_bits(values);
		buffer_offset = (buffer_offset + 1) % BACK_BUFFER_WIDTH;
		i = _rotl(i, 1);
		delay(100);
	}

    //Setup Current Time for Calendar
    currentTime.Seconds    = 0x00;
    currentTime.Minutes    = 0x19;
    currentTime.Hours      = 0x18;
    currentTime.DayOfWeek  = 0x03;
    currentTime.DayOfMonth = 0x20;
    currentTime.Month      = 0x07;
    currentTime.Year       = 0x2011;

    //Initialize Calendar Mode of RTC
    /*
     * Base Address of the RTC_A_A
     * Pass in current time, intialized above
     * Use BCD as Calendar Register Format
     */
    RTC_A_calendarInit(RTC_A_BASE,
                       currentTime,
                       RTC_A_FORMAT_BCD);

    //Specify an interrupt to assert every minute
    RTC_A_setCalendarEvent(RTC_A_BASE,
                           RTC_A_CALENDAREVENT_MINUTECHANGE);

    //Enable interrupt for RTC Ready Status, which asserts when the RTC
    //Calendar registers are ready to read.
    //Also, enable interrupts for the Calendar alarm and Calendar event.
    RTC_A_clearInterrupt(RTC_A_BASE,
                         RTCRDYIFG + RTCTEVIFG + RTCAIFG);
    RTC_A_enableInterrupt(RTC_A_BASE,
                          RTCRDYIE + RTCTEVIE + RTCAIE);

    //Start RTC Clock
    RTC_A_startClock(RTC_A_BASE);

    // End clock startup


	for (int i=0; i<64; i++) {
		test_data[i] = (uint8_t)'Q';
	}

//    // Radio startup
//	mode_rx_sync();
//	print("RX mode");
//	led_disp_bit_to_values(0, 0);
//	led_display_bits(values);

	// TODO: loop


//	while (!rfm_crcok());
//	write_serial("We seem to have gotten something!");
//	read_register_sync(RFM_FIFO, 64, reg_data);

	//		write_serial("Receive mode!");
	//		write_serial(reg_data);

	while (1) {
		mode_rx_sync();
		print("RX mode");
		led_disp_bit_to_values(0, 0);
		led_display_bits(values);

		if (f_new_minute) {
			currentTime = RTC_A_getCalendarTime(RTC_A_BASE);
			time[0] = '0' + ((currentTime.Hours & 0b11110000) >> 4);
			time[1] = '0' + (currentTime.Hours & 0b1111);
			time[3] = '0' + ((currentTime.Minutes & 0b11110000) >> 4);
			time[4] = '0' + (currentTime.Minutes & 0b1111);
			print(time);
			f_new_minute = 0;
		}
		for (int i=0; i<BACK_BUFFER_WIDTH; i++) {
			led_disp_bit_to_values(i, 0);
			led_display_bits(values);
			delay(100);
			if (rfm_crcok()) {
				read_register_sync(RFM_FIFO, 64, reg_data);
				//print((char *)reg_data);
				print("recv");
			}
		}
		mode_tx_sync();
		print(" TX");
		led_disp_bit_to_values(0, 0);
		led_display_bits(values);

		write_register(RFM_FIFO, test_data, 64);
		delay(500);
		// check 0x28 & BIT3, AKA "PacketSent"
		packet_sent = read_single_register_sync(0x28);
		packet_sent &= BIT3;
		print((packet_sent >> 3) ? "snt" : "ntx?");
		led_disp_bit_to_values(0, 0);
		led_display_bits(values);
		delay(500);



		// TODO: "scroll" flag w/ configgable ISR
	}

	//write_single_register(RFM_OPMODE, 0b00010000);

//	led_display_bits(zeroes);
	led_enable(10);

	delay(2000);
	while (1) {
		for (int i=0; i<BACK_BUFFER_WIDTH; i++) {
//			led_disp_to_values(i, 0);
			led_disp_bit_to_values(i, 0);
			led_display_bits(values);
			delay(100);
		}
//		print("test");
	}

//	uint8_t receive_status = read_single_register_sync(RFM_IRQ1);
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

#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector=RTC_VECTOR
__interrupt
#elif defined(__GNUC__)
__attribute__((interrupt(RTC_VECTOR)))
#endif
void RTC_A_ISR(void)
{
        switch (__even_in_range(RTCIV, 16)) {
        case 0: break;  //No interrupts
        case 2:         //RTCRDYIFG
                //Toggle P1.0 every second
//                GPIO_toggleOutputOnPin(
//                        GPIO_PORT_P1,
//                        GPIO_PIN0);
                break;
        case 4:         //RTCEVIFG
                //Interrupts every minute
                f_new_minute = 1;
                break;
        case 6:         //RTCAIFG
                //Interrupts 5:00pm on 5th day of week
                __no_operation();
                break;
        case 8: break;  //RT0PSIFG
        case 10: break; //RT1PSIFG
        case 12: break; //Reserved
        case 14: break; //Reserved
        case 16: break; //Reserved
        default: break;
        }
}
