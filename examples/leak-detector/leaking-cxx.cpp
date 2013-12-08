
//          Copyright Hannes Domani 2013.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file ../LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <stdio.h>

void c_leaks( void );

void cxx_leaks( void )
{
  c_leaks();

  int *new_leak = new int;
  new_leak[0] = 15;
  printf( "%d\n",new_leak[0] );

  char *newa_leak = new char[120];
  newa_leak[0] = 'n';
  newa_leak[1] = 0;
  printf( "%s\n",newa_leak );
}
