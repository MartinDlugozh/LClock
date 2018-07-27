#ifndef MILLIS_H_
#define MILLIS_H_

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>

#ifndef F_CPU
	#define F_CPU 16000000UL
#endif

// Calculate the value needed for
// the CTC match value in OCR0.
#define SYS_CTC_MATCH_OVERFLOW ((F_CPU / 1000UL) / 64)-1 //	CTC_MATCH_OVERFLOW = ((F_CPU / DESIRED_FREQUENCY) / PRESCALLER)-1

volatile uint32_t milliseconds = 0;

ISR(TIMER0_COMP_vect){
	milliseconds++;
}

void millis_init()
{
	// Configure 1ms system timer
	TCCR0 = 0;
	TCCR0 = (1<<WGM01)|(1<<CS01)|(1<<CS00);		// CTC, prescaller = 64
	OCR0 = SYS_CTC_MATCH_OVERFLOW;
	TIMSK |= (1<<OCIE0);

	sei();		// Now enable global interrupts
}

uint32_t millis()
{
	uint32_t ms;
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		ms = milliseconds;
	}
	return ms;
}

void millis_reset()
{
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		milliseconds = 0;
	}
}

#endif /* MILLIS_H_ */