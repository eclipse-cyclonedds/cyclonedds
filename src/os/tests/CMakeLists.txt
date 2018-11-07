#
# Copyright(c) 2006 to 2018 ADLINK Technology Limited and others
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

set(sources
  "atomics.c"
  "error_no.c"
  "heap.c"
  "ifaddrs.c"
  "iter.c"
  "mutex.c"
  "once.c"
  "report.c"
  "rwlock.c"
  "stdlib.c"
  "strtoll.c"
  "thread.c"
  "thread_cleanup.c"
  "strcasecmp.c")

add_cunit_executable(abstraction ${sources})
target_link_libraries(abstraction OSAPI)

