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

#include "dds/ddsrt/cdtors.h"
#include "dds/ddsi/q_entity.h"
#include "dds/ddsi/q_thread.h"
#include "dds/ddsi/q_config.h"
#include "dds/ddsi/q_plist.h"
#include "dds/ddsi/q_globals.h"
#include "dds__init.h"
#include "dds__domain.h"
#include "dds__participant.h"
#include "dds__builtin.h"
#include "dds__qos.h"

DECL_ENTITY_LOCK_UNLOCK (extern inline, dds_participant)

#define DDS_PARTICIPANT_STATUS_MASK    (0u)

static dds_return_t dds_participant_status_validate (uint32_t mask)
{
  return (mask & ~DDS_PARTICIPANT_STATUS_MASK) ? DDS_RETCODE_BAD_PARAMETER : DDS_RETCODE_OK;
}

static dds_return_t dds_participant_delete (dds_entity *e) ddsrt_nonnull_all;

static dds_return_t dds_participant_delete (dds_entity *e)
{
  dds_return_t ret;
  assert (dds_entity_kind (e) == DDS_KIND_PARTICIPANT);

  thread_state_awake (lookup_thread_state (), &e->m_domain->gv);
  if ((ret = delete_participant (&e->m_domain->gv, &e->m_guid)) < 0)
    DDS_ERROR ("dds_participant_delete: internal error %"PRId32"\n", ret);
  ddsrt_mutex_lock (&dds_global.m_mutex);
  ddsrt_avl_delete (&dds_entity_children_td, &e->m_domain->m_ppants, e);
  ddsrt_mutex_unlock (&dds_global.m_mutex);
  thread_state_asleep (lookup_thread_state ());

  /* Every dds_init needs a dds_fini. */
  dds_domain_free (e->m_domain);
  dds_fini ();
  return DDS_RETCODE_OK;
}

static dds_return_t dds_participant_qos_set (dds_entity *e, const dds_qos_t *qos, bool enabled)
{
  /* note: e->m_qos is still the old one to allow for failure here */
  if (enabled)
  {
    struct participant *pp;
    thread_state_awake (lookup_thread_state (), &e->m_domain->gv);
    if ((pp = ephash_lookup_participant_guid (e->m_domain->gv.guid_hash, &e->m_guid)) != NULL)
    {
      nn_plist_t plist;
      nn_plist_init_empty (&plist);
      plist.qos.present = plist.qos.aliased = qos->present;
      plist.qos = *qos;
      update_participant_plist (pp, &plist);
    }
    thread_state_asleep (lookup_thread_state ());
  }
  return DDS_RETCODE_OK;
}

const struct dds_entity_deriver dds_entity_deriver_participant = {
  .close = dds_entity_deriver_dummy_close,
  .delete = dds_participant_delete,
  .set_qos = dds_participant_qos_set,
  .validate_status = dds_participant_status_validate
};

dds_entity_t dds_create_participant (const dds_domainid_t domain, const dds_qos_t *qos, const dds_listener_t *listener)
{
  dds_domain *dom;
  dds_entity_t ret;
  nn_guid_t guid;
  dds_participant * pp;
  nn_plist_t plist;
  dds_qos_t *new_qos = NULL;

  /* Make sure DDS instance is initialized. */
  if ((ret = dds_init ()) < 0)
    goto err_dds_init;

  if ((ret = dds_domain_create (&dom, domain)) < 0)
    goto err_domain_create;

  new_qos = dds_create_qos ();
  if (qos != NULL)
    nn_xqos_mergein_missing (new_qos, qos, DDS_PARTICIPANT_QOS_MASK);
  nn_xqos_mergein_missing (new_qos, &dom->gv.default_plist_pp.qos, ~(uint64_t)0);
  if ((ret = nn_xqos_valid (new_qos)) < 0)
    goto err_qos_validation;

  /* Translate qos */
  nn_plist_init_empty (&plist);
  dds_merge_qos (&plist.qos, new_qos);

  thread_state_awake (lookup_thread_state (), &dom->gv);
  ret = new_participant (&guid, &dom->gv, 0, &plist);
  thread_state_asleep (lookup_thread_state ());
  nn_plist_fini (&plist);
  if (ret < 0)
  {
    ret = DDS_RETCODE_ERROR;
    goto err_new_participant;
  }

  pp = dds_alloc (sizeof (*pp));
  if ((ret = dds_entity_init (&pp->m_entity, NULL, DDS_KIND_PARTICIPANT, new_qos, listener, DDS_PARTICIPANT_STATUS_MASK)) < 0)
    goto err_entity_init;

  pp->m_entity.m_guid = guid;
  pp->m_entity.m_iid = get_entity_instance_id (&dom->gv, &guid);
  pp->m_entity.m_domain = dom;
  pp->m_builtin_subscriber = 0;

  /* Add participant to extent */
  ddsrt_mutex_lock (&dds_global.m_mutex);
  ddsrt_avl_insert (&dds_entity_children_td, &dom->m_ppants, &pp->m_entity);
  ddsrt_mutex_unlock (&dds_global.m_mutex);
  return ret;

err_entity_init:
  dds_free (pp);
err_new_participant:
err_qos_validation:
  dds_delete_qos (new_qos);
  dds_domain_free (dom);
err_domain_create:
  dds_fini ();
err_dds_init:
  return ret;
}

dds_entity_t dds_lookup_participant (dds_domainid_t domain_id, dds_entity_t *participants, size_t size)
{
  if ((participants != NULL && (size <= 0 || size >= INT32_MAX)) || (participants == NULL && size != 0))
    return DDS_RETCODE_BAD_PARAMETER;

  ddsrt_init ();
  ddsrt_mutex_t * const init_mutex = ddsrt_get_singleton_mutex ();

  if (participants)
    participants[0] = 0;

  dds_return_t ret = 0;
  ddsrt_mutex_lock (init_mutex);
  if (dds_global.m_init_count > 0)
  {
    struct dds_domain *dom;
    ddsrt_mutex_lock (&dds_global.m_mutex);
    if ((dom = dds_domain_find_locked (domain_id)) != NULL)
    {
      ddsrt_avl_iter_t it;
      for (dds_entity *e = ddsrt_avl_iter_first (&dds_entity_children_td, &dom->m_ppants, &it); e != NULL; e = ddsrt_avl_iter_next (&it))
      {
        if ((size_t) ret < size)
          participants[ret] = e->m_hdllink.hdl;
        ret++;
      }
    }
    ddsrt_mutex_unlock (&dds_global.m_mutex);
  }
  ddsrt_mutex_unlock (init_mutex);
  ddsrt_fini ();
  return ret;
}
