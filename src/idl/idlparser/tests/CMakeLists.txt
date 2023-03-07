#
# Copyright(c) 2006 to 2021 ZettaScale Technology and others
#
# This program and the accompanying materials are made available under the
# terms of the Eclipse Public License v. 2.0 which is available at
# http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
# v. 1.0 which is available at
# http://www.eclipse.org/org/documents/edl-v10.php.
#
# SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
#
include(CUnit)

add_cunit_executable(cunit_idl
  file.c
  scanner.c
  annotation.c
  expression.c
  inheritance.c
  parser.c
  typedef.c
  union.c
  enum.c
  pragma.c
  module.c
  forward.c)

target_link_libraries(cunit_idl PRIVATE idl)
