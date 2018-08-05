/*
 * timekeeper.h
 *
 * Created: 27.07.2018 11:08:30
 *  Author: Dr. Saldon
 */ 


#ifndef TIMEKEEPER_H_
#define TIMEKEEPER_H_

#define RTC_TIME_UPDATE_PERIOD_S		60

#define SECS_PER_MIN  ((time_t)(60UL))
#define SECS_PER_HOUR ((time_t)(3600UL))
#define SECS_PER_DAY  ((time_t)(SECS_PER_HOUR * 24UL))
#define DAYS_PER_WEEK ((time_t)(7UL))
#define SECS_PER_WEEK ((time_t)(SECS_PER_DAY * DAYS_PER_WEEK))
#define SECS_PER_YEAR ((time_t)(SECS_PER_WEEK * 52UL))
#define SECS_YR_2000  ((time_t)(946684800UL)) // the time at the start of y2k
#define LEAP_YEAR(Y)     ( ((1970+Y)>0) && !((1970+Y)%4) && ( ((1970+Y)%100) || !((1970+Y)%400) ) )

#define minutesToTime_t ((M)) ( (M) * SECS_PER_MIN)
#define hoursToTime_t   ((H)) ( (H) * SECS_PER_HOUR)
#define daysToTime_t    ((D)) ( (D) * SECS_PER_DAY) 
#define weeksToTime_t   ((W)) ( (W) * SECS_PER_WEEK)

#include <time.h>
#include "rtc3231.h"

static  const uint8_t monthDays[]={31,28,31,30,31,30,31,31,30,31,30,31};

//struct now_time{
	//uint8_t sec;
	//uint8_t min;
	//uint8_t hour;
//}now_time;
//
//struct now_date{
	//uint8_t wday;
	//uint8_t day;
	//uint8_t month;
	//uint8_t year;
//}now_date;

//rtc_time_t now_time;
//rtc_date_t now_date;

#define BUZZ_INACTIVE 	0
#define BUZZ_ACTIVE 	1

#define ALARM_INACTIVE 	0
#define ALARM_STANDBY	1
#define ALARM_ACTIVE	2
#define ALARM_BLOCK 	3

rtc_time_t alarm_time;
rtc_date_t alarm_date;		// dummy
time_t alarm = 0;
uint8_t alarm_is_on = 0;
uint8_t alarm_buzzer = 0;
uint8_t alarm_buzzer_cnt = 0;
uint32_t buzzer_timer = 0;

uint8_t upload_flag = 0;
time_t rtc_update_timer = 0;
time_t now = 0;				// UTC (seconds since 1970)

time_t rtcMakeTime(rtc_time_t *time, rtc_date_t *date){
	// assemble time elements into time_t
	// note year argument is offset from 1970 (see macros in time.h to convert to other formats)
	
	int i;
	uint32_t seconds;

	// seconds from 1970 till 1 jan 00:00:00 of the given year
	seconds= date->year*(SECS_PER_DAY * 365);
	for (i = 0; i < date->year; i++) {
		if (LEAP_YEAR(i)) {
			seconds +=  SECS_PER_DAY;   // add extra days for leap years
		}
	}
	
	// add days for this year, months start from 1
	for (i = 1; i < date->month; i++) {
		if ( (i == 2) && LEAP_YEAR(date->year)) {
			seconds += SECS_PER_DAY * 29;
			} else {
			seconds += SECS_PER_DAY * monthDays[i-1];  //monthDay array starts from 0
		}
	}
	seconds+= (date->day-1) * SECS_PER_DAY;
	seconds+= time->hour * SECS_PER_HOUR;
	seconds+= time->min * SECS_PER_MIN;
	seconds+= time->sec;
	return (time_t)seconds;
}

void nowBreakTime(time_t inputTime, rtc_time_t *ntime, rtc_date_t *ndate){
	// break the given time_t into time components
	// this is a more compact version of the C library localtime function
	// note that year is offset from 1970 !!!

	uint8_t year;
	uint8_t month, monthLength;
	uint32_t time;
	unsigned long days;

	time = (uint32_t)inputTime;
	ntime->sec = time % 60;
	time /= 60; // now it is minutes
	ntime->min = time % 60;
	time /= 60; // now it is hours
	ntime->hour = time % 24;
	time /= 24; // now it is days
	ndate->wday = ((time + 4) % 7) + 1;  // Sunday is day 1
	
	year = 0;
	days = 0;
	while((unsigned)(days += (LEAP_YEAR(year) ? 366 : 365)) <= time) {
		year++;
	}
	ndate->year = year; // year is offset from 1970
	
	days -= LEAP_YEAR(year) ? 366 : 365;
	time  -= days; // now it is days in this year, starting at 0
	
	days=0;
	month=0;
	monthLength=0;
	for (month=0; month<12; month++) {
		if (month==1) { // february
			if (LEAP_YEAR(year)) {
				monthLength=29;
				} else {
				monthLength=28;
			}
			} else {
			monthLength = monthDays[month];
		}
		
		if (time >= monthLength) {
			time -= monthLength;
			} else {
			break;
		}
	}
	ndate->month = month + 1;  // jan is month 1
	ndate->day = time + 1;     // day of month
}

// 1Hz update!!!
void update_time(uint8_t upload_flag, uint8_t set_mode){
	now++;

	if(upload_flag == 1){
		rtc_update_timer = now;
		nowBreakTime(now, &rtc_time, &rtc_date);
		rtc3231_write_date(&rtc_date);
		rtc3231_write_time(&rtc_time);
		upload_flag = 0;
	}else if(((now-rtc_update_timer) >= RTC_TIME_UPDATE_PERIOD_S) && (set_mode == 0))
	{
		rtc3231_read_datetime(&rtc_time, &rtc_date);
		now = rtcMakeTime(&rtc_time, &rtc_date);
		rtc_update_timer = now;
	}else{
		nowBreakTime(now, &rtc_time, &rtc_date);
	}	
}

uint8_t update_alarm(void){
	if((alarm_time.hour == rtc_time.hour) &&
		(alarm_time.min == rtc_time.min) &&
		(alarm_time.sec == rtc_time.sec)){
		return 1;
	}
	return 0;
}

void buzzer_update()
{
	if((alarm_is_on == ALARM_ACTIVE) && (alarm_buzzer_cnt > 0))
	{
		if((millis() - buzzer_timer) > 75)
		{
			if(alarm_buzzer == BUZZ_INACTIVE)
			{
				alarm_buzzer = BUZZ_ACTIVE;
			}else if(alarm_buzzer == BUZZ_ACTIVE)
			{
				alarm_buzzer = BUZZ_INACTIVE;
			}
			alarm_buzzer_cnt--;
			buzzer_timer = millis();
		}
	}

	if(((alarm_is_on == ALARM_INACTIVE) || (alarm_is_on == ALARM_BLOCK)) && (alarm_buzzer != 0))
	{
		alarm_buzzer = 0;
	}

	if(alarm_buzzer){
		PORTB |= (1<<PB3);
	}else{
		PORTB &= ~(1<<PB3);
	}
}

#endif /* TIMEKEEPER_H_ */