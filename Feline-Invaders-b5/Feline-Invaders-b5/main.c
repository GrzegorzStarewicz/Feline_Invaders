/*
 * ANG-GANG-Feline-Invaders-b5.c
 *
 * Created: 18.06.2026 11:52:22
 * 
 * Author : £ka
 */ 

#include <avr/io.h>
#define F_CPU 11059200UL
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdlib.h>

// LCD
#define magistralaDDR DDRC
#define magistrala PORTC
#define sterRSDDR DDRB
#define sterRS PORTB
#define sterEDDR DDRD
#define sterE PORTD
#define sterRW PORTD
#define RS 2 //portB
#define E 6  //portD
#define RW 7 //portD

// znaki
#define znGracz '>'
#define znWrog '*'
#define znPocisk '='
#define znNic ' '
#define znOgon '-'
#define znOgonKoniec '&'
#define znMegaPrz '{'
#define znMegaTyl 'E'

// przyciski
#define przyciski PINB
// nie moze byc na 2 bo to jest RS
#define przlewoprawo 0 //PORTB
#define przstrzel 1    //PORTB
// ani 3 4 5 6 bo to seg7
uint8_t zmlewoprawo = 0;
uint8_t zmstrzel = 0;

//pozycje
uint16_t wrogowieA = 0;
uint16_t wrogowieB = 0;
uint16_t pociskiA = 0;
uint16_t pociskiB = 0;
uint16_t eliminacja = 0;
uint16_t wrogowiemegaA1 = 0;
uint16_t wrogowiemegaA2 = 0;
uint16_t wrogowiemegaB1 = 0;
uint16_t wrogowiemegaB2 = 0;

//boole gracza
uint8_t pozycja = 0;
uint8_t strzel = 0;

//bool T0 przyciski
uint8_t sprawdzprz = 0;
//bool T1 clock
uint8_t cykl = 0;

//   BALANS GRY
//czestosc wrogow x / 255
uint8_t trudnosc = 255 / 3; // czyli 1/4
uint8_t trudnosc_mega = 255 / 24; // czestosc mocnych wrogow, czyli 1 / 16
//poczatkowa szybkosc timera 1 gry 10500 - szybkie, 5000 - bardzo szybkie, 20000 - wolne
uint16_t szybkosc_gry = 6000;
#define przyspieszanie 32

#define szybkosc_animacji 30250

//dane gry
uint8_t zycie = 9;
uint16_t score = 0;

uint8_t faza = 0;  // 0 - menu, 1 - gra, 2 - smierc
uint8_t klatka = 0;

//seg7 na 3 4 5 6 ???
uint8_t dane[] = {1,2,3,4};
#define segmentyDDR DDRA //portA I/O
#define segmenty PORTA //diody na porcie A
#define wyborDDR DDRB
#define wybor PORTB	
uint8_t seg7[] = { 0xc0,0xf9,0xa4,0xb0,0x99,0x92,0x82,0xd8,0x80,0x90 };

uint8_t cykl_255 = 0;

// Buzzer
#define BUZZER_PORT PORTD
#define BUZZER_DIR  DDRD
#define BUZZER_PIN  4

volatile uint16_t sfx_length = 0;
volatile int8_t sfx_change = 0;
volatile uint8_t sfx_which = 0;
#define sfxStrzal 1
#define sfxWrog 2
#define sfxMega 3
#define sfxZycie 4


// 8-bit pseudolosowy (nie moze byc 0!)
unsigned char pseudo_rand = 0xAA;
//rand something something
void ADC_init() {
	ADMUX = (1 << REFS0); // Use AVCC as reference
	ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1); // Enable ADC, prescaler 64
}
uint8_t entropia(void) {
	ADCSRA |= (1 << ADSC);          // Start conversion
	while (ADCSRA & (1 << ADSC));   // Wait for conversion to finish (takes ~100us)
	return ADCL;                    // Return the lowest 8 bits of noise
}
// Fast 8-bit random number generator
unsigned char fast_rand(void) {
	// Galois LFSR polynomial taps for 8-bits: bits 7, 5, 4, 3
	if (pseudo_rand & 1) {
		pseudo_rand = (pseudo_rand >> 1) ^ 0x8C;
		} else {
		pseudo_rand = (pseudo_rand >> 1);
	}
	return pseudo_rand;
}

// BUZZER
void buzzer_init_software(void) {
	// DDR out
	BUZZER_DIR |= (1 << BUZZER_PIN);

	// WGM12 = 1 (CTC Mode)
	// CS11 = 1 (Prescaler 64)
	TCCR2 = (1 << WGM21) | (1 << CS22);//|(1 << CS21);

	// Pitch
	// 11059200 / (64 * frequency * 2)
	OCR2 = 43;

	//sei();
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

void play_sfx(uint16_t length, uint16_t frequency, int8_t change)
{
	OCR2 = 11059200 / (64 * frequency * 2);
	sfx_change = change;
	sfx_length = length;//*frequency/80;
	buzzer_on();
}

void LCD_zapis(uint8_t dane)
{
	
	// zbocze opadajace (?)
	magistrala = dane;
	sterE |= (1<<E); //E = 1
	_delay_us(20);
	sterE &= ~(1<<E); //E = 0
	_delay_us(50);
	//_delay_ms(2);
}
void LCD_init(){
	// DDR
	magistralaDDR = 0xff;
	sterRSDDR |= (1<<RS); //wyjscie
	sterEDDR |= (1<<E)|(1<<RW);
	
	// inicjalizacja wyswietlacza LCD do pracy
	_delay_ms(5);
	sterRS &= ~(1<<RS); // bedziemy podawac instrukcje
	sterRW &= ~(1<<RW);
	
	// magistrala 8-bitowa
	// function set 00111000 - 001DNFXX
	// == 0x38
	LCD_zapis(0x38);
	
	// inkrementacja znaku
	// entry mode set 00000110 - 000001IS
	// == 0x06
	LCD_zapis(0x06);
	
	// wlaczenie LCD
	// display ON/OFF 00001111 -
	// == 0x0f / 0x0c bez kursora
	LCD_zapis(0x0c);
	
	// czyszczenie - trzeba zdelayowac
	LCD_zapis(0x01);
	_delay_ms(2);
}
//nieuzywane
void LCD_znak(uint8_t zn)
{
	static uint8_t j = 0;
	//zn = zn + '0';
	if(zn == '\b') {
		if(j != 0) j--;
		else j = 31;
		
		uint8_t pozycja;
		
		sterRS &= ~(1<<RS);
		if(j < 16) pozycja = 0x80+j;
		else pozycja = 0xc0+j-16;
		LCD_zapis(pozycja);
		
		sterRS |= (1<<RS);
		LCD_zapis(' ');
		
		sterRS &= ~(1<<RS);
		LCD_zapis(pozycja);
	}
	else
	{
		if(j==16)
		{
			sterRS &= ~(1<<RS); //wybor gdzie piszemy
			LCD_zapis(0xc0);
		}
		if(j==32)
		{
			sterRS &= ~(1<<RS); //wybor gdzie piszemy
			LCD_zapis(0x80);
			j = 0;
		}
		
		sterRS |= (1<<RS);
		LCD_zapis(zn+j);
		j++;
	}
}
void LCD_zpA(char znak, uint8_t poz)
{
	sterRS &= ~(1<<RS);
	LCD_zapis(0x80+poz);
	sterRS |= (1<<RS);
	LCD_zapis(znak);
}
void LCD_zpB(char znak, uint8_t poz)
{
	sterRS &= ~(1<<RS);
	LCD_zapis(0xc0+poz);
	sterRS |= (1<<RS);
	LCD_zapis(znak);
}
//nieuzywane
void Napis(uint8_t *tekst)
{
	uint8_t i=0;
	
	sterRS &= ~(1<<RS); // wybor gdzie piszemy
	LCD_zapis(0x80);
	
	sterRS |= (1<<RS);
	while(tekst[i])
	{
		LCD_zapis(tekst[i++]);
		if(i==16)
		{
			sterRS &= ~(1<<RS); //wybor gdzie piszemy
			LCD_zapis(0xc0);
			sterRS |= (1<<RS);
		}
	}
}
void NapisA(uint8_t *tekst)
{
	sterRS &= ~(1<<RS); // wybor gdzie piszemy
	LCD_zapis(0x80);
	
	sterRS |= (1<<RS);
	for(uint8_t i = 0; i<16; i++)
	{
		if (tekst[i] == 0) break;
		LCD_zapis(tekst[i]);
	}
}
void NapisB(uint8_t *tekst)
{
	sterRS &= ~(1<<RS); // wybor gdzie piszemy
	LCD_zapis(0xc0);
	
	sterRS |= (1<<RS);
	for(uint8_t i = 0; i<16; i++)
	{
		if (tekst[i] == 0) break;
		LCD_zapis(tekst[i]);
	}
}

// timer cyklu 
void T1_init(){
	TCCR1B = (1<<WGM12)|(1<<CS12); //WGM12 - tryb CTC, CS12 - clk/256
	//TCCR1B = (1<<WGM12)|(1<<CS10);
	TIMSK |= (1<<OCIE1A); //w TIMSKu zgadzamy sie na to przerwanie
	OCR1A = szybkosc_gry;
	//OCR1A = szybkosc_gry; //prescaler
	
	sei();
}
// timer przyciskow
void T0_init(){
	TCCR0 = (1<<CS01)|(1<<CS00); //CS02,CS00 - clk/1024, WGM01 - tryb CTC
	TIMSK |= (1<<TOIE0); //przerwanie od przepelnienia t0
	//OCR0 = 0xff; 
}

void Przyciski_init(){
	sterRSDDR &= ~((1<<przlewoprawo)|(1<<przstrzel)); //przycisk jako wejscie
	sterRS |= (1<<przlewoprawo)|(1<<przstrzel); //rezystor polaryzujacy na wejsciu przycisk
}

// timer cyklu
ISR(TIMER1_COMPA_vect)
{
	cykl = 1;
}
// timer przyciskow
ISR(TIMER0_OVF_vect)
{
	sprawdzprz = 1;
}

void sprawdz_przyciski(){
	// przycisk lewo prawo
	if(!(przyciski & (1<<przlewoprawo)))
	{
		switch(zmlewoprawo)
		{
			case 0:
			zmlewoprawo = 1;
			break;
			case 1:
			zmlewoprawo = 2;
			break;
		}
	}
	else
	{
		switch(zmlewoprawo)
		{
			case 3:
			zmlewoprawo = 4;
			break;
			case 4:
			zmlewoprawo = 0;
			break;
		}
	}
	
	// przycisk lewo prawo
	if(!(przyciski & (1<<przstrzel)))
	{
		switch(zmstrzel)
		{
			case 0:
			zmstrzel = 1;
			break;
			case 1:
			zmstrzel = 2;
			break;
		}
	}
	else
	{
		switch(zmstrzel)
		{
			case 3:
			zmstrzel = 4;
			break;
			case 4:
			zmstrzel = 5; //na 5!
			break;
		}
	}
}

uint8_t count_destructive(uint16_t zmienna)
{
	uint8_t count = 0;
	for (count = 0; zmienna; count++) {
		zmienna &= zmienna - 1;
	}
	return count;
}

void check_i_eliminacja()
{
	
	// eliminacja zwykla A
	eliminacja = wrogowieA & pociskiA;
	wrogowieA &= ~eliminacja;
	pociskiA &= ~eliminacja;
	
	// SFX
	if (eliminacja) if (sfx_which<sfxMega) sfx_which = sfxWrog;
	// dodaj do score
	score += count_destructive(eliminacja);
	
	// eliminacja B
	eliminacja = wrogowieB & pociskiB;
	wrogowieB &= ~eliminacja;
	pociskiB &= ~eliminacja;
	
	// SFX
	if (eliminacja) if (sfx_which<sfxMega) sfx_which = sfxWrog;
	// dodaj do score
	score += count_destructive(eliminacja);
	
	
	// eliminacja mega A
	eliminacja = wrogowiemegaA1 & pociskiA;
	wrogowiemegaA1 &= ~eliminacja;
	pociskiA &= ~eliminacja;
	
	// SFX
	if (eliminacja) if (sfx_which!=sfxZycie) sfx_which = sfxMega;
	// dodaj do score
	score += count_destructive(eliminacja);
	
	eliminacja = wrogowiemegaA2 & pociskiA;
	wrogowiemegaA2 &= ~eliminacja;
	pociskiA &= ~eliminacja;
	
	// SFX
	if (eliminacja) if (sfx_which!=sfxZycie) sfx_which = sfxMega;
	// dodaj do score
	score += count_destructive(eliminacja);
	
	
	// eliminacja mega B
	eliminacja = wrogowiemegaB1 & pociskiB;
	wrogowiemegaB1 &= ~eliminacja;
	pociskiB &= ~eliminacja;
	
	// SFX
	if (eliminacja) if (sfx_which!=sfxZycie) sfx_which = sfxMega;
	// dodaj do score
	score += count_destructive(eliminacja);
	
	eliminacja = wrogowiemegaB2 & pociskiB;
	wrogowiemegaB2 &= ~eliminacja;
	pociskiB &= ~eliminacja;
	
	// SFX
	if (eliminacja) if (sfx_which!=sfxZycie) sfx_which = sfxMega;
	// dodaj do score
	score += count_destructive(eliminacja);
}

void ruch()
{	
	
	static uint8_t co2 = 0;
	static uint8_t megaliczA = 0;
	static uint8_t megaliczB = 0;
	if(!co2){
		
		// ruch mega
		wrogowiemegaA1 >>= 1;
		wrogowiemegaA2 >>= 1;
		wrogowiemegaB1 >>= 1;
		wrogowiemegaB2 >>= 1;
		
		//zamiana miejsc
		if((fast_rand() < 25)&!(wrogowiemegaA2&wrogowieB)&!(wrogowiemegaB2&wrogowieA)){
			uint16_t wrog_temp = wrogowieA;
			wrogowieA = wrogowieB;
			wrogowieB = wrog_temp;
		} else{
			// ruch
			wrogowieA >>= 1;
			wrogowieB >>= 1;
		}
		
		// spawnowanie wrogow A
		if(megaliczA == 0){
			if(fast_rand() < trudnosc_mega)
			{
				// spawn pierwszy mocny
				megaliczA = 1;
				wrogowiemegaA1 |= 1<<15;
				wrogowiemegaA2 |= 1<<15;
			} 
			//spawn zwykly A
			else wrogowieA |= (fast_rand() < trudnosc)<<15; 
		} else
		{
			// spawn drugi mocny
			wrogowiemegaA1 |= 1<<15;
			wrogowiemegaA2 |= 1<<15;
			megaliczA = 0;
		}
		
		// spawnowanie wrogow B
		if(megaliczB == 0){
			if(fast_rand() < trudnosc_mega)
			{
				// spawn pierwszy mocny
				megaliczB = 1;
				wrogowiemegaB1 |= 1<<15;
				wrogowiemegaB2 |= 1<<15;
			}
			//spawn zwykly B
			else wrogowieB |= (fast_rand() < trudnosc)<<15;
		} else
		{
			// spawn drugi mocny
			wrogowiemegaB1 |= 1<<15;
			wrogowiemegaB2 |= 1<<15;
			megaliczB = 0;
		}
		
		check_i_eliminacja();
	}
	//co2^=1;
	if (co2++==2) co2 = 0;
	
	// zycie za pociski
	if(pociskiA & (1<<15)) {zycie--; sfx_which=sfxZycie;}
	if(pociskiB & (1<<15)) {zycie--; sfx_which=sfxZycie;}
	
	// zycie za wrogow
	if(wrogowieA & 1) {zycie--; wrogowieA &= ~1; sfx_which=sfxZycie;}
	if(wrogowieB & 1) {zycie--; wrogowieB &= ~1; sfx_which=sfxZycie;}
		
	// zycie za mocnych wrogow
	if(wrogowiemegaA2 & 1) {zycie--; wrogowiemegaA2&= ~1; sfx_which=sfxZycie;}
	if(wrogowiemegaB2 & 1) {zycie--; wrogowiemegaB2&= ~1; sfx_which=sfxZycie;}
	
	pociskiA <<= 1;
	pociskiB <<= 1;
	
	// spawnowanie pociskow
	static uint8_t co2strzel = 1;
	if(strzel == 1)
	{
		if (co2strzel++==1) co2strzel = 0;
		else
		{
			// SFX
			if (sfx_which<sfxWrog) sfx_which = sfxStrzal;
			switch (pozycja)
			{
				case 0: pociskiA |= 2;
				break;
				case 1: pociskiB |= 2;
				break;
			}
		}
		
	} else co2strzel = 1;
	
	check_i_eliminacja();
}

void LCD_ekran_odswiez()
{
	uint8_t megaA = 0;
	uint8_t megaB = 0;
	
	for (uint8_t i = 1; i < 16; i++)
	{
		// uzupelnianie linii A
		if(wrogowiemegaA2 & 1<<i)
		{
			// mega
			if(megaA++ == 0)
			LCD_zpA(znMegaPrz,i);
			else {
			LCD_zpA(znMegaTyl,i);
			megaA = 0;
			}
		} else{
			if(wrogowieA & 1<<i) 
			LCD_zpA(znWrog,i);
			else if(pociskiA & 1<<i) 
			LCD_zpA(znPocisk,i);
			else
			LCD_zpA(znNic,i);
		}
		
		// uzupelnianie linii B
		if(wrogowiemegaB2 & 1<<i)
		{
			// mega
			if(megaB++ == 0)
			LCD_zpB(znMegaPrz,i);
			else {
			LCD_zpB(znMegaTyl,i);
			megaB = 0;
			}
		} else{
			if(wrogowieB & 1<<i)
			LCD_zpB(znWrog,i);
			else if(pociskiB & 1<<i)
			LCD_zpB(znPocisk,i);
			else
			LCD_zpB(znNic,i);
		}
	}
}

void seg7_wyswietl()
{
	static uint8_t i = 3;
	//wylacz poprzednie
	wybor |= (1<<i);
	
	if(i++==6) i = 3;
	dane[0] = zycie;
	
	dane[3]=score%10;
	dane[2]=(score/10)%10;
	dane[1]=(score/100)%10;
	
	segmenty = seg7[dane[i-3]];
	
	//wlacz nowe
	wybor &= ~(1<<i);
}

void seg7_init()
{
	wyborDDR |= (1<<3)|(1<<4)|(1<<5)|(1<<6);
	segmentyDDR |= 0xff;
}

void ustawienie_poczatkowe(){
	szybkosc_gry = 6000;
	OCR1A = szybkosc_gry;
	
	wrogowieA = 0;
	wrogowieB = 0;
	pociskiA = 0;
	pociskiB = 0;
	eliminacja = 0;
	wrogowiemegaA1 = 0;
	wrogowiemegaA2 = 0;
	wrogowiemegaB1 = 0;
	wrogowiemegaB2 = 0;
	
	//przykladowe rozlozenie
	//pociskiB = 1;
	wrogowieA = 1<<15;
	//pociskiA  = 5;
	wrogowieB = 1<<15;
	
	zycie = 9;
	score = 0;
	
	pozycja = 0;
	strzel = 0;
	
	// pierwsza pozycja gracza
	LCD_zpA(znGracz,0);
	LCD_zpB(znNic,0);
}

void SFX_zajmij_sie()
{
	switch(sfx_which)
	{
		case sfxStrzal:
		play_sfx(100, 8000, -20); //tradycyjny
		//play_sfx(40, 8000, -64); //basowy dziwny
		break;
		
		case sfxWrog:
		play_sfx(20, 860, 1);
		break;
		
		case sfxMega:
		play_sfx(100, 640, -4);
		break;
		
		case sfxZycie:
		play_sfx(100, 340, 0);
		break;
		
		case 0:
		switch(cykl_255 % 4)
		{
			/*
			case 0: 
			play_sfx(8,466,0);
			break;
			case 1:
			//play_sfx(8,587,0);
			break;
			case 2:
			play_sfx(8,587,0);
			//play_sfx(8,740,0);
			break;
			case 3:
			//play_sfx(5,523,0);
			break;
			*/
			
			case 0:
			play_sfx(20,800,-32);
			break;
			case 1:
			play_sfx(20,800,-32);
			break;
			case 2:
			play_sfx(1,940,-5);
			break;
			case 3:
			play_sfx(20,240,10);
			break;
			
		}
	}
	sfx_which = 0;
}

int main(void)
{	
	
	ADC_init();
	// seed dla randa
	pseudo_rand = entropia();
	if (pseudo_rand == 0) pseudo_rand = 0x12; // nie moze byc 0
	
	buzzer_init_software();
	Przyciski_init();
	LCD_init();
	T0_init();
	T1_init();
	
	seg7_init();
	
	//faza = 2;
	
	OCR1A = szybkosc_animacji;
	
    while (1) 
    {
		
		switch(faza) {
		
		case  0: // MENU GLOWNE
			
			if (cykl == 1)
			{
				if(klatka++==3)klatka = 0;
				switch(klatka)
				{
					case 0:
					case 2:
						NapisA("  ^o o^  feline ");
						NapisB(" INVADERS  >w<  ");
						break;
					case 1:
					case 3:
						NapisA("  ^u u^  feline ");
						NapisB(" INVADERS  owo  ");
						break;
					case 4:
						NapisA(" Pressbutt> w <");
						NapisB(" ^o o^ to start");
						break;
					
				}
				cykl_255++;
				switch(cykl_255%8)
				{
					case 0:
					play_sfx(90,440,-2);
					break;
					case 1:
					play_sfx(2,500,0); //hh
					break;
					case 2:
					play_sfx(450,440,0);
					break;
					case 3:
					play_sfx(350,659,1);
					break;
					case 4:
					play_sfx(20,554,0); //hh?
					break;
					
					case 5:
					play_sfx(2,500,0); //hh
					break;
					case 6:
					play_sfx(60,400,3); //hh
					break;
					case 7:
					play_sfx(2,500,0); //hh
					break;
				}
				cykl = 0;
			}
		
		
			if (zmlewoprawo == 2){
				faza = 1;
				while (cykl == 0);
				cykl = 0;
				
				ustawienie_poczatkowe();
				
				zmlewoprawo = 3;
			}
			
			if (zmstrzel == 2){
				zmstrzel = 3;
			}
			if (zmstrzel == 5){
				zmstrzel = 0;
			}
			
			break;
			
		case 1: // GRA (KUXWA)
			//T1 cykl
			if(cykl == 1){
				ruch();
				LCD_ekran_odswiez();
				SFX_zajmij_sie();
		
				// co 255 nowy rand seed
				if(cykl_255++==0xff)
				{
					pseudo_rand ^= entropia(); //XOR
					if (pseudo_rand == 0) pseudo_rand = 0xA5; // nie moze byc 0
				}
		
				if(szybkosc_gry > 12000) szybkosc_gry -= przyspieszanie*2;
				else if(szybkosc_gry > 10000) szybkosc_gry -= przyspieszanie;
				else if(szybkosc_gry > 90000) szybkosc_gry -= przyspieszanie/2;
				else if(szybkosc_gry > 8000) szybkosc_gry -= przyspieszanie/4;
				else if(szybkosc_gry > 6000) szybkosc_gry -= przyspieszanie/8;
				else if(szybkosc_gry > 4000) szybkosc_gry -= przyspieszanie/16;
				else if(szybkosc_gry > 2000) szybkosc_gry -= przyspieszanie/32;
				OCR1A = szybkosc_gry;
		
				if (zycie == 0)
				{
					OCR1A = szybkosc_animacji;
					faza = 2;
				}
		
				cykl = 0;
			}
		
			//przycisk lewo prawo
			if(zmlewoprawo == 2){
			switch(pozycja){
				case 0: pozycja = 1;
						LCD_zpA(znNic,0);
						LCD_zpB(znGracz,0);
						break;
				case 1: pozycja = 0;
						LCD_zpA(znGracz,0);
						LCD_zpB(znNic,0);
						break;
			}
			zmlewoprawo = 3;	
			}
		
			//przycisk strzel
			if (zmstrzel == 2) //start
			{
				sfx_which = sfxStrzal;
				SFX_zajmij_sie();
				
				switch (pozycja) //pierwszy strzal
				{
					case 0: pociskiA |= 2;
							LCD_zpA(znPocisk,1);
							break;
					case 1: pociskiB |= 2;
							LCD_zpB(znPocisk,1);
							break;
				}
			
				strzel = 1;
				zmstrzel = 3;
			} else //koniec strzelania
			if (zmstrzel == 5)
			{
				strzel = 0;
				zmstrzel = 0;
			}
			break;
		
		case 2: // GAME OVER
			if (cykl == 1)
			{
				cykl_255++;
				if(klatka++==3)klatka = 0;
				switch(klatka)
				{
					case 0:
					case 2:
					NapisA("  ^o o^   game  ");
					NapisB("   OVER    >w<  ");
					break;
					case 1:
					case 3:
					NapisA("  ^u u^   GAME  ");
					NapisB("   over    owo  ");
					break;
					case 4:
					NapisA(" Pressbutt> w <");
					NapisB(" ^o o^ to start");
					break;
					
				}
				switch(cykl_255%8)
				{
					case 0:
					play_sfx(90,440,2);
					break;
					case 1:
					play_sfx(2,500,0); //hh
					break;
					case 2:
					play_sfx(850,392,0);
					break;
					case 3:
					play_sfx(850,587,0);
					break;
					case 4:
					play_sfx(20,523,0);
					break;
					
					case 5:
					play_sfx(2,500,0); //hh
					break;
					case 6:
					play_sfx(2,500,0); //hh
					break;
					case 7:
					play_sfx(2,500,0); //hh
					break;
				}
				cykl = 0;
			}
			
			
			if (zmlewoprawo == 2){
				faza = 0;
				zmlewoprawo = 3;
			}
			
			if (zmstrzel == 2){
				zmstrzel = 3;
			}
			if (zmstrzel == 5){
				zmstrzel = 0;
			}
			
			break;
		
		}
		
		//T0 sprawdz przyciski i wyswietl seg7
		if (sprawdzprz == 1)
		{
			sprawdz_przyciski();
			
			seg7_wyswietl();
			
			sprawdzprz = 0;
		}
    }
}

