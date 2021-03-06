/*
 * synth_drum.cpp
 *
 * Created: 7/10/2015 1:16:42 PM
 *  Author: byron.jacquot
 */ 

/*	
* Features? (I had these in a couple of notebooks,
* bringing in consolidated list)
*
* (+ indicates something attempted & incorporated, 
*  - indicates unexplored 
*  . indicates attempted, not terribly useful)
*
* + pitch control
* + decay control
* + env mod of pitch
- + multipe waheshapes
* - bipolar pitch modulation (or inverse switch)
* - decay shape (lin/log/exp...)
* + white noise
* - longer LFSR
* . 2nd pitch
* - chorus
* - 2nd env - run generator
* - LFO
* - S&H
* - filter
* - stereo, panning
* - click
* - env hold time
* - bit crush
*/

/*
 * port pin mapping?
 *
 * Rudimentary for now...
 *
 * PortA
 * portB = Pin B5 is pin change IRQ for triggering
 *       = Pins 0..3 are MSBs of R2R DAC
 * portC = Arduino analog inputs 0 - 5...
 * portD = Parallel outs LSBs of R2R DAC...but it'll cost me the hardware serial...
 *
 */

/*
 * Knob mapping?
 * currently using Arduino A0 to A5
 *
 * 0 - osc pitch
 * 1 - osc/noise crossfade
 * 2 - waveshape
 * 3 - decay
 * 4 - env->pitch mod
 * 5 - ??
 */


 


#include <avr/io.h>
#include <avr/interrupt.h>

// Config macros
#define SELF_TRIGGER 1


// constants
static const int8_t wavetable [3][129] = 
{
	// The first entry repeats in the last so that "phasor + 1" lookup is valid.
// Sine
	{0,6,12,19,25,31,37,43,49,54,60,65,71,76,81,85,90,94,98,102,106,109,112,115,117,120,122,123,125,126,126,127,127,
		127,126,126,125,123,122,120,117,115,112,109,106,102,98,94,90,85,81,76,71,65,60,54,49,43,37,31,25,19,12,6,
		0,-6,-12,-19,-25,-31,-37,-43,-49,-54,-60,-65,-71,-76,-81,-85,-90,-94,-98,-102,-106,-109,-112,-115,-117,-120,-122,-123,-125,-126,-126,-127,-127,
		-127,-126,-126,-125,-123,-122,-120,-117,-115,-112,-109,-106,-102,-98,-94,-90,-85,-81,-76,-71,-65,-60,-54,-49,-43,-37,-31,-25,-19,-12,-6,0},
#if 0// sine octave up AKA - the old, short table doubled up
	{0, 12, 25, 37, 49, 60, 71, 81, 90,	98, 106, 112, 117, 122, 125, 126, 127, 126, 125, 122, 117, 112, 106, 98, 90, 81, 71, 60, 49, 37, 25, 12, 0,
		-12,-25,-37,-49,-60,-71,-81,-90,-98,-106,-112,-117,-122,-125,-126,-127,-126,-125,-122,-117,-112,-106,-98,-90,-81,-71,-60,-49,-37,-25,-12,
		0, 12, 25, 37, 49, 60, 71, 81, 90,	98, 106, 112, 117, 122, 125, 126, 127, 126, 125, 122, 117, 112, 106, 98, 90, 81, 71, 60, 49, 37, 25, 12, 0,
		-12,-25,-37,-49,-60,-71,-81,-90,-98,-106,-112,-117,-122,-125,-126,-127,-126,-125,-122,-117,-112,-106,-98,-90,-81,-71,-60,-49,-37,-25,-12,0	
	},
#else
	{
		0,16,32,48,63,76,89,99,109,116,122,125,127,127,124,120,114,106,97,86,75,62,49,35,22,8,-5,-18,-30,-41,-51,-59,-67,-72,-77,-80,-81,-81,-79,-77,-73,-68,-62,-56,-49,-41,-34,-27,-20,-13,-7,-1,
		4,8,11,13,14,15,14,13,12,9,6,3,0,
		-3,-6,-9,-12,-13,-14,-15,-14,-13,-11,-8,-4,1,7,13,20,27,34,41,49,56,62,68,73,77,79,81,81,80,77,72,67,59,51,41,30,18,5,-8,-22,-35,-49,-62,-75,-86,-97,-106,-114,-120,-124,-127,-127,
		-125,-122,-116,-109,-99,-89,-76,-63,-48,-32,-16,0
	},
#endif	
	
//tri
	{
	0,4,8,12,16,20,24,28,32,36,40,44,48,52,56,60,64,67,71,75,79,83,87,91,95,99,103,107,111,115,119,123,127,
	123,119,115,111,107,103,99,95,91,87,83,79,75,71,67,64,60,56,52,48,44,40,36,32,28,24,20,16,12,8,4,
	0,-4,-8,-12,-16,-20,-24,-28,-32,-36,-40,-44,-48,-52,-56,-60,-64,-67,-71,-75,-79,-83,-87,-91,-95,-99,-103,-107,-111,-115,-119,-123,-127,
	-123,-119,-115,-111,-107,-103,-99,-95,-91,-87,-83,-79,-75,-71,-67,-64,-60,-56,-52,-48,-44,-40,-36,-32,-28,-24,-20,-16,-12,-8,-4,0
	}
//saw	
	 
};

// Forward declarations
void startTimer0();
void stopTimer0();


// module-level variables
static uint16_t phasor = 0;
static uint16_t lfsr = 1;
static uint16_t env = 0;

static uint8_t pot_data[6];

// If I'm wiggling pins for debug, configure them here...
void setupDebugPins()
{
	// PB0 = Arduino 8
	// PB1 = Arduino 9
	//DDRB = 0x03;
}

#if 1
// do 4 extra bits of linear interpolation between a and b.
int16_t linterp16(int16_t a,
                  int16_t b,
                  int8_t dist)
{
    volatile int16_t window;

    window = (int16_t)b - (int16_t)a ;

    // Out of order to maintain precision: mul then div...
    // mul by the number of intermediate steps ( 4 bits: 0 to 15)
    //then div by 16
    window *= dist;

    window >>= 4;

    return a+window;
}
#endif


uint8_t do_lfsr()
{

	uint8_t bit;
	
	// pulled straight out of wikipedia...
	// https://en.wikipedia.org/wiki/Linear_feedback_shift_register
	// Stated period of 65535, @ 20KHz = ~3 sec
	
	bit = (((lfsr >> 0) ^ (lfsr >> 2) ^ (lfsr >> 3) ^ (lfsr >> 5) ) & 1);
	lfsr =  (lfsr >> 1) | (bit << 15);
	
	return lfsr;
	
}

// Functionalized and modular so it can get broken up
// across port pins without too much trouble.
void writeDAC(uint16_t data_out)
{
	// 12 bit DAC now.
	// 4 LSBs are port B 3..0.
	// 8 LSBs are pord D 7..0.
	//PORTD = data_out;
	
	PORTB = (PORTB & 0xf0) | (data_out >> 8);
	PORTD = (data_out & 0x0ff);
}

void setupDAC()
{
	// set a 1 to enable as output
	DDRD = 0xff;
	DDRB = 0x0f;
	
}

void setupExtInIRQ()
{
	
#if (SELF_TRIGGER == 0)	
	//Setup portB, pin 5 as pin change interrupt?
	// 0 in DDRB makes pin input
	DDRB &= ~0x20;
	// 0 in DDR and 1 in port enabled pullup
	PORTB |= 0x20;
	
	// disable pullups??
	
	
	// Enable interrupt on pin change 5
	PCMSK0 |= 0x20;
	
	// Clear any pending interrupt on that group...
	PCIFR = 0;
	
	// Enable pin change group zero
	PCICR = 1;
#endif	
	
}

void setupClocks()
{
	// Configure clock scaling.
	// 2-step procedure to keep stray pointers from mangling the value.
	// Using 16 mHz resonator, we'll divide by 2 for 8 MHz.
	
	CLKPR = 0x80;
	CLKPR = 0x00; // clk div 2 = 8 MHz.
}

uint8_t waveLookup(uint16_t pha)
{
	int8_t l, r, dist;
	uint8_t wavesel;

	wavesel = pot_data[2] >> 6;
	
	// Wave table are 128 entries long, 
	// phasor is unsigned 16 bits
	// so need 7 msbs of phasor as index
	// and next 4 bits as interp value
	if(wavesel == 3)
	{
		// return top 8 of phasor as saw
		return(pha>>8);
	}
	else
	{
		l = wavetable[wavesel][pha >> 9];
		r = wavetable[wavesel][((pha >> 9)+1)];
		dist = (pha & 0x01e0) >> 5;
	
		return linterp16(l,r,dist);	
	}
}

ISR(TIMER0_COMPA_vect)
{
	// Source signal is Cleared automatically when vector called.
		
	// Pin wiggle for debug...
	// writing 1's to PIN reg toggles pin.
//	PINB = 0x01;
	
	uint16_t math;
	int8_t  sample;
	int8_t noise;
	
	// Increment phasors
	phasor += (pot_data[0] << 3)+ 0x80;

	// Apply pitch envelope...
	int16_t pitch_mod;
		
	pitch_mod = ((env >> 8) * pot_data[4])>> 4;
	phasor+= (pitch_mod);
	
	//lookup value
	sample = waveLookup(phasor);
	
	// generate noise
	noise = do_lfsr();
	
	// crossfade samp to noise w/ pot [1]
	// TBD - use a better crossfade curve...
	sample = ((sample* (255-pot_data[1]))>>8) +
			 ((noise * (pot_data[1]))>>8);
	//sample >>= 8;
	

	// Calc next envelope
	// 16-bit fixed point multiply...
	// 16 times 16 makes 32 bits...with 16 bits in the MSBs.
	// IE: 0xffff * 0xfff0 = 0xffef0010
	uint32_t res;
	res = env * (uint32_t)(0xff00UL + pot_data[3]);
	env = res >> 16;

	// Apply envelope to latest sample.
	math = ( sample * (env >> 8));		

	// write the value to the output
#if 1
	//writeDAC((math >> 8) + 128);
	writeDAC((math >> 4) + 2048);
	//writeDAC((math >> 4) + 2048);
#else
	// for debugging - just output the waveform, no env
	writeDAC(sample + 128);
#endif

	// Stop timer when envelope reaches 0
	if( !(env & 0xff00))
	{
		stopTimer0();
	}

//	PINB = 0x01;
}

ISR(PCINT0_vect)
{
	// Flag auto clears when vector taken
	
	// Read pins....
	volatile uint8_t port;
	
	port = PINB;
	
	//this does falling edge...
	//might need to change when piezo comes into play
	if(!(port & 0x20))
	{
		// By setting the peak level, and enabling ISRs
		env = 0xffff;
		phasor = 0 ;
		
		TCNT0 = 0; // start timer at zero, otherwise IRQ is already pending.
				// This keeps our sample rate regular, but delays onset by up to 50 uSec.
		//sei();
		startTimer0();		
	}
}

void setupTimer0()
{
	// 20 KHZ = 16MHZ / 800;
	//  but with timer prescale. we can reduce that.
	// 16MHZ / 8 = 2 MHZ.
	// 2MHz / 100 = 20KHz.
	
	// Pins disconnected, 
	TCCR0A = (1 << WGM01); // Clear on match
	TCCR0B = (1 << CS01);  // Clk sel as div by 8.
	
	TCNT0 = 0; // start at zero
	OCR0A = 99; // count to 100.
	
	TIMSK0 = 0x02; // interrupt on match A
	
}

void startTimer0()
{
	TIMSK0 = 0x02; // interrupt on match A
}

void stopTimer0()
{
	TIMSK0 &= ~0x02; // interrupt on match A
}

void setupADC()
{
	
}

uint8_t readADC(uint8_t channel)
{
	uint16_t value;
	
	// only allow the pins we've selected to read...
	if(channel >= 6)
	{
		// catch for attempt to read invalid channel.
		while(1);
	}
	
	// turn on adc
	// TODO: perhaps turn on and leave on?
	ADCSRA = 0x86; // power bit, prescale of 64
	
	ADCSRB |= 0x00; // not option to left justify
	
	// set digital input disable
	DIDR0 = 0x3f;
	
	// input mux, vref selection
	// Left justified, AVcc reference
	ADMUX = channel | 0x60;

	// pause a moment...
//	for(volatile uint8_t i = 0xff; i != 0; i--);
	
	// bit 6 starts conversion
	// bit 4 clears IRQ flag
	ADCSRA |= 0x50;
	
	// start bit clears when complete
	// watching the interrupt flag
	while(ADCSRA & 0x40);

	value = ADCH;
	
    return (value);
	
}

int main(void)
{
	
	volatile uint32_t pause;
	
	setupDebugPins();
	
	setupClocks();
	
	setupDAC();
	
	setupADC();

	setupExtInIRQ();

	setupTimer0();
		
	// initialize panel data by reading all controls one time
	for(uint8_t idx = 0; idx < 6; idx++)
	{
		pot_data[idx] = readADC(idx);
	}
		
		
	// enable interrupts
	sei();
		
    while(1)
    {
        //count++;
		//writeDAC(count);

		// canary in coalmine idle wiggle
//		PINB = 0x02;
		
//		for(pause = 0; pause < 0x1fffff; pause++)
		for(pause = 0; pause < 0x03fff/*f*/; pause++)
		{
			//PINB = 0x02;
			
			uint8_t channel = pause%6;
			pot_data[channel] = readADC(channel);
			
			
			// We can't disable interrupts from ISR, because the ISR exit
			// re-enables them regardless.  So we'll watch the env, and when it's done, 
			// stop them.
			//
			// TODO: try it with a "naked" isr?
			if(env == 0x0)
			{
				//cli();
			}
		}
			
#if (SELF_TRIGGER == 1)
		// for now, auto-trigger the envelope
		// By setting the peak level, and enabling ISRs
		env = 0xffff;
		phasor = 0 ;
		
		TCNT0 = 0; // start timer at zero, otherwise IRQ is already pending.
				// This keeps our sample rate regular, but delays onset by up to 50 uSec.
		startTimer0();
#endif		
    }
}