// Copyright(c) 2006 to 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDS__PUBLISHER_H
#define DDS__PUBLISHER_H

#include "dds/dds.h"
#include "dds__entity.h"

#if defined (__cplusplus)
extern "C" {
#endif

DEFINE_ENTITY_LOCK_UNLOCK(dds_publisher, DDS_KIND_PUBLISHER, publisher)

/**
 * @brief Creates a publisher with participant entity-lock held
 * @component publisher
 *
 * @param participant the parent participant
 * @param implicit indicates if implicitly created
 * @param qos qos object that is stored in the publisher
 * @param listener listener object that is stored in the publisher
 * @return dds_entity_t
 */
dds_entity_t dds__create_publisher_l(struct dds_participant *participant, bool implicit, const dds_qos_t *qos, const dds_listener_t *listener);

/** @component publisher */
dds_return_t dds_publisher_begin_coherent (dds_entity_t e);

/** @component publisher */
dds_return_t dds_publisher_end_coherent (dds_entity_t e);

#if defined (__cplusplus)
}
#endif
#endif /* DDS__PUBLISHER_H */
