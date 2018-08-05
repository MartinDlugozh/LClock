/*
 * ATMega32_LCC.c
 *
 * Created: 11.07.2018 9:45:56
 * Author : Dr. Saldon
 */ 

#define F_CPU 16000000UL

#define PERIOD_333HZ_MS		2
#define PERIOD_50HZ_MS 		10
#define PERIOD_1HZ_MS 		1000

#define BLOCK_BTN() { btn_block = 1; sys_timer.btn_block = millis(); }

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
	volatile uint8_t btn_block;		// buttons blocking timer (200ms period)
//	volatile uint8_t buzzer;		// alarm buzzer update timer
}sys_timer;

volatile typedef enum _dispMode{			// indication modes ("main menu")
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

volatile typedef enum _setMode{			// adjustment modes
	SMODE_NO = 0,				// normal indication mode (no adjustment)
	SMODE_SS = 1,					// adjust second/year
	SMODE_MM = 2,					// adjust minute/month
	SMODE_HH = 3,					// adjust hour/day
	SMODE_TEMP,					// adjust temperature (A or B)
	SMODE_PRESS,				// adjust pressure
	SMODE_ENUM_END = 0xFF
} setMode_t;

volatile uint8_t disp_mode = 0;			// time/date/alarm/temperature/pressure
volatile uint8_t set_mode = 0;			// adjustment mode

int8_t temp_a = 0;				// temperature from DS18B20 (external)
int8_t temp_b = 0;				// temperature from BMP180 (internal)
uint16_t pressure = 0;			// barometric pressure from BMP180

volatile uint8_t btn_block = 0;

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
void dispUpdMode(dispMode_t dmode, setMode_t smode){
	dispMode_t _dmode;
	disp_mode = dmode;
	set_mode = smode;

	if((dmode == DMODE_TIME) ||
		(dmode == DMODE_DATE)){
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
			iv3a[IV3A_MH].mode = MODE_ON;
			iv3a[IV3A_SH].mode = MODE_ON;				
			iv3a[IV3A_SL].mode = MODE_ON;
			if(millis() >= point_blink_timer){
				point_blink_timer = millis() + BLINK_HALFPERIOD_MS;
				if(point_blink_flag == 1){
					iv3a[IV3A_HL].mode = MODE_ON_POINT;
					iv3a[IV3A_ML].mode = MODE_ON_POINT;
				}else{
					iv3a[IV3A_HL].mode = MODE_ON;
					iv3a[IV3A_ML].mode = MODE_ON;
				}
				point_blink_flag = !point_blink_flag;
			}
			break;
		}
		case SMODE_SS:
		{
			iv3a[IV3A_HH].mode = MODE_ON;
			iv3a[IV3A_HL].mode = MODE_ON_POINT;
			iv3a[IV3A_MH].mode = MODE_ON;
			iv3a[IV3A_ML].mode = MODE_ON_POINT;
			if(millis() >= digit_blink_timer){
				digit_blink_timer = millis() + ADJ_BLINK_HALFPERIOD_MS;
				if(digit_blink_flag == 1){
					iv3a[IV3A_SH].mode = MODE_ON;
					iv3a[IV3A_SL].mode = MODE_ON;
					}else{
					iv3a[IV3A_SH].mode = MODE_OFF;
					iv3a[IV3A_SL].mode = MODE_OFF;
				}
				digit_blink_flag = !digit_blink_flag;
			}
			break;
		}
		case SMODE_MM:
		{
			iv3a[IV3A_HH].mode = MODE_ON;
			iv3a[IV3A_HL].mode = MODE_ON_POINT;
			iv3a[IV3A_SH].mode = MODE_ON;
			iv3a[IV3A_SL].mode = MODE_ON;
			if(millis() >= digit_blink_timer){
				digit_blink_timer = millis() + ADJ_BLINK_HALFPERIOD_MS;
				if(digit_blink_flag == 1){
					iv3a[IV3A_MH].mode = MODE_ON;
					iv3a[IV3A_ML].mode = MODE_ON_POINT;
					}else{
					iv3a[IV3A_MH].mode = MODE_OFF;
					iv3a[IV3A_ML].mode = MODE_OFF;
				}
				digit_blink_flag = !digit_blink_flag;
			}
			break;
		}
		case SMODE_HH:
		{
			iv3a[IV3A_MH].mode = MODE_ON;
			iv3a[IV3A_ML].mode = MODE_ON_POINT;
			iv3a[IV3A_SH].mode = MODE_ON;
			iv3a[IV3A_SL].mode = MODE_ON;
			if(millis() >= digit_blink_timer){
				digit_blink_timer = millis() + ADJ_BLINK_HALFPERIOD_MS;
				if(digit_blink_flag == 1){
					iv3a[IV3A_HH].mode = MODE_ON;
					iv3a[IV3A_HL].mode = MODE_ON_POINT;
					}else{
					iv3a[IV3A_HH].mode = MODE_OFF;
					iv3a[IV3A_HL].mode = MODE_OFF;
				}
				digit_blink_flag = !digit_blink_flag;
			}
			break;
		}
		default:
			break;
		}
		break;
	}
	case DMODE_ALARM:
	{
		switch(smode){
			case SMODE_NO:
			{
				iv3a[IV3A_HH].mode = MODE_ON;
				iv3a[IV3A_HL].mode = MODE_ON_POINT;
				iv3a[IV3A_MH].mode = MODE_ON;
				iv3a[IV3A_ML].mode = MODE_ON_POINT;
				iv3a[IV3A_SH].mode = MODE_CHAR;
				iv3a[IV3A_SL].mode = MODE_CHAR;
				break;
			}
			case SMODE_SS:
			{
				iv3a[IV3A_HH].mode = MODE_ON;
				iv3a[IV3A_HL].mode = MODE_ON_POINT;
				iv3a[IV3A_MH].mode = MODE_ON;
				iv3a[IV3A_ML].mode = MODE_ON_POINT;
				if(millis() >= digit_blink_timer){
					digit_blink_timer = millis() + ADJ_BLINK_HALFPERIOD_MS;
					if(digit_blink_flag == 1){
						iv3a[IV3A_SH].mode = MODE_CHAR;
						iv3a[IV3A_SL].mode = MODE_CHAR;
						}else{
						iv3a[IV3A_SH].mode = MODE_OFF;
						iv3a[IV3A_SL].mode = MODE_OFF;
					}
					digit_blink_flag = !digit_blink_flag;
				}
				break;
			}
			case SMODE_MM:
			{
				iv3a[IV3A_HH].mode = MODE_ON;
				iv3a[IV3A_HL].mode = MODE_ON_POINT;
				iv3a[IV3A_SH].mode = MODE_CHAR;
				iv3a[IV3A_SL].mode = MODE_CHAR;
				if(millis() >= digit_blink_timer){
					digit_blink_timer = millis() + ADJ_BLINK_HALFPERIOD_MS;
					if(digit_blink_flag == 1){
						iv3a[IV3A_MH].mode = MODE_ON;
						iv3a[IV3A_ML].mode = MODE_ON_POINT;
						}else{
						iv3a[IV3A_MH].mode = MODE_OFF;
						iv3a[IV3A_ML].mode = MODE_OFF;
					}
					digit_blink_flag = !digit_blink_flag;
				}
				break;
			}
			case SMODE_HH:
			{
				iv3a[IV3A_MH].mode = MODE_ON;
				iv3a[IV3A_ML].mode = MODE_ON_POINT;
				iv3a[IV3A_SH].mode = MODE_CHAR;
				iv3a[IV3A_SL].mode = MODE_CHAR;
				if(millis() >= digit_blink_timer){
					digit_blink_timer = millis() + ADJ_BLINK_HALFPERIOD_MS;
					if(digit_blink_flag == 1){
						iv3a[IV3A_HH].mode = MODE_ON;
						iv3a[IV3A_HL].mode = MODE_ON_POINT;
						}else{
						iv3a[IV3A_HH].mode = MODE_OFF;
						iv3a[IV3A_HL].mode = MODE_OFF;
					}
					digit_blink_flag = !digit_blink_flag;
				}
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
			default:
			break;
		}
		break;
	}
	default:
		break;
	}
}

/**
 * Update display
 * 
 * iv3a[n].foo manager. Used for updating tube control structures
 * according to indication mode and current time/date.
 *	Input:
 *		none.
 *	Return:
 *		none.
 */
void dispUpdate(void){
	//dispUpdMode(disp_mode, set_mode);
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
			if(alarm_is_on){
				iv3a[IV3A_SH].foo = 'o';
				iv3a[IV3A_SL].foo = 'n';
			}else{
				iv3a[IV3A_SH].foo = 'o';
				iv3a[IV3A_SL].foo = 'f';
			}
			break;
		}
		case DMODE_TEMPERATURE_A:
		{
			iv3a[IV3A_HH].foo = 0xFF;
			breakSNumber2(temp_a, &(iv3a[IV3A_MH].foo), &(iv3a[IV3A_ML].foo), &(iv3a[IV3A_HL].foo));
			iv3a[IV3A_SH].foo = 'd'; iv3a[IV3A_SL].foo = 'c';
			break;
		}
		case DMODE_TEMPERATURE_B:
		{
			iv3a[IV3A_HH].foo = 0xFF;
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
			dispUpdMode(disp_mode, set_mode);
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
					alarm_is_on = !alarm_is_on;
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
			dispUpdMode(disp_mode, set_mode);
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
					//alarm--;
					//nowBreakTime(alarm, &alarm_time, &alarm_date);
					alarm_is_on = !alarm_is_on;
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
	if(btn_block == 0){
		uint8_t _btn = 0;
		_btn = (PIND & (1<<PD6));
		if(_btn){
			switch(set_mode){
				case SMODE_NO:
				{
					set_mode = SMODE_SS;
					//dispUpdMode(disp_mode, SMODE_SS);
					break;
				}
				case SMODE_SS:
				{
					set_mode = SMODE_MM;
					//dispUpdMode(disp_mode, SMODE_MM);
					break;
				}
				case SMODE_MM:
				{
					set_mode = SMODE_HH;
					//dispUpdMode(disp_mode, SMODE_HH);
					break;
				}
				case SMODE_HH:
				{
					set_mode = SMODE_NO;
					//dispUpdMode(disp_mode, SMODE_NO);
					break;
				}
				default:
					break;
			}
			// save time to RTC
			// upload_flag = 1;
			dispUpdMode(disp_mode, set_mode);
			dispUpdate();

			if(alarm_is_on == ALARM_ACTIVE)
			{
				alarm_is_on = ALARM_BLOCK;
			}
			BLOCK_BTN();
			return;
		}
		
		_btn = (PIND & (1<<PD7));
		if(_btn){
			set_mode = SMODE_NO;
			dispUpdMode(disp_mode, set_mode);
			dispUpdate();

			if(alarm_is_on == ALARM_ACTIVE)
			{
				alarm_is_on = ALARM_BLOCK;
			}
			BLOCK_BTN();
			return;
		}
	}else if((millis() - sys_timer.btn_block) >= 200)
	{
		btn_block = 0;
	}

	dispUpdate();
}

/**
 * 1 Hz loop
 *
 */
void loop_1Hz(void){
	update_time(upload_flag, set_mode);	// time update method should be called every second!

	if((alarm_is_on == ALARM_STANDBY) || (alarm_is_on == ALARM_ACTIVE) || (alarm_is_on == ALARM_BLOCK))
	{
		if((update_alarm() == 1) && (alarm_is_on != ALARM_BLOCK))
		{
			alarm_is_on = ALARM_ACTIVE;
			alarm_buzzer_cnt = 12;
		}else if((update_alarm() == 0) && (alarm_is_on == ALARM_BLOCK))
		{
			alarm_is_on = ALARM_STANDBY;
		}
	}
}

void buttons_init(void){
	DDRD &= ~((1<<PD6)|(1<<PD7));
}

int main(void)
{
	_delay_ms(500);		// pre-start delay (for BMP180 hardware initialization)
	power_init();		// start complementary pwm on timer2
	millis_init();		// start system millis() on timer0
	i2c_init();			// start TWI
	rtc3231_init();		// start DS3231 RTC
	buttons_init();
	DDRB |= (1<<PB3);
	PORTB &= ~(1<<PB3);

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

	dispUpdMode(DMODE_TIME, SMODE_NO);

	// Main loop
    for(;;)
    {
		if(millis() >= sys_timer.loop_333Hz){
			sys_timer.loop_333Hz = millis() + PERIOD_333HZ_MS;
			loop_333Hz();
		}
		if(millis() >= sys_timer.loop_50Hz){
			sys_timer.loop_50Hz = millis() + PERIOD_50HZ_MS;
			loop_50Hz();
		}
		if(millis() >= sys_timer.loop_1Hz){
			sys_timer.loop_1Hz = millis() + PERIOD_1HZ_MS;
			loop_1Hz();
		}
    }
}