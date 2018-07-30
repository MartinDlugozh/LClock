/*
 * ATMega32_LCC.c
 *
 * Created: 11.07.2018 9:45:56
 * Author : Dr. Saldon
 */ 

#define F_CPU 16000000UL

#define PERIOD_333HZ_MS		2
#define PERIOD_50HZ_MS 		20
#define PERIOD_1HZ_MS 		1000

#define SELFTEST_ENABLE		0

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>
#include <time.h>
#include <delay.h>

#include <stdbool.h>

#include "millis.h"
#include "ptc.h"
#include "iv3a_display.h"
#include "i2c.h"
#include "rtc3231.h"
#include "timekeeper.h"
#include "ds18b20.h"

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

typedef enum _dispMode{			// indication modes ("main menu")
	DMODE_TIME = 0,				// 0..5 - main modes
	DMODE_DATE = 1,
	DMODE_ALARM = 2,
	DMODE_TEMPERATURE_A = 3,
	DMODE_TEMPERATURE_B = 4,
	DMODE_PRESSURE = 5,
	DMODE_CTIME,				// other - for dispSetMode() switch statement optimization 
	DMODE_CTEMPERATURE,
	DMODE_OFF = 0xFE,
	DMODE_ENUM_END = 0xFF
} dispMode_t;

typedef enum _setMode{			// adjustment modes
	SMODE_NO = 0,				// normal indication mode (no adjustment)
	SMODE_SS,					// adjust second/year
	SMODE_MM,					// adjust minute/month
	SMODE_HH,					// adjust hour/day
	SMODE_TEMP,					// adjust temperature (A or B)
	SMODE_PRESS,				// adjust pressure
	SMODE_ENUM_END = 0xFF
} setMode_t;

uint8_t disp_mode = 0;			// time/date/alarm/temperature/pressure
uint8_t set_mode = 0;			// adjustment mode

int8_t temp_a = 0;				// temperature from DS18B20 (external)
int8_t temp_b = 0;				// temperature from BMP180 (internal)
uint16_t pressure = 0;			// barometric pressure from BMP180

/**
 * Set indication mode
 * 
 * iv3a[n].mode manager. Used for updating tube control structures
 * according to indication and adjustment modes.
 *	Input:
 *		dispMode_t dmode - indication mode;
 *		setMode_t smode - adjustment mode.
 *	Return:
 *		none.
 */
void dispSetMode(dispMode_t dmode, setMode_t smode){
	dispMode_t _dmode;
	disp_mode = dmode;
	set_mode = smode;

	if((dmode == DMODE_TIME) ||
		(dmode == DMODE_DATE) ||
		(dmode == DMODE_ALARM)){
		_dmode = DMODE_CTIME;
	}else if((dmode == DMODE_TEMPERATURE_A) || (dmode == DMODE_TEMPERATURE_B)){
		_dmode = DMODE_CTEMPERATURE;
	}else{
		_dmode = dmode;
	}

	switch(_dmode){
	case DMODE_CTIME:
	{
		switch(smode){
		case SMODE_NO:
		{
			iv3a[IV3A_HH].mode = MODE_ON;
			iv3a[IV3A_HL].mode = MODE_BLINKING_POINT_ON;
			iv3a[IV3A_MH].mode = MODE_ON;
			iv3a[IV3A_ML].mode = MODE_BLINKING_POINT_ON;
			iv3a[IV3A_SH].mode = MODE_ON;				
			iv3a[IV3A_SL].mode = MODE_ON;
			break;
		}
		case SMODE_SS:
		{
			iv3a[IV3A_HH].mode = MODE_ON;
			iv3a[IV3A_HL].mode = MODE_ON_POINT;
			iv3a[IV3A_MH].mode = MODE_ON;
			iv3a[IV3A_ML].mode = MODE_ON_POINT;
			iv3a[IV3A_SH].mode = MODE_BLINKING_ON;
			iv3a[IV3A_SL].mode = MODE_BLINKING_ON;
			break;
		}
		case SMODE_MM:
		{
			iv3a[IV3A_HH].mode = MODE_ON;
			iv3a[IV3A_HL].mode = MODE_BLINKING_POINT_ON;
			iv3a[IV3A_MH].mode = MODE_BLINKING_ON;
			iv3a[IV3A_ML].mode = MODE_BLINKING_PON;
			iv3a[IV3A_SH].mode = MODE_ON;
			iv3a[IV3A_SL].mode = MODE_ON;
			break;
		}
		case SMODE_HH:
		{
			iv3a[IV3A_HH].mode = MODE_BLINKING_ON;
			iv3a[IV3A_HL].mode = MODE_BLINKING_PON;
			iv3a[IV3A_MH].mode = MODE_ON;
			iv3a[IV3A_ML].mode = MODE_BLINKING_POINT_ON;
			iv3a[IV3A_SH].mode = MODE_ON;
			iv3a[IV3A_SL].mode = MODE_ON;
			break;
		}
		default:
			break;
		}
		break;
	}
	case DMODE_CTEMPERATURE:
	{
		switch(smode){
		case SMODE_NO:
		{
			iv3a[IV3A_HH].mode = MODE_CHAR;
			iv3a[IV3A_HL].mode = MODE_CHAR;
			iv3a[IV3A_MH].mode = MODE_ON;
			iv3a[IV3A_ML].mode = MODE_ON;
			iv3a[IV3A_SH].mode = MODE_CHAR;
			iv3a[IV3A_SL].mode = MODE_CHAR;
			break;
		}
		case SMODE_TEMP:
		{
			iv3a[IV3A_HH].mode = MODE_BLINKING_ON;
			iv3a[IV3A_MH].mode = MODE_BLINKING_ON;
			iv3a[IV3A_SH].mode = MODE_CHAR;
			iv3a[IV3A_HL].mode = MODE_BLINKING_ON;
			iv3a[IV3A_ML].mode = MODE_BLINKING_ON;
			iv3a[IV3A_SL].mode = MODE_CHAR;
			break;
		}
		default:
			break;
		}
		break;
	}
	case DMODE_PRESSURE:
	{ 
		switch(smode){
			case SMODE_NO:
			{
				iv3a[IV3A_HH].mode = MODE_ON;
				iv3a[IV3A_MH].mode = MODE_ON;
				iv3a[IV3A_SH].mode = MODE_CHAR;
				iv3a[IV3A_HL].mode = MODE_ON;
				iv3a[IV3A_ML].mode = MODE_ON;
				iv3a[IV3A_SL].mode = MODE_CHAR;
				break;
			}
			case SMODE_TEMP:
			{
				iv3a[IV3A_HH].mode = MODE_BLINKING_ON;
				iv3a[IV3A_MH].mode = MODE_BLINKING_ON;
				iv3a[IV3A_SH].mode = MODE_CHAR;
				iv3a[IV3A_HL].mode = MODE_BLINKING_ON;
				iv3a[IV3A_ML].mode = MODE_BLINKING_ON;
				iv3a[IV3A_SL].mode = MODE_CHAR;
				break;
			}
			default:
			break;
		}
		break;
	}
	default:
		break;
	}
}

void dispUpdate(){
	switch(disp_mode){
		case DMODE_TIME:
		{
			breakNumber2(rtc_time.hour, &(iv3a[IV3A_HH].foo), &(iv3a[IV3A_HL].foo));
			breakNumber2(rtc_time.min, &(iv3a[IV3A_MH].foo), &(iv3a[IV3A_ML].foo));
			breakNumber2(rtc_time.sec, &(iv3a[IV3A_SH].foo), &(iv3a[IV3A_SL].foo));
			break;
		}
		case DMODE_DATE:
		{
			breakNumber2(rtc_date.day, &(iv3a[IV3A_HH].foo), &(iv3a[IV3A_HL].foo));
			breakNumber2(rtc_date.month, &(iv3a[IV3A_MH].foo), &(iv3a[IV3A_ML].foo));
			breakNumber2((rtc_date.year-30), &(iv3a[IV3A_SH].foo), &(iv3a[IV3A_SL].foo));
			break;
		}
		case DMODE_ALARM:
		{
			breakNumber2(alarm_time.hour, &(iv3a[IV3A_HH].foo), &(iv3a[IV3A_HL].foo));
			breakNumber2(alarm_time.min, &(iv3a[IV3A_MH].foo), &(iv3a[IV3A_ML].foo));
			breakNumber2(alarm_time.sec, &(iv3a[IV3A_SH].foo), &(iv3a[IV3A_SL].foo));
			break;
		}
		case DMODE_TEMPERATURE_A:
		{
			breakSNumber2(temp_a, &(iv3a[IV3A_MH].foo), &(iv3a[IV3A_ML].foo), &(iv3a[IV3A_HL].foo));
			iv3a[IV3A_SH].foo = 'd'; iv3a[IV3A_SL].foo = 'c';
			break;
		}
		case DMODE_TEMPERATURE_B:
		{
			breakSNumber2(temp_b, &(iv3a[IV3A_MH].foo), &(iv3a[IV3A_ML].foo), &(iv3a[IV3A_HL].foo));
			iv3a[IV3A_SH].foo = 'd'; iv3a[IV3A_SL].foo = 'c';
			break;
		}
		case DMODE_PRESSURE:
		{
			breakNumber4(pressure, &(iv3a[IV3A_HH].foo), &(iv3a[IV3A_HL].foo), &(iv3a[IV3A_MH].foo), &(iv3a[IV3A_ML].foo));
			iv3a[IV3A_SH].foo = 'h'; iv3a[IV3A_SL].foo = 'p';
			break;
		}
		default:
		break;
	}
}

// encoder32 routine
void encoder_on_inc(void){
	switch(set_mode){
		case SMODE_NO:				// normal indication
		{
			if(++disp_mode >= 6){
				disp_mode = 0;
			}
			break;
		}
		case SMODE_SS:
		{
			switch(disp_mode){
				case DMODE_TIME:				// adjust clock seconds
				{
					now++;
					nowBreakTime(now, &rtc_time, &rtc_date);
					break;
				}
				case DMODE_DATE:				// adjust years
				{
					now += SECS_PER_YEAR;
					nowBreakTime(now, &rtc_time, &rtc_date);
					break;
				}
				case DMODE_ALARM:				// adjust alarm seconds
				{
					alarm++;
					nowBreakTime(alarm, &alarm_time, &alarm_date);
					break;
				}
				default:
					break;
			}
			break;
		}
		case SMODE_MM:
		{
			switch(disp_mode){
				case DMODE_TIME:				// adjust clock minutes
				{
					now += SECS_PER_MIN;
					nowBreakTime(now, &rtc_time, &rtc_date);
					break;
				}
				case DMODE_DATE:				// adjust months
				{
					now += SECS_PER_DAY*30;
					nowBreakTime(now, &rtc_time, &rtc_date);
					break;
				}
				case DMODE_ALARM:				// adjust alarm minutes
				{
					alarm += SECS_PER_MIN;
					nowBreakTime(alarm, &alarm_time, &alarm_date);
					break;
				}
				default:
				break;
			}
			break;
		}
		case SMODE_HH:
		{
			switch(disp_mode){
				case DMODE_TIME:				// adjust clock hours
				{
					now += SECS_PER_HOUR;
					nowBreakTime(now, &rtc_time, &rtc_date);
					break;
				}
				case DMODE_DATE:				// adjust days
				{
					now += SECS_PER_DAY;
					nowBreakTime(now, &rtc_time, &rtc_date);
					break;
				}
				case DMODE_ALARM:				// adjust alarm hours
				{
					alarm += SECS_PER_HOUR;
					nowBreakTime(alarm, &alarm_time, &alarm_date);
					break;
				}
				default:
				break;
			}
			break;
		}
		case SMODE_TEMP:
		{

			break;
		}
		case SMODE_PRESS:
		{

			break;
		}
		default:
		break;
	}

	dispUpdate();
}

// encoder32 routine
void encoder_on_dec(void){
	switch(set_mode){
		case SMODE_NO:				// normal indication
		{
			if(disp_mode > 0){
				disp_mode--;
			}else{
				disp_mode = 5;
			}
			break;
		}
		case SMODE_SS:
		{
			switch(disp_mode){
				case DMODE_TIME:				// adjust clock seconds
				{
					now--;
					nowBreakTime(now, &rtc_time, &rtc_date);
					break;
				}
				case DMODE_DATE:				// adjust years
				{
					now -= SECS_PER_YEAR;
					nowBreakTime(now, &rtc_time, &rtc_date);
					break;
				}
				case DMODE_ALARM:				// adjust alarm seconds
				{
					alarm--;
					nowBreakTime(alarm, &alarm_time, &alarm_date);
					break;
				}
				default:
				break;
			}
			break;
		}
		case SMODE_MM:
		{
			switch(disp_mode){
				case DMODE_TIME:				// adjust clock minutes
				{
					now -= SECS_PER_MIN;
					nowBreakTime(now, &rtc_time, &rtc_date);
					break;
				}
				case DMODE_DATE:				// adjust months
				{
					now -= SECS_PER_DAY*30;
					nowBreakTime(now, &rtc_time, &rtc_date);
					break;
				}
				case DMODE_ALARM:				// adjust alarm minutes
				{
					alarm -= SECS_PER_MIN;
					nowBreakTime(alarm, &alarm_time, &alarm_date);
					break;
				}
				default:
				break;
			}
			break;
		}
		case SMODE_HH:
		{
			switch(disp_mode){
				case DMODE_TIME:				// adjust clock hours
				{
					now -= SECS_PER_HOUR;
					nowBreakTime(now, &rtc_time, &rtc_date);
					break;
				}
				case DMODE_DATE:				// adjust days
				{
					now -= SECS_PER_DAY;
					nowBreakTime(now, &rtc_time, &rtc_date);
					break;
				}
				case DMODE_ALARM:				// adjust alarm hours
				{
					alarm -= SECS_PER_HOUR;
					nowBreakTime(alarm, &alarm_time, &alarm_date);
					break;
				}
				default:
				break;
			}
			break;
		}
		case SMODE_TEMP:
		{

			break;
		}
		case SMODE_PRESS:
		{

			break;
		}
		default:
		break;
	}

	dispUpdate();
}

#include "encoder32.h"

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
	// Buttons reading code
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
		dispSetMode(DMODE_TIME);		
		selftest_counter++;				
	}
#endif
	
	update_time(upload_flag, set_mode);	// процедура обновлеиня времени часов должна вызываться каджую секунду!
	dispUpdate();
}



int main(void)
{
	_delay_ms(500);		// pre-start delay (for BMP180 hardware initialization)
	power_init();		// start complementary pwm on timer2
	millis_init();		// start system millis() on timer0
	i2c_init();			// start TWI
	rtc3231_init();		// start DS3231 RTC

//	rtc_date.year = 48;
//	rtc_date.month = 7;
//	rtc_date.day = 27;
//	rtc_date.wday = 5;
//	rtc_time.hour = 18;
//	rtc_time.min = 0;
//	rtc_time.sec = 0;
//	rtc3231_write_date(&rtc_date);
//	rtc3231_write_time(&rtc_time);

	rtc3231_read_datetime(&rtc_time, &rtc_date);
	now = rtcMakeTime(&rtc_time, &rtc_date);
	rtc_update_timer = now;
	alarm = now;

	encoder_init();					// configure encoder ISR
	// start BMP180 pressure and temperature sensor
	ds18b20_init(&ds18b20, PA0);	// start DS18B20 temperature sensor

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
	dispSetMode(DMODE_TIME, SMODE_NO);

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