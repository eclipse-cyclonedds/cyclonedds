// Copyright(c) 2006 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <assert.h>
#include <string.h>
#include "dds/ddsrt/misc.h"
#include "dds__listener.h"
#include "dds__participant.h"
#include "dds__publisher.h"
#include "dds__writer.h"
#include "dds__qos.h"
#include "dds/ddsi/ddsi_iid.h"
#include "dds/ddsi/ddsi_entity.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/version.h"

DECL_ENTITY_LOCK_UNLOCK (dds_publisher)

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
  .validate_status = dds_publisher_status_validate,
  .create_statistics = dds_entity_deriver_dummy_create_statistics,
  .refresh_statistics = dds_entity_deriver_dummy_refresh_statistics
};

dds_entity_t dds__create_publisher_l (dds_participant *par, bool implicit, const dds_qos_t *qos, const dds_listener_t *listener)
{
  dds_publisher *pub;
  dds_entity_t hdl;
  dds_qos_t *new_qos;
  dds_return_t ret;

  new_qos = dds_create_qos ();
  if (qos)
    ddsi_xqos_mergein_missing (new_qos, qos, DDS_PUBLISHER_QOS_MASK);
  ddsi_xqos_mergein_missing (new_qos, &ddsi_default_qos_publisher_subscriber, ~(uint64_t)0);
  dds_apply_entity_naming(new_qos, par->m_entity.m_qos, &par->m_entity.m_domain->gv);

  if ((ret = ddsi_xqos_valid (&par->m_entity.m_domain->gv.logconfig, new_qos)) != DDS_RETCODE_OK)
  {
    dds_delete_qos (new_qos);
    return ret;
  }

  pub = dds_alloc (sizeof (*pub));
  hdl = dds_entity_init (&pub->m_entity, &par->m_entity, DDS_KIND_PUBLISHER, implicit, true, new_qos, listener, DDS_PUBLISHER_STATUS_MASK);
  pub->m_entity.m_iid = ddsi_iid_gen ();
  dds_entity_register_child (&par->m_entity, &pub->m_entity);
  dds_entity_init_complete (&pub->m_entity);
  return hdl;
}

dds_entity_t dds_create_publisher (dds_entity_t participant, const dds_qos_t *qos, const dds_listener_t *listener)
{
  dds_participant *par;
  dds_entity_t hdl;
  dds_return_t ret;
  if ((ret = dds_participant_lock (participant, &par)) != DDS_RETCODE_OK)
    return ret;
  hdl = dds__create_publisher_l (par, false, qos, listener);
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
  dds_return_t ret;
  dds_entity *p_or_w_ent;

  if (timeout < 0)
    return DDS_RETCODE_BAD_PARAMETER;

  if ((ret = dds_entity_pin (publisher_or_writer, &p_or_w_ent)) < 0)
    return ret;

  const dds_time_t tnow = dds_time ();
  const dds_time_t abstimeout = (DDS_INFINITY - timeout <= tnow) ? DDS_NEVER : (tnow + timeout);
  switch (dds_entity_kind (p_or_w_ent))
  {
    case DDS_KIND_PUBLISHER:
      /* FIXME: wait_for_acks on all writers of the same publisher */
      dds_entity_unpin (p_or_w_ent);
      return DDS_RETCODE_UNSUPPORTED;

    case DDS_KIND_WRITER:
      ret = dds__ddsi_writer_wait_for_acks ((struct dds_writer *) p_or_w_ent, NULL, abstimeout);
      dds_entity_unpin (p_or_w_ent);
      return ret;

    default:
      dds_entity_unpin (p_or_w_ent);
      return DDS_RETCODE_ILLEGAL_OPERATION;
  }
}

dds_return_t dds_publisher_begin_coherent (dds_entity_t publisher)
{
  return dds_generic_unimplemented_operation (publisher, DDS_KIND_PUBLISHER);
}

dds_return_t dds_publisher_end_coherent (dds_entity_t publisher)
{
  return dds_generic_unimplemented_operation (publisher, DDS_KIND_PUBLISHER);
}
