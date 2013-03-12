
//          Copyright Hannes Domani 2013.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file ../LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <stdlib.h>


void obj2func( void );

void d( void )
{
  obj2func();
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
  a();

  return( 0 );
}
