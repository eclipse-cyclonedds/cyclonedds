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

#include "dds/ddsrt/cdtors.h"
#include "dds/ddsrt/environ.h"
#include "dds/ddsi/ddsi_entity.h"
#include "dds/ddsi/ddsi_participant.h"
#include "dds/ddsi/ddsi_proxy_endpoint.h"
#include "dds/ddsi/ddsi_thread.h"
#include "dds/ddsi/ddsi_plist.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsi/ddsi_entity_index.h"
#include "dds/version.h"
#include "dds__init.h"
#include "dds__domain.h"
#include "dds__participant.h"
#include "dds__builtin.h"
#include "dds__qos.h"

DECL_ENTITY_LOCK_UNLOCK (dds_participant)

#define DDS_PARTICIPANT_STATUS_MASK    (0u)

static int cmp_ktopic_name (const void *a, const void *b)
{
  return strcmp (a, b);
}

const ddsrt_avl_treedef_t participant_ktopics_treedef = DDSRT_AVL_TREEDEF_INITIALIZER_INDKEY(offsetof (struct dds_ktopic, pp_ktopics_avlnode), offsetof (struct dds_ktopic, name), cmp_ktopic_name, 0);

static dds_return_t dds_participant_status_validate (uint32_t mask)
{
  return (mask & ~DDS_PARTICIPANT_STATUS_MASK) ? DDS_RETCODE_BAD_PARAMETER : DDS_RETCODE_OK;
}

static dds_return_t dds_participant_delete (dds_entity *e) ddsrt_nonnull_all;

static dds_return_t dds_participant_delete (dds_entity *e)
{
  dds_return_t ret;
  assert (dds_entity_kind (e) == DDS_KIND_PARTICIPANT);

  /* ktopics & topics are children and therefore must all have been deleted by the time we get here */
  assert (ddsrt_avl_is_empty (&((struct dds_participant *) e)->m_ktopics));

  ddsi_thread_state_awake (ddsi_lookup_thread_state (), &e->m_domain->gv);
  if ((ret = ddsi_delete_participant (&e->m_domain->gv, &e->m_guid)) < 0)
    DDS_CERROR (&e->m_domain->gv.logconfig, "dds_participant_delete: internal error %"PRId32"\n", ret);
  ddsi_thread_state_asleep (ddsi_lookup_thread_state ());
  return DDS_RETCODE_OK;
}

static dds_return_t dds_participant_qos_set (dds_entity *e, const dds_qos_t *qos, bool enabled)
{
  /* note: e->m_qos is still the old one to allow for failure here */
  if (enabled)
  {
    struct ddsi_participant *pp;
    ddsi_thread_state_awake (ddsi_lookup_thread_state (), &e->m_domain->gv);
    if ((pp = ddsi_entidx_lookup_participant_guid (e->m_domain->gv.entity_index, &e->m_guid)) != NULL)
    {
      ddsi_plist_t plist;
      ddsi_plist_init_empty (&plist);
      plist.qos.present = plist.qos.aliased = qos->present;
      plist.qos = *qos;
      ddsi_update_participant_plist (pp, &plist);
    }
    ddsi_thread_state_asleep (ddsi_lookup_thread_state ());
  }
  return DDS_RETCODE_OK;
}

const struct dds_entity_deriver dds_entity_deriver_participant = {
  .interrupt = dds_entity_deriver_dummy_interrupt,
  .close = dds_entity_deriver_dummy_close,
  .delete = dds_participant_delete,
  .set_qos = dds_participant_qos_set,
  .validate_status = dds_participant_status_validate,
  .create_statistics = dds_entity_deriver_dummy_create_statistics,
  .refresh_statistics = dds_entity_deriver_dummy_refresh_statistics
};

dds_entity_t dds_create_participant (const dds_domainid_t domain, const dds_qos_t *qos, const dds_listener_t *listener)
{
  dds_domain *dom;
  dds_entity_t ret;
  ddsi_guid_t guid;
  dds_participant * pp;
  ddsi_plist_t plist;
  dds_qos_t *new_qos = NULL;
  const char *config = "";

  /* Make sure DDS instance is initialized. */
  if ((ret = dds_init ()) < 0)
    goto err_dds_init;

  (void) ddsrt_getenv ("CYCLONEDDS_URI", &config);

  if ((ret = dds_domain_create_internal (&dom, domain, true, config)) < 0)
    goto err_domain_create;

  new_qos = dds_create_qos ();
  if (qos != NULL)
    ddsi_xqos_mergein_missing (new_qos, qos, DDS_PARTICIPANT_QOS_MASK);
  ddsi_xqos_mergein_missing (new_qos, &dom->gv.default_local_xqos_pp, ~(uint64_t)0);
  dds_apply_entity_naming(new_qos, NULL, &dom->gv);

  if ((ret = ddsi_xqos_valid (&dom->gv.logconfig, new_qos)) < 0)
    goto err_qos_validation;
  // generic validation code will check lease duration, we only need to check kind
  // is what we insist on
  if (new_qos->liveliness.kind != DDS_LIVELINESS_AUTOMATIC)
  {
    ret = DDS_RETCODE_BAD_PARAMETER;
    goto err_qos_validation;
  }

  // DDSI layer wants a plist that it will copy, DDS layer takes ownership of QoS object
  // passed into entity_init.  That we have to copy the QoS into the plist here
  ddsi_plist_init_empty (&plist);
  ddsi_xqos_mergein_missing (&plist.qos, new_qos, ~(uint64_t)0);

  ddsi_thread_state_awake (ddsi_lookup_thread_state (), &dom->gv);
  ret = ddsi_new_participant (&guid, &dom->gv, 0, &plist);
  ddsi_thread_state_asleep (ddsi_lookup_thread_state ());
  ddsi_plist_fini (&plist);
  if (ret < 0)
  {
    ret = DDS_RETCODE_ERROR;
    goto err_new_participant;
  }

  pp = dds_alloc (sizeof (*pp));
  if ((ret = dds_entity_init (&pp->m_entity, &dom->m_entity, DDS_KIND_PARTICIPANT, false, true, new_qos, listener, DDS_PARTICIPANT_STATUS_MASK)) < 0)
    goto err_entity_init;

  pp->m_entity.m_guid = guid;
  pp->m_entity.m_iid = ddsi_get_entity_instanceid (&dom->gv, &guid);
  pp->m_entity.m_domain = dom;
  pp->m_builtin_subscriber = 0;
  ddsrt_avl_init (&participant_ktopics_treedef, &pp->m_ktopics);

  /* Add participant to extent */
  ddsrt_mutex_lock (&dom->m_entity.m_mutex);
  dds_entity_register_child (&dom->m_entity, &pp->m_entity);
  ddsrt_mutex_unlock (&dom->m_entity.m_mutex);

  dds_entity_init_complete (&pp->m_entity);
  /* drop temporary extra ref to domain, dds_init */
  dds_entity_unpin_and_drop_ref (&dom->m_entity);
  dds_entity_unpin_and_drop_ref (&dds_global.m_entity);
  return ret;

err_entity_init:
  dds_free (pp);
err_new_participant:
err_qos_validation:
  dds_delete_qos (new_qos);
  dds_entity_unpin_and_drop_ref (&dom->m_entity);
err_domain_create:
  dds_entity_unpin_and_drop_ref (&dds_global.m_entity);
err_dds_init:
  return ret;
}

dds_return_t dds_lookup_participant (dds_domainid_t domain_id, dds_entity_t *participants, size_t size)
{
  dds_return_t ret;

  if ((participants != NULL && (size == 0 || size >= INT32_MAX)) || (participants == NULL && size != 0))
    return DDS_RETCODE_BAD_PARAMETER;

  if (participants)
    participants[0] = 0;

  if ((ret = dds_init ()) < 0)
    return ret;

  ret = 0;
  struct dds_domain *dom;
  ddsrt_mutex_lock (&dds_global.m_mutex);
  if ((dom = dds_domain_find_locked (domain_id)) != NULL)
  {
    ddsrt_avl_iter_t it;
    for (dds_entity *e = ddsrt_avl_iter_first (&dds_entity_children_td, &dom->m_entity.m_children, &it); e != NULL; e = ddsrt_avl_iter_next (&it))
    {
      if ((size_t) ret < size)
        participants[ret] = e->m_hdllink.hdl;
      ret++;
    }
  }
  ddsrt_mutex_unlock (&dds_global.m_mutex);
  dds_entity_unpin_and_drop_ref (&dds_global.m_entity);
  return ret;
}
