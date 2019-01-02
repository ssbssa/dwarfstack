
//          Copyright Hannes Domani 2013.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file ../LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)


#include <dwarfstack.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>


static uint64_t prevAddr = 0;

static void stdoutPrint(
    uint64_t addr,const wchar_t *filename,int lineno,const char *funcname,
    void *context,int columnno )
{
  int *count = context;
  uint64_t tAddr = addr ? addr : prevAddr;
  int addrSize = tAddr>0xffffffff ? 16 : 8;
  switch( lineno )
  {
    case DWST_BASE_ADDR:
    case DWST_NOT_FOUND:
      break;

    case DWST_NO_DBG_SYM:
    case DWST_NO_SRC_FILE:
      printf( "    stack %02d: 0x%0*I64X (%ls)\n",
          (*count)++,addrSize,addr,filename );
      break;

    default:
      if( addr )
        printf( "    stack %02d: 0x%0*I64X",(*count)++,addrSize,addr );
      else
        printf( "                %*s",addrSize,"" );
      printf( " (%ls:%d",filename,lineno );
      if( columnno>0 )
        printf( ":%d",columnno );
      printf( ")" );
      if( funcname )
        printf( " [%s]",funcname );
      printf( "\n" );
      break;
  }
}

static void usage( const wchar_t *exe )
{
  const wchar_t *delim = wcsrchr( exe,'/' );
  if( delim ) exe = delim + 1;
  delim = wcsrchr( exe,'\\' );
  if( delim ) exe = delim + 1;

  printf( "Usage: %ls [executable] [option] [addr(s)]\n",exe );
  printf( " -b<base>                    Set base address\n" );
}

int wmain( int argc,wchar_t **argv )
{
  if( argc<3 )
  {
    usage( argv[0] );
    return( 1 );
  }

  int i;
  uint64_t addr[argc-2];
  uint64_t base = 0;
  int addrCount = 0;
  for( i=2; i<argc; i++ )
  {
    if( argv[i][0]=='-' && argv[i][1]=='b' )
    {
      base = wcstoll( argv[i]+2,NULL,16 );
      continue;
    }

    addr[addrCount++] = wcstoll( argv[i],NULL,16 );
  }

  if( !addrCount )
  {
    usage( argv[0] );
    return( 1 );
  }

  int count = 0;
  dwstOfFileW( argv[1],base,addr,addrCount,&stdoutPrint,&count );

  return( 0 );
}
