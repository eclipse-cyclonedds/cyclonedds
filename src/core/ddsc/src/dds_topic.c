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
#include <ctype.h>

#include "dds/ddsrt/misc.h"
#include "dds/ddsrt/atomics.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/hopscotch.h"
#include "dds__topic.h"
#include "dds__listener.h"
#include "dds__participant.h"
#include "dds__init.h"
#include "dds__domain.h"
#include "dds__get_status.h"
#include "dds__qos.h"
#include "dds__builtin.h"
#include "dds/ddsi/ddsi_entity.h"
#include "dds/ddsi/ddsi_endpoint.h"
#include "dds/ddsi/ddsi_entity_index.h"
#include "dds/ddsi/ddsi_thread.h"
#include "dds/ddsi/ddsi_sertype.h"
#include "dds/ddsi/ddsi_iid.h"
#include "dds/ddsi/ddsi_plist.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsi/ddsi_security_omg.h"
#include "dds/ddsi/ddsi_typebuilder.h"
#include "dds/ddsc/dds_internal_api.h"
#include "dds/cdr/dds_cdrstream.h"
#include "dds__serdata_builtintopic.h"
#include "dds__serdata_default.h"
#include "dds__psmx.h"

DECL_ENTITY_LOCK_UNLOCK (dds_topic)

#define DDS_TOPIC_STATUS_MASK                                    \
                        (DDS_INCONSISTENT_TOPIC_STATUS)

struct topic_sertype_node {
  ddsrt_avl_node_t avlnode;
  uint32_t refc;
  const struct ddsi_sertype *st;
};

static bool is_valid_name (const char *name) ddsrt_nonnull_all;

static bool is_valid_name (const char *name)
{
  // DDS Spec does not explicitly specify what constitutes a valid name.
  // Per https://github.com/eclipse-cyclonedds/cyclonedds/pull/1426
  //  Require isprint is true and not <space>*?[]"' for the time being, then work our way to supporting UTF-8

  const char* invalid = "*?[]\"'#$";

  if (name[0] == '\0')
    return false;

  for (size_t i = 0; name[i]; i++)
    if (
        (!(isprint((unsigned char) name[i])))
        || (isspace((unsigned char) name[i]))
        || (strchr(invalid, name[i]) != NULL)
      )
      return false;
  return true;
}

static dds_return_t dds_topic_status_validate (uint32_t mask)
{
  return (mask & ~DDS_TOPIC_STATUS_MASK) ? DDS_RETCODE_BAD_PARAMETER : DDS_RETCODE_OK;
}

#ifdef DDS_HAS_TOPIC_DISCOVERY
static struct ktopic_type_guid * topic_guid_map_refc_impl (const struct dds_ktopic * ktp, const struct ddsi_sertype *sertype, bool unref)
{
  struct ktopic_type_guid *m = NULL;
  ddsi_typeid_t *type_id = ddsi_sertype_typeid (sertype, DDSI_TYPEID_KIND_COMPLETE);
  if (ddsi_typeid_is_none (type_id))
    goto no_typeid;
  struct ktopic_type_guid templ = { .type_id = type_id };
  m = ddsrt_hh_lookup (ktp->topic_guid_map, &templ);
  assert (m != NULL);
  if (unref)
    m->refc--;
  else
    m->refc++;
no_typeid:
  if (type_id != NULL)
  {
    ddsi_typeid_fini (type_id);
    ddsrt_free (type_id);
  }
  return m;
}

#if 0 // not currently needed, symmetry arguments suggest it should exist but the compiler disagrees
static void topic_guid_map_ref (const struct dds_ktopic * ktp, const struct ddsi_sertype *sertype)
{
  (void) topic_guid_map_refc_impl (ktp, sertype, false);
}
#endif

static void topic_guid_map_unref (struct ddsi_domaingv * const gv, const struct dds_ktopic * ktp, const struct ddsi_sertype *sertype)
{
  struct ktopic_type_guid *m = topic_guid_map_refc_impl (ktp, sertype, true);
  if (m == NULL)
    return;

  if (m->refc == 0)
  {
    ddsrt_hh_remove_present (ktp->topic_guid_map, m);
    ddsi_thread_state_awake (ddsi_lookup_thread_state (), gv);
    (void) ddsi_delete_topic (gv, &m->guid);
    ddsi_thread_state_asleep (ddsi_lookup_thread_state ());
    ddsi_typeid_fini (m->type_id);
    ddsrt_free (m->type_id);
    dds_free (m);
  }
}

#endif /* DDS_HAS_TOPIC_DISCOVERY */

/*
  Topic status change callback handler. Supports INCONSISTENT_TOPIC
  status (only defined status on a topic).  Irrelevant until inconsistent topic
  definitions can be detected, so until topic discovery is added.
*/
#if 0
static void dds_topic_status_cb (struct dds_topic *tp)
{
  struct dds_listener const * const lst = &tp->m_entity.m_listener;

  ddsrt_mutex_lock (&tp->m_entity.m_observers_lock);
  while (tp->m_entity.m_cb_count > 0)
    ddsrt_cond_wait (&tp->m_entity.m_observers_cond, &tp->m_entity.m_observers_lock);
  tp->m_entity.m_cb_count++;

  tp->m_inconsistent_topic_status.total_count++;
  tp->m_inconsistent_topic_status.total_count_change++;
  if (lst->on_inconsistent_topic)
  {
    ddsrt_mutex_unlock (&tp->m_entity.m_observers_lock);
    dds_entity_invoke_listener (&tp->m_entity, DDS_INCONSISTENT_TOPIC_STATUS_ID, &tp->m_inconsistent_topic_status);
    ddsrt_mutex_lock (&tp->m_entity.m_observers_lock);
    tp->m_inconsistent_topic_status.total_count_change = 0;
  }

  dds_entity_status_set (&tp->m_entity, DDS_INCONSISTENT_TOPIC_STATUS);
  tp->m_entity.m_cb_count--;
  ddsrt_cond_broadcast (&tp->m_entity.m_observers_cond);
  ddsrt_mutex_unlock (&tp->m_entity.m_observers_lock);
}
#endif

dds_return_t dds_topic_pin_with_origin (dds_entity_t handle, bool from_user, struct dds_topic **tp)
{
  struct dds_entity *e;
  dds_return_t ret;
  if ((ret = dds_entity_pin_with_origin (handle, from_user, &e)) < 0)
    return ret;
  if (dds_entity_kind (e) != DDS_KIND_TOPIC)
  {
    dds_entity_unpin (e);
    return DDS_RETCODE_ILLEGAL_OPERATION;
  }
  *tp = (struct dds_topic *) e;
  return DDS_RETCODE_OK;
}

dds_return_t dds_topic_pin (dds_entity_t handle, struct dds_topic **tp)
{
  return dds_topic_pin_with_origin (handle, true, tp);
}

void dds_topic_unpin (struct dds_topic *tp)
{
  dds_entity_unpin (&tp->m_entity);
}

void dds_topic_defer_set_qos (struct dds_topic *tp)
{
  struct dds_ktopic * const ktp = tp->m_ktopic;
  struct dds_participant * const pp = dds_entity_participant (&tp->m_entity);
  ddsrt_mutex_lock (&pp->m_entity.m_mutex);
  ++ktp->defer_set_qos;
  ddsrt_mutex_unlock (&pp->m_entity.m_mutex);
}

void dds_topic_allow_set_qos (struct dds_topic *tp)
{
  struct dds_ktopic * const ktp = tp->m_ktopic;
  struct dds_participant * const pp = dds_entity_participant (&tp->m_entity);
  ddsrt_mutex_lock (&pp->m_entity.m_mutex);
  assert (ktp->defer_set_qos > 0);
  if (--ktp->defer_set_qos == 0)
    ddsrt_cond_broadcast (&pp->m_entity.m_cond);
  ddsrt_mutex_unlock (&pp->m_entity.m_mutex);
}

static void ktopic_unref (dds_participant * const pp, struct dds_ktopic * const ktp)
{
  if (--ktp->refc != 0)
    return;

  for (uint32_t i = 0; i < ktp->psmx_topics.length; i++)
  {
    ktp->psmx_topics.topics[i]->psmx_instance->ops.delete_topic (ktp->psmx_topics.topics[i]);
    ktp->psmx_topics.topics[i] = NULL;
  }

  ddsrt_avl_delete (&participant_ktopics_treedef, &pp->m_ktopics, ktp);
  dds_delete_qos (ktp->qos);
  dds_free (ktp->name);
#ifdef DDS_HAS_TOPIC_DISCOVERY
  ddsrt_hh_free (ktp->topic_guid_map);
#endif
  dds_free (ktp);
}

static void dds_topic_close (dds_entity *e) ddsrt_nonnull_all;

static void dds_topic_close (dds_entity *e)
{
  struct dds_topic * const tp = (dds_topic *) e;
  struct dds_ktopic * const ktp = tp->m_ktopic;
  assert (dds_entity_kind (e->m_parent) == DDS_KIND_PARTICIPANT);
  dds_participant * const pp = (dds_participant *) e->m_parent;
#ifdef DDS_HAS_TYPELIB
  ddsi_type_unref_sertype (&e->m_domain->gv, tp->m_stype);
#endif
  dds_free (tp->m_name);

  ddsrt_mutex_lock (&pp->m_entity.m_mutex);

#ifdef DDS_HAS_TOPIC_DISCOVERY
  topic_guid_map_unref (&e->m_domain->gv, ktp, tp->m_stype);
#endif
  ktopic_unref (pp, ktp);
  ddsrt_mutex_unlock (&pp->m_entity.m_mutex);
  ddsi_sertype_unref (tp->m_stype);
}

static dds_return_t dds_topic_qos_set (dds_entity *e, const dds_qos_t *qos, bool enabled)
{
  /* note: e->m_qos is still the old one to allow for failure here */
#ifdef DDS_HAS_TOPIC_DISCOVERY
  if (enabled)
  {
    struct dds_topic *tp = (struct dds_topic *) e;
    struct dds_ktopic * const ktp = tp->m_ktopic;
    ddsi_thread_state_awake (ddsi_lookup_thread_state (), &e->m_domain->gv);
    struct ddsrt_hh_iter it;
    /* parent pp is locked and protects ktp->topic_guid_map */
    for (struct ktopic_type_guid *obj = ddsrt_hh_iter_first(ktp->topic_guid_map, &it); obj; obj = ddsrt_hh_iter_next(&it))
    {
      struct ddsi_topic *ddsi_tp;
      if ((ddsi_tp = ddsi_entidx_lookup_topic_guid (e->m_domain->gv.entity_index, &obj->guid)) != NULL)
        ddsi_update_topic_qos (ddsi_tp, qos);
    }
    ddsi_thread_state_asleep (ddsi_lookup_thread_state ());
  }
#else
  (void) e; (void) qos; (void) enabled;
#endif
  return DDS_RETCODE_OK;
}

static bool dupdef_qos_ok (const dds_qos_t *qos, const dds_ktopic *ktp)
{
  if ((qos == NULL) != (ktp->qos == NULL))
    return false;
  else if (qos == NULL)
    return true;
  else
    return dds_qos_equal (ktp->qos, qos);
}

const struct dds_entity_deriver dds_entity_deriver_topic = {
  .interrupt = dds_entity_deriver_dummy_interrupt,
  .close = dds_topic_close,
  .delete = dds_entity_deriver_dummy_delete,
  .set_qos = dds_topic_qos_set,
  .validate_status = dds_topic_status_validate,
  .create_statistics = dds_entity_deriver_dummy_create_statistics,
  .refresh_statistics = dds_entity_deriver_dummy_refresh_statistics,
  .invoke_cbs_for_pending_events = dds_entity_deriver_dummy_invoke_cbs_for_pending_events
};

/**
* @brief Checks whether a ktopic with the same name exists in the participant,
* and if so, whether it's QoS matches or not.
*
* The set of ktopics is stored in the participant, protected by the participant's
* mutex and the internal state of these ktopics (including the QoS) is also
* protected by that mutex.
*
* @param[out] ktp_out    matching ktopic if call was successful, or NULL if no
*                        ktopic with this name exists
* @param[in]  pp         pinned & locked participant
* @param[in]  name       topic name to look for
* @param[in]  new_qos    QoS for the new topic (can be NULL)
*
* @returns success + ktopic, success + NULL or error.
*
* @retval DDS_RETCODE_OK
*             ktp_out is either NULL (first attempt at creating this topic), or
*             the matching ktopic entity
* @retval DDS_RETCODE_INCONSISTENT_POLICY
*             a ktopic exists with differing QoS
* @retval DDS_RETCODE_PRECONDITION_NOT_MET
*             a ktopic exists with a different type name
*/
static dds_return_t lookup_and_check_ktopic (struct dds_ktopic **ktp_out, dds_participant *pp, const char *name, const dds_qos_t *new_qos)
{
  struct ddsi_domaingv * const gv = &pp->m_entity.m_domain->gv;
  struct dds_ktopic *ktp;
  if ((ktp = *ktp_out = ddsrt_avl_lookup (&participant_ktopics_treedef, &pp->m_ktopics, name)) == NULL)
  {
    GVTRACE ("lookup_and_check_ktopic_may_unlock_pp: no such ktopic\n");
    return DDS_RETCODE_OK;
  }
  else if (!dupdef_qos_ok (new_qos, ktp))
  {
    GVTRACE ("lookup_and_check_ktopic_may_unlock_pp: ktp %p qos mismatch\n", (void *) ktp);
    return DDS_RETCODE_INCONSISTENT_POLICY;
  }
  else
  {
    GVTRACE ("lookup_and_check_ktopic_may_unlock_pp: ktp %p reuse\n", (void *) ktp);
    return DDS_RETCODE_OK;
  }
}

static dds_entity_t create_topic_pp_locked (struct dds_participant *pp, struct dds_ktopic *ktp, bool builtin, const char *topic_name, struct ddsi_sertype *sertype, const dds_listener_t *listener)
{
  dds_entity_t hdl;
  dds_topic *tp = dds_alloc (sizeof (*tp));
  /* builtin topics are created implicitly (and so destroyed on last reference) and may not be deleted by the application */
  hdl = dds_entity_init (&tp->m_entity, &pp->m_entity, DDS_KIND_TOPIC, builtin, !builtin, NULL, listener, DDS_TOPIC_STATUS_MASK);
  tp->m_entity.m_iid = ddsi_iid_gen ();
  dds_entity_register_child (&pp->m_entity, &tp->m_entity);
  tp->m_ktopic = ktp;
  tp->m_name = dds_string_dup (topic_name);
  tp->m_stype = sertype;
  dds_entity_init_complete (&tp->m_entity);
  return hdl;
}

#ifdef DDS_HAS_TOPIC_DISCOVERY

static bool register_topic_type_for_discovery (struct ddsi_domaingv * const gv, dds_participant * const pp, dds_ktopic * const ktp, bool is_builtin, struct ddsi_sertype * const sertype)
{
  bool new_topic_def = false;

  /* Create or reference a ktopic-sertype meta-data entry. The hash table has the
     complete xtypes type-id as key; for both local and discovered topic with type information,
     both minimal and complete type identifiers are always set */
  ddsi_typeid_t *type_id = ddsi_sertype_typeid (sertype, DDSI_TYPEID_KIND_COMPLETE);
  if (ddsi_typeid_is_none (type_id))
    goto free_typeid;

  struct ktopic_type_guid templ = { .type_id = type_id }, *m;
  if ((m = ddsrt_hh_lookup (ktp->topic_guid_map, &templ)))
  {
    m->refc++;
    goto free_typeid;
  }
  else
  {
    /* Add a ktopic-type-guid entry with the complete type identifier of the sertype as
        key and a reference to a newly create ddsi topic entity */
    ddsi_thread_state_awake (ddsi_lookup_thread_state (), gv);
    const struct ddsi_guid * pp_guid = dds_entity_participant_guid (&pp->m_entity);
    struct ddsi_participant * pp_ddsi = ddsi_entidx_lookup_participant_guid (gv->entity_index, pp_guid);

    m = dds_alloc (sizeof (*m));
    m->type_id = type_id;
    type_id = NULL; /* the ktopic_type_guid gets the ownership for the type_id */
    m->refc = 1;
    dds_return_t rc = ddsi_new_topic (&m->tp, &m->guid, pp_ddsi, ktp->name, sertype, ktp->qos, is_builtin, &new_topic_def);
    assert (rc == DDS_RETCODE_OK); /* FIXME: can be out-of-resources at the very least */
    (void) rc;
    ddsrt_hh_add_absent (ktp->topic_guid_map, m);
    ddsi_thread_state_asleep (ddsi_lookup_thread_state ());
  }
free_typeid:
  if (type_id != NULL)
  {
    ddsi_typeid_fini (type_id);
    ddsrt_free (type_id);
  }
  return new_topic_def;
}

static bool ktopic_type_guid_equal (const void *ktp_guid_a, const void *ktp_guid_b)
{
  struct ktopic_type_guid *a = (struct ktopic_type_guid *) ktp_guid_a;
  struct ktopic_type_guid *b = (struct ktopic_type_guid *) ktp_guid_b;
  return ddsi_typeid_compare (a->type_id, b->type_id) == 0;
}

static uint32_t ktopic_type_guid_hash (const void *ktp_guid)
{
  uint32_t hash32;
  struct ktopic_type_guid *x = (struct ktopic_type_guid *) ktp_guid;
  DDS_XTypes_EquivalenceHash hash;
  ddsi_typeid_get_equivalence_hash (x->type_id, &hash);
  memcpy (&hash32, hash, sizeof (hash32));
  return hash32;
}

#else

static bool register_topic_type_for_discovery (struct ddsi_domaingv * const gv, dds_participant * const pp, dds_ktopic * const ktp, bool is_builtin, struct ddsi_sertype * const sertype)
{
  (void) gv; (void) pp; (void) ktp; (void) is_builtin; (void) sertype;
  return false;
}

#endif /* DDS_HAS_TOPIC_DISCOVERY */

static dds_return_t create_psmx_topics (dds_participant *pp, struct dds_ktopic *ktp, struct ddsi_sertype *sertype_registered, struct ddsi_type *type)
{
  dds_domain *dom = pp->m_entity.m_domain;
  for (uint32_t i = 0; i < dom->psmx_instances.length; i++)
  {
    struct dds_psmx_int * const psmx = dom->psmx_instances.elems[i].instance;
    if (!psmx->ops.type_qos_supported (psmx->ext, DDS_PSMX_ENDPOINT_TYPE_UNSET, sertype_registered->data_type_props, ktp->qos))
      continue;
    struct dds_psmx_topic_int * const psmx_topic = psmx->ops.create_topic_with_type (psmx, ktp, sertype_registered, type);
    if (psmx_topic == NULL)
      goto psmx_fail;
    ktp->psmx_topics.topics[ktp->psmx_topics.length++] = psmx_topic;
  }
  return DDS_RETCODE_OK;

psmx_fail:
  for (uint32_t i = 0; i < ktp->psmx_topics.length; i++)
  {
    ktp->psmx_topics.topics[i]->psmx_instance->ops.delete_topic (ktp->psmx_topics.topics[i]);
    ktp->psmx_topics.topics[i] = NULL;
  }
  return DDS_RETCODE_ERROR;
}

dds_entity_t dds_create_topic_impl (
    dds_entity_t participant,
    const char * name,
    bool allow_dcps,
    struct ddsi_sertype **sertype,
    const dds_qos_t *qos,
    const dds_listener_t *listener,
    bool is_builtin)
{
  dds_return_t rc = DDS_RETCODE_OK;
  dds_participant *pp;
  dds_qos_t *new_qos = NULL;
  dds_entity_t hdl;
  struct ddsi_sertype *sertype_registered;

  if (sertype == NULL || *sertype == NULL || name == NULL || !is_valid_name (name))
    return DDS_RETCODE_BAD_PARAMETER;
  if (!allow_dcps && strncmp (name, "DCPS", 4) == 0)
    return DDS_RETCODE_BAD_PARAMETER;

  {
    dds_entity *par_ent;
    if ((rc = dds_entity_pin (participant, &par_ent)) < 0)
      return rc;
    if (dds_entity_kind (par_ent) != DDS_KIND_PARTICIPANT)
    {
      dds_entity_unpin (par_ent);
      return DDS_RETCODE_ILLEGAL_OPERATION;
    }
    pp = (struct dds_participant *) par_ent;
  }

#ifdef DDS_HAS_TYPELIB
  /* ensure that in case type information is present, both minimal and complete
     type identifiers are present for the top-level type */
  ddsi_typeinfo_t *type_info = ddsi_sertype_typeinfo (*sertype);
  if (type_info != NULL)
  {
    if (ddsi_typeid_is_none (ddsi_typeinfo_minimal_typeid (type_info)) || ddsi_typeid_is_none (ddsi_typeinfo_complete_typeid (type_info)))
      rc = DDS_RETCODE_BAD_PARAMETER;
    ddsi_typeinfo_fini (type_info);
    ddsrt_free (type_info);
    if (rc < 0)
    {
      dds_entity_unpin (&pp->m_entity);
      return rc;
    }
  }
#endif

  new_qos = dds_create_qos ();
  if (qos)
    ddsi_xqos_mergein_missing (new_qos, qos, DDS_TOPIC_QOS_MASK);
  /* One would expect this:
   *
   *   ddsi_xqos_mergein_missing (new_qos, &gv.default_xqos_tp, ~(uint64_t)0);
   *
   * but the crazy defaults of the DDS specification has a default setting
   * for reliability that is dependent on the entity type: readers and
   * topics default to best-effort, but writers to reliable.
   *
   * Similar for DATA_REPRESENTATION.
   *
   * Leaving the topic QoS sparse means a default-default topic QoS of
   * best-effort will do "the right thing" and let a writer still default to
   * reliable ... (and keep behaviour unchanged) */

  if ((rc = dds_ensure_valid_data_representation (new_qos, (*sertype)->allowed_data_representation, (*sertype)->data_type_props, DDS_KIND_TOPIC)) != DDS_RETCODE_OK)
    goto error;

  struct ddsi_domaingv * const gv = &pp->m_entity.m_domain->gv;
  if ((rc = ddsi_xqos_valid (&gv->logconfig, new_qos)) != DDS_RETCODE_OK)
    goto error;

  if (!ddsi_omg_security_check_create_topic (&pp->m_entity.m_domain->gv, &pp->m_entity.m_guid, name, new_qos))
  {
    rc = DDS_RETCODE_NOT_ALLOWED_BY_SECURITY;
    goto error;
  }

  /* See if we're allowed to create the topic; ktp is returned pinned & locked (protected by pp's lock)
     so we can be sure it doesn't disappear and its QoS can't change */
  GVTRACE ("dds_create_topic_impl (pp %p "PGUIDFMT" sertype %p reg?%s refc %"PRIu32" %s/%s)\n",
           (void *) pp, PGUID (pp->m_entity.m_guid), (void *) (*sertype),
           (ddsrt_atomic_ld32 (&(*sertype)->flags_refc) & DDSI_SERTYPE_REGISTERED) ? "yes" : "no",
           ddsrt_atomic_ld32 (&(*sertype)->flags_refc) & DDSI_SERTYPE_REFC_MASK,
           name, (*sertype)->type_name);
  ddsrt_mutex_lock (&pp->m_entity.m_mutex);
  struct dds_ktopic *ktp;
  if ((rc = lookup_and_check_ktopic (&ktp, pp, name, new_qos)) != DDS_RETCODE_OK)
  {
    GVTRACE ("dds_create_topic_impl: failed after compatibility check: %s\n", dds_strretcode (rc));
    ddsrt_mutex_unlock (&pp->m_entity.m_mutex);
    goto error;
  }

  /* Create a ktopic if it doesn't exist yet, else reference existing one and delete the
     unneeded "new_qos". */
  bool new_ktopic = false;
  if (ktp == NULL)
  {
    new_ktopic = true;
    ktp = dds_alloc (sizeof (*ktp));
    ktp->refc = 1;
    ktp->defer_set_qos = 0;
    ktp->qos = new_qos;
    ktp->name = dds_string_dup (name);
#ifdef DDS_HAS_TOPIC_DISCOVERY
    ktp->topic_guid_map = ddsrt_hh_new (1, ktopic_type_guid_hash, ktopic_type_guid_equal);
#endif
    ddsrt_avl_insert (&participant_ktopics_treedef, &pp->m_ktopics, ktp);
    GVTRACE ("create_and_lock_ktopic: ktp %p\n", (void *) ktp);
  }
  else
  {
    ktp->refc++;
    dds_delete_qos (new_qos);
  }
  new_qos = NULL;

  /* sertype: re-use a previously registered one if possible, else register this one */
  {
    ddsrt_mutex_lock (&gv->sertypes_lock);
    if ((sertype_registered = ddsi_sertype_lookup_locked (gv, *sertype)) != NULL)
      GVTRACE ("dds_create_topic_impl: reuse sertype %p\n", (void *) sertype_registered);
    else
    {
      GVTRACE ("dds_create_topic_impl: register new sertype %p\n", (void *) (*sertype));
      ddsi_sertype_register_locked (gv, *sertype);
      sertype_registered = *sertype;
    }
    ddsrt_mutex_unlock (&gv->sertypes_lock);
  }

  // mintype: needs to be unref'd if ddsi_type_ref_local fails for complete type
  // compltype: used for PSMX
  struct ddsi_type *mintype = NULL, *compltype = NULL;
#ifdef DDS_HAS_TYPELIB
  if (ddsi_type_ref_local (gv, &mintype, sertype_registered, DDSI_TYPEID_KIND_MINIMAL) != DDS_RETCODE_OK)
  {
    GVTRACE ("dds_create_topic_impl: invalid type (minimal type)\n");
    rc = DDS_RETCODE_BAD_PARAMETER;
    goto err_type_ref_local_mintype;
  }
  if (ddsi_type_ref_local (gv, &compltype, sertype_registered, DDSI_TYPEID_KIND_COMPLETE) != DDS_RETCODE_OK)
  {
    GVTRACE ("dds_create_topic_impl: invalid type (complete type)\n");
    rc = DDS_RETCODE_BAD_PARAMETER;
    goto err_type_ref_local_compltype;
  }
#endif

  // Concurrent QoS change is not possible: topic QoS changes lock pp->m_entity.m_mutex
  if (new_ktopic && (rc = create_psmx_topics (pp, ktp, sertype_registered, compltype)) != DDS_RETCODE_OK)
    goto err_create_psmx_topics;

  /* Create topic referencing ktopic & sertype_registered */
  hdl = create_topic_pp_locked (pp, ktp, (sertype_registered->ops == &ddsi_sertype_ops_builtintopic), name, sertype_registered, listener);
  ddsi_sertype_unref (*sertype);
  *sertype = sertype_registered;

  const bool new_topic_def = register_topic_type_for_discovery (gv, pp, ktp, is_builtin, sertype_registered);
  ddsrt_mutex_unlock (&pp->m_entity.m_mutex);

  if (new_topic_def)
  {
    ddsrt_mutex_lock (&gv->new_topic_lock);
    gv->new_topic_version++;
    ddsrt_cond_broadcast (&gv->new_topic_cond);
    ddsrt_mutex_unlock (&gv->new_topic_lock);
  }

  dds_entity_unpin (&pp->m_entity);
  GVTRACE ("dds_create_topic_impl: new topic %"PRId32"\n", hdl);
  return hdl;

err_create_psmx_topics:
#ifdef DDS_HAS_TYPELIB
  ddsi_type_unref (gv, compltype); // null pointer allowed
err_type_ref_local_compltype:
  ddsi_type_unref (gv, mintype); // null pointer allowed
err_type_ref_local_mintype:
#endif
  ddsi_sertype_unref (*sertype);
  ktopic_unref (pp, ktp);
  ddsrt_mutex_unlock (&pp->m_entity.m_mutex);
  GVTRACE ("dds_create_topic_impl: invalid type\n");

error:
  if (new_qos)
    dds_delete_qos (new_qos);
  dds_entity_unpin (&pp->m_entity);
  return rc;
}

dds_entity_t dds_create_topic_sertype (dds_entity_t participant, const char *name, struct ddsi_sertype **sertype, const dds_qos_t *qos, const dds_listener_t *listener, const struct ddsi_plist *sedp_plist)
{
  (void) sedp_plist;
  return dds_create_topic_impl (participant, name, false, sertype, qos, listener, false);
}

dds_entity_t dds_create_topic (dds_entity_t participant, const dds_topic_descriptor_t *descriptor, const char *name, const dds_qos_t *qos, const dds_listener_t *listener)
{
  struct dds_entity *ppent;
  dds_return_t ret;

  if (descriptor == NULL || name == NULL)
    return DDS_RETCODE_BAD_PARAMETER;

  if ((ret = dds_entity_pin (participant, &ppent)) < 0)
    return ret;

  dds_qos_t *tpqos = dds_create_qos ();
  if (qos)
    ddsi_xqos_mergein_missing (tpqos, qos, DDS_TOPIC_QOS_MASK);

  /* Check the data representation in the provided QoS for compatiblity with the extensibility
     of the types used in this topic. In case any functionality is used that we don't support in
     XCDR1 (extensibility mutable and appendable, optional members), the XCDR2 data representation
     is required and the only valid value for this QoS. If the data representation is not set in
     the QoS (or no QoS object provided), the allowed data representations are added to the
     QoS object. */
  uint32_t allowed_repr = descriptor->m_flagset & DDS_TOPIC_RESTRICT_DATA_REPRESENTATION ?
      descriptor->restrict_data_representation : DDS_DATA_REPRESENTATION_RESTRICT_DEFAULT;
  uint16_t min_xcdrv = dds_stream_minimum_xcdr_version (descriptor->m_ops);
  if (min_xcdrv == DDSI_RTPS_CDR_ENC_VERSION_2)
    allowed_repr &= ~DDS_DATA_REPRESENTATION_FLAG_XCDR1;
  if ((ret = dds_ensure_valid_data_representation (tpqos, allowed_repr, dds_stream_data_types (descriptor->m_ops), DDS_KIND_TOPIC)) != DDS_RETCODE_OK)
    goto err_data_repr;

  assert (tpqos->present & DDSI_QP_DATA_REPRESENTATION && tpqos->data_representation.value.n > 0);
  dds_data_representation_id_t data_representation = tpqos->data_representation.value.ids[0];

  struct dds_sertype_default *st = ddsrt_malloc (sizeof (*st));
  if ((ret = dds_sertype_default_init (ppent->m_domain, st, descriptor, min_xcdrv, data_representation)) < 0)
  {
    ddsrt_free (st);
    goto err_st_init;
  }

  struct ddsi_sertype *st_tmp = &st->c;
  dds_entity_t hdl = dds_create_topic_impl (participant, name, false, &st_tmp, tpqos, listener, false);
  if (hdl < 0)
    ddsi_sertype_unref (st_tmp);
  ret = hdl;

err_data_repr:
err_st_init:
  dds_delete_qos (tpqos);
  dds_entity_unpin (ppent);
  return ret;
}

static dds_topic *pin_if_matching_topic (dds_entity * const e_pp_child, const char *name, const ddsi_typeinfo_t *type_info)
{
  // e_pp_child can't disappear while we hold pp->m_entity.m_mutex and so we
  // can skip non-topics without first trying to pin it. That makes doing it
  // this way cheaper than dds_topic_pin.
  if (dds_entity_kind (e_pp_child) != DDS_KIND_TOPIC)
    return NULL;

  // pin to make sure that child is not closed (i.e., still accessible)
  struct dds_entity *x;
  if (dds_entity_pin (e_pp_child->m_hdllink.hdl, &x) != DDS_RETCODE_OK)
    return NULL;

  struct dds_topic * const tp = (struct dds_topic *) e_pp_child;
  if (!strcmp (tp->m_ktopic->name, name))
  {
#ifdef DDS_HAS_TYPE_DISCOVERY
    /* In case no type info is provided, returns any (the first) topic with the specified
       name. If type info is set, it should match the topic's type info should match */
    ddsi_typeinfo_t *topic_type_info = ddsi_sertype_typeinfo (tp->m_stype);
    bool ti_match = !ddsi_typeinfo_present (type_info) || (ddsi_typeinfo_present (topic_type_info) && ddsi_typeinfo_equal (topic_type_info, type_info, DDSI_TYPE_IGNORE_DEPS));
    ddsi_typeinfo_fini (topic_type_info);
    ddsrt_free (topic_type_info);
    if (ti_match)
      return tp;
#else
    (void) type_info;
    return tp;
#endif
  }

  // unpin if non-matching topic
  dds_entity_unpin (x);
  return NULL;
}

static dds_entity_t find_local_topic_pp (dds_participant *pp, const char *name, const ddsi_typeinfo_t *type_info, dds_participant *pp_topic)
{
  // On entry:
  // - pp and pp_topic are pinned, no locks held
  // - pp and pp_topic may but need not be the same
  // - pp_topic may contain a matching ktopic (regardless of whether pp & pp_topic are the same)
  // - concurrent create, delete, set_qos of topics and ktopics may be happening in parallel
  //
  // dds_create_topic does "the right thing" once you have a sertype and a qos, so if we can find
  // matching topic in pp, we clone its qos (to protect against concurrent calls to set_qos) and
  // increment the refcount of its sertype.  Following that, we can call dds_create_topic_sertype
  // at our leisure.
  struct dds_topic *tp = NULL;
  ddsrt_avl_iter_t it;

  ddsrt_mutex_lock (&pp->m_entity.m_mutex);
  for (dds_entity *e_pp_child = ddsrt_avl_iter_first (&dds_entity_children_td, &pp->m_entity.m_children, &it); e_pp_child != NULL; e_pp_child = ddsrt_avl_iter_next (&it))
  {
    // pinning the topic serves a dual purpose: checking that its handle hasn't been closed
    // and keeping it alive after we unlock the participant.
    tp = pin_if_matching_topic (e_pp_child, name, type_info);
    if (tp != NULL)
      break;
  }

  if (tp == NULL)
  {
    ddsrt_mutex_unlock (&pp->m_entity.m_mutex);
    return DDS_RETCODE_OK;
  }
  else
  {
    // QoS changes are protected by the participant lock (see dds_set_qos_locked_impl)
    // which we currently hold for the topic's participant
    dds_qos_t *qos = ddsi_xqos_dup (tp->m_ktopic->qos);
    ddsrt_mutex_unlock (&pp->m_entity.m_mutex);

    // Having pinned tp, its sertype will remain registered.  dds_create_topic_sertype
    // takes ownership of the reference it is passed and returns an (uncounted) pointer
    // to what it actually uses.  Thus, if we increment the refcount of tp->m_stype, we
    // can pass it into dds_create_topic_sertype and be sure that it uses it, and only
    // in case it fails do we need to drop the reference.  (It can fail, e.g., when the
    // application concurrently calls dds_delete on pp_topic.)
    struct ddsi_sertype *sertype = ddsi_sertype_ref (tp->m_stype);
    const dds_entity_t hdl = dds_create_topic_sertype (pp_topic->m_entity.m_hdllink.hdl, name, &sertype, qos, NULL, NULL);
    if (hdl < 0)
      ddsi_sertype_unref (sertype);
    dds_delete_qos (qos);

#ifndef NDEBUG
    if (hdl > 0)
    {
      dds_topic *new_topic;
      // concurrently calling dds_delete on random handles might cause pinning the newly
      // created topic to fail
      if (dds_topic_pin (hdl, &new_topic) == DDS_RETCODE_OK)
      {
        if (pp == pp_topic)
          assert (tp->m_ktopic == new_topic->m_ktopic);
        else
          assert (tp->m_ktopic != new_topic->m_ktopic);
        assert (tp->m_stype == new_topic->m_stype);
        dds_topic_unpin (new_topic);
      }
    }

    // must be before unpinning tp or (1) sertype may have been dropped from the table if
    // dds_create_topic_sertype failed, and (2) tp may have been deleted by the time we
    // get here and so it would no longer necessarily be accounted for in the refcount of
    // the sertype.
    {
      struct ddsi_domaingv * const gv = &pp_topic->m_entity.m_domain->gv;
      ddsrt_mutex_lock (&gv->sertypes_lock);
      assert (ddsrt_hh_lookup (gv->sertypes, sertype) == sertype);
      ddsrt_mutex_unlock (&gv->sertypes_lock);
      const uint32_t sertype_flags_refc = ddsrt_atomic_ld32 (&sertype->flags_refc);
      assert (sertype_flags_refc & DDSI_SERTYPE_REGISTERED);
      assert ((sertype_flags_refc & DDSI_SERTYPE_REFC_MASK) >= (hdl < 0 ? 1u : 2u));
    }
#endif

    dds_topic_unpin (tp);
    return hdl;
  }
}

static dds_return_t find_local_topic_impl (dds_find_scope_t scope, dds_participant *pp_topic, const char *name, const ddsi_typeinfo_t *type_info)
{
  // On entry: pp_topic is pinned, no locks held

  dds_entity *e_pp, *e_domain_child;
  dds_instance_handle_t last_iid = 0;

  if (scope == DDS_FIND_SCOPE_PARTICIPANT)
    return find_local_topic_pp (pp_topic, name, type_info, pp_topic);
  dds_domain *dom = pp_topic->m_entity.m_domain;
  ddsrt_mutex_lock (&dom->m_entity.m_mutex);
  while ((e_domain_child = ddsrt_avl_lookup_succ (&dds_entity_children_td, &dom->m_entity.m_children, &last_iid)) != NULL)
  {
    last_iid = e_domain_child->m_iid;
    if (dds_entity_kind (e_domain_child) != DDS_KIND_PARTICIPANT)
      continue;

    if (dds_entity_pin (e_domain_child->m_hdllink.hdl, &e_pp) != DDS_RETCODE_OK)
      continue;

    dds_participant *pp = (dds_participant *) e_domain_child;
    ddsrt_mutex_unlock (&dom->m_entity.m_mutex);
    dds_entity_t hdl = find_local_topic_pp (pp, name, type_info, pp_topic);
    dds_entity_unpin (e_pp);
    if (hdl != 0)
      return hdl;
    ddsrt_mutex_lock (&dom->m_entity.m_mutex);
  }
  ddsrt_mutex_unlock (&dom->m_entity.m_mutex);
  return DDS_RETCODE_OK;
}


#ifdef DDS_HAS_TOPIC_DISCOVERY

static dds_entity_t find_remote_topic_impl (dds_participant *pp_topic, const char *name, const dds_typeinfo_t *type_info, dds_duration_t timeout)
{
  // On entry: pp_topic is pinned, no locks held

  dds_entity_t ret;
  struct ddsi_topic_definition *tpd;
  struct ddsi_domaingv * gv = &pp_topic->m_entity.m_domain->gv;
  const struct ddsi_typeid *type_id = ddsi_typeinfo_complete_typeid (type_info);
  struct ddsi_type *resolved_type = NULL;

  if ((ret = ddsi_lookup_topic_definition (gv, name, type_id, &tpd)) != DDS_RETCODE_OK)
    return ret;
  if (tpd == NULL)
    return DDS_RETCODE_OK;

  if ((ret = ddsi_wait_for_type_resolved (gv, type_id, timeout, &resolved_type, DDSI_TYPE_INCLUDE_DEPS, DDSI_TYPE_SEND_REQUEST)) != DDS_RETCODE_OK)
    return ret;
  assert (!ddsi_type_compare (tpd->type_pair->complete, resolved_type));
  assert (ddsi_type_resolved (gv, tpd->type_pair->complete, DDSI_TYPE_INCLUDE_DEPS));

  dds_topic_descriptor_t *desc = ddsrt_malloc (sizeof (*desc));
  if ((ret = ddsi_topic_descriptor_from_type (gv, desc, tpd->type_pair->complete)))
    goto err_desc;
  ret = dds_create_topic (pp_topic->m_entity.m_hdllink.hdl, desc, name, tpd->xqos, NULL);
  ddsi_topic_descriptor_fini (desc);
  if (resolved_type)
    ddsi_type_unref (gv, resolved_type);
err_desc:
  ddsrt_free (desc);
  return ret;
}

#endif /* DDS_HAS_TOPIC_DISCOVERY */

static dds_entity_t dds_find_topic_impl (dds_find_scope_t scope, dds_entity_t participant, const char *name, const dds_typeinfo_t *type_info, dds_duration_t timeout)
{
  dds_entity_t hdl;
  dds_return_t ret;
  dds_entity *e;

#ifndef DDS_HAS_TOPIC_DISCOVERY
  if (scope == DDS_FIND_SCOPE_GLOBAL)
    return DDS_RETCODE_BAD_PARAMETER;
#endif

  if (name == NULL || !is_valid_name (name))
    return DDS_RETCODE_BAD_PARAMETER;
  if ((ret = dds_entity_pin (participant, &e)) < 0)
    return ret;
  if (e->m_kind != DDS_KIND_PARTICIPANT)
  {
    dds_entity_unpin (e);
    return DDS_RETCODE_BAD_PARAMETER;
  }
  dds_participant *pp_topic = (dds_participant *) e;
  struct ddsi_domaingv * gv = &e->m_domain->gv;
  const dds_time_t tnow = dds_time ();
  const dds_time_t abstimeout = (DDS_INFINITY - timeout <= tnow) ? DDS_NEVER : (tnow + timeout);
  do
  {
    ddsrt_mutex_lock (&gv->new_topic_lock);
    uint32_t tv = gv->new_topic_version;
    ddsrt_mutex_unlock (&gv->new_topic_lock);
    if ((hdl = find_local_topic_impl (scope, pp_topic, name, type_info)) == DDS_RETCODE_OK && scope == DDS_FIND_SCOPE_GLOBAL)
    {
#ifdef DDS_HAS_TOPIC_DISCOVERY
      hdl = find_remote_topic_impl (pp_topic, name, type_info, timeout);
#endif
    }
    if (hdl == 0 && timeout > 0)
    {
      ddsrt_mutex_lock (&gv->new_topic_lock);
      while (hdl != DDS_RETCODE_TIMEOUT && gv->new_topic_version == tv)
      {
        if (!ddsrt_cond_waituntil (&gv->new_topic_cond, &gv->new_topic_lock, abstimeout))
          hdl = DDS_RETCODE_TIMEOUT;
      }
      ddsrt_mutex_unlock (&gv->new_topic_lock);
    }
  } while (hdl == 0 && timeout > 0);
  dds_entity_unpin (e);
  return hdl;
}

dds_entity_t dds_find_topic (dds_find_scope_t scope, dds_entity_t participant, const char *name, const dds_typeinfo_t *type_info, dds_duration_t timeout)
{
#ifdef DDS_HAS_TOPIC_DISCOVERY
  if (type_info && !ddsi_typeinfo_valid (type_info))
    return DDS_RETCODE_BAD_PARAMETER;
#else
  if (type_info != NULL)
    return DDS_RETCODE_BAD_PARAMETER;
#endif
  return dds_find_topic_impl (scope, participant, name, type_info, timeout);
}

dds_entity_t dds_find_topic_scoped (dds_find_scope_t scope, dds_entity_t participant, const char *name, dds_duration_t timeout)
{
  return dds_find_topic_impl (scope, participant, name, NULL, timeout);
}

dds_return_t dds_set_topic_filter_extended (dds_entity_t topic, const struct dds_topic_filter *filter)
{
  struct dds_topic_filter f;
  dds_topic *t;
  dds_return_t rc;

  if (filter == NULL)
    return DDS_RETCODE_BAD_PARAMETER;

  f = *filter;

  {
    bool valid = false;
    switch (f.mode)
    {
      case DDS_TOPIC_FILTER_NONE:
        // treat function and argument as don't cares on input if mode = NONE, but
        // do make them null pointers in the internal representation
        f.f.sample = NULL;
        f.arg = NULL;
        valid = true;
        break;
      case DDS_TOPIC_FILTER_SAMPLE:
        f.arg = NULL;
        /* falls through */
      case DDS_TOPIC_FILTER_SAMPLEINFO_ARG:
      case DDS_TOPIC_FILTER_SAMPLE_ARG:
      case DDS_TOPIC_FILTER_SAMPLE_SAMPLEINFO_ARG:
        // can safely use any of the function pointers
        valid = (filter->f.sample != NULL);
        break;
    }
    if (!valid)
    {
      // only possible if the caller passed garbage in the mode argument
      return DDS_RETCODE_BAD_PARAMETER;
    }
  }

  if ((rc = dds_topic_lock (topic, &t)) != DDS_RETCODE_OK)
    return rc;
  t->m_filter = f;
  dds_topic_unlock (t);
  return DDS_RETCODE_OK;
}

dds_return_t dds_set_topic_filter_and_arg (dds_entity_t topic, dds_topic_filter_arg_fn filter, void *arg)
{
  struct dds_topic_filter f = {
    .mode = filter ? DDS_TOPIC_FILTER_SAMPLE_ARG : DDS_TOPIC_FILTER_NONE,
    .arg = arg,
    .f = { .sample_arg = filter }
  };
  return dds_set_topic_filter_extended (topic, &f);
}

dds_return_t dds_get_topic_filter_extended (dds_entity_t topic, struct dds_topic_filter *filter)
{
  dds_return_t rc;
  dds_topic *t;
  if (filter == NULL)
    return DDS_RETCODE_BAD_PARAMETER;
  if ((rc = dds_topic_lock (topic, &t)) != DDS_RETCODE_OK)
    return rc;
  *filter = t->m_filter;
  dds_topic_unlock (t);
  return rc;
}

dds_return_t dds_get_topic_filter_and_arg (dds_entity_t topic, dds_topic_filter_arg_fn *fn, void **arg)
{
  struct dds_topic_filter f;
  dds_return_t rc;
  if ((rc = dds_get_topic_filter_extended (topic, &f)) != DDS_RETCODE_OK)
    return rc;
  switch (f.mode)
  {
    case DDS_TOPIC_FILTER_NONE:
      assert (f.f.sample_arg == 0);
      /* fall through */
    case DDS_TOPIC_FILTER_SAMPLE_ARG:
      if (fn)
        *fn = f.f.sample_arg;
      if (arg)
        *arg = f.arg;
      break;
    case DDS_TOPIC_FILTER_SAMPLE:
    case DDS_TOPIC_FILTER_SAMPLEINFO_ARG:
    case DDS_TOPIC_FILTER_SAMPLE_SAMPLEINFO_ARG:
      rc = DDS_RETCODE_PRECONDITION_NOT_MET;
      break;
  }
  return rc;
}

dds_return_t dds_get_name (dds_entity_t topic, char *name, size_t size)
{
  dds_topic *t;
  dds_return_t ret;
  if (size <= 0 || name == NULL)
    return DDS_RETCODE_BAD_PARAMETER;
  name[0] = '\0';

  const char *bname;
  if (dds__get_builtin_topic_name_typename (topic, &bname, NULL) == DDS_RETCODE_OK)
    ret = (dds_return_t) ddsrt_strlcpy (name, bname, size);
  else if ((ret = dds_topic_pin (topic, &t)) == DDS_RETCODE_OK)
  {
    ret = (dds_return_t) ddsrt_strlcpy (name, t->m_name, size);
    dds_topic_unpin (t);
  }
  return ret;
}

dds_return_t dds_get_type_name (dds_entity_t topic, char *name, size_t size)
{
  dds_topic *t;
  dds_return_t ret;
  if (size <= 0 || name == NULL)
    return DDS_RETCODE_BAD_PARAMETER;
  name[0] = '\0';

  const char *bname;
  if (dds__get_builtin_topic_name_typename (topic, NULL, &bname) == DDS_RETCODE_OK)
    ret = (dds_return_t) ddsrt_strlcpy (name, bname, size);
  else if ((ret = dds_topic_pin (topic, &t)) == DDS_RETCODE_OK)
  {
    ret = (dds_return_t) ddsrt_strlcpy (name, t->m_stype->type_name, size);
    dds_topic_unpin (t);
  }
  return ret;
}

DDS_GET_STATUS(topic, inconsistent_topic, INCONSISTENT_TOPIC, total_count_change)

#ifdef DDS_HAS_TYPELIB

dds_return_t dds_create_topic_descriptor (dds_find_scope_t scope, dds_entity_t participant, const dds_typeinfo_t *type_info, dds_duration_t timeout, dds_topic_descriptor_t **descriptor)
{
  dds_return_t ret;

#ifdef DDS_HAS_TYPE_DISCOVERY
  if (scope != DDS_FIND_SCOPE_GLOBAL && scope != DDS_FIND_SCOPE_LOCAL_DOMAIN)
    return DDS_RETCODE_BAD_PARAMETER;
#else
  if (scope != DDS_FIND_SCOPE_LOCAL_DOMAIN)
    return DDS_RETCODE_BAD_PARAMETER;
#endif
  if (type_info == NULL || descriptor == NULL)
    return DDS_RETCODE_BAD_PARAMETER;

  *descriptor = dds_alloc (sizeof (**descriptor));
  if (*descriptor == NULL)
    return DDS_RETCODE_OUT_OF_RESOURCES;

  dds_entity *e;
  if ((ret = dds_entity_pin (participant, &e)) < 0)
    goto err_pin;
  if (e->m_kind != DDS_KIND_PARTICIPANT)
  {
    ret = DDS_RETCODE_BAD_PARAMETER;
    goto err;
  }

  struct ddsi_domaingv * gv = &e->m_domain->gv;
  struct ddsi_type *type;
  if ((ret = ddsi_wait_for_type_resolved (gv, ddsi_typeinfo_complete_typeid (type_info), timeout, &type, DDSI_TYPE_INCLUDE_DEPS, scope == DDS_FIND_SCOPE_GLOBAL ? DDSI_TYPE_SEND_REQUEST : DDSI_TYPE_NO_REQUEST)))
    goto err;
  assert (type && ddsi_type_resolved (gv, type, DDSI_TYPE_INCLUDE_DEPS));
  ret = ddsi_topic_descriptor_from_type (gv, *descriptor, type);
  ddsi_type_unref (gv, type);

err:
  dds_entity_unpin (e);
err_pin:
  if (ret != DDS_RETCODE_OK)
    ddsrt_free (*descriptor);
  return ret;
}

dds_return_t dds_delete_topic_descriptor (dds_topic_descriptor_t *descriptor)
{
  if (!descriptor)
    return DDS_RETCODE_BAD_PARAMETER;
  ddsi_topic_descriptor_fini (descriptor);
  dds_free (descriptor);
  return DDS_RETCODE_OK;
}

#else /* DDS_HAS_TYPELIB */

dds_return_t dds_create_topic_descriptor (dds_find_scope_t scope, dds_entity_t participant, const dds_typeinfo_t *type_info, dds_duration_t timeout, dds_topic_descriptor_t **descriptor)
{
  (void) scope; (void) participant; (void) type_info; (void) timeout; (void) descriptor;
  return DDS_RETCODE_UNSUPPORTED;
}

dds_return_t dds_delete_topic_descriptor (dds_topic_descriptor_t *descriptor)
{
  (void) descriptor;
  return DDS_RETCODE_UNSUPPORTED;
}

#endif /* DDS_HAS_TYPELIB */

void dds_cdrstream_desc_from_topic_desc (struct dds_cdrstream_desc *desc, const dds_topic_descriptor_t *topic_desc)
{
  memset (desc, 0, sizeof (*desc));
  dds_cdrstream_desc_init_with_nops (desc, &dds_cdrstream_default_allocator, topic_desc->m_size, topic_desc->m_align, topic_desc->m_flagset,
      topic_desc->m_ops, topic_desc->m_nops, topic_desc->m_keys, topic_desc->m_nkeys);
}
