#ifndef buzzer_included
#define buzzer_included

void buzzerInit();
void buzzerSetPeriod(short cycles);
void advanceState();
int currentState;

#endif
