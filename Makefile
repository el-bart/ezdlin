CC=gcc

FLAGS_BASIC=-Wall
CFLAGS=$(FLAGS_BASIC) -O3 -DNDEBUG
#CFLAGS=$(FLAGS_BASIC) -g3
LIBS_BASIC=-lm
CLIBS=$(LIBS_BASIC)
#CLIBS=$(LIBS_BASIC) -lefence

APP_NAME=ezdlin

.PHONY: all
all: $(APP_NAME)

$(APP_NAME): main.o zlprg.o
	$(CC) $(CFLAGS) -o $(APP_NAME) main.o zlprg.o $(CLIBS)

main.o: main.c zlprg.h
zlprg.o: zlprg.c zlprg.h

clean:
	rm -f *.o
	rm -f $(APP_NAME)
