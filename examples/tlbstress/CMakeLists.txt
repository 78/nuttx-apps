# ##############################################################################
# apps/examples/tlbstress/CMakeLists.txt
#
# SPDX-License-Identifier: Apache-2.0
# ##############################################################################

if(CONFIG_EXAMPLES_TLBSTRESS)
  nuttx_add_application(
    NAME
    ${CONFIG_EXAMPLES_TLBSTRESS_PROGNAME}
    SRCS
    tlbstress_main.c
    STACKSIZE
    ${CONFIG_EXAMPLES_TLBSTRESS_STACKSIZE}
    PRIORITY
    ${CONFIG_EXAMPLES_TLBSTRESS_PRIORITY})
endif()
