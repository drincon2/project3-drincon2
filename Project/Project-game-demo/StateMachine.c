#include <msp430.h>
#include "buzzer.h"

int currentState = 0;

void advanceState() {
    switch(currentState) {
        case 0:
            buzzerSetPeriod(2500);
            currentState++;
            break;
        case 1: 
            buzzerSetPeriod(5000);
            currentState++;
            break;
        default:
            buzzerSetPeriod(1000);
            currentState = 0;
    }
}
