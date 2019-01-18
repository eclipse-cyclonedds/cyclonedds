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
#ifndef DDSRT_RUSAGE_H
#define DDSRT_RUSAGE_H

#include <stddef.h>

#include "dds/ddsrt/time.h"
#include "dds/ddsrt/retcode.h"

typedef struct {
  dds_time_t utime; /* User CPU time used. */
  dds_time_t stime; /* System CPU time used. */
  size_t maxrss; /* Maximum resident set size in bytes. */
  size_t idrss; /* Integral unshared data size. Not maintained on (at least)
                   Linux and Windows. */
  size_t nvcsw; /* Voluntary context switches. Not maintained on Windows. */
  size_t nivcsw; /* Involuntary context switches. Not maintained on Windows. */
} ddsrt_rusage_t;

#define DDSRT_RUSAGE_SELF (0)
#define DDSRT_RUSAGE_THREAD (1)

/**
 * @brief Get resource usage for the current thread or process.
 *
 * @param[in]  who    DDSRT_RUSAGE_SELF or DDSRT_RUSAGE_THREAD.
 * @param[in]  usage  Structure where resource usage is returned.
 *
 * @returns A dds_retcode_t indicating success or failure.
 *
 * @retval DDS_RETCODE_OK
 *             Resource usage successfully returned in @usage.
 * @retval DDS_RETCODE_OUT_OF_RESOURCES
 *             There were not enough resources to get resource usage.
 * @retval DDS_RETCODE_ERROR
 *             An unidentified error occurred.
 */
DDS_EXPORT dds_retcode_t ddsrt_getrusage(int who, ddsrt_rusage_t *usage);

#endif /* DDSRT_RUSAGE_H */
