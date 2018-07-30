/*
 * encoder32.h
 *
 * Created: 30.07.2018 8:26:48
 *  Author: Dr. Saldon
 */ 


#ifndef ENCODER32_H_
#define ENCODER32_H_

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>

void encoder_init()
{
	cli(); 							// disable interrupts
	DDRD &= ~((1<<PD2)|(1<<PD3)); 	// input
	PORTD |= ((1<<PD2)|(1<<PD3));	// pullup (if no hardware pullup resistors)
	MCUCR|=(1<<ISC01); 				// MCU Control and Status Register -> Trigger INT0 on falling edge
	GICR|=(1<<INT0);   				// General Interrupt Control Register -> Enable INT0
	sei();							// enable interrupts
}

ISR(INT0_vect)
{
	GIFR|=(1<<INTF0); 			// General Interrupt Flag Register -> Clear INT0 flag
	if(PIND & (1<<PD3)){		// if(digitalRead(3)){
		encoder_on_dec();
		}else{
		encoder_on_inc();
	}
	//Serial.println(encoder_counter);	// debug only
}

#endif /* ENCODER32_H_ */