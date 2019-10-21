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
#include <assert.h>
#include <string.h>
#include "cyclonedds/ddsrt/misc.h"
#include "dds__listener.h"
#include "dds__participant.h"
#include "dds__publisher.h"
#include "dds__qos.h"
#include "cyclonedds/ddsi/ddsi_iid.h"
#include "cyclonedds/ddsi/q_entity.h"
#include "cyclonedds/ddsi/q_globals.h"
#include "cyclonedds/version.h"

DECL_ENTITY_LOCK_UNLOCK (extern inline, dds_publisher)

#define DDS_PUBLISHER_STATUS_MASK   (0u)

static dds_return_t dds_publisher_qos_set (dds_entity *e, const dds_qos_t *qos, bool enabled)
{
  /* note: e->m_qos is still the old one to allow for failure here */
  (void) e; (void) qos; (void) enabled;
  return DDS_RETCODE_OK;
}

static dds_return_t dds_publisher_status_validate (uint32_t mask)
{
  return (mask & ~DDS_PUBLISHER_STATUS_MASK) ? DDS_RETCODE_BAD_PARAMETER : DDS_RETCODE_OK;
}

const struct dds_entity_deriver dds_entity_deriver_publisher = {
  .interrupt = dds_entity_deriver_dummy_interrupt,
  .close = dds_entity_deriver_dummy_close,
  .delete = dds_entity_deriver_dummy_delete,
  .set_qos = dds_publisher_qos_set,
  .validate_status = dds_publisher_status_validate
};

dds_entity_t dds_create_publisher (dds_entity_t participant, const dds_qos_t *qos, const dds_listener_t *listener)
{
  dds_participant *par;
  dds_publisher *pub;
  dds_entity_t hdl;
  dds_qos_t *new_qos;
  dds_return_t ret;

  if ((ret = dds_participant_lock (participant, &par)) != DDS_RETCODE_OK)
    return ret;

  new_qos = dds_create_qos ();
  if (qos)
    nn_xqos_mergein_missing (new_qos, qos, DDS_PUBLISHER_QOS_MASK);
  nn_xqos_mergein_missing (new_qos, &par->m_entity.m_domain->gv.default_xqos_pub, ~(uint64_t)0);
  if ((ret = nn_xqos_valid (&par->m_entity.m_domain->gv.logconfig, new_qos)) != DDS_RETCODE_OK)
  {
    dds_participant_unlock (par);
    return ret;
  }

  pub = dds_alloc (sizeof (*pub));
  hdl = dds_entity_init (&pub->m_entity, &par->m_entity, DDS_KIND_PUBLISHER, new_qos, listener, DDS_PUBLISHER_STATUS_MASK);
  pub->m_entity.m_iid = ddsi_iid_gen ();
  dds_entity_register_child (&par->m_entity, &pub->m_entity);
  dds_entity_init_complete (&pub->m_entity);
  dds_participant_unlock (par);
  return hdl;
}

dds_return_t dds_suspend (dds_entity_t publisher)
{
  return dds_generic_unimplemented_operation (publisher, DDS_KIND_PUBLISHER);
}

dds_return_t dds_resume (dds_entity_t publisher)
{
  return dds_generic_unimplemented_operation (publisher, DDS_KIND_PUBLISHER);
}

dds_return_t dds_wait_for_acks (dds_entity_t publisher_or_writer, dds_duration_t timeout)
{
  if (timeout < 0)
    return DDS_RETCODE_BAD_PARAMETER;
  static const dds_entity_kind_t kinds[] = { DDS_KIND_WRITER, DDS_KIND_PUBLISHER };
  return dds_generic_unimplemented_operation_manykinds (publisher_or_writer, sizeof (kinds) / sizeof (kinds[0]), kinds);
}

dds_return_t dds_publisher_begin_coherent (dds_entity_t publisher)
{
  return dds_generic_unimplemented_operation (publisher, DDS_KIND_PUBLISHER);
}

dds_return_t dds_publisher_end_coherent (dds_entity_t publisher)
{
  return dds_generic_unimplemented_operation (publisher, DDS_KIND_PUBLISHER);
}
