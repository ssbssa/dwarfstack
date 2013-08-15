
//          Copyright Hannes Domani 2013.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file ../LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <dwarfstack.h>

#include <windows.h>


void crasher( void );

void d( void )
{
  crasher();
}

void c( void )
{
  d();
}

void b( void )
{
  c();
}

void a( void )
{
  b();
}


static LRESULT CALLBACK wndProc(
    HWND hwnd,UINT message,WPARAM wparam,LPARAM lparam )
{
  switch( message )
  {
    case WM_DESTROY:
      PostQuitMessage( 0 );
      return( 0 );

    case WM_COMMAND:
      if( LOWORD(wparam)==IDOK  )
      {
        a();
        return( 0 );
      }
      else if( LOWORD(wparam)==IDCANCEL )
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


int main( void )
{
  dwstExceptionDialog( __DATE__ " - " __TIME__ );


  WNDCLASS wc;
  wc.style = CS_HREDRAW | CS_VREDRAW;
  wc.lpfnWndProc = wndProc;
  wc.cbClsExtra = 0;
  wc.cbWndExtra = DLGWINDOWEXTRA;
  wc.hInstance = GetModuleHandle( NULL );
  wc.hIcon = NULL;
  wc.hCursor = LoadCursor( NULL,IDC_ARROW );
  wc.hbrBackground = (HBRUSH)( COLOR_BTNFACE+1 );
  wc.lpszMenuName = NULL;
  wc.lpszClassName = TEXT("crasher");
  RegisterClass( &wc );

  HWND hwnd = CreateWindow(
      TEXT("crasher"),TEXT("exception tester"),
      WS_CAPTION|WS_OVERLAPPED|WS_SYSMENU,100,100,200,100,
      NULL,NULL,GetModuleHandle(NULL),NULL );
  CreateWindow(
      TEXT("button"),TEXT("crash me"),
      WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_DEFPUSHBUTTON,
      50,25,100,24,
      hwnd,(HMENU)IDOK,NULL,NULL );

  ShowWindow( hwnd,SW_SHOW );
  UpdateWindow( hwnd );

  MSG msg;
  while( GetMessage(&msg,NULL,0,0) )
  {
    if( IsDialogMessage(hwnd,&msg) ) continue;

    TranslateMessage( &msg );
    DispatchMessage( &msg );
  }

  return( 0 );
}
