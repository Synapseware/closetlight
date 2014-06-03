#ifndef __CLOSETLIGHT__H_
#define __CLOSETLIGHT__H_


#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/pgmspace.h>
#include <avr/power.h>
#include <avr/wdt.h>
#include <math.h>


#define DOOR_PIN		PB3
#define PWM_OUT			PB1
#define STATUS_LED		PB4

#define PWM_LIMIT		200

const uint8_t			PWM_RANGE		= PWM_LIMIT-1;		// steps in PWM window
const uint8_t			MAX_PWM			= PWM_LIMIT-1;

const uint8_t			TICKS_MS		= (F_CPU / 1 / PWM_LIMIT / 1000);	// should be 48
const uint8_t			FADE_STEP_MS	= 4;



const static uint8_t FADE_TABLE[] PROGMEM = {
	0,
	0,
	0,
	0,
	0,
	1,
	1,
	1,
	1,
	1,
	1,
	1,
	1,
	1,
	1,
	1,
	1,
	1,
	2,
	2,
	2,
	2,
	2,
	2,
	2,
	3,
	3,
	3,
	3,
	4,
	4,
	4,
	5,
	5,
	5,
	6,
	6,
	7,
	8,
	8,
	9,
	10,
	10,
	11,
	12,
	13,
	14,
	15,
	17,
	18,
	20,
	21,
	23,
	25,
	27,
	29,
	32,
	34,
	37,
	40,
	44,
	47,
	51,
	56,
	60,
	65,
	71,
	77,
	83,
	90,
	97,
	105,
	114,
	124,
	134,
	145,
	157,
	170,
	185,
	200
};
const static uint8_t FADE_TABLE_LEN = sizeof(FADE_TABLE)/sizeof(uint8_t);




#endif