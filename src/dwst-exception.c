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

#ifndef NO_DBGHELP
#include "dbghelp.h"
#endif


#ifdef NO_DBGHELP
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
#endif


#define MAX_FRAMES 32

// csp ... current stack pointer
// cip ... current instruction pointer
// cfp ... current frame pointer
#ifdef _WIN64
#define csp Rsp
#define cip Rip
#define cfp Rbp
#define MACH_TYPE IMAGE_FILE_MACHINE_AMD64
#else
#define csp Esp
#define cip Eip
#define cfp Ebp
#define MACH_TYPE IMAGE_FILE_MACHINE_I386
#endif

#ifndef NO_DBGHELP
int captureStackWalk( HANDLE process,CONTEXT *context,
    uint64_t *frames,int size )
{
  CONTEXT contextCopy;
  memcpy( &contextCopy,context,sizeof(CONTEXT) );
  context = &contextCopy;

  STACKFRAME64 stack;
  ZeroMemory( &stack,sizeof(STACKFRAME64) );
  stack.AddrPC.Offset = context->cip;
  stack.AddrPC.Mode = AddrModeFlat;
  stack.AddrStack.Offset = context->csp;
  stack.AddrStack.Mode = AddrModeFlat;
  stack.AddrFrame.Offset = context->cfp;
  stack.AddrFrame.Mode = AddrModeFlat;

  int count = 0;
  HANDLE thread = GetCurrentThread();
  while( StackWalk64(MACH_TYPE,process,thread,&stack,context,
        NULL,SymFunctionTableAccess64,SymGetModuleBase64,NULL) &&
      count<size )
  {
    uint64_t frame = stack.AddrPC.Offset;
    if( !frame ) break;

    if( count ) frame--;
    frames[count++] = frame;
  }

  return( count );
}
#endif

int dwstOfException(
    void *context,
    dwstCallback *callbackFunc,void *callbackContext )
{
  uint64_t frames[MAX_FRAMES];
  int count = 0;
  CONTEXT *contextP = (CONTEXT*)context;

#ifdef NO_DBGHELP
  frames[count++] = contextP->cip;

  ULONG_PTR csp = *(ULONG_PTR*)contextP->csp;
  if( csp )
    frames[count++] = csp - 1;

  ULONG_PTR *sp = (ULONG_PTR*)contextP->cfp;
  count += captureStackTrace( sp,frames+count,MAX_FRAMES-count );
#else
  HANDLE process = GetCurrentProcess();

  SymSetOptions( SYMOPT_LOAD_LINES );
  SymInitialize( process,NULL,TRUE );

  count += captureStackWalk( process,contextP,frames+count,MAX_FRAMES-count );
#endif

  count = dwstOfProcess( frames,count,callbackFunc,callbackContext );

#ifndef NO_DBGHELP
  SymCleanup( process );
#endif

  return( count );
}
