
CC=gcc

FLAGS_BASIC=-Wall 
CFLAGS=$(FLAGS_BASIC) -g
#CFLAGS=$(FLAGS_BASIC) -O3

LIBS_BASIC=-lm
#CLIBS=$(LIBS_BASIC) -lefence
CLIBS=$(LIBS_BASIC)

BIN_NAME=ezdlin



all: main.o zlprg.o
	$(CC) $(CFLAGS) -o $(BIN_NAME) main.o zlprg.o $(CLIBS)


clean:
	rm -f *.o
	rm -f $(BIN_NAME)


main.o: main.c zlprg.h

zlprg.o: zlprg.c zlprg.h

