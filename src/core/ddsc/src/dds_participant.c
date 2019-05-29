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
#include "dds__init.h"
#include "dds__qos.h"
#include "dds__domain.h"
#include "dds__participant.h"
#include "dds__builtin.h"

DECL_ENTITY_LOCK_UNLOCK (extern inline, dds_participant)

#define DDS_PARTICIPANT_STATUS_MASK    (0u)

/* List of created participants */
static dds_entity *dds_pp_head = NULL;

static dds_return_t dds_participant_status_validate (uint32_t mask)
{
  return (mask & ~DDS_PARTICIPANT_STATUS_MASK) ? DDS_RETCODE_BAD_PARAMETER : DDS_RETCODE_OK;
}

static dds_return_t dds_participant_delete (dds_entity *e) ddsrt_nonnull_all;

static dds_return_t dds_participant_delete (dds_entity *e)
{
  assert (dds_entity_kind (e) == DDS_KIND_PARTICIPANT);

  thread_state_awake (lookup_thread_state ());
  dds_domain_free (e->m_domain);
  ddsrt_mutex_lock (&dds_global.m_mutex);
  dds_entity *prev, *iter;
  for (iter = dds_pp_head, prev = NULL; iter; prev = iter, iter = iter->m_next)
  {
    if (iter == e)
      break;
  }
  assert (iter);
  if (prev)
    prev->m_next = iter->m_next;
  else
    dds_pp_head = iter->m_next;
  ddsrt_mutex_unlock (&dds_global.m_mutex);
  thread_state_asleep (lookup_thread_state ());

  /* Every dds_init needs a dds_fini. */
  dds_fini ();
  return DDS_RETCODE_OK;
}

static dds_return_t dds_participant_instance_hdl (dds_entity *e, dds_instance_handle_t *i) ddsrt_nonnull_all;

static dds_return_t dds_participant_instance_hdl (dds_entity *e, dds_instance_handle_t *i)
{
  *i = participant_instance_id (&e->m_guid);
  return DDS_RETCODE_OK;
}

static dds_return_t dds_participant_qos_validate (const dds_qos_t *qos, bool enabled) ddsrt_nonnull_all;

static dds_return_t dds_participant_qos_validate (const dds_qos_t *qos, bool enabled)
{
  dds_return_t ret;
  (void)enabled;
  if ((ret = nn_xqos_valid (qos)) < 0)
    return ret;
  return DDS_RETCODE_OK;
}

static dds_return_t dds_participant_qos_set (dds_entity *e, const dds_qos_t *qos, bool enabled) ddsrt_nonnull_all;

static dds_return_t dds_participant_qos_set (dds_entity *e, const dds_qos_t *qos, bool enabled)
{
  dds_return_t ret;
  (void)e;
  if ((ret = dds_participant_qos_validate (qos, enabled)) != DDS_RETCODE_OK)
    return ret;
  if (enabled) /* FIXME: changing QoS */
    return DDS_RETCODE_UNSUPPORTED;
  return ret;
}

dds_entity_t dds_create_participant (const dds_domainid_t domain, const dds_qos_t *qos, const dds_listener_t *listener)
{
  dds_entity_t ret;
  nn_guid_t guid;
  dds_participant * pp;
  nn_plist_t plist;
  dds_qos_t *new_qos = NULL;

  /* Make sure DDS instance is initialized. */
  if ((ret = dds_init (domain)) != DDS_RETCODE_OK)
    goto err_dds_init;

  /* Check domain id */
  if ((ret = dds__check_domain (domain)) != DDS_RETCODE_OK)
    goto err_domain_check;


#define DDS_QOSMASK_PARTICIPANT (QP_USER_DATA | QP_PRISMTECH_ENTITY_FACTORY | QP_CYCLONE_IGNORELOCAL)
  new_qos = dds_create_qos ();
  if (qos != NULL)
    nn_xqos_mergein_missing (new_qos, qos, DDS_QOSMASK_PARTICIPANT);
  /* Validate qos or use default if NULL */
  if ((ret = dds_participant_qos_validate (new_qos, false)) != DDS_RETCODE_OK)
    goto err_qos_validation;

  /* Translate qos */
  nn_plist_init_empty (&plist);
  dds_merge_qos (&plist.qos, new_qos);

  thread_state_awake (lookup_thread_state ());
  ret = new_participant (&guid, 0, &plist);
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
  pp->m_entity.m_domain = dds_domain_create (dds_domain_default ());
  pp->m_entity.m_domainid = dds_domain_default ();
  pp->m_entity.m_deriver.delete = dds_participant_delete;
  pp->m_entity.m_deriver.set_qos = dds_participant_qos_set;
  pp->m_entity.m_deriver.get_instance_hdl = dds_participant_instance_hdl;
  pp->m_entity.m_deriver.validate_status = dds_participant_status_validate;
  pp->m_builtin_subscriber = 0;

  /* Add participant to extent */
  ddsrt_mutex_lock (&dds_global.m_mutex);
  pp->m_entity.m_next = dds_pp_head;
  dds_pp_head = &pp->m_entity;
  ddsrt_mutex_unlock (&dds_global.m_mutex);
  return ret;

err_entity_init:
  dds_free (pp);
err_new_participant:
err_qos_validation:
  dds_delete_qos (new_qos);
err_domain_check:
  dds_fini ();
err_dds_init:
  return ret;
}

dds_entity_t dds_lookup_participant (dds_domainid_t domain_id, dds_entity_t *participants, size_t size)
{
  ddsrt_mutex_t *init_mutex;

  ddsrt_init ();
  init_mutex = ddsrt_get_singleton_mutex ();

  if ((participants != NULL && (size <= 0 || size >= INT32_MAX)) || (participants == NULL && size != 0))
  {
    ddsrt_fini ();
    return DDS_RETCODE_BAD_PARAMETER;
  }

  if (participants)
    participants[0] = 0;

  dds_return_t ret = 0;
  ddsrt_mutex_lock (init_mutex);
  if (dds_global.m_init_count > 0)
  {
    ddsrt_mutex_lock (&dds_global.m_mutex);
    for (dds_entity *iter = dds_pp_head; iter; iter = iter->m_next)
    {
      if (iter->m_domainid == domain_id)
      {
        if ((size_t) ret < size)
          participants[ret] = iter->m_hdllink.hdl;
        ret++;
      }
    }
    ddsrt_mutex_unlock (&dds_global.m_mutex);
  }
  ddsrt_mutex_unlock (init_mutex);
  ddsrt_fini ();
  return ret;
}
