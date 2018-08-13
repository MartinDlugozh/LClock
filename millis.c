/*
 * millis.c
 *
 * Created: 13.08.2018 20:18:27
 *  Author: User
 */ 

 #include <avr/io.h>
 #include <avr/interrupt.h>
 #include <util/atomic.h>

 #include "millis.h"

 volatile uint32_t milliseconds = 0;

 ISR(TIMER0_COMP_vect){
	 milliseconds++;
 }

 void millis_init(void){
	 // Configure 1ms system timer
	 TCCR0 = 0;
	 TCCR0 = (1<<WGM01)|(1<<CS01)|(1<<CS00);		// CTC, prescaller = 64
	 OCR0 = SYS_CTC_MATCH_OVERFLOW;
	 TIMSK |= (1<<OCIE0);

	 sei();		// Now enable global interrupts
 }

 uint32_t millis(void){
	 uint32_t ms;
	 ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	 {
		 ms = milliseconds;
	 }
	 return ms;
 }

 void millis_reset(void){
	 ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	 {
		 milliseconds = 0;
	 }
 }