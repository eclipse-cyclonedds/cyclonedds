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
#include <string.h>
#include "dds__listener.h"
#include "dds__participant.h"
#include "dds__subscriber.h"
#include "dds__qos.h"
#include "dds/ddsi/ddsi_iid.h"
#include "dds/ddsi/q_entity.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsrt/heap.h"
#include "dds/version.h"

DECL_ENTITY_LOCK_UNLOCK (extern inline, dds_subscriber)

#define DDS_SUBSCRIBER_STATUS_MASK                               \
                        (DDS_DATA_ON_READERS_STATUS)

static dds_return_t dds_subscriber_qos_set (dds_entity *e, const dds_qos_t *qos, bool enabled)
{
  /* note: e->m_qos is still the old one to allow for failure here */
  (void) e; (void) qos; (void) enabled;
  return DDS_RETCODE_OK;
}

static dds_return_t dds_subscriber_status_validate (uint32_t mask)
{
  return (mask & ~DDS_SUBSCRIBER_STATUS_MASK) ? DDS_RETCODE_BAD_PARAMETER : DDS_RETCODE_OK;
}

const struct dds_entity_deriver dds_entity_deriver_subscriber = {
  .interrupt = dds_entity_deriver_dummy_interrupt,
  .close = dds_entity_deriver_dummy_close,
  .delete = dds_entity_deriver_dummy_delete,
  .set_qos = dds_subscriber_qos_set,
  .validate_status = dds_subscriber_status_validate,
  .create_statistics = dds_entity_deriver_dummy_create_statistics,
  .refresh_statistics = dds_entity_deriver_dummy_refresh_statistics
};

dds_entity_t dds__create_subscriber_l (dds_participant *participant, bool implicit, const dds_qos_t *qos, const dds_listener_t *listener)
{
  /* participant entity lock must be held */
  dds_subscriber *sub;
  dds_entity_t subscriber;
  dds_return_t ret;
  dds_qos_t *new_qos;

  new_qos = dds_create_qos ();
  if (qos)
    ddsi_xqos_mergein_missing (new_qos, qos, DDS_SUBSCRIBER_QOS_MASK);
  ddsi_xqos_mergein_missing (new_qos, &participant->m_entity.m_domain->gv.default_xqos_sub, ~(uint64_t)0);
  if ((ret = ddsi_xqos_valid (&participant->m_entity.m_domain->gv.logconfig, new_qos)) != DDS_RETCODE_OK)
  {
    dds_delete_qos (new_qos);
    return ret;
  }

  sub = dds_alloc (sizeof (*sub));
  subscriber = dds_entity_init (&sub->m_entity, &participant->m_entity, DDS_KIND_SUBSCRIBER, implicit, true, new_qos, listener, DDS_SUBSCRIBER_STATUS_MASK);
  sub->m_entity.m_iid = ddsi_iid_gen ();
  dds_entity_register_child (&participant->m_entity, &sub->m_entity);
  dds_entity_init_complete (&sub->m_entity);
  return subscriber;
}

dds_entity_t dds_create_subscriber (dds_entity_t participant, const dds_qos_t *qos, const dds_listener_t *listener)
{
  dds_participant *par;
  dds_entity_t hdl;
  dds_return_t ret;
  if ((ret = dds_participant_lock (participant, &par)) != DDS_RETCODE_OK)
    return ret;
  hdl = dds__create_subscriber_l (par, false, qos, listener);
  dds_participant_unlock (par);
  return hdl;
}

dds_return_t dds_notify_readers (dds_entity_t subscriber)
{
  dds_subscriber *sub;
  dds_return_t ret;
  if ((ret = dds_subscriber_lock (subscriber, &sub)) != DDS_RETCODE_OK)
    return ret;
  dds_subscriber_unlock (sub);
  return DDS_RETCODE_UNSUPPORTED;
}

dds_return_t dds_subscriber_begin_coherent (dds_entity_t e)
{
  return dds_generic_unimplemented_operation (e, DDS_KIND_SUBSCRIBER);
}

dds_return_t dds_subscriber_end_coherent (dds_entity_t e)
{
  return dds_generic_unimplemented_operation (e, DDS_KIND_SUBSCRIBER);
}

