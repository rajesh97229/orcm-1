/*
 * Copyright (c) 2014      Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */


#include "orcm_config.h"
#include "orcm/constants.h"
#include "orcm/types.h"

#include "opal/dss/dss.h"
#include "opal/util/output.h"

#include "orte/mca/errmgr/errmgr.h"

#include "orcm/mca/analytics/base/base.h"
#include "orcm/mca/analytics/base/analytics_private.h"

static int workflow_id = 0;

int orcm_analytics_base_workflow_create(opal_buffer_t* buffer, int *wfid)
{
    int num_steps, i, cnt, rc;
    char *mod_name = NULL;
    char *mod_attrs = NULL;
    orcm_workflow_step_t *wf_step = NULL;
    orcm_workflow_t *wf;
    
    /* unpack the number of steps */
    cnt = 1;
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, &num_steps,
                                              &cnt, OPAL_INT))) {
        goto error;
    }
    
    wf = OBJ_NEW(orcm_workflow_t);
    wf->workflow_id = workflow_id;
    workflow_id++;
    *wfid = wf->workflow_id;
    
    /* FIXME handle new event base */

    for (i = 0; i < num_steps; i++) {
        /* unpack the requested module name */
        cnt = 1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, &mod_name,
                                                  &cnt, OPAL_STRING))) {
            goto error;
        }
        cnt = 1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, &mod_attrs,
                                                  &cnt, OPAL_STRING))) {
            goto error;
        }
        wf_step = OBJ_NEW(orcm_workflow_step_t);
        wf_step->analytic = strdup(mod_name);
        
        if (ORCM_SUCCESS !=
            (rc = orcm_analytics_base_select_workflow(wf_step))) {
            OBJ_RELEASE(wf_step);
            /* skip this module ? */
            continue;
        }
        opal_list_append(&wf->steps, &wf_step->super);
    }
    
    opal_list_append(&orcm_analytics_base.workflows, &wf->super);
    
    return ORCM_SUCCESS;

error:
    ORTE_ERROR_LOG(rc);
    if (mod_name) {
        free(mod_name);
    }
    if (mod_attrs) {
        free(mod_attrs);
    }
    if (buffer) {
        OBJ_RELEASE(buffer);
    }
    if (wf) {
        OBJ_RELEASE(wf);
    }
    return rc;
}

int orcm_analytics_base_workflow_delete(int workflow_id)
{
    orcm_workflow_t *wf;
    orcm_workflow_t *next;
    
    OPAL_LIST_FOREACH_SAFE(wf, next, &orcm_analytics_base.workflows, orcm_workflow_t) {
        if (workflow_id == wf->workflow_id) {
            opal_list_remove_item(&orcm_analytics_base.workflows, &wf->super);
            OBJ_RELEASE(wf);
        }
    }
    return ORCM_SUCCESS;
}

int orcm_analytics_base_send_data(opal_value_t *data)
{
    orcm_workflow_t *wf;
    
    OPAL_LIST_FOREACH(wf, &orcm_analytics_base.workflows, orcm_workflow_t) {
        /* OBJ_RETAIN(data), ORCM_ACTIVATE_WORKFLOW_STEP first step */
    }
    return ORCM_SUCCESS;
}
