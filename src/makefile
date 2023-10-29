# makefile for sds011. Janaury 2019 / paulvha

CC = gcc
DEPS = sds011_lib.h serial.h
OBJ = sds.o serial.o sds011_lib.o
LIBS = -lm

.cpp.o: %c $(DEPS)
	$(CC) -Wall -Werror -c -o $@ $<

sds : $(OBJ)
	$(CC) -o $@ $^ $(LIBS)

.PHONY : clean

clean :
	rm sds $(OBJ)
