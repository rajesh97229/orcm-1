/*
 * Copyright (c) 2010      Cisco Systems, Inc.  All rights reserved. 
 * Copyright (c) 2012      Los Alamos National Security, Inc. All rights reserved.
 * Copyright (c) 2014      Intel, Inc. All rights reserved.
 *
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */


#include "orcm_config.h"
#include "orcm/constants.h"

#include "orte/mca/errmgr/errmgr.h"

#include "opal/dss/dss.h"
#include "opal/mca/event/event.h"

#include "orcm/mca/db/db.h"
#include "orcm/runtime/orcm_progress.h"
#include "orcm/mca/sensor/base/base.h"
#include "orcm/mca/sensor/base/sensor_private.h"

static bool mods_active = false;
static void take_sample(int fd, short args, void *cbdata);
static void collect_inventory(orcm_sensor_inventory_record_t *record);

static void db_open_cb(int handle, int status, opal_list_t *kvs, void *cbdata)
{
    if (0 == status) {
        orcm_sensor_base.dbhandle = handle;
    }
}

static void* progress_thread_engine(opal_object_t *obj)
{
    while (orcm_sensor_base.ev_active) {
        opal_event_loop(orcm_sensor_base.ev_base, OPAL_EVLOOP_ONCE);
    }
    return OPAL_THREAD_CANCELLED;
}


void orcm_sensor_base_start(orte_jobid_t job)
{
    orcm_sensor_active_module_t *i_module;
    int i;
    orcm_sensor_sampler_t *sampler;
    orcm_sensor_inventory_record_t *record;

    opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                        "%s sensor:base: sensor start called",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));

    /* if no modules are active, then there is nothing to do */
    if (0 == orcm_sensor_base.modules.size) {
        return;
    }

    if (!mods_active) {
        opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                            "%s sensor:base: starting sensors",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));

        if (!orcm_sensor_base.dbhandle_requested) {
            orcm_db.open("sensor", NULL, db_open_cb, NULL);
            orcm_sensor_base.dbhandle_requested = true;
        }

        /* call the start function of all modules in priority order */
        for (i=0; i < orcm_sensor_base.modules.size; i++) {
            if (NULL == (i_module = (orcm_sensor_active_module_t*)opal_pointer_array_get_item(&orcm_sensor_base.modules, i))) {
                continue;
            }
            mods_active = true;
            if (NULL != i_module->module->start) {
                i_module->module->start(job);
            }
        }

        /* create the event base and start the progress engine, if necessary */
        if (!orcm_sensor_base.ev_active) {
            orcm_sensor_base.ev_active = true;
            if (NULL == (orcm_sensor_base.ev_base = orcm_start_progress_thread("sensor", progress_thread_engine, NULL))) {
                orcm_sensor_base.ev_active = false;
                return;
            }
        }

        if (mods_active && 0 < orcm_sensor_base.sample_rate && orcm_sensor_base.metrics) {
            /* startup a timer to wake us up periodically
             * for a data sample, and pass in the sampler
             */
            opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                                "%s sensor:base: creating sampler with rate %d",
                                ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                                orcm_sensor_base.sample_rate);
            sampler = OBJ_NEW(orcm_sensor_sampler_t);
            sampler->rate.tv_sec = orcm_sensor_base.sample_rate;
            sampler->log_data = orcm_sensor_base.log_samples;
            opal_event_evtimer_set(orcm_sensor_base.ev_base, &sampler->ev,
                                   take_sample, sampler);
            opal_event_evtimer_add(&sampler->ev, &sampler->rate);
        }
    } else if (!orcm_sensor_base.ev_active) {
        orcm_sensor_base.ev_active = true;
        orcm_restart_progress_thread("sensor");
    }

    if(true == orcm_sensor_base.inventory) {
        record = OBJ_NEW(orcm_sensor_inventory_record_t);
        
        if (false == orcm_sensor_base.inventory_dynamic) { /* Collect inventory details just once when orcmd starts */
            collect_inventory(record);
            opal_output(0,"sensor:base - boot time invenotry collection requested");

        } else { /* Update inventory details when hotswap even occurs */
            /* @VINFIX: Need a way to monitor the syslog with hotswap events. */
            opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                                "sensor:base - DYNAMIC inventory collection enabled");
        }
    } else {
         opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                            "sensor:base inventory collection not requested");
    }

    return;    
}

void collect_inventory(orcm_sensor_inventory_record_t *record)
{
    orcm_sensor_active_module_t *i_module;
    int i;

    opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                        "%s sensor:base: Starting Inventory Collection",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
    /* call the inventory collection function of all enabled modules in priority order */
    for (i=0; i < orcm_sensor_base.modules.size; i++) {
        if (NULL == (i_module = (orcm_sensor_active_module_t*)opal_pointer_array_get_item(&orcm_sensor_base.modules, i))) {
            continue;
        }
        opal_output(0,"Found module: %s",i_module->component->base_version.mca_component_name);
/* @VINFIX: We need to change the function call below by adding a new API to the sensor module
 * which will be used to collect the inventory related data and send it up to the aggregator.
 * 
 * ***** NOTE ****
 * We might have to add a new plugin analogous to heartbeat which will collect the sampled inventory records
 * and push it up to the aggregator. THIS HAS TO BE DIFFERENT FROM HEARTBEAT to avoid users to run a processor
 * intesive component for collecting static inventory data
 *  Alternatively this could be part of the hwloc component which will be enabled only when inventory is enabled.
 *  but that might tie down the user to always enable hwloc component even when only ipmi inventory collection is enabled
 */
 #if 0
        if (NULL != i_module->module->inv_collect) {
            i_module->module->inv_collect(record);
        }
#endif
    }
}
void orcm_sensor_base_stop(orte_jobid_t job)
{
    orcm_sensor_active_module_t *i_module;
    int i;

    if (!mods_active) {
        opal_output(0, "sensor stop: no active mods");
        return;
    }

    opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                        "%s sensor:base: stopping sensors",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));

    if (orcm_sensor_base.ev_active) {
        orcm_sensor_base.ev_active = false;
        /* stop the thread without releasing the event base */
        orcm_stop_progress_thread("sensor", false);
    }

    /* call the stop function of all modules in priority order */
    for (i=0; i < orcm_sensor_base.modules.size; i++) {
        if (NULL == (i_module = (orcm_sensor_active_module_t*)opal_pointer_array_get_item(&orcm_sensor_base.modules, i))) {
            continue;
        }
        if (NULL != i_module->module->stop) {
            i_module->module->stop(job);
        }
    }

    return;
}

static void take_sample(int fd, short args, void *cbdata)
{
    orcm_sensor_active_module_t *i_module;
    orcm_sensor_sampler_t *sampler = (orcm_sensor_sampler_t*)cbdata;
    int i;
    
    if (!mods_active) {
        opal_output(0, "sensor sample: no active mods");
        return;
    }

    opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                        "%s sensor:base: sampling sensors",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));

    /* call the sample function of all modules in priority order from
     * highest to lowest - the heartbeat should always be the lowest
     * priority, so it will send any collected data
     */
    for (i=0; i < orcm_sensor_base.modules.size; i++) {
        if (NULL == (i_module = (orcm_sensor_active_module_t*)opal_pointer_array_get_item(&orcm_sensor_base.modules, i))) {
            continue;
        }
        /* see if this sensor is included in the request */
        if (NULL != sampler->sensors &&
            NULL == strcasestr(sampler->sensors, i_module->component->base_version.mca_component_name)) {
            continue;
        }
        if (NULL != i_module->module->sample) {
            opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                                "%s sensor:base: sampling component %s",
                                ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                                i_module->component->base_version.mca_component_name);
            i_module->module->sample(sampler);
        }
    }

    /* execute the callback, if given */
    if (NULL != sampler->cbfunc) {
        sampler->cbfunc(&sampler->bucket, sampler->cbdata);
        OBJ_DESTRUCT(&sampler->bucket);
        OBJ_CONSTRUCT(&sampler->bucket, opal_buffer_t);
    }

    /* restart the timer, if given */
    if (0 < sampler->rate.tv_sec) {
        opal_event_evtimer_add(&sampler->ev, &sampler->rate);
    } else {
        OBJ_RELEASE(sampler);
    }

    return;
}

void orcm_sensor_base_log(char *comp, opal_buffer_t *data)
{
    int i;
    orcm_sensor_active_module_t *i_module;

    /* if no modules are available, then there is nothing to do */
    if (0 == orcm_sensor_base.modules.size) {
        return;
    }

    if (NULL == comp || orcm_sensor_base.dbhandle < 0) {
        /* nothing we can do */
        return;
    }

    opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                        "%s sensor:base: logging sensor %s",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), comp);

    /* find the specified module  */
    for (i=0; i < orcm_sensor_base.modules.size; i++) {
        if (NULL == (i_module = (orcm_sensor_active_module_t*)opal_pointer_array_get_item(&orcm_sensor_base.modules, i))) {
            continue;
        }
        if (0 == strcmp(comp, i_module->component->base_version.mca_component_name)) {
            if (NULL != i_module->module->log) {
                i_module->module->log(data);
            }
            return;
        }
    }
}

void orcm_sensor_base_manually_sample(char *sensors,
                                      orcm_sensor_sample_cb_fn_t cbfunc,
                                      void *cbdata)
{
    orcm_sensor_sampler_t *sampler;

    if (!mods_active) {
        return;
    }

    opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                        "%s sensor:base: sampling sensors",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));

    /* push an event into the system to sample the sensors, collecting
     * the output in the provided buffer */
    sampler = OBJ_NEW(orcm_sensor_sampler_t);
    sampler->sensors = strdup(sensors);
    sampler->cbfunc = cbfunc;
    sampler->cbdata = cbdata;

    opal_event_set(orcm_sensor_base.ev_base, &sampler->ev, -1, OPAL_EV_WRITE, take_sample, sampler);
    opal_event_set_priority(&sampler->ev, ORTE_SYS_PRI);
    opal_event_active(&sampler->ev, OPAL_EV_WRITE, 1);
    return;
}

