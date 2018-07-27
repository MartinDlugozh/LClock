/*
 * iv3a_display.h
 *
 *  Created on: Jul 25, 2018
 *      Author: Dr. Saldon
 */

#ifndef IV3A_DISPLAY_H_
#define IV3A_DISPLAY_H_

#define BLINK_HALFPERIOD_MS 1000

#define IV3A_GRID_PORT   	PORTC		// PC2..7
#define IV3A_GRID_DDR    	DDRC

#define GRID_HH				PC2			// 0
#define GRID_HL				PC3
#define GRID_MH				PC4
#define GRID_ML				PC5
#define GRID_SH				PC6
#define GRID_SL				PC7			// 5

#include <stdint.h>
#include "seven_seg.h"

typedef enum _digitMode{
	MODE_OFF = 0,				// indication is off
	MODE_ON = 1,				// display decimal digit
	MODE_BLINKING_ON,			// display blinking decimal digit (on halfperiod)
	MODE_BLINKING_OFF,			// display blinking decimal digit (off halfperiod)
	MODE_OFF_POINT,				// display only point
	MODE_ON_POINT,				// display decimal digit with point
	MODE_BLINKING_POINT_ON,		// display decimal digit with blinking point (on halfperiod)
	MODE_BLINKING_POINT_OFF,	// display decimal digit with blinking point (off halfperiod)
	MODE_CHAR,					// display character
	MODE_CHAR_BLINKING_ON,		// display blinking character (on halfperiod)
	MODE_CHAR_BLINKING_OFF,		// display blinking character (off halfperiod)
	MODE_ENUM_END = 0xFF
}digitMode_t;

typedef struct _iv3a{
	uint8_t			foo;
	digitMode_t		mode;
	uint8_t			mask;
} iv3a_t;

#define IV3A_HH				0
#define IV3A_HL				1
#define IV3A_MH				2
#define IV3A_ML				3
#define IV3A_SH				4
#define IV3A_SL				5

iv3a_t 		iv3a[6];
uint8_t 	iv3a_loop_pointer = 0;
uint16_t 	iv3a_blink_timer = 0;

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
	case MODE_BLINKING_ON:
	{
		if((millis()-iv3a_blink_timer) >= BLINK_HALFPERIOD_MS){
			iv3a_blink_timer = millis();
			iv3a->mode = MODE_BLINKING_OFF;
		}else{
			_tmp = HC595GetDigitMask(iv3a->foo, 0);
		}

		break;
	}
	case MODE_BLINKING_OFF:
	{
		if((millis()-iv3a_blink_timer) >= BLINK_HALFPERIOD_MS){
			iv3a_blink_timer = millis();
			iv3a->mode = MODE_BLINKING_ON;
			_tmp = HC595GetDigitMask(iv3a->foo, 0);
		}
		break;
	}
	case MODE_OFF_POINT:
	{
		_tmp = (N_NAN & DP);
		break;
	}
	case MODE_ON_POINT:
	{
		HC595GetDigitMask(iv3a->foo, 1);
		break;
	}
	case MODE_BLINKING_POINT_ON:
	{
		if((millis()-iv3a_blink_timer) >= BLINK_HALFPERIOD_MS){
			iv3a_blink_timer = millis();
			iv3a->mode = MODE_BLINKING_POINT_OFF;
			_tmp = HC595GetDigitMask(iv3a->foo, 0);
		}else{
			_tmp = HC595GetDigitMask(iv3a->foo, 1);
		}
		break;
	}
	case MODE_BLINKING_POINT_OFF:
	{
		if((millis()-iv3a_blink_timer) >= BLINK_HALFPERIOD_MS){
			iv3a_blink_timer = millis();
			iv3a->mode = MODE_BLINKING_POINT_ON;
			_tmp = HC595GetDigitMask(iv3a->foo, 1);
		}else{
			_tmp = HC595GetDigitMask(iv3a->foo, 0);
		}
		break;
	}
	case MODE_CHAR:
	{
		_tmp = HC595GetCharMask(iv3a->foo, 0);
		break;
	}
	case MODE_CHAR_BLINKING_ON:
	{
		if((millis()-iv3a_blink_timer) >= BLINK_HALFPERIOD_MS){
			iv3a_blink_timer = millis();
			iv3a->mode = MODE_CHAR_BLINKING_OFF;
		}else{
			_tmp = HC595GetCharMask(iv3a->foo, 0);
		}
		break;
	}
	case MODE_CHAR_BLINKING_OFF:
	{
		if((millis()-iv3a_blink_timer) >= BLINK_HALFPERIOD_MS){
			iv3a_blink_timer = millis();
			iv3a->mode = MODE_CHAR_BLINKING_ON;
			_tmp = HC595GetCharMask(iv3a->foo, 0);
		}
		break;
	}
	default:
		break;
	}

	iv3a->mask = _tmp;
}

void IV3aDispLoop(void){
	switch(iv3a_loop_pointer){
	case 0:	// HH
	{
		IV3A_GRID_PORT |= (1 << PC2);			// OFF previous grid (now all gids are OFF)
		IV3aHC595UpdMask(&iv3a[IV3A_HH]);			// update output bitmask (anodes)
		HC595Write(iv3a[IV3A_HH].mask);			// write output bitmask to 74HC595 shifting register
		IV3A_GRID_PORT &= (~(1 << PC7));			// ON next grid
		break;
	}
	case 1:	// HL
	{
		IV3A_GRID_PORT |= (1 << PC7);
		IV3aHC595UpdMask(&iv3a[IV3A_HL]);
		HC595Write(iv3a[IV3A_HL].mask);
		IV3A_GRID_PORT &= (~(1 << PC6));
		break;
	}
	case 2:	// MH
	{
		IV3A_GRID_PORT |= (1 << PC6);
		IV3aHC595UpdMask(&iv3a[IV3A_MH]);
		HC595Write(iv3a[IV3A_MH].mask);
		IV3A_GRID_PORT &= (~(1 << PC5));
		break;
	}
	case 3:	// ML
	{
		IV3A_GRID_PORT |= (1 << PC5);
		IV3aHC595UpdMask(&iv3a[IV3A_ML]);
		HC595Write(iv3a[IV3A_ML].mask);
		IV3A_GRID_PORT &= (~(1 << PC4));
		break;
	}
	case 4:	// SH
	{
		IV3A_GRID_PORT |= (1 << PC4);
		IV3aHC595UpdMask(&iv3a[IV3A_SH]);
		HC595Write(iv3a[IV3A_SH].mask);
		IV3A_GRID_PORT &= (~(1 << PC3));
		break;
	}
	case 5:	// SL
	{
		IV3A_GRID_PORT |= (1 << PC3);
		IV3aHC595UpdMask(&iv3a[IV3A_SL]);
		HC595Write(iv3a[IV3A_SL].mask);
		IV3A_GRID_PORT &= (~(1 << PC2));
		iv3a_loop_pointer = 0xFF;				// reset loop pointer
		break;
	}
	default:
		break;
	}

	iv3a_loop_pointer++;
}

#endif /* IV3A_DISPLAY_H_ */