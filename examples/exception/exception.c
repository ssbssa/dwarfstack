
//          Copyright Hannes Domani 2013.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file ../LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <dwarfstack.h>

#include <stdio.h>
#include <windows.h>


static void stderrPrint(
    uint64_t addr,const char *filename,int lineno,const char *funcname,
    int *context )
{
  const char *delim = strrchr( filename,'/' );
  if( delim ) filename = delim + 1;
  delim = strrchr( filename,'\\' );
  if( delim ) filename = delim + 1;

  void *ptr = (void*)(uintptr_t)addr;
  switch( lineno )
  {
    case DWST_BASE_ADDR:
      fprintf( stderr,"base address: 0x%p (%s)\n",
          ptr,filename );
      break;

    case DWST_NOT_FOUND:
    case DWST_NO_DBG_SYM:
    case DWST_NO_SRC_FILE:
      fprintf( stderr,"    stack %02d: 0x%p (%s)\n",
          (*context)++,ptr,filename );
      break;

    default:
      if( ptr )
        fprintf( stderr,"    stack %02d: 0x%p (%s:%d)",
            (*context)++,ptr,filename,lineno );
      else
        fprintf( stderr,"                %*s (%s:%d)",
            (int)sizeof(void*)*2,"",filename,lineno );
      if( funcname )
        fprintf( stderr," [%s]",funcname );
      fprintf( stderr,"\n" );
      break;
  }
}

static LONG WINAPI exceptionPrinter( LPEXCEPTION_POINTERS ep )
{
  fprintf( stderr,"application crashed\n" );

  DWORD code = ep->ExceptionRecord->ExceptionCode;
  const char *desc = "";
  switch( code )
  {
#define EX_DESC( name ) \
    case EXCEPTION_##name: desc = " (" #name ")"; \
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
  fprintf( stderr,"code: 0x%08lX%s\n",code,desc );

  if( code==EXCEPTION_ACCESS_VIOLATION &&
      ep->ExceptionRecord->NumberParameters==2 )
  {
    ULONG_PTR flag = ep->ExceptionRecord->ExceptionInformation[0];
    ULONG_PTR addr = ep->ExceptionRecord->ExceptionInformation[1];
    fprintf( stderr,"%s violation at 0x%p\n",
        flag==8?"data execution prevention":
        (flag?"write access":"read access"),(void*)addr );
  }

  int count = 0;
  dwstOfException(
      ep->ContextRecord,(dwstCallback*)&stderrPrint,&count );

  fflush( stderr );

  return( EXCEPTION_EXECUTE_HANDLER );
}


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

int main( void )
{
  SetUnhandledExceptionFilter( exceptionPrinter );

  a();

  return( 0 );
}
