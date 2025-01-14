// Copyright(c) 2006 to 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDS__WRITER_H
#define DDS__WRITER_H

#include "dds/ddsi/ddsi_serdata.h"
#include "dds__entity.h"

#if defined (__cplusplus)
extern "C" {
#endif

DEFINE_ENTITY_LOCK_UNLOCK(dds_writer, DDS_KIND_WRITER, writer)

enum dds_writer_loan_type {
  DDS_WRITER_LOAN_REGULAR,
  DDS_WRITER_LOAN_RAW
};

struct ddsi_status_cb_data;

/** @component writer */
void dds_writer_status_cb (void *entity, const struct ddsi_status_cb_data * data);

/** @component writer */
void dds_writer_invoke_cbs_for_pending_events(struct dds_entity *e, uint32_t status);

/** @component writer */
dds_return_t dds__ddsi_writer_wait_for_acks (struct dds_writer *wr, ddsi_guid_t *rdguid, dds_time_t abstimeout);

/** @component writer */
dds_return_t dds_request_writer_loan (dds_writer *wr, enum dds_writer_loan_type loan_type, uint32_t sz, void **sample)
  ddsrt_nonnull_all;

/** @component writer */
dds_loaned_sample_t *dds_writer_request_psmx_loan(const dds_writer *wr, uint32_t size)
  ddsrt_nonnull_all;

/** @component writer */
dds_return_t dds_return_writer_loan (dds_writer *wr, void **samples_ptr, int32_t n_samples) ddsrt_nonnull_all;

/** @component writer */
DDS_EXPORT_INTERNAL_FUNCTION
struct dds_loaned_sample * dds_writer_psmx_loan_raw (const struct dds_writer *wr, const void *data, enum ddsi_serdata_kind sdkind, dds_time_t timestamp, uint32_t statusinfo)
  ddsrt_attribute_warn_unused_result ddsrt_nonnull_all;

/** @component writer */
DDS_EXPORT_INTERNAL_FUNCTION
struct dds_loaned_sample * dds_writer_psmx_loan_from_serdata (const struct dds_writer *wr, const struct ddsi_serdata *sd)
  ddsrt_attribute_warn_unused_result ddsrt_nonnull_all;

#if defined (__cplusplus)
}
#endif

#endif /* DDS__WRITER_H */
