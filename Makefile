ALL=test 1corr th
OBJ=util.o out.o hmload.o os_thread_posix.o \
	tlog.o critbit.o tcradix.o critnib.o critnib-tag.o \

CC=gcc
CFLAGS=-Wall -g -O3 -pthread

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
