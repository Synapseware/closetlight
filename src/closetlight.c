#include "closetlight.h"



volatile uint8_t		_tick			= 0;


//-----------------------------------------------------------------------------
// Configures the door sensor pins
void configureDoorSensor(void)
{
	// enable switch sensor on DOOR_PIN
	DDRB	&=	(1<<DOOR_PIN);			// door sensor pin is input
	PORTB	|=	(1<<DOOR_PIN);			// enable pullup resistor on door sensor pin
}


//-----------------------------------------------------------------------------
// Configures the PWM driver & pins
void configurePWM(void)
{
	// prepare timer0 for PWM output
	TCCR0A	=	(1<<COM0A1)	|		// Clear OC0A on compare match
				(0<<COM0A0)	|
				(1<<COM0B1)	|		// Clear 0C0B on compare match
				(0<<COM0B0)	|
				(1<<WGM01)	|		// Fast PWM, TOP = OCR0A
				(1<<WGM00);

	TCCR0B	=	(0<<FOC0A)	|
				(0<<FOC0B)	|
				(1<<WGM02)	|
				(0<<CS02)	|		// clk / 1 / PWM_RANGE ~= 48kHz
				(0<<CS01)	|
				(1<<CS00);

	TIMSK0	=	(1<<OCIE0A)	|		// Enable compare-B interrupt (for chip timer)
				(0<<OCIE0B)	|
				(0<<TOIE0);


	OCR0A	=	PWM_RANGE-1;		// This is the number of timer steps for each PWM window
	OCR0B	=	0;					// start with PWM off

	// start with PWM disabled
	DDRB	&=	~(1<<PWM_OUT);
}


//-----------------------------------------------------------------------------
// prepare device
void init(void)
{
	// shut down peripherals
	ADCSRA	=	0;
	ADCSRB	=	0;

	configurePWM();

	configureDoorSensor();

	DDRB	|=	(1<<STATUS_LED);

	sei();
}


//-----------------------------------------------------------------------------
// Returns 1 if the door is open, 0 if it's closed
uint8_t isDoorOpen(void)
{
	// switch is configured as NO.  with pullup enabled, pin is high
	// when door is closed.
	if ((PINB & (1<<DOOR_PIN)) != 0)
		return 1;

	return 0;
}


//-----------------------------------------------------------------------------
// Sets the PWM brightness level based on the door state
void setLightLevel(uint8_t doorState)
{
	static uint8_t brightness	= 0;
	static uint8_t stepDelay	= 0;		// max is FADE_STEP_MS
	static uint8_t fadePtr		= 0;

	// delay the processing of any fade steps
	if (++stepDelay < FADE_STEP_MS)
		return;
	stepDelay = 0;

	// handle tasks per MS window
	if (doorState)
	{
		// increase when door is open
		if (fadePtr < FADE_TABLE_LEN - 1)
			fadePtr++;
	}
	else
	{
		// decrease when door is closed
		if (fadePtr > 0)
			fadePtr--;
	}

	// set new PWM value
	brightness = pgm_read_byte(&FADE_TABLE[fadePtr]);
	if (brightness > 0)
		DDRB |= (1<<PWM_OUT);
	else
		DDRB &=	~(1<<PWM_OUT);

	OCR0B = brightness;
}


//-----------------------------------------------------------------------------
// main
int main(void)
{
	init();

	PORTB	|=	(1<<STATUS_LED);

	while(1)
	{

		wdt_reset();
		if (!_tick)
			continue;

		// set the light level based on the door state
		setLightLevel(isDoorOpen());

		_tick = 0;
	}
}


//-----------------------------------------------------------------------------
// Compare-A interrupt handler
ISR(TIM0_COMPA_vect)
{
	static timerTick = 0;

	if (timerTick++ < TICKS_MS)
		return;

	timerTick = 0;
	_tick = 1;
}
