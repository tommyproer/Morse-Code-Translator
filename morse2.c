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

void Timer0_ISR (void) __interrupt 1;
unsigned char getMorseCode(unsigned char ascii);
void buzzMorseCode(unsigned char sum);

//Subtract 48
//0,1,2,3,4,5,6,7,8,9
unsigned static char __xdata asciiSums[] = {32,48,56,60,62,63,39,35,33,71,0,0,0,0,0,0,0,6,23,21,11,3,29,9,31,7,24,10,27,4,5,8,25,18,13,15,2,14,30,12,22,20,19};
																	//A
__sbit __at 0x90 PB;					// Configure P1.0 to PB
__sbit __at 0x91 LED = 0;

__bit SW2press = 0;
__bit timer0 = 0;
int counts = 0;


//--------------------------------------------------------------------------
// MAIN Routine
//--------------------------------------------------------------------------
void main()
{
	int i;
	char key;
	char letter_count = 0;
	unsigned char cursor_reset = 0;

	unsigned char sentence[20];
	unsigned char sentence_location = 0;
	unsigned char end_sentence_location;

	WDTCN = 0xDE;		// Disable the watchdog timer
	WDTCN = 0xAD;		// note: = "DEAD"!
	
	SYSCLK_INIT();		// Initialize the oscillator
	UART0_INIT();
	PORT_INIT();
	Timer0_INIT();
	PCA0_INIT();
	
	SFRPAGE = UART0_PAGE;
	printf("\033[1;24r");			// Makes lines 1 to 24 scrollable
	printf("\033[2J");				// Erase screen and move cursor to the home posiiton.
	printf("Morse Code: enter up to 20 characters, press ESC to end\n\r");
	
	
	while(1){
		
		while(sentence_location < 20){
			// Get a character from user and print the character
			key = getchar();
			printf("%c", key);
			if(key == 27){
				end_sentence_location = sentence_location;
				break;
			}
			
			i = getMorseCode(key);	// Translate the character into a sum
			sentence[sentence_location] = i;
			sentence_location++;
		}
		
		for(sentence_location = 0; sentence_location < end_sentence_location; sentence_location++){
			buzzMorseCode(sentence[sentence_location]);

			// Silence for space between letters
			counts = 0;
			while(counts < 6);
		}
		sentence_location = 0;
		end_sentence_location = 20;

		printf(" ");
		printf("\n\r");
	}
}

// Translate the character into its sum
unsigned char getMorseCode(unsigned char ascii){
	if(ascii == ' ')	
		return 0;
	if(ascii == ':')
		return 71;
	if(ascii == ',')
		return 76;
	if(ascii == '!')
		return 84;
	if(ascii == '-')
		return 94;
	if(ascii == '@')
		return 101;
	if(ascii == '.')
		return 106;
	if(ascii == '?')
		return 115;

	ascii -= 48;
	return asciiSums[ascii];
}

// Buzz the amount of times based on the sum amount
void buzzMorseCode(unsigned char sum){
	
	int i;
	int morseArray[] = {-1, -1, -1, -1, -1, -1};
	int morseCount = 0;
	int mod;

	// If it is zero, wait 6 "counts"
	if(!sum){
		counts = 0;
		while(counts < 14);
		return;
	}

	// NOTE:
	// Instead of filling array from right to left, and reading left to right
	// We will be filling array left to right and reading array right to left

	// Reduce the sum to its binary representation and store it in an array, but ignore the last leading 1
	while(sum != 1){
		mod = sum % 2;
		sum /= 2;
		morseArray[morseCount] = mod;
		morseCount++;
	}

	// Go through the array backwards and buzz for 2 counts if it is a dot, and buzz for 6 counts if it is a dash
	morseCount = 5;
	while(morseCount > -1){
		// If it is a negative one, continue
		if(morseArray[morseCount] == -1){
			morseCount--;
			continue;
		}
		// If it is a 1, it is a dot: Buzz for 2 counts
		if(morseArray[morseCount]){
			// Dot
			LED = 1;			// Turn LED on
			PCA0CPH0 = 0x00;	// Turn buzzer on
			counts = 0;
			while(counts < 2);
		}else{
			// Dash
			LED = 1;			// Turn LED on
			PCA0CPH0 = 0x00;	// Turn buzzer on
			counts = 0;
			while(counts < 6);
		}
		// At the end, turn the buzzer off for 2 counts
		LED = 0;				// Turn LED on
		PCA0CPH0 = 0x10;		// Turn buzzer off
		counts = 0;				// Reset counts for timer
		while(counts < 2);
		morseCount--;
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

void PORT_INIT(void)
{	
	char SFRPAGE_SAVE = SFRPAGE;    // Save Current SFR page.
	SFRPAGE = CONFIG_PAGE;

	//Pushbutton at 1.0
	P0MDOUT |= 0x04;				// Set 0.2 to push-pull
	P1MDOUT = 0x02;					// P1.0 as open drain, P1.1 as push-pull
	P1 = 0x01;						// P1.0 as high impedence

	EA		= 1;			// Enable interrupts as selected.
	P3MDOUT = 0xF0;
	P3 = 0x0F;

	TCON &= 0xFC;
	IE |= 0x81;



	XBR0	= 0x0C;			// Enable UART0, Tx0 and RX0 routed to 0.0 and 0.1, CEX0 routed to 0.2
	XBR1	= 0x00;
	XBR2	= 0x40;			// Enable Crossbar and weak pull-ups.


	SFRPAGE = SFRPAGE_SAVE;	// Restore SFR page.
}

//------------------------------------------------------------------------------------
// SYSCLK_Init
//------------------------------------------------------------------------------------
//
// Initialize the system clock to use a 22.11845MHz crystal as its clock source
//
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


	P7MDOUT=0x07;					// Set E, RW, RS controls to push-pull
	P6MDOUT=0x00;					// P6 must be open-drain to be bidirectional:
									// used for both read & write operations

	P6 = 0xFF;		// Open drain

	SFRPAGE = SFRPAGE_SAVE;			// Restore SFR page
}

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

void PCA0_INIT(void)
{
	char SFRPAGE_SAVE = SFRPAGE;
	SFRPAGE = PCA0_PAGE;

	PCA0CN = 0x40;	// enable PCA0
	PCA0CPM0 = 0x46;
	PCA0MD = 0x80;
	PCA0CPH0 = 0x10;	// Disable Buzzer

	SFRPAGE = SFRPAGE_SAVE;
}
