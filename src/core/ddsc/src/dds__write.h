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
#ifndef _DDS_WRITE_H_
#define _DDS_WRITE_H_

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

dds_return_t dds_write_impl (dds_writer *wr, const void *data, dds_time_t tstamp, dds_write_action action);
dds_return_t dds_writecdr_impl (dds_writer *wr, struct ddsi_serdata *d, dds_time_t tstamp, dds_write_action action);
dds_return_t dds_writecdr_impl_lowlevel (struct writer *ddsi_wr, struct nn_xpack *xp, struct ddsi_serdata *d, bool flush);

#if defined (__cplusplus)
}
#endif
#endif
