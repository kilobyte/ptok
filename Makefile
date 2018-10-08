ALL=test 1corr
all: $(ALL)

.c.o:
	gcc -Wall -c $^

clean:
	rm -f $(ALL) *.o

test: test.o cuckoo.o util.o out.o
	gcc -Wall -o $@ $^

1corr: 1corr.o cuckoo.o util.o out.o
	gcc -Wall -o $@ $^
