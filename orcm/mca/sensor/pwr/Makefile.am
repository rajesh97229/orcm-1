#
# Copyright (c) 2013-2014 Intel, Inc. All rights reserved.
#
# $COPYRIGHT$
# 
# Additional copyrights may follow
# 
# $HEADER$
#

dist_orcmdata_DATA = help-orcm-sensor-pwr.txt

sources = \
        sensor_pwr.h \
        sensor_pwr.c \
        sensor_pwr_component.c

# Make the output library in this directory, and name it either
# mca_<type>_<name>.la (for DSO builds) or libmca_<type>_<name>.la
# (for static builds).

if MCA_BUILD_orcm_sensor_pwr_DSO
component_noinst =
component_install = mca_sensor_pwr.la
else
component_noinst = libmca_sensor_pwr.la
component_install =
endif

mcacomponentdir = $(orcmlibdir)
mcacomponent_LTLIBRARIES = $(component_install)
mca_sensor_pwr_la_SOURCES = $(sources)
mca_sensor_pwr_la_LDFLAGS = -module -avoid-version
mca_sensor_pwr_la_LIBS = -lm

noinst_LTLIBRARIES = $(component_noinst)
libmca_sensor_pwr_la_SOURCES =$(sources)
libmca_sensor_pwr_la_LDFLAGS = -module -avoid-version
libmca_sensor_pwr_la_LIBS = -lm
