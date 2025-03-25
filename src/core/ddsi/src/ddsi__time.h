// Copyright(c) 2006 to 2020 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDSI__TIME_H
#define DDSI__TIME_H

#include <stdint.h>

#include "dds/export.h"
#include "dds/ddsrt/time.h"

#if defined (__cplusplus)
extern "C" {
#endif

// DDSI 2.2 and earlier have signed seconds
typedef struct {
  int32_t seconds;
  uint32_t fraction;
} ddsi_time22_t;
#define DDSI_TIME22_INFINITE_SECONDS INT32_MAX
#define DDSI_TIME22_INFINITE_FRACTION UINT32_MAX
#define DDSI_TIME22_INFINITE ((ddsi_time22_t) { DDSI_TIME22_INFINITE_SECONDS, DDSI_TIME22_INFINITE_FRACTION })
#define DDSI_TIME22_INVALID_SECONDS (-1)
#define DDSI_TIME22_INVALID_FRACTION UINT32_MAX
#define DDSI_TIME22_INVALID ((ddsi_time22_t) { DDSI_TIME22_INVALID_SECONDS, DDSI_TIME22_INVALID_FRACTION })

// DDSI 2.3 and later have unsigned seconds
//
// The CDR representation for TIME_INVALID is the same as that for TIME22_INVALID,
// so we can allow writing data with the source timestamp set to invalid without
// breaking compatibility.  This is useful because some DDS implementations do not
// provide a source timestamp when the destination order QoS is set to "by reception
// timestamp", which Cyclone turns into an "invalid timestamp", which one would then
// assume to be valid input to "dds_forwardcdr".
typedef struct {
  uint32_t seconds;
  uint32_t fraction;
} ddsi_time_t;
#define DDSI_TIME_INFINITE_SECONDS UINT32_MAX
#define DDSI_TIME_INFINITE_FRACTION (UINT32_MAX - 1)
#define DDSI_TIME_INVALID_SECONDS UINT32_MAX
#define DDSI_TIME_INVALID_FRACTION UINT32_MAX
#define DDSI_TIME_INFINITE ((ddsi_time_t) { DDSI_TIME_INFINITE_SECONDS, DDSI_TIME_INFINITE_FRACTION })
#define DDSI_TIME_INVALID ((ddsi_time_t) { DDSI_TIME_INVALID_SECONDS, DDSI_TIME_INVALID_FRACTION })

typedef struct {
  int32_t seconds;
  uint32_t fraction;
} ddsi_duration_t;
// duration "invalid" doesn't exist in the spec, we have it for convenience in sharing
// code between time22 and duration
#define DDSI_DURATION_INFINITE_SECONDS INT32_MAX
#define DDSI_DURATION_INFINITE_FRACTION UINT32_MAX
#define DDSI_DURATION_INVALID_SECONDS (-1)
#define DDSI_DURATION_INVALID_FRACTION UINT32_MAX
#define DDSI_DURATION_INFINITE ((ddsi_duration_t) { DDSI_DURATION_INFINITE_SECONDS, DDSI_DURATION_INFINITE_FRACTION })
#define DDSI_DURATION_INVALID ((ddsi_duration_t) { DDSI_DURATION_INVALID_SECONDS, DDSI_DURATION_INVALID_FRACTION })

/** @component time_utils */
bool ddsi_is_valid_timestamp (ddsi_time_t t);

/** @component time_utils */
ddsi_time_t ddsi_wctime_to_ddsi_time (ddsrt_wctime_t t);

/** @component time_utils */
ddsi_time22_t ddsi_wctime_to_ddsi_time22 (ddsrt_wctime_t t);

/** @component time_utils */
ddsrt_wctime_t ddsi_wctime_from_ddsi_time (ddsi_time_t x);

/** @component time_utils */
bool ddsi_is_valid_timestamp22 (ddsi_time22_t t);

/** @component time_utils */
ddsrt_wctime_t ddsi_wctime_from_ddsi_time22 (ddsi_time22_t x);

/** @component time_utils */
ddsi_duration_t ddsi_duration_from_dds (dds_duration_t t);

/** @component time_utils */
dds_duration_t ddsi_duration_to_dds (ddsi_duration_t x);

#if defined (__cplusplus)
}
#endif

#endif /* DDSI__TIME_H */
