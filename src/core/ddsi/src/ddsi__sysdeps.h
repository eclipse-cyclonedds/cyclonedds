// Copyright(c) 2006 to 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDSI__SYSDEPS_H
#define DDSI__SYSDEPS_H

#include "dds/export.h"
#include "dds/ddsrt/threads.h"

#if defined (__cplusplus)
extern "C" {
#endif

#define ASSERT_RDLOCK_HELD(x) ((void) 0)
#define ASSERT_WRLOCK_HELD(x) ((void) 0)
#define ASSERT_MUTEX_HELD(x) ((void) 0)

struct ddsrt_log_cfg;

/** @component debug_support */
void ddsi_log_stacktrace (const struct ddsrt_log_cfg *logcfg, const char *name, ddsrt_thread_t tid);

#if defined (__cplusplus)
}
#endif

#endif /* DDSI__SYSDEPS_H */
