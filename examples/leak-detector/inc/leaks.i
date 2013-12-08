
//          Copyright Hannes Domani 2013.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file ../../LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

/*
 * LEAKS_PTRS
 *      number of saved stack frames
 *      (default 16)
 *
 * LEAKS_CAPTURE_STACKTRACE
 *      use CaptureStackBackTrace() instead of __builtin_return_address()
 *      (default off)
 *
 * LEAKS_USE_DWST
 *      use dwarfstack.dll to convert stack into source file/line information
 *      (default on)
 *
 * LEAKS_THREAD
 *      thread-safety
 *      (default off)
 *
 * LEAKS_EXTRA_INIT
 *      user-specified extra initialisation
 *
 * LEAKS_EXTRA_FREE
 *      user-specified extra cleanup
 *
 * user-provided leak detection should use ALLOC(f,p,s) and FREE(f,p) macros
 *      f -> flag (single char for immediate identification)
 *      p -> pointer to allocated memory
 *      s -> size of allocated memory
 */

#define __STDC_FORMAT_MACROS
#define WIN32_LEAN_AND_MEAN
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <windows.h>


#define REPEAT_X1(off,def)  def(off)
#define REPEAT_X2(off,def)  REPEAT_X1(off,def)  REPEAT_X1(off+1,def)
#define REPEAT_X4(off,def)  REPEAT_X2(off,def)  REPEAT_X2(off+2,def)
#define REPEAT_X8(off,def)  REPEAT_X4(off,def)  REPEAT_X4(off+4,def)
#define REPEAT_X16(off,def) REPEAT_X8(off,def)  REPEAT_X8(off+8,def)
#define REPEAT_X32(off,def) REPEAT_X16(off,def) REPEAT_X16(off+16,def)
#define REPEAT_X48(off,def) REPEAT_X32(off,def) REPEAT_X16(off+32,def)
#define REPEAT_X64(off,def) REPEAT_X32(off,def) REPEAT_X32(off+32,def)

#define CONCAT2(a,b) a##b
#define CONCAT(a,b) CONCAT2(a,b)

#ifndef LEAKS_PTRS
#define LEAKS_PTRS 16
#endif
#define REPEATER(def) CONCAT(REPEAT_X,LEAKS_PTRS)(0,def)

#ifndef LEAKS_USE_DWST
#define LEAKS_USE_DWST 1
#endif

#if defined(LEAKS_CAPTURE_STACKTRACE) && LEAKS_PTRS>61
#undef LEAKS_CAPTURE_STACKTRACE
#endif


typedef struct
{
  void *ptr;
  void *rets[LEAKS_PTRS];
  size_t size;
  char flag;
} allocation;

static int inited = 0;
static allocation *alloc_a = NULL;
static int alloc_q = 0;
static int alloc_s = 0;

#ifdef LEAKS_THREAD
static HANDLE allocMutex = NULL;
#define MUTEX_LOCK() WaitForSingleObject( allocMutex,INFINITE )
#define MUTEX_UNLOCK() ReleaseMutex( allocMutex )
#else
#define MUTEX_LOCK() do {} while( 0 )
#define MUTEX_UNLOCK() do {} while( 0 )
#endif


#if LEAKS_USE_DWST
#ifndef UNUSED
#ifdef __GNUC__
#define UNUSED(a) a __attribute__((unused))
#else
#define UNUSED(a) a
#endif
#endif

typedef void dwstCallback(
    uint64_t addr,const char *filename,int lineno,
    void *context );
typedef int dwstOfProcess_func(
    uint64_t *addr,int count,
    dwstCallback *callbackFunc,void *callbackContext );

static HMODULE dwstMod = NULL;
static dwstOfProcess_func *dwstOfProcess = NULL;

static void output_func(
    uint64_t addr,const char *filename,int lineno,
    void *UNUSED(context) )
{
  if( lineno<0 )
    fprintf( stderr,"   0x%p: %s\n",(void*)(uintptr_t)addr,filename );
  else if( lineno )
    fprintf( stderr,"   0x%p: %s:%d\n",(void*)(uintptr_t)addr,filename,lineno );
}
#endif


static void printStack( void **rets )
{
#if LEAKS_USE_DWST
  if( dwstOfProcess )
  {
    uint64_t addrs[LEAKS_PTRS];
    int addrc;
    for( addrc=0; addrc<LEAKS_PTRS && rets[addrc]; addrc++ )
      addrs[addrc] = (uintptr_t)rets[addrc] - 1;

    fprintf( stderr,"\n" );
    dwstOfProcess( addrs,addrc,&output_func,NULL );
    fprintf( stderr,"\n" );
  }
  else
#endif
  {
    int j;
    for( j=0; j<LEAKS_PTRS && rets[j]; j++ )
      fprintf( stderr,"; 0x%p",rets[j] );
    fprintf( stderr,"\n" );
  }
}


#ifdef LEAKS_CAPTURE_STACKTRACE
#define GET_STACKTRACE \
  do { \
    int count = CaptureStackBackTrace( 1,LEAKS_PTRS,rets,NULL ); \
    if( count ) rets[count-1] = NULL; \
  } while( 0 );
#else
#define RET_ADDR(i) \
  rets[i] = retp = retp ? __builtin_return_address(i) : NULL;
#define GET_STACKTRACE \
  void *retp = (void*)1; REPEATER(RET_ADDR)
#endif

#define ALLOC(f,p,s) \
  do { \
    if( !inited ) break; \
    MUTEX_LOCK(); \
    if( alloc_q>=alloc_s ) \
    { \
      alloc_s += 128; \
      allocation *alloc_an = (allocation*)__real_realloc( \
          alloc_a,alloc_s*sizeof(allocation) ); \
      if( !alloc_an ) \
      { \
        __real_free( alloc_a ); \
        fprintf( stderr,"LEAK DETECTION ERROR: couldn't allocate %" \
            PRIuPTR " bytes",alloc_s*sizeof(allocation) ); \
        alloc_a = NULL; \
        alloc_q = alloc_s = 0; \
        inited = 0; \
        MUTEX_UNLOCK(); \
        break; \
      } \
      alloc_a = alloc_an; \
    } \
    allocation *a = alloc_a + alloc_q; \
    alloc_q++; \
    a->ptr = p; \
    a->size = s; \
    a->flag = f; \
    void **rets = a->rets; \
    GET_STACKTRACE \
    MUTEX_UNLOCK(); \
  } while( 0 )
#define FREE(f,p) \
  do { \
    if( !inited ) break; \
    MUTEX_LOCK(); \
    int i; \
    for( i=alloc_q-1; i>=0 && alloc_a[i].ptr!=p; i-- ); \
    if( i>=0 ) \
    { \
      alloc_q--; \
      if( i<alloc_q ) alloc_a[i] = alloc_a[alloc_q]; \
      MUTEX_UNLOCK(); \
      break; \
    } \
    void *rets[LEAKS_PTRS]; \
    GET_STACKTRACE \
    fprintf( stderr,"%c:",f ); \
    printStack( rets ); \
    MUTEX_UNLOCK(); \
  } while( 0 )


#ifdef __cplusplus
extern "C" {
#endif

void *__real_malloc( size_t );
void *__real_calloc( size_t,size_t );
void __real_free( void* );
void *__real_realloc( void*,size_t );
char *__real_strdup( const char* );
wchar_t *__real_wcsdup( const wchar_t* );
void __real___main( void );


void *__wrap_malloc( size_t s )
{
  void *b = __real_malloc( s );
  if( b )
    ALLOC( 'a',b,s );
  return( b );
}

void *__wrap_calloc( size_t n,size_t s )
{
  void *b = __real_calloc( n,s );
  if( b )
    ALLOC( 'a',b,n*s );
  return( b );
}

void __wrap_free( void *b )
{
  if( b )
    FREE( 'f',b );
  __real_free( b );
}

void *__wrap_realloc( void *b,size_t s )
{
  if( b )
    FREE( 'f',b );
  b = __real_realloc( b,s );
  if( b )
    ALLOC( 'a',b,s );
  return( b );
}

char *__wrap_strdup( const char *s )
{
  char *b = __real_strdup( s );
  if( b )
    ALLOC( 'a',b,strlen(b)+1 );
  return( b );
}

wchar_t *__wrap_wcsdup( const wchar_t *s )
{
  wchar_t *b = __real_wcsdup( s );
  if( b )
    ALLOC( 'a',b,(wcslen(b)+1)*2 );
  return( b );
}


static void endfunc( void )
{
  size_t sumSize = 0;
  int sumNmb = 0;

#ifdef LEAKS_EXTRA_FREE
  LEAKS_EXTRA_FREE;
#endif

#ifdef LEAKS_THREAD
  CloseHandle( allocMutex );
#endif

  int i;
  for( i=0; i<alloc_q; i++ )
  {
    allocation a = alloc_a[i];

    char flag = a.flag;
    if( !flag ) continue;

    size_t size = a.size;
    void **rets = a.rets;
    int nmb = 1;

    int j;
    for( j=i+1; j<alloc_q; j++ )
    {
      allocation aCmp = alloc_a[j];

      if( flag!=aCmp.flag ||
          memcmp(rets,aCmp.rets,LEAKS_PTRS*sizeof(void*)) )
        continue;

      size += aCmp.size;
      nmb++;

      alloc_a[j].flag = 0;
      j--;
    }

    fprintf( stderr,"%c: %" PRIuPTR " B / %d",flag,size,nmb );

    printStack( rets );

    sumSize += size;
    sumNmb += nmb;
  }

  fprintf( stderr,"leaked: %" PRIuPTR " B / %d\n",sumSize,sumNmb );

#if LEAKS_USE_DWST
  if( dwstMod )
    FreeLibrary( dwstMod );
#endif
}

void __wrap___main( void )
{
  if( !inited && stderr->_file>=0 )
  {
    atexit( &endfunc );
    inited = 1;

#ifdef LEAKS_THREAD
    allocMutex = CreateMutex( NULL,FALSE,NULL );
#endif

#ifdef LEAKS_EXTRA_INIT
    LEAKS_EXTRA_INIT;
#endif

#if LEAKS_USE_DWST
    dwstMod = LoadLibraryA( "dwarfstack.dll" );
    if( dwstMod )
      dwstOfProcess = (dwstOfProcess_func*)GetProcAddress(
          dwstMod,"dwstOfProcess" );
#endif
  }

  __real___main();
}

#ifdef __cplusplus
};
#endif


#ifdef __cplusplus
void *operator new( size_t s )
{
  void *b = __real_malloc( s );
  if( b )
    ALLOC( 'n',b,s );
  return( b );
}

void *operator new[]( size_t s )
{
  void *b = __real_malloc( s );
  if( b )
    ALLOC( 'n',b,s );
  return( b );
}

void operator delete( void *b )
{
  if( b )
    FREE( 'd',b );
  __real_free( b );
}

void operator delete[]( void *b )
{
  if( b )
    FREE( 'd',b );
  __real_free( b );
}
#endif
