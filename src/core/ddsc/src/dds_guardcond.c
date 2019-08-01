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
#include "dds__reader.h"
#include "dds__guardcond.h"
#include "dds__participant.h"
#include "dds/ddsi/ddsi_iid.h"
#include "dds/ddsi/q_ephash.h"
#include "dds/ddsi/q_entity.h"
#include "dds/ddsi/q_thread.h"

DECL_ENTITY_LOCK_UNLOCK (extern inline, dds_guardcond)

const struct dds_entity_deriver dds_entity_deriver_guardcondition = {
  .close = dds_entity_deriver_dummy_close,
  .delete = dds_entity_deriver_dummy_delete,
  .set_qos = dds_entity_deriver_dummy_set_qos,
  .validate_status = dds_entity_deriver_dummy_validate_status
};

dds_entity_t dds_create_guardcondition (dds_entity_t participant)
{
  dds_participant *pp;
  dds_return_t rc;

  if ((rc = dds_participant_lock (participant, &pp)) != DDS_RETCODE_OK)
    return rc;
  else
  {
    dds_guardcond *gcond = dds_alloc (sizeof (*gcond));
    dds_entity_t hdl = dds_entity_init (&gcond->m_entity, &pp->m_entity, DDS_KIND_COND_GUARD, NULL, NULL, 0);
    gcond->m_entity.m_iid = ddsi_iid_gen ();
    dds_entity_register_child (&pp->m_entity, &gcond->m_entity);
    dds_participant_unlock (pp);
    return hdl;
  }
}

dds_return_t dds_set_guardcondition (dds_entity_t condition, bool triggered)
{
  dds_guardcond *gcond;
  dds_return_t rc;

  if ((rc = dds_guardcond_lock (condition, &gcond)) != DDS_RETCODE_OK)
    return rc;
  else
  {
    if (triggered)
      dds_entity_trigger_set (&gcond->m_entity, 1);
    else
      ddsrt_atomic_st32 (&gcond->m_entity.m_status.m_trigger, 0);
    dds_guardcond_unlock (gcond);
    return DDS_RETCODE_OK;
  }
}

dds_return_t dds_read_guardcondition (dds_entity_t condition, bool *triggered)
{
  dds_guardcond *gcond;
  dds_return_t rc;

  if (triggered == NULL)
    return DDS_RETCODE_BAD_PARAMETER;

  *triggered = false;
  if ((rc = dds_guardcond_lock (condition, &gcond)) != DDS_RETCODE_OK)
    return rc;
  else
  {
    *triggered = (ddsrt_atomic_ld32 (&gcond->m_entity.m_status.m_trigger) != 0);
    dds_guardcond_unlock (gcond);
    return DDS_RETCODE_OK;
  }
}

dds_return_t dds_take_guardcondition (dds_entity_t condition, bool *triggered)
{
  dds_guardcond *gcond;
  dds_return_t rc;

  if (triggered == NULL)
    return DDS_RETCODE_BAD_PARAMETER;

  *triggered = false;
  if ((rc = dds_guardcond_lock (condition, &gcond)) != DDS_RETCODE_OK)
    return rc;
  else
  {
    *triggered = (ddsrt_atomic_and32_ov (&gcond->m_entity.m_status.m_trigger, 0) != 0);
    dds_guardcond_unlock (gcond);
    return DDS_RETCODE_OK;
  }
}
