CC=g++
CFLAGS=-O2 -std=c++11 -Wall

all: scross serialtest

scross:
	$(CC) $(CFLAGS) -o scross simple_cross.cpp serializer.cpp engine.cpp book.cpp

serialtest:
	$(CC) $(CFLAGS) -o serialtest serializer.cpp test/test_serializer.cpp

clean:
	rm *.o scross serialtest


