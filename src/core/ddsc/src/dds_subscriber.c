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
#include "dds__qos.h"
#include "dds__subscriber.h"
#include "dds/ddsi/q_entity.h"
#include "dds/ddsi/q_globals.h"
#include "dds/version.h"

DECL_ENTITY_LOCK_UNLOCK (extern inline, dds_subscriber)

#define DDS_SUBSCRIBER_STATUS_MASK                               \
                        (DDS_DATA_ON_READERS_STATUS)

static dds_return_t dds_subscriber_instance_hdl (dds_entity *e, dds_instance_handle_t *i) ddsrt_nonnull_all;

static dds_return_t dds_subscriber_instance_hdl (dds_entity *e, dds_instance_handle_t *i)
{
  (void) e;
  (void) i;
  /* FIXME: Get/generate proper handle. */
  return DDS_RETCODE_UNSUPPORTED;
}

static dds_return_t dds__subscriber_qos_validate (const dds_qos_t *qos, bool enabled) ddsrt_nonnull_all;

static dds_return_t dds__subscriber_qos_validate (const dds_qos_t *qos, bool enabled)
{
  dds_return_t ret;
  if ((ret = nn_xqos_valid (qos)) < 0)
    return ret;
  return (enabled && (qos->present & QP_PRESENTATION)) ? DDS_RETCODE_IMMUTABLE_POLICY : DDS_RETCODE_OK;
}

static dds_return_t dds_subscriber_qos_set (dds_entity *e, const dds_qos_t *qos, bool enabled) ddsrt_nonnull_all;

static dds_return_t dds_subscriber_qos_set (dds_entity *e, const dds_qos_t *qos, bool enabled)
{
  /* FIXME: QoS changes. */
  dds_return_t ret;
  (void) e;
  if ((ret = dds__subscriber_qos_validate (qos, enabled)) != DDS_RETCODE_OK)
    return ret;
  return (enabled ? DDS_RETCODE_UNSUPPORTED : DDS_RETCODE_OK);
}

static dds_return_t dds_subscriber_status_validate (uint32_t mask)
{
  return (mask & ~DDS_SUBSCRIBER_STATUS_MASK) ? DDS_RETCODE_BAD_PARAMETER : DDS_RETCODE_OK;
}

dds_entity_t dds__create_subscriber_l (dds_participant *participant, const dds_qos_t *qos, const dds_listener_t *listener)
{
  /* participant entity lock must be held */
  dds_subscriber *sub;
  dds_entity_t subscriber;
  dds_return_t ret;
  dds_qos_t *new_qos;

#define DDS_QOSMASK_SUBSCRIBER (QP_PARTITION | QP_PRESENTATION | QP_GROUP_DATA | QP_PRISMTECH_ENTITY_FACTORY | QP_CYCLONE_IGNORELOCAL)
  new_qos = dds_create_qos ();
  if (qos)
    nn_xqos_mergein_missing (new_qos, qos, DDS_QOSMASK_SUBSCRIBER);
  nn_xqos_mergein_missing (new_qos, &gv.default_xqos_sub, ~(uint64_t)0);
  if ((ret = dds__subscriber_qos_validate (new_qos, false)) != DDS_RETCODE_OK)
  {
    dds_delete_qos (new_qos);
    return ret;
  }

  sub = dds_alloc (sizeof (*sub));
  subscriber = dds_entity_init (&sub->m_entity, &participant->m_entity, DDS_KIND_SUBSCRIBER, new_qos, listener, DDS_SUBSCRIBER_STATUS_MASK);
  sub->m_entity.m_deriver.set_qos = dds_subscriber_qos_set;
  sub->m_entity.m_deriver.validate_status = dds_subscriber_status_validate;
  sub->m_entity.m_deriver.get_instance_hdl = dds_subscriber_instance_hdl;
  return subscriber;
}

dds_entity_t dds_create_subscriber (dds_entity_t participant, const dds_qos_t *qos, const dds_listener_t *listener)
{
  dds_participant *par;
  dds_entity_t hdl;
  dds_return_t ret;
  if ((ret = dds_participant_lock (participant, &par)) != DDS_RETCODE_OK)
    return ret;
  hdl = dds__create_subscriber_l (par, qos, listener);
  dds_participant_unlock (par);
  return hdl;
}

dds_return_t dds_notify_readers (dds_entity_t subscriber)
{
  dds_subscriber *sub;
  dds_return_t ret;

  if ((ret = dds_subscriber_lock (subscriber, &sub)) != DDS_RETCODE_OK)
    return ret;

  ret = DDS_RETCODE_UNSUPPORTED;
  for (dds_entity *iter = sub->m_entity.m_children; iter; iter = iter->m_next)
  {
    ddsrt_mutex_lock (&iter->m_mutex);
    // FIXME: check if reader has data available, call listener
    ddsrt_mutex_unlock(&iter->m_mutex);
  }
  dds_subscriber_unlock (sub);
  return ret;
}

dds_return_t dds_subscriber_begin_coherent (dds_entity_t e)
{
  return dds_generic_unimplemented_operation (e, DDS_KIND_SUBSCRIBER);
}

dds_return_t dds_subscriber_end_coherent (dds_entity_t e)
{
  return dds_generic_unimplemented_operation (e, DDS_KIND_SUBSCRIBER);
}

