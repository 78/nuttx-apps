# ##############################################################################
# apps/examples/e1net/CMakeLists.txt
#
# SPDX-License-Identifier: Apache-2.0
# ##############################################################################

if(CONFIG_EXAMPLES_E1NET)
  nuttx_add_application(
    NAME
    ${CONFIG_EXAMPLES_E1NET_PROGNAME}
    SRCS
    e1net_main.c
    STACKSIZE
    ${CONFIG_EXAMPLES_E1NET_STACKSIZE}
    PRIORITY
    ${CONFIG_EXAMPLES_E1NET_PRIORITY}
    DEPENDS
    mbedtls)
endif()
