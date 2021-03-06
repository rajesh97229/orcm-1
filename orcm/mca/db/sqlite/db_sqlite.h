/*
 * Copyright (c) 2012-2013 Los Alamos National Security, Inc. All rights reserved.
 * Copyright (c) 2014      Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

#ifndef ORCM_DB_SQLITE_H
#define ORCM_DB_SQLITE_H

#include <sqlite3.h>

#include "orcm/mca/db/db.h"

BEGIN_C_DECLS

ORCM_MODULE_DECLSPEC extern orcm_db_base_component_t mca_db_sqlite_component;
typedef struct {
    orcm_db_base_module_t api;
    char *dbfile;
    sqlite3 **dbhandles;
    int nthreads;
    int active;
} mca_db_sqlite_module_t;
ORCM_MODULE_DECLSPEC extern mca_db_sqlite_module_t mca_db_sqlite_module;



/* Macros for manipulating sqlite */
#define ORCM_SQLITE_CMD(f, db, r)                               \
    {                                                           \
        *(r) = sqlite3_ ## f;                                   \
        if (*(r) != SQLITE_OK) {                                \
            opal_output(0, "%s failed with status %d: %s",      \
                        #f, *(r), sqlite3_errmsg(db));          \
        }                                                       \
    }                                                           \

#define ORCM_SQLITE_OP(f, x, db, r)                             \
    {                                                           \
        *(r) = sqlite3_ ## f;                                   \
        if (*(r) != SQLITE_ ## x) {                             \
            opal_output(0, "%s failed with status %d: %s",      \
                        #f, *(r), sqlite3_errmsg(db));          \
        }                                                       \
    }                                                           \

END_C_DECLS

#endif /* ORCM_DB_SQLITE_H */
