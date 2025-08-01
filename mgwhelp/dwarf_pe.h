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


#ifndef _DWARF_PE_H_
#define _DWARF_PE_H_


#include <dwarf.h>
#include <libdwarf.h>


#ifdef __cplusplus
extern "C" {
#endif


int
dwarf_pe_init(const wchar_t *image,
              Dwarf_Addr *imagebase,
              Dwarf_Handler errhand,
              Dwarf_Ptr errarg,
              Dwarf_Debug * ret_dbg, Dwarf_Error * error);

int
dwarf_pe_finish(Dwarf_Debug dbg, Dwarf_Error * error);


wchar_t *
dwst_ansi2wide(const char *str);

char *
dwst_wide2ansi(const wchar_t *str);


#ifdef __cplusplus
}
#endif


#endif /* _DWARF_PE_H_ */
