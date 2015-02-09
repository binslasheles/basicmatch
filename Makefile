CC=g++
CFLAGS=-O2 -std=c++11 -Wall

all: scross stest

scross:
	$(CC) $(CFLAGS) -o scross simple_cross.cpp serializer.cpp engine.cpp book.cpp

stest:
	$(CC) $(CFLAGS) -o stest serializer.cpp test/test_serializer.cpp

clean:
	rm *.o scross serialtest


