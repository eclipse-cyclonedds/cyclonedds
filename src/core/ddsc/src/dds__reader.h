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
void dds_reader_status_cb (void *entity, const struct ddsi_status_cb_data * data);

/** @component reader */
dds_return_t dds_return_reader_loan (dds_reader *rd, void **buf, int32_t bufsz);


#define DDS_READ_WITHOUT_LOCK (0xFFFFFFED)

/**
 * @component reader
 *
 * Returns number of samples in read cache and locks the reader cache to make
 * sure that the samples content doesn't change. Because the cache is locked,
 * you should be able to read/take without having to lock first. This is done
 * by passing the DDS_READ_WITHOUT_LOCK value to the read/take call as maxs.
 * Then the read/take will not lock but still unlock.
 *
 * Used to support LENGTH_UNLIMITED in C++.
 *
 * @param entity reader entity
 * @return the number of samples
 */
DDS_EXPORT uint32_t dds_reader_lock_samples (dds_entity_t entity);

DEFINE_ENTITY_LOCK_UNLOCK(dds_reader, DDS_KIND_READER, reader)

#if defined (__cplusplus)
}
#endif

#endif // DDS__READER_H
