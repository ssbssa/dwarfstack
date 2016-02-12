
//          Copyright Hannes Domani 2013.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file ../LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)


#include <dwarfstack.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


static uint64_t prevAddr = 0;

static void stdoutPrint(
    uint64_t addr,const char *filename,int lineno,const char *funcname,
    void *context )
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
      printf( "    stack %02d: 0x%0*I64X (%s)\n",
          (*count)++,addrSize,addr,filename );
      break;

    default:
      if( addr )
        printf( "    stack %02d: 0x%0*I64X (%s:%d)",
            (*count)++,addrSize,addr,filename,lineno );
      else
        printf( "                %*s (%s:%d)",
            addrSize,"",filename,lineno );
      if( funcname )
        printf( " [%s]",funcname );
      printf( "\n" );
      break;
  }
}

static void usage( const char *exe )
{
  const char *delim = strrchr( exe,'/' );
  if( delim ) exe = delim + 1;
  delim = strrchr( exe,'\\' );
  if( delim ) exe = delim + 1;

  printf( "Usage: %s [executable] [option] [addr(s)]\n",exe );
  printf( " -b<base>                    Set base address\n" );
}

int main( int argc,char **argv )
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
      base = strtoll( argv[i]+2,NULL,16 );
      continue;
    }

    addr[addrCount++] = strtoll( argv[i],NULL,16 );
  }

  if( !addrCount )
  {
    usage( argv[0] );
    return( 1 );
  }

  int count = 0;
  dwstOfFile( argv[1],base,addr,addrCount,&stdoutPrint,&count );

  return( 0 );
}
