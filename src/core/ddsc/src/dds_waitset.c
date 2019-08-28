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

#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/log.h"
#include "dds__entity.h"
#include "dds__participant.h"
#include "dds__querycond.h"
#include "dds__readcond.h"
#include "dds__init.h"
#include "dds__rhc.h"
#include "dds/ddsi/ddsi_iid.h"

DEFINE_ENTITY_LOCK_UNLOCK (static, dds_waitset, DDS_KIND_WAITSET)

static bool is_triggered (struct dds_entity *e)
{
  bool t;
  switch (e->m_kind)
  {
    case DDS_KIND_COND_READ:
    case DDS_KIND_COND_QUERY:
    case DDS_KIND_COND_GUARD:
    case DDS_KIND_WAITSET:
      t = ddsrt_atomic_ld32 (&e->m_status.m_trigger) != 0;
      break;
    default:
      t = (ddsrt_atomic_ld32 (&e->m_status.m_status_and_mask) & SAM_STATUS_MASK) != 0;
      break;
  }
  return t;
}

static dds_return_t dds_waitset_wait_impl (dds_entity_t waitset, dds_attach_t *xs, size_t nxs, dds_time_t abstimeout)
{
  dds_waitset *ws;
  dds_return_t ret;

  if ((xs == NULL) != (nxs == 0))
    return DDS_RETCODE_BAD_PARAMETER;

  /* Locking the waitset here will delay a possible deletion until it is
   * unlocked. Even when the related mutex is unlocked by a conditioned wait. */
  {
    dds_entity *ent;
    if ((ret = dds_entity_pin (waitset, &ent)) != DDS_RETCODE_OK)
      return ret;
    if (dds_entity_kind (ent) != DDS_KIND_WAITSET)
    {
      dds_entity_unpin (ent);
      return DDS_RETCODE_ILLEGAL_OPERATION;
    }
    ws = (dds_waitset *) ent;
  }

  /* Move any previously but no longer triggering entities back to the observed list */
  ddsrt_mutex_lock (&ws->m_entity.m_mutex);
  ws->ntriggered = 0;
  for (size_t i = 0; i < ws->nentities; i++)
  {
    if (is_triggered (ws->entities[i].entity))
    {
      dds_attachment tmp = ws->entities[i];
      ws->entities[i] = ws->entities[ws->ntriggered];
      ws->entities[ws->ntriggered++] = tmp;
    }
  }

  /* Only wait/keep waiting when we have something to observe and there aren't any triggers yet. */
  while (ws->nentities > 0 && ws->ntriggered == 0)
    if (!ddsrt_cond_waituntil (&ws->m_entity.m_cond, &ws->m_entity.m_mutex, abstimeout))
      break;

  ret = (int32_t) ws->ntriggered;
  for (size_t i = 0; i < ws->ntriggered && i < nxs; i++)
    xs[i] = ws->entities[i].arg;
  ddsrt_mutex_unlock (&ws->m_entity.m_mutex);
  dds_entity_unpin (&ws->m_entity);
  return ret;
}

static dds_return_t dds_waitset_close (struct dds_entity *e)
{
  /* deep in the process of deleting the entity, so this is the only thread */
  dds_waitset *ws = (dds_waitset *) e;
  for (size_t i = 0; i < ws->nentities; i++)
    (void) dds_entity_observer_unregister (ws->entities[i].entity, &ws->m_entity);
  return DDS_RETCODE_OK;
}

static dds_return_t dds_waitset_delete (struct dds_entity *e)
{
  /* deep in the process of deleting the entity, so this is the only thread */
  dds_waitset *ws = (dds_waitset *) e;
  ddsrt_free (ws->entities);
  return DDS_RETCODE_OK;
}

const struct dds_entity_deriver dds_entity_deriver_waitset = {
  .close = dds_waitset_close,
  .delete = dds_waitset_delete,
  .set_qos = dds_entity_deriver_dummy_set_qos,
  .validate_status = dds_entity_deriver_dummy_validate_status
};

dds_entity_t dds_create_waitset (dds_entity_t owner)
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

  dds_waitset *waitset = dds_alloc (sizeof (*waitset));
  dds_entity_t hdl = dds_entity_init (&waitset->m_entity, e, DDS_KIND_WAITSET, NULL, NULL, 0);
  waitset->m_entity.m_iid = ddsi_iid_gen ();
  dds_entity_register_child (e, &waitset->m_entity);
  waitset->nentities = 0;
  waitset->ntriggered = 0;
  waitset->entities = NULL;
  dds_entity_unlock (e);
  dds_delete (DDS_CYCLONEDDS_HANDLE);
  return hdl;

 err_entity_kind:
  dds_entity_unlock (e);
 err_entity_lock:
  dds_delete (DDS_CYCLONEDDS_HANDLE);
  return rc;
}

dds_return_t dds_waitset_get_entities (dds_entity_t waitset, dds_entity_t *entities, size_t size)
{
  dds_return_t ret;
  dds_waitset *ws;

  if ((ret = dds_waitset_lock (waitset, &ws)) != DDS_RETCODE_OK)
    return ret;

  if (entities != NULL)
  {
    for (size_t i = 0; i < ws->nentities && i < size; i++)
      entities[i] = ws->entities[i].handle;
  }
  ret = (int32_t) ws->nentities;
  dds_waitset_unlock (ws);
  return ret;
}

static void dds_waitset_remove (dds_waitset *ws, dds_entity_t observed)
{
  size_t i;
  for (i = 0; i < ws->nentities; i++)
    if (ws->entities[i].handle == observed)
      break;
  if (i < ws->nentities)
  {
    if (i < ws->ntriggered)
    {
      ws->entities[i] = ws->entities[--ws->ntriggered];
      ws->entities[ws->ntriggered] = ws->entities[--ws->nentities];
    }
    else
    {
      ws->entities[i] = ws->entities[--ws->nentities];
    }
    return;
  }
}

/* This is called when the observed entity signals a status change. */
static void dds_waitset_observer (dds_entity *ent, dds_entity_t observed, uint32_t status)
{
  assert (dds_entity_kind (ent) == DDS_KIND_WAITSET);
  dds_waitset *ws = (dds_waitset *) ent;
  (void) status;

  ddsrt_mutex_lock (&ws->m_entity.m_mutex);
  /* Move observed entity to triggered list. */
  size_t i;
  for (i = 0; i < ws->nentities; i++)
    if (ws->entities[i].handle == observed)
      break;
  if (i < ws->nentities && i >= ws->ntriggered)
  {
    dds_attachment tmp = ws->entities[i];
    ws->entities[i] = ws->entities[ws->ntriggered];
    ws->entities[ws->ntriggered++] = tmp;
  }
  /* Trigger waitset to wake up. */
  ddsrt_cond_broadcast (&ws->m_entity.m_cond);
  ddsrt_mutex_unlock (&ws->m_entity.m_mutex);
}

static void dds_waitset_delete_observer (dds_entity *ent, dds_entity_t observed)
{
  assert (dds_entity_kind (ent) == DDS_KIND_WAITSET);
  dds_waitset *ws = (dds_waitset *) ent;
  ddsrt_mutex_lock (&ws->m_entity.m_mutex);
  /* Remove this observed entity, which is being deleted, from the waitset. */
  dds_waitset_remove (ws, observed);
  /* Our registration to this observed entity will be removed automatically. */
  /* Trigger waitset to wake up. */
  ddsrt_cond_broadcast (&ws->m_entity.m_cond);
  ddsrt_mutex_unlock (&ws->m_entity.m_mutex);
}

dds_return_t dds_waitset_attach (dds_entity_t waitset, dds_entity_t entity, dds_attach_t x)
{
  dds_entity *e;
  dds_waitset *ws;
  dds_return_t ret;

  if ((ret = dds_waitset_lock (waitset, &ws)) < 0)
    return ret;

  if (waitset == entity)
    e = &ws->m_entity;
  else if ((ret = dds_entity_pin (entity, &e)) < 0)
    goto err_waitset;

  /* This will fail if given entity is already attached (or deleted). */
  if ((ret = dds_entity_observer_register (e, &ws->m_entity, dds_waitset_observer, dds_waitset_delete_observer)) != DDS_RETCODE_OK)
    goto err_entity;

  ws->entities = ddsrt_realloc (ws->entities, (ws->nentities + 1) * sizeof (*ws->entities));
  ws->entities[ws->nentities].arg = x;
  ws->entities[ws->nentities].entity = e;
  ws->entities[ws->nentities].handle = e->m_hdllink.hdl;
  ws->nentities++;
  if (is_triggered (e))
  {
    const size_t i = ws->nentities - 1;
    dds_attachment tmp = ws->entities[i];
    ws->entities[i] = ws->entities[ws->ntriggered];
    ws->entities[ws->ntriggered++] = tmp;
  }
  ddsrt_cond_broadcast (&ws->m_entity.m_cond);

err_entity:
  if (e != &ws->m_entity)
    dds_entity_unpin (e);
err_waitset:
  dds_waitset_unlock (ws);
  return ret;
}

dds_return_t dds_waitset_detach (dds_entity_t waitset, dds_entity_t entity)
{
  dds_waitset *ws;
  dds_entity *e;
  dds_return_t ret;

  if ((ret = dds_waitset_lock (waitset, &ws)) != DDS_RETCODE_OK)
    return ret;

  /* Possibly fails when entity was not attached. */
  if (waitset == entity)
    ret = dds_entity_observer_unregister (&ws->m_entity, &ws->m_entity);
  else if ((ret = dds_entity_pin (entity, &e)) < 0)
    ; /* entity invalid */
  else
  {
    ret = dds_entity_observer_unregister (e, &ws->m_entity);
    dds_entity_unpin (e);
  }

  if (ret == DDS_RETCODE_OK)
  {
    dds_waitset_remove (ws, entity);
  }
  else
  {
    if (ret != DDS_RETCODE_PRECONDITION_NOT_MET)
      ret = DDS_RETCODE_BAD_PARAMETER;
  }
  dds_waitset_unlock (ws);
  return ret;
}

dds_return_t dds_waitset_wait_until (dds_entity_t waitset, dds_attach_t *xs, size_t nxs, dds_time_t abstimeout)
{
  return dds_waitset_wait_impl (waitset, xs, nxs, abstimeout);
}

dds_return_t dds_waitset_wait (dds_entity_t waitset, dds_attach_t *xs, size_t nxs, dds_duration_t reltimeout)
{
  if (reltimeout < 0)
    return DDS_RETCODE_BAD_PARAMETER;
  const dds_time_t tnow = dds_time ();
  const dds_time_t abstimeout = (DDS_INFINITY - reltimeout <= tnow) ? DDS_NEVER : (tnow + reltimeout);
  return dds_waitset_wait_impl (waitset, xs, nxs, abstimeout);
}

dds_return_t dds_waitset_set_trigger (dds_entity_t waitset, bool trigger)
{
  dds_entity *ent;
  dds_return_t rc;

  if ((rc = dds_entity_pin (waitset, &ent)) != DDS_RETCODE_OK)
    return rc;
  else if (dds_entity_kind (ent) != DDS_KIND_WAITSET)
  {
    dds_entity_unpin (ent);
    return DDS_RETCODE_ILLEGAL_OPERATION;
  }

  ddsrt_mutex_lock (&ent->m_observers_lock);
  dds_entity_trigger_set (ent, trigger);
  ddsrt_mutex_unlock (&ent->m_observers_lock);

  dds_entity_unpin (ent);
  return DDS_RETCODE_OK;
}
