/*
 * Copyright (C) 2013-2014 Hannes Domani
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


#include <dwarfstack.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <stdio.h>
#include <tchar.h>
#include <stdlib.h>

#ifndef NO_DBGHELP
#include <dbghelp.h>
#endif


static BOOL CALLBACK enumWindowsForDisable( HWND hwnd,LPARAM lparam )
{
  DWORD processId;
  GetWindowThreadProcessId( hwnd,&processId );
  if( processId==(DWORD)lparam ) EnableWindow( hwnd,FALSE );

  return( TRUE );
}

static LRESULT CALLBACK exceptionWndProc(
    HWND hwnd,UINT message,WPARAM wparam,LPARAM lparam )
{
  switch( message )
  {
    case WM_DESTROY:
      PostQuitMessage( 0 );
      return( 0 );

    case WM_COMMAND:
      if( LOWORD(wparam)==IDCANCEL )
      {
        DestroyWindow( hwnd );
        return( 0 );
      }
      break;

    case WM_CLOSE:
      DestroyWindow( hwnd );
      return( 0 );
  }

  return( DefDlgProc(hwnd,message,wparam,lparam) );
}

struct dialog_info
{
  int count;
  HWND hwnd;
  void *lastBase;
};

static void printAddr( HWND hwnd,const TCHAR *beg,int num,
    void *ptr,const TCHAR *posFile,int posLine,const TCHAR *funcName )
{
  TCHAR hexNum[20];

  if( ptr )
  {
    Edit_ReplaceSel( hwnd,beg );

    if( num>=0 )
    {
      _stprintf( hexNum,TEXT("%02d"),num );
      Edit_ReplaceSel( hwnd,hexNum );
    }

    Edit_ReplaceSel( hwnd,TEXT(": 0x") );
    _stprintf( hexNum,TEXT("%p"),ptr );
    Edit_ReplaceSel( hwnd,hexNum );
  }
  else
  {
    _stprintf( hexNum,TEXT("%*s"),14,TEXT("") );
    Edit_ReplaceSel( hwnd,hexNum );
    _stprintf( hexNum,TEXT("%*s"),2+(int)sizeof(void*)*2,TEXT("") );
    Edit_ReplaceSel( hwnd,hexNum );
  }

  if( posFile )
  {
    Edit_ReplaceSel( hwnd,TEXT(" (") );
    Edit_ReplaceSel( hwnd,posFile );

    if( posLine>0 )
    {
      Edit_ReplaceSel( hwnd,TEXT(":") );
      _stprintf( hexNum,TEXT("%d"),posLine );
      Edit_ReplaceSel( hwnd,hexNum );
    }

    Edit_ReplaceSel( hwnd,TEXT(")") );
  }

  if( funcName )
  {
    Edit_ReplaceSel( hwnd,TEXT(" [") );
    Edit_ReplaceSel( hwnd,funcName );
    Edit_ReplaceSel( hwnd,TEXT("]") );
  }

  Edit_ReplaceSel( hwnd,TEXT("\r\n") );
}

static void dlgPrint(
    uint64_t addr,const char *filename,int lineno,const char *funcname,
    struct dialog_info *context )
{
#ifndef NO_DBGHELP
  char buffer[sizeof(SYMBOL_INFO)+MAX_SYM_NAME];
  SYMBOL_INFO *si = (SYMBOL_INFO*)&buffer;
  if( lineno==DWST_NO_DBG_SYM )
  {
    IMAGEHLP_LINE64 ihl;
    memset( &ihl,0,sizeof(IMAGEHLP_LINE64) );
    ihl.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
    DWORD displ;
    if( SymGetLineFromAddr64(GetCurrentProcess(),addr,&displ,&ihl) )
    {
      filename = ihl.FileName;
      lineno = ihl.LineNumber;
    }

    DWORD64 displ2;
    si->SizeOfStruct = sizeof(SYMBOL_INFO);
    si->MaxNameLen = MAX_SYM_NAME;
    if( SymFromAddr(GetCurrentProcess(),addr,&displ2,si) )
    {
      char *fn = si->Name;
      fn[MAX_SYM_NAME] = 0;
      funcname = fn;
    }
  }
#endif

  const char *delim = strrchr( filename,'/' );
  if( delim ) filename = delim + 1;
  delim = strrchr( filename,'\\' );
  if( delim ) filename = delim + 1;

  void *ptr = (void*)(uintptr_t)addr;
  switch( lineno )
  {
    case DWST_BASE_ADDR:
      if( ptr==context->lastBase ) break;
      context->lastBase = ptr;

      printAddr( context->hwnd,TEXT("base address"),-1,
          ptr,filename,0,NULL );
      break;

    case DWST_NOT_FOUND:
    case DWST_NO_DBG_SYM:
    case DWST_NO_SRC_FILE:
      printAddr( context->hwnd,TEXT("    stack "),context->count++,
          ptr,NULL,0,funcname );
      break;

    default:
      printAddr( context->hwnd,TEXT("    stack "),context->count,
          ptr,filename,lineno,funcname );
      if( ptr ) context->count++;
      break;
  }
}

static char *myExtraInfo = NULL;

static LONG WINAPI exceptionPrinter( LPEXCEPTION_POINTERS ep )
{
  EnumWindows( enumWindowsForDisable,GetCurrentProcessId() );

#define DWARFSTACK_DIALOG_CLASS TEXT("dwarfstack_exception_dialog")
  WNDCLASS wc;
  wc.style = CS_HREDRAW | CS_VREDRAW;
  wc.lpfnWndProc = exceptionWndProc;
  wc.cbClsExtra = 0;
  wc.cbWndExtra = DLGWINDOWEXTRA;
  wc.hInstance = GetModuleHandle( NULL );
  wc.hIcon = NULL;
  wc.hCursor = LoadCursor( NULL,IDC_ARROW );
  wc.hbrBackground = (HBRUSH)( COLOR_BTNFACE+1 );
  wc.lpszMenuName = NULL;
  wc.lpszClassName = DWARFSTACK_DIALOG_CLASS;
  RegisterClass( &wc );

  int sw = GetSystemMetrics( SM_CXSCREEN );
  int sh = GetSystemMetrics( SM_CYSCREEN );
#define DLG_W 500
#define DLG_H 500

  HWND hwnd = CreateWindow(
      DWARFSTACK_DIALOG_CLASS,TEXT("application crashed"),
      WS_CAPTION,(sw-DLG_W)/2,(sh-DLG_H)/2,DLG_W,DLG_H,
      NULL,NULL,GetModuleHandle(NULL),NULL );

  RECT rect;
  GetClientRect( hwnd,&rect );
  int w = rect.right - rect.left;
  int h = rect.bottom - rect.top;

  HWND textHwnd = CreateWindow( TEXT("edit"),TEXT(""),
      WS_CHILD|WS_VISIBLE|WS_TABSTOP|WS_BORDER|WS_HSCROLL|WS_VSCROLL|
      ES_MULTILINE|ES_AUTOHSCROLL|ES_AUTOVSCROLL|ES_READONLY,
      0,0,w,h-32,hwnd,NULL,NULL,NULL );
  CreateWindow( TEXT("button"),TEXT("OK"),
      WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_DEFPUSHBUTTON,
      (w-50)/2,h-28,50,24,
      hwnd,(HMENU)IDCANCEL,NULL,NULL );

#ifdef DWST_SHARED
  // needs -lgdi32 -> only in shared library
  HFONT font = GetStockObject( ANSI_FIXED_FONT );
  if( font )
    SendMessage( textHwnd,WM_SETFONT,(WPARAM)font,FALSE );
#endif


  {
    TCHAR exeName[MAX_PATH];
    if( GetModuleFileName(GetModuleHandle(NULL),exeName,MAX_PATH) )
    {
      Edit_ReplaceSel( textHwnd,TEXT("application:\r\n") );
      Edit_ReplaceSel( textHwnd,exeName );
      Edit_ReplaceSel( textHwnd,TEXT("\r\n\r\n") );
    }
  }

  if( myExtraInfo )
  {
    Edit_ReplaceSel( textHwnd,TEXT("extra information:\r\n") );
    Edit_ReplaceSel( textHwnd,myExtraInfo );
    Edit_ReplaceSel( textHwnd,TEXT("\r\n\r\n") );
  }

  DWORD code = ep->ExceptionRecord->ExceptionCode;
  const TCHAR *desc = NULL;
  switch( code )
  {
#define EX_DESC( name ) \
    case EXCEPTION_##name: desc = TEXT(" (") #name ")"; \
                           break

    EX_DESC( ACCESS_VIOLATION );
    EX_DESC( ARRAY_BOUNDS_EXCEEDED );
    EX_DESC( BREAKPOINT );
    EX_DESC( DATATYPE_MISALIGNMENT );
    EX_DESC( FLT_DENORMAL_OPERAND );
    EX_DESC( FLT_DIVIDE_BY_ZERO );
    EX_DESC( FLT_INEXACT_RESULT );
    EX_DESC( FLT_INVALID_OPERATION );
    EX_DESC( FLT_OVERFLOW );
    EX_DESC( FLT_STACK_CHECK );
    EX_DESC( FLT_UNDERFLOW );
    EX_DESC( ILLEGAL_INSTRUCTION );
    EX_DESC( IN_PAGE_ERROR );
    EX_DESC( INT_DIVIDE_BY_ZERO );
    EX_DESC( INT_OVERFLOW );
    EX_DESC( INVALID_DISPOSITION );
    EX_DESC( NONCONTINUABLE_EXCEPTION );
    EX_DESC( PRIV_INSTRUCTION );
    EX_DESC( SINGLE_STEP );
    EX_DESC( STACK_OVERFLOW );
  }
  TCHAR hexNum[20];

  Edit_ReplaceSel( textHwnd,TEXT("code: 0x") );
  _stprintf( hexNum,TEXT("%08lX"),code );
  Edit_ReplaceSel( textHwnd,hexNum );
  if( desc )
    Edit_ReplaceSel( textHwnd,desc );
  Edit_ReplaceSel( textHwnd,TEXT("\r\n") );

  if( code==EXCEPTION_ACCESS_VIOLATION &&
      ep->ExceptionRecord->NumberParameters==2 )
  {
    ULONG_PTR flag = ep->ExceptionRecord->ExceptionInformation[0];
    ULONG_PTR addr = ep->ExceptionRecord->ExceptionInformation[1];
    Edit_ReplaceSel( textHwnd,
        flag==8?TEXT("data execution prevention"):
        (flag?TEXT("write access"):TEXT("read access")) );
    Edit_ReplaceSel( textHwnd,TEXT(" violation at 0x") );
    _stprintf( hexNum,TEXT("%p"),(void*)addr );
    Edit_ReplaceSel( textHwnd,hexNum );
    Edit_ReplaceSel( textHwnd,TEXT("\r\n") );
  }
  Edit_ReplaceSel( textHwnd,TEXT("\r\n") );

  struct dialog_info di = { 0,textHwnd,NULL };
  dwstOfException(
      ep->ContextRecord,(dwstCallback*)&dlgPrint,&di );

  SendMessage( hwnd,WM_NEXTDLGCTL,(WPARAM)textHwnd,TRUE );
  Edit_SetSel( textHwnd,0,0 );

  ShowWindow( hwnd,SW_SHOW );
  UpdateWindow( hwnd );

  MSG msg;
  while( GetMessage(&msg,NULL,0,0) )
  {
    if( IsDialogMessage(hwnd,&msg) ) continue;

    TranslateMessage( &msg );
    DispatchMessage( &msg );
  }

  return( EXCEPTION_EXECUTE_HANDLER );
}


void dwstExceptionDialog( const char *extraInfo )
{
  SetUnhandledExceptionFilter( exceptionPrinter );

  free( myExtraInfo );
  myExtraInfo = extraInfo ? strdup( extraInfo ) : NULL;
}
