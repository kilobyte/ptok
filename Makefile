ALL=test 1corr th
OBJ=util.o out.o hmload.o \
	tlog.o critbit.o critnib.o tcradix.o critnib-atcount.o \

CC=gcc
CFLAGS=-Wall -g -O3 -pthread

ifdef M
CFLAGS+=-DTRACEMEM
endif

all: $(ALL)

.c.o:
	$(CC) $(CFLAGS) -c $<

*.o:	hmproto.h

clean:
	rm -f $(ALL) *.o

test: test.o $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

1corr: 1corr.o $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

th: th.o $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^
