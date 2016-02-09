/*
 * Copyright 2012 Jose Fonseca
 * Copyright (C) 2013-2015 Hannes Domani
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
#include "dwarf_incl.h"


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
                    Dwarf_Obj_Access_Section *return_section,
                    int *UNUSED(error))
{
    pe_access_object_t *pe_obj = (pe_access_object_t *)obj;

    return_section->addr = 0;
    if (section_index == 0) {
        /* Non-elf object formats must honor elf convention that pSection index
         * is always empty. */
        return_section->size = 0;
        return_section->name = "";
    } else {
        PIMAGE_SECTION_HEADER pSection = pe_obj->Sections + section_index - 1;
        if (pSection->Misc.VirtualSize < pSection->SizeOfRawData) {
            return_section->size = pSection->Misc.VirtualSize;
        } else {
            return_section->size = pSection->SizeOfRawData;
        }
        return_section->name = (const char *)pSection->Name;
        if (return_section->name[0] == '/') {
            return_section->name = &pe_obj->pStringTable[atoi(&return_section->name[1])];
        }
    }
    return_section->link = 0;
    return_section->entrysize = 0;

    return DW_DLV_OK;
}


static Dwarf_Endianness
pe_get_byte_order(void *UNUSED(obj))
{
    return DW_OBJECT_LSB;
}


static Dwarf_Small
pe_get_pointer_size(void *obj)
{
    pe_access_object_t *pe_obj = (pe_access_object_t *)obj;
    PIMAGE_FILE_HEADER pFileHeader = &pe_obj->pNtHeaders->FileHeader;
    return pFileHeader->Machine == IMAGE_FILE_MACHINE_I386 ? 4 : 8;
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
                int *UNUSED(error))
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


static const Dwarf_Obj_Access_Methods
pe_methods = {
    pe_get_section_info,
    pe_get_byte_order,
    pe_get_pointer_size,
    pe_get_pointer_size,
    pe_get_section_count,
    pe_load_section,
    NULL
};


static int
dwarf_pe_init_link(const char *image,
                   Dwarf_Addr *imagebase,
                   Dwarf_Handler errhand,
                   Dwarf_Ptr errarg,
                   Dwarf_Debug *ret_dbg,
                   Dwarf_Error *error,
                   char *link_path)
{
    int res = 0;
    pe_access_object_t *pe_obj = 0;
    Dwarf_Obj_Access_Interface *intfc = 0;

    /* Initialize the internal struct */
    pe_obj = (pe_access_object_t *)calloc(1, sizeof *pe_obj);
    if (!pe_obj) {
        goto no_internals;
    }

    pe_obj->hFile = CreateFile(image, GENERIC_READ, FILE_SHARE_READ, NULL,
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
    intfc = (Dwarf_Obj_Access_Interface *)calloc(1, sizeof *intfc);
    if (!intfc) {
        goto no_intfc;
    }
    intfc->object = pe_obj;
    intfc->methods = &pe_methods;

    res = dwarf_object_init(intfc, errhand, errarg, ret_dbg, error);
    if (res != DW_DLV_OK) {
        Dwarf_Unsigned num_sections = pe_obj->pNtHeaders->FileHeader.NumberOfSections;
        Dwarf_Obj_Access_Section section;
        char *link;
        if (link_path
                && num_sections > 0
                && pe_get_section_info(pe_obj, num_sections, &section, NULL) == DW_DLV_OK
                && !strcmp(section.name, ".gnu_debuglink")
                && pe_load_section(pe_obj, num_sections, (Dwarf_Small **)&link, NULL) == DW_DLV_OK
                && link && link[0]) {
            strcpy(link_path, image);
            char *delim1 = strrchr(link_path, '/');
            char *delim2 = strrchr(link_path, '\\');
            if (delim2 > delim1) delim1 = delim2;
            if (delim1) delim1++;
            else delim1 = link_path;
            strcpy(delim1, link);
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
dwarf_pe_init(const char *image,
              Dwarf_Addr *imagebase,
              Dwarf_Handler errhand,
              Dwarf_Ptr errarg,
              Dwarf_Debug *ret_dbg,
              Dwarf_Error *error)
{
    char link_path[MAX_PATH];

    link_path[0] = 0;
    int res = dwarf_pe_init_link(image, imagebase, errhand, errarg, ret_dbg, error, link_path);
    if (res == DW_DLV_OK || !link_path[0]) return res;

    return dwarf_pe_init_link(link_path, imagebase, errhand, errarg, ret_dbg, error, NULL);
}


int
dwarf_pe_finish(Dwarf_Debug dbg,
                Dwarf_Error *error)
{
    pe_access_object_t *pe_obj = (pe_access_object_t *)dbg->de_obj_file->object;
    UnmapViewOfFile(pe_obj->lpFileBase);
    CloseHandle(pe_obj->hFileMapping);
    CloseHandle(pe_obj->hFile);
    free(pe_obj);
    free(dbg->de_obj_file);
    return dwarf_object_finish(dbg, error);
}
