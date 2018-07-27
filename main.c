/*
 * ATMega32_LCC.c
 *
 * Created: 11.07.2018 9:45:56
 * Author : Dr. Saldon
 */ 

#define F_CPU 16000000UL

#define TIM_DT ((F_CPU / 55556UL) / 8)-1		// 55556UL
#define TIM_PU ((F_CPU / 500000UL) / 8)-1

#define UART_BUFFER_LEN 32

#define PERIOD_333HZ_MS		2
#define PERIOD_50HZ_MS 		20
#define PERIOD_1HZ_MS 		200

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>

#include <stdbool.h>

#include "millis.h"
#include "iv3a_display.h"

volatile bool dte_flag = false;		// push-pull dead-time flag

uint32_t tim = 0;
/**
 * Main loop timers
 */
struct {
	volatile uint32_t loop_333Hz;	// anode switching loop (1ms period)
	volatile uint32_t loop_50Hz;		// buttons reading loop	(20 ms period)
	volatile uint32_t loop_1Hz;		// time updating loop (1000ms period)
//	volatile uint8_t btn_block;		// buttons blocking timer (200ms period)
//	volatile uint8_t buzzer;		// alarm buzzer update timer
}timer;

uint8_t selftest_counter = 0;

/**
 * Timer2 on compare interrupt handler
 * 
 * Используется для формирования комплементраного ШИМ на 
 * PD4 и PD5 с dead-time для управления ключами трансформатора.
 */
ISR(TIMER2_COMP_vect){
	if(OCR2 == TIM_DT){
		PORTD &= ~(1<<PD4);
		PORTD &= ~(1<<PD5);
		OCR2 = TIM_PU;
	}else{
		if (dte_flag == true){
			PORTD |= (1<<PD4);
		}else{
			PORTD |= (1<<PD5);
		}
		OCR2 = TIM_DT;
		dte_flag = !dte_flag;
	}
}

void loop_333Hz(void){
	IV3aDispLoop();
}

void loop_50Hz(void){

}

void loop_1Hz(void){
	if(selftest_counter < 10){
		iv3a[IV3A_HH].foo= selftest_counter++;
	}else if(selftest_counter < 20){
		iv3a[IV3A_HL].foo= (selftest_counter++ - 10);
	}else if(selftest_counter < 30){
		iv3a[IV3A_MH].foo= (selftest_counter++ - 20);
	}else if(selftest_counter < 40){
		iv3a[IV3A_ML].foo= (selftest_counter++ - 30);
	}else if(selftest_counter < 50){
		iv3a[IV3A_SH].foo= (selftest_counter++ - 40);
	}else if(selftest_counter < 60){
		iv3a[IV3A_SL].foo= (selftest_counter++ - 50);
	}else{
		selftest_counter = 0;
	}
}

int main(void)
{
	cli();
	/************************************************/
	/*		  Power controler configuration			*/
	/************************************************/
	// PD4 - MOSFET B
	// PD5 - MOSFET A
	PORTD = 0; // set pull-down for PORTD
	DDRD = (1<<PD4)|(1<<PD5); // set PD4 and PD5 as outputs

	// Configure power transformer control timer (Timer2)
	TCCR2 = 0; // reset timer
	TCCR2 = (1<<WGM21)|(1<<CS21); // set CTC WGM on Timer2 and prescaller = 8

	OCR2 = TIM_PU;	// srt initial comparison vlaue
	TIMSK = 0;		// reset TIMSK
	TIMSK = (1<<OCIE2);	// enable on compare interrupt

	millis_init();

	/************************************************/
	/*		  		Display initialization			*/
	/************************************************/
	IV3aInit();

	timer.loop_333Hz = millis();
	timer.loop_50Hz = timer.loop_333Hz;
	timer.loop_1Hz = timer.loop_333Hz;
	//	timer.btn_block= millis();
	//	timer.buzzer = millis();
	for(uint8_t i = 0; i < 6; i++){
		iv3a[i].mode = MODE_ON;
		iv3a[i].foo = selftest_counter;
	}

    while (1) 
    {
		if((millis() - timer.loop_333Hz) >= PERIOD_333HZ_MS){
			timer.loop_333Hz = millis();
			loop_333Hz();
		}
		if((millis() - timer.loop_50Hz) >= PERIOD_50HZ_MS){
			timer.loop_50Hz = millis();
			loop_50Hz();
		}
		if((millis() - timer.loop_1Hz) >= PERIOD_1HZ_MS){
			timer.loop_1Hz = millis();
			loop_1Hz();
		}
    }
}
