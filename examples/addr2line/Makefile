
CC = gcc
OPT = -O3
CFLAGS = $(OPT) -Wall -Wextra -I../../include -DDWST_STATIC


a2l.exe: a2l.c ../../lib/libdwarfstack.a
	$(CC) $(CFLAGS) -municode -s -o $@ $^

../../lib/libdwarfstack.a:
	$(MAKE) -C ../.. lib/libdwarfstack.a


clean:
	rm -f a2l.exe
