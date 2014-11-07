/*
 * Copyright (c) 2009-2011 Cisco Systems, Inc.  All rights reserved.
 * Copyright (c) 2013-2014 Intel, Inc.  All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "orcm_config.h"

#include "opal/dss/dss.h"

#include "orte/mca/errmgr/errmgr.h"

#include "orcm/runtime/orcm_globals.h"
#include "orcm/mca/scd/base/base.h"

int orcm_dt_init(void)
{
    int rc;
    opal_data_type_t tmp;

    /* register the scheduler types for packing/unpacking services */
    tmp = ORCM_ALLOC;
    if (OPAL_SUCCESS !=
        (rc = opal_dss.register_type(orcm_pack_alloc,
                                     orcm_unpack_alloc,
                                     (opal_dss_copy_fn_t)orcm_copy_alloc,
                                     (opal_dss_compare_fn_t)orcm_compare_alloc,
                                     (opal_dss_print_fn_t)orcm_print_alloc,
                                     OPAL_DSS_STRUCTURED,
                                     "ORCM_ALLOC", &tmp))) {
        ORTE_ERROR_LOG(rc);
        return rc;
    }
    
    return ORCM_SUCCESS;
}

const char *orcm_node_state_to_str(orcm_node_state_t state)
{
    char *s;
    
    switch (state) {
        case ORCM_NODE_STATE_UNDEF:
            s = "UNDEF";
            break;
        case ORCM_NODE_STATE_UNKNOWN:
            s = "UNKNOWN";
            break;
        case ORCM_NODE_STATE_UP:
            s = "UP";
            break;
        case ORCM_NODE_STATE_DOWN:
            s = "DOWN";
            break;
        case ORCM_NODE_STATE_SESTERM:
            s = "SESSION TERMINATING";
            break;
        default:
            s = "STATEUNDEF";
    }
    return s;
}

const char *orcm_node_state_to_char(orcm_node_state_t state)
{
    char *s;
    
    switch (state) {
        case ORCM_NODE_STATE_UNKNOWN:
            s = "?";
            break;
        case ORCM_NODE_STATE_UP:
            s = "\u2191";
            break;
        case ORCM_NODE_STATE_DOWN:
            s = "\u2193";
            break;
        case ORCM_NODE_STATE_SESTERM:
            s = "\u21BB";
            break;
        default:
            s = "\u2297";
    }
    return s;
}

static void omv_constructor(orcm_metric_value_t *omvp)
{
    OBJ_CONSTRUCT(&omvp->value, opal_value_t);
    omvp->units = NULL;
}

static void omv_destructor(orcm_metric_value_t *omvp)
{
    if (NULL != omvp->units) {
        free(omvp->units);
    }

    OBJ_DESTRUCT(&omvp->value);
}

OBJ_CLASS_INSTANCE(orcm_metric_value_t,
                   opal_value_t,
                   omv_constructor,
                   omv_destructor);
