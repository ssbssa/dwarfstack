/*
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */


#include "dwarfstack.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>


int dwstOfFileExt(
    const char *name,const wchar_t *nameW,uint64_t imageBase,
    uint64_t *addr,int count,
    dwstCallback *callbackFunc,dwstCallbackW *callbackFuncW,
    void *callbackContext );

int dwstOfProcessExt(
    uintptr_t *addr,int count,
    dwstCallback *callbackFunc,dwstCallbackW *callbackFuncW,
    void *callbackContext )
{
  if( !addr || !count || (!callbackFunc && !callbackFuncW) ) return( 0 );

  MEMORY_BASIC_INFORMATION mbi;
  wchar_t name[MAX_PATH];
  int s;
  int converted = 0;
  for( s=0; s<count; s++ )
  {
    uintptr_t ptr = addr[s];

    if( !VirtualQuery((void*)ptr,
          &mbi,sizeof(MEMORY_BASIC_INFORMATION)) ||
        mbi.State!=MEM_COMMIT ||
        !(mbi.Protect&(PAGE_EXECUTE|PAGE_EXECUTE_READ)) ||
        mbi.Type!=MEM_IMAGE )
      continue;

    void *base = mbi.AllocationBase;
    if( !GetModuleFileNameW(base,name,MAX_PATH) )
      continue;

    int c;
    for( c=1; s+c<count; c++ )
    {
      ptr = addr[s+c];

      if( !VirtualQuery((void*)ptr,
            &mbi,sizeof(MEMORY_BASIC_INFORMATION)) ||
          mbi.AllocationBase!=base )
        break;
    }

#ifndef _WIN64
    uint64_t addr64[c];
    int c64;
    for( c64=0; c64<c; c64++ )
      addr64[c64] = addr[s+c64];
    uint64_t *addrPos = addr64;
#else
    uint64_t *addrPos = addr + s;
#endif

    converted += dwstOfFileExt(
        NULL,name,(uintptr_t)base,addrPos,c,
        callbackFunc,callbackFuncW,callbackContext );
    s += c - 1;
  }

  return( converted );
}

int dwstOfProcess(
    uintptr_t *addr,int count,
    dwstCallback *callbackFunc,void *callbackContext )
{
  return( dwstOfProcessExt(addr,count,callbackFunc,NULL,callbackContext) );
}

int dwstOfProcessW(
    uintptr_t *addr,int count,
    dwstCallbackW *callbackFunc,void *callbackContext )
{
  return( dwstOfProcessExt(addr,count,NULL,callbackFunc,callbackContext) );
}
