/*
 * Buzzer-testy2.c
 *
 * Created: 14.06.2026 21:51:35
 * Author : annab
 */ 

#include <avr/io.h>
#include <avr/interrupt.h>

#define BUZZER_PORT PORTD
#define BUZZER_DIR  DDRD
#define BUZZER_PIN  4

uint16_t sfx_length = 0;
int8_t sfx_change = 0;

void buzzer_init_software(void) {
	// DDR out
	BUZZER_DIR |= (1 << BUZZER_PIN);

	// WGM12 = 1 (CTC Mode)
	// CS11 = 1 (Prescaler 8)
	TCCR1B = (1 << WGM12) | (1 << CS11);

	// Pitch
	// 11059200 / (8 * frequency * 2)
	OCR1A = 64000;

	sei();
}

void buzzer_on(void) {
	// Enable the Timer 1 Compare A Interrupt
	TIMSK |= (1 << OCIE1A);
}

void buzzer_off(void) {
	// Disable the Interrupt and ensure the pin is low
	TIMSK &= ~(1 << OCIE1A);
	BUZZER_PORT &= ~(1 << BUZZER_PIN);
}


ISR(TIMER1_COMPA_vect) {
	// Toggle the buzzer pin (Flips 0 to 1, or 1 to 0)
	BUZZER_PORT ^= (1 << BUZZER_PIN);
	
	if((sfx_length & 1) && (sfx_length & 2))
	OCR1A -= sfx_change;
	
	if(!sfx_length--) buzzer_off();
}

void play_sfx(uint8_t length, uint16_t frequency, int8_t change)
{
	OCR1A = 11059200 / (8 * frequency * 2);
	sfx_change = change;
	sfx_length = length*frequency/40;
	buzzer_on();
}

int main(void)
{
    buzzer_init_software();
	
	
	
	// strzal
	//play_sfx(1, 2000, -32);
	
	// zabicie mega?
	//play_sfx(4, 300, -128);
	
	// zabicie zwyklego
	//play_sfx(4, 160, -64);
	
	// -HP
	//play_sfx(10, 80, 0);
	
	
	
	
    while (1) 
    {
		
    }
}

