#include <avr/io.h>
#include <avr/interrupt.h>
#include <bit.h>
#include <timer.h>
#include <stdio.h>
#include "io.c"

// Struct for Tasks represent a running process in our simple real-time operating system.
typedef struct _task {
	signed char state; //Task's current state
	unsigned long int period; //Task period
	unsigned long int elapsedTime; //Time elapsed since last task tick
	int (*TickFct)(int); //Task tick function
} task;

//Enumeration of states.
enum SM1_States { SM1_Init, SM1_Wait, SM1_On };

int SMTick1(int state) {
	unsigned char button = ~PINA & 0x01;
	unsigned char led = ~PINB & 0x02;
	
	switch (state) {
		case SM1_Init:
			state = SM1_Wait;
			break;
		case SM1_Wait:
			if (!button) {
				state = SM1_Wait;
			}
			else {
				state = SM1_On;
			}
			break;
		case SM1_On:
			if (button) {
				state = SM1_On;
			}
			else {
				state = SM1_Wait;
			}
			break;
		default:
			break;
	}
	
	switch (state) {
		case SM1_On:
			led = 0;
			break;
		case SM1_Wait:
			led = 3;
			break;
		default:
			break;
	}
	
	PORTB = led;

	return state;
}

int main()
{
	DDRB = 0xFF; PORTB = 0x00;
	DDRA = 0x00; PORTA = 0xFF;

	// Period for the tasks
	unsigned long int SMTick1_calc = 100;

	//Recalculate GCD periods for scheduler
	unsigned long int SMTick1_period = SMTick1_calc;

	//Declare an array of tasks
	static task task1;
	task *tasks[] = { &task1 };
	const unsigned short numTasks = sizeof(tasks)/sizeof(task*);

	task1.state = SM1_Init; //Task initial state.
	task1.period = SMTick1_period; //Task Period.
	task1.elapsedTime = SMTick1_period; //Task current elapsed time.
	task1.TickFct = &SMTick1; //Function pointer for the tick.

	// Set the timer and turn it on
	TimerSet(task1.period);
	TimerOn();

	unsigned short i; // Scheduler for-loop iterator
	while(1) {
		// Scheduler code
		for ( i = 0; i < numTasks; i++ ) {
			tasks[i]->state = tasks[i]->TickFct(tasks[i]->state);
		}
		while(!TimerFlag);
		TimerFlag = 0;
	}

	return 0;
}