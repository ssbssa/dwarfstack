
//          Copyright Hannes Domani 2013.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file ../LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

char *c_leaks( void )
{
  char *strdup_leak = strdup( "leak 1" );
  strdup_leak[1] = 0;
  printf( "%s\n",strdup_leak );

  char *malloc_leak = (char*)malloc( 60 );
  malloc_leak[0] = 'm';
  malloc_leak[1] = 0;
  printf( "%s\n",malloc_leak );

  char *realloc_leak = (char*)realloc( NULL,100 );
  realloc_leak[0] = 'r';
  realloc_leak[1] = 0;
  printf( "%s\n",realloc_leak );

  wchar_t *wcsdup_leak = wcsdup( L"leak 2" );
  wcsdup_leak[1] = 0;
  printf( "%s\n",(char*)wcsdup_leak );

  char *calloc_leak = (char*)calloc( 3,200 );
  calloc_leak[0] = 'c';
  calloc_leak[1] = 0;
  printf( "%s\n",calloc_leak );

  return( malloc_leak );
}
