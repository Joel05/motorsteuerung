/************************************************************/
/* Basisfunktionen für die Steuerung der UART Schnittstelle */
/*															*/
/* Autor: ZKS												*/
/*															*/
/* zkslibuart.c                                             */
/*															*/
/* Versionsinfos:											*/
/* 26.8.2020, Initial release V1							*/
/*															*/
/* Major Update 16.6.2022 Interrupt Support					*/
/*															*/
/************************************************************/
#include <AVR/io.h>
#include <util/delay.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "zkslibuart.h"
#include "avr/interrupt.h"

int	_uart_put(char c, FILE * f);

#ifdef STDOUT_UART
FILE uart_str = FDEV_SETUP_STREAM(_uart_put, NULL, _FDEV_SETUP_WRITE);
#endif


static uint8_t _loc_RxFlow=0;	
static uint8_t _loc_TxFlow=0;

#ifdef UART_USE_EXCH

static uint8_t _loc_UartRxBuffer[UART_EXCH_BUF_SIZE];
static uint8_t _loc_UartRxCnt=0;	


static uint8_t _loc_UartTxBuffer[UART_EXCH_BUF_SIZE+2];
static uint8_t _loc_UartTxCnt=0;
static uint8_t _loc_UartTxBufCnt=0;

int16_t _loc_UartResult[UART_EXCH_INT_SIZE];
#endif


/****************************************************************************************/
// Lighweight code for conversion from unsigned int to Text
static uint8_t _loc_uint2txt(uint32_t  BinData, char * TextBuffer, char NDigit)
{
	uint32_t Divisor;
	uint8_t Result;
	uint8_t	DigitCnt;
	uint8_t CharCnt;
	uint8_t StartDigits;
	
	/* Init the Algorithm */
	Divisor=1000000000;
	CharCnt=0;
	StartDigits=0;
	
	for(DigitCnt=10;DigitCnt>0;DigitCnt--)
	{
		Result=(BinData/Divisor);
		BinData=BinData-Result*Divisor;
		Divisor=Divisor/10;
		
		if(NDigit!=0)
		{
			if (DigitCnt<=NDigit)
			{
				// neue Ziffer oder führende Null bei Bedarf hinzufügen
				TextBuffer[NDigit-DigitCnt]='0'+Result;
				CharCnt++;
			}
		}
		else
		{ 
			if ((Result!=0)||(StartDigits))
			{
				StartDigits=1;
				TextBuffer[CharCnt]='0'+Result;
				CharCnt++;
			}
		}
	}
	
	if(CharCnt==0)
	{
		CharCnt=1;
		TextBuffer[0]='0';
	}
	
	return CharCnt;
}

uint8_t uart_Uint2Txt(uint32_t  BinData, char * TextBuffer, char NDigit)
{
	return _loc_uint2txt(BinData,TextBuffer, NDigit);
}
/****************************************************************************************/

/*********************************************************************/
/* HW.Abhängige funktionen Funktionen							     */
/*********************************************************************/

/*********************************************************************/
// Low Level Hardware-Dependent Functions
#ifdef DEVICE_ATMEGA16

#define UART_RX_PIN 0
#define UART_TX_PIN 1
#define UART_PORT PORTD
#define UART_DDR DDRD
 
uint8_t _loc_StopRxFlow(void);

void _loc_Init(unsigned char UartBaudRate, unsigned char UartMode)
{
	
	unsigned long Baud[N_BAUDRATES] ={1200, 2400, 9600, 57600, 115200};
	unsigned long UbrrCalc;
	
	// Program the Pins
	// RX: PD0, TX: PD1
	// RX ist Eingang mit Pullup
	UART_DDR&=~(1<<UART_RX_PIN);
	UART_PORT|=(1<<UART_RX_PIN);
	
	// TX ist Ausgang auf high (Idle)
	UART_DDR|=(1<<UART_TX_PIN);
	UART_PORT|=(1<<UART_TX_PIN);
		
	
	// Berechnen des Ubrr Registers für die Baudrate
	UbrrCalc=F_CPU/(16*Baud[UartBaudRate])-1;
	
	// Set the BAUD Rate for the 12MHz System Clock
	UBRRL=(UbrrCalc & 0x000000ff);
	UBRRH=(UbrrCalc >> 8);
	
	 //Set Com Mode
	 //UCSRC: 0b10 P1 P0 T0 1 1 0
	 //T0: 0 1 Stopbit, 1 2 Stopbits
	 //P1 P0: 0 0 No Parity, 1 0 Even, 1 1 Odd
	 
	 switch(UartMode)
	 {
		 case UART_CONFIG_8N1:
		 UCSRC=0b10000110;
		 break;
		 
		 case UART_CONFIG_8N2:
		 UCSRC=0b10001110;
		 break;
		 
		 case UART_CONFIG_8E1:
		 UCSRC=0b10100110;
		 break;
		 
		 case UART_CONFIG_8E2:
		 UCSRC=0b10101110;
		 break;

		 case UART_CONFIG_8O1:
		 UCSRC=0b10110110;
		 break;
		 
		 case UART_CONFIG_8O2:
		 UCSRC=0b10111110;
		 break;

		 default:
		 UCSRC=0b10000110;
		 break;
		 
	 }
	
	// Enable Transmit and Receive Operation
	UCSRB=(1<<RXEN) | (1<<TXEN);
	

	
	
}
 
#ifdef UART_USE_EXCH
ISR(USART_RXC_vect)
{
	uint8_t myChar;
	
	// Read Character to release UDR Buffer register for next data transfer
	myChar=UDR;	
	
	// Store character in Rx Buffer if there is space left 
	// If no Space, RX Stream ist stoppped, all incoming data discarded
	if(_loc_UartRxCnt<UART_RX_BUF_LEN)
	{
		// Filter out printable characters
		if(myChar>=32)
		{
			_loc_UartRxBuffer[_loc_UartRxCnt]=myChar;
			_loc_UartRxCnt++;
		}

		// if newline, end of record or end of string is received the RX Stream is stopped
		// and a NEWLINE event is issued
		if ((myChar==UART_CR)||(myChar==0)||(myChar==';'))
		{
			_loc_StopRxFlow();
			uart_UartFunction(UART_EVT_NEWLINE);
		}
		
	}
	else
	{
		// If Buffer is full, RX stream is stopped and
		// Overflow Event is issued
		_loc_StopRxFlow();
		uart_UartFunction(UART_EVT_BUF_OVL);
	}
}

ISR(USART_TXC_vect)
{
	_loc_UartTxCnt++;
	
	if (_loc_UartTxCnt<_loc_UartTxBufCnt)
	{
		_loc_SetTxData(_loc_UartTxBuffer[_loc_UartTxCnt]);
	}
	else
	{
		_loc_StopTxFlow();
		_loc_UartTxCnt=0;
		uart_UartFunction(UART_EVT_TX_DONE);
	}
}

void _loc_StartRxFlow(void)
{
	_loc_RxFlow=1;
	UCSRB|=(1<<RXCIE);
	sei();
}

uint8_t _loc_StopRxFlow(void)
{
	_loc_RxFlow=0;
	UCSRB&=~(1<<RXCIE);
	return _loc_UartRxCnt;
}

void _loc_StartTxFlow(void)
{
	_loc_TxFlow=1;
	UCSRB|=(1<<TXCIE);
	sei();
}

void _loc_StopTxFlow(void)
{
	_loc_TxFlow=0;
	UCSRB&=~(1<<TXCIE);
}
#endif


// Empfangsregister voll ?
uint8_t _loc_RxComplete(void)
{
	return ((UCSRA & (1<<RXC)) != 0);
}

// Senderegister übertragen ?
uint8_t _loc_TxComplete(void)
{
	uint8_t Result;
	
	Result=((UCSRA & (1<<TXC)) != 0);

	// clear the TXC Bit by writing 1 into it
	UCSRA|=(1<<TXC);
	return Result;
}

// Empfangsregister lesen und ausgeben
uint8_t _loc_GetRxData(void)
{
	return UDR;
}

// Datenbyte ausgeben
void _loc_SetTxData(uint8_t Data)
{
	UDR=Data;
}
#endif
/*********************************************************************/

#if ((defined(DEVICE_ATMEGA328)) && (!defined(SUBTYPE_PB)))
void _loc_Init(uint8_t UartBaudRate, uint8_t UartMode)
{
	
	unsigned long Baud[N_BAUDRATES] ={1200, 2400, 9600, 57600, 115200};
	unsigned long UbrrCalc;
	
	// Program the Pins
	// RX: PD0, TX: PD1
	DDRD&=0xfe;
	DDRD|=0x02;
	
	// Activate Pullup to keep RX in idle mode when nothing is connected
	PORTD&=0xfe;
	
	// UBRR = fosc / (16 BAUD) -1
	UbrrCalc=F_CPU/(16*Baud[UartBaudRate])-1;
	
	// Set the BAUD Rate 
	UBRR0L=(UbrrCalc & 0x000000ff);
	
	UBRR0H=(UbrrCalc >> 8);
	
	
	
	 //Set Com Mode for 328
	 //UCSRC: 0b00 P1 P0 T0 1 1 0
	 //T0: 0 1 Stopbit, 1 2 Stopbits
	 //P1 P0: 0 0 No Parity, 1 0 Even, 1 1 Odd
	 
	 switch(UartMode)
	 {
		 case UART_CONFIG_8N1:
		 UCSR0C=0b00000110;
		 break;
		 
		 case UART_CONFIG_8N2:
		 UCSR0C=0b00001110;
		 break;
		 
		 case UART_CONFIG_8E1:
		 UCSR0C=0b00100110;
		 break;
		 
		 case UART_CONFIG_8E2:
		 UCSR0C=0b00101110;
		 break;

		 case UART_CONFIG_8O1:
		 UCSR0C=0b00110110;
		 break;
		 
		 case UART_CONFIG_8O2:
		 UCSR0C=0b00111110;
		 break;
		 
		 default:
		 UCSR0C=0b00000110;
		 break;
		 
	 }
	
	// Enable Transmit and Receive Operation
	UCSR0B=(1<<RXEN0) | (1<<TXEN0);

}



/* Prüft auf empfangene Daten */
uint8_t _loc_RxComplete(void)
{
	
	return ((UCSR0A & (1<<RXC0)) != 0);
	
}

/* Prüft auf gesendete Daten */
uint8_t _loc_TxComplete(void)
{
	uint8_t Result;
	
	
	Result=((UCSR0A & (1<<TXC0)) != 0);
	// clear the TXC Bit by writing 1 into it
	UCSR0A|=(1<<TXC0);
	return Result;
	
}

/* Gibt empfangene Daten zurück */
uint8_t _loc_GetRxData(void)
{
	return UDR0;
}


/* Sendet ein Datenbyte und wartet bis die Daten vollständig gesendet sind */
void _loc_SetTxData(uint8_t Data)
{
	UDR0=Data;
}


#ifdef UART_USE_EXCH

void _loc_StartRxFlow(void)
{
	_loc_RxFlow=1;
	UCSR0B|=(1<<RXCIE0);
	sei();
}

uint8_t _loc_StopRxFlow(void)
{
	_loc_RxFlow=0;
	UCSR0B&=~(1<<RXCIE0);
	return _loc_UartRxCnt;
}

void _loc_StartTxFlow(void)
{
	_loc_TxFlow=1;
	UCSR0B|=(1<<TXCIE0);
	sei();
}

void _loc_StopTxFlow(void)
{
	_loc_TxFlow=0;
	UCSR0B&=~(1<<TXCIE0);
}



ISR(USART_RX_vect)
{
	uint8_t myChar;
	
	// Read Character to release UDR Buffer register for next data transfer
	myChar=UDR0;
	
	// Store character in Rx Buffer if there is space left
	// If no Space, RX Stream ist stoppped, all incoming data discarded and Error reported
	if(_loc_UartRxCnt<UART_RX_BUF_LEN)
	{
		// Filter out printable characters
		if((myChar>=32)&&(myChar<=127))
		{
			_loc_UartRxBuffer[_loc_UartRxCnt]=myChar;
			_loc_UartRxCnt++;
		}

		// if newline, end of record or end of string is received the RX Stream is stopped
		// and a NEWLINE event is issued
		if ((myChar==UART_CR)||(myChar==0)||(myChar==';')||(myChar==UART_LF))
		{
			_loc_StopRxFlow();
			uart_UartFunction(UART_EVT_NEWLINE);
		}
		
	}
	else
	{
		// If Buffer is full, RX stream is stopped and
		// Overflow Event is issued
		_loc_StopRxFlow();
		uart_UartFunction(UART_EVT_BUF_OVL);
	}
}

ISR(USART_TX_vect)
{
	_loc_UartTxCnt++;
	
	if (_loc_UartTxCnt<_loc_UartTxBufCnt)
	{
		_loc_SetTxData(_loc_UartTxBuffer[_loc_UartTxCnt]);
	}
	else
	{
		_loc_StopTxFlow();
		_loc_UartTxCnt=0;
		uart_UartFunction(UART_EVT_TX_DONE);
	}
}


#endif

#endif

#if ((defined(DEVICE_ATMEGA328)) && (defined(SUBTYPE_PB)))
void _loc_Init(uint8_t UartBaudRate, uint8_t UartMode)
{
	
	unsigned long Baud[N_BAUDRATES] ={1200, 2400, 9600, 57600, 115200};
	unsigned long UbrrCalc;
	
	// Program the Pins
	// RX: PD0, TX: PD1
	DDRD&=0xfe;
	DDRD|=0x02;
	
	// Activate Pullup to keep RX in idle mode when nothing is connected
	PORTD&=0xfe;
	
	// UBRR = fosc / (16 BAUD) -1
	UbrrCalc=F_CPU/(16*Baud[UartBaudRate])-1;
	
	// Set the BAUD Rate
	UBRR0L=(UbrrCalc & 0x000000ff);
	
	UBRR0H=(UbrrCalc >> 8);
	
	
	
	//Set Com Mode for 328
	//UCSRC: 0b00 P1 P0 T0 1 1 0
	//T0: 0 1 Stopbit, 1 2 Stopbits
	//P1 P0: 0 0 No Parity, 1 0 Even, 1 1 Odd
	
	switch(UartMode)
	{
		case UART_CONFIG_8N1:
		UCSR0C=0b00000110;
		break;
		
		case UART_CONFIG_8N2:
		UCSR0C=0b00001110;
		break;
		
		case UART_CONFIG_8E1:
		UCSR0C=0b00100110;
		break;
		
		case UART_CONFIG_8E2:
		UCSR0C=0b00101110;
		break;

		case UART_CONFIG_8O1:
		UCSR0C=0b00110110;
		break;
		
		case UART_CONFIG_8O2:
		UCSR0C=0b00111110;
		break;
		
		default:
		UCSR0C=0b00000110;
		break;
		
	}
	
	// Enable Transmit and Receive Operation
	UCSR0B=(1<<RXEN) | (1<<TXEN);

}

/* Prüft auf empfangene Daten */
uint8_t _loc_RxComplete(void)
{
	
	return ((UCSR0A & (1<<RXC)) != 0);
	
}

/* Prüft auf gesendete Daten */
uint8_t _loc_TxComplete(void)
{
	uint8_t Result;
	
	
	Result=((UCSR0A & (1<<TXC)) != 0);
	// clear the TXC Bit by writing 1 into it
	UCSR0A|=(1<<TXC);
	return Result;
	
}

/* Gibt empfangene Daten zurück */
uint8_t _loc_GetRxData(void)
{
	return UDR0;
}


/* Sendet ein Datenbyte und wartet bis die Daten vollständig gesendet sind */
void _loc_SetTxData(uint8_t Data)
{
	UDR0=Data;
}


// Für PB wurde die Interrupt-Unterstützung noch nicht implementiert

#endif

/*********************************************************************/
// Lokale HW Unabhängige FUnktionen
/*********************************************************************/
int	_uart_put(char c, FILE * f)
{
	uart_SendByte(c,UART_YES);
	return 0;
}

void _loc_ClearRxBuffer(void)
{
	
};

void _loc_ClearTxBuffer(void)
{

};

/*********************************************************************/


/*********************************************************************/
//HW Unabhängige Funktionsprototypen für die Uart-Steuerung
/*********************************************************************/

//Initialisiert die Schnittstelle für einen gegebenen Parameter-Satz
// Für die Parameter müssen die Defines aus diesem .h File verwendet werden
// UartBaudRate: Übergabe der Baudrate
// UartMode: definiert den Mode für die Uart Schnittstelle
void uart_Init(uint8_t UartBaudRate, uint8_t UartMode)
{
	_loc_Init(UartBaudRate, UartMode);
	// Send a Dummy Byte to Clear the Send Buffer 
	uart_SendByte(0,UART_YES);
	//uart_SendByte(UART_CR,UART_YES);
	//uart_SendByte(UART_LF,UART_YES);
	
	#ifdef STDOUT_UART
	uart_SendTextWait("Ready",5,UART_YES);
	stdout = &uart_str;
	#endif
	
	// Read Data to empty receive Buffer
	uart_GetData();
	uart_GetData();
	
	
}

// Prüft ob neue Daten empfangen wurden
// Liefert als Ergebnis TRUE wenn neue Daten vorhanden andernfalls FALSE
uint8_t uart_NewData(void)
{
	return _loc_RxComplete();
}

// Prüft ob die letzte Sendung komplett ausgeführt wurde
// TRUE: Sendebuffer ist leer
// FALSE: Sendung läuft noch
uint8_t uart_SendComplete(void)
{
	return _loc_TxComplete();
}

// Warten auf empfangene Daten mit Timeout
// TRUE: Daten wurden empfangen
// FALSE: Timeout
uint8_t uart_WaitForNewData(void)
{
	uint16_t TimeOutCnt=0;
	uint8_t RxOk=0;
	
	while ((TimeOutCnt<1000) && !RxOk)
	{
		RxOk=_loc_RxComplete();
		_delay_us(TIMEOUT_N_US);
		TimeOutCnt++;
	}
	
	if (RxOk) return UART_OK;
	else return UART_ERR;	
}

// Wartet bis die aktuelle Kommunikation abgeschlossen ist
// Timeout nach 100ms
void uart_WaitForSendComplete(void)
{
	uint16_t TimeOutCnt=0;
	uint8_t TxOk=0;

	while ((TimeOutCnt<1000) && !TxOk)
	{
		TxOk=_loc_TxComplete();
		_delay_us(TIMEOUT_N_US);
		TimeOutCnt++;
	}
}

// Gibt das älteste empfangene Datenbyte aus dem Empfangsbuffer zurück
// (Ohne Interrupt kann nur eine Byte empfangen werden)
// Wenn keine Daten vorliegen wird 0x00 zurückgegeben
uint8_t uart_GetData(void)
{
	return _loc_GetRxData();
}


// Kopiert den aktuellen Empfangsbuffer an eine Speicheradresse und löscht den Empfangsbuffer
// Rückgabewert: die Anzahl der gelesenen Zeichen
// Vorsicht: diese Fuktion wird normalerweise innerhalb der ISR aufgerufen

//uint8_t uart_ReadLn(uint8_t * Dest)
//{
	//uint8_t myCnt;
//
	//cli();
	//memcpy(Dest, _loc_UartRxBuffer,myCnt);
	//_loc_UartRxCnt=0;
	//sei();
	//return myCnt;
//}

// Sendet ein einzelnes Datenbyte und wartet (optional) bis die Daten vollständig gesendet sind
// Data: das zu sendende Datenbyte
// WaitYesNo: Flag das anzeigt ob gewartet werden soll bis die Übertragung komplett ist oder nicht
void uart_SendByte(uint8_t Data, uint8_t WaitYesNo)
{
	_loc_SetTxData(Data);
	if (WaitYesNo)
	{
		uart_WaitForSendComplete();
	}
}

// Nur für Interrupt-gesteuerte Systeme
// Addiert ein weiteres Datenbyte zum Ausgangspuffer
// Ist der Puffer voll, wartet die Funktion bis ein Zeichen hinzugefügt werden kann
// Rückgabewert: die Anzahld er noch freien Speicherplätze im Puffer
uint8_t uart_AddByte(uint8_t Data)
{
	
}

// Sendet einen Text über die Schnittstelle, optional mit nachfolgendem CRLF
void uart_SendTextWait(char  * TextOut,  uint8_t NChars, uint8_t SendCrLf)
{
	//uint8_t StrLen = strlen(TextOut);
	uint8_t Cnt;
	
	for(Cnt=0;Cnt<NChars;Cnt++)
	{
		uart_SendByte(TextOut[Cnt],UART_YES);
	}	
	
	// Wenn gewünscht, CRLF nachsenden
	if(SendCrLf)
	{
		uart_SendByte(UART_CR, UART_YES);
		uart_SendByte(UART_LF, UART_YES);
	}
}


// Ausgabe einer Integer-Variable als Zeichenfolge auf die uart
// x: Variable, N: Anzahl Stellen inklusive Vorzeichen
// es wird immer eine Vorzeichen ausgegeben + oder -.
// N muss >= 2 sein und <=12 sein, sonst passiert gar nichts
void uart_IntToUart(int32_t x, char N)
{
	char Text[13];
	if ((N>=2) && (N<=12))
	{
		if (x>=0)
		{
			_loc_uint2txt(x, Text, N-1);
			uart_SendByte('+',UART_YES);
			uart_SendTextWait(Text,N-1,UART_NO);
		}
		else
		{
			_loc_uint2txt(-x, Text, N-1);
			uart_SendByte('-',UART_YES);
			uart_SendTextWait(Text,N-1,UART_NO);
		}
	}
}

// AUsgabe einer Unsigned Integer-Variable als Zeichenfolge auf dem Display an der aktuellen Position
// x: Variable, N: Anzahl Stellen
// N muss >=1 und <=12 sein
void uart_UintToUart(uint32_t x, char N)
{
	char Text[12];
	if ((N>=2) && (N<=12))
	{
		_loc_uint2txt(x, Text, N);
		uart_SendTextWait(Text,N,UART_NO);
	}
}

// Ausgabe von CRLF auf die Uart
void uart_SendCrLf()
{
	uart_SendByte(UART_CR,UART_YES);
	uart_SendByte(UART_LF,UART_YES);
}


uint8_t uart_WaitCmd()
{
	uint8_t C1=0;
	uint8_t C2=0;
	uint8_t C3=0; 
	
	uart_SendByte(0x0d,UART_YES);
	uart_SendByte(0x0a,UART_YES);
	uart_SendByte('>',UART_YES);
	
	while(C1!=0x0a)
	{
		if(uart_WaitForNewData())
		{
			C3=C2;
			C2=C1;
			C1=uart_GetData();
			uart_SendByte(C1,UART_YES);
		}
	}
	return C3;
}


//-------------------------------------------------------------------------
// Hier folgen die Erweiterungsfunktionen mit Interrupt Steuerung
#ifdef UART_USE_EXCH



//------------------------------------------------------------------------
// Fügt ein Zeichen zum Emfangsbuffer hinzu
uint8_t uart_AddCharToBuffer(uint8_t InChar)
{
	_loc_UartRxBuffer[_loc_UartRxCnt]=InChar;
	_loc_UartRxCnt++;
	if (_loc_UartRxCnt>=UART_EXCH_BUF_SIZE)
	{
		_loc_UartRxCnt=UART_EXCH_BUF_SIZE-1;
		return 0;
	}
	else
	{
		return 1;
	}
}

// Setzt den Empfangsbuffer zurück ohne diesen zu löschen
void uart_ResetRxBuffer(void)
{
	_loc_UartRxCnt=0;
}

// EValuiert eine empfangene Zeichenfolge entsprechend dem Standard-Format
// *Zahl:Zahl:Zahl:...; CR/LF
// Es können bis zu maximal UART_EXCH_INT Zahlen in einem Telegram übertragen werden
// Der Rückgabewert ist die Anzahl korrekt empfangene ganze Zahlen
// Das Ergebnis wird im lokalen Array gespeichert
uint8_t uart_EvalMessage(void)
{
	uint8_t ResultCnt=0;
	uint8_t BufCnt=0;
	uint8_t Completed=0;
	uint8_t myChar;
	uint8_t isNegative=0;
	int16_t myInt=0;
	

	// Prüfen ob das erste Zeichen dem Kontrollzeichen entspricht
	BufCnt=0;
	if(_loc_UartRxBuffer[BufCnt]!='*')
	{
		return 0;
	}
	BufCnt++;

	// Hauptschleife über alle Zahlen
	// Completed enthält einen Code, der Auskunft gibt über den Abschlußzustand
	// 0: Prozess läuft noch
	// 1: Abschluß der inneren Schleife durch Trennzeichen, weiter zur nächsten Zahl
	// 2: regulärer Abschluß durch Kontrollzeichen oder CR
	// 3: Abschluß durch Erreichen des Buffer-Ende
	// 4: Abschluß durch maximale Anzahl Zahlen
	// 5: Abschluß durch invalide Daten
	//
	// In jedem Fall wird als Rückgabewert die Anzahl der erfolgreich eingelesenen Zahlen zurückgegeben
	Completed=0;
	ResultCnt=0;
	while(!Completed)
	{
		// Den nächsten Zahlenwert einlesen
		myInt=0;
		isNegative=0;
		
		do
		{
			// nächstes Zeichen
			myChar=_loc_UartRxBuffer[BufCnt];
			BufCnt++;
			
			

			// Prüfen des Zeichens
			switch(myChar)
			{
				case '-':
				isNegative=1;
				break;
				case '+':
				// ignorieren
				break;
				case '0':
				case '1':
				case '2':
				case '3':
				case '4':
				case '5':
				case '6':
				case '7':
				case '8':
				case '9':
				// einrechnen
				myInt=myInt*10+(myChar-'0');
				break;
				
				case ';':
				case UART_CR:
				case UART_LF:
				case 0:
				// regulärer Abschluß
				Completed=2;
				break;
				case ':':
				// Weiter zur nächsten Zahl
				Completed=1;
				break;
				default:
				// invalide Zeichen gefunden
				Completed=5;
				break;
			}
			
			// Prüfen auf Buffer Ende
			if (BufCnt>=UART_EXCH_BUF_SIZE) Completed=3;
			
		}
		while(!Completed);
		// Ende der Zahlenschleife
		
		// Bei regulärem Abschluß oder bei Zeilenende die Zahl verarbeiten
		if ((Completed==1)||(Completed==2))
		{
			if (isNegative) myInt=myInt*-1;
			_loc_UartResult[ResultCnt]=myInt;
			ResultCnt++;
		}
		
		// wenn die Anzahl zu lesende Zahlen erreicht wurde, beenden
		if(ResultCnt>=UART_EXCH_INT_SIZE) Completed=4;

		// Wenn Trennzeichen, weiter zur nächsten Zahl
		if (Completed==1) Completed=0;

	} // ende der Hauptschleife
	return ResultCnt;
}

// liefert eine Zahl aus dem Ganzzahlspeicher
// wenn der Index nicht im Bereich liegt, wird 0xffff zurückgegeben
uint32_t uart_GetInt(uint8_t IntIndex)
{
	if(IntIndex<=UART_EXCH_INT_SIZE)
	{
		return _loc_UartResult[IntIndex];
	}
	else
	{
		return 0xffff;
	}
}

// Liefert einen Pointer auf das Ganzzahlarray
int32_t *  uart_GetIntPointer(void)
{
	return _loc_UartResult;
}

// Steuerung der Streams--------------------------------------------

// Starten des Empfangsstremas
void uart_StartRxFlow(void)
{
	_loc_StartRxFlow();
}

// Stoppt den laufenden EMpfangsstream
// Rückgabewert: ANzahl der gültigen Zeichen im Empfangsbuffer
uint8_t uart_StopRxFlow(void)
{
	return _loc_StopRxFlow();
}


// Gibt den aktuellen Zählerstand des Buffer Counters zurück
uint8_t uart_GetRxBufCnt(void)
{
	return _loc_UartRxCnt;
}

// kopiert den aktuellen RX Buffer in einen lokalen Speicher
// Gibt die Anzahl kopierte Zeichen zurück
// stoppt den laufenden RX Stream
uint8_t uart_ReadRxBuffer(uint8_t * Dest)
{
	_loc_StopRxFlow();
	memcpy(Dest, _loc_UartRxBuffer,_loc_UartRxCnt);
	return _loc_UartRxCnt;
}

// Setzt den Sendebuffer zurück
void uart_ResetTxBuffer(void)
{
	_loc_UartTxCnt=0;
}

// Starten des Sendestreams
void uart_StartTxFlow(void)
{
	_loc_UartTxCnt=0;
	_loc_StartTxFlow();		
}

// Stoppt den laufenden Sendestream
void uart_StopTxFlow(void)
{
	_loc_StopTxFlow();
}

// kopiert den lokalen Speicher Src in den TX Buffer
// ein laufender Sendeprozess wird unterbrochen 
void uart_WriteTxBuffer(uint8_t * Src, uint8_t NBytes)
{
	_loc_StopTxFlow();
	if((NBytes>0)&&(NBytes<UART_EXCH_BUF_SIZE))
	{
		memcpy(_loc_UartTxBuffer, Src, NBytes);	
		_loc_UartTxBufCnt=NBytes;
		// printf("%u bytes copied\n",NBytes);
	}
	else
	{
		_loc_UartTxBufCnt=0;
	}
}

// Sendet den aktuellen Bufferinhalt auf die Schnittstelle
void uart_SendBuffer(void)
{	
	_loc_UartTxCnt=0;
	if(_loc_UartTxBufCnt>0)
	{
		_loc_StartTxFlow();	
		_loc_SetTxData(_loc_UartTxBuffer[0]);
	}
}

// Sendet den Inhalt in Src an die Uart unter verwendung eines Streams
// Sofern noch ein Stream am Laufen ist wartet die Funktion bis dieser Abgeschlossen wurde
// Src: Pointer auf die Datenquelle
// SrcLen: Anzahl der Bytes die übertragen werden
// CrLf: Zeigt an ob ein CR und LF zusätzlich übertragen werden sollen
// Return-Value: 0=Fehler, 1=OK
uint8_t uart_SendText(uint8_t * Src, uint8_t SrcLen,  uint8_t CrLf)
{
	uint8_t myTimeOutCnt = 0;
	
	// Abwarten bis der Sende Stream frei ist
	while((_loc_TxFlow)&&(myTimeOutCnt<UART_TIMEOUT_MSEC))
	{
		myTimeOutCnt++;
		_delay_ms(1);
	}
	
	// Bei Timeout Abbruch
	if(myTimeOutCnt>=UART_TIMEOUT_MSEC) return 0;
	
	// Buffer kopieren und absenden
	if(SrcLen<UART_EXCH_BUF_SIZE)
	{
		uart_WriteTxBuffer(Src,SrcLen);	
		if(CrLf)
		{
			_loc_UartTxBuffer[SrcLen]=UART_CR;
			_loc_UartTxBufCnt++;
			_loc_UartTxBuffer[SrcLen+1]=UART_LF;
			_loc_UartTxBufCnt++;
		}
		uart_SendBuffer();
	}
	
	return 1;
}


// sendet einen null-terminierten String
// Aufruf der SendText funktion mit berechneter Länge
uint8_t uart_SendStr(uint8_t * Str, uint8_t CrLf)
{
	return uart_SendText(Str, strlen(Str), CrLf);
}

//------------------------------------------------------------------------
#endif