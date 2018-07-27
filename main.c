/*
 * ATMega32_LCC.c
 *
 * Created: 11.07.2018 9:45:56
 * Author : Dr. Saldon
 */ 

#define F_CPU 16000000UL

#define TIM_DT ((F_CPU / 55556UL) / 8)-1		// 55556UL
#define TIM_PU ((F_CPU / 500000UL) / 8)-1

#define PERIOD_333HZ_MS		2
#define PERIOD_50HZ_MS 		20
#define PERIOD_1HZ_MS 		1000

#define SELFTEST_ENABLE		1

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>
#include <time.h>

#include <stdbool.h>

#include "millis.h"
#include "iv3a_display.h"
#include "i2c.h"
#include "rtc3231.h"
#include "timekeeper.h"

volatile bool dte_flag = false;		// push-pull dead-time flag

// Main loop timers
struct {
	volatile uint32_t loop_333Hz;	// anode switching loop (1ms period)
	volatile uint32_t loop_50Hz;		// buttons reading loop	(20 ms period)
	volatile uint32_t loop_1Hz;		// time updating loop (1000ms period)
//	volatile uint8_t btn_block;		// buttons blocking timer (200ms period)
//	volatile uint8_t buzzer;		// alarm buzzer update timer
}sys_timer;

#if(SELFTEST_ENABLE == 1)
	uint8_t selftest_counter = 0;		// debug only
#endif

typedef enum _dispMode{
	DMODE_TIME = 0,
	DMODE_DATE,
	DMODE_ALARM,
	DMODE_TEMPERATURE_A,
	DMODE_TEMPERATURE_B,
	DMODE_PRESSURE,
	DMODE_OFF = 0xFE,
	DMODE_ENUM_END = 0xFF
} dispMode_t;

uint8_t disp_mode = 0;				// time/date/alarm/temperature/pressure

void breakNumber(uint8_t n, uint8_t *h, uint8_t *l)
{
	*h = (uint8_t)(n/10);
	*l = (uint8_t)(n%10);
}

void dispSetMode(dispMode_t mode){
	disp_mode = mode;
	switch(mode){
	case DMODE_TIME:
	{
		iv3a[IV3A_HH].mode = MODE_ON;
		iv3a[IV3A_MH].mode = MODE_ON;
		iv3a[IV3A_SH].mode = MODE_ON;
		iv3a[IV3A_HL].mode = MODE_BLINKING_POINT_ON;
		iv3a[IV3A_ML].mode = MODE_BLINKING_POINT_ON;
		iv3a[IV3A_SL].mode = MODE_ON; // ? =)

		break;
	}
	case DMODE_DATE:
	{
		iv3a[IV3A_HH].mode = MODE_ON;
		iv3a[IV3A_MH].mode = MODE_ON;
		iv3a[IV3A_SH].mode = MODE_ON;
		iv3a[IV3A_HL].mode = MODE_BLINKING_POINT_ON;
		iv3a[IV3A_ML].mode = MODE_BLINKING_POINT_ON;
		iv3a[IV3A_SL].mode = MODE_ON; // ? =)

		break;
	}
	case DMODE_ALARM:
	{
		iv3a[IV3A_HH].mode = MODE_ON;
		iv3a[IV3A_MH].mode = MODE_ON;
		iv3a[IV3A_SH].mode = MODE_ON;
		iv3a[IV3A_HL].mode = MODE_BLINKING_POINT_ON;
		iv3a[IV3A_ML].mode = MODE_BLINKING_POINT_ON;
		iv3a[IV3A_SL].mode = MODE_ON; // ? =)

		break;
	}
	case DMODE_TEMPERATURE_A:
	{
		iv3a[IV3A_HH].mode = MODE_ON;
		iv3a[IV3A_MH].mode = MODE_ON;
		iv3a[IV3A_SH].mode = MODE_ON;
		iv3a[IV3A_HL].mode = MODE_ON;
		iv3a[IV3A_ML].mode = MODE_CHAR;
		iv3a[IV3A_SL].mode = MODE_CHAR; // ? =)

		break;
	}
	case DMODE_TEMPERATURE_B:
	{
		iv3a[IV3A_HH].mode = MODE_ON;
		iv3a[IV3A_MH].mode = MODE_ON;
		iv3a[IV3A_SH].mode = MODE_ON;
		iv3a[IV3A_HL].mode = MODE_ON;
		iv3a[IV3A_ML].mode = MODE_CHAR;
		iv3a[IV3A_SL].mode = MODE_CHAR; // ? =)

		break;
	}
	case DMODE_PRESSURE:
	{
		iv3a[IV3A_HH].mode = MODE_ON;
		iv3a[IV3A_MH].mode = MODE_ON;
		iv3a[IV3A_SH].mode = MODE_ON;
		iv3a[IV3A_HL].mode = MODE_ON;
		iv3a[IV3A_ML].mode = MODE_CHAR;
		iv3a[IV3A_SL].mode = MODE_CHAR; // ? =)

		break;
	}
	default:
		break;
	}
}

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

/**
 * 333 Hz loop 
 * IV-3A display update only
 */
void loop_333Hz(void){
	IV3aDispLoop();
}

/**
 * 50 Hz loop
 *
 */
void loop_50Hz(void){
	// Обработка кнопок
}

/**
 * 1 Hz loop
 *
 */
void loop_1Hz(void){

#if(SELFTEST_ENABLE == 1)
	if(selftest_counter < 10){
		iv3a[IV3A_HH].foo = selftest_counter++;
	}else if(selftest_counter < 20){
		iv3a[IV3A_HL].foo = (selftest_counter++ - 10);
	}else if(selftest_counter < 30){
		iv3a[IV3A_MH].foo = (selftest_counter++ - 20);
	}else if(selftest_counter < 40){
		iv3a[IV3A_ML].foo = (selftest_counter++ - 30);
	}else if(selftest_counter < 50){
		iv3a[IV3A_SH].foo = (selftest_counter++ - 40);
	}else if(selftest_counter < 60){
		iv3a[IV3A_SL].foo = (selftest_counter++ - 50);
	}else if(selftest_counter == 60){
		dispSetMode(DMODE_TIME);		// после селфтеста переключаемся на отображение времени
		selftest_counter++;				// делаем заглушку (желательно выбросить из релизной сборки)
	}
#endif
	
	update_time();	// процедура обновлеиня времени часов должна вызываться каджую секунду!

	switch(disp_mode){
		case DMODE_TIME:
		{
			breakNumber(now_time.hour, &(iv3a[IV3A_HH].foo), &(iv3a[IV3A_HL].foo));
			breakNumber(now_time.min, &(iv3a[IV3A_MH].foo), &(iv3a[IV3A_ML].foo));
			breakNumber(now_time.sec, &(iv3a[IV3A_SH].foo), &(iv3a[IV3A_SL].foo));
			break;
		}
		case DMODE_DATE:
		{
			breakNumber(now_date.day, &(iv3a[IV3A_HH].foo), &(iv3a[IV3A_HL].foo));
			breakNumber(now_date.month, &(iv3a[IV3A_MH].foo), &(iv3a[IV3A_ML].foo));
			breakNumber((now_date.year-30), &(iv3a[IV3A_SH].foo), &(iv3a[IV3A_SL].foo));
			break;
		}
		case DMODE_ALARM:
		{
			iv3a[IV3A_HH].mode = MODE_ON;
			iv3a[IV3A_MH].mode = MODE_ON;
			iv3a[IV3A_SH].mode = MODE_ON;
			iv3a[IV3A_HL].mode = MODE_BLINKING_POINT_ON;
			iv3a[IV3A_ML].mode = MODE_BLINKING_POINT_ON;
			iv3a[IV3A_SL].mode = MODE_ON; // ? =)

			break;
		}
		case DMODE_TEMPERATURE_A:
		{
			iv3a[IV3A_HH].mode = MODE_ON;
			iv3a[IV3A_MH].mode = MODE_ON;
			iv3a[IV3A_SH].mode = MODE_ON;
			iv3a[IV3A_HL].mode = MODE_ON;
			iv3a[IV3A_ML].mode = MODE_CHAR;
			iv3a[IV3A_SL].mode = MODE_CHAR; // ? =)

			break;
		}
		case DMODE_TEMPERATURE_B:
		{
			iv3a[IV3A_HH].mode = MODE_ON;
			iv3a[IV3A_MH].mode = MODE_ON;
			iv3a[IV3A_SH].mode = MODE_ON;
			iv3a[IV3A_HL].mode = MODE_ON;
			iv3a[IV3A_ML].mode = MODE_CHAR;
			iv3a[IV3A_SL].mode = MODE_CHAR; // ? =)

			break;
		}
		case DMODE_PRESSURE:
		{
			iv3a[IV3A_HH].mode = MODE_ON;
			iv3a[IV3A_MH].mode = MODE_ON;
			iv3a[IV3A_SH].mode = MODE_ON;
			iv3a[IV3A_HL].mode = MODE_ON;
			iv3a[IV3A_ML].mode = MODE_CHAR;
			iv3a[IV3A_SL].mode = MODE_CHAR; // ? =)

			break;
		}
		default:
		break;
	}
}

void power_init(void){
	cli();
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
	sei();
}

int main(void)
{
	power_init();		// start complementary pwm on timer2
	millis_init();		// start system millis() on timer0
	i2c_init();			// start TWI
	rtc3231_init();		// start DS3231 RTC
	IV3aInit();			// start IV-3A luminescent display

	sys_timer.loop_333Hz = millis();				// initialize system timer
	sys_timer.loop_50Hz = sys_timer.loop_333Hz;
	sys_timer.loop_1Hz = sys_timer.loop_333Hz;
	//	sys_timer.btn_block= millis();
	//	sys_timer.buzzer = millis();

#if(SELFTEST_ENABLE == 1)
	for(uint8_t i = 0; i < 6; i++){					// initialize on-selftest mode
		iv3a[i].mode = MODE_ON;
		iv3a[i].foo = selftest_counter;
	}
#endif

	// Main loop
    for(;;)
    {
		if((millis() - sys_timer.loop_333Hz) >= PERIOD_333HZ_MS){
			sys_timer.loop_333Hz = millis();
			loop_333Hz();
		}
		if((millis() - sys_timer.loop_50Hz) >= PERIOD_50HZ_MS){
			sys_timer.loop_50Hz = millis();
			loop_50Hz();
		}
		if((millis() - sys_timer.loop_1Hz) >= PERIOD_1HZ_MS){
			sys_timer.loop_1Hz = millis();
			loop_1Hz();
		}
    }
}
