
DWST_VERSION = 1.3-git

CC = gcc
CPPFLAGS = -DNO_DBGHELP
OPT = -O3
FRAME_POINTER = -fno-omit-frame-pointer -fno-optimize-sibling-calls
INCLUDE = -Ilibdwarf -Imgwhelp -Iinclude
WARN = -Wall -Wextra
CFLAGS = $(CPPFLAGS) $(OPT) $(WARN) $(FRAME_POINTER) $(INCLUDE)
CFLAGS_STATIC = $(CFLAGS) -DDWST_STATIC
CFLAGS_SHARED = $(CFLAGS) -DDWST_SHARED
CFLAGS_LIBDWARF = $(CFLAGS) -DDW_TSHASHTYPE=uintptr_t \
		  -Wno-unused \
		  -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast


# libdwarf
DWARF_SRC_REL = dwarf_alloc.c \
		dwarf_die_deliv.c \
		dwarf_error.c \
		dwarf_form.c \
		dwarf_frame.c \
		dwarf_frame2.c \
		dwarf_global.c \
		dwarf_harmless.c \
		dwarf_init_finish.c \
		dwarf_leb.c \
		dwarf_line.c \
		dwarf_macro5.c \
		dwarf_query.c \
		dwarf_ranges.c \
		dwarf_tied.c \
		dwarf_tsearchhash.c \
		dwarf_util.c \
		dwarf_xu_index.c \
		pro_encode_nm.c \

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
LIB = lib/libdwarfstack.a lib/libdwarfstack.dll.a
BIN = bin/dwarfstack.dll
BUILD = $(LIB) $(BIN) $(INC)

all: $(BUILD)

.SUFFIXES: .c .o .dll.o


# libdwarf

$(DWARF_OBJ) $(DWARF_PE_OBJ): %.o: %.c
	$(CC) -c $(CFLAGS_LIBDWARF) -o $@ $<


# static library
.c.o:
	$(CC) -c $(CFLAGS_STATIC) -o $@ $<

lib/libdwarfstack.a: $(DWARF_OBJ) $(DWARF_PE_OBJ) $(DWST_OBJ) | lib
	ar rcs $@ $(DWARF_OBJ) $(DWARF_PE_OBJ) $(DWST_OBJ)


# shared library
.c.dll.o:
	$(CC) -c $(CFLAGS_SHARED) -o $@ $<

bin/dwarfstack.dll: $(DWARF_OBJ) $(DWARF_PE_OBJ) $(DWST_DLL_OBJ) | lib bin
	$(CC) -s -shared -o $@ $(DWARF_OBJ) $(DWARF_PE_OBJ) $(DWST_DLL_OBJ) -ldbghelp -Wl,--out-implib,lib/libdwarfstack.dll.a -lgdi32 -lstdc++

lib/libdwarfstack.dll.a: bin/dwarfstack.dll


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
package: $(BUILD) LICENSE.txt
	tar -cJf dwarfstack-$(DWST_VERSION)-mingw.tar.xz $^

package-src:
	git archive "HEAD^{tree}" |xz >dwarfstack-$(DWST_VERSION).tar.xz
