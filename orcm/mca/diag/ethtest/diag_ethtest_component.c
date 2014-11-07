/*
# Copyright (c) 2014      Intel, Inc.  All rights reserved. 
 *
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "orcm_config.h"
#include "opal/util/output.h"

#include "opal/mca/base/mca_base_var.h"

#include "orcm/runtime/orcm_globals.h"
#include "diag_ethtest.h"

/*
 * Public string for version number
 */
const char *orcm_diag_ethtest_component_version_string =
    "ORCM DIAG ethtest MCA component version " ORCM_VERSION;

/*
 * Local functionality
 */
static int diag_ethtest_open(void);
static int diag_ethtest_close(void);
static int diag_ethtest_component_query(mca_base_module_t **module, int *priority);

/*
 * Instantiate the public struct with all of our public information
 * and pointer to our public functions in it
 */
orcm_diag_base_component_t mca_diag_ethtest_component = {
    {
        ORCM_DIAG_BASE_VERSION_1_0_0,
        /* Component name and version */
        "ethtest",
        ORCM_MAJOR_VERSION,
        ORCM_MINOR_VERSION,
        ORCM_RELEASE_VERSION,

        /* Component open and close functions */
        diag_ethtest_open,
        diag_ethtest_close,
        diag_ethtest_component_query,
        NULL
    },
    {
        /* The component is checkpoint ready */
        MCA_BASE_METADATA_PARAM_CHECKPOINT
    },
};

static int diag_ethtest_open(void)
{
    return ORCM_SUCCESS;
}

static int diag_ethtest_close(void)
{
    return ORCM_SUCCESS;
}

static int diag_ethtest_component_query(mca_base_module_t **module, int *priority)
{
    *priority = 50;
    *module = (mca_base_module_t *)&orcm_diag_ethtest_module;
    return ORCM_SUCCESS;

}
