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
#include "dds__err.h"
#include "ddsi/q_ephash.h"
#include "ddsi/q_entity.h"
#include "ddsi/q_thread.h"

DECL_ENTITY_LOCK_UNLOCK(extern inline, dds_guardcond)

dds_entity_t dds_create_guardcondition (dds_entity_t participant)
{
  dds_participant *pp;
  dds__retcode_t rc;

  if ((rc = dds_participant_lock (participant, &pp)) != DDS_RETCODE_OK)
    return DDS_ERRNO (rc);
  else
  {
    dds_guardcond * gcond = dds_alloc (sizeof (*gcond));
    dds_entity_t hdl = dds_entity_init (&gcond->m_entity, &pp->m_entity, DDS_KIND_COND_GUARD, NULL, NULL, 0);
    dds_participant_unlock (pp);
    return hdl;
  }
}

dds_return_t dds_set_guardcondition (dds_entity_t condition, bool triggered)
{
  dds_guardcond *gcond;
  dds__retcode_t rc;

  if ((rc = dds_guardcond_lock (condition, &gcond)) != DDS_RETCODE_OK)
    return DDS_ERRNO (dds_valid_hdl (condition, DDS_KIND_COND_GUARD));
  else
  {
    os_mutexLock (&gcond->m_entity.m_observers_lock);
    if (triggered)
      dds_entity_status_set (&gcond->m_entity, DDS_WAITSET_TRIGGER_STATUS);
    else
      dds_entity_status_reset (&gcond->m_entity, DDS_WAITSET_TRIGGER_STATUS);
    os_mutexUnlock (&gcond->m_entity.m_observers_lock);
    dds_guardcond_unlock (gcond);
    return DDS_RETCODE_OK;
  }
}

dds_return_t dds_read_guardcondition (dds_entity_t condition, bool *triggered)
{
  dds_guardcond *gcond;
  dds__retcode_t rc;

  if (triggered == NULL)
    return DDS_ERRNO (DDS_RETCODE_BAD_PARAMETER);

  *triggered = false;
  if ((rc = dds_guardcond_lock (condition, &gcond)) != DDS_RETCODE_OK)
    return DDS_ERRNO (dds_valid_hdl (condition, DDS_KIND_COND_GUARD));
  else
  {
    os_mutexLock (&gcond->m_entity.m_observers_lock);
    *triggered = dds_entity_status_match (&gcond->m_entity, DDS_WAITSET_TRIGGER_STATUS);
    os_mutexUnlock (&gcond->m_entity.m_observers_lock);
    dds_guardcond_unlock (gcond);
    return DDS_RETCODE_OK;
  }
}

dds_return_t dds_take_guardcondition (dds_entity_t condition, bool *triggered)
{
  dds_guardcond *gcond;
  dds__retcode_t rc;

  if (triggered == NULL)
    return DDS_ERRNO (DDS_RETCODE_BAD_PARAMETER);

  *triggered = false;
  if ((rc = dds_guardcond_lock (condition, &gcond)) != DDS_RETCODE_OK)
    return DDS_ERRNO (dds_valid_hdl (condition, DDS_KIND_COND_GUARD));
  else
  {
    os_mutexLock (&gcond->m_entity.m_observers_lock);
    *triggered = dds_entity_status_match (&gcond->m_entity, DDS_WAITSET_TRIGGER_STATUS);
    dds_entity_status_reset (&gcond->m_entity, DDS_WAITSET_TRIGGER_STATUS);
    os_mutexUnlock (&gcond->m_entity.m_observers_lock);
    dds_guardcond_unlock (gcond);
    return DDS_RETCODE_OK;
  }
}
