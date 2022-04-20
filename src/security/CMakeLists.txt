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
if(ENABLE_SECURITY)
  add_subdirectory(api)
  add_subdirectory(core)

  if(ENABLE_SSL AND OPENSSL_FOUND)
    add_subdirectory(openssl)
    add_subdirectory(builtin_plugins)
  endif()
endif()
