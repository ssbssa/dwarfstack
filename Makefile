
DWST_VERSION = 2.1-git

SRC_DIR := $(dir $(lastword $(MAKEFILE_LIST)))

HOSTPREFIX =
CC = $(HOSTPREFIX)gcc
AR = $(HOSTPREFIX)ar
CPPFLAGS = -DNO_DBGHELP
OPT = -O3
FRAME_POINTER = -fno-omit-frame-pointer -fno-optimize-sibling-calls
INCLUDE = -I$(SRC_DIR)libdwarf -I$(SRC_DIR)mgwhelp -I$(SRC_DIR)include
WARN = -Wall -Wextra -Wno-implicit-fallthrough
CFLAGS = $(CPPFLAGS) $(OPT) $(WARN) $(FRAME_POINTER) $(INCLUDE)
CFLAGS_STATIC = $(CFLAGS) -DDWST_STATIC
CFLAGS_SHARED = $(CFLAGS) -DDWST_SHARED
CFLAGS_LIBDWARF = $(CFLAGS) -DDW_TSHASHTYPE=uintptr_t \
		  -I$(SRC_DIR)zlib \
		  -Wno-unused \
		  -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast


# libdwarf
DWARF_SRC_REL = dwarf_alloc.c \
		dwarf_die_deliv.c \
		dwarf_dnames.c \
		dwarf_dsc.c \
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


# zlib
ZLIB_SRC_REL = adler32.c \
	       crc32.c \
	       inffast.c \
	       inflate.c \
	       inftrees.c \
	       uncompr.c \
	       zutil.c \

ZLIB_SRC = $(patsubst %,zlib/%,$(ZLIB_SRC_REL))
ZLIB_OBJ = $(patsubst %.c,%.o,$(ZLIB_SRC))


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


src libdwarf mgwhelp zlib include lib bin:
	@mkdir -p $@


ifneq ($(SRC_DIR),./)

vpath %.c $(SRC_DIR)

$(DWST_OBJ) $(DWST_DLL_OBJ) : | src
$(DWARF_OBJ) : | libdwarf
$(DWARF_PE_OBJ) : | mgwhelp
$(ZLIB_OBJ) : | zlib

$(INC): $(SRC_DIR)$(INC) | include
	cp -fp $< $@

endif


share/licenses/dwarfstack/LICENSE.txt: $(SRC_DIR)LICENSE.txt
	@mkdir -p share/licenses/dwarfstack
	cp -fp $< $@


# libdwarf

$(DWARF_OBJ) $(DWARF_PE_OBJ): %.o: %.c
	$(CC) -c $(CFLAGS_LIBDWARF) -o $@ $<


# static library
.c.o:
	$(CC) -c $(CFLAGS_STATIC) -o $@ $<

lib/libdwarfstack.a: $(DWARF_OBJ) $(ZLIB_OBJ) $(DWARF_PE_OBJ) $(DWST_OBJ) | lib
	$(AR) rcs $@ $(DWARF_OBJ) $(ZLIB_OBJ) $(DWARF_PE_OBJ) $(DWST_OBJ)


# shared library
.c.dll.o:
	$(CC) -c $(CFLAGS_SHARED) -o $@ $<

bin/dwarfstack.dll: $(DWARF_OBJ) $(ZLIB_OBJ) $(DWARF_PE_OBJ) $(DWST_DLL_OBJ) | lib bin
	$(CC) -s -shared -o $@ $(DWARF_OBJ) $(ZLIB_OBJ) $(DWARF_PE_OBJ) $(DWST_DLL_OBJ) -ldbghelp -Wl,--out-implib,lib/libdwarfstack.dll.a -lgdi32 -lstdc++

lib/libdwarfstack.dll.a: bin/dwarfstack.dll


clean:
	rm -rf $(DWARF_OBJ) $(ZLIB_OBJ) $(DWARF_PE_OBJ) $(DWST_OBJ) $(DWST_DLL_OBJ) lib bin


# install
PREFIX = /usr/local

install: $(BUILD)
	@mkdir -p $(PREFIX)/include $(PREFIX)/lib $(PREFIX)/bin
	cp -f $(INC) $(PREFIX)/include
	cp -f $(LIB) $(PREFIX)/lib
	cp -f $(BIN) $(PREFIX)/bin


# builds
build32 build64:
	@mkdir -p $@

build32/bin/dwarfstack.dll: | build32
	$(MAKE) -C build32 -f ../Makefile HOSTPREFIX=i686-w64-mingw32- bin/dwarfstack.dll

build64/bin/dwarfstack.dll: | build64
	$(MAKE) -C build64 -f ../Makefile HOSTPREFIX=x86_64-w64-mingw32- bin/dwarfstack.dll

build32/dwarfstack-$(DWST_VERSION)-mingw.tar.xz: build32/bin/dwarfstack.dll
	$(MAKE) -C build32 -f ../Makefile HOSTPREFIX=i686-w64-mingw32- package

build64/dwarfstack-$(DWST_VERSION)-mingw.tar.xz: build64/bin/dwarfstack.dll
	$(MAKE) -C build64 -f ../Makefile HOSTPREFIX=x86_64-w64-mingw32- package

dwarfstack%.dll: build%/bin/dwarfstack.dll
	cp -fp $< $@


# package
packages: package-src package32 package64 package-dlls

dwarfstack-$(DWST_VERSION)-mingw.tar.xz: $(BUILD) share/licenses/dwarfstack/LICENSE.txt
	@rm -f $@
	tar -cJf $@ $^

dwarfstack-$(DWST_VERSION).tar.xz:
	@rm -f $@
	CURDIR=`pwd`; cd $(SRC_DIR) && git archive "HEAD^{tree}" |xz >$$CURDIR/$@

dwarfstack-$(DWST_VERSION)-mingw%.tar.xz: build%/dwarfstack-$(DWST_VERSION)-mingw.tar.xz
	cp -fp $< $@

dwarfstack-$(DWST_VERSION)-dlls.7z: LICENSE.txt dwarfstack32.dll dwarfstack64.dll
	@rm -f $@
	7z a -mx=9 $@ $^

package: dwarfstack-$(DWST_VERSION)-mingw.tar.xz

package-src: dwarfstack-$(DWST_VERSION).tar.xz

package32: dwarfstack-$(DWST_VERSION)-mingw32.tar.xz

package64: dwarfstack-$(DWST_VERSION)-mingw64.tar.xz

package-dlls: dwarfstack-$(DWST_VERSION)-dlls.7z
