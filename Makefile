all: test 1corr

test: test.o cuckoo.o util.o out.o
	gcc -Wall -o $@ $^

1corr: 1corr.o cuckoo.o util.o out.o
	gcc -Wall -o $@ $^
