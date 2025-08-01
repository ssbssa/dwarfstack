/*
 * Copyright 2012 Jose Fonseca
 * Copyright (C) 2013-2025 Hannes Domani
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */


#include <stdlib.h>

#include <windows.h>

#include "config.h"
#include "libdwarf_private.h"
#include "dwarf.h"
#include "libdwarf.h"
#include "dwarf_base_types.h"
#include "dwarf_opaque.h"


wchar_t *
dwst_ansi2wide(const char *str)
{
    if (!str) return NULL;
    int len = MultiByteToWideChar(CP_ACP, 0, str, -1, NULL, 0);
    if (!len) return NULL;
    wchar_t *strW = malloc(2 * len);
    if (!strW) return NULL;
    len = MultiByteToWideChar(CP_ACP, 0, str, -1, strW, len);
    if (!len) strW[0] = 0;
    return strW;
}

char *
dwst_wide2ansi(const wchar_t *str)
{
    if (!str) return NULL;
    int len = WideCharToMultiByte(CP_ACP, 0, str, -1, NULL, 0, NULL, NULL);
    if (!len) return NULL;
    char *strA = malloc(len);
    if (!strA) return NULL;
    len = WideCharToMultiByte(CP_ACP, 0, str, -1, strA, len, NULL, NULL);
    if (!len) strA[0] = 0;
    return strA;
}


typedef struct {
    HANDLE hFile;
    HANDLE hFileMapping;
    union {
        PBYTE lpFileBase;
        PIMAGE_DOS_HEADER pDosHeader;
    };
    PIMAGE_NT_HEADERS pNtHeaders;
    PIMAGE_SECTION_HEADER Sections;
    PIMAGE_SYMBOL pSymbolTable;
    PSTR pStringTable;
} pe_access_object_t;


static int
pe_get_section_info(void *obj,
                    Dwarf_Half section_index,
                    Dwarf_Obj_Access_Section_a *return_section,
                    UNUSEDARG int *error)
{
    pe_access_object_t *pe_obj = (pe_access_object_t *)obj;

    return_section->as_addr = 0;
    if (section_index == 0) {
        /* Non-elf object formats must honor elf convention that pSection index
         * is always empty. */
        return_section->as_size = 0;
        return_section->as_name = "";
    } else {
        PIMAGE_SECTION_HEADER pSection = pe_obj->Sections + section_index - 1;
        if (pSection->Misc.VirtualSize < pSection->SizeOfRawData) {
            return_section->as_size = pSection->Misc.VirtualSize;
        } else {
            return_section->as_size = pSection->SizeOfRawData;
        }
        return_section->as_name = (const char *)pSection->Name;
        if (return_section->as_name[0] == '/') {
            return_section->as_name = &pe_obj->pStringTable[atoi(&return_section->as_name[1])];
        }
    }
    return_section->as_type = 0;
    return_section->as_flags = 0;
    return_section->as_offset = 0;
    return_section->as_link = 0;
    return_section->as_info = 0;
    return_section->as_addralign = 0;
    return_section->as_entrysize = 0;

    return DW_DLV_OK;
}


static Dwarf_Small
pe_get_byte_order(UNUSEDARG void *obj)
{
    return DW_END_little;
}


static Dwarf_Small
pe_get_pointer_size(void *obj)
{
    pe_access_object_t *pe_obj = (pe_access_object_t *)obj;
    PIMAGE_FILE_HEADER pFileHeader = &pe_obj->pNtHeaders->FileHeader;
    return pFileHeader->Machine == IMAGE_FILE_MACHINE_I386 ? 4 : 8;
}


static Dwarf_Unsigned
pe_get_filesize(void *obj)
{
    pe_access_object_t *pe_obj = (pe_access_object_t *)obj;
    LARGE_INTEGER li;
    if (!GetFileSizeEx(pe_obj->hFile, &li))
        return 0;
    return li.QuadPart;
}


static Dwarf_Unsigned
pe_get_section_count(void *obj)
{
    pe_access_object_t *pe_obj = (pe_access_object_t *)obj;
    PIMAGE_FILE_HEADER pFileHeader = &pe_obj->pNtHeaders->FileHeader;
    return pFileHeader->NumberOfSections + 1;
}


static int
pe_load_section(void *obj,
                Dwarf_Half section_index,
                Dwarf_Small **return_data,
                UNUSEDARG int *error)
{
    pe_access_object_t *pe_obj = (pe_access_object_t *)obj;
    if (section_index == 0) {
        return DW_DLV_NO_ENTRY;
    } else {
        PIMAGE_SECTION_HEADER pSection = pe_obj->Sections + section_index - 1;
        *return_data = pe_obj->lpFileBase + pSection->PointerToRawData;
        return DW_DLV_OK;
    }
}


static const Dwarf_Obj_Access_Methods_a
pe_methods = {
    pe_get_section_info,
    pe_get_byte_order,
    pe_get_pointer_size,
    pe_get_pointer_size,
    pe_get_filesize,
    pe_get_section_count,
    pe_load_section,
    NULL
};


static int
dwarf_pe_init_link(const wchar_t *image,
                   Dwarf_Addr *imagebase,
                   Dwarf_Handler errhand,
                   Dwarf_Ptr errarg,
                   Dwarf_Debug *ret_dbg,
                   Dwarf_Error *error,
                   wchar_t *link_path)
{
    int res = 0;
    pe_access_object_t *pe_obj = 0;
    Dwarf_Obj_Access_Interface_a *intfc = 0;

    /* Initialize the internal struct */
    pe_obj = (pe_access_object_t *)calloc(1, sizeof *pe_obj);
    if (!pe_obj) {
        goto no_internals;
    }

    pe_obj->hFile = CreateFileW(image, GENERIC_READ, FILE_SHARE_READ, NULL,
                       OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    if (pe_obj->hFile == INVALID_HANDLE_VALUE) {
        goto no_file;
    }

    pe_obj->hFileMapping = CreateFileMapping(pe_obj->hFile, NULL, PAGE_READONLY, 0, 0, NULL);
    if (!pe_obj->hFileMapping) {
        goto no_file_mapping;
    }

    pe_obj->lpFileBase = (PBYTE)MapViewOfFile(pe_obj->hFileMapping, FILE_MAP_READ, 0, 0, 0);
    if (!pe_obj->lpFileBase) {
        goto no_view_of_file;
    }

    if (pe_obj->pDosHeader->e_magic != IMAGE_DOS_SIGNATURE) {
        goto no_intfc;
    }
    pe_obj->pNtHeaders = (PIMAGE_NT_HEADERS) (
        pe_obj->lpFileBase +
        pe_obj->pDosHeader->e_lfanew
    );
    if (pe_obj->pNtHeaders->Signature != IMAGE_NT_SIGNATURE) {
        goto no_intfc;
    }
    pe_obj->Sections = (PIMAGE_SECTION_HEADER) (
        (PBYTE)pe_obj->pNtHeaders +
        sizeof(DWORD) +
        sizeof(IMAGE_FILE_HEADER) +
        pe_obj->pNtHeaders->FileHeader.SizeOfOptionalHeader
    );
    pe_obj->pSymbolTable = (PIMAGE_SYMBOL) (
        pe_obj->lpFileBase +
        pe_obj->pNtHeaders->FileHeader.PointerToSymbolTable
    );
    pe_obj->pStringTable = (PSTR)
        &pe_obj->pSymbolTable[pe_obj->pNtHeaders->FileHeader.NumberOfSymbols];

    if (imagebase) {
        WORD sooh = pe_obj->pNtHeaders->FileHeader.SizeOfOptionalHeader;
        if (sooh==sizeof(IMAGE_OPTIONAL_HEADER32)) {
            PIMAGE_OPTIONAL_HEADER32 opt = (PIMAGE_OPTIONAL_HEADER32)(
                    (PBYTE)pe_obj->pNtHeaders +
                    sizeof(DWORD) +
                    sizeof(IMAGE_FILE_HEADER) );
            *imagebase = opt->ImageBase;
        }
        else if (sooh==sizeof(IMAGE_OPTIONAL_HEADER64)) {
            PIMAGE_OPTIONAL_HEADER64 opt = (PIMAGE_OPTIONAL_HEADER64)(
                    (PBYTE)pe_obj->pNtHeaders +
                    sizeof(DWORD) +
                    sizeof(IMAGE_FILE_HEADER) );
            *imagebase = opt->ImageBase;
        }
        else {
            *imagebase = 0;
        }
    }

    /* Initialize the interface struct */
    intfc = (Dwarf_Obj_Access_Interface_a *)calloc(1, sizeof *intfc);
    if (!intfc) {
        goto no_intfc;
    }
    intfc->ai_object = pe_obj;
    intfc->ai_methods = &pe_methods;

    res = dwarf_object_init_b(intfc, errhand, errarg, DW_GROUPNUMBER_ANY, ret_dbg, error);
    if (res != DW_DLV_OK) {
        Dwarf_Unsigned num_sections = pe_obj->pNtHeaders->FileHeader.NumberOfSections;
        Dwarf_Obj_Access_Section_a section;
        char *link;
        if (link_path
                && num_sections > 0
                && pe_get_section_info(pe_obj, num_sections, &section, NULL) == DW_DLV_OK
                && !strcmp(section.as_name, ".gnu_debuglink")
                && pe_load_section(pe_obj, num_sections, (Dwarf_Small **)&link, NULL) == DW_DLV_OK
                && link && link[0]) {
            wcscpy(link_path, image);
            wchar_t *delim1 = wcsrchr(link_path, '/');
            wchar_t *delim2 = wcsrchr(link_path, '\\');
            if (delim2 > delim1) delim1 = delim2;
            if (delim1) delim1++;
            else delim1 = link_path;
            wchar_t *linkW = dwst_ansi2wide(link);
            if (linkW) {
                wcscpy(delim1, linkW);
                if (GetFileAttributesW(link_path) == INVALID_FILE_ATTRIBUTES) {
                    wcscpy(delim1, L".debug/");
                    wcscat(delim1, linkW);
                    if (GetFileAttributesW(link_path) == INVALID_FILE_ATTRIBUTES) {
                        link_path[0] = 0;
                    }
                }
                free(linkW);
            } else {
                link_path[0] = 0;
            }
        }

        goto no_dbg;
    }

    return DW_DLV_OK;

no_dbg:
    free(intfc);
no_intfc:
    UnmapViewOfFile(pe_obj->lpFileBase);
no_view_of_file:
    CloseHandle(pe_obj->hFileMapping);
no_file_mapping:
    CloseHandle(pe_obj->hFile);
no_file:
    free(pe_obj);
no_internals:
    return DW_DLV_ERROR;
}


int
dwarf_pe_init(const wchar_t *image,
              Dwarf_Addr *imagebase,
              Dwarf_Handler errhand,
              Dwarf_Ptr errarg,
              Dwarf_Debug *ret_dbg,
              Dwarf_Error *error)
{
    wchar_t link_path[MAX_PATH];

    link_path[0] = 0;
    int res = dwarf_pe_init_link(image, imagebase, errhand, errarg, ret_dbg, error, link_path);
    if (res == DW_DLV_OK || !link_path[0]) return res;

    return dwarf_pe_init_link(link_path, imagebase, errhand, errarg, ret_dbg, error, NULL);
}


int
dwarf_pe_finish(Dwarf_Debug dbg,
                Dwarf_Error *error)
{
    pe_access_object_t *pe_obj = (pe_access_object_t *)dbg->de_obj_file->ai_object;
    UnmapViewOfFile(pe_obj->lpFileBase);
    CloseHandle(pe_obj->hFileMapping);
    CloseHandle(pe_obj->hFile);
    free(pe_obj);
    free(dbg->de_obj_file);
    return dwarf_object_finish(dbg);
}
