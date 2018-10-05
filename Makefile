all: test

test: test.o cuckoo.o util.o out.o
	gcc -Wall -o $@ $^
