/*
 * seven_seg.h
 *
 * Created: 16.11.2017 23:10:56
 * Last edition: 25.07.2018 10:25:36
 *  Author: Dr. Saldon
 */ 

#ifndef SEVEN_SEG_H_
#define SEVEN_SEG_H_

/*-----------------------------------------------------------------------------
INCLUDE SECTION
-----------------------------------------------------------------------------*/
#include <avr/io.h>

/*-----------------------------------------------------------------------------
MACRO SECTION
-----------------------------------------------------------------------------*/
/**
 *  Bit definitions for SN74HC595 to IV-3A tube
 *  ( LClock v.1.3, v.1.4 only )
 *
 * A - 10
 * B - 6
 * C - 11 (point)
 * D - 5
 * E - 1
 * F - 4
 * G - 2
 * H - 3
 */
#define N_NAN	0b00000000
#define N0		0b01111011
#define N1		0b00010001
#define N2		0b11011010
#define N3		0b11010011
#define N4		0b10110001
#define N5		0b11100011
#define N6		0b11101011
#define N7		0b01010001
#define N8		0b11111011
#define N9		0b11110011
#define DP		0b00000100

#define NA		0b11111001
#define NB		0b10101011
#define NC		0b01101010
#define ND		0b10011011
#define NE		0b11101010
#define NF		0b11101000
#define NH		0b10101001
#define NL		0b00101010
#define NN		0b10001001
#define NO		0b10001011
#define NP		0b11111000
#define NR		0b10001000
#define NS		0b11100011
#define NT		0b10101010
#define NU		0b00111011
#define NY		0b10110011
#define NDASH	0b10000000

#define HC595_PORT   	PORTB
#define HC595_DDR    	DDRB

#define HC595_DS_POS 	PB2			//Data pin (DS or SER) pin location

#define HC595_SH_CP_POS PB0			//Shift Clock (SH_CP or SRCLK) pin location
#define HC595_ST_CP_POS PB1			//Store Clock (ST_CP or RCLK) pin location

//Low level macros to change data (DS)lines
#define HC595DataHigh() (HC595_PORT|=(1<<HC595_DS_POS))
#define HC595DataLow() (HC595_PORT&=(~(1<<HC595_DS_POS)))

/*-----------------------------------------------------------------------------
HEADER SECTION
-----------------------------------------------------------------------------*/
void HC595Init(void);
void HC595Pulse(void);
void HC595Latch(void);
void HC595Write(uint8_t data);

/*-----------------------------------------------------------------------------
IMPLEMENTATION SECTION
-----------------------------------------------------------------------------*/

//Initialize HC595 System
void HC595Init(void)
{
   //Make the Data(DS), Shift clock (SH_CP), Store Clock (ST_CP) lines output
   HC595_DDR|=((1<<HC595_SH_CP_POS)|(1<<HC595_ST_CP_POS)|(1<<HC595_DS_POS));
   HC595_PORT &= ~((1<<HC595_SH_CP_POS)|(1<<HC595_ST_CP_POS)|(1<<HC595_DS_POS));
}

//Sends a clock pulse on SH_CP line
void HC595Pulse(void)
{
   //Pulse the Shift Clock
   HC595_PORT|=(1<<HC595_SH_CP_POS);//HIGH
   //_delay_loop_1(4);
   HC595_PORT&=(~(1<<HC595_SH_CP_POS));//LOW
   //_delay_loop_1(4);
}

//Sends a clock pulse on ST_CP line
void HC595Latch(void)
{
   //Pulse the Store Clock
   HC595_PORT|=(1<<HC595_ST_CP_POS);      //HIGH
   //_delay_loop_1(4);
   HC595_PORT&=(~(1<<HC595_ST_CP_POS));   //LOW
   //_delay_loop_1(4);
}

/**
 * Main High level function to write a single byte to
 * Output shift register 74HC595. 
 * 
 * Arguments:
 *    single byte to write to the 74HC595 IC
 * 
 * Returns:
 *    NONE
 * 
 * Description:
 *    The byte is serially transfered to 74HC595
 *    and then latched. The byte is then available on
 *    output line Q0 to Q7 of the HC595 IC.
 * 
 */
void HC595Write(uint8_t data)
{
   //Send each 8 bits serially
   //Order is MSB first
   for(uint8_t i=0;i<8;i++)
   {
      //Output the data on DS line according to the
      //Value of MSB
      if(data & 0b10000000)
      {
         HC595DataHigh();		//MSB is 1 so output high
      }
      else
      {
         HC595DataLow();		//MSB is 0 so output high
      }

      HC595Pulse();  //Pulse the Clock line
      data=data<<1;  //Now bring next bit at MSB position
   }
   //Now all 8 bits have been transferred to shift register
   //Move them to output latch at one
   HC595Latch();
}

uint8_t HC595GetDigitMask(uint8_t digit, bool point)
{
	uint8_t _tmp = 0;
	switch(digit)
	{
	case 0:
	{
		_tmp = N0;
		break;
	}
	case 1:
	{
		_tmp = N1;
		break;
	}
	case 2:
	{
		_tmp = N2;
		break;
	}
	case 3:
	{
		_tmp = N3;
		break;
	}
	case 4:
	{
		_tmp = N4;
		break;
	}
	case 5:
	{
		_tmp = N5;
		break;
	}
	case 6:
	{
		_tmp = N6;
		break;
	}
	case 7:
	{
		_tmp = N7;
		break;
	}
	case 8:
	{
		_tmp = N8;
		break;
	}
	case 9:
	{
		_tmp = N9;
		break;
	}
	default:
	{
		_tmp = N_NAN;
		break;
	}
	}

	if(point == 1){
		_tmp ^= DP;
	}

	return _tmp;
}

uint8_t HC595GetCharMask(char ch, bool point)
{
	uint8_t _tmp = 0;
	switch(ch)
	{
	case 'a':
	{
		_tmp = NA;
		break;
	}
	case 'b':
	{
		_tmp = NB;
		break;
	}
	case 'c':
	{
		_tmp = NC;
		break;
	}
	case 'd':
	{
		_tmp = ND;
		break;
	}
	case 'e':
	{
		_tmp = NE;
		break;
	}
	case 'f':
	{
		_tmp = NF;
		break;
	}
	case 'h':
	{
		_tmp = NH;
		break;
	}
	case 'l':
	{
		_tmp = NL;
		break;
	}
	case 'n':
	{
		_tmp = NN;
		break;
	}
	case 'o':
	{
		_tmp = NO;
		break;
	}
	case 'p':
	{
		_tmp = NP;
		break;
	}
	case 'r':
	{
		_tmp = NR;
		break;
	}
	case 's':
	{
		_tmp = NS;
		break;
	}
	case 't':
	{
		_tmp = NT;
		break;
	}
	case 'u':
	{
		_tmp = NU;
		break;
	}
	case 'y':
	{
		_tmp = NY;
		break;
	}
	case '-':
	{
		_tmp = NDASH;
		break;
	}
	default:
	{
		_tmp = N_NAN;
		break;
	}
	}

	if(point == 1){
		_tmp ^= DP;
	}

	return _tmp;
}

/**
 * HC595WriteDigit
 * Write decimal digit.
 *
 * 	Input:
 * 		uint8_t digit Decimal gigit to disply.
 * 		bool point Display point or not.
 */
void HC595WriteDigit(uint8_t digit, bool point)
{
	HC595Write(HC595GetDigitMask(digit, point));
}

/**
 * HC595WriteChar
 * Write character.
 *
 * 	Input:
 * 		uint8_t digit Character to disply.
 * 		bool point Display point or not.
 */
void HC595WriteChar(uint8_t ch, bool point)
{
	HC595Write(HC595GetCharMask(ch, point));
}

#endif /* SEVEN_SEG_H_ */
