/*
 * Copyright (C) 2013-2019 Hannes Domani
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


#ifdef NO_DBGHELP
int captureStackTrace( ULONG_PTR *frameAddr,uintptr_t *frames,int size );
#endif

typedef USHORT WINAPI CaptureStackBackTraceFunc( ULONG,ULONG,PVOID*,PULONG );


#define MAX_FRAMES 32

int dwstOfProcessExt(
    uintptr_t *addr,int count,
    dwstCallback *callbackFunc,dwstCallbackW *callbackFuncW,
    void *callbackContext );

static int dwstOfLocationExt(
    dwstCallback *callbackFunc,dwstCallbackW *callbackFuncW,
    void *callbackContext )
{
  uintptr_t frames[MAX_FRAMES];

  int count;
  HANDLE kernel32 = GetModuleHandle( "kernel32.dll" );
  CaptureStackBackTraceFunc *CaptureStackBackTrace = NULL;
  if( kernel32 )
    CaptureStackBackTrace = (CaptureStackBackTraceFunc*)GetProcAddress(
        kernel32,"RtlCaptureStackBackTrace" );
  if( CaptureStackBackTrace )
  {
    count = CaptureStackBackTrace( 1,MAX_FRAMES,(PVOID*)frames,NULL );

    int i;
    for( i=0; i<count; i++ )
      frames[i]--;
  }
  else
  {
    // fallback for older systems
#ifdef NO_DBGHELP
    ULONG_PTR *sp = __builtin_frame_address( 0 );

    count = captureStackTrace( sp,frames,MAX_FRAMES );
#else
    frames[0] = (uintptr_t)__builtin_return_address( 0 ) - 1;
    count = 1;
#endif
  }

  return( dwstOfProcessExt(frames,count,
        callbackFunc,callbackFuncW,callbackContext) );
}

int dwstOfLocation( dwstCallback *callbackFunc,void *callbackContext )
{
  return( dwstOfLocationExt(callbackFunc,NULL,callbackContext) );
}

int dwstOfLocationW(
    dwstCallbackW *callbackFunc,void *callbackContext )
{
  return( dwstOfLocationExt(NULL,callbackFunc,callbackContext) );
}
