/*
 * Copyright (c) 2012-2013 Los Alamos National Security, Inc. All rights reserved.
 * Copyright (c) 2014      Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 *
 */

#include "orcm_config.h"
#include "orcm/constants.h"

#include <time.h>
#include <string.h>
#include <sys/types.h>
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif
#include <stdio.h>
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "opal/class/opal_pointer_array.h"
#include "opal/util/argv.h"
#include "opal/util/output.h"
#include "opal_stdint.h"

#include "orcm/runtime/orcm_globals.h"

#include "orcm/mca/db/base/base.h"
#include "db_print.h"

/* Module API functions */
static int init(struct orcm_db_base_module_t *imod);
static void finalize(struct orcm_db_base_module_t *imod);
static int store(struct orcm_db_base_module_t *imod,
                 const char *primary_key,
                 opal_list_t *kvs);
static int record_data_samples(struct orcm_db_base_module_t *imod,
                               const char *hostname,
                               const struct tm *time_stamp,
                               const char *data_group,
                               opal_list_t *samples);
static int update_node_features(struct orcm_db_base_module_t *imod,
                                const char *hostname,
                                opal_list_t *features);

/* Internal helper functions */
static void print_value(const opal_value_t *kv, char *tbuf, size_t size);

mca_db_print_module_t mca_db_print_module = {
    {
        init,
        finalize,
        store,
        record_data_samples,
        update_node_features,
        NULL,
        NULL,
        NULL
    },
};

static int init(struct orcm_db_base_module_t *imod)
{
    mca_db_print_module_t *mod = (mca_db_print_module_t*)imod;

    if (0 == strcmp(mod->file, "-")) {
        mod->fp = stdout;
    } else if (0 == strcmp(mod->file, "+")) {
        mod->fp = stderr;
    } else if (NULL == (mod->fp = fopen(mod->file, "w"))) {
        opal_output(0, "ERROR: cannot open log file %s", mod->file);
        return ORCM_ERROR;
    }

    return ORCM_SUCCESS;
}

static void finalize(struct orcm_db_base_module_t *imod)
{
    mca_db_print_module_t *mod = (mca_db_print_module_t*)imod;

    if (NULL != mod->fp &&
        stdout != mod->fp &&
        stderr != mod->fp) {
        fclose(mod->fp);
    }
}

static int store(struct orcm_db_base_module_t *imod,
                 const char *primary_key,
                 opal_list_t *kvs)
{
    mca_db_print_module_t *mod = (mca_db_print_module_t*)imod;
    char **cmdargs=NULL, *vstr;
    char tbuf[1024];
    opal_value_t *kv;
    char **key_argv;
    int argv_count;
    int len;

    snprintf(tbuf, sizeof(tbuf), "%s=%s", "primary_key", primary_key);
    opal_argv_append_nosize(&cmdargs, tbuf);

    /* cycle through the provided values and print them */
    /* print the data in the following format: <key>=<value>:<units> */
    OPAL_LIST_FOREACH(kv, kvs, opal_value_t) {
        /* the key could include the units (<key>:<units>) */
        key_argv = opal_argv_split(kv->key, ':');
        argv_count = opal_argv_count(key_argv);
        if (0 != argv_count) {
            snprintf(tbuf, sizeof(tbuf), "%s=", key_argv[0]);
            len = strlen(tbuf);
        } else {
            /* no key :o */
            len = 0;
        }

        print_value(kv, tbuf + len, sizeof(tbuf) - len);

        if (argv_count > 1) {
            /* units were included, so add them to the buffer */
            len = strlen(tbuf);
            snprintf(tbuf + len, sizeof(tbuf) - len, ":%s", key_argv[1]);
        }

        opal_argv_append_nosize(&cmdargs, tbuf);

        opal_argv_free(key_argv);
    }

    /* assemble the value string */
    vstr = opal_argv_join(cmdargs, ',');

    /* print it */
    fprintf(mod->fp, "DB request: store; data:\n%s\n", vstr);
    free(vstr);
    opal_argv_free(cmdargs);

    return ORCM_SUCCESS;
}

static int record_data_samples(struct orcm_db_base_module_t *imod,
                              const char *hostname,
                              const struct tm *time_stamp,
                              const char *data_group,
                              opal_list_t *samples)
{
    mca_db_print_module_t *mod = (mca_db_print_module_t*)imod;
    orcm_metric_value_t *mv;

    char **cmdargs=NULL, *vstr;
    char time_str[40];
    char tbuf[1024];
    int len;

    snprintf(tbuf, sizeof(tbuf), "%s=%s", "hostname", hostname);
    opal_argv_append_nosize(&cmdargs, tbuf);

    strftime(time_str, sizeof(time_str), "%F %T%z", time_stamp);
    snprintf(tbuf, sizeof(tbuf), "%s=%s", "time_stamp", time_str);
    opal_argv_append_nosize(&cmdargs, tbuf);

    snprintf(tbuf, sizeof(tbuf), "%s=%s", "data_group", data_group);
    opal_argv_append_nosize(&cmdargs, tbuf);

    /* cycle through the provided values and print them */
    /* print the data in the following format: <key>=<value>:<units> */
    OPAL_LIST_FOREACH(mv, samples, orcm_metric_value_t) {
        if (NULL != mv->value.key) {
            snprintf(tbuf, sizeof(tbuf), "%s=", mv->value.key);
            len = strlen(tbuf);
        } else {
            /* no key :o */
            len = 0;
        }

        print_value(&mv->value, tbuf + len, sizeof(tbuf) - len);

        if (NULL != mv->units) {
            /* units were included, so add them to the buffer */
            len = strlen(tbuf);
            snprintf(tbuf + len, sizeof(tbuf) - len, ":%s", mv->units);
        }

        opal_argv_append_nosize(&cmdargs, tbuf);
    }

    /* assemble the value string */
    vstr = opal_argv_join(cmdargs, ',');

    /* print it */
    fprintf(mod->fp, "DB request: record_data_samples; data:\n%s\n", vstr);
    free(vstr);
    opal_argv_free(cmdargs);

    return ORCM_SUCCESS;
}

static int update_node_features(struct orcm_db_base_module_t *imod,
                                const char *hostname,
                                opal_list_t *features)
{
    mca_db_print_module_t *mod = (mca_db_print_module_t*)imod;
    orcm_metric_value_t *mv;

    char **cmdargs=NULL, *vstr;
    char tbuf[1024];
    int len;

    snprintf(tbuf, sizeof(tbuf), "%s=%s", "hostname", hostname);
    opal_argv_append_nosize(&cmdargs, tbuf);

    /* cycle through the provided values and print them */
    /* print the data in the following format: <key>=<value>:<units> */
    OPAL_LIST_FOREACH(mv, features, orcm_metric_value_t) {
        if (NULL != mv->value.key) {
            snprintf(tbuf, sizeof(tbuf), "%s=", mv->value.key);
            len = strlen(tbuf);
        } else {
            /* no key :o */
            len = 0;
        }

        print_value(&mv->value, tbuf + len, sizeof(tbuf) - len);

        if (NULL != mv->units) {
            /* units were included, so add them to the buffer */
            len = strlen(tbuf);
            snprintf(tbuf + len, sizeof(tbuf) - len, ":%s", mv->units);
        }

        opal_argv_append_nosize(&cmdargs, tbuf);
    }

    /* assemble the value string */
    vstr = opal_argv_join(cmdargs, ',');

    /* print it */
    fprintf(mod->fp, "DB request: update_node_features; data:\n%s\n", vstr);
    free(vstr);
    opal_argv_free(cmdargs);

    return ORCM_SUCCESS;
}

static void print_value(const opal_value_t *kv, char *tbuf, size_t size)
{
    time_t nowtime;
    struct tm nowtm;

    switch (kv->type) {
    case OPAL_STRING:
        snprintf(tbuf, size, "%s", kv->data.string);
        break;
    case OPAL_SIZE:
        snprintf(tbuf, size, "%" PRIsize_t "", kv->data.size);
        break;
    case OPAL_INT:
        snprintf(tbuf, size, "%d", kv->data.integer);
        break;
    case OPAL_INT8:
        snprintf(tbuf, size, "%" PRIi8 "", kv->data.int8);
        break;
    case OPAL_INT16:
        snprintf(tbuf, size, "%" PRIi16 "", kv->data.int16);
        break;
    case OPAL_INT32:
        snprintf(tbuf, size, "%" PRIi32 "", kv->data.int32);
        break;
    case OPAL_INT64:
        snprintf(tbuf, size, "%" PRIi64 "", kv->data.int64);
        break;
    case OPAL_UINT:
        snprintf(tbuf, size, "%u", kv->data.uint);
        break;
    case OPAL_UINT8:
        snprintf(tbuf, size, "%" PRIu8 "", kv->data.uint8);
        break;
    case OPAL_UINT16:
        snprintf(tbuf, size, "%" PRIu16 "", kv->data.uint16);
        break;
    case OPAL_UINT32:
        snprintf(tbuf, size, "%" PRIu32 "", kv->data.uint32);
        break;
    case OPAL_UINT64:
        snprintf(tbuf, size, "%" PRIu64 "", kv->data.uint64);
        break;
    case OPAL_PID:
        snprintf(tbuf, size, "%lu", (unsigned long)kv->data.pid);
        break;
    case OPAL_FLOAT:
        snprintf(tbuf, size, "%f", kv->data.fval);
        break;
    case OPAL_DOUBLE:
        snprintf(tbuf, size, "%f", kv->data.dval);
        break;
    case OPAL_TIMEVAL:
        /* we only care about seconds */
        nowtime = kv->data.tv.tv_sec;
        (void)localtime_r(&nowtime, &nowtm);
        strftime(tbuf, size, "%Y-%m-%d %H:%M:%S", &nowtm);
        break;
    default:
        snprintf(tbuf, size, "Unsupported type: %s",
                 opal_dss.lookup_data_type(kv->type));
        break;
    }
}
