#include "qcxi.h"

#if !BADGE_TARGET
/*
 * Software functions for MSP430 to drive WS2812/B RGB LEDs via one-wire bus
 *
 * The SPI peripheral will be used in cooperation with one of two transport stream encodings schemes.
 * One 3-bit and one 4-bit encoding was implemented.
 *
 */

#include "ws2812.h"
#include <string.h>

volatile ledcount_t ws_bytes_to_send = 0;
volatile ledcount_t ws_byte_index = 0;

uint8_t ws_frameBuffer[(ENCODING * sizeof(ledcolor_t) * NUMBEROFLEDS)] = { 0, };

ledcolor_t leds[21] = {
		// rainbow colors
		{ 0xc, 0x17, 0x1 },
		{ 0x10, 0x15, 0x0 },
		{ 0x13, 0x12, 0x0 },
		{ 0x16, 0xe, 0x0 },
		{ 0x18, 0xa, 0x2 },
		{ 0x19, 0x7, 0x5 },
		{ 0x19, 0x4, 0x9 },
		{ 0x17, 0x1, 0xc },
		{ 0x15, 0x0, 0x10 },
		{ 0x12, 0x0, 0x14 },
		{ 0xe, 0x1, 0x16 },
		{ 0xa, 0x2, 0x18 },
		{ 0x7, 0x5, 0x19 },
		{ 0x4, 0x9, 0x19 },
		{ 0x1, 0xc, 0x17 },
		{ 0x0, 0x10, 0x15 },
		{ 0x0, 0x14, 0x12 },
		{ 0x1, 0x16, 0xe },
		{ 0x2, 0x18, 0xa },
		{ 0x5, 0x19, 0x7 },
		{ 0x9, 0x19, 0x4 },
};

ledcolor_t blankLed = {0x00, 0x00, 0x00};

void ws2812_init() {

	USCI_B_SPI_masterInit(
		USCI_B0_BASE,
		USCI_B_SPI_CLOCKSOURCE_SMCLK,
		12000000, // should always be so.
		2400000, // we're really going for 2400000 but sometimes rounding happens
		USCI_B_SPI_MSB_FIRST,
		USCI_B_SPI_PHASE_DATA_CHANGED_ONFIRST_CAPTURED_ON_NEXT,
		USCI_B_SPI_CLOCKPOLARITY_INACTIVITY_LOW
	);

	USCI_B_SPI_enable(USCI_B0_BASE);

	GPIO_setAsPeripheralModuleFunctionOutputPin(GPIO_PORT_P3, GPIO_PIN0);

	// Note: We are specifically NOT enabling interrupts here because the
	// 		 asynchronous functions for sending LED buffers activate them,
	//		 then the ISR deactivates them once it's finished.

	fillFrameBufferSingleColor(&blankLed, NUMBEROFLEDS, ws_frameBuffer, ENCODING);
	ws_set_colors_blocking(ws_frameBuffer, NUMBEROFLEDS);
}

/*
 * Shift the current LED buffer forward by 1, in a circular fashion.
 *
 */
void ws_rotate(ledcolor_t* leds, ledcount_t ledCount) {
	ledcolor_t tmpLed;
	ledcount_t ledIdx;

	tmpLed = leds[ledCount-1];
	for(ledIdx=(ledCount-1); ledIdx > 0; ledIdx--) {
		leds[ledIdx] = leds[ledIdx-1];
	}
	leds[0] = tmpLed;
}

void ws_set_colors_async(ledcount_t ledCount) {
	ws_bytes_to_send = (ENCODING * sizeof(ledcolor_t) * ledCount);
	USCI_B_SPI_clearInterruptFlag(USCI_B0_BASE, USCI_B_SPI_TRANSMIT_INTERRUPT);
	USCI_B_SPI_enableInterrupt(USCI_B0_BASE, USCI_B_SPI_TRANSMIT_INTERRUPT);
	ws_byte_index = 0;
	USCI_B_SPI_transmitData(USCI_B0_BASE, 0);
}

// copy bytes from the buffer to SPI transmit register
// should be reworked to use DMA
void ws_set_colors_blocking(uint8_t* buffer, ledcount_t ledCount) {
	__bic_SR_register(GIE); // TODO: need to make this interrupt based:
	for (ws_byte_index=0; ws_byte_index < (ENCODING * sizeof(ledcolor_t) * ledCount); ws_byte_index++) {
		while (!(UCB0IFG & UCTXIFG));		// wait for TX buffer to be ready
		USCI_B_SPI_transmitData(USCI_B0_BASE,buffer[ws_byte_index]);
	}
	__bis_SR_register(GIE);
	__delay_cycles(300);
}


void fillFrameBuffer(ledcolor_t* leds, ledcount_t ledCount, uint8_t* buffer, uint8_t encoding) {
	// encoding is 3, like it or not.
	ledcount_t ledIdx;
	uint16_t fbIdx;

	fbIdx = 0;
	for (ledIdx = 0; ledIdx < ledCount; ledIdx++) {
		encodeData3bit(&leds[ledIdx], &buffer[fbIdx]);
		fbIdx += (3 * sizeof(ledcolor_t));
	}
}

void fillFrameBufferSingleColor(ledcolor_t* led, ledcount_t ledCount, uint8_t* buffer, uint8_t encoding) {
	//encoding is 3, like it or not.
	ledcount_t ledIdx;
	uint16_t fbIdx;

	fbIdx = 0;
	for (ledIdx = 0; ledIdx < ledCount; ledIdx++) {
		encodeData3bit(led, &buffer[fbIdx]);
		fbIdx += (3 * sizeof(ledcolor_t));
	}
}

/*
 ******************
 * 3-bit encoding *
 ******************
 *
 * 8 bits from LED color stream encoded in 3 byte for transport stream (SPI TX)
 * or: 1 bit from LED color stream encoded in 3 bit for transport stream
 *
 *				_
 * ZERO: 100	 |__
 *	 	 	 	__
 * ONE : 110	  |_
 *
 * the bit   in the middle defines the value
 *
 * data stream: 0x23		 		 0  0  1  0  0  0  1  1
 * encoding:						1x01x01x01x01x01x01x01x0
 * transport stream:				100100110100100100110110
 *
 * initial mask: 0x92 0x49 0x24		100100100100100100100100
 *
 * sourcebit :						 7  6  5  4  3  2  1  0
 * encoding  :						1x01x01x01x01x01x01x01x0
 * targetbit :						 6  3  0  5  2  7  4  1
 * targetbyte:						|   0   |   1   |   2   |
 *
 * sourcebit -> (targetbit,targetbyte)
 * 7->(6,0)
 * 6->(3,0)
 * 5->(0,0)
 * 4->(5,1)
 * 3->(2,1)
 * 2->(7,2)
 * 1->(4,2)
 * 0->(1,2)
 */
void encodeData3bit(ledcolor_t* led, uint8_t* output) {
	uint8_t colorIdx, outputIdx;
	uint8_t grbLED[sizeof(*led)];	// reordered color order
	uint8_t shiftRegister;

	// WS2812 is expecting GRB instead of RGB
	grbLED[0] = led->green;
	grbLED[1] = led->red;
	grbLED[2] = led->blue;

	outputIdx = 0;
	// loop over the color bytes and convert each bit to three bits for transport stream
	for (colorIdx=0; colorIdx < sizeof(grbLED); colorIdx++) {
		// prepare frameBuffer with initial transport bitmask
		output[outputIdx+0] = 0x92;
		output[outputIdx+1] = 0x49;
		output[outputIdx+2] = 0x24;

		/*
		 * bit remapping starts here
		 */

		// right shift bits
		shiftRegister = grbLED[colorIdx];
		shiftRegister >>= 1;	// 1 shift from original
		output[outputIdx+0] |= (shiftRegister & BIT6);	// source bit 7
		output[outputIdx+1] |= (shiftRegister & BIT2);	// source bit 3
		shiftRegister >>= 2;	// 3 shifts from original
		output[outputIdx+0] |= (shiftRegister & BIT3);	// source bit 6
		shiftRegister >>= 2;	// 5 shifts from original
		output[outputIdx+0] |= (shiftRegister & BIT0);	// source bit 5

		// left shift bits
		shiftRegister = grbLED[colorIdx];
		shiftRegister <<= 1;	// 1 shift from original
		output[outputIdx+1] |= (shiftRegister & BIT5);	// source bit 4
		output[outputIdx+2] |= (shiftRegister & BIT1);	// source bit 0
		shiftRegister <<= 2;	// 3 shifts from original
		output[outputIdx+2] |= (shiftRegister & BIT4);	// source bit 1
		shiftRegister <<= 2;	// 5 shifts from original
		output[outputIdx+2] |= (shiftRegister & BIT7);	// source bit 2

		outputIdx += 3;	// next three bytes (color)
	}
}

// SERIAL:
volatile char ser_buffer_rx[255] = {0};
volatile char ser_buffer_tx[255] = {0};
volatile uint8_t ser_index_rx = 0;
volatile uint8_t ser_index_tx = 0;


void ser_init() {

	GPIO_setAsPeripheralModuleFunctionOutputPin(GPIO_PORT_P4, GPIO_PIN4); // USCI_A1_TXD
	GPIO_setAsPeripheralModuleFunctionInputPin(GPIO_PORT_P4, GPIO_PIN5);  // USCI_A1_RXD

	// UART Serial to PC //////////////////////////////////////////////////////
	//
	// Initialize the UART serial, used to speak over USB.
	// Debug serial. 9600 baud, 8N1:

	USCI_A_UART_initAdvance(
			USCI_A1_BASE,
			USCI_A_UART_CLOCKSOURCE_SMCLK,
			1250,
			0,
			0,
			USCI_A_UART_NO_PARITY,
			USCI_A_UART_LSB_FIRST,
			USCI_A_UART_ONE_STOP_BIT,
			USCI_A_UART_MODE,
			USCI_A_UART_LOW_FREQUENCY_BAUDRATE_GENERATION
	);

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

void ser_print(char* text) {
	strcpy(ser_buffer_tx, text);
	ser_index_tx = 0;
	USCI_A_UART_transmitData(USCI_A1_BASE, ser_buffer_tx[ser_index_tx]);
}

#pragma vector=USCI_A1_VECTOR
__interrupt void ser_debug_isr(void)
{
	switch(__even_in_range(UCA1IV,4))
	{
	case 0:	// 0: No interrupt.
		break;
	case 2:	// RXIFG: RX buffer ready to read.
		ser_buffer_rx[ser_index_rx] = USCI_A_UART_receiveData(USCI_A1_BASE);
		if (ser_buffer_rx[ser_index_rx] == 0x0d) {
			f_ser_rx = 1;
			ser_index_rx = 0;
			__bic_SR_register(LPM3_bits);
		} else {
			ser_index_rx++;
		}
		break;
	case 4:	// TXIFG: TX buffer is sent.
		ser_index_tx++;
		if (ser_buffer_tx[ser_index_tx]) {
			USCI_A_UART_transmitData(USCI_A1_BASE, ser_buffer_tx[ser_index_tx]);
		}
		break;
	default: break;
	}
}

#pragma vector=USCI_B0_VECTOR
__interrupt void ws_isr(void)
{
	/*
	 * NOTE: The RX interrupt has priority over TX interrupt. As a result,
	 * although normally after transmitting over IR we'll see the TX interrupt
	 * first, then the corresponding RX interrupt (because the transceiver
	 * echoes TX to RX), when stepping through in debug mode it will often
	 * be the case the the order is reversed: RXI, followed by the corresponding
	 * TX interrupt.
	 */
	switch(__even_in_range(UCB0IV,4))
	{
	case 0:	// 0: No interrupt.
		break;
	case 2:	// RXIFG: RX buffer ready to read.
		// This will not be happening.
		break;
	case 4:	// TXIFG: TX buffer is sent.
		// ws_byte_index is the index of the byte we just sent.
		UCB0TXBUF = ws_frameBuffer[ws_byte_index];
		ws_byte_index++;
		if (ws_byte_index == ws_bytes_to_send) {
			USCI_B_SPI_disableInterrupt(USCI_B0_BASE, USCI_B_SPI_TRANSMIT_INTERRUPT);
		}
		break;
	default: break;
	}
}

#endif
