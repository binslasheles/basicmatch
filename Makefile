CC=g++
CFLAGS=-O2 -std=c++11 -Wall

all: bmatch serialtest

bmatch:
	$(CC) $(CFLAGS) -o bmatch basic_match.cpp serializer.cpp engine.cpp book.cpp

serialtest:
	$(CC) $(CFLAGS) -o serialtest serializer.cpp test/test_serializer.cpp

clean:
	rm *.o bmatch serialtest


