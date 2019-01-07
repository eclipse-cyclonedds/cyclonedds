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
#ifndef OS_RUSAGE_H
#define OS_RUSAGE_H

#include "os/os_defs.h"

typedef struct {
  os_time utime; /* User CPU time used. */
  os_time stime; /* System CPU time used. */
  size_t maxrss; /* Maximum resident set size in bytes. */
  size_t idrss; /* Integral unshared data size. Not maintained on (at least)
                   Linux and Windows. */
  size_t nvcsw; /* Voluntary context switches. Not maintained on Windows. */
  size_t nivcsw; /* Involuntary context switches. Not maintained on Windows. */
} os_rusage_t;

#define OS_RUSAGE_SELF 0
#define OS_RUSAGE_THREAD 1

_Pre_satisfies_((who == OS_RUSAGE_SELF) || \
                (who == OS_RUSAGE_THREAD))
_Success_(return == 0)
int os_getrusage(_In_ int who, _Out_ os_rusage_t *usage);

#endif /* OS_GETRUSAGE_H */
