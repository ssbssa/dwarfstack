/*
 * Copyright (C) 2013 Hannes Domani
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
    int res = dwarf_siblingof( dbg,child,&next_child,NULL );

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
  res = dwarf_formudata( range_attr,&range_off,NULL );

  if( res==DW_DLV_OK )
    res = dwarf_get_ranges_a( dbg,range_off,die,ranges,rangeCount,NULL,NULL );

  dwarf_dealloc( dbg,range_attr,DW_DLA_ATTR );

  return( res );
}


typedef struct inline_info
{
  Dwarf_Addr ptr,low;
  char **files;
  int fileCount;
  dwstCallback *callbackFunc;
  void *callbackContext;
  int64_t baseOffs;
} inline_info;

// find the calling-location of inlined functions
static void findInlined( Dwarf_Debug dbg,Dwarf_Die die,inline_info *cuInfo )
{
  Dwarf_Half tag;
  if( dwarf_tag(die,&tag,NULL)!=DW_DLV_OK ||
      tag!=DW_TAG_inlined_subroutine )
    return;

  Dwarf_Addr low,high;
  if( dwarf_lowpc(die,&low,NULL)==DW_DLV_OK &&
      dwarf_highpc(die,&high,NULL)==DW_DLV_OK )
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
    for( i=0; i<rangeCount; i++ )
    {
      Dwarf_Ranges *range = ranges + i;
      if( range->dwr_type==DW_RANGES_END ) continue;

      low = range->dwr_addr1;
      high = range->dwr_addr2;

      if( range->dwr_type==DW_RANGES_ENTRY )
      {
        low += cuInfo->low;
        high += cuInfo->low;
      }

      if( cuInfo->ptr<low || cuInfo->ptr>=high )
        continue;

      break;
    }

    dwarf_ranges_dealloc( dbg,ranges,rangeCount );

    if( i>=rangeCount ) return;
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
      dwarf_formudata(callline,&lineno,NULL)==DW_DLV_OK )
  {
    if( (int)fileno<=cuInfo->fileCount )
      cuInfo->callbackFunc( cuInfo->ptr-cuInfo->baseOffs,
          cuInfo->files[fileno-1],lineno,cuInfo->callbackContext );
  }

  dwarf_dealloc( dbg,callfile,DW_DLA_ATTR );
  dwarf_dealloc( dbg,callline,DW_DLA_ATTR );
}


typedef struct cu_info
{
  Dwarf_Off offs;
  Dwarf_Addr low,high;
} cu_info;

int dwstOfFile(
    const char *name,uint64_t imageBase,
    uint64_t *addr,int count,
    dwstCallback *callbackFunc,void *callbackContext )
{
  if( !name || !addr || !count || !callbackFunc ) return( 0 );

  if( imageBase )
    callbackFunc( imageBase,name,DWST_BASE_ADDR,callbackContext );

  Dwarf_Addr imageBase_dbg;
  Dwarf_Debug dbg;
  if( dwarf_pe_init(name,&imageBase_dbg,0,0,&dbg,NULL)!=DW_DLV_OK )
  {
    int i;
    for( i=0; i<count; i++ )
      callbackFunc( addr[i],name,DWST_NO_DBG_SYM,callbackContext );

    return( count );
  }

  int64_t baseOffs = 0;
  if( imageBase && imageBase_dbg )
    baseOffs = imageBase_dbg - imageBase;

  cu_info *cuArr = NULL;
  int cuQty = 0;
  while( 1 )
  {
    Dwarf_Unsigned next_cu_header;
    if( dwarf_next_cu_header(dbg,NULL,NULL,NULL,NULL,
          &next_cu_header,NULL)!=DW_DLV_OK )
      break;

    Dwarf_Die die;
    if( dwarf_siblingof(dbg,0,&die,NULL)!=DW_DLV_OK )
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

    if( dwarf_lowpc(die,&cuInfo->low,NULL)!=DW_DLV_OK ||
        dwarf_highpc(die,&cuInfo->high,NULL)!=DW_DLV_OK )
    {
      cuInfo->low = cuInfo->high = 0;

      Dwarf_Signed rangeCount;
      Dwarf_Ranges *ranges;
      if( dwarf_ranges(dbg,die,&ranges,&rangeCount)==DW_DLV_OK )
      {
        int i;
        for( i=0; i<rangeCount; i++ )
        {
          Dwarf_Ranges *range = ranges + i;
          if( range->dwr_type==DW_RANGES_END ) continue;

          Dwarf_Addr low = range->dwr_addr1;
          Dwarf_Addr high = range->dwr_addr2;

          if( range->dwr_type==DW_RANGES_ENTRY )
          {
            low += cuInfo->low;
            high += cuInfo->low;
          }

          if( !cuInfo->low || low<cuInfo->low )
            cuInfo->low = low;
          if( high>cuInfo->high )
            cuInfo->high = high;
        }

        dwarf_ranges_dealloc( dbg,ranges,rangeCount );
      }
    }

    dwarf_dealloc( dbg,die,DW_DLA_DIE );
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
          dwarf_offdie(dbg,cuArr[j].offs,&die,NULL)==DW_DLV_OK )
      {
        Dwarf_Line *lines;
        Dwarf_Signed lineCount;
        Dwarf_Unsigned srcfileno = 0;
        Dwarf_Unsigned lineno = 0;
        if( dwarf_srclines(die,&lines,&lineCount,NULL)==DW_DLV_OK )
        {
          int c;
          int onEnd = 1;
          for( c=0; c<lineCount; c++ )
          {
            Dwarf_Addr add;
            if( dwarf_lineaddr(lines[c],&add,NULL)!=DW_DLV_OK )
              break;
            Dwarf_Bool endsequence;
            if( dwarf_lineendsequence(lines[c],&endsequence,NULL)!=DW_DLV_OK )
              break;

            if( add<=ptr )
            {
              onEnd = endsequence;
              continue;
            }

            if( onEnd ) break;

            dwarf_line_srcfileno( lines[c-1],&srcfileno,NULL );
            dwarf_lineno( lines[c-1],&lineno,NULL );
            break;
          }

          dwarf_srclines_dealloc( dbg,lines,lineCount );
        }

        char **files;
        Dwarf_Signed fileCount;
        if( srcfileno && lineno &&
            dwarf_srcfiles(die,&files,&fileCount,NULL)==DW_DLV_OK )
        {
          found_ptr = 1;

          if( srcfileno<=(Dwarf_Unsigned)fileCount )
            callbackFunc( ptrOrig,
                files[srcfileno-1],lineno,callbackContext );
          else
            callbackFunc( ptrOrig,
                name,DWST_NO_SRC_FILE,callbackContext );

          inline_info ii = { ptr,cuInfo->low,
            files,fileCount,callbackFunc,callbackContext,baseOffs };
          walkChilds( dbg,die,(ChildWalker*)findInlined,&ii );

          int fc;
          for( fc=0; fc<fileCount; fc++ )
            dwarf_dealloc( dbg,files[fc],DW_DLA_STRING );

          dwarf_dealloc( dbg,files,DW_DLA_LIST );
        }

        dwarf_dealloc( dbg,die,DW_DLA_DIE );
      }

      if( found_ptr ) break;
    }

    if( !found_ptr )
      callbackFunc( ptrOrig,
          name,DWST_NOT_FOUND,callbackContext );
  }

  free( cuArr );

  dwarf_pe_finish( dbg,NULL );

  return( i );
}
