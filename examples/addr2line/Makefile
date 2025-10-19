
CC = gcc
OPT = -O3
LDFLAGS = -s
CFLAGS = $(OPT) -Wall -Wextra -I../../include -DDWST_STATIC


a2l.exe: a2l.c ../../lib/libdwarfstack.a
	$(CC) $(CFLAGS) -municode $(LDFLAGS) -o $@ $^

../../lib/libdwarfstack.a:
	$(MAKE) -C ../.. lib/libdwarfstack.a


clean:
	rm -f a2l.exe
