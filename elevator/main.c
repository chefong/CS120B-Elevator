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
unsigned char currentFloor = 1;
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

	PORTC = 0xEF; // Enable col 4 with 0, disable others with 1�s
	asm("nop"); // add a delay to allow PORTC to stabilize before checking
	if (GetBit(PINC,0)==0) { return('1'); }
	if (GetBit(PINC,1)==0) { return('4'); }
	if (GetBit(PINC,2)==0) { return('7'); }
	if (GetBit(PINC,3)==0) { return('*'); }

	// Check keys in col 2
	PORTC = 0xDF; // Enable col 5 with 0, disable others with 1�s
	asm("nop"); // add a delay to allow PORTC to stabilize before checking
	if (GetBit(PINC,0)==0) { return('2'); }
	if (GetBit(PINC,1)==0) { return('5'); }
	if (GetBit(PINC,2)==0) { return('8'); }
	if (GetBit(PINC,3)==0) { return('0'); }
	// ... *****FINISH*****

	// Check keys in col 3
	PORTC = 0xBF; // Enable col 6 with 0, disable others with 1�s
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
	
	if (USART_IsSendReady(0)) {
		if (floorNumber == 1) {
			// Assign display to value of segments to turn on number "1"
			//display = one;
			USART_Send(one, 0);
		}
		else if (floorNumber == 2) {
			// Assign display to value of segments to turn on number "2" 
			//display = two;
			USART_Send(two, 0);
		}
	}
	
	return state;
}

//Enumeration of states.
enum SM2_States { SM2_Init, SM2_Wait, SM2_BlinkOn, SM2_BlinkOff };

int SMTick2(int state) {
	unsigned char button = ~PINA & 0x01;
	
	switch (state) {
		case SM2_Init:
			state = SM2_Wait;
			break;
		case SM2_Wait:
			if (button && !moving) { // If the button is pressed AND the elevator isn't moving
				state = SM2_BlinkOn;
				moving = 1;
			}
			else {
				state = SM2_Wait;
			}
			break;
		case SM2_BlinkOn:
			if (!moving) { // Stop blinking once the motor stops and go back to wait state
				state = SM2_Wait;
			}
			else if (blinkTime >= 2) { // Switch to off after 200 ms
				state = SM2_BlinkOff;
				blinkTime = 0;
			}
			else {
				state = SM2_BlinkOn;
			}
			break;
		case SM2_BlinkOff:
			if (blinkTime >= 2) { // Switch to on after 200 ms
				state = SM2_BlinkOn;
				blinkTime = 0;
			}
			else {
				state = SM2_BlinkOff;
			}
			break;
		default:
			break; 
	 }
	 
	 switch (state) {
		case SM2_BlinkOn:
			if (floorNumber == 1) {
				display = one;
			}
			else if (floorNumber == 2) {
				display = two;
			}
			blinkTime++;
			break;
		case SM2_BlinkOff:
			display = off;
			blinkTime++;
			break;
		default:
			break;	
	}
	
	//PORTD = display;
	
	return state;
}

//Enumeration of states.
enum SM3_States { SM3_Init, SM3_Wait, SM3_MoveUp, SM3_MoveDown };

int SMTick3(int state) {
	unsigned char button = ~PINA & 0x01;
	
	switch (state) {
		case SM3_Init:
			state = SM3_Wait;
			break;
		case SM3_Wait:
			if (button) { // if the button is pressed, turn on the motor to move up
				if (floorNumber == 2 && currentFloor == 1) { // check if I'm on the 1st floor before moving up
					state = SM3_MoveUp;
				}
				else if (floorNumber == 1 && currentFloor == 2) { // check if I'm on the 2nd floor before moving down
					state = SM3_MoveDown;
				}
				else {
					state = SM3_Wait; // else, don't do anything
				}
			}
			else {
				state = SM3_Wait;
			}
			break;
		case SM3_MoveUpOne:
			if (moveTime < 30) { // Turn on motor for 3 seconds
				state = SM3_MoveUpOne;
			}
			else {
				state = SM3_Wait;
			}
			break;
		case SM3_MoveDown:
			break;
		default:
			break;
	}

	switch (state) {
		case SM3_MoveUp:
			output3 = 1;
			output4 = 0;
			moveTime++;
			currentFloor = 2; // Now I'm on the 2nd floor
			break;
		case SM3_MoveDown:
			output3 = 0;
			output4 = 1;
			moveTime++;
			currentFloor = 1; // Now I'm on the 1st floor
			break;
		case SM3_Wait:
			output3 = 0;
			output4 = 0;
			moveTime = 0;
			moving = 0;
			break;
		default:
			break;
	}
	
	PORTB = 0x03 & (output3 + (output4 << 1));
	
	return state;
}

int main()
{
	DDRA = 0x00; PORTA = 0xFF; // Input
	DDRD = 0xFF; PORTD = 0x00; // Output
	DDRB = 0xFF; PORTB = 0x00; // Output
	DDRC = 0xF0; PORTC = 0x0F; // PC7..4 outputs init 0s, PC3..0 inputs init 1s
	
	// Period for the tasks
	unsigned long int SMTick1_calc = 100;
	unsigned long int SMTick2_calc = 500;
	unsigned long int SMTick3_calc = 100;
	
	//Calculating GCD
	unsigned long int tmpGCD = 1;
	tmpGCD = findGCD(SMTick1_calc, SMTick2_calc);
	tmpGCD = findGCD(tmpGCD, SMTick3_calc);
	
	//Greatest common divisor for all tasks or smallest time unit for tasks.
	unsigned long int GCD = tmpGCD;

	//Recalculate GCD periods for scheduler
	unsigned long int SMTick1_period = SMTick1_calc/GCD;
	unsigned long int SMTick2_period = SMTick2_calc/GCD;
	unsigned long int SMTick3_period = SMTick3_calc/GCD;

	//Declare an array of tasks
	static task task1;
	static task task2;
	static task task3;
	task *tasks[] = { &task1, &task2, &task3 };
	const unsigned short numTasks = sizeof(tasks)/sizeof(task*);
	
	// BCD to 7 Segment and Keypad task
	task1.state = SM1_Init; //Task initial state.
	task1.period = SMTick1_period; //Task Period.
	task1.elapsedTime = SMTick1_period; //Task current elapsed time.
	task1.TickFct = &SMTick1; //Function pointer for the tick.
	
	// Blinking task
	task2.state = SM2_Init; //Task initial state.
	task2.period = SMTick2_period; //Task Period.
	task2.elapsedTime = SMTick2_period; //Task current elapsed time.
	task2.TickFct = &SMTick2; //Function pointer for the tick.
	
	// DC Motor task
	task3.state = SM3_Init; //Task initial state.
	task3.period = SMTick3_period; //Task Period.
	task3.elapsedTime = SMTick3_period; //Task current elapsed time.
	task3.TickFct = &SMTick3; //Function pointer for the tick.

	// Set the timer and turn it on
	TimerSet(GCD);
	TimerOn();
	
	// Initialize USART
	initUSART(0);
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