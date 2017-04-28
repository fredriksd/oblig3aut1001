/*
 * oblig3.c
 *
 * Created: 20.04.2017 15:08:42
 * Author : fsa011
 */ 

#define F_CPU 14745600

#include <avr/io.h>
#include <avr/interrupt.h>
#include "serlcd.h"

void received(char rx_value[]);
void uart_send();
char A_array[50], B_array[18], C_array[7], D_array[50], C_to_send[6];
unsigned char lf_cnt = 0, rx_counter = 0, sendcounter = 1;


ISR(USART0_TX_vect) {
	if(sendcounter < 6)
		UDR0 = C_to_send[sendcounter++];
	else
		sendcounter = 0;
}


ISR(USART0_RX_vect) {
	unsigned char uart = UDR0;

	if (uart == '\n')
	{
		lf_cnt++;
		rx_counter = 0;
	}
	else
	{
		switch(lf_cnt)
		{
			case 0:
			A_array[rx_counter++] = uart;
			break;
			case 1:
			B_array[rx_counter++] = uart;
			break;
			case 2:
			C_array[rx_counter++] = uart;
			break;
			case 3:
			D_array[rx_counter++] = uart;
			break;
			case 5:
			lf_cnt = 0;
			break;
			
			default:
			break;
		}	
	}
	
}


int main(void)
{
	
	init_lcd();
	lcd_cursoron();
	DDRB = 0xf0;
	PORTB = 0x0f;
	DDRC = 0x00;
	UCSR0B = (1<<RXEN0)|(1<<TXEN0)|(1<<RXCIE0)|(1<<TXCIE0);
	UCSR0C = (1<<UCSZ01)|(1<<UCSZ00);
	UBRR0L = 95;
	UBRR0H = 0;
	sei();
	
	while (1)
	{
		if(UCSR0A && (1<<RXC0))
		{
			while(!(UCSR0A & (1<<UDRE0))); //Hvis UDR-registeret ikke er tomt: vent.
			received(A_array);
			received(B_array);
			received(C_array);
			uart_send();
		}
	}
}


void received(char rx_value[]) {
	/*Behandler informasjonen mottatt
	basert pÂ arrayet som mottas*/
	unsigned char cnt = 1, buffer_cnt = 0;
	char buffer[50];
	switch (rx_value[0])
	{
	case 'A':
		while(cnt <=18)
		{
			buffer[buffer_cnt++] = rx_value[cnt++];
		}
		lcd_printline(0,0,buffer);
		break;
	case 'B':
		while(cnt <=18)
		{
			buffer[buffer_cnt++] = rx_value[cnt++];
		}
		lcd_printline(1,0,buffer);
		break;
	case 'C':
		if(0x0f&rx_value[1])
			PORTB |= (0x0f&rx_value[1])<<4;
		else
			PORTB &= ~(0x10);
		if (0x0f&rx_value[2])
			PORTB |= (0x0f&rx_value[2])<<5;
		else
			PORTB &= ~(0x20);
		if (0x0f&rx_value[3])
			PORTB |= (0x0f&rx_value[3])<<6;
		else
			PORTB &= ~(0x40);
		if (0x0f&rx_value[4])
			PORTB |= (0x0f&rx_value[4])<<7;
		else
			PORTB &= ~(0x80);
 		break;
	case 'D':
		break;
	default:
		break;
	}
}

void uart_send() {
	//Appending data for transmit
	unsigned char i;
	C_to_send[0] = 'C';
	for (i = 1; i < 4; i++)
	{
		if(PINB&(1<<(i-1))==0)
			C_to_send[i] = '1';
		else
			C_to_send[i] = '0';
	}
    
	UDR0 = C_to_send[0];
	
// 	C_to_send[0] = 'C';
// 	C_to_send[1] = (~PINB&0x01) + '0';
// 	C_to_send[2] = (PINB&0x02) + '0';
// 	C_to_send[3] = (PINB&0x04) + '0';
// 	C_to_send[4] = (PINB&0x08) + '0';
// 	C_to_send[4] = '\n';
// 	UDR0 = C_to_send[0]; //Begin transmit
}
