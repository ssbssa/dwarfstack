
include inc/leaks.mk

CC = gcc
CXX = g++
OPT = -O3 -g
CFLAGS = $(OPT) $(LEAKS_CFLAGS)
LDFLAGS = $(LEAKS_LDFLAGS)

all: leaks-c.exe leaks-cxx.exe

leaks-c.exe: main-c.c leaks-c.c leaking-c.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

leaks-cxx.exe: main-cxx.cpp leaks-cxx.cpp leaking-c.c leaking-cxx.cpp
	$(CXX) $(CFLAGS) $(LDFLAGS) -o $@ $^


clean:
	rm -f leaks-c.exe leaks-cxx.exe
