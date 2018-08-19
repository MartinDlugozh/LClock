/*
 * ATMega32_LCC.c
 *
 * Created: 11.07.2018 9:45:56
 * Author : Dr. Saldon
 */ 

#define F_CPU 16000000UL

#define PERIOD_333HZ_MS		3
#define PERIOD_100HZ_MS		10
#define PERIOD_2HZ_MS		500
#define PERIOD_1HZ_MS 		1000

#define BLOCK_BTN() { btn_block = 1; sys_timer.btn_block = millis(); }

#define DS18B20_CALIBRATION_OFFSET (int)(0)
#define BMP180_CALIBRATION_OFFSET (int)(0)

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>
#include <time.h>
#include <util/delay.h>

#include <stdbool.h>

#include "millis.h"
#include "ptc.h"
#include "iv3a_display.h"
#include "i2c.h"
#include "rtc3231.h"
#include "timekeeper.h"
#include "onewire.h"
#include "ds18x20.h"
#include "bmp085.h"

// Main loop timers
struct {
	volatile uint32_t loop_333Hz;		// anode switching loop (1ms period)
	volatile uint32_t loop_100Hz;		// buttons reading loop	(10 ms period)
	volatile uint32_t loop_2Hz;		//	(500 ms period)
	volatile uint32_t loop_1Hz;			// time updating loop (1000ms period)
	volatile uint8_t btn_block;			// buttons blocking timer (200ms period)
}sys_timer;

volatile typedef enum _dispMode{		// indication modes ("main menu")
	DMODE_TIME = 0,						// 0..5(3) - main modes
	DMODE_DATE = 1,
	DMODE_ALARM = 2,
	DMODE_TEMPERATURE_A = 3,
	DMODE_TEMPERATURE_B = 4,
	DMODE_PRESSURE = 5,
	DMODE_CTIME,						// other - for dispSetMode() switch statement optimization 
	DMODE_CTEMPERATURE,
	DMODE_OFF = 0xFE,
	DMODE_ENUM_END = 0xFF
} dispMode_t;

volatile typedef enum _setMode{			// adjustment modes
	SMODE_NO = 0,						// normal indication mode (no adjustment)
	SMODE_SS = 1,						// adjust second/year
	SMODE_MM = 2,						// adjust minute/month
	SMODE_HH = 3,						// adjust hour/day
	SMODE_ENUM_END = 0xFF
} setMode_t;

volatile dispMode_t disp_mode = 0;			// time/date/alarm/temperature/pressure
volatile setMode_t set_mode = 0;			// adjustment mode

int8_t temp_a = 0;						// temperature from DS18B20 (external)
int8_t temp_b = 0;						// temperature from BMP180 (internal)
uint16_t pressure = 0;					// barometric pressure from BMP180

volatile uint8_t btn_block;			// button debouncing flag
uint8_t btn_set_prev_state = 0;
uint8_t btn_ok_prev_state = 0;

void USART_init()
{
	// Set baud rate
	UBRRH = 0;
	UBRRL = 51;
	UCSRA = 0;
	// Enable receiver and transmitter
	UCSRB = (1<<TXEN)|(1<<RXEN);
	// Set frame format
	UCSRC = (1<<UCSZ1) | (1<<UCSZ0) | (1<<URSEL);
}

void USART0_write(unsigned char data)
{
	while ( !( UCSRA & (1<<UDRE)) ) ;
	UDR = data;
}

typedef enum _ds18b20_conv_state{
	DSCS_UNINIT = 0,
	DSCS_CONV,
	DSCS_ENUM_END = 0xFF
}ds18b20_conv_state_t;

ds18b20_conv_state_t ds18b20_cs = DSCS_UNINIT;
uint8_t ds18b20_rom[8];

void ds18b20_init(void){
	USART_init();
	OW_ReadROM(ds18b20_rom);
}

void ds18b20_conv(void){
	switch(ds18b20_cs){
		case DSCS_UNINIT:
		{
			ds18b20_cs = DSCS_CONV;
			DS18x20_StartMeasure(ds18b20_rom);
			break;
		}
		case DSCS_CONV:
		{
			ds18b20_cs = DSCS_UNINIT;
			uint8_t data[2];
			DS18x20_ReadData(ds18b20_rom, data);
			temp_a = DS18x20_ConvertToThemperatureFl(data) + DS18B20_CALIBRATION_OFFSET;
			break;
		}
		default:
			break;
	}
}

void bmp180_pressure_conv(void){
	uint16_t _ptmp = 0;
	_ptmp += (bmp085_getpressure()/100);
	pressure = _ptmp;
}

void dispReload(dispMode_t dmode, setMode_t smode){
	dispMode_t _dmode;
	//disp_mode = dmode;
	//set_mode = smode;

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
					if(disp_mode == DMODE_TIME){
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
					}else if(disp_mode == DMODE_DATE){
						iv3a[IV3A_HL].mode = MODE_ON_POINT;
						iv3a[IV3A_ML].mode = MODE_ON_POINT;
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

			if(disp_mode == DMODE_TIME){
				breakNumber2(rtc_time.hour, &(iv3a[IV3A_HH].foo), &(iv3a[IV3A_HL].foo));
				breakNumber2(rtc_time.min, &(iv3a[IV3A_MH].foo), &(iv3a[IV3A_ML].foo));
				breakNumber2(rtc_time.sec, &(iv3a[IV3A_SH].foo), &(iv3a[IV3A_SL].foo));
			}else if(disp_mode == DMODE_DATE){
				breakNumber2(rtc_date.day, &(iv3a[IV3A_HH].foo), &(iv3a[IV3A_HL].foo));
				breakNumber2(rtc_date.month, &(iv3a[IV3A_MH].foo), &(iv3a[IV3A_ML].foo));
				breakNumber2((rtc_date.year-30), &(iv3a[IV3A_SH].foo), &(iv3a[IV3A_SL].foo));
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
		case DMODE_CTEMPERATURE:
		{
			iv3a[IV3A_HH].mode = MODE_CHAR;
			iv3a[IV3A_HL].mode = MODE_CHAR;
			iv3a[IV3A_MH].mode = MODE_ON;
			iv3a[IV3A_ML].mode = MODE_ON;
			iv3a[IV3A_SH].mode = MODE_CHAR;
			iv3a[IV3A_SL].mode = MODE_CHAR;

			if(disp_mode == DMODE_TEMPERATURE_A){
				iv3a[IV3A_HH].foo = 'O';
				breakSNumber2(temp_a, &(iv3a[IV3A_MH].foo), &(iv3a[IV3A_ML].foo), &(iv3a[IV3A_HL].foo));
				iv3a[IV3A_SH].foo = 'd'; iv3a[IV3A_SL].foo = 'c';
			}
			else if(disp_mode == DMODE_TEMPERATURE_B){
				iv3a[IV3A_HH].foo = 'I';
				breakSNumber2(temp_b, &(iv3a[IV3A_MH].foo), &(iv3a[IV3A_ML].foo), &(iv3a[IV3A_HL].foo));
				iv3a[IV3A_SH].foo = 'd'; iv3a[IV3A_SL].foo = 'c';
				break;
			}
			break;
		}
		case DMODE_PRESSURE:
		{
			iv3a[IV3A_HH].mode = MODE_ON;
			iv3a[IV3A_MH].mode = MODE_ON;
			iv3a[IV3A_SH].mode = MODE_CHAR;
			iv3a[IV3A_HL].mode = MODE_ON;
			iv3a[IV3A_ML].mode = MODE_ON;
			iv3a[IV3A_SL].mode = MODE_CHAR;
			breakNumber4(pressure, &(iv3a[IV3A_HH].foo), &(iv3a[IV3A_HL].foo), &(iv3a[IV3A_MH].foo), &(iv3a[IV3A_ML].foo));
			iv3a[IV3A_SH].foo = 'h'; iv3a[IV3A_SL].foo = 'p';
			break;
		}
		default:
			break;
	}
}

void dmode_inc(void){
	if(++disp_mode >= 6){
		disp_mode = 0;
	}
	dispReload(disp_mode, set_mode);
}

void dmode_dec(void){
	if(disp_mode > 0){
		disp_mode--;
		}else{
		disp_mode = 5;
	}
	dispReload(disp_mode, set_mode);
}

// encoder32 routine
void encoder_on_inc(void){
	switch(set_mode){
		case SMODE_NO:				// normal indication
		{
			dmode_inc();
			if(disp_mode == DMODE_TEMPERATURE_B){
				temp_b = bmp085_gettemperature()+BMP180_CALIBRATION_OFFSET;
			}
			if(disp_mode == DMODE_PRESSURE){
				//bmp180_pressure_conv();
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
					alarm_is_on = !alarm_is_on;
					break;
				}
				default:
				{
					dmode_inc();
					break;
				}
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
				{
					dmode_inc();
					break;
				}
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
				{
					dmode_inc();
					break;
				}
			}
			break;
		}
		default:
			break;
	}

	dispReload(disp_mode, set_mode);
}

// encoder32 routine
void encoder_on_dec(void){
	switch(set_mode){
		case SMODE_NO:				// normal indication
		{
			dmode_dec();
			if(disp_mode == DMODE_TEMPERATURE_B){
				temp_b = bmp085_gettemperature();
			}
			if(disp_mode == DMODE_PRESSURE){
				//bmp180_pressure_conv();
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
					alarm_is_on = !alarm_is_on;
					break;
				}
				default:
				{
					dmode_dec();
					break;
				}
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
				{
					dmode_dec();
					break;
				}
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
				{
					dmode_dec();
					break;
				}
			}
			break;
		}
		default:
			break;
	}

	dispReload(disp_mode, set_mode);
}

#include "encoder32.h"

/**
 * 333 Hz loop 
 * IV-3A display update only
 */
extern void loop_333Hz(void){
	if(millis() >= sys_timer.loop_333Hz){
		sys_timer.loop_333Hz = millis() + PERIOD_333HZ_MS;

		IV3aDispLoop();
	}
}

/**
 * 100 Hz loop
 *
 */
void loop_100Hz(void){
	if(millis() >= sys_timer.loop_100Hz){
		sys_timer.loop_100Hz = millis() + PERIOD_100HZ_MS;
				
		// Buttons reading code
		if(btn_block == 0){
			uint8_t _btn_set = 0;
			uint8_t _btn_ok = 0;
			_btn_set = (PIND & (1<<PIND6)) >> PIND6;		// read "SET" button
			if((_btn_set == 1) && (_btn_set != btn_set_prev_state)){
				if(alarm_is_on == ALARM_ACTIVE)		// reset alarm
				{
					alarm_is_on = ALARM_BLOCK;
				}else{
					setMode_t _smode = set_mode;
					switch(set_mode){
						case SMODE_NO:
						{
							_smode = SMODE_SS;
							break;
						}
						case SMODE_SS:
						{
							_smode = SMODE_MM;
							break;
						}
						case SMODE_MM:
						{
							_smode = SMODE_HH;
							break;
						}
						case SMODE_HH:
						{
							_smode = SMODE_NO;
							break;
						}
						default:
							break;
					}
					set_mode = _smode;
					// save time to RTC
					
					dispReload(disp_mode, set_mode);
				}
				BLOCK_BTN();
			}
			btn_set_prev_state = _btn_set;
			
			_btn_ok = (PIND & (1<<PIND7)) >> PIND7;		// read "OK" button
			if((_btn_ok == 1) && (_btn_ok != btn_ok_prev_state)){
				if(alarm_is_on == ALARM_ACTIVE)		// reset alarm
				{
					alarm_is_on = ALARM_BLOCK;
				}else{
					if((disp_mode == DMODE_TIME) || (disp_mode == DMODE_DATE)){
						upload_to_rtc();
					}else{
						disp_mode = DMODE_TIME;
					}
					set_mode = SMODE_NO;
					dispReload(disp_mode, set_mode);
				}
				BLOCK_BTN();
			}
			btn_ok_prev_state = _btn_ok;
		}else if(millis() >= sys_timer.btn_block)
		{
			btn_block = 0;
		}
	}
}

void loop_2Hz(void){
	if(millis() >= sys_timer.loop_2Hz){
		sys_timer.loop_2Hz = millis() + PERIOD_2HZ_MS;

		ds18b20_conv();
		temp_b = bmp085_gettemperature()+BMP180_CALIBRATION_OFFSET;
		bmp180_pressure_conv();

		dispReload(disp_mode, set_mode);
	}
}

/**
 * 1 Hz loop
 *
 */
void loop_1Hz(void){
	if(millis() >= sys_timer.loop_1Hz){
		sys_timer.loop_1Hz = millis() + PERIOD_1HZ_MS;
		
		update_time(set_mode);	// time update method should be called every second!
		dispReload(disp_mode, set_mode);

		// alarm check and tone genertion
		if((alarm_is_on == ALARM_STANDBY) || (alarm_is_on == ALARM_ACTIVE) || (alarm_is_on == ALARM_BLOCK))
		{
			if((update_alarm() == 1) && (alarm_is_on != ALARM_BLOCK))
			{
				alarm_is_on = ALARM_ACTIVE;
				alarm_buzzer_cnt = 12;
				buzzer_update(btn_block);
			}else if((update_alarm() == 0) && (alarm_is_on == ALARM_BLOCK))
			{
				alarm_is_on = ALARM_STANDBY;
			}
		}
	}
}

void buttons_init(void){
	DDRD &= ~((1<<PD6)|(1<<PD7));	// PD6 - "SET"; PD7 - "OK"
}

void buzzer_init(void){
	DDRB |= (1<<PB3);				// buzzer output on PB3
	PORTB &= ~(1<<PB3);
}

/**
 * Main loop tasks
 */
extern void run_tasks(void){
	loop_333Hz();
	loop_100Hz();
	loop_2Hz();
	loop_1Hz();
	buzzer_update(btn_block);
}

int main(void)
{
	_delay_ms(500);		// pre-start delay (for BMP180 hardware initialization)
	power_init();		// start complementary pwm on timer2
	millis_init();		// start system millis() on timer0
	i2c_init();			// start TWI
	rtc3231_init();		// start DS3231 RTC
	buttons_init();		// initialize buttons pins
	buzzer_init();		// initialize buzzer pin
	_delay_ms(500);

//	rtc_date.year = 48;		// uncomment and set actual time and date
//	rtc_date.month = 7;
//	rtc_date.day = 27;
//	rtc_date.wday = 5;
//	rtc_time.hour = 18;
//	rtc_time.min = 0;
//	rtc_time.sec = 0;
//	rtc3231_write_date(&rtc_date);
//	rtc3231_write_time(&rtc_time);

	rtc3231_read_datetime(&rtc_time, &rtc_date);		// read time and date from RTC
	now = rtcMakeTime(&rtc_time, &rtc_date);			// make UTC from RTC time and date
	rtc_update_timer = now;								// reset RTC update timer
	alarm = now;										// reset alarm time

	encoder_init();		// configure encoder ISR
	ds18b20_init();		// start DS18B20 temperature sensor

	IV3aInit();			// start IV-3A luminescent display

	sys_timer.loop_333Hz = millis();				// initialize system timer
	sys_timer.loop_100Hz = sys_timer.loop_333Hz;
	sys_timer.loop_2Hz = sys_timer.loop_333Hz;
	sys_timer.loop_1Hz = sys_timer.loop_333Hz;

	disp_mode = DMODE_TIME;
	set_mode = SMODE_NO;
	dispReload(disp_mode, set_mode);

	bmp085_init();		// start BMP180 pressure and temperature sensor; do i2c_init() before this!

	// Main loop
    for(;;)
    {
		run_tasks();
    }
}