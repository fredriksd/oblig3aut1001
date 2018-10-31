/*
* oblig3.c
*   OBLIG 3
*   Gjennomført av Fredrik Sandhei og Mathias Haukås
*
*/

#define F_CPU 14745600

//Inkluderinger av ulike bibliotek
#include <avr/io.h>
#include <avr/interrupt.h>
#include "serlcd.h"
#include <util/delay.h>

//Funksjonsprotyper
void received(char input);
void uart_send();

//Deklarasjon av variabler
char A_array[18], B_array[18], C_array[7], D_array[7], C_to_send[6], D_to_send[4], A_to_send[8], send_array[50];
char rec_array[20];
volatile unsigned char rx_counter = 0, sendcounter = 1, receive_done, add;
unsigned int freq, pwm_on, voltage, adc_reader;
//Fri tekst i B
char B_to_send[22] = { "BEmmy slår..\n" };

//ADC interrupt
ISR(ADC_vect) {
	//Bruker et pseudoregister sammenslått av ACDL-og ADCH - registrene
	adc_reader = ADCW;
	voltage = 5000 * (long)adc_reader / 1023;  //regner om til mV
}

//Timer2 Output Compare Interrupt 
//som produserer kvadratbølger med frekvens
//mellom 1000 - 3000Hz
ISR(TIMER2_COMPA_vect)
{
	OCR2A = OCR2A + add;
}

//Shifter ut innholdet fra send_array() som ble lagt sammen i uart_send()
ISR(USART0_TX_vect) {
	//Dersom det er noen elementer i index sendcounter, shift ut på UDR.
	if (send_array[sendcounter]) {
		UDR0 = send_array[sendcounter++];
	}//Hvis ikke, tilbakestill sendcounter
	else {
		sendcounter = 1;
	}
}

//Behandler dataen mottatt over UDR-registeret fra PC
//og legger dataen inn i sine respektive arrayer.
ISR(USART0_RX_vect) {
	unsigned char uart = UDR0;
	unsigned char i = 0;
	if (uart == '\n') {
		if (rec_array[0] == 'A')
			for (i = 0; i<16; i++)
				A_array[i] = rec_array[i + 1];
		else if (rec_array[0] == 'B')
			for (i = 0; i<16; i++)
				B_array[i] = rec_array[i + 1];
		else if (rec_array[0] == 'C')
			for (i = 0; i<4; i++)
				C_array[i] = rec_array[i + 1];
		else if (rec_array[0] == 'D')
			for (i = 0; i<5; i++)
				D_array[i] = rec_array[i + 1];
		else if (rec_array[0] == 'R')
			receive_done = 1;
		rx_counter = 0;
	}
	else
		rec_array[rx_counter++] = uart;
}

int main(void)
{
	init_lcd();
	lcd_cursoron();
	//Oppsett av porter, input/output
	//Da de ulike enhetene var koblet til jord
	//vil uK-portene registere logisk 0.
	//Det er derfor viktig å aktivere de interne pull-up-resistanene
	//til uK-portene, for å hindre at verdiene som blir lest bare 

	DDRB = 0xf0;
	PORTB = 0x0f;
	DDRC = 0x10;
	PORTC = 0x0f;
	DDRD = 0xf0;

	//Timer 2 - konfigurasjon
	TCCR2B = (1 << CS22);
	TIMSK2 = (1 << OCIE2A);
	//UART - konfigurasjon
	//Rx enable, Tx enable, Rx interrupt enable, Tx interrupt enable
	UCSR0B = (1 << RXEN0) | (1 << TXEN0) | (1 << RXCIE0) | (1 << TXCIE0);
	//Bitrate 9600 - standard hastighet på Rs-232 
	UBRR0H = 0;
	UBRR0L = 95;
	//ADC - konfigurasjon
	//Bruker ADC0 som input
	ADMUX = 0x00;
	//ADC enable, interrupt enable, clock speed = 115200Hz
	ADCSRA = (1 << ADEN) | (1 << ADIE) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
	//Assembly - Global Interrupt Enable
	sei();
	while (1)
	{
		//Mottar data først fra PC
		//For så å sende data fra uK til PC
		//ved hjelp av halv-dupleks-protokollen/Bernt-protokollen
		if (receive_done)
		{
			received('A');
			received('B');
			received('C');
			received('D');
			uart_send();
			receive_done = 0;
		}
	}
}
void received(char input) {
	//Behandler informasjonen mottatt
	//basert på arrayet som mottas

	switch (input)
	{
	case 'A':
		lcd_printline(0, 0, A_array);
		break;

	case 'B':
		lcd_printline(1, 0, B_array);
		break;

		//Setter portene høy basert på 
		//innholdet i C_array
	case 'C':
		if (0x0f & C_array[0])
			PORTB |= (0x0f & C_array[0]) << 4;
		else
			PORTB &= ~(0x10);
		if (0x0f & C_array[1])
			PORTB |= (0x0f & C_array[1]) << 5;
		else
			PORTB &= ~(0x20);
		if (0x0f & C_array[2])
			PORTB |= (0x0f & C_array[2]) << 6;
		else
			PORTB &= ~(0x40);
		if (0x0f & C_array[3])
			PORTB |= (0x0f & C_array[3]) << 7;
		else
			PORTB &= ~(0x80);
		break;

	case 'D':

		if (D_array[0] == '1')
		{
			//Dersom pwm-flagget ikke er høy -> sett høy.
			//Gjør det slik at denne kodesnutten bare kjøres
			//en gang. 
			if (pwm_on == 0)
			{
				TCCR2A |= (1 << COM2A0);
				pwm_on = 1;
			}
			//Da hvert siffer av frekvensen er
			//lagret i hver sin posisjon i arrayet
			//må de subtraheres med 48 (eller '0')
			//for så å multipliseres med nødvendig verdi
			freq = (D_array[1] - '0') * 1000;
			freq += (D_array[2] - '0') * 100;
			freq += (D_array[3] - '0') * 10;
			freq += (D_array[4] - '0');
			//Regner ut klokketiks med F_CPU_/64.
			add = 115200 / freq;
		}
		else
			if (pwm_on)
			{
				//Toggler av OC2A (Output Compare 2A)
				TCCR2A &= ~(1 << COM2A0);
				pwm_on = 0;
			}
		break;
	default:
		break;
	}
}

//Legger til datapakkene som skal sendes fra uK til sine respektive pakker
//og deretter legger de sammen til et array som sendes ved UART. 
//Denne metoden gjør ISR-transmit liten og enkelt implementert.
void uart_send() {
	//Appending data for transmit
	unsigned char lf = 0, sep_cntr = 0;

	//Legger inn pakke A til sending'
	//Start conversion for ADC
	//Dette fører til en ADC-ISR-rutine
	ADCSRA ^= (1 << ADSC);

	A_to_send[0] = 'A';
	A_to_send[1] = (voltage / 1000 + 0x30);
	A_to_send[2] = ((voltage % 1000) / 100 + 0x30);
	A_to_send[3] = ((voltage % 100) / 100 + 0x30);
	A_to_send[4] = ((voltage % 10) + 0x30);
	A_to_send[5] = 'm';
	A_to_send[6] = 'V';
	A_to_send[7] = '\n';

	//Legger inn pakke C til sending

	//Sjekker om bryteren er trykket ned
	C_to_send[0] = 'C';
	for (unsigned char i = 1; i < 5; i++)
	{
		if ((PINB&(1 << (i - 1))) == 0)
			C_to_send[i] = '1';
		else
			C_to_send[i] = '0';
	}
	C_to_send[5] = '\n';


	//Legger inn pakke D til sending
	char address_val = 0x30;
	if (PINC & 0x01)
		address_val += 8;
	if (PINC & 0x02)
		address_val += 4;
	if (PINC & 0x04)
		address_val += 2;
	if (PINC & 0x08)
		address_val += 1;

	if (address_val >= 57)
		address_val += 7;

	D_to_send[0] = 'D';
	D_to_send[1] = address_val;
	D_to_send[2] = '\n';

	//Legger alt sammen i et monster - array. Big time
	for (unsigned char i = 0; i < 50; i++)
	{
		if (lf == 0)
		{
			send_array[i] = A_to_send[sep_cntr];
			if (A_to_send[sep_cntr] == '\n')
			{
				//Dersom linefeed - inkrementer lf, nullstill sep_cntr og hoppe til neste iterasjon
				lf = 1;
				sep_cntr = 0;
				continue;
			}
		}
		if (lf == 1)
		{
			send_array[i] = B_to_send[sep_cntr];
			if (B_to_send[sep_cntr] == '\n')
			{
				//Dersom linefeed - inkrementer lf, nullstill sep_cntr og hoppe til neste iterasjon
				lf = 2;
				sep_cntr = 0;
				continue;
			}
		}
		if (lf == 2)
		{
			send_array[i] = C_to_send[sep_cntr];
			if (C_to_send[sep_cntr] == '\n')
			{
				//Dersom linefeed - inkrementer lf, nullstill sep_cntr og hoppe til neste iterasjon
				lf = 3;
				sep_cntr = 0;
				continue;
			}
		}

		if (lf == 3)
		{
			send_array[i] = D_to_send[sep_cntr];
			if (D_to_send[sep_cntr] == '\n')
			{
				//Dersom linefeed - inkrementer lf, nullstill sep_cntr og hoppe til neste iterasjon
				lf = 4;
				sep_cntr = 0;
				continue;
			}
		}
		sep_cntr++;
	}
	//Begynner transmit.
	//Dette vil føre til at TXC-flagget blir høy,
	//og en TXC ISR vil gjennomføres.
	UDR0 = send_array[0];
}