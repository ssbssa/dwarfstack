/*
 * Copyright (C) 2013 Hannes Domani
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */


#include "dwarfstack.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>


int dwstOfProcess(
    uint64_t *addr,int count,
    dwstCallback *callbackFunc,void *callbackContext )
{
  if( !addr || !count || !callbackFunc ) return( 0 );

  MEMORY_BASIC_INFORMATION mbi;
  char name[MAX_PATH];
  int s;
  int converted = 0;
  for( s=0; s<count; s++ )
  {
    uint64_t ptr = addr[s];

    if( !VirtualQuery((void*)(uintptr_t)ptr,
          &mbi,sizeof(MEMORY_BASIC_INFORMATION)) ||
        mbi.State!=MEM_COMMIT ||
        !(mbi.Protect&(PAGE_EXECUTE|PAGE_EXECUTE_READ)) ||
        mbi.Type!=MEM_IMAGE )
      continue;

    void *base = mbi.AllocationBase;
    if( !GetModuleFileName(base,name,MAX_PATH) )
      continue;

    int c;
    for( c=1; s+c<count; c++ )
    {
      ptr = addr[s+c];

      if( !VirtualQuery((void*)(uintptr_t)ptr,
            &mbi,sizeof(MEMORY_BASIC_INFORMATION)) ||
          mbi.AllocationBase!=base )
        break;
    }

    converted += dwstOfFile(
        name,(uintptr_t)base,addr+s,c,callbackFunc,callbackContext );
    s += c - 1;
  }

  return( converted );
}
