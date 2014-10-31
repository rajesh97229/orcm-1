# -*- shell-script -*-
#
# Copyright (c) 2009-2010 Cisco Systems, Inc.  All rights reserved.
# Copyright (c) 2009-2010 The Trustees of Indiana University and Indiana
#                         University Research and Technology
#                         Corporation.  All rights reserved.
# Copyright (c) 2011-2012 Los Alamos National Security, LLC.  All rights
#                         reserved. 
# $COPYRIGHT$
# 
# Additional copyrights may follow
# 
# $HEADER$
#

AC_DEFUN([ORTE_CONFIG_FILES],[
    AC_CONFIG_FILES([
        orte/Makefile
        orte/include/Makefile
        orte/etc/Makefile
    
        orte/tools/orted/Makefile
    ])
])
