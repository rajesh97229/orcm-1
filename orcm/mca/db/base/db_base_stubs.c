/*
 * Copyright (c) 2012-2013 Los Alamos National Security, Inc.  All rights reserved. 
 * Copyright (c) 2013-2014 Intel Inc. All rights reserved
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */


#include "orcm_config.h"
#include "orcm/constants.h"

#include "opal_stdint.h"
#include "opal/mca/mca.h"
#include "opal/util/error.h"
#include "opal/util/output.h"
#include "opal/mca/base/base.h"
#include "opal/dss/dss_types.h"

#include "orcm/mca/db/base/base.h"


static void process_open(int fd, short args, void *cbdata)
{
    orcm_db_request_t *req = (orcm_db_request_t*)cbdata;
    orcm_db_handle_t *hdl;
    orcm_db_base_module_t *mod;
    orcm_db_base_active_component_t *active;
    orcm_db_base_component_t *component;
    int i, index;
    char **cmps = NULL;
    opal_value_t *kv;
    bool found;

    /* see if the caller provided the magic "components" property */
    if (NULL != req->properties) {
        OPAL_LIST_FOREACH(kv, req->properties, opal_value_t) {
            if (0 == strcmp(kv->key, "components")) {
                cmps = opal_argv_split(kv->data.string, ',');
                break;
            }
        }
    }

    /* cycle thru the available components until one saids
     * it can create a handle for these properties
     */
    OPAL_LIST_FOREACH(active, &orcm_db_base.actives, orcm_db_base_active_component_t) {
        component = active->component;
        found = true;
        if (NULL != cmps) {
            found = false;
            for (i=0; NULL != cmps[i]; i++) {
                if (0 == strcmp(cmps[i], component->base_version.mca_component_name)) {
                    found = true;
                    break;
                }
            }
        }
        if (found) {
            /* let this component try */
            if (NULL != (mod = component->create_handle(req->properties))) {
                /* create the handle */
                hdl = OBJ_NEW(orcm_db_handle_t);
                hdl->component = component;
                hdl->module = mod;
                index = opal_pointer_array_add(&orcm_db_base.handles, hdl);
                if (NULL != req->cbfunc) {
                    req->cbfunc(index, ORCM_SUCCESS, NULL, req->cbdata);
                }
                opal_argv_free(cmps);
                OBJ_RELEASE(req);
                return;
            }
        }
    }

    /* if we get here, we were unable to create the handle */
    if (NULL != req->cbfunc) {
        req->cbfunc(-1, ORCM_ERROR, NULL, req->cbdata);
    }
    opal_argv_free(cmps);
    OBJ_RELEASE(req);
}

void orcm_db_base_open(char *name,
                       opal_list_t *properties,
                       orcm_db_callback_fn_t cbfunc,
                       void *cbdata)
{
    orcm_db_request_t *req;

    /* push this request into our event_base
     * for processing to ensure nobody else is
     * using that dbhandle
     */
    req = OBJ_NEW(orcm_db_request_t);
    /* transfer the name in the primary key */
    req->primary_key = name;
    req->properties = properties;
    req->cbfunc = cbfunc;
    req->cbdata = cbdata;
    opal_event_set(orcm_db_base.ev_base, &req->ev, -1,
                   OPAL_EV_WRITE,
                   process_open, req);
    opal_event_set_priority(&req->ev, OPAL_EV_SYS_HI_PRI);
    opal_event_active(&req->ev, OPAL_EV_WRITE, 1);
}

static void process_close(int fd, short args, void *cbdata)
{
    orcm_db_request_t *req = (orcm_db_request_t*)cbdata;
    orcm_db_handle_t *hdl;
    int rc=ORCM_SUCCESS;

    /* get the handle object */
    if (NULL == (hdl = (orcm_db_handle_t*)opal_pointer_array_get_item(&orcm_db_base.handles, req->dbhandle))) {
        rc = ORCM_ERR_NOT_FOUND;
        goto found;
    }
    if (NULL ==  hdl->module) {
        rc = ORCM_ERR_NOT_FOUND;
        goto found;
    }
    if (NULL != hdl->module->finalize) {
        hdl->module->finalize((struct orcm_db_base_module_t*)hdl->module);
    }

 found:
    if (NULL != req->cbfunc) {
        req->cbfunc(req->dbhandle, rc, NULL, req->cbdata);
    }
    /* release the handle */
    opal_pointer_array_set_item(&orcm_db_base.handles, req->dbhandle, NULL);
    OBJ_RELEASE(hdl);
    OBJ_RELEASE(req);
}

void orcm_db_base_close(int dbhandle,
                        orcm_db_callback_fn_t cbfunc,
                        void *cbdata)
{
    orcm_db_request_t *req;

    /* push this request into our event_base
     * for processing to ensure nobody else is
     * using that dbhandle
     */
    req = OBJ_NEW(orcm_db_request_t);
    req->dbhandle = dbhandle;
    req->cbfunc = cbfunc;
    req->cbdata = cbdata;
    opal_event_set(orcm_db_base.ev_base, &req->ev, -1,
                   OPAL_EV_WRITE,
                   process_close, req);
    opal_event_set_priority(&req->ev, OPAL_EV_SYS_HI_PRI);
    opal_event_active(&req->ev, OPAL_EV_WRITE, 1);
}


static void process_store(int fd, short args, void *cbdata)
{
    orcm_db_request_t *req = (orcm_db_request_t*)cbdata;
    orcm_db_handle_t *hdl;
    int rc=ORCM_SUCCESS;

    /* get the handle object */
    if (NULL == (hdl = (orcm_db_handle_t*)opal_pointer_array_get_item(&orcm_db_base.handles, req->dbhandle))) {
        rc = ORCM_ERR_NOT_FOUND;
        goto found;
    }
    if (NULL ==  hdl->module) {
        rc = ORCM_ERR_NOT_FOUND;
        goto found;
    }
    if (NULL != hdl->module->store) {
        rc = hdl->module->store((struct orcm_db_base_module_t*)hdl->module, req->primary_key, req->kvs);
    }

 found:
    if (NULL != req->cbfunc) {
        req->cbfunc(req->dbhandle, rc, req->kvs, req->cbdata);
    }
    OBJ_RELEASE(req);
}

void orcm_db_base_store(int dbhandle,
                        const char *primary_key,
                        opal_list_t *kvs,
                        orcm_db_callback_fn_t cbfunc,
                        void *cbdata)
{
    orcm_db_request_t *req;

    /* push this request into our event_base
     * for processing to ensure nobody else is
     * using that dbhandle
     */
    req = OBJ_NEW(orcm_db_request_t);
    req->dbhandle = dbhandle;
    req->primary_key = (char*)primary_key;
    req->kvs = kvs;
    req->cbfunc = cbfunc;
    req->cbdata = cbdata;
    opal_event_set(orcm_db_base.ev_base, &req->ev, -1,
                   OPAL_EV_WRITE,
                   process_store, req);
    opal_event_set_priority(&req->ev, OPAL_EV_SYS_HI_PRI);
    opal_event_active(&req->ev, OPAL_EV_WRITE, 1);
}

static void process_commit(int fd, short args, void *cbdata)
{
    orcm_db_request_t *req = (orcm_db_request_t*)cbdata;
    orcm_db_handle_t *hdl;
    int rc=ORCM_SUCCESS;

    /* get the handle object */
    if (NULL == (hdl = (orcm_db_handle_t*)opal_pointer_array_get_item(&orcm_db_base.handles, req->dbhandle))) {
        rc = ORCM_ERR_NOT_FOUND;
        goto found;
    }
    if (NULL ==  hdl->module) {
        rc = ORCM_ERR_NOT_FOUND;
        goto found;
    }
    if (NULL != hdl->module->commit) {
        hdl->module->commit((struct orcm_db_base_module_t*)hdl->module);
    }

 found:
    if (NULL != req->cbfunc) {
        req->cbfunc(req->dbhandle, rc, NULL, req->cbdata);
    }
    OBJ_RELEASE(req);
}

void orcm_db_base_commit(int dbhandle,
                         orcm_db_callback_fn_t cbfunc,
                         void *cbdata)
{
    orcm_db_request_t *req;

    /* push this request into our event_base
     * for processing to ensure nobody else is
     * using that dbhandle
     */
    req = OBJ_NEW(orcm_db_request_t);
    req->dbhandle = dbhandle;
    req->cbfunc = cbfunc;
    req->cbdata = cbdata;
    opal_event_set(orcm_db_base.ev_base, &req->ev, -1,
                   OPAL_EV_WRITE,
                   process_commit, req);
    opal_event_set_priority(&req->ev, OPAL_EV_SYS_HI_PRI);
    opal_event_active(&req->ev, OPAL_EV_WRITE, 1);
}

static void process_fetch(int fd, short args, void *cbdata)
{
    orcm_db_request_t *req = (orcm_db_request_t*)cbdata;
    orcm_db_handle_t *hdl;
    int rc;

    /* get the handle object */
    if (NULL == (hdl = (orcm_db_handle_t*)opal_pointer_array_get_item(&orcm_db_base.handles, req->dbhandle))) {
        rc = ORCM_ERR_NOT_FOUND;
        goto found;
    }
    if (NULL ==  hdl->module) {
        rc = ORCM_ERR_NOT_FOUND;
        goto found;
    }
    rc = hdl->module->fetch((struct orcm_db_base_module_t*)hdl->module, req->primary_key, req->key, req->kvs);
 found:
    if (NULL != req->cbfunc) {
        req->cbfunc(req->dbhandle, rc, req->kvs, req->cbdata);
    }
    OBJ_RELEASE(req);
}

void orcm_db_base_fetch(int dbhandle,
                        const char *primary_key,
                        const char *key,
                        opal_list_t *kvs,
                        orcm_db_callback_fn_t cbfunc,
                        void *cbdata)
{
    orcm_db_request_t *req;

    /* push this request into our event_base
     * for processing to ensure nobody else is
     * using that dbhandle
     */
    req = OBJ_NEW(orcm_db_request_t);
    req->dbhandle = dbhandle;
    req->primary_key = (char*)primary_key;
    req->key = (char*)key;
    req->kvs = kvs;
    req->cbfunc = cbfunc;
    req->cbdata = cbdata;
    opal_event_set(orcm_db_base.ev_base, &req->ev, -1,
                   OPAL_EV_WRITE,
                   process_fetch, req);
    opal_event_set_priority(&req->ev, OPAL_EV_SYS_HI_PRI);
    opal_event_active(&req->ev, OPAL_EV_WRITE, 1);
}

static void process_remove(int fd, short args, void *cbdata)
{
    orcm_db_request_t *req = (orcm_db_request_t*)cbdata;
    orcm_db_handle_t *hdl;
    int rc;

    /* get the handle object */
    if (NULL == (hdl = (orcm_db_handle_t*)opal_pointer_array_get_item(&orcm_db_base.handles, req->dbhandle))) {
        rc = ORCM_ERR_NOT_FOUND;
        goto found;
    }
    if (NULL ==  hdl->module) {
        rc = ORCM_ERR_NOT_FOUND;
        goto found;
    }
    rc = hdl->module->remove((struct orcm_db_base_module_t*)hdl->module, req->primary_key, req->key);
 found:
    if (NULL != req->cbfunc) {
        req->cbfunc(req->dbhandle, rc, NULL, req->cbdata);
    }
    OBJ_RELEASE(req);
}

void orcm_db_base_remove_data(int dbhandle,
                              const char *primary_key,
                              const char *key,
                              orcm_db_callback_fn_t cbfunc,
                              void *cbdata)
{
    orcm_db_request_t *req;

    /* push this request into our event_base
     * for processing to ensure nobody else is
     * using that dbhandle
     */
    req = OBJ_NEW(orcm_db_request_t);
    req->dbhandle = dbhandle;
    req->primary_key = (char*)primary_key;
    req->key = (char*)key;
    req->cbfunc = cbfunc;
    req->cbdata = cbdata;
    opal_event_set(orcm_db_base.ev_base, &req->ev, -1,
                   OPAL_EV_WRITE,
                   process_remove, req);
    opal_event_set_priority(&req->ev, OPAL_EV_SYS_HI_PRI);
    opal_event_active(&req->ev, OPAL_EV_WRITE, 1);
}
