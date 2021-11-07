/*
 * Copyright (C) 2013-2021 Hannes Domani
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */


#include "dwarfstack.h"

#include "dwarf_pe.h"

#include <stdlib.h>
#include <string.h>


typedef void ChildWalker( Dwarf_Debug dbg,Dwarf_Die die,void *context );

// call walkFunc() recursively for every child DIE
static void walkChilds( Dwarf_Debug dbg,Dwarf_Die die,
    ChildWalker *walkFunc,void *context )
{
  Dwarf_Die child;
  if( dwarf_child(die,&child,NULL)!=DW_DLV_OK )
    return;

  while( 1 )
  {
    walkChilds( dbg,child,walkFunc,context );
    walkFunc( dbg,child,context );

    Dwarf_Die next_child;
    int res = dwarf_siblingof_b( dbg,child,1,&next_child,NULL );

    dwarf_dealloc( dbg,child,DW_DLA_DIE );

    if( res!=DW_DLV_OK ) break;
    child = next_child;
  }
}


// get Dwarf_Ranges of specified DIE
static int dwarf_ranges( Dwarf_Debug dbg,Dwarf_Die die,
    Dwarf_Ranges **ranges,Dwarf_Signed *rangeCount )
{
  Dwarf_Attribute range_attr;
  int res = dwarf_attr( die,DW_AT_ranges,&range_attr,NULL );
  if( res!=DW_DLV_OK ) return( res );

  Dwarf_Off range_off;
  res = dwarf_global_formref( range_attr,&range_off,NULL );

  if( res==DW_DLV_OK )
    res = dwarf_get_ranges_b( dbg,range_off,die,NULL,
        ranges,rangeCount,NULL,NULL );

  dwarf_dealloc( dbg,range_attr,DW_DLA_ATTR );

  return( res );
}

// get low_pc and high_pc of specified DIE
static int dwarf_lowhighpc( Dwarf_Die die,
    Dwarf_Addr *low,Dwarf_Addr *high )
{
  int res = dwarf_lowpc( die,low,NULL );
  if( res!=DW_DLV_OK ) return( res );

  Dwarf_Half form;
  enum Dwarf_Form_Class class;
  res = dwarf_highpc_b( die,high,&form,&class,NULL );
  if( res!=DW_DLV_OK )
  {
    *high = 0;
    return( DW_DLV_OK );
  }

  if( class!=DW_FORM_CLASS_ADDRESS )
    *high += *low;

  return( res );
}

// get DIE by reference attribute
static int dwarf_die_by_ref( Dwarf_Debug dbg,Dwarf_Die die,
    Dwarf_Half attr,Dwarf_Die *return_die )
{
  Dwarf_Attribute ref_attr;
  int res = dwarf_attr( die,attr,&ref_attr,NULL );
  if( res!=DW_DLV_OK ) return( res );

  Dwarf_Off ref_off;
  res = dwarf_global_formref( ref_attr,&ref_off,NULL );
  if( res==DW_DLV_OK )
    res = dwarf_offdie_b( dbg,ref_off,1,return_die,NULL );

  dwarf_dealloc( dbg,ref_attr,DW_DLA_ATTR );

  return( res );
}

#ifdef DWST_SHARED
char *__cxa_demangle( const char*,char*,size_t*,int* );
Dwarf_Ptr _dwarf_get_alloc( Dwarf_Debug,Dwarf_Small,Dwarf_Unsigned );


size_t dwstDemangle(
    const char *mangled,
    char *demangled,size_t length )
{
  if( !mangled || mangled[0]!='_' || mangled[1]!='Z' ||
      !demangled || !length )
    return( 0 );

  char *d = __cxa_demangle( mangled,NULL,NULL,NULL );
  if( !d ) return( 0 );

  strncpy( demangled,d,length );
  length = strlen( d );
  free( d );

  return( length );
}
#endif

// get function name of specified DIE
static int dwarf_name_of_func( Dwarf_Debug dbg,Dwarf_Die die,
    char **funcname )
{
  *funcname = NULL;
  char *local_funcname;
  int res;

#ifdef DWST_SHARED
  Dwarf_Attribute linkage_attr;
  if( dwarf_attr(die,DW_AT_linkage_name,&linkage_attr,NULL)==DW_DLV_OK ||
      dwarf_attr(die,DW_AT_MIPS_linkage_name,&linkage_attr,NULL)==DW_DLV_OK )
  {
    res = dwarf_formstring( linkage_attr,&local_funcname,NULL );

    dwarf_dealloc( dbg,linkage_attr,DW_DLA_ATTR );

    if( res==DW_DLV_OK )
    {
      char *demangled = __cxa_demangle( local_funcname,NULL,NULL,NULL );

      dwarf_dealloc( dbg,local_funcname,DW_DLA_STRING );

      if( demangled )
      {
        *funcname = _dwarf_get_alloc( dbg,DW_DLA_STRING,strlen(demangled)+1 );
        if( *funcname )
          strcpy( *funcname,demangled );

        free( demangled );

        if( *funcname )
          return( DW_DLV_OK );
      }
    }
  }
#else
  (void)dbg;
#endif

  res = dwarf_diename( die,&local_funcname,NULL );
  if( res!=DW_DLV_OK ) return( res );

  *funcname = local_funcname;

  return( DW_DLV_OK );
}

static char *dwarf_name_of_func_linked( Dwarf_Debug dbg,Dwarf_Die die )
{
  char *funcname = NULL;
  if( dwarf_name_of_func(dbg,die,&funcname)==DW_DLV_OK )
    return( funcname );

  Dwarf_Die link;
  if( dwarf_die_by_ref(dbg,die,DW_AT_abstract_origin,&link)==DW_DLV_OK ||
      dwarf_die_by_ref(dbg,die,DW_AT_specification,&link)==DW_DLV_OK )
  {
    funcname = dwarf_name_of_func_linked( dbg,link );

    dwarf_dealloc( dbg,link,DW_DLA_DIE );
  }

  return( funcname );
}


static void dwarf_callback(
    dwstCallback *callbackFunc,dwstCallbackW *callbackFuncW,
    uint64_t addr,const char *filename,const wchar_t *filenameW,
    int lineno,const char *funcname,void *context,int columnno )
{
  if( callbackFunc )
  {
    char *ansi = NULL;
    if( !filename )
      filename = ansi = dwst_wide2ansi( filenameW );
    callbackFunc( addr,filename,lineno,funcname,context,columnno );
    free( ansi );
  }
  else
  {
    wchar_t *wide = NULL;
    if( !filenameW )
      filenameW = wide = dwst_ansi2wide( filename );
    callbackFuncW( addr,filenameW,lineno,funcname,context,columnno );
    free( wide );
  }
}


typedef struct inline_info
{
  Dwarf_Addr ptr,low;
  char **files;
  int fileCount;
  dwstCallback *callbackFunc;
  dwstCallbackW *callbackFuncW;
  void *callbackContext;
  uint64_t ptrOrig;
  int fileno,lineno,columnno;
} inline_info;

// find the calling-location of inlined functions
static void findInlined( Dwarf_Debug dbg,Dwarf_Die die,inline_info *cuInfo )
{
  Dwarf_Half tag;
  if( dwarf_tag(die,&tag,NULL)!=DW_DLV_OK ||
      (tag!=DW_TAG_inlined_subroutine &&
       tag!=DW_TAG_subprogram) )
    return;

  Dwarf_Addr low,high;
  if( dwarf_lowhighpc(die,&low,&high)==DW_DLV_OK && high )
  {
    if( cuInfo->ptr<low || cuInfo->ptr>=high )
      return;
  }
  else
  {
    Dwarf_Signed rangeCount;
    Dwarf_Ranges *ranges;
    if( dwarf_ranges(dbg,die,&ranges,&rangeCount)!=DW_DLV_OK )
      return;

    int i;
    Dwarf_Addr base = cuInfo->low;
    for( i=0; i<rangeCount; i++ )
    {
      Dwarf_Ranges *range = ranges + i;
      if( range->dwr_type==DW_RANGES_END ) continue;

      low = range->dwr_addr1;
      high = range->dwr_addr2;

      if( range->dwr_type==DW_RANGES_ENTRY )
      {
        low += base;
        high += base;
      }
      else
      {
        base = high;
        continue;
      }

      if( cuInfo->ptr<low || cuInfo->ptr>=high )
        continue;

      break;
    }

    dwarf_dealloc_ranges( dbg,ranges,rangeCount );

    if( i>=rangeCount ) return;
  }

  if( tag==DW_TAG_subprogram )
  {
    char *funcname = dwarf_name_of_func_linked( dbg,die );

    dwarf_callback( cuInfo->callbackFunc,cuInfo->callbackFuncW,
        cuInfo->ptrOrig,cuInfo->files[cuInfo->fileno-1],NULL,
        cuInfo->lineno,funcname,cuInfo->callbackContext,cuInfo->columnno );

    if( funcname )
      dwarf_dealloc( dbg,funcname,DW_DLA_STRING );
    return;
  }

  Dwarf_Attribute callfile;
  if( dwarf_attr(die,DW_AT_call_file,&callfile,NULL)!=DW_DLV_OK )
    return;

  Dwarf_Attribute callline;
  if( dwarf_attr(die,DW_AT_call_line,&callline,NULL)!=DW_DLV_OK )
  {
    dwarf_dealloc( dbg,callfile,DW_DLA_ATTR );
    return;
  }

  Dwarf_Unsigned fileno,lineno;
  if( dwarf_formudata(callfile,&fileno,NULL)==DW_DLV_OK &&
      dwarf_formudata(callline,&lineno,NULL)==DW_DLV_OK &&
      (int)fileno<=cuInfo->fileCount )
  {
    char *funcname = dwarf_name_of_func_linked( dbg,die );

    dwarf_callback( cuInfo->callbackFunc,cuInfo->callbackFuncW,
        cuInfo->ptrOrig,cuInfo->files[cuInfo->fileno-1],NULL,
        cuInfo->lineno,funcname,cuInfo->callbackContext,cuInfo->columnno );

    cuInfo->fileno = fileno;
    cuInfo->lineno = lineno;
    cuInfo->columnno = 0;
    cuInfo->ptrOrig = 0;

    Dwarf_Attribute callcolumn;
    if( dwarf_attr(die,DW_AT_call_column,&callcolumn,NULL)==DW_DLV_OK )
    {
      Dwarf_Unsigned columnno;
      if( dwarf_formudata(callcolumn,&columnno,NULL)==DW_DLV_OK )
        cuInfo->columnno = columnno;

      dwarf_dealloc( dbg,callcolumn,DW_DLA_ATTR );
    }

    if( funcname )
      dwarf_dealloc( dbg,funcname,DW_DLA_STRING );
  }

  dwarf_dealloc( dbg,callfile,DW_DLA_ATTR );
  dwarf_dealloc( dbg,callline,DW_DLA_ATTR );
}


typedef struct cu_info
{
  Dwarf_Off offs;
  Dwarf_Addr low,high;
  Dwarf_Line *lines;
  Dwarf_Signed lineCount;
  Dwarf_Line_Context lineContext;
  char **files;
  Dwarf_Signed fileCount;
} cu_info;

int dwstOfFileExt(
    const char *name,const wchar_t *nameW,uint64_t imageBase,
    uint64_t *addr,int count,
    dwstCallback *callbackFunc,dwstCallbackW *callbackFuncW,
    void *callbackContext )
{
  if( !nameW || !addr || !count || (!callbackFunc && !callbackFuncW) )
    return( 0 );

  if( imageBase )
    dwarf_callback( callbackFunc,callbackFuncW,imageBase,name,nameW,
        DWST_BASE_ADDR,NULL,callbackContext,0 );

  Dwarf_Addr imageBase_dbg;
  Dwarf_Debug dbg;
  if( dwarf_pe_init(nameW,&imageBase_dbg,0,0,&dbg,NULL)!=DW_DLV_OK )
  {
    int i;
    for( i=0; i<count; i++ )
      dwarf_callback( callbackFunc,callbackFuncW,addr[i],name,nameW,
          DWST_NO_DBG_SYM,NULL,callbackContext,0 );

    return( count );
  }

  cu_info *cuArr = NULL;
  int cuQty = 0;
  Dwarf_Addr lowestLow = 0;
  while( 1 )
  {
    Dwarf_Unsigned next_cu_header;
    if( dwarf_next_cu_header_d(dbg,1,NULL,NULL,NULL,NULL,NULL,
          NULL,NULL,NULL,&next_cu_header,NULL,NULL)!=DW_DLV_OK )
      break;

    Dwarf_Die die;
    if( dwarf_siblingof_b(dbg,0,1,&die,NULL)!=DW_DLV_OK )
      continue;

    {
      cu_info *newArr = realloc( cuArr,(cuQty+1)*sizeof(cu_info) );
      if( !newArr )
      {
        dwarf_dealloc( dbg,die,DW_DLA_DIE );
        break;
      }
      cuQty++;
      cuArr = newArr;
    }

    cu_info *cuInfo = &cuArr[cuQty-1];

    if( dwarf_dieoffset(die,&cuInfo->offs,NULL)!=DW_DLV_OK )
      cuInfo->offs = 0;

    int res = dwarf_lowhighpc( die,&cuInfo->low,&cuInfo->high );
    if( res==DW_DLV_OK && cuInfo->low &&
        (!lowestLow || cuInfo->low<lowestLow) )
      lowestLow = cuInfo->low;
    if( res!=DW_DLV_OK || !cuInfo->high )
    {
      int hasLow = res==DW_DLV_OK;
      if( !hasLow ) cuInfo->low = 0;
      cuInfo->high = 0;

      Dwarf_Signed rangeCount;
      Dwarf_Ranges *ranges;
      if( dwarf_ranges(dbg,die,&ranges,&rangeCount)==DW_DLV_OK )
      {
        int i;
        Dwarf_Addr base = 0;
        for( i=0; i<rangeCount; i++ )
        {
          Dwarf_Ranges *range = ranges + i;
          if( range->dwr_type==DW_RANGES_END ) continue;

          Dwarf_Addr low = range->dwr_addr1;
          Dwarf_Addr high = range->dwr_addr2;

          if( range->dwr_type==DW_RANGES_ENTRY )
          {
            low += base;
            high += base;
          }
          else
          {
            base = high;
            continue;
          }

          if( !low ) continue;

          if( !hasLow || low<cuInfo->low )
          {
            cuInfo->low = low;
            hasLow = 1;
          }
          if( high>cuInfo->high )
            cuInfo->high = high;
        }

        dwarf_dealloc_ranges( dbg,ranges,rangeCount );
      }
    }

    cuInfo->lines = NULL;
    cuInfo->lineCount = -1;
    cuInfo->lineContext = NULL;
    cuInfo->files = NULL;
    cuInfo->fileCount = -1;

    dwarf_dealloc( dbg,die,DW_DLA_DIE );
  }

  uint64_t baseOffs = 0;
  if( imageBase )
  {
    if( imageBase_dbg )
      baseOffs = imageBase_dbg - imageBase;
  }

  int i,j;
  for( i=0; i<count; i++ )
  {
    uint64_t ptrOrig = addr[i];
    uint64_t ptr = ptrOrig + baseOffs;

    int found_ptr = 0;
    for( j=0; j<cuQty; j++ )
    {
      cu_info *cuInfo = &cuArr[j];
      if( ptr<cuInfo->low || ptr>=cuInfo->high ) continue;

      Dwarf_Die die;
      if( cuInfo->offs &&
          dwarf_offdie_b(dbg,cuArr[j].offs,1,&die,NULL)==DW_DLV_OK )
      {
        Dwarf_Line *lines = cuInfo->lines;
        Dwarf_Signed lineCount = cuInfo->lineCount;
        if( lineCount<0 )
        {
          Dwarf_Unsigned lineVersion = 0;
          Dwarf_Small tableCount = 0;
          Dwarf_Line_Context lineContext = NULL;
          if( dwarf_srclines_b(die,&lineVersion,
                &tableCount,&lineContext,NULL)!=DW_DLV_OK ||
              tableCount!=1 ||
              dwarf_srclines_from_linecontext(lineContext,
                &lines,&lineCount,NULL)!=DW_DLV_OK )
          {
            dwarf_srclines_dealloc_b( lineContext );
            lines = NULL;
            lineCount = 0;
            lineContext = NULL;
          }
          cuInfo->lines = lines;
          cuInfo->lineCount = lineCount;
          cuInfo->lineContext = lineContext;
        }

        Dwarf_Unsigned srcfileno = 0;
        Dwarf_Unsigned lineno = 0;
        Dwarf_Unsigned columnno = 0;
        if( lines )
        {
          int c;
          int onEnd = 1;
          Dwarf_Addr prevAdd = 0;
          for( c=0; c<lineCount; c++ )
          {
            Dwarf_Addr add;
            if( dwarf_lineaddr(lines[c],&add,NULL)!=DW_DLV_OK )
              break;

            if( onEnd || add<=ptr || prevAdd>ptr )
            {
              Dwarf_Bool endsequ;
              if( dwarf_lineendsequence(lines[c],&endsequ,NULL)!=DW_DLV_OK )
                break;

              onEnd = endsequ;
              prevAdd = add;
              continue;
            }

            dwarf_line_srcfileno( lines[c-1],&srcfileno,NULL );
            dwarf_lineno( lines[c-1],&lineno,NULL );
            dwarf_lineoff_b( lines[c-1],&columnno,NULL );
            break;
          }
        }

        char **files = cuInfo->files;
        Dwarf_Signed fileCount = cuInfo->fileCount;
        if( srcfileno && lineno && fileCount<0 )
        {
          if( dwarf_srcfiles(die,&files,&fileCount,NULL)!=DW_DLV_OK )
          {
            files = NULL;
            fileCount = 0;
          }
          cuInfo->files = files;
          cuInfo->fileCount = fileCount;
        }

        if( srcfileno && lineno && files )
        {
          found_ptr = 1;

          if( srcfileno<=(Dwarf_Unsigned)fileCount )
          {
            inline_info ii = { ptr,cuInfo->low,
              files,fileCount,callbackFunc,callbackFuncW,callbackContext,
              ptrOrig,srcfileno,lineno,columnno };
            walkChilds( dbg,die,(ChildWalker*)findInlined,&ii );
          }
          else
            dwarf_callback( callbackFunc,callbackFuncW,ptrOrig,name,nameW,
                DWST_NO_SRC_FILE,NULL,callbackContext,0 );
        }

        dwarf_dealloc( dbg,die,DW_DLA_DIE );
      }

      if( found_ptr ) break;
    }

    if( !found_ptr )
      dwarf_callback( callbackFunc,callbackFuncW,ptrOrig,name,nameW,
          DWST_NOT_FOUND,NULL,callbackContext,0 );
  }

  for( j=0; j<cuQty; j++ )
  {
    if( cuArr[j].lines )
      dwarf_srclines_dealloc_b( cuArr[j].lineContext );

    if( cuArr[j].files )
    {
      char **files = cuArr[j].files;
      int fileCount = cuArr[j].fileCount;
      int fc;
      for( fc=0; fc<fileCount; fc++ )
        dwarf_dealloc( dbg,files[fc],DW_DLA_STRING );

      dwarf_dealloc( dbg,files,DW_DLA_LIST );
    }
  }
  free( cuArr );

  dwarf_pe_finish( dbg,NULL );

  return( i );
}

int dwstOfFile(
    const char *name,uint64_t imageBase,
    uint64_t *addr,int count,
    dwstCallback *callbackFunc,void *callbackContext )
{
  wchar_t *nameW = dwst_ansi2wide( name );
  int ret = dwstOfFileExt( name,nameW,imageBase,addr,count,
      callbackFunc,NULL,callbackContext );
  free( nameW );
  return( ret );
}

int dwstOfFileW(
    const wchar_t *name,uint64_t imageBase,
    uint64_t *addr,int count,
    dwstCallbackW *callbackFunc,void *callbackContext )
{
  return( dwstOfFileExt(NULL,name,imageBase,addr,count,
        NULL,callbackFunc,callbackContext) );
}
