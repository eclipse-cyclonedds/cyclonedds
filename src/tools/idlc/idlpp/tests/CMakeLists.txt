# Copyright(c) 2026 ZettaScale Technology and others
# SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

include(CUnit)

add_cunit_executable(cunit_idlpp support.c expand.c)
# These tests include implementation sources and need the same warning policy.
idlpp_set_warning_options(cunit_idlpp)
target_link_libraries(cunit_idlpp PRIVATE idlpp ddsrt-internal)
target_compile_definitions(cunit_idlpp PRIVATE
  HAVE_CONFIG_H=1
  IDLPP_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data")
