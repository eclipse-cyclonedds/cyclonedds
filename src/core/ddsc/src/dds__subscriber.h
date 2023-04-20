// Copyright(c) 2006 to 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDS__SUBSCRIBER_H
#define DDS__SUBSCRIBER_H

#include "dds/dds.h"
#include "dds__entity.h"

#if defined (__cplusplus)
extern "C" {
#endif

DEFINE_ENTITY_LOCK_UNLOCK(dds_subscriber, DDS_KIND_SUBSCRIBER, subscriber)

/**
 * @brief Creates a subscriber with participant entity-lock held
 * @component subscriber
 *
 * @param participant the parent participant
 * @param implicit indicates if implicitly created
 * @param qos qos object that is stored in the subscriber
 * @param listener listener object that is stored in the subscriber
 * @return dds_entity_t
 */
dds_entity_t dds__create_subscriber_l(struct dds_participant *participant, bool implicit, const dds_qos_t *qos, const dds_listener_t *listener);

/** @component subscriber */
dds_return_t dds_subscriber_begin_coherent (dds_entity_t e);

/** @component subscriber */
dds_return_t dds_subscriber_end_coherent (dds_entity_t e);

/** @component subscriber */
bool dds_subscriber_compute_data_on_readers_locked (dds_subscriber *sub);

/** @component subscriber */
void dds_subscriber_adjust_materialize_data_on_readers (dds_subscriber *sub, bool materialization_needed) ddsrt_nonnull_all;

#if defined (__cplusplus)
}
#endif

#endif /* DDS__SUBSCRIBER_H */
