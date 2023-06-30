// Copyright(c) 2006 to 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDS__READER_H
#define DDS__READER_H

#include "dds__types.h"
#include "dds__entity.h"

#if defined (__cplusplus)
extern "C" {
#endif
struct ddsi_status_cb_data;

/** @component reader */
void dds_reader_status_cb (void *ventity, const struct ddsi_status_cb_data * data);

/** @brief Invokes listeners for events signalled in the entity status
  * @component reader
  * @note expects `e->m_observers_lock` to be held
  */
void dds_reader_invoke_cbs_for_pending_events(struct dds_entity *e, uint32_t status);

/** @component reader */
dds_return_t dds_return_reader_loan (dds_reader *rd, void **buf, int32_t bufsz);

DEFINE_ENTITY_LOCK_UNLOCK(dds_reader, DDS_KIND_READER, reader)

#if defined (__cplusplus)
}
#endif

#endif // DDS__READER_H
