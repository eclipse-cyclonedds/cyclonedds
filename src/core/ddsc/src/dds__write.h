// Copyright(c) 2006 to 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDS__WRITE_H
#define DDS__WRITE_H

#include "dds/export.h"
#include "dds__types.h"

#if defined (__cplusplus)
extern "C" {
#endif

#define DDS_WR_KEY_BIT 0x01
#define DDS_WR_DISPOSE_BIT 0x02
#define DDS_WR_UNREGISTER_BIT 0x04

struct ddsi_serdata;

typedef enum {
  DDS_WR_ACTION_WRITE = 0,
  DDS_WR_ACTION_WRITE_DISPOSE = DDS_WR_DISPOSE_BIT,
  DDS_WR_ACTION_DISPOSE = DDS_WR_KEY_BIT | DDS_WR_DISPOSE_BIT,
  DDS_WR_ACTION_UNREGISTER = DDS_WR_KEY_BIT | DDS_WR_UNREGISTER_BIT
} dds_write_action;

/** @component write_data */
DDS_EXPORT_INTERNAL_FUNCTION
dds_return_t dds_write_impl (dds_writer *wr, const void *data, dds_time_t timestamp, dds_write_action action)
  ddsrt_attribute_warn_unused_result ddsrt_nonnull_all;

/** @component write_data */
DDS_EXPORT_INTERNAL_FUNCTION
dds_return_t dds_writecdr_impl (dds_writer *wr, struct ddsi_xpack *xp, struct ddsi_serdata *d, bool flush)
  ddsrt_attribute_warn_unused_result ddsrt_nonnull_all;

/** @component write_data */
dds_return_t dds_writecdr_local_orphan_impl (struct ddsi_local_orphan_writer *lowr, struct ddsi_serdata *d)
  ddsrt_nonnull_all;

/** @component write_data */
void dds_write_flush_impl (dds_writer *wr);

inline bool dds_source_timestamp_is_valid_ddsi_time (dds_time_t timestamp, ddsi_protocol_version_t protover) {
  // infinity as a source timestamp makes no sense so we disallow it
  // invalid is useful because of dds_forwardcdr i.c.w. inputs with invalid/missing source timestamps
  // negative timestamps have always been disallowed by Cyclone and are unrepresentable in DDSI 2.3 and later
  assert (protover.major == 2);
  if (protover.minor > 2)
    return (timestamp >= 0 && timestamp <= INT64_C (4294967295999999999)) || (timestamp == DDS_TIME_INVALID);
  else
    return (timestamp >= 0 && timestamp <= INT64_C (2147483647999999999)) || (timestamp == DDS_TIME_INVALID);
}

#if defined (__cplusplus)
}
#endif
#endif /* DDS__WRITE_H */
