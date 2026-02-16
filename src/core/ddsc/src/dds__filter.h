// Copyright(c) 2025 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDS__FILTER_H
#define DDS__FILTER_H

#include "dds__types.h"

#if defined (__cplusplus)
extern "C" {
#endif

dds_return_t dds_filter_create (dds_domainid_t domain_id, const struct dds_content_filter *filter, const struct ddsi_sertype *st, struct dds_filter **out);
void dds_filter_free (struct dds_filter *filter);
dds_return_t dds_filter_update (const struct dds_content_filter *filter, const struct ddsi_sertype *st, struct dds_filter *out);
bool dds_filter_reader_accept (const struct dds_filter *filter, const struct dds_reader *rd, const struct ddsi_serdata *sd, const struct dds_sample_info *si);
bool dds_filter_writer_accept (const struct dds_filter *filter, const struct dds_writer *wr, const void *sample);

#if defined (__cplusplus)
}
#endif

#endif // DDS__FILTER_H
