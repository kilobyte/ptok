ALL=test 1corr th
OBJ=cuckoo.o util.o out.o cuckoo_mutex.o cuckoo_rwlock.o hmload.o \
	tcradix_8.o tcradix_11.o tcradix_13.o tcradix_16.o \
	tcradix_4.o tcradix_5.o tcradix_6.o tcradix_7.o \
	tcradix-mutex_8.o tcradix-mutex_11.o tcradix-mutex_13.o tcradix-mutex_16.o \
	tcradix-mutex_4.o tcradix-mutex_5.o tcradix-mutex_6.o tcradix-mutex_7.o \
	radix_8.o radix_11.o radix_13.o radix_16.o \
	radix_4.o radix_5.o radix_6.o radix_7.o \
	tlog.o critbit.o critnib.o tcradix-atcount.o \

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
	rm -f $(ALL) *.o tcradix_*.c tcradix-mutex_*.c radix_*.c

test: test.o $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

1corr: 1corr.o $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

th: th.o $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

tcradix_4.c: tcradix.c
	sed s/SLICE/4/g <$< >$@
tcradix_5.c: tcradix.c
	sed s/SLICE/5/g <$< >$@
tcradix_6.c: tcradix.c
	sed s/SLICE/6/g <$< >$@
tcradix_7.c: tcradix.c
	sed s/SLICE/7/g <$< >$@
tcradix_8.c: tcradix.c
	sed s/SLICE/8/g <$< >$@
tcradix_11.c: tcradix.c
	sed s/SLICE/11/g <$< >$@
tcradix_13.c: tcradix.c
	sed s/SLICE/13/g <$< >$@
tcradix_16.c: tcradix.c
	sed s/SLICE/16/g <$< >$@
tcradix-mutex_4.c: tcradix-mutex.c
	sed s/SLICE/4/g <$< >$@
tcradix-mutex_5.c: tcradix-mutex.c
	sed s/SLICE/5/g <$< >$@
tcradix-mutex_6.c: tcradix-mutex.c
	sed s/SLICE/6/g <$< >$@
tcradix-mutex_7.c: tcradix-mutex.c
	sed s/SLICE/7/g <$< >$@
tcradix-mutex_8.c: tcradix-mutex.c
	sed s/SLICE/8/g <$< >$@
tcradix-mutex_11.c: tcradix-mutex.c
	sed s/SLICE/11/g <$< >$@
tcradix-mutex_13.c: tcradix-mutex.c
	sed s/SLICE/13/g <$< >$@
tcradix-mutex_16.c: tcradix-mutex.c
	sed s/SLICE/16/g <$< >$@
radix_4.c: radix.c
	sed s/SLICE/4/g <$< >$@
radix_5.c: radix.c
	sed s/SLICE/5/g <$< >$@
radix_6.c: radix.c
	sed s/SLICE/6/g <$< >$@
radix_7.c: radix.c
	sed s/SLICE/7/g <$< >$@
radix_8.c: radix.c
	sed s/SLICE/8/g <$< >$@
radix_11.c: radix.c
	sed s/SLICE/11/g <$< >$@
radix_13.c: radix.c
	sed s/SLICE/13/g <$< >$@
radix_16.c: radix.c
	sed s/SLICE/16/g <$< >$@
