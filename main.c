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

// For clock initialization:
#define UCS_XT1_TIMEOUT 50000
#define UCS_XT2_TIMEOUT 50000
#define UCS_XT1_CRYSTAL_FREQUENCY    32768
#define UCS_XT2_CRYSTAL_FREQUENCY   16000000
uint8_t returnValue = 0;
uint32_t clockValue;
// Status of oscillator fault flags:
uint16_t status;

//Define our pins
#define DATA BIT5 // DS -> 1.0
#define CLOCK BIT4 // SH_CP -> 1.4
#define LATCH BIT7 // ST_CP -> 1.5
#define BLANK BIT3 // OE -> 1.6
// The OE pin can be tied directly to ground, but controlling
// it from the MCU lets you turn off the entire array without
// zeroing the register

// Declare functions
void delay ( unsigned int );
void pulseClock ( void );
void shiftOut ( uint16_t* );
void enable ( void );
void disable ( void );
void init ( void );
void pinWrite ( uint16_t, uint16_t );

uint16_t values[5] = {65535, 65535, 65535, 65535, 65535};
uint16_t zeroes[5] = {0, 0, 0, 0, 0};

int main( void )
{
	// Stop watchdog timer to prevent time out reset
	WDT_A_hold(WDT_A_BASE);

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

	P1DIR |= (DATA + CLOCK + LATCH + BLANK);  // Setup pins as outputs

	P5SEL = BIT2 + BIT3 + BIT4 + BIT5;
	UCSCTL4 |= SELA_0 + SELS_5 + SELM_5;
	UCSCTL6 &= ~XT2BYPASS;



	enable(); // Enable output (set blank low)

	for(;;){
	  shiftOut(values);
	  delay(100);
	  shiftOut(zeroes);
	  delay(100);
	}
}

// Delays by the specified Milliseconds
// thanks to:
// http://www.threadabort.com/archive/2010/09/05/msp430-delay-function-like-the-arduino.aspx
void delay(uint16_t ms)
{
 while (ms--)
    {
        __delay_cycles(16000); // set for 16Mhz change it to 1000 for 1 Mhz
    }
}

// Writes a value to the specified bitmask/pin. Use built in defines
// when calling this, as the shiftOut() function does.
// All nonzero values are treated as "high" and zero is "low"
void pinWrite( uint16_t bit, uint16_t val )
{
  if (val){
    P1OUT |= bit;
  } else {
    P1OUT &= ~bit;
  }
}

// Pulse the clock pin
void pulseClock( void )
{
  P1OUT |= CLOCK;
  P1OUT ^= CLOCK;

}

// Take the given 8-bit value and shift it out, LSB to MSB
void shiftOut(uint16_t* val)
{
  //Set latch to low (should be already)
  P1OUT &= ~LATCH;

  uint16_t i;
  uint8_t j;

  for (j=5; j; j--) {

	  // Iterate over each bit, set data pin, and pulse the clock to send it
	  // to the shift register
	  for (i = 0; i < 16; i++)  {
		  pinWrite(DATA, (val[j] & (1 << i)));
		  pulseClock();
	  }
  }

  // Pulse the latch pin to write the values into the display register
  P1OUT |= LATCH;
  P1OUT &= ~LATCH;
}

// These functions are just a shortcut to turn on and off the array of
// LED's when you have the enable pin tied to the MCU. Entirely optional.
void enable( void )
{
  P1OUT &= ~BLANK;
}

void disable( void )
{
  P1OUT |= BLANK;
}
