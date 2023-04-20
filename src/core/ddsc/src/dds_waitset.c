// Copyright(c) 2006 to 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <assert.h>

#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/log.h"
#include "dds__entity.h"
#include "dds__participant.h"
#include "dds__readcond.h"
#include "dds__init.h"
#include "dds__subscriber.h" // only for (de)materializing data_on_readers
#include "dds/ddsc/dds_rhc.h"
#include "dds/ddsi/ddsi_iid.h"

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
    default: {
      const uint32_t sm = ddsrt_atomic_ld32 (&e->m_status.m_status_and_mask);
      t = (sm & (sm >> SAM_ENABLED_SHIFT)) != 0;
      break;
    }
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
  ddsrt_mutex_lock (&ws->wait_lock);
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
  while (ws->nentities > 0 && ws->ntriggered == 0 && !dds_handle_is_closed (&ws->m_entity.m_hdllink))
    if (!ddsrt_cond_waituntil (&ws->wait_cond, &ws->wait_lock, abstimeout))
      break;

  ret = (int32_t) ws->ntriggered;
  for (size_t i = 0; i < ws->ntriggered && i < nxs; i++)
    xs[i] = ws->entities[i].arg;
  ddsrt_mutex_unlock (&ws->wait_lock);
  dds_entity_unpin (&ws->m_entity);
  return ret;
}

static void dds_waitset_interrupt (struct dds_entity *e)
{
  dds_waitset *ws = (dds_waitset *) e;
  ddsrt_mutex_lock (&ws->wait_lock);
  assert (dds_handle_is_closed (&ws->m_entity.m_hdllink));
  ddsrt_cond_broadcast (&ws->wait_cond);
  ddsrt_mutex_unlock (&ws->wait_lock);
}

static void dds_waitset_close (struct dds_entity *e)
{
  dds_waitset *ws = (dds_waitset *) e;
  ddsrt_mutex_lock (&ws->wait_lock);
  while (ws->nentities > 0)
  {
    dds_entity *observed;
    if (dds_entity_pin (ws->entities[0].handle, &observed) < 0)
    {
      /* can't be pinned => being deleted => will be removed from wait set soon enough
       and go through delete_observer (which will trigger the condition variable) */
      ddsrt_cond_wait (&ws->wait_cond, &ws->wait_lock);
    }
    else
    {
      /* entity will remain in existence */
      ddsrt_mutex_unlock (&ws->wait_lock);
      (void) dds_entity_observer_unregister (observed, ws, true);
      ddsrt_mutex_lock (&ws->wait_lock);
      assert (ws->nentities == 0 || ws->entities[0].entity != observed);
      dds_entity_unpin (observed);
    }
  }
  ddsrt_mutex_unlock (&ws->wait_lock);
}

static dds_return_t dds_waitset_delete (struct dds_entity *e)
{
  dds_waitset *ws = (dds_waitset *) e;
  ddsrt_mutex_destroy (&ws->wait_lock);
  ddsrt_cond_destroy (&ws->wait_cond);
  ddsrt_free (ws->entities);
  return DDS_RETCODE_OK;
}

const struct dds_entity_deriver dds_entity_deriver_waitset = {
  .interrupt = dds_waitset_interrupt,
  .close = dds_waitset_close,
  .delete = dds_waitset_delete,
  .set_qos = dds_entity_deriver_dummy_set_qos,
  .validate_status = dds_entity_deriver_dummy_validate_status,
  .create_statistics = dds_entity_deriver_dummy_create_statistics,
  .refresh_statistics = dds_entity_deriver_dummy_refresh_statistics
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
  dds_entity_t hdl = dds_entity_init (&waitset->m_entity, e, DDS_KIND_WAITSET, false, true, NULL, NULL, 0);
  ddsrt_mutex_init (&waitset->wait_lock);
  ddsrt_cond_init (&waitset->wait_cond);
  waitset->m_entity.m_iid = ddsi_iid_gen ();
  dds_entity_register_child (e, &waitset->m_entity);
  waitset->nentities = 0;
  waitset->ntriggered = 0;
  waitset->entities = NULL;
  dds_entity_init_complete (&waitset->m_entity);
  dds_entity_unlock (e);
  dds_entity_unpin_and_drop_ref (&dds_global.m_entity);
  return hdl;

 err_entity_kind:
  dds_entity_unlock (e);
 err_entity_lock:
  dds_entity_unpin_and_drop_ref (&dds_global.m_entity);
  return rc;
}

dds_return_t dds_waitset_get_entities (dds_entity_t waitset, dds_entity_t *entities, size_t size)
{
  dds_return_t ret;
  dds_entity *wsent;
  if ((ret = dds_entity_pin (waitset, &wsent)) < 0)
    return ret;
  else if (dds_entity_kind (wsent) != DDS_KIND_WAITSET)
  {
    dds_entity_unpin (wsent);
    return DDS_RETCODE_ILLEGAL_OPERATION;
  }
  else
  {
    dds_waitset *ws = (dds_waitset *) wsent;
    ddsrt_mutex_lock (&ws->wait_lock);
    if (entities != NULL)
    {
      for (size_t i = 0; i < ws->nentities && i < size; i++)
        entities[i] = ws->entities[i].handle;
    }
    ret = (int32_t) ws->nentities;
    ddsrt_mutex_unlock (&ws->wait_lock);
    dds_entity_unpin (&ws->m_entity);
    return ret;
  }
}

/* This is called when the observed entity signals a status change. */
static void dds_waitset_observer (struct dds_waitset *ws, dds_entity_t observed, uint32_t status)
{
  (void) status;

  ddsrt_mutex_lock (&ws->wait_lock);
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
  ddsrt_cond_broadcast (&ws->wait_cond);
  ddsrt_mutex_unlock (&ws->wait_lock);
}

struct dds_waitset_attach_observer_arg {
  dds_attach_t x;
};

static bool dds_waitset_attach_observer (struct dds_waitset *ws, struct dds_entity *observed, void *varg)
{
  struct dds_waitset_attach_observer_arg *arg = varg;
  ddsrt_mutex_lock (&ws->wait_lock);
  ws->entities = ddsrt_realloc (ws->entities, (ws->nentities + 1) * sizeof (*ws->entities));
  ws->entities[ws->nentities].arg = arg->x;
  ws->entities[ws->nentities].entity = observed;
  ws->entities[ws->nentities].handle = observed->m_hdllink.hdl;
  ws->nentities++;
  if (is_triggered (observed))
  {
    const size_t i = ws->nentities - 1;
    dds_attachment tmp = ws->entities[i];
    ws->entities[i] = ws->entities[ws->ntriggered];
    ws->entities[ws->ntriggered++] = tmp;
  }
  ddsrt_cond_broadcast (&ws->wait_cond);
  ddsrt_mutex_unlock (&ws->wait_lock);
  return true;
}

static void dds_waitset_delete_observer (struct dds_waitset *ws, dds_entity_t observed)
{
  size_t i;
  ddsrt_mutex_lock (&ws->wait_lock);
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
  }
  ddsrt_cond_broadcast (&ws->wait_cond);
  ddsrt_mutex_unlock (&ws->wait_lock);
}

dds_return_t dds_waitset_attach (dds_entity_t waitset, dds_entity_t entity, dds_attach_t x)
{
  dds_entity *wsent;
  dds_entity *e;
  dds_return_t ret;

  if ((ret = dds_entity_pin (waitset, &wsent)) < 0)
    return ret;
  else if (dds_entity_kind (wsent) != DDS_KIND_WAITSET)
  {
    dds_entity_unpin (wsent);
    return DDS_RETCODE_ILLEGAL_OPERATION;
  }
  else
  {
    dds_waitset *ws = (dds_waitset *) wsent;

    if ((ret = dds_entity_pin (entity, &e)) < 0)
      goto err_entity;

    /* Entity must be "in scope": within the participant, domain or (self-evidently true) Cyclone DDS,
       depending on the parent of the waitset, so that one can't use a waitset created in participant
       A to wait for entities in participant B, &c.  While there is no technical obstacle (though
       there might be one for cross-domain use one day), it seems rather unhygienic practice. */
    if (!dds_entity_in_scope (e, ws->m_entity.m_parent))
    {
      ret = DDS_RETCODE_BAD_PARAMETER;
      goto err_scope;
    }

    // Attaching a subscriber to a waitset forces materialization of DATA_ON_READERS
    // subscribers have no other statuses, so no point in also looking at the status mask
    // note: no locks held
    if (dds_entity_kind (e) == DDS_KIND_SUBSCRIBER)
      dds_subscriber_adjust_materialize_data_on_readers ((dds_subscriber *) e, true);

    /* This will fail if given entity is already attached (or deleted). */
    struct dds_waitset_attach_observer_arg attach_arg = { .x = x };
    ret = dds_entity_observer_register (e, ws, dds_waitset_observer, dds_waitset_attach_observer, &attach_arg, dds_waitset_delete_observer);

    // If it failed for a subscriber, undo the DATA_ON_READERS materialize changes
    // note: no locks held
    if (ret < 0 && dds_entity_kind (e) == DDS_KIND_SUBSCRIBER)
      dds_subscriber_adjust_materialize_data_on_readers ((dds_subscriber *) e, false);

  err_scope:
    dds_entity_unpin (e);
  err_entity:
    dds_entity_unpin (&ws->m_entity);
    return ret;
  }
}

dds_return_t dds_waitset_detach (dds_entity_t waitset, dds_entity_t entity)
{
  dds_entity *wsent;
  dds_return_t ret;

  if ((ret = dds_entity_pin (waitset, &wsent)) != DDS_RETCODE_OK)
    return ret;
  else if (dds_entity_kind (wsent) != DDS_KIND_WAITSET)
  {
    dds_entity_unpin (wsent);
    return DDS_RETCODE_ILLEGAL_OPERATION;
  }
  else
  {
    dds_waitset *ws = (dds_waitset *) wsent;
    dds_entity *e;
    /* Possibly fails when entity was not attached. */
    if (waitset == entity)
      ret = dds_entity_observer_unregister (&ws->m_entity, ws, true);
    else if ((ret = dds_entity_pin (entity, &e)) < 0)
      ; /* entity invalid */
    else
    {
      ret = dds_entity_observer_unregister (e, ws, true);

      // This waitset no longer requires a subscriber to have a materialized DATA_ON_READERS
      if (ret >= 0 && dds_entity_kind (e) == DDS_KIND_SUBSCRIBER)
        dds_subscriber_adjust_materialize_data_on_readers ((dds_subscriber *) e, false);

      dds_entity_unpin (e);
    }

    dds_entity_unpin (&ws->m_entity);
    if (ret != DDS_RETCODE_OK && ret != DDS_RETCODE_PRECONDITION_NOT_MET)
      ret = DDS_RETCODE_BAD_PARAMETER;
    return ret;
  }
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
  else
  {
    uint32_t oldst;
    ddsrt_mutex_lock (&ent->m_observers_lock);
    do {
      oldst = ddsrt_atomic_ld32 (&ent->m_status.m_trigger);
    } while (!ddsrt_atomic_cas32 (&ent->m_status.m_trigger, oldst, trigger));
    if (oldst == 0 && trigger != 0)
      dds_entity_observers_signal (ent, trigger);
    ddsrt_mutex_unlock (&ent->m_observers_lock);
    dds_entity_unpin (ent);
    return DDS_RETCODE_OK;
  }
}
