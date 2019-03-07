#include <avr/io.h>
#include <avr/interrupt.h>
#include <bit.h>
#include <timer.h>
#include <stdio.h>
#include "io.c"
#include <usart_ATmega1284.h>

// Global variables
const unsigned char one = 0xF6; // Maps to "1" on the BCD to 7 segment display
const unsigned char two = 0xC8; // Maps to "2" on the BCD to 7 segment display
const unsigned char off = 0xFF; // Displays nothing on the BCD to 7 segment display
unsigned char floorNumber = 1; 
unsigned char display;
unsigned char blinkTime = 0;
unsigned char moving = 0;
unsigned char output3 = 0;
unsigned char output4 = 0;
unsigned char moveTime = 0;

// Function that calculates and returns the GCD of 2 long ints
unsigned long int findGCD(unsigned long int a, unsigned long int b)
{
	unsigned long int c;
	while(1){
		c = a%b;
		if(c==0){return b;}
		a = b;
		b = c;
	}
	return 0;
}

// Struct for Tasks represent a running process in our simple real-time operating system.
typedef struct _task {
	signed char state; //Task's current state
	unsigned long int period; //Task period
	unsigned long int elapsedTime; //Time elapsed since last task tick
	int (*TickFct)(int); //Task tick function
} task;

//Enumeration of states.
enum SM1_States { SM1_Init, SM1_Wait };

int SMTick1(int state) {
	unsigned char display;
	
	switch (state) {
		case SM1_Init:
			state = SM1_Wait;
			break;
		case SM1_Wait:
			if (USART_HasReceived(1)) {
				display = USART_Receive(1);
			}
			else {
				display = 0xFF;
			}
			state = SM1_Wait;
			break;
		default:
			break;
	}
	
	PORTB = display;
	
	return state;
}

int main()
{
	DDRB = 0xFF; PORTB = 0x00; // Output
	
	// Period for the tasks
	unsigned long int SMTick1_calc = 100;

	//Recalculate GCD periods for scheduler
	unsigned long int SMTick1_period = SMTick1_calc;

	//Declare an array of tasks
	static task task1;
	task *tasks[] = { &task1 };
	const unsigned short numTasks = sizeof(tasks)/sizeof(task*);
	
	// BCD to 7 Segment and Keypad task
	task1.state = SM1_Init; //Task initial state.
	task1.period = SMTick1_period; //Task Period.
	task1.elapsedTime = SMTick1_period; //Task current elapsed time.
	task1.TickFct = &SMTick1; //Function pointer for the tick.
 
	// Set the timer and turn it on
	TimerSet(SMTick1_period);
	TimerOn();
	
	// Initialize USART
	initUSART(1);

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