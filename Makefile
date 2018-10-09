ALL=test 1corr th
OBJ=cuckoo.o util.o out.o cuckoo_mutex.o hmload.o \
	tcradix_8.o tcradix_11.o tcradix_13.o tcradix_16.o \
	radix_8.o radix_11.o radix_13.o radix_16.o \

CC=gcc
CFLAGS=-Wall -g -O3 -pthread

all: $(ALL)

.c.o:
	$(CC) $(CFLAGS) -c $<

*.o:	hmproto.h

clean:
	rm -f $(ALL) *.o tcradix_*.c radix_*.c

test: test.o $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

1corr: 1corr.o $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

th: th.o $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

tcradix_8.c: tcradix.c
	sed s/SLICE/8/g <$< >$@
tcradix_11.c: tcradix.c
	sed s/SLICE/11/g <$< >$@
tcradix_13.c: tcradix.c
	sed s/SLICE/13/g <$< >$@
tcradix_16.c: tcradix.c
	sed s/SLICE/16/g <$< >$@
radix_8.c: radix.c
	sed s/SLICE/8/g <$< >$@
radix_11.c: radix.c
	sed s/SLICE/11/g <$< >$@
radix_13.c: radix.c
	sed s/SLICE/13/g <$< >$@
radix_16.c: radix.c
	sed s/SLICE/16/g <$< >$@
