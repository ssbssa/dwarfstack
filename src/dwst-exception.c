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


int captureStackTrace( ULONG_PTR *frameAddr,uint64_t *frames,int size )
{
  int count;
  ULONG_PTR *sp = frameAddr;

  for( count=0; count<size; )
  {
    if( IsBadReadPtr(sp,2*sizeof(ULONG_PTR)) || !sp[0] || !sp[1] )
      break;

    ULONG_PTR *np = (ULONG_PTR*)sp[0];
    frames[count++] = sp[1] - 1;

    sp = np;
  }

  return( count );
}


#define MAX_FRAMES 32

// csp ... current stack pointer
// cip ... current instruction pointer
// cfp ... current frame pointer
#ifdef _WIN64
#define csp Rsp
#define cip Rip
#define cfp Rbp
#else
#define csp Esp
#define cip Eip
#define cfp Ebp
#endif

int dwstOfException(
    void *context,
    dwstCallback *callbackFunc,void *callbackContext )
{
  uint64_t frames[MAX_FRAMES];
  int count = 0;
  CONTEXT *contextP = (CONTEXT*)context;

  frames[count++] = contextP->cip;

  ULONG_PTR csp = *(ULONG_PTR*)contextP->csp;
  if( csp )
    frames[count++] = csp - 1;

  ULONG_PTR *sp = (ULONG_PTR*)contextP->cfp;
  count += captureStackTrace( sp,frames+count,MAX_FRAMES-count );

  return( dwstOfProcess(frames,count,
        callbackFunc,callbackContext) );
}
