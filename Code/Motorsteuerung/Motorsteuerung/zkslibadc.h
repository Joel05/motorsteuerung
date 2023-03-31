/****************************************************************/
/* Major Update der ADC Library 								*/
/*																*/
/* Autor: ZKS													*/
/*																*/
/* Datum: 4.9.2022												*/
/*                                 								*/
/* Die Library ist r�ckw�rtskompatibel wurde aber durch			*/
/* eine vollst�ndige Interrupt Steuerung erweiter.				*/
/* Alle neuen Function folgen der Convention					*/	
/* adc_yyyy_Int(....)											*/
/* Um die Erweiterung beim Kompilieren zu aktivieren, muss das	*/
/* symbol ADCFUNCTION definiert sein							*/	
//
// am 1.3.2023 Funktionen zur Nutzung des Komparators hinzugef�gt 
//
// Erg�nzungen beid er Definition der Sampling-Kan�le
//								  			
/****************************************************************/
#include <stdint.h>
#include "avr/interrupt.h"

// Der ADC wird auf eine kontinuierliche Wandlung mit ca. 4us Conversion Time konfiguriert (Tclk = clk/4)
// Die Referenz wird anhand der Initialisierung gesetzt. 
// F�r eine Rekonfiguration muss der ADC zuerst �ber adc_close geschlossen werden
// Die Kanalauswahl erfolgt beim Lesen des ADCs. Die entsprechenden Delays sind implementiert.



#ifdef DEVICE_ATMEGA16
// Vordefinierte Konstanten zur Kanalauswahl des ADCs
#define ADC_CH_0	0
#define ADC_CH_1	1
#define ADC_CH_2	2
#define ADC_CH_3	3
#define ADC_CH_4	4
#define ADC_CH_5	5
#define ADC_CH_6	6
#define ADC_CH_7	7
#define ADC_CH_VBG	30
#define ADC_CH_GND	31


// Vordefinierte Konstanten zur Referenzauswahl des ADCs
#define ADC_VREF_EXT	0
#define ADC_VREF_VCC	1
#define ADC_VREF_256	3

// Konstane f�r busy wait beim Lesen der Spannungen
#define ADC_NWAIT_US 150

// Konstanten f�r den Komparator
#define ADC_COMP_NINV_AIN0 0
#define ADC_COMP_NINV_VBG 1

#define ADC_COMP_INV_A0 0
#define ADC_COMP_INV_A1 1
#define ADC_COMP_INV_A2 2
#define ADC_COMP_INV_A3 3
#define ADC_COMP_INV_A4 4
#define ADC_COMP_INV_A5 5
#define ADC_COMP_INV_A6 6
#define ADC_COMP_INV_A7 7
#define ADC_COMP_INV_GND 31
#define ADC_COMP_INV_VBG 30
#define ADC_COMP_INV_AIN1 32

#define ADC_COMP_INT_NONE		0
#define ADC_COMP_INT_RISING	1
#define ADC_COMP_INT_FALLING	2
#define ADC_COMP_INT_TOGGLE	3




#endif

#ifdef DEVICE_ATMEGA328
// Vordefinierte Konstanten zur Kanalauswahl des ADCs
#define ADC_CH_0	0
#define ADC_CH_1	1
#define ADC_CH_2	2
#define ADC_CH_3	3
#define ADC_CH_4	4
#define ADC_CH_5	5
#define ADC_CH_6	6
#define ADC_CH_7	7
#define ADC_CH_TMP  8 
#define ADC_CH_VBG	14
#define ADC_CH_GND	15


// Vordefinierte Konstanten zur Referenzauswahl des ADCs
#define ADC_VREF_EXT	0
#define ADC_VREF_VCC	1
#define ADC_VREF_110	3

#define ADC_CLKDIV_2	1
#define ADC_CLKDIV_4	2
#define ADC_CLKDIV_8	3
#define ADC_CLKDIV_16	4
#define ADC_CLKDIV_32	5
#define ADC_CLKDIV_64	6
#define ADC_CLKDIV_128	7

// Konstane f�r busy wait beim Lesen der Spannungen
#define ADC_NWAIT_US 150

#endif

#ifdef DEVICE_ATTINY85
// Vordefinierte Konstanten zur Kanalauswahl des ADCs
#define ADC_CH_0	0
#define ADC_CH_1	1
#define ADC_CH_2	2
#define ADC_CH_3	3
#define ADC_CH_4	15
#define ADC_CH_VBG	12
#define ADC_CH_GND	13




// Vordefinierte Konstanten zur Referenzauswahl des ADCs
#define ADC_VREF_EXT	1 // PB0
#define ADC_VREF_VCC	0 // Vcc internal
#define ADC_VREF_11	    3 // internal 1.1V Reference
#define ADC_VREF_256    6 // internal 2.56V Reference, no bypass
#define ADC_VREF_256_CAP 7 // internal 2.56V Reference, bypass at PB0

// Konstane f�r busy wait beim Lesen der Spannungen
#define ADC_NWAIT_US 150
#endif


// Festelgung der nominalen VCC in mV
#define ADC_VCC_NOM_MV 3300

// Konstanten f�r die Verwaltung des Zustands
#define ADC_STAT_CLOSED 0
#define ADC_STAT_READY 1

// Konstanten f�r die Fehlermeldung
#define ADC_ERR_OK 0
#define ADC_ERR_STAT 1




// Pins m�ssen seitens der Anwendersoftware als Eingang konfiguriert werden
// Der Grund daf�r ist, dass der ADC unabh�ngig von der Pin Konfiguration arbeitet.
// D.h. es k�nnen auch die Spannungen gemessen werden, die am Pin als Ausgang anliegen


// HW-Unabh�ngige Funktionen zum Arbeiten mit dem ADC
// F�r die genaue Definitionen der verschiedenen Refrenzspannungen
// bitte das Datenblatt des jeweiligen Prozessors zu Rate ziehen


// konfigurieren des ADCs f�r eine Referenzspannung
uint8_t adc_Init(uint8_t AdcVref);

// Auslesen des ADC als 8-Bit Wert
// Die Funktion liest den zuletzt gewandelten Wert aus ohne auf die laufende Konvertierung zu achten
uint8_t adc_Read_8(uint8_t AdcChannel);

// Auslesen des ADC als 10-Bit Wert
// Die Funktion liest den zuletzt gewandelten Wert aus ohne auf die laufende Konvertierung zu achten
uint16_t adc_Read_10(uint8_t AdcChannel);

// Auslesen des ADC in mV
uint16_t adc_Read_mV(uint8_t AdcChannel);

// Auslesen des ADC in mV unter Ber�cksichtigung eines Spannungsteilers
// R1 und R2 werden als ganze Zahl von 0..100 �bergeben
// Die Absolutwerte werden also im Z�hler und Nenner des Spannungsteiler gleich skaliert
uint16_t adc_Read_mV_Divider(uint8_t AdcChannel, uint8_t R1, uint8_t R2);

// Auslesen des gerade aktuellen Kanals ohne Wartezeit. Es wird das zuletzt konvertierte Ergebnis ausgelesen
// Danach wird der Kanal umgeschaltet. DIe Annahme ist, dass vor dem n�chsten Aufruf mindestens 2 Konvertierungen komplettiert wurden
uint16_t adc_ReadImmediateAndChange_10(uint8_t NextChannel);

// Auslesen des gerade aktuellen Kanals ohne Wartezeit. Es wird das zuletzt konvertierte Ergebnis ausgelesen
// Danach wird der Kanal umgeschaltet. DIe Annahme ist, dass vor dem n�chsten Aufruf mindestens 2 Konvertierungen komplettiert wurden
uint16_t adc_ReadImmediateAndChange_mV_10_Divider(uint8_t NextChannel,uint8_t R1, uint8_t R2);


// Wechseln der Referenzspannung
// Der Muxer wird umegschaltet und 1us gewartet um die SIgnale zu stabilsieren
void adc_SetRef(uint8_t RefSelect);

// Schliessen des ADC f�r neue Konfiguration
void adc_Close();


// Konfigurieren des Komparators
void adc_Init_Comp(uint8_t NINV_Select, uint8_t INV_Select, uint8_t Interrupt_Select);

uint8_t adc_Get_Comp(void);


#ifdef ADCFUNCTION
/*******************************************************************/
// Erweiterte Funktionen _Int									   */	

// Gibt die Anzahl der zu messenden Kan�le im extended Mode an


// ADC Events f�r die Trigger Funktion
#define ADC_EVT_TRIG_POS 0
#define ADC_EVT_TRIG_NEG 1
#define ADC_EVT_TRIG_EXIT_POS 2
#define ADC_EVT_TRIG_EXIT_NEG 3
#define ADC_EVT_TRIG_WAIT 4


// Deklaration der optionalen Callback-Funktionen
extern void adc_AdcFunction(uint8_t ScanId, uint8_t EventId);

// Initialisierung f�r eine bestimmtes Timing-Setup
// Scan Pattern:
// Jedes Bit steht f�r einen zu messenden Kanal
uint8_t adc_Init_Int(uint8_t ClkDiv);

// Referenz, Triggerbedingung und Analogeingang f�r einen Kanal definieren
void adc_ConfigChannel_Int(uint8_t ScanId, uint8_t AdSel, uint8_t VrefSel, uint16_t TrigPos, uint16_t TrigNeg, uint8_t Hyst);

// Einen gewandelten Wert auslesen
uint16_t adc_Read_Value_Int(uint8_t AdSel);

// Einen gewandelten Wert in mV umrechnen
uint16_t adc_Convert_mV_Int(int32_t AdcValue, int32_t Vref, uint8_t R1, uint8_t R2);


#endif

#ifdef COMPFUNCTION
// Deklaration der optionalen Callback-Funktionen
extern void adc_CompFunction(void);
#endif