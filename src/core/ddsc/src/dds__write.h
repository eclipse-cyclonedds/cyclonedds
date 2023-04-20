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
dds_return_t dds_write_impl (dds_writer *wr, const void *data, dds_time_t tstamp, dds_write_action action);

/** @component write_data */
dds_return_t dds_writecdr_impl (dds_writer *wr, struct ddsi_xpack *xp, struct ddsi_serdata *d, bool flush);

/** @component write_data */
dds_return_t dds_writecdr_local_orphan_impl (struct ddsi_local_orphan_writer *lowr, struct ddsi_serdata *d);

#if defined (__cplusplus)
}
#endif
#endif /* DDS__WRITE_H */
