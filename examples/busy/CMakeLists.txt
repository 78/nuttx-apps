# ##############################################################################
# apps/examples/busy/CMakeLists.txt
#
# SPDX-License-Identifier: Apache-2.0
# ##############################################################################

if(CONFIG_EXAMPLES_BUSY)
  nuttx_add_application(
    NAME
    ${CONFIG_EXAMPLES_BUSY_PROGNAME}
    SRCS
    busy_main.c
    STACKSIZE
    ${CONFIG_EXAMPLES_BUSY_STACKSIZE}
    PRIORITY
    ${CONFIG_EXAMPLES_BUSY_PRIORITY})
endif()
