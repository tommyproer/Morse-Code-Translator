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
unsigned int Calibrate(void);
void ADC_Interrupt(void) __interrupt 15;

char __xdata mySet[] = "##TEMNAIOGKDWRUS##QZYCXBJP#L#FVH09#8###7#######61#######2###3#45";
																//A
__sbit __at 0x90 PB;					// Configure P1.0 to PB
__sbit __at 0x91 LED;

__bit SW2press = 0;
__bit timer0 = 0;
unsigned int counts = 0;
unsigned int timer = 0;
unsigned int runningADC[10];
unsigned char array_location = 0;
unsigned int analoghi, analoglo;		// For ADC inputs
unsigned int sum;
unsigned int average;

//--------------------------------------------------------------------------
// MAIN Routine
//--------------------------------------------------------------------------
void main()
{
	char key;
	int i;
	char space;
	char letter_count = 0;
	
	char symbol;
	unsigned char cursor_reset = 0;
	unsigned char bitSum;

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
	printf("Morse Code\n\r");
	printf("1 is a dot, 0 is dash\n\r");
	

	EA = 1;				// Enable all interrupts
	EIE2 = 0x02;		// Enable ADC0 End of Conversion Interrupt

	SFRPAGE = ADC0_PAGE;
	AD0BUSY = 1;
	AD0INT = 0;

	printf("Press any key to start calibrating\n\r");
	key = getchar();
	average = Calibrate();
	printf("0x%x\n\r", average);

	while(1){

		// Might need to recalibrate after some time because the analog ADC value of the microphone changes
		if(counts > 100){
			printf("Press any key to start calibrating\n\r");
			PCA0CPH0 = 0x10;
			key = getchar();
			average = Calibrate();
			printf("0x%x\n\r", average);
			counts = 0;
		}
		
		// Turns the buzzer on if pushbutton is pressed
		if(PB){
			PCA0CPH0 = 0x10;	// Turn buzzer off
		}else{
			PCA0CPH0 = 0x00;	// Turn buzzer on
		}

		// Get the running sum of the last 10 ADC conversions and find its average
		sum = 0;
		array_location = 0;
		while(array_location < 10){
			runningADC[array_location] = (analoghi << 8) + analoglo;	// Get analog value of the ADC
			array_location++;
			for(i = 0; i < 100; i++);		// Debounce
		}
		for(i = 0; i < 10; i++){
			sum += runningADC[i];
		}
		sum /= 10;
		
		// If the ADC conversion is out of range of the average found during calibration, there is noise
		if(sum > (average + 100) || sum < (average - 100)){
			bitSum = 1;
			printf("There is noise: 1 ");
			// Get dot or dash
			i = getDashDot();
			printf("%d ", i);
			
			bitSum *= 2;
			bitSum += i;
			
			space = getSpace();
			letter_count = 0;
			while(!space && letter_count < 5){
				i = getDashDot();
				printf("%d ", i);
				space = getSpace();
				letter_count++;
				bitSum *= 2;
				bitSum += i;
			}
			printf("\n\r");

			// Print out sum
			printf("Sum: %d\n\r", bitSum);
			symbol = getCharacter(bitSum);
			printf("%c\n\r", symbol);

		}
	}
}




// Used to calibrate the ADC Level when there is no noise
unsigned int Calibrate(void){
	unsigned int i;
	unsigned int sum = 0;
	array_location = 0;
	counts = 0;
	while(counts < 2);
	while(array_location < 10){
		AD0BUSY = 1;			// Start conversion
		while(!AD0INT);			// Wait for conversion to be complete
		AD0INT = 0;				// Reset AD0INT

		runningADC[array_location] = analoglo + (analoghi << 8);
		array_location++;
		for(i = 0; i < 1000; i++);	// Small delay
	}
	for(i = 0; i < 10; i++){
		sum += runningADC[i];
	}
	return sum / 10;
}


// 1 -> dot, 2 -> dash
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
// 50 millseconds is a dot
// Return 1 if dot, return 0 if dash
char getDashDot(void){
	int i;
	unsigned char flag = 0;
	timer = 0;
	
	// There needs to be 80 "counts of silence" to exit the while loop
	// This is to ensure that there is definitely no more noise
	while(flag < 80){

		// Update buzzer if pushbutton is released or pressed
		if(PB){
			PCA0CPH0 = 0x10;
		}else{
			PCA0CPH0 = 0x00;
		}

		// Get the average of the last 10 ADC conversions
		sum = 0;
		array_location = 0;
		while(array_location < 10){
			runningADC[array_location] = (analoghi << 8) + analoglo;
			array_location++;
			for(i = 0; i < 100; i++);	// Small Delay
		}
		for(i = 0; i < 10; i++){
			sum += runningADC[i];
		}
		sum /= 10;

		// If the ADC conversion is in the range of its average, then there is silence and increment the flag counter
		// Otherwise there is noise, set the flag back to 0
		if(sum < (average + 100) && sum > (average - 100)){
			flag++;
		}else{
			flag = 0;
		}
	}
	for(i = 0; i < 300; i++);	// Debounce
	
	// If the noise continued for less than 2 counts, it was a dot and return 1
	// Else it is a dash and return a 0
	if(timer < 2){
		return 1;
	}else{
		return 0;
	}
}


// Space between same letter returns 0, space between same word returns 1, space between separate word returns 2
char getSpace(void){
	unsigned int i;
	unsigned char threshold = 0;
	timer = 0;
	
	while(1){
		// Update buzzer if pushbutton was either released or pressed
		if(PB){
			PCA0CPH0 = 0x10;
		}else{
			PCA0CPH0 = 0x00;
		}
		
		// Get the average of the last 10 ADC conversions
		sum = 0;
		array_location = 0;
		while(array_location < 10){
			runningADC[array_location] = (analoghi << 8) + analoglo;
			array_location++;
			for(i = 0; i < 100; i++);
		}
		for(i = 0; i < 10; i++){
			sum += runningADC[i];
		}
		sum /= 10;
		
		// If the ADC value is out of range of the average, there is noise and break out of the while loop
		if(sum > (average + 100) || sum < (average - 100)){
			threshold++;
			if(threshold == 3){
				break;
			}
		}else{
			threshold = 0;
		}
	}
	
	// If silence lasted less than 3 counts, then the space is within the letter and return 0
	// Else if the silence lasted less than 6 counts, then the space is withing the word and return 1
	// Else it is a new word and return a 2
	if(timer < 3){
		return 0;
	}else if(timer < 6){
		return 1;
	}else{
		return 2;
	}
}


void ADC_Interrupt(void) __interrupt 15
{
	SFRPAGE = ADC0_PAGE;
	analoglo = ADC0L;
	analoghi = ADC0H;
	AD0BUSY = 1;
	AD0INT = 0;
}

// Timer interrupt
void Timer0_ISR (void) __interrupt 1
{
	counts++;
	timer++;		
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
	P1MDOUT &= ~0x01;					// P1.0 as open drain
	P1 = 0x01;						// P1.0 as high impedence

	EA		= 1;			// Enable interrupts as selected.
	P3MDOUT = 0xF0;
	P3 = 0x0F;
	TCON &= 0xFC;
	IE |= 0x81;

	XBR0	= 0x0C;			// Enable UART0, Tx0 and RX0 routed to 0.0 and 0.1
	XBR1	= 0x00;			// /INT0 routed to port pin. 0.2
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

void ADC_INIT(void){

	char SFRPAGE_SAVE = SFRPAGE;    // Save Current SFR page.
	SFRPAGE = ADC0_PAGE;

	REF0CN = 0x03;					// VREF0 = 2.4 V (= 0x02 to use external VREF0)
	AMX0SL = 0x00;					// Use AIN0.0
	AMX0CF = 0x00;					
	ADC0CN = 0x80;					// Enable ADC0

	SFRPAGE = SFRPAGE_SAVE;			// Restore SFR page
}
