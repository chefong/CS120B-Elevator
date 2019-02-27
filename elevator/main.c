#include <avr/io.h>
#include <avr/interrupt.h>
#include <bit.h>
#include <timer.h>
#include <stdio.h>
#include "io.c"

// Global variables
unsigned char floorNumber = 1;

// Returns '\0' if no key pressed, else returns char '1', '2', ... '9', 'A', ...
// If multiple keys pressed, returns leftmost-topmost one
// Keypad must be connected to port C
/* Keypad arrangement
        PC4 PC5 PC6 PC7
   col  1   2   3   4
row
PC0 1   1 | 2 | 3 | A
PC1 2   4 | 5 | 6 | B
PC2 3   7 | 8 | 9 | C
PC3 4   * | 0 | # | D
*/
unsigned char GetKeypadKey() {

	PORTC = 0xEF; // Enable col 4 with 0, disable others with 1’s
	asm("nop"); // add a delay to allow PORTC to stabilize before checking
	if (GetBit(PINC,0)==0) { return('1'); }
	if (GetBit(PINC,1)==0) { return('4'); }
	if (GetBit(PINC,2)==0) { return('7'); }
	if (GetBit(PINC,3)==0) { return('*'); }

	// Check keys in col 2
	PORTC = 0xDF; // Enable col 5 with 0, disable others with 1’s
	asm("nop"); // add a delay to allow PORTC to stabilize before checking
	if (GetBit(PINC,0)==0) { return('2'); }
	if (GetBit(PINC,1)==0) { return('5'); }
	if (GetBit(PINC,2)==0) { return('8'); }
	if (GetBit(PINC,3)==0) { return('0'); }
	// ... *****FINISH*****

	// Check keys in col 3
	PORTC = 0xBF; // Enable col 6 with 0, disable others with 1’s
	asm("nop"); // add a delay to allow PORTC to stabilize before checking
	// ... *****FINISH*****
	if (GetBit(PINC,0)==0) { return('3'); }
	if (GetBit(PINC,1)==0) { return('6'); }
	if (GetBit(PINC,2)==0) { return('9'); }
	if (GetBit(PINC,3)==0) { return('#'); }

	// Check keys in col 4	
	// ... *****FINISH*****
	PORTC = 0x7F;
	asm("nop"); // add a delay to allow PORTC to stabilize before checking
	// ... *****FINISH*****
	if (GetBit(PINC,0)==0) { return('A'); }
	if (GetBit(PINC,1)==0) { return('B'); }
	if (GetBit(PINC,2)==0) { return('C'); }
	if (GetBit(PINC,3)==0) { return('D'); }

	return('\0'); // default value

}

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
	unsigned char display = ~PIND; // Set D0 - D5 as outputs
	unsigned char press = GetKeypadKey();
	
	switch (state) {
		case SM1_Init:
			state = SM1_Wait;
			break;
		case SM1_Wait:
			if (press == '1') {
				floorNumber = 1;
			}
			else if (press == '2') {
				floorNumber = 2;
			}
			state = SM1_Wait;
			break;
		default:
			break;
	}
	
	if (floorNumber == 1) {
		// Assign display to value of segments to turn on number "1"
		display = 0xF6;
	}
	else if (floorNumber == 2) {
		// Assign display to value of segments to turn on number "2" 
		display = 0xC8;
	}
	
	PORTD = display; // Assign PORTB to the floor number value stored in display

	return state;
}

int main()
{
	DDRA = 0x00; PORTA = 0xFF; // Input
	DDRD = 0xFF; PORTD = 0x00; // Output
	DDRC = 0xF0; PORTC = 0x0F; // PC7..4 outputs init 0s, PC3..0 inputs init 1s

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