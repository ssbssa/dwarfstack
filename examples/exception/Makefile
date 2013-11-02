
CC = gcc
OPT = -O3
CFLAGS = $(OPT) -g -Wall -Wextra -I../../include -fno-omit-frame-pointer -fno-optimize-sibling-calls


exception.exe: exception.c crasher.c ../../lib/libdwarfstack.dll.a
	$(CC) $(CFLAGS) -o $@ $^

../../lib/libdwarfstack.dll.a:
	$(MAKE) -C ../.. lib/libdwarfstack.dll.a


clean:
	rm -f exception.exe
