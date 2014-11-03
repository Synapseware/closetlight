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
// ADC is used in free-running mode to generate random-ish numbers
void configureAdc(void)
{
	ADMUX	=	(1<<REFS0)	|		// internal v-ref
				(0<<ADLAR)	|		// don't left-adjust result
				(0<<MUX1)	|		// sample from PB2 (MUX1:0 = 01)
				(1<<MUX0);

	ADCSRA	=	(1<<ADEN)	|
				(1<<ADSC)	|
				(1<<ADATE)	|		// we want to use free-running to enable autotriggering
				(0<<ADIF)	|
				(0<<ADIE)	|
				(1<<ADPS2)	|		// overclock ADC so it's noisy
				(0<<ADPS1)	|
				(1<<ADPS0);

	ADCSRB	=	(0<<ADTS2)	|		// ADTS2:0 = 0 for free running mode
				(0<<ADTS1)	|
				(0<<ADTS0);

	DIDR0	=	(1<<ADC1D);			// PB2 is left floating on purpose
	DDRB	&=	~(1<<PB2);			// make sure PB2 is in input mode
	PINB	&=	~(1<<PB2);			// make sure pullup resistor is off
	PORTB	&=	~(1<<PB2);
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
	static uint8_t
		stepDelay	= 0,			// max is FADE_STEP_MS
		fadeVal		= BRIGHT_OFF,	// mathmatically determined brightness value
		brightness	= 0,			// the current brightness value
		effect		= EFFECT_OFF;	// which effect are we on

	static uint16_t
		secondsOn	= 0,			// total # of seconds for each effect
		ticks		= 0,			// # of ticks (1/1000 second)
		delay		= 0;			// each effect can have it's own internal delays

	// increment the seconds counter
	if (ticks++ > 1000)
	{
		ticks = 0;
		if (doorState)
			secondsOn++;
	}

	// if the door is closed, then we're off
	if (!doorState)
		effect = EFFECT_OFF;

	// always delay the processing of any brightness changes
	if (stepDelay++ < FADE_STEP_MS)
		return;
	stepDelay = 0;

	// Effects state engine
	switch (effect)
	{
		case EFFECT_OFF:
			if (doorState)
			{
				secondsOn = 0;
				effect = EFFECT_NORMAL;
			}

			fadeVal = 0;
			break;

		case EFFECT_NORMAL:
			if (secondsOn > ONTIME_NORMAL)
			{
				effect = EFFECT_DIMMED;
				secondsOn = 0;
			}

			// set fadeVal based on door state
			fadeVal = BRIGHT_NORMAL;
			break;

		case EFFECT_DIMMED:
			if (secondsOn > ONTIME_DIMMED)
			{
				effect = EFFECT_LOW;
				secondsOn = 0;
			}

			// steady mediaum brightness
			fadeVal = BRIGHT_MED;
			break;

		case EFFECT_LOW:
			if (secondsOn > ONTIME_LOW)
			{
				effect = EFFECT_BLINK;
				secondsOn = 0;
			}

			// steady low brightness
			fadeVal = BRIGHT_LOW;
			break;

		case EFFECT_BLINK:
		{
			static uint8_t blink_state = 0;

			if (secondsOn > ONTIME_BLINK)
			{
				effect = EFFECT_SLEEPY;
				secondsOn = 0;
			}

			// this effect has it's own delays
			if (delay)
			{
				delay--;
			}
			else
			{
				switch (blink_state)
				{
					// long low to fast bright
					case 0:
						fadeVal = BRIGHT_LOW;
						delay = ADCH | (ADCL << 3) | (TCNT0 << 3);
						blink_state = 1;
						break;

					// fast bright to off pause
					case 1:
						fadeVal = BRIGHT_NORMAL;
						delay = 1;
						blink_state = 2;
						break;

					// off pause to fast bright
					case 2:
						fadeVal = BRIGHT_OFF;
						delay = ADCH | (ADCL << 3) + 53;
						blink_state = 3;
						break;

					// fast bright
					case 3:
						fadeVal = BRIGHT_NORMAL;
						delay = 15;
						blink_state = (TCNT0 & 0x01) | (ADCH & 0x01)
							? 4
							: 0;
						break;

					// quick medium, random out to off pause or long low
					case 4:
						fadeVal = BRIGHT_MED;
						delay = 3 + (TCNT0 & 0x0F);
						blink_state = 0;
						break;
				}
			}

			brightness = fadeVal;
		}
		break;

		case EFFECT_SLEEPY:
		{
			static uint8_t	sleepy_state = 0;

			if (secondsOn > ONTIME_SLEEPY)
			{
				effect = EFFECT_OFF;
				secondsOn = 0;
			}

			// sleepy eye effects...
			if (delay)
			{
				delay--;
			}
			else
			{
				switch (sleepy_state)
				{
					// going up
					case 2:
						if (fadeVal < BRIGHT_LOW)
							fadeVal += 4;
						else
							sleepy_state = 1;

						delay = 5;
						break;

					// going down 
					case 1:
						if (fadeVal > 1)
							fadeVal -= 2;
						else
							sleepy_state = 0;

						delay = 5;
						break;

					// resting
					case 0:
						sleepy_state = 2;
						fadeVal = 1;
						delay = 420;
						break;
				}
			}
		}
		break;
	}

	// adjust current brightness to match target brightness
	if (brightness > fadeVal)
		brightness -= FADE_STEPS;
	else if (brightness < fadeVal)
		brightness += FADE_STEPS;

	// set new PWM value
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
	static uint8_t timerTick = 0;

	if (timerTick++ < TICKS_MS)
		return;

	timerTick = 0;
	_tick = 1;
}
