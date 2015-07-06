
//          Copyright Hannes Domani 2013.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file ../LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <dwarfstack.h>

#include <stdio.h>
#include <string.h>


static void stdoutPrint(
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
      printf( "base address: 0x%p (%s)\n",
          ptr,filename );
      break;

    case DWST_NOT_FOUND:
      printf( "   not found: 0x%p (%s)\n",
          ptr,filename );
      break;

    case DWST_NO_DBG_SYM:
    case DWST_NO_SRC_FILE:
      printf( "    stack %02d: 0x%p (%s)\n",
          (*context)++,ptr,filename );
      break;

    default:
      if( ptr )
        printf( "    stack %02d: 0x%p (%s:%d)",
            (*context)++,ptr,filename,lineno );
      else
        printf( "                %*s (%s:%d)",
            (int)sizeof(void*)*2,"",filename,lineno );
      if( funcname )
        printf( " [%s]",funcname );
      printf( "\n" );
      break;
  }
}

static void obj2func2( void )
{
  int count = 0;
  dwstOfLocation( (dwstCallback*)stdoutPrint,&count );
}

void obj2func( void )
{
  obj2func2();
}
