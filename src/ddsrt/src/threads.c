// Copyright(c) 2006 to 2019 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <assert.h>

#include "dds/ddsrt/threads.h"

void
ddsrt_threadattr_init (
  ddsrt_threadattr_t *tattr)
{
  assert(tattr != NULL);
  tattr->schedClass = DDSRT_SCHED_DEFAULT;
  tattr->schedPriority = 0;
  tattr->stackSize = 0;
}
