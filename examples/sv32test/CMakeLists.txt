# ##############################################################################
# apps/examples/sv32test/CMakeLists.txt
#
# SPDX-License-Identifier: Apache-2.0
# ##############################################################################

if(CONFIG_EXAMPLES_SV32TEST)
  nuttx_add_application(
    NAME
    ${CONFIG_EXAMPLES_SV32TEST_PROGNAME}
    SRCS
    sv32test_main.c
    STACKSIZE
    ${CONFIG_EXAMPLES_SV32TEST_STACKSIZE}
    PRIORITY
    ${CONFIG_EXAMPLES_SV32TEST_PRIORITY})
endif()
