/* SPDX-License-Identifier: Public Domain */
#ifndef SQLAR_H
#define SQLAR_H

#include <sqlite3.h>

/*
** Register the SQLAR extension functions with a database connection.
** Returns SQLITE_OK on success, error code otherwise.
*/
int sqlite3_sqlar_init(sqlite3 *db, char **pzErrMsg, const void *pApi);

#endif /* SQLAR_H */
