
DWST_VERSION = 0.3-git

CC = gcc
FRAME_POINTER = -fno-omit-frame-pointer -fno-optimize-sibling-calls
INCLUDE = -Ilibdwarf -Imgwhelp -Iinclude
CFLAGS = -O3 -Wall -Wextra $(FRAME_POINTER) $(INCLUDE) -DDWST_MODE
CFLAGS_STATIC = $(CFLAGS) -DDWST_STATIC
CFLAGS_SHARED = $(CFLAGS) -DDWST_SHARED


# libdwarf
DWARF_SRC_REL = dwarf_alloc.c \
		dwarf_die_deliv.c \
		dwarf_error.c \
		dwarf_form.c \
		dwarf_init_finish.c \
		dwarf_leb.c \
		dwarf_line.c \
		dwarf_query.c \
		dwarf_ranges.c \
		dwarf_util.c \

DWARF_SRC = $(patsubst %,libdwarf/%,$(DWARF_SRC_REL))
DWARF_OBJ = $(patsubst %.c,%.o,$(DWARF_SRC))


# mgwhelp
DWARF_PE_SRC_REL = dwarf_pe.c \

DWARF_PE_SRC = $(patsubst %,mgwhelp/%,$(DWARF_PE_SRC_REL))
DWARF_PE_OBJ = $(patsubst %.c,%.o,$(DWARF_PE_SRC))


# dwarfstack
DWST_SRC_REL = dwst-file.c \
	       dwst-process.c \
	       dwst-location.c \
	       dwst-exception.c \
	       dwst-exception-dialog.c \

DWST_HEADER_REL = dwarfstack.h \

DWST_SRC = $(patsubst %,src/%,$(DWST_SRC_REL))
DWST_OBJ = $(patsubst %.c,%.o,$(DWST_SRC))
DWST_DLL_OBJ = $(patsubst %.c,%.dll.o,$(DWST_SRC))


INC = include/dwarfstack.h
LIB = lib/libdwarfstack.a lib/libdwarfstack.dll.a lib/libdbghelp.a
BIN = bin/libdwarfstack.dll
BUILD = $(LIB) $(BIN) $(INC)

all: $(BUILD)

.SUFFIXES: .c .o .dll.o


# static library
.c.o:
	$(CC) -c $(CFLAGS_STATIC) -o $@ $<

lib/libdwarfstack.a: $(DWARF_OBJ) $(DWARF_PE_OBJ) $(DWST_OBJ) | lib
	ar rcs $@ $(DWARF_OBJ) $(DWARF_PE_OBJ) $(DWST_OBJ)


# shared library
.c.dll.o:
	$(CC) -c $(CFLAGS_SHARED) -o $@ $<

lib/libdbghelp.a: src/dbghelp.def | lib
	dlltool -k -d $< -l $@

bin/libdwarfstack.dll: $(DWARF_OBJ) $(DWARF_PE_OBJ) $(DWST_DLL_OBJ) lib/libdbghelp.a | lib bin
	$(CC) -s -shared -o $@ $(DWARF_OBJ) $(DWARF_PE_OBJ) $(DWST_DLL_OBJ) -Llib -ldbghelp -Wl,--out-implib,lib/libdwarfstack.dll.a

lib/libdwarfstack.dll.a: bin/libdwarfstack.dll


lib:
	@mkdir -p lib

bin:
	@mkdir -p bin


clean:
	rm -rf $(DWARF_OBJ) $(DWARF_PE_OBJ) $(DWST_OBJ) $(DWST_DLL_OBJ) lib bin


# install
PREFIX = /usr/local

install: $(BUILD)
	@mkdir -p $(PREFIX)/include $(PREFIX)/lib $(PREFIX)/bin
	cp -f $(INC) $(PREFIX)/include
	cp -f $(LIB) $(PREFIX)/lib
	cp -f $(BIN) $(PREFIX)/bin


# package
package: $(BUILD)
	tar -cJf dwarfstack-$(DWST_VERSION).tar.xz $(BUILD)
