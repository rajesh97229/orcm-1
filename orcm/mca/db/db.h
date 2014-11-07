/*
 * Copyright (c) 2010      Cisco Systems, Inc.  All rights reserved. 
 * Copyright (c) 2012-2013 Los Alamos National Security, Inc.  All rights reserved.
 * Copyright (c) 2013-2014 Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */
/** @file:
 *
 * The Database Framework
 *
 */

#ifndef ORCM_DB_H
#define ORCM_DB_H

#include "orcm_config.h"
#include "orcm/types.h"

#include "opal/mca/mca.h"
#include "opal/mca/event/event.h"
#include "opal/dss/dss_types.h"

/**
 * DATABASE DESIGN
 *
 * DB APIs are non-blocking and executed by pushing the request onto the ORCM
 * event base. Upon completion, the provided cbfunc will be called to return
 * the status resulting from the operation (a NULL cbfunc is permitted). The
 * cbfunc is responsible for releasing the returned list
 */

BEGIN_C_DECLS

/* forward declare */
struct orcm_db_base_module_t;

/* callback function for async requests */
typedef void (*orcm_db_callback_fn_t)(int dbhandle, int status,
                                      opal_list_t *kvs, void *cbdata);

/*
 * Initialize the module
 */
typedef int (*orcm_db_base_module_init_fn_t)(struct orcm_db_base_module_t *imod);

/*
 * Finalize the module
 */
typedef void (*orcm_db_base_module_finalize_fn_t)(struct orcm_db_base_module_t *imod);

/*
 * Open a database
 * 
 * Open a database for access (read, write, etc.). The request
 * can contain a user-specified name for this database that
 * has nothing to do with the backend database - it is solely
 * for use as a debug tool to help identify the database. The
 * request can also optionally provide a list of opal_value_t
 * properties - this is where one might specify the name of
 * the backend database, a URI for contacting it, the name of
 * a particular table for request, etc. Thus, it is important
 * to note that the returned "handle" is associated solely with
 * the defined request - i.e., if the properties specify a database
 * and table, then the handle will be specific to that combination.
 *
 * NOTE: one special "property" allows you to specify the
 * name(s) of the component(s) you want considered for this
 * handle - i.e., the equivalent of specifying the MCA param
 * "db=list" - using the  reserved property name "components".
 * The components will be queried in the order specified. The ^
 * character is also supported, with the remaining components
 * considered in priority order
 *
 * Just like the standard POSIX file open, the call will return
 * a unique "handle" that must be provided with any subsequent
 * call to store or fetch data from this database. 
 */
typedef void (*orcm_db_base_API_open_fn_t)(char *name,
                                           opal_list_t *properties,
                                           orcm_db_callback_fn_t cbfunc,
                                           void *cbdata);

/*
 * Close a database handle
 *
 * Close the specified database handle. This may or may not invoke
 * termination of a connection to a remote database or release of
 * memory storage, depending on the precise implementation of the
 * active database components. A -1 handle indicates that ALL open
 * database handles are to be closed.
 */
typedef void (*orcm_db_base_API_close_fn_t)(int dbhandle,
                                            orcm_db_callback_fn_t cbfunc,
                                            void *cbdata);

/*
 * Store one or more data elements against a primary key.  The values are
 * passed as a key-value list in the kvs parameter.  The semantics of the 
 * primary key and list of values will depend on the data that needs to be 
 * stored.
 *
 * At the moment the API store function is designed to handle storing data 
 * collected by the sensor framework components.  In this case, the primary key 
 * is a name for the group of data being passed (to classify the data and avoid 
 * naming conflicts with other data items collected by other sensors) and the 
 * list of values shall contain: the time stamp, the hostname and the values. 
 * For the values, sensors may optionally provide the data units in the key 
 * field using the following format: <data item name>:<data units>.  Note that 
 * this means the colon (":") is a reserved character.
 */
typedef void (*orcm_db_base_API_store_fn_t)(int dbhandle,
                                            const char *primary_key,
                                            opal_list_t *kvs,
                                            orcm_db_callback_fn_t cbfunc,
                                            void *cbdata);
typedef int (*orcm_db_base_module_store_fn_t)(struct orcm_db_base_module_t *imod,
                                              const char *primary_key,
                                              opal_list_t *kvs);

/*
 * Update one or more features for a node as part of the inventory data, for
 * example: number of sockets, cores per socket, RAM, etc.  The features are
 * passed as a list of key-value pairs plus units: orcm_metric_value_t.  The
 * units may be left NULL if not applicable.
 */
typedef void (*orcm_db_base_API_update_node_features_fn_t)(
        int dbhandle,
        const char *hostname,
        opal_list_t *features,
        orcm_db_callback_fn_t cbfunc,
        void *cbdata);
typedef int (*orcm_db_base_module_update_node_features_fn_t)(
        struct orcm_db_base_module_t *imod,
        const char *hostname,
        opal_list_t *features);

/*
 * Commit data to the database - action depends on implementation within
 * each active component
 */
typedef void (*orcm_db_base_API_commit_fn_t)(int dbhandle,
                                             orcm_db_callback_fn_t cbfunc,
                                             void *cbdata);
typedef void (*orcm_db_base_module_commit_fn_t)(struct orcm_db_base_module_t *imod);

/*
 * Retrieve data
 *
 * Retrieve data for the given primary key associated with the specified key. Wildcards
 * are supported here as well. Caller is responsible for releasing the returned list
 * of opal_keyval_t objects.
 */
typedef void (*orcm_db_base_API_fetch_fn_t)(int dbhandle,
                                            const char *primary_key,
                                            const char *key,
                                            opal_list_t *kvs,
                                            orcm_db_callback_fn_t cbfunc,
                                            void *cbdata);
typedef int (*orcm_db_base_module_fetch_fn_t)(struct orcm_db_base_module_t *imod,
                                              const char *primary_key,
                                              const char *key,
                                              opal_list_t *kvs);
/*
 * Delete data
 *
 * Delete the data for the given primary key that is associated with the specified key.
 * If a NULL key is provided, all data for the given primary key will be deleted.
 */
typedef void (*orcm_db_base_API_remove_fn_t)(int dbhandle,
                                             const char *primary_key,
                                             const char *key,
                                             orcm_db_callback_fn_t cbfunc,
                                             void *cbdata);
typedef int (*orcm_db_base_module_remove_fn_t)(struct orcm_db_base_module_t *imod,
                                               const char *primary_key,
                                               const char *key);

/*
 * the standard module data structure
 */
typedef struct  {
    orcm_db_base_module_init_fn_t                 init;
    orcm_db_base_module_finalize_fn_t             finalize;
    orcm_db_base_module_store_fn_t                store;
    orcm_db_base_module_update_node_features_fn_t update_node_features;
    orcm_db_base_module_commit_fn_t               commit;
    orcm_db_base_module_fetch_fn_t                fetch;
    orcm_db_base_module_remove_fn_t               remove;
} orcm_db_base_module_t;

typedef struct {
    orcm_db_base_API_open_fn_t                 open;
    orcm_db_base_API_close_fn_t                close;
    orcm_db_base_API_store_fn_t                store;
    orcm_db_base_API_update_node_features_fn_t update_node_features;
    orcm_db_base_API_commit_fn_t               commit;
    orcm_db_base_API_fetch_fn_t                fetch;
    orcm_db_base_API_remove_fn_t               remove;
} orcm_db_API_module_t;


/* function to determine if this component is available for use.
 * Note that we do not use the standard component open
 * function as we do not want/need return of a module.
 */
typedef bool (*mca_db_base_component_avail_fn_t)(void);

/* create and return a database module */
typedef orcm_db_base_module_t* (*mca_db_base_component_create_hdl_fn_t)(opal_list_t *props);

/* provide a chance for the component to finalize */
typedef void (*mca_db_base_component_finalize_fn_t)(void);

typedef struct {
    mca_base_component_t                  base_version;
    mca_base_component_data_t             base_data;
    int                                   priority;
    mca_db_base_component_avail_fn_t      available;
    mca_db_base_component_create_hdl_fn_t create_handle;
    mca_db_base_component_finalize_fn_t   finalize;
} orcm_db_base_component_t;

/*
 * Macro for use in components that are of type db
 */
#define ORCM_DB_BASE_VERSION_2_0_0 \
  MCA_BASE_VERSION_2_0_0, \
  "db", 2, 0, 0

/* Global structure for accessing DB functions */
ORCM_DECLSPEC extern orcm_db_API_module_t orcm_db;  /* holds API function pointers */

END_C_DECLS

#endif
