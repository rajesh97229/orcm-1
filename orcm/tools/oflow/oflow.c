/*
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
#include <errno.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif  /* HAVE_UNISTD_H */
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif  /*  HAVE_STDLIB_H */
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif  /* HAVE_SYS_STAT_H */
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif  /* HAVE_SYS_TYPES_H */
#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif  /* HAVE_SYS_WAIT_H */
#ifdef HAVE_STRING_H
#include <string.h>
#endif  /* HAVE_STRING_H */
#ifdef HAVE_DIRENT_H
#include <dirent.h>
#endif  /* HAVE_DIRENT_H */

#include "opal/dss/dss.h"
#include "opal/mca/mca.h"
#include "opal/mca/base/base.h"
#include "opal/mca/event/event.h"
#include "opal/util/basename.h"
#include "opal/util/cmd_line.h"
#include "opal/util/output.h"
#include "opal/util/opal_environ.h"
#include "opal/util/show_help.h"
#include "opal/mca/base/base.h"
#include "opal/runtime/opal.h"

#include "orte/runtime/orte_wait.h"
#include "orte/util/error_strings.h"
#include "orte/util/show_help.h"
#include "orte/mca/errmgr/errmgr.h"
#include "orte/mca/rml/rml.h"

#include "orcm/runtime/runtime.h"

#include "orcm/mca/analytics/base/base.h"

#define ORCM_MAX_LINE_LENGTH  4096

/******************
 * Local Functions
 ******************/
static int orcm_oflow_init(int argc, char *argv[]);
static int parse_args(int argc, char *argv[]);
static opal_value_t *oflow_parse_next_line(FILE *fp);
static void orcm_oflow_recv(int status, orte_process_name_t* sender,
                            opal_buffer_t* buffer, orte_rml_tag_t tag,
                            void* cbdata);

/*****************************************
 * Global Vars for Command line Arguments
 *****************************************/
typedef struct {
    bool help;
    bool verbose;
    char *file;
    int output;
} orcm_oflow_globals_t;

orcm_oflow_globals_t orcm_oflow_globals;

opal_cmd_line_init_t cmd_line_opts[] = {
    { NULL,
      'h', NULL, "help",
      0,
      &orcm_oflow_globals.help, OPAL_CMD_LINE_TYPE_BOOL,
      "This help message" },

    { NULL,
      'v', NULL, "verbose",
      0,
      &orcm_oflow_globals.verbose, OPAL_CMD_LINE_TYPE_BOOL,
      "Be Verbose" },
    
    { NULL,
      'f', NULL, "file",
      1,
      &orcm_oflow_globals.file, OPAL_CMD_LINE_TYPE_STRING,
      "Input file" },

    /* End of list */
    { NULL,
      '\0', NULL, NULL,
      0,
      NULL, OPAL_CMD_LINE_TYPE_NULL,
      NULL }
};

int
main(int argc, char *argv[])
{
    orte_rml_recv_cb_t xfer;
    opal_buffer_t *buf;
    int rc, i, n, wfid;
    orcm_analytics_cmd_flag_t command;
    FILE *fp;
    opal_value_t *oflow_value;
    opal_value_t **oflow_array = NULL;
    orte_process_name_t wf_agg;
    
    /* initialize, parse command line, and setup frameworks */
    orcm_oflow_init(argc, argv);
    
    if (NULL == (fp = fopen(orcm_oflow_globals.file, "r"))) {
        perror("Can't open workflow file");
        if (ORTE_SUCCESS != orcm_finalize()) {
            fprintf(stderr, "Failed orcm_finalize\n");
            exit(1);
        }
        return ORCM_ERR_BAD_PARAM;
    }
    
    i = 0;
    oflow_value = oflow_parse_next_line(fp);
    while(oflow_value) {
        if (0 == strncmp("VPID", oflow_value->key, ORCM_MAX_LINE_LENGTH)) {
            wf_agg.jobid = 0;
            wf_agg.vpid = (orte_vpid_t)strtol(oflow_value->data.string, (char **)NULL, 10);
            printf("Sending to %s\n", ORTE_NAME_PRINT(&wf_agg));
            free(oflow_value);
            oflow_value = oflow_parse_next_line(fp);
            continue;
        }
        printf("KEY: %s \n\tVALUE: %s\n", oflow_value->key, oflow_value->data.string);
        oflow_array = (opal_value_t**)realloc(oflow_array, (sizeof(oflow_array) + sizeof(opal_value_t*)));
        if (!oflow_array) {
            fclose(fp);
            free(oflow_value);
            return ORCM_ERR_OUT_OF_RESOURCE;
        }
        oflow_array[i] = oflow_value;
        oflow_value = oflow_parse_next_line(fp);
        i++;
    }
    
    fclose(fp);

    /* setup to receive the result */
    OBJ_CONSTRUCT(&xfer, orte_rml_recv_cb_t);
    xfer.active = true;
    orte_rml.recv_buffer_nb(ORTE_NAME_WILDCARD,
                            ORCM_RML_TAG_ANALYTICS,
                            ORTE_RML_NON_PERSISTENT,
                            orte_rml_recv_callback, &xfer);
    
    /* setup to recieve workflow output */
    orte_rml.recv_buffer_nb(ORTE_NAME_WILDCARD,
                            12345,
                            ORTE_RML_PERSISTENT,
                            orcm_oflow_recv,
                            NULL);

    buf = OBJ_NEW(opal_buffer_t);

    command = ORCM_ANALYTICS_WORKFLOW_CREATE;
    /* pack the alloc command flag */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &command, 1, OPAL_UINT8))) {
        goto ERROR;
    }
    /* pack the length of the array */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &i, 1, OPAL_INT))) {
        goto ERROR;
    }
    if (oflow_array) {
        /* pack the array */
        if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, oflow_array, i, OPAL_VALUE))) {
            goto ERROR;
        }
    }
    /* send it to the aggregator */
    if (ORTE_SUCCESS != (rc = orte_rml.send_buffer_nb(&wf_agg, buf,
                                                      ORCM_RML_TAG_ANALYTICS,
                                                      orte_rml_send_callback, NULL))) {
        goto ERROR;
    }

    /* unpack workflow id */
    ORTE_WAIT_FOR_COMPLETION(xfer.active);
    n=1;
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(&xfer.data, &wfid, &n, OPAL_INT))) {
        goto ERROR;
    }

    printf("Workflow created with id: %i\n", wfid);

    OBJ_DESTRUCT(&xfer);


    if (ORTE_SUCCESS != orcm_finalize()) {
        fprintf(stderr, "Failed orcm_finalize\n");
        exit(1);
    }

    return ORTE_SUCCESS;
    
ERROR:
    if (NULL != oflow_array) {
        for (n = 0; n < i; n++) {
            OBJ_RELEASE(oflow_array[n]);
        }
        free(oflow_array);
    }
    ORTE_ERROR_LOG(rc);
    OBJ_RELEASE(buf);
    OBJ_DESTRUCT(&xfer);
    return rc;
}

static int parse_args(int argc, char *argv[])
{
    int ret;
    opal_cmd_line_t cmd_line;
    orcm_oflow_globals_t tmp = { false,    /* help */
                                 false,    /* verbose */
                                 NULL,    /* verbose */
                                 -1 };     /* output */

    orcm_oflow_globals = tmp;

    /* Parse the command line options */
    opal_cmd_line_create(&cmd_line, cmd_line_opts);
    
    mca_base_cmd_line_setup(&cmd_line);
    ret = opal_cmd_line_parse(&cmd_line, false, argc, argv);
    
    if (OPAL_SUCCESS != ret) {
        if (OPAL_ERR_SILENT != ret) {
            fprintf(stderr, "%s: command line error (%s)\n", argv[0],
                    opal_strerror(ret));
        }
        return ret;
    }

    /**
     * Now start parsing our specific arguments
     */
    if (orcm_oflow_globals.help) {
        char *str, *args = NULL;
        args = opal_cmd_line_get_usage_msg(&cmd_line);
        str = opal_show_help_string("help-oflow.txt", "usage", true,
                                    args);
        if (NULL != str) {
            printf("%s", str);
            free(str);
        }
        free(args);
        /* If we show the help message, that should be all we do */
        exit(0);
    }
    
    if (!orcm_oflow_globals.file) {
        char *str, *args = NULL;
        args = opal_cmd_line_get_usage_msg(&cmd_line);
        str = opal_show_help_string("help-oflow.txt", "file", true,
                                    args);
        if (NULL != str) {
            printf("%s", str);
            free(str);
        }
        free(args);
        /* If we show the help message, that should be all we do */
        exit(0);
    }

    /*
     * Since this process can now handle MCA/GMCA parameters, make sure to
     * process them.
     */
    mca_base_cmd_line_process_args(&cmd_line, &environ, &environ);

    return ORTE_SUCCESS;
}

static int orcm_oflow_init(int argc, char *argv[]) 
{
    int ret;

    /*
     * Make sure to init util before parse_args
     * to ensure installdirs is setup properly
     * before calling mca_base_open();
     */
    if( ORTE_SUCCESS != (ret = opal_init_util(&argc, &argv)) ) {
        return ret;
    }

    /*
     * Parse Command Line Arguments
     */
    if (ORTE_SUCCESS != (ret = parse_args(argc, argv))) {
        return ret;
    }

    /*
     * Setup OPAL Output handle from the verbose argument
     */
    if( orcm_oflow_globals.verbose ) {
        orcm_oflow_globals.output = opal_output_open(NULL);
        opal_output_set_verbosity(orcm_oflow_globals.output, 10);
    } else {
        orcm_oflow_globals.output = 0; /* Default=STDERR */
    }

    ret = orcm_init(ORCM_TOOL);

    return ret;
}

/* get key/value from line */
static opal_value_t *oflow_parse_next_line(FILE *fp)
{
    char *ret, *ptr;
    char **tokens = NULL;
    char input[ORCM_MAX_LINE_LENGTH];
    size_t i;
    opal_value_t *tokenized;
    int array_length;
    
    ret = fgets(input, ORCM_MAX_LINE_LENGTH, fp);
    if (NULL != ret) {
        /* remove newline */
        input[strlen(input)-1] = '\0';
        /* strip leading spaces */
        ptr = input;
        for (i=0; i < strlen(input)-1; i++) {
            if (' ' != input[i]) {
                ptr = &input[i];
                break;
            }
        }
        tokens = opal_argv_split(ptr, ':');
        array_length = opal_argv_count(tokens);
        if (2 == array_length) {
            tokenized = (opal_value_t *)malloc(sizeof(opal_value_t));
            if (!tokenized) {
                opal_argv_free(tokens);
                return NULL;
            }
            tokenized->type = OPAL_STRING;
            tokenized->key = tokens[0];
            tokenized->data.string = tokens[1];
            opal_argv_free(tokens);
            return tokenized;
        }
        opal_argv_free(tokens);
    }
    
    return NULL;
}

static void orcm_oflow_recv(int status, orte_process_name_t* sender,
                            opal_buffer_t* buffer, orte_rml_tag_t tag,
                            void* cbdata)
{
    int rc, i, cnt, num_values;
    opal_value_t **value_list = NULL;
    char *output;
    
    /* unpack the number of values */
    cnt = 1;
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, &num_values,
                                              &cnt, OPAL_INT))) {
        ORTE_ERROR_LOG(rc);
    }

    /* unpack the values */
    /* FIXME change this to pack 2 at a time, analytic name and attributes
     * because thats what the other side is expecting 
     */
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, value_list,
                                              &num_values, OPAL_VALUE))) {
        ORTE_ERROR_LOG(rc);
    }
    
    if (value_list) {
        for (i = 0; i < num_values; i++) {
            if (ORTE_SUCCESS != (rc = opal_dss.print(&output, "OFLOW ", value_list[i],
                                                     OPAL_VALUE))) {
                ORTE_ERROR_LOG(rc);
                return;
            }
            if (output) {
                printf("%s\n", output);
                free(output);
            }
        }
    }
}
