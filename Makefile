ALL=test 1corr th
OBJ=cuckoo.o util.o out.o cuckoo_mutex.o tcradix.o
CFLAGS=-Wall -g -O3 -pthread

all: $(ALL)

.c.o:
	gcc $(CFLAGS) -c $<

*.o:	hmproto.h

clean:
	rm -f $(ALL) *.o

test: test.o $(OBJ)
	gcc $(CFLAGS) -o $@ $^

1corr: 1corr.o $(OBJ)
	gcc $(CFLAGS) -o $@ $^

th: th.o $(OBJ)
	gcc $(CFLAGS) -o $@ $^
