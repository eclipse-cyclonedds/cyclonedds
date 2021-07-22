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
#ifndef _DDS_PUBLISHER_H_
#define _DDS_PUBLISHER_H_

#include "dds/dds.h"
#include "dds__entity.h"

#if defined (__cplusplus)
extern "C" {
#endif

DEFINE_ENTITY_LOCK_UNLOCK(dds_publisher, DDS_KIND_PUBLISHER)

dds_entity_t
dds__create_publisher_l(
  struct dds_participant *participant, /* entity-lock must be held */
  bool implicit,
  const dds_qos_t *qos,
  const dds_listener_t *listener);

dds_return_t dds_publisher_begin_coherent (dds_entity_t e);
dds_return_t dds_publisher_end_coherent (dds_entity_t e);

#if defined (__cplusplus)
}
#endif
#endif /* _DDS_PUBLISHER_H_ */
