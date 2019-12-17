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
#ifndef NN_TIME_H
#define NN_TIME_H

#include <stdint.h>

#include "dds/export.h"

#if defined (__cplusplus)
extern "C" {
#endif

#define T_NEVER 0x7fffffffffffffffll
#define T_MILLISECOND 1000000ll
#define T_SECOND (1000 * T_MILLISECOND)
#define T_MICROSECOND (T_MILLISECOND/1000)

typedef struct {
  int32_t seconds;
  uint32_t fraction;
} ddsi_time_t;
#define DDSI_TIME_INFINITE ((ddsi_time_t) { INT32_MAX, UINT32_MAX })
#define DDSI_TIME_INVALID ((ddsi_time_t) { -1, UINT32_MAX })

typedef ddsi_time_t ddsi_duration_t;

typedef struct {
  int64_t v;
} nn_mtime_t;

typedef struct {
  int64_t v;
} nn_wctime_t;

typedef struct {
  int64_t v;
} nn_etime_t;

#define NN_MTIME_NEVER ((nn_mtime_t) { T_NEVER })
#define NN_WCTIME_NEVER ((nn_wctime_t) { T_NEVER })
#define NN_ETIME_NEVER ((nn_etime_t) { T_NEVER })
#define NN_WCTIME_INVALID ((nn_wctime_t) { INT64_MIN })

int valid_ddsi_timestamp (ddsi_time_t t);

DDS_EXPORT nn_wctime_t now (void);       /* wall clock time */
DDS_EXPORT nn_mtime_t now_mt (void);     /* monotonic time */
DDS_EXPORT nn_etime_t now_et (void);     /* elapsed time */
DDS_EXPORT void mtime_to_sec_usec (int32_t * __restrict sec, int32_t * __restrict usec, nn_mtime_t t);
DDS_EXPORT void wctime_to_sec_usec (int32_t * __restrict sec, int32_t * __restrict usec, nn_wctime_t t);
DDS_EXPORT void etime_to_sec_usec (int32_t * __restrict sec, int32_t * __restrict usec, nn_etime_t t);
DDS_EXPORT nn_mtime_t mtime_round_up (nn_mtime_t t, int64_t round);
DDS_EXPORT nn_mtime_t add_duration_to_mtime (nn_mtime_t t, int64_t d);
DDS_EXPORT nn_wctime_t add_duration_to_wctime (nn_wctime_t t, int64_t d);
DDS_EXPORT nn_etime_t add_duration_to_etime (nn_etime_t t, int64_t d);

DDS_EXPORT ddsi_time_t nn_wctime_to_ddsi_time (nn_wctime_t t);
DDS_EXPORT nn_wctime_t nn_wctime_from_ddsi_time (ddsi_time_t x);
DDS_EXPORT ddsi_duration_t nn_to_ddsi_duration (int64_t t);
DDS_EXPORT int64_t nn_from_ddsi_duration (ddsi_duration_t x);

#if defined (__cplusplus)
}
#endif

#endif /* NN_TIME_H */
