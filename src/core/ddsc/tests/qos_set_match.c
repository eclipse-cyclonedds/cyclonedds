// Copyright(c) 2024 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "dds/dds.h"
#include "dds/ddsrt/environ.h"
#include "test_common.h"

enum check_mode {
  CM_UNSET,
  CM_SET,
  CM_SET_DEFAULT
};

static void durability_set (dds_qos_t * const q, int const * const v) {
  assert ((int) DDS_DURABILITY_VOLATILE <= v[0] && v[0] <= (int) DDS_DURABILITY_PERSISTENT);
  dds_qset_durability (q, (dds_durability_kind_t) v[0]);
}
static void durability_check (const enum check_mode check_mode, const dds_qos_t * const q, int const * const v) {
  assert ((int) DDS_DURABILITY_VOLATILE <= v[0] && v[0] <= (int) DDS_DURABILITY_PERSISTENT);
  dds_durability_kind_t k = (dds_durability_kind_t) ((int) DDS_DURABILITY_PERSISTENT - v[0]);
  CU_ASSERT_FATAL (dds_qget_durability (q, &k) == (check_mode != CM_UNSET));
  if (check_mode == CM_SET) {
    CU_ASSERT_FATAL ((int) k == v[0]);
  }
}
static void durability_invalid (dds_qos_t * const q, int const v) {
  dds_qset_durability (q, (dds_durability_kind_t) ((int) DDS_DURABILITY_PERSISTENT + v + 1));
}

// Use constants derived from line numbers to ensure different QoS settings use different values
// for attributes that can be set reasonably freely.
static const dds_duration_t max_blocking_time_offset = DDS_MSECS (__LINE__);
static void reliability_set (dds_qos_t * const q, int const * const v) {
  assert ((int) DDS_RELIABILITY_BEST_EFFORT <= v[0] && v[0] <= (int) DDS_RELIABILITY_RELIABLE);
  dds_qset_reliability (q, (dds_reliability_kind_t) v[0], max_blocking_time_offset);
}
static void reliability_check (const enum check_mode check_mode, const dds_qos_t * const q, int const * const v) {
  assert ((int) DDS_RELIABILITY_BEST_EFFORT <= v[0] && v[0] <= (int) DDS_RELIABILITY_RELIABLE);
  dds_reliability_kind_t k = (dds_reliability_kind_t) ((int) DDS_RELIABILITY_RELIABLE - v[0]);
  dds_duration_t d = -1;
  CU_ASSERT_FATAL (dds_qget_reliability (q, &k, &d) == (check_mode != CM_UNSET));
  if (check_mode == CM_SET) {
    CU_ASSERT_FATAL ((int) k == v[0]);
    CU_ASSERT_FATAL (d == max_blocking_time_offset);
  }
}
static void reliability_invalid (dds_qos_t * const q, int const v) {
  if (v == 0)
    dds_qset_reliability (q, DDS_RELIABILITY_RELIABLE, -1);
  else
    dds_qset_reliability (q, (dds_reliability_kind_t) ((int) DDS_RELIABILITY_RELIABLE + v), 0);
}

static const int latency_budget_offset = __LINE__;
static void latency_budget_set (dds_qos_t * const q, int const * const v) {
  dds_qset_latency_budget (q, v[0] + latency_budget_offset);
}
static void latency_budget_check (const enum check_mode check_mode, const dds_qos_t * const q, int const * const v) {
  dds_duration_t d = -1;
  CU_ASSERT_FATAL (dds_qget_latency_budget (q, &d) == (check_mode != CM_UNSET));
  if (check_mode == CM_SET) {
    CU_ASSERT_FATAL (d == v[0] + latency_budget_offset);
  }
}
static void latency_budget_invalid (dds_qos_t * const q, int const v) {
  (void)v;
  dds_qset_latency_budget (q, -1);
}

#if DDS_HAS_DEADLINE_MISSED
// deadline >= time_based_filter
static const int deadline_offset = DDS_MSECS (1) + __LINE__;
static void deadline_set (dds_qos_t * const q, int const * const v) {
  dds_qset_deadline (q, v[0] + deadline_offset);
}
static void deadline_check (const enum check_mode check_mode, const dds_qos_t * const q, int const * const v) {
  dds_duration_t d = -1;
  CU_ASSERT_FATAL (dds_qget_deadline (q, &d) == (check_mode != CM_UNSET));
  if (check_mode == CM_SET) {
    CU_ASSERT_FATAL (d == v[0] + deadline_offset);
  }
}
static void deadline_invalid (dds_qos_t * const q, int const v) {
  (void)v;
  dds_qset_deadline (q, -1);
}
#endif

// deadline >= time_based_filter
static const int time_based_filter_offset = __LINE__;
static void time_based_filter_set (dds_qos_t * const q, int const * const v) {
  dds_qset_time_based_filter (q, v[0] + time_based_filter_offset);
}
static void time_based_filter_check (const enum check_mode check_mode, const dds_qos_t * const q, int const * const v) {
  dds_duration_t d = -1;
  CU_ASSERT_FATAL (dds_qget_time_based_filter (q, &d) == (check_mode != CM_UNSET));
  if (check_mode == CM_SET) {
    CU_ASSERT_FATAL (d == v[0] + time_based_filter_offset);
  }
}
static void time_based_filter_invalid (dds_qos_t * const q, int const v) {
  (void)v;
  dds_qset_time_based_filter (q, -1);
}

static void ownership_set (dds_qos_t * const q, int const * const v) {
  assert ((int) DDS_OWNERSHIP_SHARED <= v[0] && v[0] <= (int) DDS_OWNERSHIP_EXCLUSIVE);
  dds_qset_ownership (q, (dds_ownership_kind_t) v[0]);
}
static void ownership_check (const enum check_mode check_mode, const dds_qos_t * const q, int const * const v) {
  assert ((int) DDS_OWNERSHIP_SHARED <= v[0] && v[0] <= (int) DDS_OWNERSHIP_EXCLUSIVE);
  dds_ownership_kind_t k = (dds_ownership_kind_t) ((int) DDS_OWNERSHIP_EXCLUSIVE - v[0]);
  CU_ASSERT_FATAL (dds_qget_ownership (q, &k) == (check_mode != CM_UNSET));
  if (check_mode == CM_SET) {
    CU_ASSERT_FATAL ((int) k == v[0]);
  }
}
static void ownership_invalid (dds_qos_t * const q, int const v) {
  dds_qset_ownership (q, (dds_ownership_kind_t) ((int) DDS_OWNERSHIP_EXCLUSIVE + v + 1));
}

static const int ownership_strength_offset = __LINE__;
static void ownership_strength_set (dds_qos_t * const q, int const * const v) {
  dds_qset_ownership_strength (q, v[0] + ownership_strength_offset);
}
static void ownership_strength_check (const enum check_mode check_mode, const dds_qos_t * const q, int const * const v) {
  int32_t k = -1;
  CU_ASSERT_FATAL (dds_qget_ownership_strength (q, &k) == (check_mode != CM_UNSET));
  if (check_mode == CM_SET) {
    CU_ASSERT_FATAL (k == v[0] + ownership_strength_offset);
  }
}
// no invalid value for ownership strength exists

static void destination_order_set (dds_qos_t * const q, int const * const v) {
  assert ((int) DDS_DESTINATIONORDER_BY_RECEPTION_TIMESTAMP <= v[0] && v[0] <= (int) DDS_DESTINATIONORDER_BY_SOURCE_TIMESTAMP);
  dds_qset_destination_order (q, (dds_destination_order_kind_t) v[0]);
}
static void destination_order_check (const enum check_mode check_mode, const dds_qos_t * const q, int const * const v) {
  assert ((int) DDS_DESTINATIONORDER_BY_RECEPTION_TIMESTAMP <= v[0] && v[0] <= (int) DDS_DESTINATIONORDER_BY_SOURCE_TIMESTAMP);
  dds_destination_order_kind_t k = (dds_destination_order_kind_t) ((int) DDS_DESTINATIONORDER_BY_SOURCE_TIMESTAMP - v[0]);
  CU_ASSERT_FATAL (dds_qget_destination_order (q, &k) == (check_mode != CM_UNSET));
  if (check_mode == CM_SET) {
    CU_ASSERT_FATAL ((int) k == v[0]);
  }
}
static void destination_order_invalid (dds_qos_t * const q, int const v) {
  dds_qset_destination_order (q, (dds_destination_order_kind_t) ((int) DDS_DESTINATIONORDER_BY_SOURCE_TIMESTAMP + v + 1));
}

#if DDS_HAS_LIFESPAN
static const int lifespan_offset = __LINE__;
static void lifespan_set (dds_qos_t * const q, int const * const v) {
  dds_qset_lifespan (q, v[0] + lifespan_offset);
}
static void lifespan_check (const enum check_mode check_mode, const dds_qos_t * const q, int const * const v) {
  dds_duration_t d = -1;
  CU_ASSERT_FATAL (dds_qget_lifespan (q, &d) == (check_mode != CM_UNSET));
  if (check_mode == CM_SET) {
    CU_ASSERT_FATAL (d == v[0] + lifespan_offset);
  }
}
static void lifespan_invalid (dds_qos_t * const q, int const v) {
  (void)v;
  dds_qset_lifespan (q, -1);
}
#endif

static const int transport_priority_offset = __LINE__;
static void transport_priority_set (dds_qos_t * const q, int const * const v) {
  dds_qset_transport_priority (q, v[0] + transport_priority_offset);
}
static void transport_priority_check (const enum check_mode check_mode, const dds_qos_t * const q, int const * const v) {
  int32_t d = -1;
  CU_ASSERT_FATAL (dds_qget_transport_priority (q, &d) == (check_mode != CM_UNSET));
  if (check_mode == CM_SET) {
    CU_ASSERT_FATAL (d == v[0] + transport_priority_offset);
  }
}
// no invalid value for transport priority exists

static const int history_depth = __LINE__;
static void history_set (dds_qos_t * const q, int const * const v) {
  assert ((int) DDS_HISTORY_KEEP_LAST <= v[0] && v[0] <= (int) DDS_HISTORY_KEEP_ALL);
  dds_qset_history (q, (dds_history_kind_t) v[0], history_depth);
}
static void history_check (const enum check_mode check_mode, const dds_qos_t * const q, int const * const v) {
  assert ((int) DDS_HISTORY_KEEP_LAST <= v[0] && v[0] <= (int) DDS_HISTORY_KEEP_ALL);
  dds_history_kind_t k = (dds_history_kind_t) ((int) DDS_HISTORY_KEEP_ALL - v[0]);
  int32_t d = -1;
  CU_ASSERT_FATAL (dds_qget_history (q, &k, &d) == (check_mode != CM_UNSET));
  if (check_mode == CM_SET) {
    CU_ASSERT_FATAL ((int) k == v[0]);
    CU_ASSERT_FATAL (d == history_depth);
  }
}
static void history_invalid (dds_qos_t * const q, int const v) {
  if (v == 0)
    dds_qset_history (q, DDS_HISTORY_KEEP_LAST, 0);
  else
    dds_qset_history (q, (dds_history_kind_t) ((int) DDS_HISTORY_KEEP_ALL + v + 1), 0);
}

static void liveliness_set (dds_qos_t * const q, int const * const v) {
  assert ((int) DDS_LIVELINESS_AUTOMATIC <= v[0] && v[0] <= (int) DDS_LIVELINESS_MANUAL_BY_TOPIC && v[1] >= 0);
  dds_qset_liveliness (q, (dds_liveliness_kind_t) v[0], DDS_SECS (1) + v[1]);
}
static void liveliness_check (const enum check_mode check_mode, const dds_qos_t * const q, int const * const v) {
  assert ((int) DDS_LIVELINESS_AUTOMATIC <= v[0] && v[0] <= (int) DDS_LIVELINESS_MANUAL_BY_TOPIC && v[1] >= 0);
  dds_liveliness_kind_t k = (dds_liveliness_kind_t) ((int) DDS_LIVELINESS_MANUAL_BY_TOPIC - v[0]);
  dds_duration_t d = -1;
  CU_ASSERT_FATAL (dds_qget_liveliness (q, &k, &d) == (check_mode != CM_UNSET));
  if (check_mode == CM_SET) {
    CU_ASSERT_FATAL ((int) k == v[0]);
    CU_ASSERT_FATAL (d == DDS_SECS (1) + v[1]);
  }
}
static void liveliness_invalid (dds_qos_t * const q, int const v) {
  if (v == 0)
    dds_qset_liveliness (q, DDS_LIVELINESS_MANUAL_BY_TOPIC, -1);
  else
    dds_qset_liveliness (q, (dds_liveliness_kind_t) ((int) DDS_LIVELINESS_MANUAL_BY_TOPIC + v + 1), DDS_SECS (1));
}

static int32_t resource_limits_cnv (const int v, const int f, const int o)
{
  assert (0 <= v && v <= 7);
  assert (0 <= f && f <= 2);
  int l;
  switch (f) {
    case 0: l = v % 2; break;
    case 1: l = (v/2) % 2; break;
    case 2: l = (v/4) % 2; break;
    default: abort ();
  }
  // 1000: so resource limits are always greater than what is required for the history settings used in this test
  // o: offset so we can put in unique values for "resource limits" and "durability service resource limits"
  return (l == 0) ? DDS_LENGTH_UNLIMITED : 1000 + l + o;
}
static void resource_limits_set (dds_qos_t * const q, int const * const v) {
  dds_qset_resource_limits (q, resource_limits_cnv (v[0],0,2), resource_limits_cnv (v[0],1,1), resource_limits_cnv (v[0],2,0));
}
static void resource_limits_check (const enum check_mode check_mode, const dds_qos_t * const q, int const * const v) {
  int32_t a = 0, b = 0, c = 0;
  CU_ASSERT_FATAL (dds_qget_resource_limits (q, &a, &b, &c) == (check_mode != CM_UNSET));
  if (check_mode == CM_SET) {
    CU_ASSERT_FATAL (a == resource_limits_cnv (v[0],0,2));
    CU_ASSERT_FATAL (b == resource_limits_cnv (v[0],1,1));
    CU_ASSERT_FATAL (c == resource_limits_cnv (v[0],2,0));
  }
}
static void resource_limits_invalid (dds_qos_t * const q, int const v) {
  assert (0 <= v && v <= 2);
  switch (v) {
    case 0: dds_qset_resource_limits (q, 0, DDS_LENGTH_UNLIMITED, DDS_LENGTH_UNLIMITED); break;
    case 1: dds_qset_resource_limits (q, DDS_LENGTH_UNLIMITED, 0, DDS_LENGTH_UNLIMITED); break;
    case 2: dds_qset_resource_limits (q, DDS_LENGTH_UNLIMITED, DDS_LENGTH_UNLIMITED, 0); break;
  }
}

static void presentation_set (dds_qos_t * const q, int const * const v) {
  assert ((int) DDS_PRESENTATION_INSTANCE <= v[0] && v[0] <= (int) DDS_PRESENTATION_GROUP);
  assert ((v[1] == 0 || v[1] == 1) && (v[2] == 0 || v[2] == 1));
  dds_qset_presentation (q, (dds_presentation_access_scope_kind_t) v[0], (bool) v[1], (bool) v[2]);
}
static void presentation_check (const enum check_mode check_mode, const dds_qos_t * const q, int const * const v) {
  assert ((int) DDS_PRESENTATION_INSTANCE <= v[0] && v[0] <= (int) DDS_PRESENTATION_GROUP);
  assert ((v[1] == 0 || v[1] == 1) && (v[2] == 0 || v[2] == 1));
  dds_presentation_access_scope_kind_t k = (dds_presentation_access_scope_kind_t) ((int) DDS_PRESENTATION_GROUP - v[0]);
  bool w = !v[1], x = !v[2];
  CU_ASSERT_FATAL (dds_qget_presentation (q, &k, &w, &x) == (check_mode != CM_UNSET));
  if (check_mode == CM_SET) {
    CU_ASSERT_FATAL ((int) k == v[0]);
    CU_ASSERT_FATAL (w == v[1]);
    CU_ASSERT_FATAL (x == v[2]);
  }
}
static void presentation_invalid (dds_qos_t * const q, int const v) {
  dds_qset_presentation (q, (dds_presentation_access_scope_kind_t) ((int) DDS_PRESENTATION_GROUP + v + 1), false, false);
  // can't pass in non-bools for coherent/ordered access arguments
}

static void partition_set (dds_qos_t * const q, int const * const v) {
  assert (0 <= v[0] && v[0] <= 2);
  if (v[0] == 0)
    dds_qset_partition1 (q, NULL);
  else
  {
    char name[13];
    snprintf (name, sizeof (name), "p%d", v[0]);
    dds_qset_partition1 (q, name);
  }
}
static void partition_check (const enum check_mode check_mode, const dds_qos_t * const q, int const * const v) {
  assert (0 <= v[0] && v[0] <= 2);
  uint32_t n = UINT32_MAX;
  char dummy, *dummyptr = &dummy, **ps = &dummyptr;
  bool r = dds_qget_partition (q, &n, &ps);
  CU_ASSERT_FATAL (r == (check_mode != CM_UNSET));
  if (check_mode == CM_SET) {
    if (v[0] == 0) {
      CU_ASSERT_FATAL (n == 0 && ps == NULL); // Beware: there is an open PR to change this case!
    } else {
      char name[13];
      snprintf (name, sizeof (name), "p%d", v[0]);
      CU_ASSERT_FATAL (n == 1 && strcmp (ps[0], name) == 0);
    }
  }
  if (r && n > 0) {
    dds_free (ps[0]);
    dds_free (ps);
  }
}
// no invalid value for partition exists

static void ignorelocal_set (dds_qos_t * const q, int const * const v) {
  assert ((int) DDS_IGNORELOCAL_NONE <= v[0] && v[0] <= (int) DDS_IGNORELOCAL_PROCESS);
  dds_qset_ignorelocal (q, (dds_ignorelocal_kind_t) v[0]);
}
static void ignorelocal_check (const enum check_mode check_mode, const dds_qos_t * const q, int const * const v) {
  assert ((int) DDS_IGNORELOCAL_NONE <= v[0] && v[0] <= (int) DDS_IGNORELOCAL_PROCESS);
  dds_ignorelocal_kind_t k = (dds_ignorelocal_kind_t) ((int) DDS_IGNORELOCAL_PROCESS - v[0]);
  CU_ASSERT_FATAL (dds_qget_ignorelocal (q, &k) == (check_mode != CM_UNSET));
  if (check_mode == CM_SET) {
    CU_ASSERT_FATAL ((int) k == v[0]);
  }
}
static void ignorelocal_invalid (dds_qos_t * const q, int const v) {
  dds_qset_ignorelocal (q, (dds_ignorelocal_kind_t) ((int) DDS_IGNORELOCAL_PROCESS + v + 1));
}

static void writer_batching_set (dds_qos_t * const q, int const * const v) {
  assert (0 <= v[0] && v[0] <= 1);
  dds_qset_writer_batching (q, (bool) v[0]);
}
static void writer_batching_check (const enum check_mode check_mode, const dds_qos_t * const q, int const * const v) {
  assert (0 <= v[0] && v[0] <= 1);
  bool k = (bool) (1 - v[0]);
  CU_ASSERT_FATAL (dds_qget_writer_batching (q, &k) == (check_mode != CM_UNSET));
  if (check_mode == CM_SET) {
    CU_ASSERT_FATAL ((int) k == v[0]);
  }
}
// no invalid value for writer batching exists: can't pass in garbage for a bool

static void writer_data_lifecycle_set (dds_qos_t * const q, int const * const v) {
  assert (0 <= v[0] && v[0] <= 1);
  dds_qset_writer_data_lifecycle (q, (bool) v[0]);
}
static void writer_data_lifecycle_check (const enum check_mode check_mode, const dds_qos_t * const q, int const * const v) {
  assert (0 <= v[0] && v[0] <= 1);
  bool k = (bool) (1 - v[0]);
  CU_ASSERT_FATAL (dds_qget_writer_data_lifecycle (q, &k) == (check_mode != CM_UNSET));
  if (check_mode == CM_SET) {
    CU_ASSERT_FATAL ((int) k == v[0]);
  }
}
// no invalid value for writer data lifecycle exists: can't pass in garbage for a bool

static void reader_data_lifecycle_set (dds_qos_t * const q, int const * const v) {
  assert (0 <= v[0] && 0 <= v[1]);
  dds_qset_reader_data_lifecycle (q, DDS_MSECS(100) + v[0], DDS_MSECS(200) + v[1]);
}
static void reader_data_lifecycle_check (const enum check_mode check_mode, const dds_qos_t * const q, int const * const v) {
  assert (0 <= v[0] && 0 <= v[1]);
  dds_duration_t k = -1, l = -1;
  CU_ASSERT_FATAL (dds_qget_reader_data_lifecycle (q, &k, &l) == (check_mode != CM_UNSET));
  if (check_mode == CM_SET) {
    CU_ASSERT_FATAL ((int) k == DDS_MSECS(100) + v[0]);
    CU_ASSERT_FATAL ((int) l == DDS_MSECS(200) + v[1]);
  }
}
static void reader_data_lifecycle_invalid (dds_qos_t * const q, int const v) {
  assert (0 <= v && v <= 1);
  switch (v) {
    case 0: dds_qset_reader_data_lifecycle (q, -1, 1); break;
    case 1: dds_qset_reader_data_lifecycle (q, 1, -1); break;
  }
}

static const dds_duration_t cleanup_service_delay_offset = DDS_MSECS(__LINE__);
static void durability_service_set (dds_qos_t * const q, int const * const v) {
  assert ((int) DDS_HISTORY_KEEP_LAST <= v[1] && v[1] <= (int) DDS_HISTORY_KEEP_ALL);
  dds_qset_durability_service (
    q,
    cleanup_service_delay_offset + v[0],
    (dds_history_kind_t) v[1], 2,    // history depth & resource limits must be compatible
    resource_limits_cnv (v[2],0,12), // which it is with depth = 2 and all resource limits
    resource_limits_cnv (v[2],1,11), // either UNLIMITED or plentiful
    resource_limits_cnv (v[2],2,10));
}
static void durability_service_check (const enum check_mode check_mode, const dds_qos_t * const q, int const * const v) {
  assert ((int) DDS_HISTORY_KEEP_LAST <= v[1] && v[1] <= (int) DDS_HISTORY_KEEP_ALL);
  dds_duration_t a = -1;
  dds_history_kind_t b = (dds_history_kind_t) ((int) DDS_HISTORY_KEEP_ALL - v[1]);
  int32_t c = 0, d = 0, e = 0, f = 0;
  CU_ASSERT_FATAL (dds_qget_durability_service (q, &a, &b, &c, &d, &e, &f) == (check_mode != CM_UNSET));
  if (check_mode == CM_SET) {
    CU_ASSERT_FATAL (a == cleanup_service_delay_offset + v[0]);
    CU_ASSERT_FATAL (b == (dds_history_kind_t) v[1]);
    CU_ASSERT_FATAL (c == 2);
    CU_ASSERT_FATAL (d == resource_limits_cnv (v[2],0,12));
    CU_ASSERT_FATAL (e == resource_limits_cnv (v[2],1,11));
    CU_ASSERT_FATAL (f == resource_limits_cnv (v[2],2,10));
  }
}
static void durability_service_invalid (dds_qos_t * const q, int const v) {
  switch (v) {
    case 0: dds_qset_durability_service (q, -1, DDS_HISTORY_KEEP_LAST, 1, DDS_LENGTH_UNLIMITED, DDS_LENGTH_UNLIMITED, DDS_LENGTH_UNLIMITED); break;
    case 1: dds_qset_durability_service (q, 0, DDS_HISTORY_KEEP_LAST, 0, DDS_LENGTH_UNLIMITED, DDS_LENGTH_UNLIMITED, DDS_LENGTH_UNLIMITED); break;
    case 2: dds_qset_durability_service (q, 0, DDS_HISTORY_KEEP_LAST, 1, 0, DDS_LENGTH_UNLIMITED, DDS_LENGTH_UNLIMITED); break;
    case 3: dds_qset_durability_service (q, 0, DDS_HISTORY_KEEP_LAST, 1, DDS_LENGTH_UNLIMITED, 0, DDS_LENGTH_UNLIMITED); break;
    case 4: dds_qset_durability_service (q, 0, DDS_HISTORY_KEEP_LAST, 1, DDS_LENGTH_UNLIMITED, DDS_LENGTH_UNLIMITED, 0); break;
    default: dds_qset_durability_service (q, 0, (dds_history_kind_t) ((int) DDS_HISTORY_KEEP_ALL + 1), 0, DDS_LENGTH_UNLIMITED, DDS_LENGTH_UNLIMITED, DDS_LENGTH_UNLIMITED); break;

  }
}

static void entity_name_set (dds_qos_t * const q, int const * const v) {
  assert (0 <= v[0] && v[0] <= 1);
  char name[13];
  snprintf (name, sizeof (name), "q%d", v[0]);
  dds_qset_entity_name (q, name);
}
static void entity_name_check (const enum check_mode check_mode, const dds_qos_t * const q, int const * const v) {
  assert (0 <= v[0] && v[0] <= 2);
  char dummy, *n = &dummy;
  bool r = dds_qget_entity_name (q, &n);
  // Deviation from pattern: entity name we expect to see only when we set it explicitly because we run
  // with entity auto-naming disabled (the default)
  CU_ASSERT_FATAL (r == (check_mode == CM_SET));
  if (check_mode == CM_SET) {
    char name[13];
    snprintf (name, sizeof (name), "q%d", v[0]);
    CU_ASSERT_FATAL (strcmp (n, name) == 0);
  }
  if (r) {
    dds_free (n);
  }
}
// no invalid value for entity name exists

#define QA_TP (1u << 0)
#define QA_PUB (1u << 1)
#define QA_SUB (1u << 2)
#define QA_RD (1u << 3)
#define QA_WR (1u << 4)

enum rxo_sense {
  RXO_INAPPLICABLE,
  RXO_IF_EQ,
  RXO_IF_RD_LEQ,
  RXO_IF_RD_GEQ,
  RXO_PARTITION,   // special for partition: like IF_EQ, but not really RxO (no incompatible QoS)
  RXO_IGNORELOCAL, // special for ignorelocal: weird rules, not really RxO (no incompatible QoS)
  RXO_DONTEVENTRY  // special for entity name: don't even try RxO matching on this
};

enum changeable {
  C_NO,    // immutable QoS
  C_YES,   // changeable in spec and impl
  C_UNSUPP // changeable in spec, not in impl
};

#define MAX_VALUES 3

struct qostable_elem {
  const char *name;
  void (*set) (dds_qos_t * const q, int const * const v);
  void (*check) (const enum check_mode check_mode, const dds_qos_t * const q, int const * const v);
  void (*invalid) (dds_qos_t * const q, int const v);
  dds_qos_policy_id_t policy_id;
  uint32_t appl;
  int max[MAX_VALUES];
  enum rxo_sense rxo[MAX_VALUES];
  int max_invalid;
  enum changeable changeable;
};

// Note: user/topic/group data covered by ddsc_userdata tests
// FIXME: property (qset_prop, qset_bprop)
// FIXME: type_consistency_enforcement
// FIXME: data_representation
// FIXME: psmx_instances
// FIXME: ignore_local (partial because it can be set on pp/pub/sub/rd/wr, we only apply to once)
#define QI(name) #name, name##_set, name##_check, name##_invalid
#define Q0(name) #name, name##_set, name##_check, NULL
#define P(NAME) DDS_##NAME##_QOS_POLICY_ID
static const struct qostable_elem qostable[] = {
  { QI(durability),            P(DURABILITY),            QA_TP | QA_RD | QA_WR, {3,0,0}, { RXO_IF_RD_LEQ }, 0, C_NO },
  { QI(reliability),           P(RELIABILITY),           QA_TP | QA_RD | QA_WR, {1,0,0}, { RXO_IF_RD_LEQ }, 1, C_NO },
  { QI(latency_budget),        P(LATENCYBUDGET),         QA_TP | QA_RD | QA_WR, {1,0,0}, { RXO_IF_RD_GEQ }, 0, C_UNSUPP },
#if DDS_HAS_DEADLINE_MISSED
  { QI(deadline),              P(DEADLINE),              QA_TP | QA_RD | QA_WR, {1,0,0}, { RXO_IF_RD_GEQ }, 0, C_UNSUPP },
#endif
  { QI(time_based_filter),     P(TIMEBASEDFILTER),               QA_RD,         {1,0,0}, { RXO_INAPPLICABLE }, 0, C_YES },
  { QI(ownership),             P(OWNERSHIP),             QA_TP | QA_RD | QA_WR, {1,0,0}, { RXO_IF_EQ }, 0, C_NO },
  { Q0(ownership_strength),    P(OWNERSHIPSTRENGTH),                     QA_WR, {1,0,0}, { RXO_INAPPLICABLE }, 0, C_YES },
  { QI(destination_order),     P(DESTINATIONORDER),      QA_TP | QA_RD | QA_WR, {1,0,0}, { RXO_IF_RD_LEQ }, 0, C_NO },
#if DDS_HAS_LIFESPAN
  { QI(lifespan),              P(LIFESPAN),              QA_TP |         QA_WR, {1,0,0}, { RXO_INAPPLICABLE }, 0, C_YES },
#endif
  { Q0(transport_priority),    P(TRANSPORTPRIORITY),     QA_TP |         QA_WR, {1,0,0}, { RXO_INAPPLICABLE }, 0, C_YES },
  { QI(history),               P(HISTORY),               QA_TP | QA_RD | QA_WR, {1,0,0}, { RXO_INAPPLICABLE }, 0, C_NO },
  { QI(liveliness),            P(LIVELINESS),            QA_TP | QA_RD | QA_WR, {2,1,0}, { RXO_IF_RD_LEQ, RXO_IF_RD_GEQ }, 1, C_NO },
  { QI(resource_limits),       P(RESOURCELIMITS),        QA_TP | QA_RD | QA_WR, {7,0,0}, { RXO_INAPPLICABLE }, 2, C_NO },
  { QI(presentation),          P(PRESENTATION),                QA_PUB | QA_SUB, {2,1,1}, { RXO_IF_RD_LEQ, RXO_IF_RD_LEQ, RXO_IF_RD_LEQ }, 0, C_NO },
  { Q0(partition),             P(PARTITION),                   QA_PUB | QA_SUB, {2,0,0}, { RXO_PARTITION }, 0, C_UNSUPP },
  { QI(ignorelocal),           P(INVALID),      QA_PUB | QA_SUB| QA_RD | QA_WR, {2,0,0}, { RXO_IGNORELOCAL }, 0, C_NO },
  { Q0(writer_batching),       P(INVALID),                               QA_WR, {1,0,0}, { RXO_INAPPLICABLE }, 0, C_NO },
  { Q0(writer_data_lifecycle), P(INVALID),                               QA_WR, {1,0,0}, { RXO_INAPPLICABLE }, 0, C_YES },
  { QI(reader_data_lifecycle), P(INVALID),                       QA_RD,         {1,1,0}, { RXO_INAPPLICABLE }, 1, C_YES },
  { QI(durability_service),    P(DURABILITYSERVICE),     QA_TP |         QA_WR, {1,1,7}, { RXO_INAPPLICABLE }, 6, C_NO },
  { Q0(entity_name),           P(INVALID),     QA_TP|QA_PUB|QA_SUB|QA_RD|QA_WR, {1,0,0}, { RXO_DONTEVENTRY }, 0, C_NO },
};
#undef P
#undef Q

CU_Test(ddsc_qos_set, zero)
{
  // Verify a newly created QoS has nothing set
  const int v0[MAX_VALUES] = { 0 };
  dds_qos_t *qos = dds_create_qos ();
  for (size_t i = 0; i < sizeof (qostable) / sizeof (qostable[0]); i++)
    qostable[i].check (CM_UNSET, qos, v0);
  dds_delete_qos (qos);
}

CU_Test(ddsc_qos_set, one)
{
  // Verify set/get on QoS object can handle each individual QoS with multiple values
  DDSRT_STATIC_ASSERT(MAX_VALUES == 3); // matters for loop nesting
  const int v0[MAX_VALUES] = { 0 };
  for (size_t i = 0; i < sizeof (qostable) / sizeof (qostable[0]); i++)
  {
    int v[MAX_VALUES];
    for (v[0] = 0; v[0] <= qostable[i].max[0]; v[0]++)
    {
      for (v[1] = 0; v[1] <= qostable[i].max[1]; v[1]++)
      {
        for (v[2] = 0; v[2] <= qostable[i].max[2]; v[2]++)
        {
          dds_qos_t *qos = dds_create_qos ();
          qostable[i].set (qos, v);
          for (size_t k = 0; k < sizeof (qostable) / sizeof (qostable[0]); k++)
            qostable[k].check ((i == k) ? CM_SET : CM_UNSET, qos, (i == k) ? v : v0);
          dds_delete_qos (qos);
        }
      }
    }
  }
}

CU_Test(ddsc_qos_set, two)
{
  // Verify we can set all pairs of QoS, setting one to "0" and one to "max"
  // to check that we don't accidentally read/write the one QoS's fields in
  // two different set/get functions
  DDSRT_STATIC_ASSERT(MAX_VALUES == 3); // matters for loop nesting
  const int v0[MAX_VALUES] = { 0 };
  for (size_t i = 0; i < sizeof (qostable) / sizeof (qostable[0]); i++)
  {
    for (size_t j = 0; j < sizeof (qostable) / sizeof (qostable[0]); j++)
    {
      if (i == j)
        continue;
      dds_qos_t *qos = dds_create_qos ();
      qostable[i].set (qos, v0);
      qostable[j].set (qos, qostable[j].max);
      for (size_t k = 0; k < sizeof (qostable) / sizeof (qostable[0]); k++)
      {
        if (k != i && k != j)
          qostable[k].check (CM_UNSET, qos, v0);
        else if (k == i)
          qostable[k].check (CM_SET, qos, v0);
        else
          qostable[k].check (CM_SET, qos, qostable[k].max);
      }
      dds_delete_qos (qos);
    }
  }
}

static dds_entity_t create_topic_wrapper (dds_entity_t base, const dds_qos_t *qos)
{
  char topicname[100];
  create_unique_topic_name ("ddsc_qosmatch_endpoint", topicname, sizeof (topicname));
  return dds_create_topic (base, &Space_Type1_desc, topicname, qos, NULL);
}

static dds_entity_t create_reader_wrapper (dds_entity_t base, const dds_qos_t *qos)
{
  return dds_create_reader (dds_get_parent (base), base, qos, NULL);
}

static dds_entity_t create_writer_wrapper (dds_entity_t base, const dds_qos_t *qos)
{
  return dds_create_writer (dds_get_parent (base), base, qos, NULL);
}

static dds_entity_t create_subscriber_wrapper (dds_entity_t base, const dds_qos_t *qos)
{
  return dds_create_subscriber (base, qos, NULL);
}

static dds_entity_t create_publisher_wrapper (dds_entity_t base, const dds_qos_t *qos)
{
  return dds_create_publisher (base, qos, NULL);
}

static void check_qos_entity_one (uint32_t check_mask, const dds_entity_t entity, const size_t i, int const * const v, const bool sparse_qos)
{
  const int v0[MAX_VALUES] = { 0 };
  dds_qos_t *qos = dds_create_qos ();
  const dds_return_t rc = dds_get_qos (entity, qos);
  CU_ASSERT_FATAL (rc == 0);
  for (size_t k = 0; k < sizeof (qostable) / sizeof (qostable[0]); k++)
  {
    if ((qostable[k].appl & check_mask) == 0)
      qostable[k].check (CM_UNSET, qos, v0);
    else if (k != i)
      qostable[k].check (sparse_qos ? CM_UNSET : CM_SET_DEFAULT, qos, v0);
    else
      qostable[k].check (CM_SET, qos, v);
  }
  dds_delete_qos (qos);
}

static void do_entity_one (const uint32_t appl_mask, const uint32_t check_mask, const bool sparse_qos, dds_entity_t base, dds_entity_t (* const create) (dds_entity_t base, const dds_qos_t *qos))
{
  // We just try setting everything to catch things that shouldn't be there
  (void)appl_mask;
  // Check that for each applicable QoS a create + dds_get_qos pair of operations
  // returns the value we set, and that for all other QoS it remains unset/defaulted
  // (which of the two depends on "sparse_qos").
  //
  // Use various values for each QoS to make sure we are not getting fooled by
  // the default values.
  for (size_t i = 0; i < sizeof (qostable) / sizeof (qostable[0]); i++)
  {
    DDSRT_STATIC_ASSERT(MAX_VALUES == 3); // matters for loop nesting
    int v[MAX_VALUES];
    for (v[0] = 0; v[0] <= qostable[i].max[0]; v[0]++)
    {
      for (v[1] = 0; v[1] <= qostable[i].max[1]; v[1]++)
      {
        for (v[2] = 0; v[2] <= qostable[i].max[2]; v[2]++)
        {
          printf ("%s: %d %d %d\n", qostable[i].name, v[0], v[1], v[2]);
          dds_qos_t *qos = dds_create_qos ();
          qostable[i].set (qos, v);
          const dds_entity_t ent = create (base, qos);
          CU_ASSERT_FATAL (ent > 0);
          dds_delete_qos (qos);
          check_qos_entity_one (check_mask, ent, i, v, sparse_qos);
          const dds_return_t rc = dds_delete (ent);
          CU_ASSERT_FATAL (rc == 0);
        }
      }
    }
  }
}

static void check_qos_entity_two (uint32_t check_mask, const dds_entity_t entity, const size_t i, const size_t j, const bool sparse_qos)
{
  const int v0[MAX_VALUES] = { 0 };
  dds_qos_t *qos = dds_create_qos ();
  const dds_return_t rc = dds_get_qos (entity, qos);
  CU_ASSERT_FATAL (rc == 0);
  for (size_t k = 0; k < sizeof (qostable) / sizeof (qostable[0]); k++)
  {
    if ((qostable[k].appl & check_mask) == 0)
      qostable[k].check (CM_UNSET, qos, v0);
    else if (k != i && k != j)
      qostable[k].check (sparse_qos ? CM_UNSET : CM_SET_DEFAULT, qos, v0);
    else if (k == i)
      qostable[k].check (CM_SET, qos, v0);
    else
      qostable[k].check (CM_SET, qos, qostable[k].max);
  }
  dds_delete_qos (qos);
}

static void do_entity_two (const uint32_t appl_mask, const uint32_t check_mask, const bool sparse_qos, dds_entity_t base, dds_entity_t (* const create) (dds_entity_t base, const dds_qos_t *qos))
{
  // Check that each pair QoS applicable to the entity type can be set independently,
  // and that a create + dds_get_qos pair of operations returns the correct values.
  //
  // Use two different values for each QoS to make sure we are not accidentally
  // passing a case where the two QoS alias to each other internally.  E.g., if QoS
  // A and B both map to local L, then setA(1);setB(1) would still pass, but
  // setA(0);setB(1) won't.
  const int v0[MAX_VALUES] = { 0 };
  for (size_t i = 0; i < sizeof (qostable) / sizeof (qostable[0]); i++)
  {
    if ((qostable[i].appl & appl_mask) == 0)
      continue;
    for (size_t j = 0; j < sizeof (qostable) / sizeof (qostable[0]); j++)
    {
      if ((qostable[j].appl & appl_mask) == 0)
        continue;
      if (i == j)
        continue;
      dds_qos_t *qos = dds_create_qos ();
      qostable[i].set (qos, v0);
      qostable[j].set (qos, qostable[j].max);
      const dds_entity_t ent = create (base, qos);
      CU_ASSERT_FATAL (ent > 0);
      dds_delete_qos (qos);
      check_qos_entity_two (check_mask, ent, i, j, sparse_qos);
      const dds_return_t rc = dds_delete (ent);
      CU_ASSERT_FATAL (rc == 0);
    }
  }
}

static void do_nonendpoint (const uint32_t appl_mask, const uint32_t check_mask, bool sparse_qos, void (* const do_entity) (const uint32_t appl_mask, const uint32_t check_mask, const bool sparse_qos, dds_entity_t base, dds_entity_t (* const create) (dds_entity_t base, const dds_qos_t *qos)), dds_entity_t (* const create) (dds_entity_t base, const dds_qos_t *qos))
{
  // Non-endpoint needs no topic
  const dds_entity_t dp = dds_create_participant (0, NULL, NULL);
  CU_ASSERT_FATAL (dp > 0);
  do_entity (appl_mask, check_mask, sparse_qos, dp, create);
  const dds_return_t rc = dds_delete (dp);
  CU_ASSERT_FATAL (rc == 0);
}

static void do_endpoint (const uint32_t appl_mask, const uint32_t check_mask, bool sparse_qos, void (* const do_entity) (const uint32_t appl_mask, const uint32_t check_mask, const bool sparse_qos, dds_entity_t base, dds_entity_t (* const create) (dds_entity_t base, const dds_qos_t *qos)), dds_entity_t (* const create) (dds_entity_t base, const dds_qos_t *qos))
{
  // Endpoints need a topic, can figure out participant from topic
  const dds_entity_t dp = dds_create_participant (0, NULL, NULL);
  CU_ASSERT_FATAL (dp > 0);
  char topicname[100];
  create_unique_topic_name ("ddsc_qosmatch_endpoint", topicname, sizeof (topicname));
  const dds_entity_t tp = dds_create_topic (dp, &Space_Type1_desc, topicname, NULL, NULL);
  CU_ASSERT_FATAL (tp > 0);
  do_entity (appl_mask, check_mask, sparse_qos, tp, create);
  const dds_return_t rc = dds_delete (dp);
  CU_ASSERT_FATAL (rc == 0);
}

CU_Test(ddsc_qos_set, topic_one)
{
  do_nonendpoint (QA_TP, QA_TP, true, do_entity_one, create_topic_wrapper);
}

CU_Test(ddsc_qos_set, subscriber_one)
{
  do_nonendpoint (QA_SUB, QA_SUB, false, do_entity_one, create_subscriber_wrapper);
}

CU_Test(ddsc_qos_set, publisher_one)
{
  do_nonendpoint (QA_PUB, QA_PUB, false, do_entity_one, create_publisher_wrapper);
}

CU_Test(ddsc_qos_set, reader_one)
{
  do_endpoint (QA_RD, QA_RD | QA_SUB, false, do_entity_one, create_reader_wrapper);
}

CU_Test(ddsc_qos_set, writer_one)
{
  do_endpoint (QA_WR, QA_WR | QA_PUB, false, do_entity_one, create_writer_wrapper);
}

CU_Test(ddsc_qos_set, topic_two)
{
  do_nonendpoint (QA_TP, QA_TP, true, do_entity_two, create_topic_wrapper);
}

CU_Test(ddsc_qos_set, subscriber_two)
{
  do_nonendpoint (QA_SUB, QA_SUB, false, do_entity_two, create_subscriber_wrapper);
}

CU_Test(ddsc_qos_set, publisher_two)
{
  do_nonendpoint (QA_PUB, QA_PUB, false, do_entity_two, create_publisher_wrapper);
}

CU_Test(ddsc_qos_set, reader_two)
{
  do_endpoint (QA_RD, QA_RD | QA_SUB, false, do_entity_two, create_reader_wrapper);
}

CU_Test(ddsc_qos_set, writer_two)
{
  do_endpoint (QA_WR, QA_WR | QA_PUB, false, do_entity_two, create_writer_wrapper);
}

static void do_entity_one_invalid (uint32_t appl_mask, const uint32_t check_mask, const bool sparse_qos, dds_entity_t base, dds_entity_t (* const create) (dds_entity_t base, const dds_qos_t *qos))
{
  (void)check_mask;
  (void)sparse_qos;
  // Check that for each applicable QoS setting garbage will cause the entity
  // creation to fail.
  for (size_t i = 0; i < sizeof (qostable) / sizeof (qostable[0]); i++)
  {
    if (qostable[i].invalid == NULL)
      continue;
    if ((qostable[i].appl & appl_mask) == 0)
      continue;
    for (int v = 0; v <= qostable[i].max_invalid; v++)
    {
      dds_qos_t *qos = dds_create_qos ();
      qostable[i].invalid (qos, v);
      const dds_entity_t ent = create (base, qos);
      CU_ASSERT_FATAL (ent < 0);
      dds_delete_qos (qos);
    }
  }
}

CU_Test(ddsc_qos_set, topic_one_invalid)
{
  do_nonendpoint (QA_TP, QA_TP, true, do_entity_one_invalid, create_topic_wrapper);
}

CU_Test(ddsc_qos_set, subscriber_one_invalid)
{
  do_nonendpoint (QA_SUB, QA_SUB, false, do_entity_one_invalid, create_subscriber_wrapper);
}

CU_Test(ddsc_qos_set, publisher_one_invalid)
{
  do_nonendpoint (QA_PUB, QA_PUB, false, do_entity_one_invalid, create_publisher_wrapper);
}

CU_Test(ddsc_qos_set, reader_one_invalid)
{
  do_endpoint (QA_RD, QA_RD | QA_SUB, false, do_entity_one_invalid, create_reader_wrapper);
}

CU_Test(ddsc_qos_set, writer_one_invalid)
{
  do_endpoint (QA_WR, QA_WR | QA_PUB, false, do_entity_one_invalid, create_writer_wrapper);
}

static void check_qos (const bool is_appl, struct qostable_elem const * const te, dds_entity_t entity, int const * const v)
{
  dds_qos_t *qos = dds_create_qos ();
  dds_return_t rc;
  rc = dds_get_qos (entity, qos);
  CU_ASSERT_FATAL (rc == 0);
  te->check (is_appl ? CM_SET : CM_UNSET, qos, v);
  dds_delete_qos (qos);
}

static void do_entity_one_change (uint32_t appl_mask, const uint32_t check_mask, const bool sparse_qos, dds_entity_t base, dds_entity_t (* const create) (dds_entity_t base, const dds_qos_t *qos))
{
  const int v0[MAX_VALUES] = { 0 };
  const int v1[MAX_VALUES] = { 1 }; // intentionally {1,0...}
  (void)check_mask;
  (void)sparse_qos;
  // Check that changes to mutable QoS (in spec & impl) are allowed and that changes
  // to the others are rejected.  Check both the return value of dds_set_qos and the
  // QoS after the (attempted) change.
  for (size_t i = 0; i < sizeof (qostable) / sizeof (qostable[0]); i++)
  {
    if ((qostable[i].appl & appl_mask) == 0)
      continue;
    dds_qos_t *qos = dds_create_qos ();
    qostable[i].set (qos, v0);
    const dds_entity_t ent = create (base, qos);
    CU_ASSERT_FATAL (ent > 0);
    qostable[i].set (qos, v1);
    dds_return_t rc = dds_set_qos (ent, qos);
    dds_return_t expected = 0;
    switch (qostable[i].changeable)
    {
      case C_YES: expected = 0; break;
      case C_NO: expected = DDS_RETCODE_IMMUTABLE_POLICY; break;
      case C_UNSUPP: expected = DDS_RETCODE_UNSUPPORTED; break;
    }
    CU_ASSERT_FATAL (rc == expected);
    dds_delete_qos (qos);
    check_qos (true, &qostable[i], ent, (rc == 0) ? v1 : v0);
    rc = dds_delete (ent);
    CU_ASSERT_FATAL (rc == 0);
  }
}

CU_Test(ddsc_qos_set, topic_one_change)
{
  do_nonendpoint (QA_TP, QA_TP, true, do_entity_one_change, create_topic_wrapper);
}

CU_Test(ddsc_qos_set, subscriber_one_change)
{
  do_nonendpoint (QA_SUB, QA_SUB, false, do_entity_one_change, create_subscriber_wrapper);
}

CU_Test(ddsc_qos_set, publisher_one_change)
{
  do_nonendpoint (QA_PUB, QA_PUB, false, do_entity_one_change, create_publisher_wrapper);
}

CU_Test(ddsc_qos_set, reader_one_change)
{
  do_endpoint (QA_RD, QA_RD | QA_SUB, false, do_entity_one_change, create_reader_wrapper);
}

CU_Test(ddsc_qos_set, writer_one_change)
{
  do_endpoint (QA_WR, QA_WR | QA_PUB, false, do_entity_one_change, create_writer_wrapper);
}

static void sync_on_discovery (const dds_entity_t dprd, const dds_entity_t dpwr)
{
  // Use a new, unique topic for each pair: the next-easiest way to prevent trouble
  // from the discovery running asynchronously and us continuing based on a match
  // with an already-deleted remote endpoint.  The easiest way would be a unique
  // partition name, but the QoS mechanism is the thing we're testing.
  char topicname[100];
  dds_return_t rc;
  create_unique_topic_name ("ddsc_qosmatch_endpoint_check", topicname, sizeof (topicname));
  const dds_entity_t tpckrd = dds_create_topic (dprd, &Space_Type1_desc, topicname, NULL, NULL);
  CU_ASSERT_FATAL (tpckrd > 0);
  const dds_entity_t tpckwr = dds_create_topic (dpwr, &Space_Type1_desc, topicname, NULL, NULL);
  CU_ASSERT_FATAL (tpckwr > 0);
  const dds_entity_t rd = dds_create_reader (dprd, tpckrd, NULL, NULL);
  CU_ASSERT_FATAL (rd > 0);
  rc = dds_set_status_mask (rd, DDS_SUBSCRIPTION_MATCHED_STATUS);
  CU_ASSERT_FATAL (rc == 0);
  const dds_entity_t wr = dds_create_writer (dpwr, tpckwr, NULL, NULL);
  CU_ASSERT_FATAL (wr > 0);
  rc = dds_set_status_mask (wr, DDS_PUBLICATION_MATCHED_STATUS);
  CU_ASSERT_FATAL (rc == 0);
  const dds_entity_t ws = dds_create_waitset (DDS_CYCLONEDDS_HANDLE);
  CU_ASSERT_FATAL (ws > 0);
  rc = dds_waitset_attach (ws, rd, 0);
  CU_ASSERT_FATAL (rc == 0);
  rc = dds_waitset_wait (ws, NULL, 0, DDS_INFINITY);
  CU_ASSERT_FATAL (rc == 1);
  CU_ASSERT_FATAL (dds_get_matched_publications (rd, NULL, 0) == 1);
  rc = dds_waitset_detach (ws, rd);
  CU_ASSERT_FATAL (rc == 0);
  rc = dds_waitset_attach (ws, wr, 0);
  CU_ASSERT_FATAL (rc == 0);
  rc = dds_waitset_wait (ws, NULL, 0, DDS_INFINITY);
  CU_ASSERT_FATAL (rc == 1);
  CU_ASSERT_FATAL (dds_get_matched_subscriptions (wr, NULL, 0) == 1);
  rc = dds_waitset_detach (ws, wr);
  CU_ASSERT_FATAL (rc == 0);
  rc = dds_delete (ws);
  CU_ASSERT_FATAL (rc == 0);
  rc = dds_delete (rd);
  CU_ASSERT_FATAL (rc == 0);
  rc = dds_delete (wr);
  CU_ASSERT_FATAL (rc == 0);
  rc = dds_delete (tpckrd);
  CU_ASSERT_FATAL (rc == 0);
  rc = dds_delete (tpckwr);
  CU_ASSERT_FATAL (rc == 0);
}

static void do_ddsc_qos_set_endpoints_with_rxo (const dds_entity_t dprd, const dds_entity_t dpwr)
{
  // 1. Check that for each QoS applicable to readers/writers that creating endpoints
  //    and reading back the QoS yields the correct value.  Use various values for
  //    each QoS to make sure we are not getting fooled by the default values.
  //
  // 2. Check that RxO matching is respected.  Technically this ought to be a different
  //    test but the setup would just be another round through 1.
  dds_return_t rc;
  for (size_t i = 0; i < sizeof (qostable) / sizeof (qostable[0]); i++)
  {
    if (qostable[i].rxo[0] == RXO_DONTEVENTRY)
      continue;
    // All QoS apply to at least a reader, writer, publisher or subscriber
    assert ((qostable[i].appl & (QA_RD | QA_WR | QA_PUB | QA_SUB)) != 0);
    const bool is_rd = (qostable[i].appl & QA_RD) != 0;
    const bool is_wr = (qostable[i].appl & QA_WR) != 0;
    const bool is_sub = (qostable[i].appl & QA_SUB) != 0;
    const bool is_pub = (qostable[i].appl & QA_PUB) != 0;
    // No QoS applies to (reader or writer) and (publisher or subscriber), except
    // ignore_local, and there we don't mind setting it on both sub/pub and rd/wr
    assert ((is_rd || is_wr) != (is_sub || is_pub) || qostable[i].set == ignorelocal_set);
    const bool one_wr = !(is_wr || is_pub);
    const bool one_rd = !(is_rd || is_sub);
    // Must be looping over multiple values for at least one of { writer, reader }
    assert (!(one_wr && one_rd));
    DDSRT_STATIC_ASSERT(MAX_VALUES == 3); // matters for loop nesting
    int vrd[3], vwr[3];
    for (vwr[0] = 0; vwr[0] <= (one_wr ? 0 : qostable[i].max[0]); vwr[0]++)
    {
      for (vwr[1] = 0; vwr[1] <= (one_wr ? 0 : qostable[i].max[1]); vwr[1]++)
      {
        for (vwr[2] = 0; vwr[2] <= (one_wr ? 0 : qostable[i].max[2]); vwr[2]++)
        {
          for (vrd[0] = (one_rd ? vwr[0] : 0); vrd[0] <= (one_rd ? vwr[0] : qostable[i].max[0]); vrd[0]++)
          {
            for (vrd[1] = (one_rd ? vwr[1] : 0); vrd[1] <= (one_rd ? vwr[1] : qostable[i].max[1]); vrd[1]++)
            {
              for (vrd[2] = (one_rd ? vwr[2] : 0); vrd[2] <= (one_rd ? vwr[2] : qostable[i].max[2]); vrd[2]++)
              {
                // Use a new, unique topic for each pair: the next-easiest way to prevent trouble
                // from the discovery running asynchronously and us continuing based on a match
                // with an already-deleted remote endpoint.  The easiest way would be a unique
                // partition name, but the QoS mechanism is the thing we're testing.
                char topicname[100];
                create_unique_topic_name ("ddsc_qosmatch_endpoint", topicname, sizeof (topicname));
                const dds_entity_t tprd = dds_create_topic (dprd, &Space_Type1_desc, topicname, NULL, NULL);
                CU_ASSERT_FATAL (tprd > 0);
                const dds_entity_t tpwr = dds_create_topic (dpwr, &Space_Type1_desc, topicname, NULL, NULL);
                CU_ASSERT_FATAL (tpwr > 0);

                dds_qos_t *qos;
                printf ("%s: wr %d %d %d rd %d %d %d", qostable[i].name, vwr[0], vwr[1], vwr[2], vrd[0], vrd[1], vrd[2]);
                fflush (stdout);

                qos = dds_create_qos ();
                qostable[i].set (qos, vrd);
                const dds_entity_t sub = dds_create_subscriber (dprd, is_sub ? qos : NULL, NULL);
                CU_ASSERT_FATAL (sub > 0);
                const dds_entity_t rd = dds_create_reader (sub, tprd, is_rd ? qos : NULL, NULL);
                CU_ASSERT_FATAL (rd > 0);
                dds_delete_qos (qos);

                check_qos (is_sub, &qostable[i], sub, vrd);
                // reader does store local copies of subscriber QoS for partition, presentation
                // and ignorelocal because those are used in discovery
                check_qos (is_sub || is_rd, &qostable[i], rd, vrd);

                qos = dds_create_qos ();
                qostable[i].set (qos, vwr);
                const dds_entity_t pub = dds_create_publisher (dpwr, is_pub ? qos : NULL, NULL);
                CU_ASSERT_FATAL (pub > 0);
                const dds_entity_t wr = dds_create_writer (pub, tpwr, is_wr ? qos : NULL, NULL);
                CU_ASSERT_FATAL (wr > 0);
                dds_delete_qos (qos);
                check_qos (is_pub, &qostable[i], pub, vwr);
                // writer does store local copies of publisher QoS for partition, presentation
                // and ignorelocal because those are used in discovery
                check_qos (is_pub || is_wr, &qostable[i], wr, vwr);

                // If remote endpoints, wait until matching info is correct.  We do this by creating
                // an additional rd/wr pair with a different topic.  The DDSI discovery protocol has
                // to process these after processing the test reader/writer, and so once this second
                // pair matches, the matching info for the first pair is also reliable, including in
                // the case of an RxO mismatch.
                if (dds_get_parent (dprd) != dds_get_parent (dpwr))
                  sync_on_discovery (dprd, dpwr);

                // Compute whether the endpoints match based on the RxO rules in the table, and
                // whether this will give rise to an "incompatible QoS" notification.  The latter
                // is usually the case, but not for partition and ignore_local because those are
                // considered intentional non-matches.
                bool match = true;
                bool expect_incompatible_qos = false;
                for (size_t m = 0; m < sizeof (qostable[i].rxo) / sizeof (qostable[i].rxo[0]); m++)
                {
                  switch (qostable[i].rxo[m])
                  {
                    case RXO_INAPPLICABLE:
                    case RXO_DONTEVENTRY:
                      break;
                    case RXO_IF_EQ: // match iff rd and wr have same value set
                      if (vrd[m] != vwr[m]) {
                        expect_incompatible_qos = true;
                        match = false;
                      }
                      break;
                    case RXO_IF_RD_GEQ: // match iff rd has at least wr value set
                      if (vrd[m] < vwr[m]) {
                        expect_incompatible_qos = true;
                        match = false;
                      }
                      break;
                    case RXO_IF_RD_LEQ: // match iff rd has at most wr value set
                      if (vrd[m] > vwr[m]) {
                        expect_incompatible_qos = true;
                        match = false;
                      }
                      break;
                    case RXO_PARTITION: // match only if equal, no incompatible QoS
                      if (vrd[m] != vwr[m])
                        match = false;
                      break;
                    case RXO_IGNORELOCAL: { // use participant, domain handles to figure it out, no incompatible QoS
                      bool ilmatch;
                      if (vrd[m] == (int) DDS_IGNORELOCAL_PROCESS || vwr[m] == (int) DDS_IGNORELOCAL_PROCESS)
                        ilmatch = (dds_get_parent (dprd) != dds_get_parent (dpwr));
                      else if (vrd[m] == (int) DDS_IGNORELOCAL_PARTICIPANT || vwr[m] == (int) DDS_IGNORELOCAL_PARTICIPANT)
                        ilmatch = (dprd != dpwr);
                      else
                        ilmatch = true;
                      if (!ilmatch)
                        match = false;
                      break;
                    }
                  }
                }
                printf (" match %d", match);
                fflush (stdout);
                rc = dds_get_matched_publications (rd, NULL, 0);
                CU_ASSERT_FATAL (rc == (int) match);
                rc = dds_get_matched_subscriptions (wr, NULL, 0);
                CU_ASSERT_FATAL (rc == (int) match);
                if (expect_incompatible_qos)
                {
                  dds_offered_incompatible_qos_status_t oiq;
                  dds_requested_incompatible_qos_status_t riq;
                  rc = dds_get_offered_incompatible_qos_status (wr, &oiq);
                  CU_ASSERT_FATAL (rc == 0);
                  rc = dds_get_requested_incompatible_qos_status (rd, &riq);
                  CU_ASSERT_FATAL (rc == 0);
                  CU_ASSERT_FATAL (oiq.last_policy_id == qostable[i].policy_id);
                  CU_ASSERT_FATAL (riq.last_policy_id == qostable[i].policy_id);
                }
                rc = dds_delete (wr);
                CU_ASSERT_FATAL (rc == 0);
                rc = dds_delete (rd);
                CU_ASSERT_FATAL (rc == 0);
                rc = dds_delete (tpwr);
                CU_ASSERT_FATAL (rc == 0);
                rc = dds_delete (tprd);
                CU_ASSERT_FATAL (rc == 0);
                printf ("\n");
              }
            }
          }
        }
      }
    }
  }
}

CU_Test(ddsc_qos_set, local_endpoints_with_rxo)
{
  const char *config = "${CYCLONEDDS_URI},<Discovery><Tag>${CYCLONEDDS_PID}</Tag></Discovery>";
  char *conf = ddsrt_expand_envvars (config, 0);
  const dds_entity_t dom = dds_create_domain (0, conf);
  CU_ASSERT_FATAL (dom > 0);
  ddsrt_free (conf);
  const dds_entity_t dp = dds_create_participant (0, NULL, NULL);
  CU_ASSERT_FATAL (dp > 0);
  dds_return_t rc;
  do_ddsc_qos_set_endpoints_with_rxo (dp, dp);
  rc = dds_delete (dom);
  CU_ASSERT_FATAL (rc == 0);
}

CU_Test(ddsc_qos_set, semilocal_endpoints_with_rxo)
{
  const char *config = "${CYCLONEDDS_URI},<Discovery><Tag>${CYCLONEDDS_PID}</Tag></Discovery>";
  char *conf = ddsrt_expand_envvars (config, 0);
  const dds_entity_t dom = dds_create_domain (0, conf);
  CU_ASSERT_FATAL (dom > 0);
  ddsrt_free (conf);
  const dds_entity_t dprd = dds_create_participant (0, NULL, NULL);
  CU_ASSERT_FATAL (dprd > 0);
  const dds_entity_t dpwr = dds_create_participant (0, NULL, NULL);
  CU_ASSERT_FATAL (dpwr > 0);
  do_ddsc_qos_set_endpoints_with_rxo (dprd, dpwr);
  dds_return_t rc = dds_delete (dom);
  CU_ASSERT_FATAL (rc == 0);
}

CU_Test(ddsc_qos_set, remote_endpoints_with_rxo)
{
  /* Domains for pub and sub use a different domain id, but the portgain setting
   * in configuration is 0, so that both domains will map to the same port number.
   * This allows to create two domains in a single test process. */
  const char *config = "${CYCLONEDDS_URI},<Discovery><Tag>${CYCLONEDDS_PID}</Tag><ExternalDomainId>0</ExternalDomainId></Discovery>";
  char *confrd = ddsrt_expand_envvars (config, 0);
  char *confwr = ddsrt_expand_envvars (config, 1);
  const dds_entity_t domrd = dds_create_domain (0, confrd);
  CU_ASSERT_FATAL (domrd > 0);
  const dds_entity_t domwr = dds_create_domain (1, confwr);
  CU_ASSERT_FATAL (domwr > 0);
  ddsrt_free (confwr);
  ddsrt_free (confrd);

  const dds_entity_t dprd = dds_create_participant (0, NULL, NULL);
  CU_ASSERT_FATAL (dprd > 0);
  const dds_entity_t dpwr = dds_create_participant (1, NULL, NULL);
  CU_ASSERT_FATAL (dpwr > 0);

  do_ddsc_qos_set_endpoints_with_rxo (dprd, dpwr);

  dds_return_t rc = dds_delete (domrd);
  CU_ASSERT_FATAL (rc == 0);
  rc = dds_delete (domwr);
  CU_ASSERT_FATAL (rc == 0);
}
