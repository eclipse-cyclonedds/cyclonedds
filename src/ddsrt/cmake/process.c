/*
 * Copyright(c) 2006 to 2018 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#include "cyclonedds/ddsrt/process.h"

#if DDSRT_HAVE_MULTI_PROCESS
# error "cmake_HAVE_MULTI_PROCESS=true"
#else
# error "cmake_HAVE_MULTI_PROCESS=false"
#endif
