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
#include "dds__rhc.h"
#include "dds/ddsi/ddsi_iid.h"

DEFINE_ENTITY_LOCK_UNLOCK (static, dds_waitset, DDS_KIND_WAITSET)

static void dds_waitset_swap (dds_attachment **dst, dds_attachment **src, dds_attachment *prev, dds_attachment *idx)
{
  /* Remove from source. */
  if (prev == NULL)
    *src = idx->next;
  else
    prev->next = idx->next;

  /* Add to destination. */
  idx->next = *dst;
  *dst = idx;
}

static dds_return_t dds_waitset_wait_impl (dds_entity_t waitset, dds_attach_t *xs, size_t nxs, dds_time_t abstimeout)
{
  dds_waitset *ws;
  dds_return_t ret;
  dds_attachment *idx;
  dds_attachment *prev;

  if (xs == NULL && nxs != 0)
    return DDS_RETCODE_BAD_PARAMETER;
  if (xs != NULL && nxs == 0)
    return DDS_RETCODE_BAD_PARAMETER;

  /* Locking the waitset here will delay a possible deletion until it is
   * unlocked. Even when the related mutex is unlocked by a conditioned wait. */
  if ((ret = dds_waitset_lock (waitset, &ws)) != DDS_RETCODE_OK)
    return ret;

  /* Move any previously but no longer triggering entities back to the observed list */
  idx = ws->triggered;
  prev = NULL;
  while (idx != NULL)
  {
    dds_attachment *next = idx->next;
    if (idx->entity->m_trigger == 0)
      dds_waitset_swap (&ws->observed, &ws->triggered, prev, idx);
    else
      prev = idx;
    idx = next;
  }
  /* Check if any of the observed entities are currently triggered, moving them
     to the triggered list */
  idx = ws->observed;
  prev = NULL;
  while (idx != NULL)
  {
    dds_attachment *next = idx->next;
    if (idx->entity->m_trigger > 0)
      dds_waitset_swap (&ws->triggered, &ws->observed, prev, idx);
    else
      prev = idx;
    idx = next;
  }

  /* Only wait/keep waiting when we have something to observe and there aren't any triggers yet. */
  while (ws->observed != NULL && ws->triggered == NULL)
    if (!ddsrt_cond_waituntil (&ws->m_entity.m_cond, &ws->m_entity.m_mutex, abstimeout))
      break;

  /* Get number of triggered entities
   *   - set attach array when needed
   *   - swap them back to observed */
  ret = 0;
  idx = ws->triggered;
  while (idx != NULL)
  {
    if ((uint32_t) ret < (uint32_t) nxs)
      xs[ret] = idx->arg;
    ret++;

    /* The idx is always the first in triggered, so no prev. */
    dds_attachment *next = idx->next;
    dds_waitset_swap (&ws->observed, &ws->triggered, NULL, idx);
    idx = next;
  }
  dds_waitset_unlock (ws);
  return ret;
}

static void dds_waitset_close_list (dds_attachment **list, dds_entity_t waitset)
{
  dds_attachment *idx = *list;
  dds_attachment *next;
  while (idx != NULL)
  {
    next = idx->next;
    (void) dds_entity_observer_unregister (idx->entity->m_hdllink.hdl, waitset);
    ddsrt_free (idx);
    idx = next;
  }
  *list = NULL;
}

static bool dds_waitset_remove_from_list (dds_attachment **list, dds_entity_t observed)
{
  dds_attachment *idx, *prev;
  for (idx = *list, prev = NULL; idx != NULL; prev = idx, idx = idx->next)
    if (idx->entity->m_hdllink.hdl == observed)
      break;
  if (idx == NULL)
    return false;

  if (prev == NULL)
    *list = idx->next;
  else
    prev->next = idx->next;
  ddsrt_free (idx);
  return true;
}

static dds_return_t dds_waitset_close (struct dds_entity *e)
{
  dds_waitset *ws = (dds_waitset *) e;
  dds_waitset_close_list (&ws->observed,  e->m_hdllink.hdl);
  dds_waitset_close_list (&ws->triggered, e->m_hdllink.hdl);
  ddsrt_cond_broadcast (&e->m_cond);
  return DDS_RETCODE_OK;
}

dds_entity_t dds_create_waitset (dds_entity_t participant)
{
  dds_entity_t hdl;
  dds_participant *par;
  dds_return_t rc;

  if ((rc = dds_participant_lock (participant, &par)) != DDS_RETCODE_OK)
    return rc;

  dds_waitset *waitset = dds_alloc (sizeof (*waitset));
  hdl = dds_entity_init (&waitset->m_entity, &par->m_entity, DDS_KIND_WAITSET, NULL, NULL, 0);
  waitset->m_entity.m_iid = ddsi_iid_gen ();
  waitset->m_entity.m_deriver.close = dds_waitset_close;
  waitset->observed = NULL;
  waitset->triggered = NULL;
  dds_participant_unlock (par);
  return hdl;
}


dds_return_t dds_waitset_get_entities (dds_entity_t waitset, dds_entity_t *entities, size_t size)
{
  dds_return_t ret;
  dds_waitset *ws;

  if ((ret = dds_waitset_lock (waitset, &ws)) != DDS_RETCODE_OK)
    return ret;

  ret = 0;
  for (dds_attachment *iter = ws->observed; iter != NULL; iter = iter->next)
  {
    if ((size_t) ret < size && entities != NULL)
      entities[ret] = iter->entity->m_hdllink.hdl;
    ret++;
  }
  for (dds_attachment *iter = ws->triggered; iter != NULL; iter = iter->next)
  {
    if ((size_t) ret < size && entities != NULL)
      entities[ret] = iter->entity->m_hdllink.hdl;
    ret++;
  }
  dds_waitset_unlock(ws);
  return ret;
}

static void dds_waitset_move (dds_attachment **src, dds_attachment **dst, dds_entity_t entity)
{
  dds_attachment *idx, *prev;
  for (idx = *src, prev = NULL; idx != NULL; prev = idx, idx = idx->next)
    if (idx->entity->m_hdllink.hdl == entity)
      break;
  if (idx != NULL)
  {
    /* Swap idx from src to dst. */
    dds_waitset_swap (dst, src, prev, idx);
  }
}

static void dds_waitset_remove (dds_waitset *ws, dds_entity_t observed)
{
  if (!dds_waitset_remove_from_list (&ws->observed, observed))
    (void) dds_waitset_remove_from_list (&ws->triggered, observed);
}

/* This is called when the observed entity signals a status change. */
static void dds_waitset_observer (dds_entity_t observer, dds_entity_t observed, uint32_t status)
{
  dds_waitset *ws;
  if (dds_waitset_lock (observer, &ws) == DDS_RETCODE_OK) {
    if (status & DDS_DELETING_STATUS) {
      /* Remove this observed entity, which is being deleted, from the waitset. */
      dds_waitset_remove (ws, observed);
      /* Our registration to this observed entity will be removed automatically. */
    } else if (status != 0) {
      /* Move observed entity to triggered list. */
      dds_waitset_move (&ws->observed, &ws->triggered, observed);
    } else {
      /* Remove observed entity from triggered list (which it possibly resides in). */
      dds_waitset_move (&ws->triggered, &ws->observed, observed);
    }
    /* Trigger waitset to wake up. */
    ddsrt_cond_broadcast (&ws->m_entity.m_cond);
    dds_waitset_unlock (ws);
  }
}

dds_return_t dds_waitset_attach (dds_entity_t waitset, dds_entity_t entity, dds_attach_t x)
{
  dds_entity *e;
  dds_waitset *ws;
  dds_return_t ret;

  if ((ret = dds_waitset_lock (waitset, &ws)) != DDS_RETCODE_OK)
    return ret;

  if (waitset == entity)
    e = &ws->m_entity;
  else if ((ret = dds_entity_lock (entity, DDS_KIND_DONTCARE, &e)) != DDS_RETCODE_OK)
  {
    ret = DDS_RETCODE_BAD_PARAMETER;
    goto err_waitset;
  }

  /* This will fail if given entity is already attached (or deleted). */
  if ((ret = dds_entity_observer_register_nl (e, waitset, dds_waitset_observer)) != DDS_RETCODE_OK)
    goto err_entity;

  dds_attachment *a = ddsrt_malloc (sizeof (*a));
  a->arg = x;
  a->entity = e;
  if (e->m_trigger > 0) {
    a->next = ws->triggered;
    ws->triggered = a;
  } else {
    a->next = ws->observed;
    ws->observed = a;
  }

err_entity:
  if (e != &ws->m_entity)
    dds_entity_unlock (e);
err_waitset:
  dds_waitset_unlock (ws);
  return ret;
}

dds_return_t dds_waitset_detach (dds_entity_t waitset, dds_entity_t entity)
{
  dds_waitset *ws;
  dds_return_t ret;

  if ((ret = dds_waitset_lock (waitset, &ws)) != DDS_RETCODE_OK)
    return ret;

  /* Possibly fails when entity was not attached. */
  if (waitset == entity)
    ret = dds_entity_observer_unregister_nl (&ws->m_entity, waitset);
  else
    ret = dds_entity_observer_unregister (entity, waitset);

  if (ret == DDS_RETCODE_OK)
    dds_waitset_remove (ws, entity);
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
  return dds_waitset_wait_impl(waitset, xs, nxs, abstimeout);
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
  dds_waitset *ws;
  dds_return_t rc;

  if ((rc = dds_waitset_lock (waitset, &ws)) != DDS_RETCODE_OK)
    return rc;

  ddsrt_mutex_unlock (&ws->m_entity.m_mutex);

  ddsrt_mutex_lock (&ws->m_entity.m_observers_lock);
  if (trigger)
    dds_entity_status_set (&ws->m_entity, DDS_WAITSET_TRIGGER_STATUS);
  else
    dds_entity_status_reset (&ws->m_entity, DDS_WAITSET_TRIGGER_STATUS);
  ddsrt_mutex_unlock (&ws->m_entity.m_observers_lock);

  ddsrt_mutex_lock (&ws->m_entity.m_mutex);
  dds_waitset_unlock (ws);
  return DDS_RETCODE_OK;
}
