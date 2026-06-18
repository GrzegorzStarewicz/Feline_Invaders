/*
 * buzzer-na-t0.c
 *
 * Created: 18.06.2026 10:46:37
 * Author : annab
 */ 

#include <avr/io.h>
#include <avr/interrupt.h>

#define BUZZER_PORT PORTD
#define BUZZER_DIR  DDRD
#define BUZZER_PIN  4

volatile uint16_t sfx_length = 0;
volatile int8_t sfx_change = 0;

void buzzer_init_software(void) {
	// DDR out
	BUZZER_DIR |= (1 << BUZZER_PIN);

	// WGM12 = 1 (CTC Mode)
	// CS11 = 1 (Prescaler 64)
	TCCR2 = (1 << WGM21) | (1 << CS22);//|(1 << CS21);

	// Pitch
	// 11059200 / (64 * frequency * 2)
	OCR2 = 43;

	sei();
}

void buzzer_on(void) {
	// Enable the Timer 1 Compare A Interrupt
	TIMSK |= (1 << OCIE2);
}

void buzzer_off(void) {
	// Disable the Interrupt and ensure the pin is low
	TIMSK &= ~(1 << OCIE2);
	BUZZER_PORT &= ~(1 << BUZZER_PIN);
}


ISR(TIMER2_COMP_vect) {
	// Toggle the buzzer pin (Flips 0 to 1, or 1 to 0)
	BUZZER_PORT ^= (1 << BUZZER_PIN);
	
	if((sfx_length & 1) && (sfx_length & 2))
	OCR2 -= sfx_change;
	
	if(!sfx_length--) buzzer_off();
}

void play_sfx(uint8_t length, uint16_t frequency, int8_t change)
{
	OCR2 = 11059200 / (64 * frequency * 2);
	sfx_change = change;
	sfx_length = length;//*frequency/80;
	buzzer_on();
}

int main(void)
{
    buzzer_init_software();
	
	
	// strzal
	//play_sfx(40, 8000, -64);
	play_sfx(100, 8000, -8);
	
	// zabicie mega?
	//play_sfx(100, 640, -4);
	
	//while(sfx_length);
	// zabicie zwyklego
	//play_sfx(20, 860, -1);
	
	// -HP
	//play_sfx(100, 340, 0);
	
	
	
	
    while (1) 
    {
		
    }
}



