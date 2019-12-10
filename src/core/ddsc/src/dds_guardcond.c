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
#include "dds__init.h"
#include "dds__reader.h"
#include "dds__guardcond.h"
#include "dds__participant.h"
#include "dds/ddsi/ddsi_iid.h"
#include "dds/ddsi/ddsi_entity_index.h"
#include "dds/ddsi/q_entity.h"
#include "dds/ddsi/q_thread.h"

DECL_ENTITY_LOCK_UNLOCK (extern inline, dds_guardcond)

const struct dds_entity_deriver dds_entity_deriver_guardcondition = {
  .interrupt = dds_entity_deriver_dummy_interrupt,
  .close = dds_entity_deriver_dummy_close,
  .delete = dds_entity_deriver_dummy_delete,
  .set_qos = dds_entity_deriver_dummy_set_qos,
  .validate_status = dds_entity_deriver_dummy_validate_status
};

dds_entity_t dds_create_guardcondition (dds_entity_t owner)
{
  dds_entity *e;
  dds_return_t rc;

  /* If the owner is any ordinary (allowed) entity, the library is already initialised and calling
     init here is cheap.  If it is DDS_CYCLONEDDS_HANDLE, we may have to initialise the library, so
     have to call it.  If it is some bogus value and the library is not initialised yet ... so be
     it.  Naturally, this requires us to call delete on DDS_CYCLONEDDS_HANDLE afterward. */
  if ((rc = dds_init ()) < 0)
    return rc;

  if ((rc = dds_entity_lock (owner, DDS_KIND_DONTCARE, &e)) != DDS_RETCODE_OK)
    goto err_entity_lock;

  switch (dds_entity_kind (e))
  {
    case DDS_KIND_CYCLONEDDS:
    case DDS_KIND_DOMAIN:
    case DDS_KIND_PARTICIPANT:
      break;
    default:
      rc = DDS_RETCODE_ILLEGAL_OPERATION;
      goto err_entity_kind;
  }

  dds_guardcond *gcond = dds_alloc (sizeof (*gcond));
  dds_entity_t hdl = dds_entity_init (&gcond->m_entity, e, DDS_KIND_COND_GUARD, false, NULL, NULL, 0);
  gcond->m_entity.m_iid = ddsi_iid_gen ();
  dds_entity_register_child (e, &gcond->m_entity);
  dds_entity_init_complete (&gcond->m_entity);
  dds_entity_unlock (e);
  dds_entity_unpin_and_drop_ref (&dds_global.m_entity);
  return hdl;

 err_entity_kind:
  dds_entity_unlock (e);
 err_entity_lock:
  dds_entity_unpin_and_drop_ref (&dds_global.m_entity);
  return rc;
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
