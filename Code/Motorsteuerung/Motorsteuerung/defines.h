/*
 * defines.h
 *
 * Created: 06/02/2023 20:47:31
 *  Author: joelr
 */ 



#ifndef DEFINES_H_
#define DEFINES_H_


#include <avr/io.h>

#define MOTOR_Reverse (1<<PB0)
#define MOTOR_Forward (1<<PB1)
#define MOTOR_Enable (1<<PB2)
#define Forward (PORTB |= MOTOR_Forward); (PORTB &= ~MOTOR_Reverse)
#define Reverse (PORTB|=MOTOR_Reverse); (PORTB&=~MOTOR_Forward)
#define Enable (PORTB|=MOTOR_Enable)
#define Brake (PORTB|=MOTOR_Forward|MOTOR_Reverse)

#define LED_Red (1<<PD6)
#define LED_Green (1<<PD7)
#define LED_Green_On (PORTD|=LED_Green)
#define LED_Green_Off (PORTD&=~LED_Green)
#define LED_Red_On (PORTD|=LED_Red)
#define LED_Red_Off (PORTD&=~LED_Red)

#define SpeedChannel ADC_CH_3
#define SetSpeed DutyCycle = adc_Read_8(SpeedChannel)


#define DirForward 0x01
#define DirReverse 0x02
#define DirBrake 0x03

#define ModeMan 0x01
#define ModeAuto 0x02
#define ModeStop 0x03

#define CCW (1<<PD4)
#define CW (1<<PD5)

#define MAN (1<<PD3)
#define AUTO (1<<PD2)

#define SwitchChannel ADC_CH_2
#define Schwellwert (char)(76+(adc_Read_8(SwitchChannel)*0.4))

#define MeasureChannel1 ADC_CH_0
#define MeasureChannel2 ADC_CH_1
#define MeasureChannel1Value adc_Read_8(MeasureChannel1)
#define MeasureChannel2Value adc_Read_8(MeasureChannel2)
#define untererSchwellwert 50

#define DutyCycle OCR0A

#define Stop 0x02
#define Go 0x01

volatile unsigned char direction = 0;
volatile unsigned char mode = 0;
volatile unsigned char stopped = Go;


void CCW_CW(){
	switch(PIND & (CCW | CW)){
		case CW:
			direction = DirForward;
			LED_Green_Off;
			LED_Red_On;
			break;
		case CCW:
			direction = DirReverse;
			LED_Green_On;
			LED_Red_Off;
			break;
		default:
			direction = DirBrake;
			LED_Red_Off;
			LED_Green_Off;
			break;
	}
}

void Auto_Man(){
	switch(PIND & (MAN | AUTO)){
		case MAN:
			mode=ModeMan;
			break;
		case AUTO:
			mode=ModeAuto;
			break;
		default:
			mode=ModeStop;
			break;
	}
}

void timer0_init() {
	// Set timer0 to normal mode
	TCCR0A &= ~((1<<WGM01) | (1<<WGM00));
	TCCR0B &= ~(1<<WGM02);

	// Enable compare match interrupt
	TIMSK0 |= (1<<OCIE0A);
	// Enable timer overflow interrupt
	TIMSK0 |= (1<<TOIE0);

	// Set the initial value for OCR0A (the compare match register)
	OCR0A = 0x00;

	// Set the prescaler to 64
	TCCR0B |= (1<<CS01);

	// Enable global interrupts
	sei();
}
#endif /* DEFINES_H_ */