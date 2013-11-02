
CC = gcc
OPT = -O3
FRAME_POINTER = -fno-omit-frame-pointer -fno-optimize-sibling-calls
CFLAGS = $(OPT) -g -Wall -Wextra -I../../include $(FRAME_POINTER)


location.exe: location.c obj2.c ../../lib/libdwarfstack.dll.a
	$(CC) $(CFLAGS) -o $@ $^

../../lib/libdwarfstack.dll.a:
	$(MAKE) -C ../.. lib/libdwarfstack.dll.a


clean:
	rm -f location.exe
