CC=g++
CFLAGS=-O2 -std=c++11 -Wall -o scross 

all: scross

scross:
	$(CC) $(CFLAGS) simple_cross.cpp serializer.cpp engine.cpp book.cpp

clean:
	rm *.o scross


