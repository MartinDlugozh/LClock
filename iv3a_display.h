/*
 * iv3a_display.h
 *
 *  Created on: Jul 25, 2018
 *      Author: Dr. Saldon
 */

#ifndef IV3A_DISPLAY_H_
#define IV3A_DISPLAY_H_

#define BLINK_HALFPERIOD_MS			1000
#define ADJ_BLINK_HALFPERIOD_MS		250

#define IV3A_GRID_PORT   	PORTC		// PC2..7
#define IV3A_GRID_DDR    	DDRC

#define GRID_HH				PC2			// 5
#define GRID_HL				PC3			// 4
#define GRID_MH				PC4			// 3
#define GRID_ML				PC5			// 2
#define GRID_SH				PC6			// 1
#define GRID_SL				PC7			// 0

#include <stdint.h>
#include "seven_seg.h"

typedef enum _digitMode{
	MODE_OFF = 0,				// indication is off
	MODE_ON = 1,				// display decimal digit
	MODE_OFF_POINT,				// display only point
	MODE_ON_POINT,				// display decimal digit with point
	MODE_CHAR,					// display character
	MODE_ENUM_END = 0xFF
}digitMode_t;

typedef struct _iv3a{
	uint8_t			foo;
	digitMode_t		mode;
	uint8_t			mask;
} iv3a_t;

#define IV3A_SL				0
#define IV3A_SH				1
#define IV3A_ML				2
#define IV3A_MH				3
#define IV3A_HL				4
#define IV3A_HH				5

iv3a_t 		iv3a[6];
uint8_t 	iv3a_loop_pointer = 0;
uint32_t	point_blink_timer = 0;
uint32_t	digit_blink_timer = 0;
uint8_t		digit_blink_flag = 0;
uint8_t		point_blink_flag = 0;

void breakNumber2(uint8_t n, uint8_t *h, uint8_t *l)
{
	*h = (uint8_t)(n/10);
	*l = (uint8_t)(n%10);
}

void breakSNumber2(int8_t n, uint8_t *h, uint8_t *l, uint8_t *s)
{
	int8_t _tmp = n;
	if(n < 0){
		*s = (uint8_t)('-');
		_tmp *= (-1);
	}else{
		*s = 0xFF;	// NAN
	}
	*h = (uint8_t)(_tmp/10);
	*l = (uint8_t)(_tmp%10);
}

void breakNumber4(uint16_t n, uint8_t *hh, uint8_t *hl, uint8_t *lh, uint8_t *ll)
{
	uint16_t _tmp;
	*hh = (uint8_t)(n/1000);
	_tmp = (uint16_t)(n%1000);
	*hl = (uint8_t)(_tmp/100);
	_tmp = (uint16_t)(_tmp%100);
	*lh = (uint8_t)(_tmp/10);
	*ll = (uint8_t)(_tmp%10);
}

void IV3aInit(void){
	IV3A_GRID_DDR|=((1<<GRID_HH)|(1<<GRID_HL)|
					(1<<GRID_MH)|(1<<GRID_ML)|
					(1<<GRID_SH)|(1<<GRID_SL)); // set grid pins as outputs
	IV3A_GRID_PORT|=((1<<GRID_HH)|(1<<GRID_HL)|
					(1<<GRID_MH)|(1<<GRID_ML)|
					(1<<GRID_SH)|(1<<GRID_SL));				// clear grid control area (set all bases high to disable indication)
	HC595Init();								// init SN74HC595 hsifting register
}

void IV3aHC595UpdMask(iv3a_t *iv3a){
	uint8_t _tmp = 0;

	switch(iv3a->mode){
	case MODE_OFF:
	{
		break;
	}
	case MODE_ON:
	{
		_tmp = HC595GetDigitMask(iv3a->foo, 0);
		break;
	}
	case MODE_OFF_POINT:
	{
		_tmp = (N_NAN & DP);
		break;
	}
	case MODE_ON_POINT:
	{
		_tmp = HC595GetDigitMask(iv3a->foo, 1);
		break;
	}
	case MODE_CHAR:
	{
		_tmp = HC595GetCharMask((char)(iv3a->foo), 0);
		break;
	}
	default:
		break;
	}

	iv3a->mask = _tmp;
}

void IV3aDispLoop(void){
	switch(iv3a_loop_pointer){
	case 0:	// SL
	{
		IV3A_GRID_PORT |= (1 << GRID_HH);			// OFF previous grid (now all gids are OFF)
		IV3aHC595UpdMask(&iv3a[IV3A_SL]);			// update output bitmask (anodes)
		HC595Write(iv3a[IV3A_SL].mask);			// write output bitmask to 74HC595 shifting register
		IV3A_GRID_PORT &= (~(1 << GRID_SL));			// ON next grid
		break;
	}
	case 1:	// SH
	{
		IV3A_GRID_PORT |= (1 << GRID_SL);
		IV3aHC595UpdMask(&iv3a[IV3A_SH]);
		HC595Write(iv3a[IV3A_SH].mask);
		IV3A_GRID_PORT &= (~(1 << GRID_SH));
		break;
	}
	case 2:	// ML
	{
		IV3A_GRID_PORT |= (1 << GRID_SH);
		IV3aHC595UpdMask(&iv3a[IV3A_ML]);
		HC595Write(iv3a[IV3A_ML].mask);
		IV3A_GRID_PORT &= (~(1 << GRID_ML));
		break;
	}
	case 3:	// MH
	{
		IV3A_GRID_PORT |= (1 << GRID_ML);
		IV3aHC595UpdMask(&iv3a[IV3A_MH]);
		HC595Write(iv3a[IV3A_MH].mask);
		IV3A_GRID_PORT &= (~(1 << GRID_MH));
		break;
	}
	case 4:	// HL
	{
		IV3A_GRID_PORT |= (1 << GRID_MH);
		IV3aHC595UpdMask(&iv3a[IV3A_HL]);
		HC595Write(iv3a[IV3A_HL].mask);
		IV3A_GRID_PORT &= (~(1 << GRID_HL));
		break;
	}
	case 5:	// HH
	{
		IV3A_GRID_PORT |= (1 << GRID_HL);
		IV3aHC595UpdMask(&iv3a[IV3A_HH]);
		HC595Write(iv3a[IV3A_HH].mask);
		IV3A_GRID_PORT &= (~(1 << GRID_HH));
		iv3a_loop_pointer = 0xFF;				// reset loop pointer
		break;
	}
	default:
		break;
	}

	iv3a_loop_pointer++;
}

#endif /* IV3A_DISPLAY_H_ */
