/************************************************************/
/* Implementierung zkslibadc.h								*/
/************************************************************/
#include "zkslibadc.h"
#include <AVR/io.h>
#include <util/delay.h>
#include <stdint.h>
#include "avr/interrupt.h"

// Globale Variablen
static uint8_t Adc_Status = ADC_STAT_CLOSED;
static uint16_t _Vref_mV = 0;
static uint16_t _Vcc_mV = ADC_VCC_NOM_MV;

#ifdef ADCFUNCTION

#define TRIG_STATUS_WAIT 0
#define TRIG_STATUS_POS 1
#define TRIG_STATUS_NEG 2
#define TRIG_STATUS_INIT 3

#define ADC_SCAN_CHANNELS 4


// Speicher für die Interruptbasierte Abtastung
static volatile uint16_t _loc_ScanData[ADC_SCAN_CHANNELS];
static volatile uint16_t _loc_MuxData[ADC_SCAN_CHANNELS]={0b01000000, 0b01000001, 0b01000010, 0b01000011};
static volatile uint16_t _loc_TrigData_Pos[ADC_SCAN_CHANNELS];
static volatile uint16_t _loc_TrigData_Neg[ADC_SCAN_CHANNELS];	
static volatile uint8_t _loc_HystData[ADC_SCAN_CHANNELS];	
static volatile uint8_t _loc_TrigStatus[ADC_SCAN_CHANNELS]={TRIG_STATUS_INIT,TRIG_STATUS_INIT,TRIG_STATUS_INIT,TRIG_STATUS_INIT};
static volatile uint8_t _loc_ScanCnt = 0;
static volatile uint8_t _loc_StepCnt = 0;
static volatile uint8_t _loc_SampleCnt = 0;
#endif




#ifdef DEVICE_ATMEGA16


// konfigurieren des ADCs 
uint8_t _loc_adc_init(uint8_t AdcVref)
{
	if (Adc_Status==ADC_STAT_CLOSED)
	{
		
		// set ADCSRA to: ADC enable, Start Conversion, Auto Trigger Enable, clk/64 as ADC Clock
		// at 12MHz this gives a Conversion time of 140us
		
		ADCSRA=0b11100110;
		
		// clear SFIO ADC Bits to enable free running mode
		SFIOR&=0b00011111;
		
		// set ADMUX to: Vcc as Reference, Left adjusted result for 8 Bit readout, Muxer according to Channel
		// Clear Bits
		ADMUX&=0b00111111;
		
		// Set Bits
		ADMUX|=(AdcVref & 0x03)<<6;
		
		// Process internal Data
		switch (AdcVref)
		{
			case ADC_VREF_256:
				_Vref_mV=2560;
				break;
			case ADC_VREF_VCC:
				_Vref_mV=_Vcc_mV;
				break;
			case ADC_VREF_EXT:
				_Vref_mV=_Vcc_mV;
				break;
			default:
				_Vref_mV=_Vcc_mV;
		}
		
		
		Adc_Status=ADC_STAT_READY;
		
		return ADC_ERR_OK;
				
	}
	else
	{
		return ADC_ERR_STAT;
	}
}

// Auslesen des ADC als 8-Bit Wert
uint8_t _loc_adc_read_8(uint8_t AdcChannel)
{
		uint8_t AdcValue=0x00;
			
		// Select the Channel for the Conversion
		ADMUX&=0b11000000;
		ADMUX|=0b00100000;
		ADMUX|=(AdcChannel&0x1f);		
			
		// Delay um die Signale zu stabilisieren und eine Wandlung abzuwarten
		_delay_us(ADC_NWAIT_US);				

		// Den ADC Wert auslesen
		AdcValue=ADCH;
	
		return AdcValue;
}

// Auslesen des ADC als 10-Bit Wert
uint16_t _loc_adc_read_10(uint8_t AdcChannel)
{
		uint16_t AdcValue=0x0000;
		
		// Select the Channel for the Conversion
		ADMUX&=0b11000000;
		ADMUX|=(AdcChannel&0x1f);
		
		// Wait for at least 2 completed conversions
		_delay_us(ADC_NWAIT_US);
		
		// Read Data
		AdcValue=ADC;
		
		return AdcValue;
}

// Auslesen des ADC in mV
uint16_t _loc_adc_read_mV(uint8_t AdcChannel)
{

	uint32_t _loc_Nadc=0;

	// Kanal auslesen	
	_loc_Nadc=_loc_adc_read_10(AdcChannel);
	
	// umrechnen in mV
	_loc_Nadc=_loc_Nadc*_Vref_mV;
	
	// Skalierung korrigieren um 1024 (2^10)
	_loc_Nadc=_loc_Nadc>>10;
		
	// Rückgabe als 16 Bit unsigned Integer	
	return((uint16_t)(_loc_Nadc));	
}

// Schliessen des ADC für neue Konfiguration
void _loc_adc_close()
{
	ADCSRA=0x00;
	Adc_Status=ADC_STAT_CLOSED;
	//return ADC_ERR_OK;	
}

void _loc_adc_setref(uint8_t AdcVref)
{
	
	// Process internal Data
	switch (AdcVref)
	{
		case ADC_VREF_256:
			_Vref_mV=2560;
			ADMUX&=0b00111111;
			ADMUX|=0b11000000;
			break;
		case ADC_VREF_VCC:
			_Vref_mV=_Vcc_mV;
			ADMUX&=0b00111111;
			ADMUX|=0b01000000;
			break;
		case ADC_VREF_EXT:
			ADMUX&=0b00111111;
			_Vref_mV=_Vcc_mV;
			break;
		default:
			break;
	}	
}

#ifdef ADCFUNCTION

#define ADC_STEP_READ 0
#define ADC_STEP_MUX 1
#define ADC_STEP_WAIT 2

static volatile uint16_t _loc_AdcValueNow;



uint8_t _loc_adc_Init_Int(uint8_t ClkDiv)
{
	
	// disable digital read on all analog pins
	//DIDR0&=0xff;
	
	// set ADMUX to: Vcc as Reference, Left adjusted result for 8 Bit readout, Muxer according to Channel
	// ADMUX wird auf das erste Scan-Pattern gesetzt
	ADMUX=0;
	ADMUX|=_loc_MuxData[0];
	
	// Set Free Running Mode
	// ADCSRB=0;

	// ADC Einschalten, erste Wandlung starten, Interrupt enable, Auto Trigger enable, Prescaler gemäss ClkDiv
	ADCSRA=0;
	ADCSRA|=0b10101000 | (ClkDiv);
	ADCSRA|=0b01000000;

}

void _loc_adc_ConfigChannel_Int(uint8_t ScanId, uint8_t AdSel, uint8_t VrefSel, uint16_t TrigPos, uint16_t TrigNeg, uint8_t Hyst)
{
	if(ScanId<ADC_SCAN_CHANNELS)
	{
		_loc_MuxData[ScanId]=(VrefSel<<6) | (AdSel&0x0f);
		_loc_TrigData_Pos[ScanId]=TrigPos;
		_loc_TrigData_Neg[ScanId]=TrigNeg;
		_loc_HystData[ScanId]=Hyst;
	}
}


// Hier folgt die Interrupt Routine
// Gemäss den konfigurierten Daten werden nacheinander die Kanäle abgefragt
// wobei jeweils die erste Wandlung verworfen wird, da die Signale noch nicht
// Stabil sind.
ISR(ADC_vect)
{

	uint16_t myOldValue;
	uint16_t myThreshold;
	
	switch(_loc_StepCnt)
	{
		
		case ADC_STEP_READ:
		// Stabile Daten können gelesen werden
		
		// Gewandelter Wert lesen und ablegen
		_loc_AdcValueNow=ADC;
		
		// -------------------------------------------------------------------------------------------------
		// -------------------------------------------------------------------------------------------------
		// Trigger State Machine ausführen und die Callback der App aufrufen
		myOldValue=_loc_ScanData[_loc_ScanCnt];
		
		switch(_loc_TrigStatus[_loc_ScanCnt])
		{
			case TRIG_STATUS_WAIT:
			
			// Check for positive Trigger
			if((myOldValue<_loc_TrigData_Pos[_loc_ScanCnt])&&(_loc_AdcValueNow>=_loc_TrigData_Pos[_loc_ScanCnt]))
			{
				_loc_TrigStatus[_loc_ScanCnt]=TRIG_STATUS_POS;
				// Execute Positive Trigger Event
				adc_AdcFunction(_loc_ScanCnt, ADC_EVT_TRIG_POS);
			}
			
			// Check for negative Trigger
			if((myOldValue>_loc_TrigData_Neg[_loc_ScanCnt])&&(_loc_AdcValueNow<=_loc_TrigData_Neg[_loc_ScanCnt]))
			{
				_loc_TrigStatus[_loc_ScanCnt]=TRIG_STATUS_NEG;
				// Execute Positive Trigger Event
				adc_AdcFunction(_loc_ScanCnt, ADC_EVT_TRIG_NEG);
			}
			break;
			
			case TRIG_STATUS_NEG:
			// Check for positive going Hysteresis Transition vom Neg Trigger
			myThreshold=_loc_TrigData_Neg[_loc_ScanCnt]+_loc_HystData[_loc_ScanCnt];
			if((myOldValue<myThreshold)&&(_loc_AdcValueNow>=myThreshold))
			{
				_loc_TrigStatus[_loc_ScanCnt]=TRIG_STATUS_WAIT;
				// Execute Positive Trigger Event
				adc_AdcFunction(_loc_ScanCnt, ADC_EVT_TRIG_EXIT_NEG);
			}
			break;
			
			case TRIG_STATUS_POS:
			// Check for negative going Hysteresis Transition
			myThreshold=_loc_TrigData_Pos[_loc_ScanCnt]-_loc_HystData[_loc_ScanCnt];
			if((myOldValue>myThreshold)&&(_loc_AdcValueNow<=myThreshold))
			{
				_loc_TrigStatus[_loc_ScanCnt]=TRIG_STATUS_WAIT;
				// Execute Positive Trigger Event
				adc_AdcFunction(_loc_ScanCnt, ADC_EVT_TRIG_EXIT_POS);
			}
			break;
			
			case TRIG_STATUS_INIT:
			if (_loc_SampleCnt>=20)
			{
				_loc_TrigStatus[_loc_ScanCnt]=TRIG_STATUS_WAIT;
				// Execute Positive Trigger Event
				adc_AdcFunction(_loc_ScanCnt, ADC_EVT_TRIG_WAIT);
			}
			break;
			
			default:
			_loc_TrigStatus[_loc_ScanCnt]=TRIG_STATUS_WAIT;
			break;
		}
		
		// ALte Version:
		//if((myOldValue<_loc_TrigData_Pos[_loc_ScanCnt])&&(_loc_AdcValueNow>=_loc_TrigData_Pos[_loc_ScanCnt]))
		//{
		//// Execute Positive Trigger Event
		//adc_AdcFunction(_loc_ScanCnt, ADC_EVT_TRIG_POS);
		//}
		//
		//if((myOldValue>_loc_TrigData_Neg[_loc_ScanCnt])&&(_loc_AdcValueNow<=_loc_TrigData_Neg[_loc_ScanCnt]))
		//{
		//// Execute Positive Trigger Event
		//adc_AdcFunction(_loc_ScanCnt, ADC_EVT_TRIG_NEG);
		//}
		// -------------------------------------------------------------------------------------------------
		// -------------------------------------------------------------------------------------------------
		// Ende der Triggerung
		// -------------------------------------------------------------------------------------------------
		
		// Neuen Wert Speichern
		_loc_ScanData[_loc_ScanCnt]=_loc_AdcValueNow;
		
		// Scan Counter erhöhen und begrenzen. Sample Cnt erhöhen
		_loc_ScanCnt++;
		if(_loc_ScanCnt>=ADC_SCAN_CHANNELS)
		{
			_loc_ScanCnt=0;
			_loc_SampleCnt++;
		}
		
		// Multiplexer auf die nächsten Settings umschalten
		ADMUX=_loc_MuxData[_loc_ScanCnt];
		
		// nächster Step ist warten
		_loc_StepCnt=ADC_STEP_MUX;
		break;
		
		case ADC_STEP_MUX:
		// diese Wandlung wird einfach verworfen
		_loc_StepCnt=ADC_STEP_WAIT;
		break;
		
		case ADC_STEP_WAIT:
		// diese Wandlung wird verworfen
		_loc_StepCnt=ADC_STEP_READ;
		break;
		
		default:
		_loc_StepCnt=ADC_STEP_READ;
		break;
	}

}
#endif



#ifdef COMPFUNCTION
ISR(ANA_COMP_vect)
{
	adc_CompFunction();
}
#endif

// Komparator konfigurieren
void _loc_comp_init(uint8_t NinvSelect, uint8_t InvSelect, uint8_t IntSelect)
{
	
	// Komparator aktivieren
	ACSR&=~(1<<ACD);
	
	// NINV Eingang konfigurieren
	switch(NinvSelect)
	{
		case ADC_COMP_NINV_VBG:
			ACSR|=(1<<ACBG);
			break;
		case ADC_COMP_NINV_AIN0:
			ACSR&=~(1<<ACBG);
		default:
			break;					
	}

	// INV Eingang Konfigurieren
	switch(InvSelect)
	{
		case ADC_COMP_INV_AIN1:
			SFIOR&=~(1<<ACME);
			break;	
		case ADC_COMP_INV_A0:
		case ADC_COMP_INV_A1:
		case ADC_COMP_INV_A2:
		case ADC_COMP_INV_A3:
		case ADC_COMP_INV_A4:
		case ADC_COMP_INV_A5:
		case ADC_COMP_INV_A6:
		case ADC_COMP_INV_A7:
		case ADC_COMP_INV_GND:
		case ADC_COMP_INV_VBG:
			SFIOR|=(1<<ACME);
			ADCSRA&=~(1<<ADEN);			
			ADMUX&=0b1110000;
			ADMUX|=InvSelect;
			break;
		default:
		break;
	}
	
	// Interrupt Einstellungen 
	switch(IntSelect)
	{
		case ADC_COMP_INT_NONE:
			ACSR&=~(1<<ACIE);
			break;
		
		case ADC_COMP_INT_RISING:
			ACSR|=(1<<ACIE);
			ACSR&=0b11111100;
			ACSR|=0x03;
			break;

		case ADC_COMP_INT_FALLING:
			ACSR|=(1<<ACIE);
			ACSR&=0b11111100;
			ACSR|=0x02;
			break;		
	
		case ADC_COMP_INT_TOGGLE:
			ACSR|=(1<<ACIE);
			ACSR&=0b11111100;
			break;	
	
		default:
		break;
	}		
}

uint8_t _loc_adc_get_comp(void)
{
	if(ACSR&(1<<ACO)) return 1;
	else return 0; 
}

#endif


#ifdef DEVICE_ATMEGA328
// konfigurieren des ADCs 
uint8_t _loc_adc_init(uint8_t AdcVref)
{
	if (Adc_Status==ADC_STAT_CLOSED)
	{
		
		// set ADCSRA to: ADC enable, Start Conversion, Auto Trigger Enable, clk/4 as ADC Clock
		// at 16MHz this gives a Conversion time of ~4us
		ADCSRA=0b11100010;
		
		// keep digital inputs readable
		DIDR0&=0b11000000;
		
		
		// set ADMUX to: Vcc as Reference, Left adjusted result for 8 Bit readout, Muxer according to Channel
		// Clear Bits
		ADMUX&=0b00111111;
		
		// Set ADMUX Bits
		ADMUX|=(AdcVref & 0x03)<<6;
		
		// Process internal Data
		switch (AdcVref)
		{
			case ADC_VREF_110:
				_Vref_mV=1100;
				break;
			case ADC_VREF_VCC:
				_Vref_mV=_Vcc_mV;
				break;
			case ADC_VREF_EXT:
				_Vref_mV=_Vcc_mV;
				break;
			default:
				_Vref_mV=_Vcc_mV;
				break;		
		}
		
		Adc_Status=ADC_STAT_READY;
		return ADC_ERR_OK;
				
	}
	else
	{
		return ADC_ERR_STAT;
	}
}

// Auslesen des ADC als 8-Bit Wert
uint8_t _loc_adc_read_8(uint8_t AdcChannel)
{
		uint8_t AdcValue=0x00;
	
		// Select the Channel for the Conversion
		ADMUX&=0b11000000;
		
		//set to left adjusted
		ADMUX|=0b00100000;
		
		// and select the channel
		ADMUX|=(AdcChannel&0x0f);
		
		// Wait for at least 2 completed conversions
		_delay_us(ADC_NWAIT_US);
		
		// Read Data
		AdcValue=ADCH;
		
		return AdcValue;
}

// Auslesen des ADC als 10-Bit Wert
uint16_t _loc_adc_read_10(uint8_t AdcChannel)
{
		uint16_t AdcValue=0x0000;
		
		// Select the Channel for the Conversion as 16Bit
		ADMUX&=0b11000000;
		ADMUX|=(AdcChannel&0x0f);
		
		// Wait for at least 2 completed conversions
		_delay_us(ADC_NWAIT_US);
		
		// Read Data
		AdcValue=ADC;
		
		return AdcValue;
}

uint16_t _loc_adc_read_andchange_10(uint8_t NextChannel)
{
		uint16_t AdcValue=0x0000;
		
		// laufende Konvertierung lesen
		AdcValue=ADC;
		
		// Select the Channel for the Conversion as 16Bit
		ADMUX&=0b11000000;
		ADMUX|=(NextChannel&0x0f);
		
		return AdcValue;
}

// Auslesen des ADC in mV
uint16_t _loc_adc_read_mV(uint8_t AdcChannel)
{

	uint32_t _loc_Nadc=0;

	// Kanal auslesen	
	_loc_Nadc=_loc_adc_read_10(AdcChannel);
	
	// umrechnen in mV
	_loc_Nadc=_loc_Nadc*_Vref_mV;
	_loc_Nadc=_loc_Nadc>>10;
		
	// Rückgabe als 16 Bit Integer	
	return((uint16_t)(_loc_Nadc));	
}

// Auslesen des ADC in mV
uint16_t _loc_adc_read_andchange_mV(uint8_t NextChannel)
{

	uint32_t _loc_Nadc=0;

	// Kanal auslesen
	_loc_Nadc=_loc_adc_read_andchange_10(NextChannel);
	
	// umrechnen in mV
	_loc_Nadc=_loc_Nadc*_Vref_mV;
	_loc_Nadc=_loc_Nadc>>10;
	
	// Rückgabe als 16 Bit Integer
	return((uint16_t)(_loc_Nadc));
}

// Schliessen des ADC für neue Konfiguration
void _loc_adc_close()
{
	ADCSRA=0x00;
	Adc_Status=ADC_STAT_CLOSED;
	return ADC_ERR_OK;	
}

#ifdef ADCFUNCTION

#define ADC_STEP_READ 0
#define ADC_STEP_MUX 1
#define ADC_STEP_WAIT 2

static volatile uint16_t _loc_AdcValueNow;



uint8_t _loc_adc_Init_Int(uint8_t ClkDiv)
{
	
		// disable digital read on all analog pins
		DIDR0&=0xff;
		
		// set ADMUX to: Vcc as Reference, Left adjusted result for 8 Bit readout, Muxer according to Channel
		// ADMUX wird auf das erste Scan-Pattern gesetzt
		ADMUX=0;
		ADMUX|=_loc_MuxData[0];
		
		// Set Free Running Mode
		ADCSRB=0;

		// ADC Einschalten, erste Wandlung starten, Interrupt enable, Auto Trigger enable, Prescaler gemäss ClkDiv
		ADCSRA=0;
		ADCSRA|=0b10101000 | (ClkDiv);
		ADCSRA|=0b01000000;			

}

void _loc_adc_ConfigChannel_Int(uint8_t ScanId, uint8_t AdSel, uint8_t VrefSel, uint16_t TrigPos, uint16_t TrigNeg, uint8_t Hyst)
{
	
	
	if(ScanId<ADC_SCAN_CHANNELS)
	{
		_loc_MuxData[ScanId]=(VrefSel<<6) | (AdSel&0x0f);
		_loc_TrigData_Pos[ScanId]=TrigPos;
		_loc_TrigData_Neg[ScanId]=TrigNeg;
		_loc_HystData[ScanId]=Hyst;
	}
}


// Hier folgt die Interrupt Routine
// Gemäss den konfigurierten Daten werden nacheinander die Kanäle abgefragt
// wobei jeweils die erste Wandlung verworfen wird, da die Signale noch nicht
// Stabil sind.
ISR(ADC_vect)
{

	uint16_t myOldValue;
	uint16_t myThreshold;
	
	switch(_loc_StepCnt)
	{
		
		case ADC_STEP_READ:
			// Stabile Daten können gelesen werden
		
			// Gewandelter Wert lesen und ablegen
			_loc_AdcValueNow=ADC;
				
			// -------------------------------------------------------------------------------------------------	
			// -------------------------------------------------------------------------------------------------	
			// Trigger State Machine ausführen und die Callback der App aufrufen			
			myOldValue=_loc_ScanData[_loc_ScanCnt];
			
			switch(_loc_TrigStatus[_loc_ScanCnt])
			{
				case TRIG_STATUS_WAIT:
				
					// Check for positive Trigger
					if((myOldValue<_loc_TrigData_Pos[_loc_ScanCnt])&&(_loc_AdcValueNow>=_loc_TrigData_Pos[_loc_ScanCnt]))
					{
						_loc_TrigStatus[_loc_ScanCnt]=TRIG_STATUS_POS;
						// Execute Positive Trigger Event
						adc_AdcFunction(_loc_ScanCnt, ADC_EVT_TRIG_POS);
					}
					
					// Check for negative Trigger
					if((myOldValue>_loc_TrigData_Neg[_loc_ScanCnt])&&(_loc_AdcValueNow<=_loc_TrigData_Neg[_loc_ScanCnt]))
					{
						_loc_TrigStatus[_loc_ScanCnt]=TRIG_STATUS_NEG;
						// Execute Positive Trigger Event
						adc_AdcFunction(_loc_ScanCnt, ADC_EVT_TRIG_NEG);
					}
					break;
					
				case TRIG_STATUS_NEG:
					// Check for positive going Hysteresis Transition vom Neg Trigger
					myThreshold=_loc_TrigData_Neg[_loc_ScanCnt]+_loc_HystData[_loc_ScanCnt];
					if((myOldValue<myThreshold)&&(_loc_AdcValueNow>=myThreshold))
					{
						_loc_TrigStatus[_loc_ScanCnt]=TRIG_STATUS_WAIT;
						// Execute Positive Trigger Event
						adc_AdcFunction(_loc_ScanCnt, ADC_EVT_TRIG_EXIT_NEG);
					}				
					break;
					
				case TRIG_STATUS_POS:
					// Check for negative going Hysteresis Transition
					myThreshold=_loc_TrigData_Pos[_loc_ScanCnt]-_loc_HystData[_loc_ScanCnt];
					if((myOldValue>myThreshold)&&(_loc_AdcValueNow<=myThreshold))
					{
						_loc_TrigStatus[_loc_ScanCnt]=TRIG_STATUS_WAIT;
						// Execute Positive Trigger Event
						adc_AdcFunction(_loc_ScanCnt, ADC_EVT_TRIG_EXIT_POS);
					}
					break;					
					
				case TRIG_STATUS_INIT:
					if (_loc_SampleCnt>=20)
					{
						_loc_TrigStatus[_loc_ScanCnt]=TRIG_STATUS_WAIT;
						// Execute Positive Trigger Event
						adc_AdcFunction(_loc_ScanCnt, ADC_EVT_TRIG_WAIT);						
					}	
					break;
						
				default:
					_loc_TrigStatus[_loc_ScanCnt]=TRIG_STATUS_WAIT;
					break;
			}
			
			// ALte Version:
			//if((myOldValue<_loc_TrigData_Pos[_loc_ScanCnt])&&(_loc_AdcValueNow>=_loc_TrigData_Pos[_loc_ScanCnt]))
			//{
				//// Execute Positive Trigger Event
				//adc_AdcFunction(_loc_ScanCnt, ADC_EVT_TRIG_POS);				
			//}
		//
			//if((myOldValue>_loc_TrigData_Neg[_loc_ScanCnt])&&(_loc_AdcValueNow<=_loc_TrigData_Neg[_loc_ScanCnt]))
			//{
				//// Execute Positive Trigger Event
				//adc_AdcFunction(_loc_ScanCnt, ADC_EVT_TRIG_NEG);			
			//}		
			// -------------------------------------------------------------------------------------------------
			// -------------------------------------------------------------------------------------------------
			// Ende der Triggerung
			// -------------------------------------------------------------------------------------------------			
			
			// Neuen Wert Speichern
			_loc_ScanData[_loc_ScanCnt]=_loc_AdcValueNow;		
		
			// Scan Counter erhöhen und begrenzen. Sample Cnt erhöhen
			_loc_ScanCnt++;
			if(_loc_ScanCnt>=ADC_SCAN_CHANNELS)
			{
				 _loc_ScanCnt=0;
				 _loc_SampleCnt++;
			}
				
			// Multiplexer auf die nächsten Settings umschalten
			ADMUX=_loc_MuxData[_loc_ScanCnt];		
		 
			// nächster Step ist warten 
			_loc_StepCnt=ADC_STEP_MUX;
			break;
	
		case ADC_STEP_MUX:	
			// diese Wandlung wird einfach verworfen	
			_loc_StepCnt=ADC_STEP_WAIT;
			break;
		
		case ADC_STEP_WAIT:	
			// diese Wandlung wird verworfen
			_loc_StepCnt=ADC_STEP_READ;
			break;
		
		default:
			_loc_StepCnt=ADC_STEP_READ;
			break;
		}

}
#endif


#endif





#ifdef DEVICE_ATTINY85


// konfigurieren des ADCs
uint8_t _loc_adc_init(uint8_t AdcVref)
{
	
	// Change the Vref data format
	// AdcVref[2]..AdcVref[0] wird zu AdcVref[1] AdcVrref[0] x AdcVref[2] x x x x x
	AdcVref=AdcVref&0x07;
	AdcVref=(AdcVref<<6)|((AdcVref&0x04)<<2);	
	
	
	if (Adc_Status==ADC_STAT_CLOSED)
	{
		
		// set ADCSRA to: ADC enable, Start Conversion, Auto Trigger Enable, clk/4 as ADC Clock
		// at 12MHz this gives a Conversion time of ~4us
		ADCSRA=(1<<ADEN)|(1<<ADSC)|(1<<ADATE)|(0x02);
		
		// Configure ADCSRB for free runing mode, unipolar and no plarity change
		//ADCSRB=0x00;
		
		
		// set ADMUX to: Vcc as Reference, Left adjusted result for 8 Bit readout, Muxer on Channel 0 initially
		// Clear Bits
		ADMUX&=0x00;
		
		// Set Bits
		ADMUX|=AdcVref | (1<<ADLAR);
		
		Adc_Status=ADC_STAT_READY;
		return ADC_ERR_OK;
		
	}
	else
	{
		return ADC_ERR_STAT;
	}
}


// Auslesen des ADC als 8-Bit Wert
uint8_t _loc_adc_read_8(uint8_t AdcChannel)
{
	uint8_t AdcValue=0x00;
	
	// Select the Channel for the Conversion
	ADMUX&=0b11010000;
	ADMUX|=0b00100000;
	ADMUX|=(AdcChannel&0x0f);
	
	// Wait for at least 2 completed conversions
	_delay_us(ADC_NWAIT_US);
	
	// Read Data
	AdcValue=ADCH;
	
	return AdcValue;
}

// Auslesen des ADC als 10-Bit Wert
uint16_t _loc_adc_read_10(uint8_t AdcChannel)
{
	uint16_t AdcValue=0x0000;
	
	// Select the Channel for the Conversion
	ADMUX&=0b11010000;
	ADMUX|=(AdcChannel&0x0f);
	
	// Wait for at least 2 completed conversions
	_delay_us(ADC_NWAIT_US);
	
	// Read Data
	AdcValue=ADC;
	
	return AdcValue;
}


// Schliessen des ADC für neue Konfiguration
void _loc_adc_close()
{
	ADCSRA=0x00;
	Adc_Status=ADC_STAT_CLOSED;
	return ADC_ERR_OK;
}

#endif

//-------------------------------------------------------------------
// Implementierung der HW-Unabhängigen Funktionen

// konfigurieren des ADCs für eine Referenzspannung
uint8_t adc_Init(uint8_t AdcVref)
{
	return _loc_adc_init(AdcVref);
}

// Auslesen des ADC als 8-Bit Wert
uint8_t adc_Read_8(uint8_t AdcChannel)
{
	return _loc_adc_read_8(AdcChannel);
}

// Auslesen des ADC als 10-Bit Wert
uint16_t adc_Read_10(uint8_t AdcChannel)
{
	return _loc_adc_read_10(AdcChannel);
}

// Auslesen des gerade aktuellen Kanals ohne Wartezeit. Es wird das zuletzt konvertierte Ergebnis ausgelesen
// Danach wird der Kanal umgeschaltet. DIe Annahme ist, dass vor dem nächsten Aufruf mindestens 2 Konvertierungen komplettiert wurden
uint16_t adc_ReadImmediateAndChange_10(uint8_t NextChannel)
{
	return _loc_adc_read_andchange_10(NextChannel);
}

// Auslesen des ADC in mV
uint16_t adc_Read_mV(uint8_t AdcChannel)
{
	return _loc_adc_read_mV(AdcChannel);
}

// Auslesen des ADC in mV unter Berücksichtigung eines Spannungsteilers
// R1 und R2 werden als ganze Zahl von 0..100 übergeben
// Die Absolutwerte werden also im Zähler und Nenner des Spannungsteiler gleich skaliert
uint16_t adc_Read_mV_Divider(uint8_t AdcChannel, uint8_t R1, uint8_t R2)
{
	int32_t AdcValue;
	int32_t DivValue;
	
	AdcValue=adc_Read_mV(AdcChannel);
	AdcValue=AdcValue*(R1+R2);
	DivValue=R2;
	AdcValue=AdcValue/DivValue;
	return AdcValue&0xffff;
}

// Auslesen des gerade aktuellen Kanals ohne Wartezeit. Es wird das zuletzt konvertierte Ergebnis ausgelesen
// Danach wird der Kanal umgeschaltet. DIe Annahme ist, dass vor dem nächsten Aufruf mindestens 2 Konvertierungen komplettiert wurden
uint16_t adc_ReadImmediateAndChange_mV_10_Divider(uint8_t NextChannel,uint8_t R1, uint8_t R2)
{
	int32_t AdcValue;
	int32_t DivValue;
	
	AdcValue=_loc_adc_read_andchange_mV(NextChannel);
	AdcValue=AdcValue*(R1+R2);
	DivValue=R2;
	AdcValue=AdcValue/DivValue;
	return AdcValue&0xffff;
}


// Wechseln der Referenzspannung
// Der Muxer wird umegschaltet und 1us gewartet um die SIgnale zu stabilsieren
void adc_SetRef(uint8_t RefSelect)
{
	_loc_adc_setref(RefSelect);
}

// Schliessen des ADC für neue Konfiguration
void adc_Close()
{
	return _loc_adc_close();
}

// Konfigurieren des Komparators
void adc_Init_Comp(uint8_t NINV_Select, uint8_t INV_Select, uint8_t Interrupt_Select)
{
	_loc_comp_init(NINV_Select, INV_Select, Interrupt_Select);
}

// Komparator auslesen
uint8_t adc_Get_Comp(void)
{
	return _loc_adc_get_comp();
}

//--------------------------------------------------------------------------------------
// Öffentliche Funktionen der INterrupt-gesteuerten Funktionen


#ifdef ADCFUNCTION
uint8_t adc_Init_Int(uint8_t ClkDiv)
{
	return _loc_adc_Init_Int(ClkDiv);
}

// Referenz und Analogeingang für einen Kanal definieren
void adc_ConfigChannel_Int(uint8_t ScanId, uint8_t AdSel, uint8_t VrefSel, uint16_t TrigPos, uint16_t TrigNeg, uint8_t Hyst)
{
	_loc_adc_ConfigChannel_Int(ScanId, AdSel,VrefSel,TrigPos,TrigNeg, Hyst);
}

// Einen gewandelten Wert auslesen
uint16_t adc_Read_Value_Int(uint8_t AdSel)
{
	if(AdSel<ADC_SCAN_CHANNELS) return _loc_ScanData[AdSel];
	else return 0xffff;
}

// Einen gewandelten Wert in mV umrechnen
uint16_t adc_Convert_mV_Int(int32_t AdcValue, int32_t Vref, uint8_t R1, uint8_t R2)
{
	int32_t DivValue;
	
	AdcValue=(Vref*AdcValue)>>10;
	AdcValue=AdcValue*(R1+R2);
	DivValue=R2;
	AdcValue=AdcValue/DivValue;
	return AdcValue&0xffff;	
}

//-------------------------------------------------------------------
#endif