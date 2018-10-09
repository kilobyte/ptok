ALL=test 1corr th
OBJ=cuckoo.o util.o out.o cuckoo_mutex.o hmload.o tcradix_8.o tcradix_11.o tcradix_13.o tcradix_16.o
CFLAGS=-Wall -g -O3 -pthread

all: $(ALL)

.c.o:
	gcc $(CFLAGS) -c $<

*.o:	hmproto.h

clean:
	rm -f $(ALL) *.o tcradix_*.c

test: test.o $(OBJ)
	gcc $(CFLAGS) -o $@ $^

1corr: 1corr.o $(OBJ)
	gcc $(CFLAGS) -o $@ $^

th: th.o $(OBJ)
	gcc $(CFLAGS) -o $@ $^

tcradix_8.c: tcradix.c
	sed s/SLICE/8/g <$< >$@
tcradix_11.c: tcradix.c
	sed s/SLICE/11/g <$< >$@
tcradix_13.c: tcradix.c
	sed s/SLICE/13/g <$< >$@
tcradix_16.c: tcradix.c
	sed s/SLICE/16/g <$< >$@
