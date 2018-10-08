ALL=test 1corr th
all: $(ALL)

.c.o:
	gcc -Wall -c $^

clean:
	rm -f $(ALL) *.o

test: test.o cuckoo.o util.o out.o
	gcc -Wall -o $@ $^

1corr: 1corr.o cuckoo.o util.o out.o
	gcc -Wall -o $@ $^

th: th.o cuckoo.o util.o out.o
	gcc -Wall -pthread -o $@ $^
