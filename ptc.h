/*
 * ptc.h
 *
 * Power Transformer Control 
 *
 * Created		: 30.07.2018 8:30:34
 * Last modified: 03.08.2018 8:50:15
 * Author		: Dr. Saldon
 */ 


#ifndef PTC_H_
#define PTC_H_

#ifndef F_CPU
	#define F_CPU 16000000UL
#endif

#define TIM_DT 	(uint8_t)160	// OCR2 value for dead time: ((F_CPU / 120000UL) / 8)-1 (255)
#define TIM_PU 	(uint8_t)4 //16		// OCR2 value for pulse: ((F_CPU / 500000UL) / 8)-1 (6)

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>

volatile bool ab_flag = false;			// push-pull on-sequence flag
volatile uint8_t pls_counter = 0;		// push-pull ISR breaking counter (DT only)


void power_init(void){
	cli();
	// PD4 - MOSFET B
	// PD5 - MOSFET A
	PORTD = 0; // set pull-down for PORTD
	DDRD |= ((1<<PD4)|(1<<PD5)); // set PD4 and PD5 as outputs

	// Configure power transformer control timer (Timer2)
	TCCR2 = 0; // reset timer
	TCCR2 = (1<<WGM21)|(1<<CS20); // set CTC WGM on Timer2 and prescaller = 8

	OCR2 = TIM_PU;	// srt initial comparison vlaue
	TIMSK = 0;		// reset TIMSK
	TIMSK = (1<<OCIE2);	// enable on compare interrupt
	sei();
}

/**
 * Timer2 on compare interrupt handler
 * 
 * Используется для формирования комплементраного ШИМ на 
 * PD4 и PD5 с dead-time для управления ключами трансформатора.
 */
ISR(TIMER2_COMP_vect){
	cli();
	if(OCR2 == TIM_DT){
		PORTD &= ~(1<<PD4);
		PORTD &= ~(1<<PD5);
		if(pls_counter >= 2){
			OCR2 = TIM_PU;
			pls_counter = 0;
		}
		pls_counter++;
	}else{
		if (ab_flag == true){
			PORTD |= (1<<PD5);
		}else{
			PORTD |= (1<<PD4);
		}
		OCR2 = TIM_DT;
		ab_flag = !ab_flag;
	}
	sei();
}

#endif /* PTC_H_ */