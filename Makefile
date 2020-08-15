#

TARGET = eosend

CC = gcc
IFLAGS = 
CFLAGS = -g -Wall -Wno-parentheses -Wno-unused-function -Wno-format-overflow $(IFLAGS)

all: $(TARGET)
	touch make.date

eosend: eosend.c esp3.c serial.c utils.c
	$(CC) $(CFLAGS) eosend.c esp3.c serial.c utils.c -o eosend -lpthread

clean:
	/bin/rm -f core *.o *~ $(TARGET) make.date
