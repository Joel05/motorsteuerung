/************************************************************/
/* Basisfunktionen für die Steuerung verschiedener Displays */
/*															*/
/* Autor: ZKS												*/
/*															*/
/*															*/
/* Versionsinfos:											*/
/* 26.8.2020, Initial release V2021.					    */
/* 4.9.2021, Update 2022									*/
/************************************************************/
#include <stdint.h>
#define ZKSLIBUART 202201

// Defines für die Definition der Baudrate
#define N_BAUDRATES 5
#define UART_BAUDRATE_1200 0
#define UART_BAUDRATE_2400 1
#define UART_BAUDRATE_9600 2
#define UART_BAUDRATE_57600 3
#define UART_BAUDRATE_115200 4

// Defines für die Konfiguration
#define UART_CONFIG_8N1 0
#define UART_CONFIG_8N2 1
#define UART_CONFIG_8E1 2
#define UART_CONFIG_8E2 3
#define UART_CONFIG_8O1 4
#define UART_CONFIG_8O2 5

// Rückmeldecodes
#define UART_ERR 0
#define UART_OK 1

// Einstellungen des Timeouts für das Warten auf ankommende Daten
// Das Delay wird 1000x abgewartet
#define TIMEOUT_N_US 100

// Defines für Labels und Sonderzeichen
#define UART_CR 13
#define UART_LF 10

#define UART_YES 1
#define UART_NO 0

// Defines für das Interrupt Management
// müssen 2er Potenzen sein
#define UART_RX_BUF_LEN 32
#define UART_TX_BUF_LEN 32
#define UART_BUF_MASK 0x1f

// Steuerungszeichen für die UART Ereignisse
#define UART_EVT_NEWLINE 0
#define UART_EVT_BUF_OVL 1
#define UART_EVT_TX_DONE 2


// Defines für das Standardisierte Austauschprotokoll
#ifdef UART_USE_EXCH
	#define UART_EXCH_BUF_SIZE 32
	#define UART_EXCH_INT_SIZE 8
	#define UART_TIMEOUT_MSEC 100  
#endif

//------------------------------------------------------------------------
//HW Unabhängige Funktionsprototypen für die Uart-Steuerung

//Initialisiert die Schnittstelle für einen gegebenen Parameter-Satz
// Für die Parameter müssen die Defines aus diesem .h File verwendet werden
// UartBaudRate: Übergabe der Baudrate
// UartMode: definiert den Mode für die Uart Schnittstelle
void uart_Init(uint8_t UartBaudRate, uint8_t UartMode);

// Prüft ob neue Daten empfangen wurden
// Liefert als Ergebnis TRUE wenn neue Daten vorhanden andernfalls FALSE 
uint8_t uart_NewData(void);

// Prüft ob die letzte Sendung komplett ausgeführt wurde ohne zu warten
// TRUE: Sendebuffer ist leer
// FALSE: Sendung läuft noch
uint8_t uart_SendComplete(void);

// Warten auf empfangene Daten mit Timeout
// TRUE: Daten wurden empfangen
// FALSE: Timeout
// Die Dauer des Timeouts wird als #define festgelegt
uint8_t uart_WaitForNewData(void);

// Wartet bis eine aktuell laufende Kommunikation abgeschlossen ist
void uart_WaitForSendComplete(void);

// Gibt das zuletzt empfangene Datenbyte aus dem Empfangsbuffer zurück
// (Ohne Interrupt kann nur eine Byte empfangen werden) 
// Wenn keine Daten empfangen wurden wird 0x00 zurückgegeben
// Vorsicht: die Daten sind nur gültig, wenn zuvor auf neue Daten geprüft wurde
uint8_t uart_GetData(void);


// Kopiert den aktuellen Empfangsbuffer an eine Speicheradresse und löscht den Empfangsbuffer 
// Rückgabewert: die Anzahl der gelesenen Zeichen
uint8_t uart_ReadLn(uint8_t * Dest);


// Sendet ein einzelnes Datenbyte und wartet (optional) bis die Daten vollständig gesendet sind 
// Data: das zu sendende Datenbyte
// WaitYesNo: Flag das anzeigt ob gewartet werden soll bis die Übertragung komplett ist oder nicht
void uart_SendByte(uint8_t Data, uint8_t WaitYesNo);


// Nur für Interrupt-gesteuerte Systeme
// Addiert ein weiteres Datenbyte zum Ausgangspuffer
// Ist der Puffer voll, wartet die Funktion bis ein Zeichen hinzugefügt werden kann
// Rückgabewert: die Anzahld er noch freien Speicherplätze im Puffer
uint8_t uart_AddByte(uint8_t Data);


// Sendet einen Text über die Schnittstelle, optional mit nachfolgendem CRLF
// IN NChars wird die Anzahl Zeichen angegeben, die gesendet werden soll
// NCHars muss kleiner oder gleich der Länge des Textes sein
// Das Flag SendCrLf gibt an, ob ein abschliessendes CR+LF mitgesendet wird
// Die FUnktione wartet, bis alle Zeichen übertragen sind.
void uart_SendTextWait(char  * TextOut,  uint8_t NChars, uint8_t SendCrLf);

// Wartet auf ein Kommando von der Uart Schnittstelle. 
// Das Kommando wird durch ein CR+LF übernommen
// Die Funktion gibt den letzten zuvor gespeicherten Wert als Info zurück
uint8_t uart_WaitCmd();


// AUsgabe einer Unsigned Integer-Variable als Zeichenfolge auf dem Display an der aktuellen Position
// x: Variable, N: Anzahl Stellen
// N muss >=1 und <=12 sein
void uart_UintToUart(uint32_t x, char N);

// Ausgabe einer Integer-Variable als Zeichenfolge auf die uart
// x: Variable, N: Anzahl Stellen inklusive Vorzeichen
// es wird immer eine Vorzeichen ausgegeben + oder -.
// N muss >= 2 sein und <=12 sein, sonst passiert gar nichts
void uart_IntToUart(int32_t x, char N);

// Ausgabe von CRLF auf die Uart
void uart_SendCrLf();





//------------------------------------------------------------------------
#ifdef UART_USE_EXCH 

// Deklaration der optionalen Callback-Funktionen
extern void uart_UartFunction(uint8_t EventId);

// Fügt ein Zeichen zum Emfangsbuffer hinzu 
uint8_t uart_AddCharToBuffer(uint8_t InChar);


/*****************************************************************************/
// Steuerung des Empfangsstreams

// Setzt den Empfangsbuffer zurück ohne diesen zu löschen
void uart_ResetRxBuffer(void);

// Gibt den aktuellen Zählerstand des Buffer Counters zurück
uint8_t uart_GetRxBufCnt(void);

// Starten des Empfangsstremas
void uart_StartRxFlow(void);

// Stoppt den laufenden EMpfangsstream
// Rückgabewert: ANzahl der gültigen Zeichen im Empfangsbuffer
uint8_t uart_StopRxFlow(void);

// kopiert den aktuellen RX Buffer Stand in einen lokalen Speicher
// Gibt die Anzahl kopierte Zeichen zurück
uint8_t uart_ReadRxBuffer(uint8_t * Dest);
/*****************************************************************************/

/*****************************************************************************/
// Steuerung des Sendestreams

// Setzt den Sendebuffer zurück 
void uart_ResetTxBuffer(void);

// Starten des Sendestreams
void uart_StartTxFlow(void);

// Stoppt den laufenden Sendestream
void uart_StopTxFlow(void);

// kopiert den lokalen Speicher in den TX Buffer
// Gibt die Anzahl kopierte Zeichen zurück
void uart_WriteTxBuffer(uint8_t * Src, uint8_t NBytes);

// Sendet den aktuellen Bufferinhalt auf die Schnittstelle
void uart_SendBuffer(void);

// Sendet den Inhalt in Src an die Uart unter verwendung eines Streams
// Sofern noch ein Stream am Laufen ist wartet die Funktion bis dieser Abgeschlossen wurde oder ein Timeout auftritt
// Src: Pointer auf die Datenquelle
// SrcLen: Anzahl der Bytes die übertragen werden
// CrLf: Zeigt an ob ein CR und LF zusätzlich übertragen werden sollen
// Return-Value: 0=Fehler, 1=OK
uint8_t uart_SendText(uint8_t * Src, uint8_t SrcLen,  uint8_t CrLf);
uint8_t uart_SendStr(uint8_t * Str, uint8_t CrLf);
/*****************************************************************************/



// Liefert einen Pointer auf das Ganzzahlarray
int32_t *  uart_GetIntPointer(void);

// EValuiert eine empfangene Zeichenfolge entsprechend dem Standard-Format
// *Zahl:Zahl:Zahl:...; CR/LF
// Es können bis zu maximal UART_EXCH_INT Zahlen in einem Telegram übertragen werden
// Am Ende der Fuktion wird der EMpfangsbuffer zurückgesetzt
// Der Rückgabewert ist die Anzahl korrekt empfangene ganze Zahlen
uint8_t uart_EvalMessage(void);

// liefert eine Zahl aus dem Ganzzahlspeicher
// wenn der Index nicht im Bereich liegt, wird 0xffffffff zurückgegeben
uint32_t uart_GetInt(uint8_t IntIndex);

// Hilfsfunktion zum Umwandeln einer Zahl in einen Text
// Keine Datenprüfung !!
uint8_t uart_Uint2Txt(uint32_t  BinData, char * TextBuffer, char NDigit);

//------------------------------------------------------------------------
#endif