import qbs

DynamicLibrary {
    name: "dwarfstack"
    targetName: "dwarfstack" + (qbs.architecture === "x86_64" ? '64' : '32')
    Depends { name: "cpp" }
    Export {
        Depends { name: "cpp" }
        cpp.includePaths: "include"
    }
    Group {
        fileTagsFilter: product.type
        qbs.install: true
    }

    files: [
        "src/*.c",
        "mgwhelp/dwarf_pe.c",
    ]
    cpp.windowsApiCharacterSet: "mbcs"
    cpp.cFlags: [
        "-Wno-unused",
        "-Wno-pointer-to-int-cast",
        "-Wno-int-to-pointer-cast",
    ]
    cpp.defines: [
        "DWST_SHARED",
        "DW_TSHASHTYPE=uintptr_t",
    ]
    cpp.dynamicLibraries: [
        "dbghelp",
        "gdi32",
        "stdc++",
    ]
    cpp.includePaths: [
        "include",
        "mgwhelp",
        "libdwarf",
        "zlib",
    ]
    Group {
        name: "libdwarf"
        prefix: "libdwarf/"
        files: [
            "dwarf_*.c",
            "pro_encode_nm.c",
        ]
        excludeFiles: [
            "dwarf_errmsg_list.c",
            "dwarf_line_table_reader_common.c",
        ]
    }

    Group {
        name: "zlib"
        files: "zlib/*.c"
    }
}
