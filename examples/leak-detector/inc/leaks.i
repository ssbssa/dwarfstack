
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
 * LEAKS_PROTECT
 *      detect invalid access before/after allocated memory block
 *      0 -> off
 *      1 -> check access after allocated block
 *      2 -> check access before allocated block
 *      (default 1)
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

#ifndef LEAKS_PROTECT
#define LEAKS_PROTECT 1
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
static CRITICAL_SECTION allocMutex;
#define MUTEX_LOCK() EnterCriticalSection( &allocMutex )
#define MUTEX_UNLOCK() LeaveCriticalSection( &allocMutex )
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
typedef int dwstOfException_func(
    void *context,
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


#if LEAKS_PROTECT
static DWORD pageSize = 0;
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

#if !LEAKS_PROTECT
#define l_malloc   __real_malloc
#define l_calloc   __real_calloc
#define l_free     __real_free
#define l_realloc  __real_realloc
#define l_strdup   __real_strdup
#define l_wcsdup   __real_wcsdup
#else
static size_t get_alloc_size( void *p )
{
  MUTEX_LOCK();
  int i;
  for( i=alloc_q-1; i>=0 && alloc_a[i].ptr!=p; i-- );
  size_t s = i>=0 ? alloc_a[i].size : 0;
  MUTEX_UNLOCK();
  return( s );
}

static void *protect_alloc_m( size_t s )
{
  if( !pageSize )
  {
    SYSTEM_INFO si;
    GetSystemInfo( &si );
    pageSize = si.dwPageSize;
  }

  if( !s ) return( NULL );

  size_t pages = ( s-1 )/pageSize + 2;

  unsigned char *b = (unsigned char*)VirtualAlloc(
      NULL,pages*pageSize,MEM_RESERVE,PAGE_NOACCESS );
  if( !b ) return( NULL );

#if LEAKS_PROTECT>1
  b += pageSize;
#endif

  VirtualAlloc( b,(pages-1)*pageSize,MEM_COMMIT,PAGE_READWRITE );

#if LEAKS_PROTECT==1
  b += ( pageSize - (s%pageSize) )%pageSize;
#endif

  return( b );
}
static void protect_free_m( void *b )
{
  if( !b ) return;

  size_t s = get_alloc_size( b );
  if( !s ) return;

  size_t pages = ( s-1 )/pageSize + 2;

  uintptr_t p = (uintptr_t)b;
#if LEAKS_PROTECT==1
  p -= p%pageSize;
#else
  p -= pageSize;
#endif
  b = (void*)p;

  VirtualFree( b,pages*pageSize,MEM_DECOMMIT );
  VirtualFree( b,0,MEM_RELEASE );
}

static void *protect_malloc( size_t s )
{
  void *b = protect_alloc_m( s );
  if( !b ) return( NULL );

  memset( b,0xcc,s );

  return( b );
}
static void *protect_calloc( size_t n,size_t s )
{
  return( protect_alloc_m(n*s) );
}
static void protect_free( void *b )
{
  protect_free_m( b );
}
static void *protect_realloc( void *b,size_t s )
{
  if( !s )
  {
    protect_free( b );
    return( NULL );
  }

  if( !b )
    return( protect_malloc(s) );

  size_t os = get_alloc_size( b );
  if( !os ) return( NULL );

  void *nb = protect_malloc( s );
  if( !nb ) return( NULL );

  memcpy( nb,b,os<s?os:s );

  protect_free( b );

  return( nb );
}
static char *protect_strdup( const char *s )
{
  size_t l = strlen( s ) + 1;

  char *b = (char*)protect_alloc_m( l );
  if( !b ) return( NULL );

  memcpy( b,s,l );

  return( b );
}
static wchar_t *protect_wcsdup( const wchar_t *s )
{
  size_t l = ( wcslen( s ) + 1 )*2;

  wchar_t *b = (wchar_t*)protect_alloc_m( l );
  if( !b ) return( NULL );

  memcpy( b,s,l );

  return( b );
}

#define l_malloc   protect_malloc
#define l_calloc   protect_calloc
#define l_free     protect_free
#define l_realloc  protect_realloc
#define l_strdup   protect_strdup
#define l_wcsdup   protect_wcsdup
#endif


void *__wrap_malloc( size_t s )
{
  void *b = l_malloc( s );
  if( b )
    ALLOC( 'a',b,s );
  return( b );
}

void *__wrap_calloc( size_t n,size_t s )
{
  void *b = l_calloc( n,s );
  if( b )
    ALLOC( 'a',b,n*s );
  return( b );
}

void __wrap_free( void *b )
{
  l_free( b );
  if( b )
    FREE( 'f',b );
}

void *__wrap_realloc( void *b,size_t s )
{
  void *nb = l_realloc( b,s );
  if( b )
    FREE( 'f',b );
  if( nb )
    ALLOC( 'a',nb,s );
  return( nb );
}

char *__wrap_strdup( const char *s )
{
  char *b = l_strdup( s );
  if( b )
    ALLOC( 'a',b,strlen(b)+1 );
  return( b );
}

wchar_t *__wrap_wcsdup( const wchar_t *s )
{
  wchar_t *b = l_wcsdup( s );
  if( b )
    ALLOC( 'a',b,(wcslen(b)+1)*2 );
  return( b );
}


#if LEAKS_PROTECT
#ifdef _WIN64
#define cip Rip
#else
#define cip Eip
#endif

static LONG WINAPI exceptionWalker( LPEXCEPTION_POINTERS ep )
{
  fprintf( stderr,"exception code: 0x%08lX",
      ep->ExceptionRecord->ExceptionCode );

  const char *desc = NULL;
  switch( ep->ExceptionRecord->ExceptionCode )
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
  if( desc )
    fprintf( stderr,"%s",desc );

  fprintf( stderr,"\nexception on:" );

  dwstOfException_func *dwstOfException = NULL;
  if( dwstMod )
  {
    dwstOfException = (dwstOfException_func*)GetProcAddress(
        dwstMod,"dwstOfException" );
  }
  if( dwstOfException )
  {
    fprintf( stderr,"\n" );
    dwstOfException( ep->ContextRecord,output_func,NULL );
  }
  else
    fprintf( stderr," 0x%p\n",(void*)ep->ContextRecord->cip );

  if( ep->ExceptionRecord->ExceptionCode==EXCEPTION_ACCESS_VIOLATION &&
      ep->ExceptionRecord->NumberParameters==2 )
  {
    ULONG_PTR flag = ep->ExceptionRecord->ExceptionInformation[0];
    char *addr = (char*)ep->ExceptionRecord->ExceptionInformation[1];
    fprintf( stderr,"\n%s violation at 0x%p\n",
        flag==8?"data execution prevention":
        (flag?"write access":"read access"),addr );

    MUTEX_LOCK();
    int i;
    for( i=alloc_q-1; i>=0; i-- )
    {
      allocation a = alloc_a[i];

      char *ptr = (char*)a.ptr;
      size_t size = a.size;

#if LEAKS_PROTECT==1
      char *noAccessStart = ptr + size;
      char *noAccessEnd = noAccessStart + pageSize;
#else
      char *noAccessStart = ptr - pageSize;
      char *noAccessEnd = ptr;
#endif

      if( addr>=noAccessStart && addr<noAccessEnd )
      {
        fprintf( stderr,"maybe intended for 0x%p (size %"
            PRIuPTR ", offset "
#if LEAKS_PROTECT==1
            "+"
#endif
            "%" PRIdPTR ")",
            ptr,size,addr-ptr );
        printStack( a.rets );
        break;
      }
    }
    MUTEX_UNLOCK();
  }

  fflush( stderr );

  inited = 2;

  return( EXCEPTION_EXECUTE_HANDLER );
}
#endif

static void endfunc( void )
{
  size_t sumSize = 0;
  int sumNmb = 0;

#ifdef LEAKS_EXTRA_FREE
  LEAKS_EXTRA_FREE;
#endif

  MUTEX_LOCK();
  if( inited==2 ) alloc_q = 0;
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
  __real_free( alloc_a );
  alloc_a = NULL;
  alloc_q = alloc_s = 0;
  MUTEX_UNLOCK();

  if( inited!=2 )
    fprintf( stderr,"leaked: %" PRIuPTR " B / %d\n",sumSize,sumNmb );

#ifdef LEAKS_THREAD
  DeleteCriticalSection( &allocMutex );
#endif

#if LEAKS_USE_DWST
  if( dwstMod )
    FreeLibrary( dwstMod );
#endif
}

void __wrap___main( void )
{
  int doinit = 0;

  if( stderr->_file<0 )
    freopen( "leak-detector.txt","w",stderr );

  if( !inited && stderr->_file>=0 )
  {
    atexit( &endfunc );
    inited = 1;
    doinit = 1;
  }

#if LEAKS_PROTECT
  if( !inited )
  {
    atexit( &endfunc );
    inited = 2;
    doinit = 1;
  }
#endif

  if( doinit )
  {
#ifdef LEAKS_THREAD
    InitializeCriticalSection( &allocMutex );
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

#if LEAKS_PROTECT
    SetUnhandledExceptionFilter( exceptionWalker );
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
  void *b = l_malloc( s );
  if( b )
    ALLOC( 'n',b,s );
  return( b );
}

void *operator new[]( size_t s )
{
  void *b = l_malloc( s );
  if( b )
    ALLOC( 'n',b,s );
  return( b );
}

void operator delete( void *b )
{
  l_free( b );
  if( b )
    FREE( 'd',b );
}

void operator delete[]( void *b )
{
  l_free( b );
  if( b )
    FREE( 'd',b );
}
#endif
