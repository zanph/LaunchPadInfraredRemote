#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <inc/hw_memmap.h>
#include <driverlib/gpio.h>
#include <driverlib/pin_map.h>
#include <driverlib/sysctl.h>
#include "launchpad.h"
#include "seg7.h"
#include "seg7digit.h"
#include "buzzer.h"
#include "infrared.h"


#define NUMBER_OF_DIGITS 4
#define ERROR            99 // passed by the infrared code
#define BLANK            10
#define OKAY             10 // okay button
#define DELETE           11 // back button
#define MODE_SWITCH      12 // down arrow
#define START
#define STOP

// Immediately upon start we let the user enter a value for the timer
// if they hit OK, then the timer starts. If they hit mode, we
// switch to stopwatch mode
static  enum {Timer, Stopwatch, Entry} Mode = Entry;

static  uint8_t                        Digits[NUMBER_OF_DIGITS] = {BLANK, BLANK, BLANK, BLANK};

typedef enum {Run, Pause} state_t;
// Status of the stopwatch
static struct {
   state_t state;// the running state of the stopwatch
   uint8_t m1;// 1st digit of minutes
   uint8_t m2;// 2nd digit of minutes
   uint8_t s1;// 1st digit of seconds
   uint8_t s2;// 2nd digit of seconds
} display = {Pause, 0, 0, 0, 0};


uint8_t getDecodedDigit() {
   //execute callbacks while we wait for the next
   //digit so that the timer/stopwatch functionality
   //will work without the remote functionality
   //blocking it.
	while(!getDigitReadyStatus()){
		schdExecute();
	}
	return getCurrentDigit();
}

//reset all digits to blank
void resetDigits() {
   Digits[0] = BLANK;
   Digits[1] = BLANK;
   Digits[2] = BLANK;
   Digits[3] = BLANK;
}

//reset display state values to zero
void resetStopwatch() {
   display.s2 = 0;
   display.s1 = 0;
   display.m2 = 0;
   display.m1 = 0;
}

//delete the least significant digit, and shift
//all the others to the right one place.
void deleteDigit() {
   Digits[3] = Digits[2];
   Digits[2] = Digits[1];
   Digits[1] = Digits[0];
   Digits[0] = BLANK;
}

//simply load 0 if the digit is BLANK, otherwise,
//load the digit value.
void getDisplayStateFromDigits() {
	display.s2 = (Digits[3] == BLANK) ? 0 : Digits[3];
	display.s1 = (Digits[2] == BLANK) ? 0 : Digits[2];
	display.m2 = (Digits[1] == BLANK) ? 0 : Digits[1];
	display.m1 = (Digits[0] == BLANK) ? 0 : Digits[0];
}


//written using empirically obtained values
//to make the buzzer beep a few times
//to sound kind of like an egg timer or alarm
//going off.
void playBuzzer() {
   uint16_t i;
   uint8_t  j;
   for(j = 0; j < 15; ++j) {
      for(i = 0; i < 700; ++i) {
        // Turn on/off the buzzer every 10000 microseconds, producing a 2KHz square waveform
        buzzOn();
        waitUs(1000);
        buzzOff();
        waitUs(1000);
      }
      waitMs(5000);
   }
   //re-enter entry mode after the timer is done.
   Mode = Entry;
   display.state = Pause;
   resetDigits();
}

void stopwatchUpdate() {
   // Calculate the display digits for the next update
   //assuming we are in stopwatch mode
   if (++display.s2 >= 10) {
      display.s2 = 0;
      if (++display.s1 >= 6) {
         display.s1 = 0;
         if (++display.m2 >= 10) {
            display.m2 = 0;
            if (++display.m1 >= 6) {
               display.m1 = 0;
            }
         }
      }
   }
}

void timerUpdate() {
   // Calculate the display digits for the next update
   //assuming we are in timer mode
   if(display.s2 == 0) {
      if(display.s1 != 0) { //borrow from s1
         --display.s1;
         display.s2 = 9;
      }
      else if(display.m2 != 0) { //borrow from m2
         --display.m2;
         display.s1 = 5;
         display.s2 = 9;
      }
      else if(display.m1 != 0) { //borrow from m1
         --display.m1;
         display.m2 = 9;
         display.s1 = 5;
         display.s2 = 9;
      }
      else { //we're at all zeros!
    	  playBuzzer();
      }
   }
   else { //no borrow necessary
      --display.s2;
   }
}

void updateDigits(uint8_t digit) {
   //if there is no space, return
   if(Digits[0] != BLANK) {
      return;
   }
   //otherwise shift all digits left one,
   //and set the least significant digit
   //to our new digit.
   Digits[0] = Digits[1];
   Digits[1] = Digits[2];
   Digits[2] = Digits[3];
   Digits[3] = digit;
}


// Update the clock display
void
displayUpdate(uint32_t time)
{
   if (Mode == Entry) { //we dont want to update the display
      //need to schedule a callback here too otherwise the display
      //never updates again
      schdCallback(displayUpdate, time + 100);
      return;
   }
   // Update clock and display only if the stopwatch is running
   if(display.state == Run && Mode == Timer) {
      timerUpdate();
      // Update the display
      seg7DigitUpdate(display.m1, display.m2, display.s1, display.s2); // call stack: stopWatchUpdate->seg7DigitUpdate->Seg7Update
   }
   else if(display.state == Run && Mode == Stopwatch) {
      stopwatchUpdate();
      seg7DigitUpdate(display.m1, display.m2, display.s1, display.s2); // call stack: stopWatchUpdate->seg7DigitUpdate->Seg7Update      
   }

   // Call back after 1 second
   schdCallback(displayUpdate, time + 1000);
}


// interface between main.c and IR receiver
/* Remember, we're only going to be entering
 * a sequence of digits when we're setting the
 * timer, that is, when we're in ENTRY mode. Other-
 * wise, we're parsing single digits that map to control
 * functions like pause, set, reset, delete, etc.
 */
void interpretDigit(uint32_t time) {
   uprintf("Getting digit\n\r");
   uint8_t digit = getDecodedDigit();
   uprintf("decoded digit: %d\n\r", digit);
   waitForNextDigit();

   if(Mode == Entry) {
      switch (digit) {
      case(MODE_SWITCH):
         //maybe makes more sense than only being able to
         //enter stopwatch mode for timer mode?
         Mode = Stopwatch;
         resetStopwatch();
         display.state = Run;
         break;
      case(OKAY):
         //OKAY => start timer with given sequence
         Mode = Timer;
         getDisplayStateFromDigits();
         display.state = Run;
         break;
      case(DELETE):
         //delete digit if possible
         deleteDigit();
         seg7DigitUpdate(Digits[0], Digits[1], Digits[2], Digits[3]);
         break;
      case(ERROR):
         break;
         //todo: handle error some other way?
      default://digits 0-9
         //update digit sequence with entered digit
         updateDigits(digit);
         seg7DigitUpdate(Digits[0], Digits[1], Digits[2], Digits[3]);
      }
   }
   else { //if we are in timer or stopwatch mode
      if(display.state == Pause) { //timer is paused
         if(digit == OKAY) {
            display.state = Run;
         }
         if(digit == MODE_SWITCH) {
            //reset display, switch back to entry
            resetDigits();
            seg7DigitUpdate(BLANK, BLANK, BLANK, BLANK);
            Mode = Entry;
         }
      }
      else { //timer is running currently
         if(digit == OKAY) {
            display.state = Pause;
         }
      }
   }

   uint32_t delay = 1000;
   schdCallback(interpretDigit, time + delay);
}

int main(void)
{
   //initialize all of our devices
   lpInit();
   seg7Init();
   buzzInit();
   infraredInit();

   uprintf("%s\n\r", "here we go");

   schdCallback(interpretDigit, 1000);
   schdCallback(displayUpdate, 1005);
   while(true) {
      schdExecute();
   }
}
