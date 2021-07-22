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
#ifndef _DDS_READER_H_
#define _DDS_READER_H_

#include "dds__types.h"
#include "dds__entity.h"

#if defined (__cplusplus)
extern "C" {
#endif

struct status_cb_data;

void dds_reader_status_cb (void *entity, const struct status_cb_data * data);

dds_return_t dds_return_reader_loan (dds_reader *rd, void **buf, int32_t bufsz);

/*
  dds_reader_lock_samples: Returns number of samples in read cache and locks the
  reader cache to make sure that the samples content doesn't change.
  Because the cache is locked, you should be able to read/take without having to
  lock first. This is done by passing the DDS_READ_WITHOUT_LOCK value to the
  read/take call as maxs. Then the read/take will not lock but still unlock.

  See also CHAM-287, CHAM-306 and LITE-1183.

  Used to support LENGTH_UNLIMITED in C++.
*/
#define DDS_READ_WITHOUT_LOCK (0xFFFFFFED)
DDS_EXPORT uint32_t dds_reader_lock_samples (dds_entity_t entity);

struct nn_rsample_info;
struct nn_rdata;
DDS_EXPORT void dds_reader_ddsi2direct (dds_entity_t entity, void (*cb) (const struct nn_rsample_info *sampleinfo, const struct nn_rdata *fragchain, void *arg), void *cbarg);

DEFINE_ENTITY_LOCK_UNLOCK(dds_reader, DDS_KIND_READER)

#if defined (__cplusplus)
}
#endif
#endif
