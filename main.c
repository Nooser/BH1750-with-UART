#define F_CPU 1000000L
#include <stdio.h>
#include <stdlib.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <avr/io.h>
#include <string.h>

//#define UART_BAUD_RATE 2400
#include "bh1750.h"

/*#define UART_BAUD_RATE 9600
#include "uart.h"*/

//zmienne dot. odbioru danych
volatile unsigned char odb_x;       //odebrana liczba X
volatile unsigned char odb_flaga =0;//flaga informująca main o odebraniu liczby

//bufor znaków ze wzorem funkcji, który wysyłamy po starcie programu
volatile unsigned int usart_bufor_ind;           //indeks bufora nadawania
char usart_bufor[100] = "Dane"; //bufor nadawania

void usart_inicjuj(void)
{
	//definiowanie parametrów transmisji za pomocą makr zawartych w pliku
	//nagłówkowym setbaud.h. Jeżeli wybierzesz prędkość, która nie będzie
	//możliwa do realizacji otrzymasz ostrzeżenie:
	//#warning "Baud rate achieved is higher than allowed"
	
	#define BAUD 9600        //tutaj podaj żądaną prędkość transmisji
	#include <util/setbaud.h> //linkowanie tego pliku musi być
	//po zdefiniowaniu BAUD
	
	//ustaw obliczone przez makro wartości
	UBRRH = UBRRH_VALUE;
	UBRRL = UBRRL_VALUE;
	#if USE_2X
	UCSRA |=  (1<<U2X);
	#else
	UCSRA &= ~(1<<U2X);
	#endif
	
	
	//Ustawiamy pozostałe parametry moduł USART
	//U W A G A !!!
	//W ATmega8, aby zapisać do rejestru UCSRC należy ustawiać bit URSEL
	//zobacz także: http://mikrokontrolery.blogspot.com/2011/04/avr-czyhajace-pulapki.html#avr_pulapka_2
	UCSRC = (1<<URSEL) | (1<<UCSZ1) | (1<<UCSZ0);  //bitów danych: 8
	//bity stopu:  1
	//parzystość:  brak
	//włącz nadajnik i odbiornik oraz ich przerwania odbiornika
	//przerwania nadajnika włączamy w funkcji wyslij_wynik()
	UCSRB = (1<<TXEN) | (1<<RXEN) | (1<<RXCIE);
}


//--------------------------------------------------------------


ISR(USART_RXC_vect)
{
	//przerwanie generowane po odebraniu bajtu
	odb_x = UDR;   //zapamiętaj odebraną liczbę
	odb_flaga = 1; //ustaw flagę odbioru liczby dla main()
}


//--------------------------------------------------------------

ISR(USART_UDRE_vect){
	
	//przerwanie generowane, gdy bufor nadawania jest już pusty,
	//odpowiedzialne za wysłanie wszystkich znaków z tablicy usart_bufor[]
	
	//sprawdzamy, czy bajt do wysłania jest znakiem końca tekstu, czyli zerem
	if(usart_bufor[usart_bufor_ind]!= 0){
		
		//załaduj znak do rejestru wysyłki i ustaw indeks na następny znak
		UDR = usart_bufor[usart_bufor_ind++];
		
		}else{
		
		//osiągnięto koniec napisu w tablicy usart_bufor[]
		UCSRB &= ~(1<<UDRIE); //wyłącz przerwania pustego bufora nadawania
	}
}


//--------------------------------------------------------------


void wyslij_wynik(void){
	
	//funkcja rozpoczyna wysyłanie, wysyłając pierwszy znak znajdujący się
	//w tablicy usart_bufor[]. Pozostałe wyśle funkcja przerwania,
	//która zostanie wywołana automatycznie po wysłaniu każdego znaku.
	
	//dodaj do tekstu wyniku znaki końca linii (CR+LF), by na
	//ekranie terminala wyniki pojawiały się w nowych liniach
	unsigned char z;
	for(z=0; z<30; z++){
		if(usart_bufor[z]==0){   //czy to koniec takstu w tablicy
			//tak znaleziono koniec tekstu, dodajemy znak CR
			usart_bufor[z]   = 13;  //znak powrotu karetki CR (Carrige Return)
			usart_bufor[z+1]  = 10; //znak nowej linii LF (Line Feed)
			usart_bufor[z+2]  = 0;  //znak końca ciągu tekstu w tablicy
			break;
		}
	}
	
	
	//Zaczekaj, aż bufor nadawania będzie pusty
	while (!(UCSRA & (1<<UDRE)));
	
	//bufor jest pusty można wysłać
	
	//następny znak do wysyłki to znak nr 1
	usart_bufor_ind = 0;
	
	//włącz przerwania pustego bufora UDR, co rozpocznie transmisję
	//aktualnej zawartości bufora
	UCSRB |= (1<<UDRIE);
	
}


int main(void) {
	char printbuff[100];

	//init uart
	//uart_init( UART_BAUD_SELECT(UART_BAUD_RATE,F_CPU) );

	//init interrupt
	//sei();

	
	
	usart_inicjuj(); //inicjuj moduł USART (RS-232)
	sei();           //włącz przerwania globalne
	
	wyslij_wynik();  //na początku wysyłamy text wzoru, który po
	//resecie jest w tablicy usart_bufor[]
	
	bh1750_init();

	for (;;) {
		char result[100];
		
		//get lux
		int lux = bh1750_getlux();

		//print value
		itoa(lux, printbuff, 10);
	    sprintf(result, " Lux: %s\n", printbuff);
	    strcpy(usart_bufor, result);
	    wyslij_wynik();
		_delay_ms(1000);
	}
	return 0;
}
