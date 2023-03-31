/*
* Motorsteuerung.c
*
* Created: 06/02/2023 20:21:27
* Author : joelr
*/

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdlib.h>
#include "zkslibadc.h"
#include "defines.h"		//Eigene Headerdatei einbinden
#include "zkslibuart.h"

// Compare match ISR
ISR(TIMER0_COMPA_vect) {
	switch (direction) {		//Motorpin nach vorgegebener Richtung togglen
		case DirForward:
			Forward;
			break;
		case DirReverse:
			Reverse;
			break;
		case DirBrake:
			Brake;
			break;
		default:
			Brake;
			break;
	}
}

// Timer overflow ISR
ISR(TIMER0_OVF_vect) {
	Brake;
}


int main(void)
{
	DDRB = MOTOR_Enable | MOTOR_Forward | MOTOR_Reverse;	//Datenrichtungsregister f�r MotorEnable, MotorForward und MotorReverse aus Ausgang setzen
	DDRD = LED_Green | LED_Red;		//Datenrichtungsregister f�r LED-Green und LED-Red aus Ausgang setzen
	DDRD &= ~CCW & ~CW & ~MAN & ~AUTO;	//Datenrichtungsregister f�r CCW, CW, MAN und AUTO auf Eingang setzen
	PORTD |= CCW | CW | MAN | AUTO;	//Pullup f�r CCW, CW, MAN und AUTO aktivieren
	Enable;			//Motortreiber enablen
	timer0_init();	//Timer0 initialisieren
	adc_Init(ADC_VREF_VCC);
	DutyCycle=255;	//Duty Cycle auf Stillstand (0%) setzen
	//uart_Init(UART_BAUDRATE_9600, UART_CONFIG_8N1);
	//printf("Start\n");
	/* Replace with your application code */
	while (1)
	{
		//printf("\f");
		//printf("Channel 1: %u \nChannel 2: %u \nSchwellwert: %u\nSpeed: %u\n\n", MeasureChannel1Value, MeasureChannel2Value, Schwellwert, adc_Read_8(SpeedChannel));
		SetSpeed;		//Geschwindigkeit setzen
		Auto_Man();			//Modus ausw�hlen
		if (mode==ModeMan)	//Falls im Manuellen Modus
		{
			//printf("Manual\n");
			stopped=Go;		//Stopvariable zur�cksetzen
			CCW_CW();		//CCW(Reverse) oder CW(Forward) fahren
		}
		if (mode==ModeAuto)		//Falls im Automatikmodus
		{
			//printf("Automatik: ");
			//printf("Schwellwert: %u\n", Schwellwert);
			//printf("Kanal 2: %u\n\n", MeasureChannel2Value);
			if (((abs(MeasureChannel2Value-MeasureChannel1Value))<Schwellwert)&&stopped==Go)//Schwellwert unterschritten --> CW (Forward) fahren
			{
				//printf("Forward\n");
				direction=DirForward;		//Vorw�rts (CW) fahren
				LED_Red_Off;	//Rote LED ausschalten
				LED_Green_On;	//Gr�ne LED einschalten
			}
			if (((abs(MeasureChannel2Value-MeasureChannel1Value))>Schwellwert)&&stopped==Go)//Schwellwert �berschritten -->CCW (Reverse) fahren
			{
				//printf("Reverse\n");
				direction=DirReverse;		//R�ckw�rts (CCW) fahren
				LED_Green_Off;	//Gr�ne LED ausschalten
				LED_Red_On;		//Rote LED einschalten
			}
			else if ((abs(MeasureChannel2Value-MeasureChannel1Value))<untererSchwellwert)//Spannung unter Schwellwert(10V) gefallen --> Stoppen
			{
				//printf("Stopped\n");
				stopped=Stop;	//Stopvariable setzen --> System steht
				direction=DirBrake;			//Motor bremsen
				LED_Green_Off;	//Gr�ne LED ausschalten
				LED_Red_Off;	//Rote LED ausschalten
			}
			//_delay_ms(500);
		}
		
	}
}

