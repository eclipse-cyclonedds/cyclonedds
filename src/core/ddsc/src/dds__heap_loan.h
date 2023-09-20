// Copyright(c) 2022 to 2023 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDS__HEAP_LOAN_H
#define DDS__HEAP_LOAN_H

#include "dds__types.h"

#if defined(__cplusplus)
extern "C" {
#endif

struct dds_loaned_sample;

dds_return_t dds_heap_loan (const struct ddsi_sertype *type, dds_loaned_sample_state_t sample_state, struct dds_loaned_sample **loaned_sample)
  ddsrt_nonnull_all;

void dds_heap_loan_reset (struct dds_loaned_sample *loaned_sample)
  ddsrt_nonnull_all;

#if defined(__cplusplus)
}
#endif

#endif /* DDS__HEAP_LOAN_H */
