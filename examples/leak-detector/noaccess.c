
//          Copyright Hannes Domani 2013.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file ../LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <stdio.h>

char *c_leaks( void );

int main( void )
{
  char *l = c_leaks();
  l[3] = l[-5];
  l[100] = 4;

  return( 0 );
}
