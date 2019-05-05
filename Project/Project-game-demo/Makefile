# makfile configuration
CPU             	= msp430g2553
CFLAGS          	= -mmcu=${CPU} -Os -I../h
LDFLAGS		= -L../lib -L/opt/ti/msp430_gcc/include/ 

#switch the compiler (for the internal make rules)
CC              = msp430-elf-gcc
AS              = msp430-elf-gcc -mmcu=${CPU} -c

all:game.elf

#additional rules for files
game.elf: ${COMMON_OBJECTS} game.o wdt_handler.o ship.o buzzer.o StateMachine.o
	${CC} ${CFLAGS} ${LDFLAGS} -o $@ $^ -lTimer -lLcd -lShape -lCircle -lp2sw

load: game.elf
	mspdebug rf2500 "prog $^"
	
StateMachine.o: StateMachine.s

clean:
	rm -f *.o *.elf
