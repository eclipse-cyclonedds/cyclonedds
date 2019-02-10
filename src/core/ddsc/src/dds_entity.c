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
#include "dds__entity.h"
#include "dds__write.h"
#include "dds__writer.h"
#include "dds__reader.h"
#include "dds__listener.h"
#include "dds__err.h"
#include "ddsc/ddsc_project.h"

/* Sanity check. */
#if DDS_ENTITY_KIND_MASK != UT_HANDLE_KIND_MASK
#error "DDS_ENTITY_KIND_MASK != UT_HANDLE_KIND_MASK"
#endif

extern inline bool dds_entity_is_enabled (const dds_entity *e);
extern inline void dds_entity_status_reset (dds_entity *e, uint32_t t);
extern inline bool dds_entity_status_match (const dds_entity *e, uint32_t t);
extern inline dds_entity_kind_t dds_entity_kind (const dds_entity *e);
extern inline dds_entity_kind_t dds_entity_kind_from_handle (dds_entity_t hdl);

static void dds_entity_observers_signal (dds_entity *observed, uint32_t status);
static void dds_entity_observers_delete (dds_entity *observed);

void dds_entity_add_ref_nolock (dds_entity *e)
{
  e->m_refc++;
}

void dds_entity_add_ref (dds_entity *e)
{
  os_mutexLock (&e->m_mutex);
  dds_entity_add_ref_nolock (e);
  os_mutexUnlock (&e->m_mutex);
}

dds_domain *dds__entity_domain (dds_entity *e)
{
  return e->m_domain;
}

static void dds_set_explicit (dds_entity_t entity)
{
  dds_entity *e;
  dds__retcode_t rc;
  if ((rc = dds_entity_lock (entity, DDS_KIND_DONTCARE, &e)) == DDS_RETCODE_OK)
  {
    e->m_flags &= ~DDS_ENTITY_IMPLICIT;
    dds_entity_unlock (e);
  }
}

static dds_entity *dds__nonself_parent (dds_entity *e)
{
  return e->m_parent == e ? NULL : e->m_parent;
}

dds_entity_t dds_entity_init (dds_entity *e, dds_entity *parent, dds_entity_kind_t kind, dds_qos_t *qos, const dds_listener_t *listener, uint32_t mask)
{
  assert ((kind == DDS_KIND_PARTICIPANT) == (parent == NULL));
  assert (e);

  e->m_refc = 1;
  e->m_qos = qos;
  e->m_cb_count = 0;
  e->m_observers = NULL;
  e->m_trigger = 0;

  /* TODO: CHAM-96: Implement dynamic enabling of entity. */
  e->m_flags |= DDS_ENTITY_ENABLED;

  /* set the status enable based on kind */
  e->m_status_enable = mask | DDS_INTERNAL_STATUS_MASK;

  os_mutexInit (&e->m_mutex);
  os_mutexInit (&e->m_observers_lock);
  os_condInit (&e->m_cond, &e->m_mutex);
  os_condInit (&e->m_observers_cond, &e->m_observers_lock);

  if (parent)
  {
    e->m_parent = parent;
    e->m_domain = parent->m_domain;
    e->m_domainid = parent->m_domainid;
    e->m_participant = parent->m_participant;
    e->m_next = parent->m_children;
    parent->m_children = e;
  }
  else
  {
    e->m_participant = e;
    e->m_parent = e;
  }

  dds_reset_listener (&e->m_listener);
  if (listener)
    dds_merge_listener (&e->m_listener, listener);
  if (parent)
  {
    os_mutexLock (&e->m_observers_lock);
    dds_inherit_listener (&e->m_listener, &parent->m_listener);
    os_mutexUnlock (&e->m_observers_lock);
  }

  e->m_hdllink = NULL;
  e->m_hdl = ut_handle_create ((int32_t) kind, e);
  if (e->m_hdl > 0)
  {
    e->m_hdllink = ut_handle_get_link (e->m_hdl);
    assert(e->m_hdllink);
  }
  else
  {
    if (e->m_hdl == UT_HANDLE_OUT_OF_RESOURCES) {
      DDS_ERROR ("Can not create new entity; too many where created previously\n");
      e->m_hdl = DDS_ERRNO (DDS_RETCODE_OUT_OF_RESOURCES);
    } else if (e->m_hdl == UT_HANDLE_NOT_INITALIZED) {
      DDS_ERROR (DDSC_PROJECT_NAME" is not yet initialized. Please create a participant before executing an other method\n");
      e->m_hdl = DDS_ERRNO (DDS_RETCODE_PRECONDITION_NOT_MET);
    } else {
      DDS_ERROR ("An internal error has occurred\n");
      e->m_hdl = DDS_ERRNO (DDS_RETCODE_ERROR);
    }
  }

  /* An ut_handle_t is directly used as dds_entity_t. */
  return (dds_entity_t) e->m_hdl;
}

dds_return_t dds_delete (dds_entity_t entity)
{
  return dds_delete_impl (entity, false);
}

static dds_entity *next_non_topic_child (dds_entity *remaining_children)
{
  while (remaining_children != NULL && dds_entity_kind (remaining_children) == DDS_KIND_TOPIC)
    remaining_children = remaining_children->m_next;
  return remaining_children;
}

dds_return_t dds_delete_impl (dds_entity_t entity, bool keep_if_explicit)
{
  os_time timeout = { 10, 0 };
  dds_entity *e;
  dds_entity *child;
  dds_entity *parent;
  dds_entity *prev = NULL;
  dds_entity *next = NULL;
  dds_return_t ret;
  dds__retcode_t rc;

  rc = dds_entity_lock (entity, UT_HANDLE_DONTCARE_KIND, &e);
  if (rc != DDS_RETCODE_OK)
  {
    DDS_TRACE ("dds_delete_impl: error on locking entity %"PRIu32" keep_if_explicit %d\n", entity, (int) keep_if_explicit);
    return DDS_ERRNO (rc);
  }

  if (keep_if_explicit == true && (e->m_flags & DDS_ENTITY_IMPLICIT) == 0)
  {
    dds_entity_unlock(e);
    return DDS_RETCODE_OK;
  }

  if (--e->m_refc != 0)
  {
    dds_entity_unlock (e);
    return DDS_RETCODE_OK;
  }

  ut_handle_close (e->m_hdl, e->m_hdllink);
  os_mutexLock (&e->m_observers_lock);
  while (e->m_cb_count > 0)
    os_condWait (&e->m_observers_cond, &e->m_observers_lock);
  e->m_status_enable = 0;
  dds_reset_listener (&e->m_listener);
  e->m_trigger |= DDS_DELETING_STATUS;
  os_mutexUnlock (&e->m_observers_lock);
  dds_entity_unlock(e);

  /* Signal observers that this entity will be deleted. */
  dds_entity_status_signal (e);

  /*
   * Recursively delete children.
   *
   * It is possible that a writer/reader has the last reference
   * to a topic. This will mean that when deleting a writer could
   * cause a topic to be deleted.
   * This can cause issues when deleting the children of a participant:
   * when a topic is the next child in line to be deleted, while at the
   * same time it is already being deleted due to the recursive deletion
   * of a publisher->writer.
   *
   * Another problem is that when the topic was already deleted, and
   * we'd delete it here for the second time before the writer/reader
   * is deleted, they will have dangling pointers.
   *
   * To circumvent the problem. We ignore topics in the first loop.
   */
  ret = DDS_RETCODE_OK;
  child = next_non_topic_child (e->m_children);
  while (child != NULL && ret == DDS_RETCODE_OK)
  {
    next = next_non_topic_child (child->m_next);
    /* This will probably delete the child entry from the current children's list */
    ret = dds_delete (child->m_hdl);
    child = next;
  }
  child = e->m_children;
  while (child != NULL && ret == DDS_RETCODE_OK)
  {
    next = child->m_next;
    assert (dds_entity_kind (child) == DDS_KIND_TOPIC);
    ret = dds_delete (child->m_hdl);
    child = next;
  }
  if (ret == DDS_RETCODE_OK && e->m_deriver.close)
  {
    /* Close the entity. This can terminate threads or kick of
     * other destroy stuff that takes a while. */
    ret = e->m_deriver.close (e);
  }

  if (ret == DDS_RETCODE_OK)
  {
    /* The ut_handle_delete will wait until the last active claim on that handle
     * is released. It is possible that this last release will be done by a thread
     * that was kicked during the close(). */
    if (ut_handle_delete (e->m_hdl, e->m_hdllink, timeout) != UT_HANDLE_OK)
      return DDS_ERRNO (DDS_RETCODE_TIMEOUT);
  }

  if (ret == DDS_RETCODE_OK)
  {
    /* Remove all possible observers. */
    dds_entity_observers_delete (e);

    /* Remove from parent */
    if ((parent = dds__nonself_parent(e)) != NULL)
    {
      os_mutexLock (&parent->m_mutex);
      child = parent->m_children;
      while (child && child != e)
      {
        prev = child;
        child = child->m_next;
      }
      assert (child != NULL);
      if (prev)
        prev->m_next = e->m_next;
      else
        parent->m_children = e->m_next;
      os_mutexUnlock (&parent->m_mutex);
    }

    /* Do some specific deletion when needed. */
    if (e->m_deriver.delete)
      ret = e->m_deriver.delete(e);
  }

  if (ret == DDS_RETCODE_OK)
  {
    dds_delete_qos (e->m_qos);
    os_condDestroy (&e->m_cond);
    os_condDestroy (&e->m_observers_cond);
    os_mutexDestroy (&e->m_mutex);
    os_mutexDestroy (&e->m_observers_lock);
    dds_free (e);
  }

    return ret;
  }

dds_entity_t dds_get_parent (dds_entity_t entity)
{
  dds_entity *e;
  dds__retcode_t rc;
  if ((rc = dds_entity_lock (entity, DDS_KIND_DONTCARE, &e)) != DDS_RETCODE_OK)
    return DDS_ERRNO (rc);
  else
  {
    dds_entity *parent;
    dds_entity_t hdl;
    if ((parent = dds__nonself_parent(e)) == NULL)
      hdl = DDS_ENTITY_NIL;
    else
    {
      hdl = parent->m_hdl;
      dds_set_explicit (hdl);
    }
    dds_entity_unlock (e);
    return hdl;
  }
}

dds_entity_t dds_get_participant (dds_entity_t entity)
{
  dds_entity *e;
  dds__retcode_t rc;
  if ((rc = dds_entity_lock (entity, DDS_KIND_DONTCARE, &e)) != DDS_RETCODE_OK)
    return DDS_ERRNO (rc);
  else
  {
    dds_entity_t hdl = e->m_participant->m_hdl;
    dds_entity_unlock (e);
    return hdl;
  }
}

dds_return_t dds_get_children (dds_entity_t entity, dds_entity_t *children, size_t size)
{
  dds_entity *e;
  dds__retcode_t rc;

  if (children != NULL && (size <= 0 || size >= INT32_MAX))
    return DDS_ERRNO (DDS_RETCODE_BAD_PARAMETER);
  if (children == NULL && size != 0)
    return DDS_ERRNO (DDS_RETCODE_BAD_PARAMETER);

  if ((rc = dds_entity_lock (entity, DDS_KIND_DONTCARE, &e)) != DDS_RETCODE_OK)
    return DDS_ERRNO (rc);
  else
  {
    dds_return_t n = 0;
    dds_entity *iter = e->m_children;
    while (iter)
    {
      if ((size_t) n < size)
      {
        children[n] = iter->m_hdl;
        dds_set_explicit (iter->m_hdl);
      }
      n++;
      iter = iter->m_next;
    }
    dds_entity_unlock(e);
    return n;
  }
}

dds_return_t dds_get_qos (dds_entity_t entity, dds_qos_t *qos)
{
  dds_entity *e;
  dds__retcode_t rc;
  dds_return_t ret;

  if (qos == NULL)
    return DDS_ERRNO (DDS_RETCODE_BAD_PARAMETER);

  if ((rc = dds_entity_lock (entity, DDS_KIND_DONTCARE, &e)) != DDS_RETCODE_OK)
    return DDS_ERRNO (rc);

  if (e->m_deriver.set_qos == 0)
    ret = DDS_ERRNO(DDS_RETCODE_ILLEGAL_OPERATION);
  else
  {
    dds_reset_qos (qos);
    ret = dds_copy_qos (qos, e->m_qos);
  }
  dds_entity_unlock(e);
  return ret;
}

dds_return_t dds_set_qos (dds_entity_t entity, const dds_qos_t *qos)
{
  dds_entity *e;
  dds__retcode_t rc;
  dds_return_t ret;

  if (qos == NULL)
    return DDS_ERRNO (DDS_RETCODE_BAD_PARAMETER);

  if ((rc = dds_entity_lock (entity, DDS_KIND_DONTCARE, &e)) != DDS_RETCODE_OK)
    return DDS_ERRNO (rc);

  if (e->m_deriver.set_qos == 0)
    ret = DDS_ERRNO (DDS_RETCODE_ILLEGAL_OPERATION);
  else
  {
    if ((ret = e->m_deriver.set_qos (e, qos, e->m_flags & DDS_ENTITY_ENABLED)) == DDS_RETCODE_OK)
    {
      if (e->m_qos == NULL)
        e->m_qos = dds_create_qos ();
      rc = dds_copy_qos (e->m_qos, qos);
      ret = DDS_ERRNO (rc);
    }
  }
  dds_entity_unlock (e);
  return ret;
}

dds_return_t dds_get_listener (dds_entity_t entity, dds_listener_t *listener)
{
  dds_entity *e;
  dds_return_t ret = DDS_RETCODE_OK;
  dds__retcode_t rc;

  if (listener != NULL) {
    rc = dds_entity_lock(entity, DDS_KIND_DONTCARE, &e);
    if (rc == DDS_RETCODE_OK) {
      os_mutexLock (&e->m_observers_lock);
      dds_copy_listener (listener, &e->m_listener);
      os_mutexUnlock (&e->m_observers_lock);
      dds_entity_unlock(e);
    } else {
      DDS_ERROR("Error occurred on locking entity\n");
      ret = DDS_ERRNO(rc);
    }
  } else {
    DDS_ERROR("Argument listener is NULL\n");
    ret = DDS_ERRNO(DDS_RETCODE_BAD_PARAMETER);
  }

  return ret;
}

void dds_entity_invoke_listener (const dds_entity *entity, enum dds_status_id which, const void *vst)
{
  struct dds_listener const * const lst = &entity->m_listener;
  switch (which)
  {
    case DDS_INCONSISTENT_TOPIC_STATUS_ID: {
      struct dds_inconsistent_topic_status const * const st = vst;
      lst->on_inconsistent_topic (entity->m_hdl, *st, lst->on_inconsistent_topic_arg);
      break;
    }
    case DDS_REQUESTED_DEADLINE_MISSED_STATUS_ID: {
      struct dds_requested_deadline_missed_status const * const st = vst;
      lst->on_requested_deadline_missed (entity->m_hdl, *st, lst->on_requested_deadline_missed_arg);
      break;
    }
    case DDS_REQUESTED_INCOMPATIBLE_QOS_STATUS_ID: {
      struct dds_requested_incompatible_qos_status const * const st = vst;
      lst->on_requested_incompatible_qos (entity->m_hdl, *st, lst->on_requested_incompatible_qos_arg);
      break;
    }
    case DDS_SAMPLE_LOST_STATUS_ID: {
      struct dds_sample_lost_status const * const st = vst;
      lst->on_sample_lost (entity->m_hdl, *st, lst->on_sample_lost_arg);
      break;
    }
    case DDS_SAMPLE_REJECTED_STATUS_ID: {
      struct dds_sample_rejected_status const * const st = vst;
      lst->on_sample_rejected (entity->m_hdl, *st, lst->on_sample_rejected_arg);
      break;
    }
    case DDS_LIVELINESS_CHANGED_STATUS_ID: {
      struct dds_liveliness_changed_status const * const st = vst;
      lst->on_liveliness_changed (entity->m_hdl, *st, lst->on_liveliness_changed_arg);
      break;
    }
    case DDS_SUBSCRIPTION_MATCHED_STATUS_ID: {
      struct dds_subscription_matched_status const * const st = vst;
      lst->on_subscription_matched (entity->m_hdl, *st, lst->on_subscription_matched_arg);
      break;
    }
    case DDS_OFFERED_DEADLINE_MISSED_STATUS_ID: {
      struct dds_offered_deadline_missed_status const * const st = vst;
      lst->on_offered_deadline_missed (entity->m_hdl, *st, lst->on_offered_deadline_missed_arg);
      break;
    }
    case DDS_LIVELINESS_LOST_STATUS_ID: {
      struct dds_liveliness_lost_status const * const st = vst;
      lst->on_liveliness_lost (entity->m_hdl, *st, lst->on_liveliness_lost_arg);
      break;
    }
    case DDS_OFFERED_INCOMPATIBLE_QOS_STATUS_ID: {
      struct dds_offered_incompatible_qos_status const * const st = vst;
      lst->on_offered_incompatible_qos (entity->m_hdl, *st, lst->on_offered_incompatible_qos_arg);
      break;
    }
    case DDS_PUBLICATION_MATCHED_STATUS_ID: {
      struct dds_publication_matched_status const * const st = vst;
      lst->on_publication_matched (entity->m_hdl, *st, lst->on_publication_matched_arg);
      break;
    }
    case DDS_DATA_AVAILABLE_STATUS_ID: {
      lst->on_data_available (entity->m_hdl, lst->on_data_available_arg);
      break;
    }
    case DDS_DATA_ON_READERS_STATUS_ID: {
      lst->on_data_on_readers (entity->m_hdl, lst->on_data_on_readers_arg);
      break;
    }
  }
}

static void clear_status_with_listener (struct dds_entity *e)
{
  const struct dds_listener *lst = &e->m_listener;
  if (lst->on_inconsistent_topic)
    e->m_trigger &= ~DDS_INCONSISTENT_TOPIC_STATUS;
  if (lst->on_liveliness_lost)
    e->m_trigger &= ~DDS_LIVELINESS_LOST_STATUS;
  if (lst->on_offered_deadline_missed)
    e->m_trigger &= ~DDS_OFFERED_DEADLINE_MISSED_STATUS;
  if (lst->on_offered_deadline_missed_arg)
    e->m_trigger &= ~DDS_OFFERED_DEADLINE_MISSED_STATUS;
  if (lst->on_offered_incompatible_qos)
    e->m_trigger &= ~DDS_OFFERED_INCOMPATIBLE_QOS_STATUS;
  if (lst->on_data_on_readers)
    e->m_trigger &= ~DDS_DATA_ON_READERS_STATUS;
  if (lst->on_sample_lost)
    e->m_trigger &= ~DDS_SAMPLE_LOST_STATUS;
  if (lst->on_data_available)
    e->m_trigger &= ~DDS_DATA_AVAILABLE_STATUS;
  if (lst->on_sample_rejected)
    e->m_trigger &= ~DDS_SAMPLE_REJECTED_STATUS;
  if (lst->on_liveliness_changed)
    e->m_trigger &= ~DDS_LIVELINESS_CHANGED_STATUS;
  if (lst->on_requested_deadline_missed)
    e->m_trigger &= ~DDS_REQUESTED_DEADLINE_MISSED_STATUS;
  if (lst->on_requested_incompatible_qos)
    e->m_trigger &= ~DDS_REQUESTED_INCOMPATIBLE_QOS_STATUS;
  if (lst->on_publication_matched)
    e->m_trigger &= ~DDS_PUBLICATION_MATCHED_STATUS;
  if (lst->on_subscription_matched)
    e->m_trigger &= ~DDS_SUBSCRIPTION_MATCHED_STATUS;
}

static void pushdown_listener (dds_entity_t entity)
{
  dds_entity_t *cs = NULL;
  int ncs, size = 0;
  while ((ncs = dds_get_children (entity, cs, (size_t) size)) > size)
  {
    size = ncs;
    cs = os_realloc (cs, (size_t) size * sizeof (*cs));
  }
  for (int i = 0; i < ncs; i++)
  {
    dds_entity *e;
    if (dds_entity_lock (cs[i], DDS_KIND_DONTCARE, &e) == DDS_RETCODE_OK)
    {
      dds_listener_t tmp;
      os_mutexLock (&e->m_observers_lock);
      while (e->m_cb_count > 0)
        os_condWait (&e->m_observers_cond, &e->m_observers_lock);
      dds_get_listener (entity, &tmp);
      dds_override_inherited_listener (&e->m_listener, &tmp);
      clear_status_with_listener (e);
      os_mutexUnlock (&e->m_observers_lock);
      dds_entity_unlock (e);
    }
  }
  os_free (cs);
}

dds_return_t dds_set_listener (dds_entity_t entity, const dds_listener_t *listener)
{
  dds_entity *e, *x;
  dds__retcode_t rc;

  if ((rc = dds_entity_lock (entity, DDS_KIND_DONTCARE, &e)) != DDS_RETCODE_OK)
    return DDS_ERRNO (rc);

  os_mutexLock (&e->m_observers_lock);
  while (e->m_cb_count > 0)
    os_condWait (&e->m_observers_cond, &e->m_observers_lock);

  /* new listener is constructed by combining "listener" with the ancestral listeners;
     the new set of listeners is then pushed down into the descendant entities, overriding
     the ones they originally inherited from */
  dds_reset_listener (&e->m_listener);
  if (listener)
    dds_merge_listener (&e->m_listener, listener);
  x = e;
  while (dds_entity_kind (x) != DDS_KIND_PARTICIPANT)
  {
    x = x->m_parent;
    dds_inherit_listener (&e->m_listener, &x->m_listener);
  }
  clear_status_with_listener (e);
  os_mutexUnlock (&e->m_observers_lock);
  dds_entity_unlock (e);
  pushdown_listener (entity);
  return DDS_RETCODE_OK;
}

dds_return_t dds_enable (dds_entity_t entity)
{
  dds_entity *e;
  dds__retcode_t rc;

  if ((rc = dds_entity_lock(entity, DDS_KIND_DONTCARE, &e)) != DDS_RETCODE_OK)
    return DDS_ERRNO (rc);

  if ((e->m_flags & DDS_ENTITY_ENABLED) == 0)
  {
    /* TODO: Really enable. */
    e->m_flags |= DDS_ENTITY_ENABLED;
    DDS_ERROR ("Delayed entity enabling is not supported\n");
  }
  dds_entity_unlock(e);
  return DDS_RETCODE_OK;
}

dds_return_t dds_get_status_changes (dds_entity_t entity, uint32_t *status)
{
  dds_entity *e;
  dds__retcode_t rc;
  dds_return_t ret;

  if (status == NULL)
    return DDS_ERRNO (DDS_RETCODE_BAD_PARAMETER);

  if ((rc = dds_entity_lock (entity, DDS_KIND_DONTCARE, &e)) != DDS_RETCODE_OK)
    return DDS_ERRNO (rc);

  if (e->m_deriver.validate_status == 0)
    ret = DDS_ERRNO (DDS_RETCODE_ILLEGAL_OPERATION);
  else
  {
    os_mutexLock (&e->m_observers_lock);
    *status = e->m_trigger;
    os_mutexUnlock (&e->m_observers_lock);
    ret = DDS_RETCODE_OK;
  }
  dds_entity_unlock(e);
  return ret;
}

dds_return_t dds_get_status_mask (dds_entity_t entity, uint32_t *mask)
{
  dds_entity *e;
  dds__retcode_t rc;
  dds_return_t ret;

  if (mask == NULL)
    return DDS_ERRNO (DDS_RETCODE_BAD_PARAMETER);

  if ((rc = dds_entity_lock (entity, DDS_KIND_DONTCARE, &e)) != DDS_RETCODE_OK)
    return DDS_ERRNO (rc);

  if (e->m_deriver.validate_status == 0)
    ret = DDS_ERRNO (DDS_RETCODE_ILLEGAL_OPERATION);
  else
  {
    os_mutexLock (&e->m_observers_lock);
    *mask = (e->m_status_enable & ~DDS_INTERNAL_STATUS_MASK);
    os_mutexUnlock (&e->m_observers_lock);
    ret = DDS_RETCODE_OK;
  }
  dds_entity_unlock(e);
  return ret;
}

dds_return_t dds_get_enabled_status (dds_entity_t entity, uint32_t *status)
{
  return dds_get_status_mask(entity, status);
}

dds_return_t dds_set_status_mask (dds_entity_t entity, uint32_t mask)
{
  dds_entity *e;
  dds__retcode_t rc;
  dds_return_t ret;

  if ((rc = dds_entity_lock (entity, DDS_KIND_DONTCARE, &e)) != DDS_RETCODE_OK)
    return DDS_ERRNO (rc);

  if (e->m_deriver.validate_status == 0)
    ret = DDS_ERRNO (DDS_RETCODE_ILLEGAL_OPERATION);
  else if ((ret = e->m_deriver.validate_status (mask)) == DDS_RETCODE_OK)
  {
    os_mutexLock (&e->m_observers_lock);
    /* Don't block internal status triggers. */
    mask |= DDS_INTERNAL_STATUS_MASK;
    e->m_status_enable = mask;
    e->m_trigger &= mask;
    os_mutexUnlock (&e->m_observers_lock);
  }
  dds_entity_unlock(e);
  return ret;
}

dds_return_t dds_set_enabled_status(dds_entity_t entity, uint32_t mask)
{
  return dds_set_status_mask( entity, mask);
}

static dds_return_t dds_readtake_status (dds_entity_t entity, uint32_t *status, uint32_t mask, bool reset)
{
  dds_entity *e;
  dds__retcode_t rc;
  dds_return_t ret;

  if (status == NULL)
    return DDS_ERRNO (DDS_RETCODE_BAD_PARAMETER);

  if ((rc = dds_entity_lock (entity, DDS_KIND_DONTCARE, &e)) != DDS_RETCODE_OK)
    return DDS_ERRNO (rc);

  if (e->m_deriver.validate_status == 0)
    ret = DDS_ERRNO (DDS_RETCODE_ILLEGAL_OPERATION);
  else if ((ret = e->m_deriver.validate_status (mask)) == DDS_RETCODE_OK)
  {
    os_mutexLock (&e->m_observers_lock);
    *status = e->m_trigger & mask;
    if (reset)
      e->m_trigger &= ~mask;
    os_mutexUnlock (&e->m_observers_lock);
  }
  dds_entity_unlock (e);
  return ret;
}


dds_return_t dds_read_status (dds_entity_t entity, uint32_t *status, uint32_t mask)
{
  return dds_readtake_status (entity, status, mask, false);
}

dds_return_t dds_take_status (dds_entity_t entity, uint32_t *status, uint32_t mask)
{
  return dds_readtake_status (entity, status, mask, true);
}

dds_return_t dds_get_domainid (dds_entity_t entity, dds_domainid_t *id)
{
  dds_entity *e;
  dds__retcode_t rc;

  if (id == NULL)
    return DDS_ERRNO (DDS_RETCODE_BAD_PARAMETER);

  if ((rc = dds_entity_lock (entity, DDS_KIND_DONTCARE, &e)) != DDS_RETCODE_OK)
    return DDS_ERRNO (rc);

  *id = e->m_domainid;
  dds_entity_unlock(e);
  return DDS_RETCODE_OK;
}

dds_return_t dds_get_instance_handle (dds_entity_t entity, dds_instance_handle_t *ihdl)
{
  dds_entity *e;
  dds__retcode_t rc;
  dds_return_t ret;

  if (ihdl == NULL)
    return DDS_ERRNO (DDS_RETCODE_BAD_PARAMETER);

  if ((rc = dds_entity_lock (entity, DDS_KIND_DONTCARE, &e)) != DDS_RETCODE_OK)
    return DDS_ERRNO (rc);

  if (e->m_deriver.get_instance_hdl)
    ret = e->m_deriver.get_instance_hdl (e, ihdl);
  else
    ret = DDS_ERRNO (DDS_RETCODE_ILLEGAL_OPERATION);
  dds_entity_unlock(e);
  return ret;
}


dds__retcode_t dds_valid_hdl (dds_entity_t hdl, dds_entity_kind_t kind)
{
  ut_handle_t utr;
  if ((utr = ut_handle_status (hdl, NULL, (int32_t) kind)) == UT_HANDLE_OK)
    return DDS_RETCODE_OK;
  else if (hdl < 0)
    return DDS_RETCODE_BAD_PARAMETER;
  else
  {
    switch (utr)
    {
      case UT_HANDLE_OK:
        assert (0);
        /* FALLS THROUGH */
      case UT_HANDLE_UNEQUAL_KIND:
        return DDS_RETCODE_ILLEGAL_OPERATION;
      case UT_HANDLE_INVALID:
        return DDS_RETCODE_BAD_PARAMETER;
      case UT_HANDLE_DELETED:
      case UT_HANDLE_CLOSED:
        return DDS_RETCODE_ALREADY_DELETED;
      default:
        return DDS_RETCODE_ERROR;
    }
  }
}

dds__retcode_t dds_entity_lock (dds_entity_t hdl, dds_entity_kind_t kind, dds_entity **eptr)
{
  ut_handle_t utr;
  void *raw;

  /* When the given handle already contains an error, then return that
   * same error to retain the original information. */
  if ((utr = ut_handle_claim (hdl, NULL, (int32_t) kind, &raw)) == UT_HANDLE_OK)
  {
    dds_entity *e;
    *eptr = e = raw;
    os_mutexLock (&e->m_mutex);
    /* FIXME: The handle could have been closed while we were waiting for the mutex -- that should be handled differently!

       For now, however, it is really important at two points in the logic:
         (1) preventing creating new entities as children of a one that is currently being deleted, and
         (2) preventing dds_delete_impl from doing anything once the entity is being deleted.

       Without (1), it would be possible to add children while trying to delete them, without (2) you're looking at crashes. */
    if (ut_handle_is_closed (hdl, e->m_hdllink))
    {
      dds_entity_unlock (e);
      return DDS_RETCODE_ALREADY_DELETED;
    }
    return DDS_RETCODE_OK;
  }
  else if (hdl < 0)
  {
    return DDS_RETCODE_BAD_PARAMETER;
  }
  else
  {
    switch (utr)
    {
      case UT_HANDLE_OK:
        assert (0);
        /* FALLS THROUGH */
      case UT_HANDLE_UNEQUAL_KIND:
        return DDS_RETCODE_ILLEGAL_OPERATION;
      case UT_HANDLE_INVALID:
        return DDS_RETCODE_BAD_PARAMETER;
      case UT_HANDLE_DELETED:
      case UT_HANDLE_CLOSED:
        return DDS_RETCODE_ALREADY_DELETED;
      default:
        return DDS_RETCODE_ERROR;
    }
  }
}

void dds_entity_unlock (dds_entity *e)
{
  os_mutexUnlock (&e->m_mutex);
  ut_handle_release (e->m_hdl, e->m_hdllink);
}

dds_return_t dds_triggered (dds_entity_t entity)
{
  dds_entity *e;
  dds_return_t ret;
  dds__retcode_t rc;

  if ((rc = dds_entity_lock(entity, DDS_KIND_DONTCARE, &e)) != DDS_RETCODE_OK)
    return DDS_ERRNO (rc);
  os_mutexLock (&e->m_observers_lock);
  ret = (e->m_trigger != 0);
  os_mutexUnlock (&e->m_observers_lock);
  dds_entity_unlock (e);
  return ret;
}

static bool in_observer_list_p (const struct dds_entity *observed, const dds_entity_t observer)
{
  dds_entity_observer *cur;
  for (cur = observed->m_observers; cur != NULL; cur = cur->m_next)
    if (cur->m_observer == observer)
      return true;
  return false;
}

dds__retcode_t dds_entity_observer_register_nl (dds_entity *observed, dds_entity_t observer, dds_entity_callback cb)
{
  dds__retcode_t rc;
  assert (observed);
  os_mutexLock (&observed->m_observers_lock);
  if (in_observer_list_p (observed, observer))
    rc = DDS_RETCODE_PRECONDITION_NOT_MET;
  else
  {
    dds_entity_observer *o = os_malloc (sizeof (dds_entity_observer));
    o->m_cb = cb;
    o->m_observer = observer;
    o->m_next = observed->m_observers;
    observed->m_observers = o;
    rc = DDS_RETCODE_OK;
  }
  os_mutexUnlock (&observed->m_observers_lock);
  return rc;
}

dds__retcode_t dds_entity_observer_register (dds_entity_t observed, dds_entity_t observer, dds_entity_callback cb)
{
  dds__retcode_t rc;
  dds_entity *e;
  assert (cb);
  if ((rc = dds_entity_lock (observed, DDS_KIND_DONTCARE, &e)) != DDS_RETCODE_OK)
    return rc;
  rc = dds_entity_observer_register_nl (e, observer, cb);
  dds_entity_unlock (e);
  return rc;
}

dds__retcode_t dds_entity_observer_unregister_nl (dds_entity *observed, dds_entity_t observer)
{
  dds__retcode_t rc;
  dds_entity_observer *prev, *idx;

  os_mutexLock (&observed->m_observers_lock);
  prev = NULL;
  idx = observed->m_observers;
  while (idx != NULL && idx->m_observer != observer)
  {
    prev = idx;
    idx = idx->m_next;
  }
  if (idx == NULL)
    rc = DDS_RETCODE_PRECONDITION_NOT_MET;
  else
  {
    if (prev == NULL)
      observed->m_observers = idx->m_next;
    else
      prev->m_next = idx->m_next;
    os_free (idx);
    rc = DDS_RETCODE_OK;
  }
  os_mutexUnlock (&observed->m_observers_lock);
  return rc;
}

dds__retcode_t dds_entity_observer_unregister (dds_entity_t observed, dds_entity_t observer)
{
  dds__retcode_t rc;
  dds_entity *e;
  if ((rc = dds_entity_lock (observed, DDS_KIND_DONTCARE, &e)) != DDS_RETCODE_OK)
    return rc;
  rc = dds_entity_observer_unregister_nl (e, observer);
  dds_entity_unlock (e);
  return rc;
}

static void dds_entity_observers_delete (dds_entity *observed)
{
  dds_entity_observer *idx;
  os_mutexLock (&observed->m_observers_lock);
  idx = observed->m_observers;
  while (idx != NULL)
  {
    dds_entity_observer *next = idx->m_next;
    os_free (idx);
    idx = next;
  }
  observed->m_observers = NULL;
  os_mutexUnlock (&observed->m_observers_lock);
}

static void dds_entity_observers_signal (dds_entity *observed, uint32_t status)
{
  for (dds_entity_observer *idx = observed->m_observers; idx; idx = idx->m_next)
    idx->m_cb (idx->m_observer, observed->m_hdl, status);
}

void dds_entity_status_signal (dds_entity *e)
{
  os_mutexLock (&e->m_observers_lock);
  dds_entity_observers_signal (e, e->m_trigger);
  os_mutexUnlock (&e->m_observers_lock);
}

void dds_entity_status_set (dds_entity *e, uint32_t t)
{
  if (!(e->m_trigger & t))
  {
    e->m_trigger |= e->m_status_enable & t;
    dds_entity_observers_signal (e, e->m_trigger);
  }
}

dds_entity_t dds_get_topic (dds_entity_t entity)
{
  dds__retcode_t rc;
  dds_entity_t hdl;
  dds_entity *e;

  if ((rc = dds_entity_lock (entity, DDS_KIND_DONTCARE, &e)) != DDS_RETCODE_OK)
    return DDS_ERRNO (rc);
  switch (dds_entity_kind (e))
  {
    case DDS_KIND_READER: {
      dds_reader *rd = (dds_reader *) e;
      hdl = rd->m_topic->m_entity.m_hdl;
      break;
    }
    case DDS_KIND_WRITER: {
      dds_writer *wr = (dds_writer *) e;
      hdl = wr->m_topic->m_entity.m_hdl;
      break;
    }
    case DDS_KIND_COND_READ:
    case DDS_KIND_COND_QUERY: {
      assert (dds_entity_kind (e->m_parent) == DDS_KIND_READER);
      dds_reader *rd = (dds_reader *) e->m_parent;
      hdl = rd->m_topic->m_entity.m_hdl;
      break;
    }
    default: {
      hdl = DDS_ERRNO (DDS_RETCODE_ILLEGAL_OPERATION);
      break;
    }
  }
  dds_entity_unlock (e);
  return hdl;
}

const char *dds__entity_kind_str (dds_entity_t e)
{
  if (e <= 0)
    return "(ERROR)";
  switch (e & DDS_ENTITY_KIND_MASK)
  {
    case DDS_KIND_TOPIC:        return "Topic";
    case DDS_KIND_PARTICIPANT:  return "Participant";
    case DDS_KIND_READER:       return "Reader";
    case DDS_KIND_WRITER:       return "Writer";
    case DDS_KIND_SUBSCRIBER:   return "Subscriber";
    case DDS_KIND_PUBLISHER:    return "Publisher";
    case DDS_KIND_COND_READ:    return "ReadCondition";
    case DDS_KIND_COND_QUERY:   return "QueryCondition";
    case DDS_KIND_WAITSET:      return "WaitSet";
    default:                    return "(INVALID_ENTITY)";
  }
}
