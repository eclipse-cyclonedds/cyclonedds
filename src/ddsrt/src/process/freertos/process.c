// Copyright(c) 2006 to 2019 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include "dds/ddsrt/process.h"

#include <FreeRTOS.h>
#include <task.h>

ddsrt_pid_t
ddsrt_getpid(void)
{
  return xTaskGetCurrentTaskHandle();
}

char *
ddsrt_getprocessname(void)
{
  return pcTaskGetName(xTaskGetCurrentTaskHandle());
}
