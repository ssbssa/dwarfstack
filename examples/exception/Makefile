
CC = gcc
CFLAGS = -O3 -g -Wall -Wextra -I../../include


exception.exe: exception.c crasher.c ../../lib/libdwarfstack.dll.a
	$(CC) $(CFLAGS) -o $@ $^

../../lib/libdwarfstack.dll.a:
	$(MAKE) -C ../.. lib/libdwarfstack.dll.a


clean:
	rm -f exception.exe
