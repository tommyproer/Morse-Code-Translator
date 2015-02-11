// Filename: Morse Code
//--------------------------------------------------------------------------
// Includes
//--------------------------------------------------------------------------
#include <LCD.c>
#include <stdio.h>					// Necessary for printf.
#include <stdlib.h>
#include <c8051f120.h>				// SFR declarations.
#include "putget.h"

//--------------------------------------------------------------------------
// Function Prototypes
//--------------------------------------------------------------------------
void main(void);
void SYSCLK_INIT(void);
void UART0_INIT(void);
void PORT_INIT(void);
void Timer0_INIT(void);
void PCA0_INIT(void);
void ADC_INIT(void);

char getDashDot(void);
char getSpace(void);
void Timer0_ISR (void) __interrupt 1;
char getCharacter(unsigned int sum);

char mySet[] = "##TEMNAIOGKDWRUS##QZYCXBJP#L#FVH09#8###7#######61#######2###3#45";

__sbit __at 0x90 PB;					// Configure P1.0 to PB
__sbit __at 0x91 LED;

__bit SW2press = 0;
__bit timer0 = 0;
int counts = 0;

//--------------------------------------------------------------------------
// MAIN Routine
//--------------------------------------------------------------------------
void main()
{
	char key;
	int i;
	__bit dot;
	char space;
	char letter_count = 0;
	unsigned int sum;
	char symbol;
	unsigned char cursor_reset = 0;

	unsigned char analoghi, analoglo;		// For ADC inputs

	WDTCN = 0xDE;		// Disable the watchdog timer
	WDTCN = 0xAD;		// note: = "DEAD"!
	
	SYSCLK_INIT();		// Initialize the oscillator
	UART0_INIT();
	PORT_INIT();
	Timer0_INIT();
	PCA0_INIT();
	ADC_INIT();
	
	SFRPAGE = UART0_PAGE;
	printf("\033[1;24r");			// Makes lines 1 to 24 scrollable
	printf("\033[2J");				// Erase screen and move cursor to the home posiiton.
	printf("Morse Code: press the pushbutton to start\n\r");
	printf("1 is a dot, 0 is dash\n\r");

	while(1){
		
		// Initialize sum and add leading 1 to sum
		sum = 0;
		dot = 1;
		sum += dot;
		printf("%d ", dot);

		// Get input from user
		while(PB);					// Wait for user to push button
		PCA0CPH0 = 0x00;			// Turn on buzzer
		for(i = 0; i < 300; i++);	// Debounce
		dot = getDashDot();			// Determine if dash or dot based on how long the user holds down pushbutton
		printf("%d ", dot);			// Print out dash or dot (if it is a dot it is a 1, if it is a dash it is a 0)
		space = getSpace();			// Determine if space will seperate within the letter, within the word or create a whole new word
									// (if it is within the letter it is = 0, within the letter = 1, a new word = 2)
		// Add the dot or dash to the sum
		sum *= 2;
		sum += dot;
		
		// While it is the same letter, precede to get whether or not it is a dash or dot, get the space value and add to the sum
		// Make sure the number of dots and dashes don't exceed 5
		letter_count = 0;
		while(!space && letter_count < 5){
			dot = getDashDot();
			printf("%d ", dot);
			space = getSpace();
			letter_count++;
			sum *= 2;
			sum += dot;
		}
		printf("\n\r");

		// Print out total sum of the character
		printf("Sum: %d\n\r", sum);

		//Find which character it is and print it
		symbol = getCharacter(sum);
		printf("%c\n\r", symbol);

	}
}


// 1 -> dot, 2 -> dash
// Get the character from the sum
char getCharacter(unsigned int sum){
	if(sum < 64){
		return mySet[sum];
	}
	if(sum == 71)
		return ':';
	if(sum == 76)
		return ',';
	if(sum == 84)
		return '!';
	if(sum == 94)
		return '-';
	if(sum == 101)
		return '@';
	if(sum == 106)
		return '.';
	if(sum == 115)
		return '?';

	return '#';
}


// A dash is the length of 3 dots, and spacings are specified in number of dot lengths. 
// Return 1 if dot, return 0 if dash
char getDashDot(void){
	int i;
	counts = 0;					// Initialize counts to 0

	while(!PB);					// Wait for user to release button
	PCA0CPH0 = 0x10;			// Turn off buzzer
	for(i = 0; i < 500; i++);	// Debounce

	// If the user held down the button below a certain threshold, then it is a dot and return a 1
	// Else it is a dash and return a 0
	if(counts < 2){
		return 1;
	}else{
		return 0;
	}
}


// Space between same letter returns 0, space between same word returns 1, space between separate word returns 2
char getSpace(void){
	int i;
	counts = 0;

	// 1 unit, 3 units, 7 units
	while(PB);			// Wait for user to press button
	PCA0CPH0 = 0x00;	// Turn buzzer on
	for(i = 0; i < 500; i++);	// Debounce

	// If the spacing is below certain thresholds, it can either be within the same letter, within the same word, or separate words
	// Return a 0 if within the same letter, return a 1 if within the same word, return 2 if separate words
	if(counts < 3){
		return 0;
	}else if(counts < 6){
		return 1;
	}else{
		return 2;
	}
}


// Timer interrupt
void Timer0_ISR (void) __interrupt 1
{
	counts++;		
	timer0 = 1;		// Set timer0 bit to 1
	TH0 = 0x4C;		// Reload the Timer 0 High byte value
	TL0 = 0x00;		// Reload the Timer 0 Low byte value
}

// Port initialization
void PORT_INIT(void)
{	
	char SFRPAGE_SAVE = SFRPAGE;    // Save Current SFR page.
	SFRPAGE = CONFIG_PAGE;

	//Pushbutton at 1.0
	P0MDOUT |= 0x04;				// Set 0.2 to push-pull
	P1MDOUT &= ~0x01;					// P1.0 as open drain
	P1 = 0x01;						// P1.0 as high impedence

	EA		= 1;			// Enable interrupts as selected.
	P3MDOUT = 0xF0;
	P3 = 0x0F;
	TCON &= 0xFC;
	IE |= 0x81;

	XBR0	= 0x0C;			// Enable UART0, Tx0 and RX0 routed to 0.0 and 0.1
	XBR1	= 0x00;
	XBR2	= 0x40;			// Enable Crossbar and weak pull-ups.

	SFRPAGE = SFRPAGE_SAVE;	// Restore SFR page.
}

//------------------------------------------------------------------------------------
// SYSCLK_Init
//------------------------------------------------------------------------------------
// Initialize the system clock to use a 22.11845MHz crystal as its clock source
void SYSCLK_INIT(void)
{
	int i;
	char SFRPAGE_SAVE;

	SFRPAGE_SAVE = SFRPAGE;			// Save Current SFR page
	SFRPAGE = CONFIG_PAGE;

	OSCXCN    = 0x67;
	for (i = 0; i < 3000; i++);		// Wait 1ms for initialization
	while (!(OSCXCN & 0x80));
	CLKSEL    = 0x01;

	SFRPAGE = SFRPAGE_SAVE;			// Restore SFR page
}

// UART0 initialization
void UART0_INIT(void)
{
	char SFRPAGE_SAVE = SFRPAGE;    // Save Current SFR page.
	SFRPAGE = TIMER01_PAGE;
	
	TCON	 = 0x40;				// Enable Timer 1 running (TR1)
	TMOD	&= 0x0F;
	TMOD	|= 0x20;				// Timer1, Mode 2: 8-bit counter/timer with auto-reload.
	CKCON	|= 0x10;				// Timer1 uses SYSCLK as time base.
//	TH1		 = 256 - SYSCLK/(BAUDRATE*32)  Set Timer1 reload baudrate value T1 Hi Byte
	TH1		 = 0xE8;				// 0xE8 = 232
	TR1		 = 1;					// Start Timer1.

	SFRPAGE = UART0_PAGE;
	SCON0	= 0x50;					// Set Mode 1: 8-Bit UART
	SSTA0	 = 0x00;				// SMOD0 = 0, in this mode
									// TH1 = 256 - SYSCLK/(baud rate * 32)
	TI0		= 1;					// Indicate TX0 ready.
	
	SFRPAGE = SFRPAGE_SAVE;			// Restore SFR page.
}

// Timer0 Initialization
void Timer0_INIT(void)
{
	char SFRPAGE_SAVE = SFRPAGE;    // Save Current SFR page.
	SFRPAGE = TIMER01_PAGE;
	ET0 = 1;						// Enable Timer0 interrupt
	TMOD |= 0x01;					// Set to Mode 1: 16-bit counter/timer
	CKCON |= 0x02;					// Set to system clock and system clock divided by 48
	CKCON &= ~0x09;
	//TMOD &= ~0x02;				// Set to Mode 0: 13-bit counter/timer				
	TR0 = 1;						// Timer 0 enabled
	SFRPAGE = SFRPAGE_SAVE;			// Restore SFR page
}

// PCA0 initialization
void PCA0_INIT(void)
{
	char SFRPAGE_SAVE = SFRPAGE;
	SFRPAGE = PCA0_PAGE;

	PCA0CN = 0x40;		// Enable PCA0
	PCA0CPM0 = 0x46;	// 
	PCA0MD = 0x80;
	PCA0CPH0 = 0x10;	// Disable Buzzer

	SFRPAGE = SFRPAGE_SAVE;
}

void ADC_INIT(void){

	char SFRPAGE_SAVE = SFRPAGE;    // Save Current SFR page.
	SFRPAGE = ADC0_PAGE;

	REF0CN = 0x02;					// internal buffer is off, and use VREF0
	AMX0SL = 0x00;					// Use AIN0.0
	AMX0CF = 0x00;					
	ADC0CN = 0x80;					// Enable ADC0

	SFRPAGE = SFRPAGE_SAVE;			// Restore SFR page
}
