cmake_minimum_required(VERSION 4.1.1)

project(dwarfstack VERSION 2.3.0 LANGUAGES C CXX)

set(DWST_VERSION "2.3-git")
set(DWST_VER_NUM 2,3,0,99)
set(DWST_PRERELEASE 1)
set(DWST_COPYRIGHT_YEARS "2013-2025")

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(TARGET_NAME dwarfstack64)
else()
    set(TARGET_NAME dwarfstack32)
endif()

add_library(${TARGET_NAME} SHARED
    src/dwst-exception-dialog.c
    src/dwst-exception.c
    src/dwst-file.c
    src/dwst-location.c
    src/dwst-process.c
    mgwhelp/dwarf_pe.c
    dwarfstack-ver.rc

    libdwarf/dwarf_abbrev.c
    libdwarf/dwarf_alloc.c
    libdwarf/dwarf_debuglink.c
    libdwarf/dwarf_debugnames.c
    libdwarf/dwarf_die_deliv.c
    libdwarf/dwarf_dsc.c
    libdwarf/dwarf_error.c
    libdwarf/dwarf_find_sigref.c
    libdwarf/dwarf_fission_to_cu.c
    libdwarf/dwarf_form.c
    libdwarf/dwarf_frame.c
    libdwarf/dwarf_frame2.c
    libdwarf/dwarf_global.c
    libdwarf/dwarf_gnu_index.c
    libdwarf/dwarf_groups.c
    libdwarf/dwarf_harmless.c
    libdwarf/dwarf_init_finish.c
    libdwarf/dwarf_leb.c
    libdwarf/dwarf_line.c
    libdwarf/dwarf_loc.c
    libdwarf/dwarf_locationop_read.c
    libdwarf/dwarf_loclists.c
    libdwarf/dwarf_macro5.c
    libdwarf/dwarf_memcpy_swap.c
    libdwarf/dwarf_names.c
    libdwarf/dwarf_query.c
    libdwarf/dwarf_ranges.c
    libdwarf/dwarf_rnglists.c
    libdwarf/dwarf_string.c
    libdwarf/dwarf_str_offsets.c
    libdwarf/dwarf_tied.c
    libdwarf/dwarf_tsearchhash.c
    libdwarf/dwarf_util.c
    libdwarf/dwarf_xu_index.c

    zlib/adler32.c
    zlib/crc32.c
    zlib/inffast.c
    zlib/inflate.c
    zlib/inftrees.c
    zlib/uncompr.c
    zlib/zutil.c
)

set_source_files_properties(dwarfstack-ver.rc PROPERTIES
    COMPILE_DEFINITIONS "DWST_VER_STR=\\\"${DWST_VERSION}\\\";DWST_VER_NUM=${DWST_VER_NUM};DWST_PRERELEASE=${DWST_PRERELEASE};DWST_COPYRIGHT_YEARS=\\\"${DWST_COPYRIGHT_YEARS}\\\""
)

target_compile_options(${TARGET_NAME} PRIVATE
    -Wno-unused
    -Wno-pointer-to-int-cast
    -Wno-int-to-pointer-cast
)

target_compile_definitions(${TARGET_NAME} PRIVATE
    DWST_SHARED
    DW_TSHASHTYPE=uintptr_t
    LIBDWARF_STATIC
)

target_link_libraries(${TARGET_NAME} PRIVATE
    dbghelp
    gdi32
    stdc++
)

target_include_directories(${TARGET_NAME} PRIVATE
    mgwhelp
    libdwarf
    zlib
    PUBLIC
    include
)
