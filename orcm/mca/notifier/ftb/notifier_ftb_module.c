/*
 * Copyright (c) 2004-2010 The Trustees of Indiana University and Indiana
 *                         University Research and Technology
 *                         Corporation.  All rights reserved.
 * Copyright (c) 2004-2005 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 * Copyright (c) 2004-2005 High Performance Computing Center Stuttgart,
 *                         University of Stuttgart.  All rights reserved.
 * Copyright (c) 2004-2005 The Regents of the University of California.
 *                         All rights reserved.
 * Copyright (c) 2007      Sun Microsystems, Inc.  All rights reserved.
 * Copyright (c) 2009      Cisco Systems, Inc.  All rights reserved.
 * Copyright (c) 2014      Intel, Inc.  All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "orcm_config.h"
#include "orcm/constants.h"

#include <stdio.h>
#include <string.h>
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif  /* HAVE_SYS_TIME_H */
#ifdef HAVE_STDARG_H
#include <stdarg.h>
#endif

#include "opal/mca/installdirs/installdirs.h"
#include "opal/util/show_help.h"
#include "opal/util/os_path.h"

#include "orte/mca/plm/base/plm_private.h"
#include "orte/mca/plm/plm.h"
#include "orte/mca/sensor/sensor.h"
#include "orte/mca/ess/ess.h"
#include "orte/util/show_help.h"
#include "orte/mca/snapc/snapc.h"
#include "orte/mca/snapc/base/base.h"

#include "orte/mca/errmgr/errmgr.h"
#include "orte/mca/errmgr/base/base.h"

#include "orcm/mca/notifier/base/base.h"
#include "notifier_ftb.h"

/* Static API's */
static int init(void);
static void finalize(void);
static void ftb_log(orcm_notifier_base_severity_t severity, int errcode, 
                  const char *msg, va_list ap);
static void ftb_help(orcm_notifier_base_severity_t severity, int errcode, 
                      const char *filename, const char *topic, va_list ap);
static void ftb_peer(orcm_notifier_base_severity_t severity, int errcode, 
                      orte_process_name_t *peer_proc, const char *msg, 
                      va_list ap);

/* Module def */
orcm_notifier_base_module_t orcm_notifier_ftb_module = {
    init,
    finalize,
    ftb_log,
    ftb_help,
    ftb_peer,
    NULL
};

/* FTB client information */
FTB_client_t ftb_client_info;

/* FTB client handle */
FTB_client_handle_t ftb_client_handle;

static int init(void) {
    int ret;
    char *schema_file;

    /* Locate the FTB events schema file */
    if (NULL == (schema_file = opal_os_path(false, opal_install_dirs.pkgdatadir,
                                            "help-ftb-event-schema.txt", NULL))) {
        schema_file = strdup("help-ftb-event-schema.txt");
    }

    /* Declare the Open MPI publishable events to the FTB */
    ret = FTB_Declare_publishable_events(ftb_client_handle, schema_file, NULL, 0);
    free(schema_file);

    if (FTB_SUCCESS != ret) {
        orte_show_help("help-orcm-notifier-ftb.txt", "declare events failed", true, 
                       "FTB_Declare_publishable_events() failed", ret);

        FTB_Disconnect(ftb_client_handle);
        return ORCM_ERROR;
    }

    return ORCM_SUCCESS;
}

static void finalize(void) {
    /* If the FTB client handle is valid, disconnect the client from FTB. */
    if (1 == ftb_client_handle.valid) {
	    FTB_Disconnect(ftb_client_handle);
    }
}

static const char* get_ftb_event_severity(orcm_notifier_base_severity_t severity)
{
    switch (severity) {
    case ORCM_NOTIFIER_EMERG:
    case ORCM_NOTIFIER_ALERT:
        return "ALL";
    case ORCM_NOTIFIER_CRIT:
        return "FATAL";
    case ORCM_NOTIFIER_ERROR:
        return "ERROR";
    case ORCM_NOTIFIER_WARN:
    case ORCM_NOTIFIER_NOTICE:
        return "WARNING";
    case ORCM_NOTIFIER_INFO:
    case ORCM_NOTIFIER_DEBUG:
        return "INFO";
    default:
        return "UNKNOWN";
    }
}

static const char* get_ftb_event_name(int errnum)
{
    /* Handle checkpoint/restart and migration events */
    if ( CHECK_ORTE_SNAPC_CKPT_STATE(errnum) ) {
        errnum = ORTE_SNAPC_CKPT_STATE(errnum);
        switch (errnum) {
        case ORTE_SNAPC_CKPT_STATE_ESTABLISHED:
            return FTB_EVENT(FTB_MPI_PROCS_CKPTED);

        case ORTE_SNAPC_CKPT_STATE_NO_CKPT:
        case ORTE_SNAPC_CKPT_STATE_ERROR:
            return FTB_EVENT(FTB_MPI_PROCS_CKPT_FAIL);

        /* Restart events */
        case ORTE_SNAPC_CKPT_STATE_RECOVERED:
            return FTB_EVENT(FTB_MPI_PROCS_RESTARTED);

        case ORTE_SNAPC_CKPT_STATE_NO_RESTART:
            return FTB_EVENT(FTB_MPI_PROCS_RESTART_FAIL);

        /* Process migration events */
        case ORTE_ERRMGR_MIGRATE_STATE_FINISH:
            return FTB_EVENT(FTB_MPI_PROCS_MIGRATED);

        case ORTE_ERRMGR_MIGRATE_STATE_ERROR:
        case ORTE_ERRMGR_MIGRATE_STATE_ERR_INPROGRESS:
            return FTB_EVENT(FTB_MPI_PROCS_MIGRATE_FAIL);

        default:
            return NULL;
        }
    } else {
        /* Handle process and communication failure events */
        switch (errnum) {
        case ORTE_ERR_CONNECTION_REFUSED:
        case ORTE_ERR_CONNECTION_FAILED:
        case ORTE_ERR_UNREACH:
        case ORTE_PROC_STATE_HEARTBEAT_FAILED:
            return FTB_EVENT(FTB_MPI_PROCS_UNREACHABLE);

        case ORTE_ERR_COMM_FAILURE:
        case ORTE_PROC_STATE_COMM_FAILED:
            return FTB_EVENT(FTB_MPI_PROCS_COMM_ERROR);

        case ORTE_PROC_STATE_FAILED_TO_START:
        case ORTE_PROC_STATE_CALLED_ABORT:
            return FTB_EVENT(FTB_MPI_PROCS_ABORTED);

        case ORTE_PROC_STATE_ABORTED:
        case ORTE_PROC_STATE_ABORTED_BY_SIG:
        case ORTE_PROC_STATE_TERM_WO_SYNC:
        case ORTE_PROC_STATE_TERMINATED:
        case ORTE_PROC_STATE_KILLED_BY_CMD:
            return FTB_EVENT(FTB_MPI_PROCS_DEAD);

        default:
            return NULL;
        }
    }

    return NULL;
}

/* Extracts the FTB payload (inside the brackets []) from notifier
 * message payload. 
 * For instance: "<FTB message [payload]>" would return "payload".
 */
static unsigned int extract_payload(char *dest, char *src, unsigned int size)
{
    unsigned int ret;
    char *lbrace, *rbrace;
    rbrace = strrchr(src, ']');
    lbrace = strchr(src, '[');

    if (NULL == rbrace || NULL == lbrace) {
        strncpy(dest, src, size);
        ret = size;
    } else {
        ret = rbrace - lbrace + 1;
        if (ret > size) {
            ret = size;
        }
        strncpy(dest, lbrace, ret);
    }
    return ret;
}

static void publish_ftb_event(orcm_notifier_base_severity_t severity, int errcode,
                              FTB_event_properties_t *eprop)
{
    int ret;
    const char *event_name;
    FTB_event_handle_t ehandle;

    /* Publish the event to the Fault Tolerant Backplane */
    event_name = get_ftb_event_name(errcode);
    if (NULL != event_name) {
        ret = FTB_Publish(ftb_client_handle, event_name, eprop, &ehandle);
        if (FTB_SUCCESS != ret) {
            orte_show_help("help-orcm-notifier-ftb.txt", "publish failed", true,
                           "FTB_Publish() failed", ret, get_ftb_event_severity(severity),
                           event_name, eprop->event_payload, errcode);
        }
    }
}

static void ftb_log(orcm_notifier_base_severity_t severity, int errcode, const char *msg,
                    va_list ap)
{
    char *payload;
    FTB_event_properties_t ev_prop;

    /* Only normal FTB events are supported currently. */
    ev_prop.event_type = (int) FTB_EVENT_NORMAL;

    /* Copy the event payload, if we have one */
    vasprintf(&payload, msg, ap);
    if (NULL != payload) {
        extract_payload(ev_prop.event_payload, payload, FTB_MAX_PAYLOAD_DATA);
        free(payload);
        publish_ftb_event(severity, errcode, &ev_prop);
    }
}

static void ftb_help(orcm_notifier_base_severity_t severity, int errcode,
                     const char *filename, const char *topic, va_list ap)
{
    char *payload;
    FTB_event_properties_t ev_prop;

    /* Only normal FTB events are supported currently. */
    ev_prop.event_type = (int) FTB_EVENT_NORMAL;

    payload = opal_show_help_vstring(filename, topic, false, ap);
    if (NULL != payload) {
        extract_payload(ev_prop.event_payload, payload, FTB_MAX_PAYLOAD_DATA);
        free(payload);
        publish_ftb_event(severity, errcode, &ev_prop);
    }
}

static void ftb_peer(orcm_notifier_base_severity_t severity, int errcode,
                     orcm_process_name_t *peer_proc, const char *msg,
                     va_list ap)
{
    char *payload, *peer_host;
    FTB_event_properties_t ev_prop;

    /* Only normal FTB events are supported currently. */
    ev_prop.event_type = (int) FTB_EVENT_NORMAL;

    peer_host = NULL;
    if (peer_proc) {
        peer_host = orte_proc_get_hostname(peer_proc);
        /* Ignore the peer_host for now. */
    }

    vasprintf(&payload, msg, ap);
    if (NULL != payload) {
        extract_payload(ev_prop.event_payload, payload, FTB_MAX_PAYLOAD_DATA);
        free(payload);
        publish_ftb_event(severity, errcode, &ev_prop);
    }
}
