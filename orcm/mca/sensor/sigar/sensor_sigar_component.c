/*
 * Copyright (c) 2013-2014 Intel, Inc. All rights reserved.
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

#include "orcm_config.h"
#include "orcm/constants.h"

#include "opal/mca/base/base.h"
#include "opal/mca/base/mca_base_var.h"

#include "orcm/mca/sensor/base/sensor_private.h"
#include "sensor_sigar.h"

/*
 * Local functions
 */

static int orcm_sensor_sigar_open(void);
static int orcm_sensor_sigar_close(void);
static int orcm_sensor_sigar_query(mca_base_module_t **module, int *priority);
static int sigar_component_register(void);

orcm_sensor_sigar_component_t mca_sensor_sigar_component = {
    {
        {
            ORCM_SENSOR_BASE_VERSION_1_0_0,
            
            "sigar", /* MCA component name */
            ORCM_MAJOR_VERSION,  /* MCA component major version */
            ORCM_MINOR_VERSION,  /* MCA component minor version */
            ORCM_RELEASE_VERSION,  /* MCA component release version */
            orcm_sensor_sigar_open,  /* component open  */
            orcm_sensor_sigar_close, /* component close */
            orcm_sensor_sigar_query,  /* component query */
            sigar_component_register
        },
        {
            /* The component is checkpoint ready */
            MCA_BASE_METADATA_PARAM_CHECKPOINT
        },
        "procresource,noderesource"
    }
};

/**
  * component open/close/init function
  */
static int orcm_sensor_sigar_open(void)
{
    return ORCM_SUCCESS;
}

static int orcm_sensor_sigar_query(mca_base_module_t **module, int *priority)
{
    /* if we can build, then we definitely want to be used
     * even if we aren't going to sample as we have to be
     * present in order to log any received results
     */
    *priority = 150;  /* ahead of heartbeat and resusage */
    *module = (mca_base_module_t *)&orcm_sensor_sigar_module;
    return ORCM_SUCCESS;
}

/**
 *  Close all subsystems.
 */

static int orcm_sensor_sigar_close(void)
{
    return ORCM_SUCCESS;
}

static int sigar_component_register(void)
{
    mca_base_component_t *c = &mca_sensor_sigar_component.super.base_version;

    mca_sensor_sigar_component.test = false;
    (void) mca_base_component_var_register (c, "test",
                                            "Generate and pass test vector",
                                            MCA_BASE_VAR_TYPE_BOOL, NULL, 0, 0,
                                            OPAL_INFO_LVL_9,
                                            MCA_BASE_VAR_SCOPE_READONLY,
                                            & mca_sensor_sigar_component.test);
    mca_sensor_sigar_component.mem = true;
    (void) mca_base_component_var_register (c, "mem",
                                            "Enable collecting memory usage",
                                            MCA_BASE_VAR_TYPE_BOOL, NULL, 0, 0,
                                            OPAL_INFO_LVL_9,
                                            MCA_BASE_VAR_SCOPE_READONLY,
                                            & mca_sensor_sigar_component.mem);
    mca_sensor_sigar_component.swap = true;
    (void) mca_base_component_var_register (c, "swap",
                                            "Enabale collecting swap usage",
                                            MCA_BASE_VAR_TYPE_BOOL, NULL, 0, 0,
                                            OPAL_INFO_LVL_9,
                                            MCA_BASE_VAR_SCOPE_READONLY,
                                            & mca_sensor_sigar_component.swap);
    mca_sensor_sigar_component.cpu = true;
    (void) mca_base_component_var_register (c, "cpu",
                                            "Enable collecting cpu usage",
                                            MCA_BASE_VAR_TYPE_BOOL, NULL, 0, 0,
                                            OPAL_INFO_LVL_9,
                                            MCA_BASE_VAR_SCOPE_READONLY,
                                            & mca_sensor_sigar_component.cpu);
    mca_sensor_sigar_component.load = true;
    (void) mca_base_component_var_register (c, "load",
                                            "Enable collecting cpu load",
                                            MCA_BASE_VAR_TYPE_BOOL, NULL, 0, 0,
                                            OPAL_INFO_LVL_9,
                                            MCA_BASE_VAR_SCOPE_READONLY,
                                            & mca_sensor_sigar_component.load);
    mca_sensor_sigar_component.disk = true;
    (void) mca_base_component_var_register (c, "disk",
                                            "Enable collecting disk usage",
                                            MCA_BASE_VAR_TYPE_BOOL, NULL, 0, 0,
                                            OPAL_INFO_LVL_9,
                                            MCA_BASE_VAR_SCOPE_READONLY,
                                            & mca_sensor_sigar_component.disk);
    mca_sensor_sigar_component.network = true;
    (void) mca_base_component_var_register (c, "network",
                                            "Enable collecting network usage",
                                            MCA_BASE_VAR_TYPE_BOOL, NULL, 0, 0,
                                            OPAL_INFO_LVL_9,
                                            MCA_BASE_VAR_SCOPE_READONLY,
                                            & mca_sensor_sigar_component.network);
    mca_sensor_sigar_component.sys = true;
    (void) mca_base_component_var_register (c, "sys",
                                            "Enable collecting system information like uptime",
                                            MCA_BASE_VAR_TYPE_BOOL, NULL, 0, 0,
                                            OPAL_INFO_LVL_9,
                                            MCA_BASE_VAR_SCOPE_READONLY,
                                            & mca_sensor_sigar_component.proc);
    mca_sensor_sigar_component.proc = true;
    (void) mca_base_component_var_register (c, "proc",
                                            "Enable collecting process information of daemon and child processes",
                                            MCA_BASE_VAR_TYPE_BOOL, NULL, 0, 0,
                                            OPAL_INFO_LVL_9,
                                            MCA_BASE_VAR_SCOPE_READONLY,
                                            & mca_sensor_sigar_component.proc);
    return ORCM_SUCCESS;
}
