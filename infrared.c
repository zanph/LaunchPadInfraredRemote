#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h> //strcmp
#include <inc/hw_memmap.h>
#include <inc/hw_ints.h>
#include <driverlib/gpio.h>
#include <driverlib/pin_map.h>
#include <driverlib/sysctl.h>
#include <driverlib/interrupt.h>
#include <driverlib/systick.h>
#include <driverlib/timer.h>
#include <launchpad.h>
#include "infrared.h"



//**Define macros here**//
#define MAX_CLOCK_COUNT           4294967296 //2^32
#define PRESCALER                 999
//effective clock = 80Mhz / (1 + prescale)
#define PRESCALED_CLOCK_RATE      80000000/(PRESCALER + 1)
//set timeout of ~50ms of consecutive high output from IR sensor
// = 50ms * (1s / 1000ms) * 80M cycles/sec = X cycles
#define IR_OUTPUT_HIGH_TIMEOUT_MS 50 * (PRESCALED_CLOCK_RATE / 1000)
//empirically, the encodings are 34 digits long.
#define ENCODING_LENGTH           34
//chosen arbitrarily
#define ERROR_DIGIT               99
//will be used once we encode the mode switch button
#define NUMBER_OF_BUTTONS         13


static volatile uint32_t PreviousEdge    = 0;
static volatile char     Encoding        [ENCODING_LENGTH + 1];
static volatile uint8_t  Counter         = 0;
static volatile uint8_t  CurrentDigit    = ERROR_DIGIT;
static volatile uint8_t  DigitIsReady    = 0;
//initially we are in capture mode
static volatile uint8_t  Capture         = 1;

const char * DigitEncodings[NUMBER_OF_BUTTONS] = {
   /*0*/          "XXAABAAAAABBABBBBBAAAABAAABBBBABBB",
   /*1*/          "XXAABAAAAABBABBBBBBAAABAAAABBBABBB",
   /*2*/          "XXAABAAAAABBABBBBBABAABAAABABBABBB",
   /*3*/          "XXAABAAAAABBABBBBBBBAABAAAAABBABBB",
   /*4*/          "XXAABAAAAABBABBBBBAABABAAABBABABBB",
   /*5*/          "XXAABAAAAABBABBBBBBABABAAAABABABBB",
   /*6*/          "XXAABAAAAABBABBBBBABBABAAABAABABBB",
   /*7*/          "XXAABAAAAABBABBBBBBBBABAAAAAABABBB",
   /*8*/          "XXAABAAAAABBABBBBBAAABBAAABBBAABBB",
   /*9*/          "XXAABAAAAABBABBBBBBAABBAAAABBAABBB",
   /*OK*/         "XXAABAAAAABBABBBBBAABAAABABBABBBAB",
   /*BACK*/       "XXAABAAAAABBABBBBBAAABABAABBBABABB",
   /*MODE SWITCH*/"XXAABAAAAABBABBBBBBAAAAABAABBBBBAB"
};


void waitForNextDigit() {
   Capture = 1;
   DigitIsReady = 0;
}

uint8_t getDigitReadyStatus() {
	return DigitIsReady;
}

uint8_t getCurrentDigit() {
	return CurrentDigit;
}

uint32_t clockDelta(uint32_t start, uint32_t end) {
   if(end < start) {
      return ((MAX_CLOCK_COUNT - end) + (MAX_CLOCK_COUNT - start));
      // this will theoretically overflow if start + end < MAX_CLOCK_COUNT,
      // which will only happen if between start and end the clock wraps around
      // these two events are highly unlikely to happen at the same time,
      // especially using the prescaler.
   }
   return end - start;
}

void DecodeDigit() {
   uint8_t i = 0;
   //uncomment for debugging
   //uprintf("Encoded: %s\n\r", Encoding);
   for(; i < NUMBER_OF_BUTTONS; ++i) {
      //        uprintf("Encoded: %s\n", Encoding);
      //uprintf("Table Value: %s", DigitEncodings[i]);
      if(strcmp((const char *)Encoding, DigitEncodings[i]) == 0) {
         //if we match the decoded value to a value in our array,
         //update the current status of CurrentDigit to reflect this
         CurrentDigit = i;
         return;
      }
   }
   //uprintf("We decoded as: %d\n\r", CurrentDigit);
   //if our Encoding doesn't match anything in the table, update the state
   //of CurrentDigit to reflect this
   CurrentDigit = ERROR_DIGIT;
}
// GPT input capture interrupt handler
void getAllEdgeTimes() {
   //it's apparently important to clear the interrupt as soon as possible
   TimerIntClear(WTIMER3_BASE, TIMER_CAPA_EVENT);

   if(!Capture) {
      return;
   }
   uint32_t edgeTime = TimerValueGet(WTIMER3_BASE, TIMER_A);
   uint32_t delta = (clockDelta(PreviousEdge, edgeTime))/80;

   if(delta <= 750) {
      Encoding[Counter] = 'A';
   }
   else if (delta <= 1450) {
      Encoding[Counter] = 'B';
   }
   else {
      Encoding[Counter] = 'X';
   }
   PreviousEdge = edgeTime;

   if (Counter == ENCODING_LENGTH - 1) {
      //update the status of CurrentDigit
      DecodeDigit();
      Counter = 0;
      DigitIsReady = 1;
      Capture = 0;
      return;
   }

   ++Counter;
}

void timerConfig() {
   //configure timer for input capture on Timer A of WTIMER3
   TimerConfigure(WTIMER3_BASE, TIMER_CFG_SPLIT_PAIR | TIMER_CFG_A_CAP_TIME_UP);

   //set prescaler for the timer
   TimerPrescaleSet(WTIMER3_BASE, TIMER_A, PRESCALER);

   //setup timer to wait for falling edge capture
   TimerControlEvent(WTIMER3_BASE,
                     TIMER_A,             //WT3CCP0 = timer A
                     TIMER_EVENT_NEG_EDGE);  //TIMER_EVENT_POS_EDGE, or
   //TIMER_EVENT_NEG_EDGE, TIMER_EVENT_BOTH_EDGES

   //this interrupt will generate Encoding for a given signal
   TimerIntRegister(WTIMER3_BASE, TIMER_A, getAllEdgeTimes); //register handler

   TimerIntEnable(WTIMER3_BASE, TIMER_CAPA_EVENT); //enable interrupt
   TimerEnable(WTIMER3_BASE, TIMER_A); //enable timer

}

void infraredInit() {
   //We're using J7 for our ranger, which corresponds to PD2 (which
   //corresponds to WTIMER3) per table 11-2 in datasheet

   //init timer
   SysCtlPeripheralEnable(SYSCTL_PERIPH_WTIMER3);

   //setup port of gpio pin that can be routed to a timer input
   SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOD);

   GPIOPinTypeTimer(GPIO_PORTD_BASE, GPIO_PIN_2);
   GPIOPinConfigure(GPIO_PD2_WT3CCP0); //

   timerConfig();
}
