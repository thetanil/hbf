/*
 * SPDX-License-Identifier: MIT
 * HBF QuickJS ES Module Loader
 */

#ifndef HBF_QJS_MODULE_LOADER_H
#define HBF_QJS_MODULE_LOADER_H

#include <quickjs.h>
#include <sqlite3.h>

/*
 * Initialize the ES module loader for a QuickJS runtime.
 * Must be called after JS_NewRuntime() and before loading any modules.
 *
 * rt: QuickJS runtime
 * db: SQLite database handle containing the versioned filesystem
 */
void hbf_qjs_module_loader_init(JSRuntime *rt, sqlite3 *db);

#endif /* HBF_QJS_MODULE_LOADER_H */
