#include <msp430.h>
#include "libTimer.h"
#include "buzzer.h"

void buzzerInit() {
    timerAUpmode();
    P2SEL2 &= ~(BIT6 | BIT7);
    P2SEL &= ~BIT7;
    P2SEL |= BIT6;
    P2DIR = BIT6;
}

void buzzerSetPeriod(short cycles) {
  CCR0 = cycles; 
  CCR1 = cycles >> 1;
}
