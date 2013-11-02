
CC = gcc
OPT = -O3
CFLAGS = $(OPT) -g -Wall -Wextra -I../../include -fno-omit-frame-pointer -fno-optimize-sibling-calls

LIBS=../../lib/libdwarfstack.dll.a


exception-dialog.exe: exception-dialog.c crasher.c $(LIBS)
	$(CC) $(CFLAGS) -mwindows -o $@ $^

$(LIBS):
	$(MAKE) -C ../.. $@

clean:
	rm -f exception-dialog.exe
