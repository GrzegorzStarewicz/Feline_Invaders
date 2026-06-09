/*
 * ANG-GANG-Feline-Invaders-b2.c
 *
 * Created: 03.06.2026 11:19:22
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
#define znWrog 'X'
#define znPocisk '='
#define znNic ' '
#define znOgon '-'
#define znOgonKoniec '&'
#define znMegaPrz 'W'
#define znMegaTyl 'o'

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
uint8_t trudnosc = 255 / 4; // czyli 1/4 jako poczatkowa
//szybkosc timera 1 gry 10500 - szybkie, 5000 - bardzo szybkie, 20000 - wolne
uint16_t szybkosc_gry = 14500;
#define przyspieszanie 64

//dane gry
uint8_t zycie = 9;
uint16_t score = 0;

//seg7 na 3 4 5 6 ???
uint8_t dane[] = {1,2,3,4};
#define segmentyDDR DDRA //portA I/O
#define segmenty PORTA //diody na porcie A
#define wyborDDR DDRB
#define wybor PORTB	
uint8_t seg7[] = { 0xc0,0xf9,0xa4,0xb0,0x99,0x92,0x82,0xd8,0x80,0x90 };

uint8_t cykl_255 = 0;

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
	
	// dodaj do score
	score += count_destructive(eliminacja);
	
	// eliminacja B
	eliminacja = wrogowieB & pociskiB;
	wrogowieB &= ~eliminacja;
	pociskiB &= ~eliminacja;
	
	// dodaj do score
	score += count_destructive(eliminacja);
	
	
	// eliminacja mega A
	eliminacja = wrogowiemegaA1 & pociskiA;
	wrogowiemegaA1 &= ~eliminacja;
	pociskiA &= ~eliminacja;
	
	// dodaj do score
	score += count_destructive(eliminacja);
	
	eliminacja = wrogowiemegaA2 & pociskiA;
	wrogowiemegaA2 &= ~eliminacja;
	pociskiA &= ~eliminacja;
	
	// dodaj do score
	score += count_destructive(eliminacja);
	
	
	// eliminacja mega B
	eliminacja = wrogowiemegaB1 & pociskiB;
	wrogowiemegaB1 &= ~eliminacja;
	pociskiB &= ~eliminacja;
	
	// dodaj do score
	score += count_destructive(eliminacja);
	
	eliminacja = wrogowiemegaB2 & pociskiB;
	wrogowiemegaB2 &= ~eliminacja;
	pociskiB &= ~eliminacja;
	
	// dodaj do score
	score += count_destructive(eliminacja);
}

void ruch()
{	
	
	static uint8_t co2 = 0;
	static uint8_t megaliczA = 0;
	static uint8_t megaliczB = 0;
	if(co2){
		// ruch
		wrogowieA >>= 1;
		wrogowieB >>= 1;
		
		wrogowiemegaA1 >>= 1;
		wrogowiemegaA2 >>= 1;
		wrogowiemegaB1 >>= 1;
		wrogowiemegaB2 >>= 1;
		
		// spawnowanie wrogow A
		if(megaliczA == 0){
			if(fast_rand() < trudnosc / 4)
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
			if(fast_rand() < trudnosc / 4)
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
	co2^=1;
	
	// zycie za pociski
	if(pociskiA & (1<<15)) zycie--;
	if(pociskiB & (1<<15)) zycie--;
	
	// zycie za wrogow
	if(wrogowieA & 1) {zycie--; wrogowieA &= ~1;}
	if(wrogowieB & 1) {zycie--; wrogowieB &= ~1;}
		
	// zycie za mocnych wrogow
	if(wrogowiemegaA2 & 1) {zycie--; wrogowiemegaA2&= ~1;}
	if(wrogowiemegaB2 & 1) {zycie--; wrogowiemegaB2&= ~1;}
	
	pociskiA <<= 1;
	pociskiB <<= 1;
	
	// spawnowanie pociskow
	if(strzel == 1)
	{
		switch (pozycja)
		{
			case 0: pociskiA |= 2;
					break;
			case 1: pociskiB |= 2;
					break;
		}
	}
	
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

int main(void)
{	
	
	ADC_init();
	// seed dla randa
	pseudo_rand = entropia();
	if (pseudo_rand == 0) pseudo_rand = 0x12; // nie moze byc 0
	
	Przyciski_init();
	LCD_init();
	T0_init();
	T1_init();
	
	seg7_init();
	
	//przykladowe rozlozenie
	//pociskiB = 1;
	wrogowieA = 1<<15;
	//pociskiA  = 5;
	wrogowieB = 1<<15;
	
	// pierwsza pozycja gracza
	LCD_zpA(znGracz,0);
	LCD_zpB(znNic,0);
	
    while (1) 
    {
		//T1 cykl
		if(cykl == 1){
		ruch();
		LCD_ekran_odswiez();
		
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
		} else //koniec
		if (zmstrzel == 5)
		{
			strzel = 0;
			zmstrzel = 0;
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

