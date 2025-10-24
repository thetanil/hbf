/* SPDX-License-Identifier: Public Domain */
/*
** 2017-12-17
**
** The author disclaims copyright to this source code.  In place of
** a legal notice, here is a blessing:
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
**
******************************************************************************
**
** Utility functions sqlar_compress() and sqlar_uncompress(). Useful
** for working with sqlar archives.
**
** Extracted from SQLite shell.c for use in HBF.
*/
#include "sqlar.h"
#include <zlib.h>
#include <assert.h>

/*
** Implementation of the "sqlar_compress(X)" SQL function.
*/
static void sqlarCompressFunc(
  sqlite3_context *context,
  int argc,
  sqlite3_value **argv
){
  assert( argc==1 );
  if( sqlite3_value_type(argv[0])==SQLITE_BLOB ){
    const Bytef *pData = sqlite3_value_blob(argv[0]);
    uLong nData = sqlite3_value_bytes(argv[0]);
    uLongf nOut = compressBound(nData);
    Bytef *pOut;

    pOut = (Bytef*)sqlite3_malloc(nOut);
    if( pOut==0 ){
      sqlite3_result_error_nomem(context);
      return;
    }else{
      if( Z_OK!=compress(pOut, &nOut, pData, nData) ){
        sqlite3_result_error(context, "error in compress()", -1);
      }else if( nOut<nData ){
        sqlite3_result_blob(context, pOut, nOut, SQLITE_TRANSIENT);
      }else{
        sqlite3_result_value(context, argv[0]);
      }
      sqlite3_free(pOut);
    }
  }else{
    sqlite3_result_value(context, argv[0]);
  }
}

/*
** Implementation of the "sqlar_uncompress(X,SZ)" SQL function
*/
static void sqlarUncompressFunc(
  sqlite3_context *context,
  int argc,
  sqlite3_value **argv
){
  uLong nData;
  sqlite3_int64 sz;

  assert( argc==2 );
  sz = sqlite3_value_int(argv[1]);

  if( sz<=0 || sz==(nData = sqlite3_value_bytes(argv[0])) ){
    sqlite3_result_value(context, argv[0]);
  }else{
    uLongf szf = sz;
    const Bytef *pData= sqlite3_value_blob(argv[0]);
    Bytef *pOut = sqlite3_malloc(sz);
    if( pOut==0 ){
      sqlite3_result_error_nomem(context);
    }else if( Z_OK!=uncompress(pOut, &szf, pData, nData) ){
      sqlite3_result_error(context, "error in uncompress()", -1);
    }else{
      sqlite3_result_blob(context, pOut, szf, SQLITE_TRANSIENT);
    }
    sqlite3_free(pOut);
  }
}

/*
** Register the SQLAR functions with a database connection.
*/
int sqlite3_sqlar_init(
  sqlite3 *db,
  char **pzErrMsg,
  const void *pApi
){
  int rc = SQLITE_OK;
  (void)pzErrMsg;  /* Unused parameter */
  (void)pApi;      /* Unused parameter */

  rc = sqlite3_create_function(db, "sqlar_compress", 1,
                               SQLITE_UTF8, 0,
                               sqlarCompressFunc, 0, 0);
  if( rc==SQLITE_OK ){
    rc = sqlite3_create_function(db, "sqlar_uncompress", 2,
                                 SQLITE_UTF8, 0,
                                 sqlarUncompressFunc, 0, 0);
  }
  return rc;
}
