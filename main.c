/*
* oblig3.c
*	OBLIG 3 
*	Gjennomf¯rt av Fredrik Sandhei og Mathias HaukÂs
*
*/

#define F_CPU 14745600

#include <avr/io.h>
#include <avr/interrupt.h>
#include "serlcd.h"
#include <util/delay.h>

//Funksjonsprotyper
void received(char input);
void uart_send();

char A_array[18], B_array[18], C_array[7], D_array[7], C_to_send[6], D_to_send[4], A_to_send[8], B_to_send[22] = { "BDerp herp\n" }, send_array[50];
char rec_array[20];
volatile unsigned char rx_counter = 0, sendcounter = 1, receive_done, add;
unsigned int freq, pwm_on, voltage, adc_reader;

ISR(ADC_vect) {
	adc_reader = ADCW;
	voltage = 5000*(long)adc_reader/1024;
}

ISR(TIMER2_COMPA_vect)
{
	OCR2A = OCR2A + add;
}


ISR(USART0_TX_vect) {

	if (send_array[sendcounter]) {
		UDR0 = send_array[sendcounter++];
	}
	else {
		sendcounter = 1;
	}

}


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
	DDRB = 0xf0;
	PORTB = 0x0f;
	DDRC = 0x10;
	PORTC = 0x0f;
	DDRD = 0xf0;

	//Timer 2 - konfigurasjon
	TCCR2B = (1<<CS22);
	TIMSK2 = (1 << OCIE2A);
	//UART - konfigurasjon
	//Rx enable, Tx enable, Rx interrupt enable, Tx interrupt enable
	UCSR0B = (1 << RXEN0) | (1 << TXEN0) | (1 << RXCIE0) | (1 << TXCIE0);
	//Bitrate 9600 - standard hastighet pÂ rs-232 
	UBRR0H = 0;
	UBRR0L = 95;
	//ADC - konfigurasjon
	//Bruker ADC0 som input
	ADMUX = 0x00;
	//ADC enable, interrupt enable, clock speed = 115200Hz
	ADCSRA = (1<<ADEN)|(1<<ADIE)|(1<<ADPS2)|(1<<ADPS1)|(1<<ADPS0);
	
	sei();
	while (1)
	{
		//Mottar data f¯rst fra PC
		//For sÂ Â sende data fra uK til PC
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
	/*Behandler informasjonen mottatt
	basert pÂ arrayet som mottas*/
	
	switch (input)
	{
	case 'A':
		lcd_printline(0, 0, A_array);

		break;

	case 'B':
		lcd_printline(1, 0, B_array);
		break;

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
			//Dersom pwm-flagget ikke er h¯y -> sett h¯y.
			//Gj¯r det slik at denne kodesnutten bare kj¯res
			//en gang. 
			if(pwm_on == 0)
			{
				TCCR2A |= (1 << COM2A0);
				pwm_on = 1;

			}
			freq = (D_array[1] - '0') * 1000;
			freq += (D_array[2] - '0') * 100;
			freq += (D_array[3] - '0') * 10;
			freq += (D_array[4] - '0');			

			add = (long)115200/freq;
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
//Denne metoden gj¯r ISR-transmit liten og enkelt implementert.

void uart_send() {
	//Appending data for transmit
	unsigned char lf = 0, sep_cntr = 0;

	//Legger inn pakke A til sending'
	//Start conversion for ADC
	ADCSRA ^= (1<<ADSC); 

	A_to_send[0] = 'A';
	A_to_send[1] = (voltage/1000 + 0x30);
	A_to_send[2] = ((voltage%1000)/100 + 0x30);
	A_to_send[3] = ((voltage%100)/100 + 0x30);
	A_to_send[4] = ((voltage%10) + 0x30);
	A_to_send[5] = 'm';
	A_to_send[6] = 'V';
	A_to_send[7] = '\n';
	
	//Legger inn pakke C til sending
	
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
	if(PINC&0x01)
		address_val += 8;
	if(PINC&0x02)
		address_val += 4;
	if(PINC&0x04)
		address_val += 2;
	if(PINC&0x08)
		address_val += 1;
		
	if(address_val >= 57)
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
	//Dette vil f¯re til at TXC-flagget blir h¯y,
	//og en TXC ISR vil gjennomf¯res.
	UDR0 = send_array[0]; 
}