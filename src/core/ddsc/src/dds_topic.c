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
#include <ctype.h>

#include "dds/ddsrt/misc.h"
#include "dds/ddsrt/atomics.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/string.h"
#include "dds__topic.h"
#include "dds__listener.h"
#include "dds__participant.h"
#include "dds__init.h"
#include "dds__domain.h"
#include "dds__get_status.h"
#include "dds__qos.h"
#include "dds/ddsi/q_entity.h"
#include "dds/ddsi/ddsi_entity_index.h"
#include "dds/ddsi/q_thread.h"
#include "dds/ddsi/ddsi_sertype.h"
#include "dds/ddsi/ddsi_sertopic.h"
#include "dds/ddsi/q_ddsi_discovery.h"
#include "dds/ddsi/ddsi_iid.h"
#include "dds/ddsi/ddsi_plist.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsi/ddsi_cdrstream.h"
#include "dds/ddsi/ddsi_security_omg.h"
#include "dds__serdata_builtintopic.h"

DECL_ENTITY_LOCK_UNLOCK (extern inline, dds_topic)

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
  /* DDS Spec:
   *  |  TOPICNAME - A topic name is an identifier for a topic, and is defined as any series of characters
   *  |     'a', ..., 'z',
   *  |     'A', ..., 'Z',
   *  |     '0', ..., '9',
   *  |     '-' but may not start with a digit.
   * It is considered that '-' is an error in the spec and should say '_'. So, that's what we'll check for.
   *  |     '/' got added for ROS2
   */
  if (name[0] == '\0' || isdigit ((unsigned char) name[0]))
    return false;
  for (size_t i = 0; name[i]; i++)
    if (!(isalnum ((unsigned char) name[i]) || name[i] == '_' || name[i] == '/'))
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
  type_identifier_t *tid = ddsi_typeid_from_sertype (sertype);
  if (ddsi_typeid_none (tid))
    return NULL;

  struct ktopic_type_guid templ;
  memset (&templ, 0, sizeof (templ));
  templ.type_id = tid;
  struct ktopic_type_guid *m = ddsrt_hh_lookup (ktp->topic_guid_map, &templ);
  ddsrt_free (templ.type_id);
  assert (m != NULL);
  if (unref)
    m->refc--;
  else
    m->refc++;
  return m;
}

static void topic_guid_map_ref (const struct dds_ktopic * ktp, const struct ddsi_sertype *sertype)
{
  (void) topic_guid_map_refc_impl (ktp, sertype, false);
}

static void topic_guid_map_unref (struct ddsi_domaingv * const gv, const struct dds_ktopic * ktp, const struct ddsi_sertype *sertype)
{
  struct ktopic_type_guid *m = topic_guid_map_refc_impl (ktp, sertype, true);
  if (m == NULL)
    return;

  if (m->refc == 0)
  {
    ddsrt_hh_remove (ktp->topic_guid_map, m);
    thread_state_awake (lookup_thread_state (), gv);
    (void) delete_topic (gv, &m->guid);
    thread_state_asleep (lookup_thread_state ());
    ddsrt_free ((type_identifier_t *) m->type_id);
    ddsrt_free (m);
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

dds_return_t dds_topic_pin (dds_entity_t handle, struct dds_topic **tp)
{
  struct dds_entity *e;
  dds_return_t ret;
  if ((ret = dds_entity_pin (handle, &e)) < 0)
    return ret;
  if (dds_entity_kind (e) != DDS_KIND_TOPIC)
  {
    dds_entity_unpin (e);
    return DDS_RETCODE_ILLEGAL_OPERATION;
  }
  *tp = (struct dds_topic *) e;
  return DDS_RETCODE_OK;
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

static void dds_topic_close (dds_entity *e) ddsrt_nonnull_all;

static void dds_topic_close (dds_entity *e)
{
  struct dds_topic * const tp = (dds_topic *) e;
  struct dds_ktopic * const ktp = tp->m_ktopic;
  assert (dds_entity_kind (e->m_parent) == DDS_KIND_PARTICIPANT);
  dds_participant * const pp = (dds_participant *) e->m_parent;
#ifdef DDS_HAS_TYPE_DISCOVERY
  ddsi_tl_meta_local_unref (&e->m_domain->gv, NULL, tp->m_stype);
#endif
  ddsrt_free (tp->m_name);

  ddsrt_mutex_lock (&pp->m_entity.m_mutex);

#ifdef DDS_HAS_TOPIC_DISCOVERY
  topic_guid_map_unref (&e->m_domain->gv, ktp, tp->m_stype);
#endif

  // unref ktopic and delete if last ref
  if (--ktp->refc == 0)
  {
    ddsrt_avl_delete (&participant_ktopics_treedef, &pp->m_ktopics, ktp);
    dds_delete_qos (ktp->qos);
    ddsrt_free (ktp->name);
    ddsrt_free (ktp->type_name);
#ifdef DDS_HAS_TOPIC_DISCOVERY
    ddsrt_hh_free (ktp->topic_guid_map);
#endif
    dds_free (ktp);
  }

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
    thread_state_awake (lookup_thread_state (), &e->m_domain->gv);
    struct ddsrt_hh_iter it;
    /* parent pp is locked and protects ktp->topic_guid_map */
    for (struct ktopic_type_guid *obj = ddsrt_hh_iter_first(ktp->topic_guid_map, &it); obj; obj = ddsrt_hh_iter_next(&it))
    {
      struct topic *ddsi_tp;
      if ((ddsi_tp = entidx_lookup_topic_guid (e->m_domain->gv.entity_index, &obj->guid)) != NULL)
        update_topic_qos (ddsi_tp, qos);
    }
    thread_state_asleep (lookup_thread_state ());
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
  .refresh_statistics = dds_entity_deriver_dummy_refresh_statistics
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
* @param[in]  type_name  type name the topic must have
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
static dds_return_t lookup_and_check_ktopic (struct dds_ktopic **ktp_out, dds_participant *pp, const char *name, const char *type_name, const dds_qos_t *new_qos)
{
  struct ddsi_domaingv * const gv = &pp->m_entity.m_domain->gv;
  struct dds_ktopic *ktp;
  if ((ktp = *ktp_out = ddsrt_avl_lookup (&participant_ktopics_treedef, &pp->m_ktopics, name)) == NULL)
  {
    GVTRACE ("lookup_and_check_ktopic_may_unlock_pp: no such ktopic\n");
    return DDS_RETCODE_OK;
  }
  else if (strcmp (ktp->type_name, type_name) != 0)
  {
    GVTRACE ("lookup_and_check_ktopic_may_unlock_pp: ktp %p typename %s mismatch\n", (void *) ktp, ktp->type_name);
    return DDS_RETCODE_PRECONDITION_NOT_MET;
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

static dds_entity_t create_topic_pp_locked (struct dds_participant *pp, struct dds_ktopic *ktp, bool implicit, const char *topic_name, struct ddsi_sertype *sertype_registered, const dds_listener_t *listener, const ddsi_plist_t *sedp_plist)
{
  (void) sedp_plist;
  dds_entity_t hdl;
  dds_topic *tp = dds_alloc (sizeof (*tp));
  hdl = dds_entity_init (&tp->m_entity, &pp->m_entity, DDS_KIND_TOPIC, implicit, NULL, listener, DDS_TOPIC_STATUS_MASK);
  tp->m_entity.m_iid = ddsi_iid_gen ();
  dds_entity_register_child (&pp->m_entity, &tp->m_entity);
  tp->m_ktopic = ktp;
  tp->m_name = ddsrt_strdup (topic_name);
  tp->m_stype = sertype_registered;
  dds_entity_init_complete (&tp->m_entity);
  return hdl;
}

#ifdef DDS_HAS_TOPIC_DISCOVERY

static int ktopic_type_guid_equal (const void *ktp_guid_a, const void *ktp_guid_b)
{
  struct ktopic_type_guid *a = (struct ktopic_type_guid *) ktp_guid_a;
  struct ktopic_type_guid *b = (struct ktopic_type_guid *) ktp_guid_b;
  return ddsi_typeid_equal (a->type_id, b->type_id);
}

static uint32_t ktopic_type_guid_hash (const void *ktp_guid)
{
  struct ktopic_type_guid *x = (struct ktopic_type_guid *)ktp_guid;
  return (uint32_t) *x->type_id->hash;
}

#endif /* DDS_HAS_TOPIC_DISCOVERY */

dds_entity_t dds_create_topic_impl (
    dds_entity_t participant,
    const char * name,
    bool allow_dcps,
    struct ddsi_sertype **sertype,
    const dds_qos_t *qos,
    const dds_listener_t *listener,
    const ddsi_plist_t *sedp_plist,
    bool is_builtin)
{
  dds_return_t rc;
  dds_participant *pp;
  dds_qos_t *new_qos = NULL;
  dds_entity_t hdl;
  struct ddsi_sertype *sertype_registered;
  bool new_topic_def = false;

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
   * Leaving the topic QoS sparse means a default-default topic QoS of
   * best-effort will do "the right thing" and let a writer still default to
   * reliable ... (and keep behaviour unchanged) */
  struct ddsi_domaingv * const gv = &pp->m_entity.m_domain->gv;
  if ((rc = ddsi_xqos_valid (&gv->logconfig, new_qos)) != DDS_RETCODE_OK)
    goto error;

  if (!q_omg_security_check_create_topic (&pp->m_entity.m_domain->gv, &pp->m_entity.m_guid, name, new_qos))
  {
    rc = DDS_RETCODE_NOT_ALLOWED_BY_SECURITY;
    goto error;
  }

  /* See if we're allowed to create the topic; ktp is returned pinned & locked (protected by pp's lock)
     so we can be sure it doesn't disappear and its QoS can't change */
  GVTRACE ("dds_create_topic_generic (pp %p "PGUIDFMT" sertype %p reg?%s refc %"PRIu32" %s/%s)\n",
           (void *) pp, PGUID (pp->m_entity.m_guid), (void *) (*sertype),
           (ddsrt_atomic_ld32 (&(*sertype)->flags_refc) & DDSI_SERTYPE_REGISTERED) ? "yes" : "no",
           ddsrt_atomic_ld32 (&(*sertype)->flags_refc) & DDSI_SERTYPE_REFC_MASK,
           name, (*sertype)->type_name);
  ddsrt_mutex_lock (&pp->m_entity.m_mutex);
  struct dds_ktopic *ktp;
  if ((rc = lookup_and_check_ktopic (&ktp, pp, name, (*sertype)->type_name, new_qos)) != DDS_RETCODE_OK)
  {
    GVTRACE ("dds_create_topic_generic: failed after compatibility check: %s\n", dds_strretcode (rc));
    ddsrt_mutex_unlock (&pp->m_entity.m_mutex);
    goto error;
  }

  /* Create a ktopic if it doesn't exist yet, else reference existing one and delete the
     unneeded "new_qos". */
  if (ktp == NULL)
  {
    ktp = dds_alloc (sizeof (*ktp));
    ktp->refc = 1;
    ktp->defer_set_qos = 0;
    ktp->qos = new_qos;
    ktp->name = ddsrt_strdup (name);
    /* have to copy these because the ktopic can outlast any specific sertype */
    ktp->type_name = ddsrt_strdup ((*sertype)->type_name);
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

  /* sertype: re-use a previously registered one if possible, else register this one */
  {
    ddsrt_mutex_lock (&gv->sertypes_lock);
    if ((sertype_registered = ddsi_sertype_lookup_locked (gv, *sertype)) != NULL)
      GVTRACE ("dds_create_topic_generic: reuse sertype %p\n", (void *) sertype_registered);
    else
    {
      GVTRACE ("dds_create_topic_generic: register new sertype %p\n", (void *) (*sertype));
      ddsi_sertype_register_locked (gv, *sertype);
      sertype_registered = *sertype;
    }
    ddsrt_mutex_unlock (&gv->sertypes_lock);
  }

  /* Create topic referencing ktopic & sertype_registered */
  /* FIXME: setting "implicit" based on sertype->ops is a hack */
  hdl = create_topic_pp_locked (pp, ktp, (sertype_registered->ops == &ddsi_sertype_ops_builtintopic), name, sertype_registered, listener, sedp_plist);
  ddsi_sertype_unref (*sertype);
  *sertype = sertype_registered;

#ifdef DDS_HAS_TOPIC_DISCOVERY
  /* create or reference ktopic-sertype meta-data entry */
  struct ktopic_type_guid templ, *m;
  type_identifier_t *tid = ddsi_typeid_from_sertype (sertype_registered);
  if (!ddsi_typeid_none (tid))
  {
    memset (&templ, 0, sizeof (templ));
    templ.type_id = tid;
    if ((m = ddsrt_hh_lookup (ktp->topic_guid_map, &templ)) == NULL)
    {
      /* create ddsi topic and new ktopic-guid entry */
      thread_state_awake (lookup_thread_state (), gv);
      const struct ddsi_guid * ppguid = dds_entity_participant_guid (&pp->m_entity);
      struct participant * pp_ddsi = entidx_lookup_participant_guid (gv->entity_index, ppguid);

      m = ddsrt_malloc (sizeof (*m));
      m->type_id = tid;
      m->refc = 1;
      rc = ddsi_new_topic (&m->tp, &m->guid, pp_ddsi, ktp->name, sertype_registered, ktp->qos, is_builtin, &new_topic_def);
      assert (rc == DDS_RETCODE_OK); /* FIXME: can be out-of-resources at the very least */
      ddsrt_hh_add (ktp->topic_guid_map, m);
      thread_state_asleep (lookup_thread_state ());
    }
    else
    {
      /* refc existing */
      m->refc++;
      ddsrt_free (tid);
    }
  }
#else
  DDSRT_UNUSED_ARG (is_builtin);
#endif

  ddsrt_mutex_unlock (&pp->m_entity.m_mutex);
#ifdef DDS_HAS_TYPE_DISCOVERY
  ddsi_tl_meta_local_ref (gv, NULL, sertype_registered);
  ddsi_tl_meta_register_with_proxy_endpoints (gv, sertype_registered);
#endif

  if (new_topic_def)
  {
    ddsrt_mutex_lock (&gv->new_topic_lock);
    gv->new_topic_version++;
    ddsrt_cond_broadcast (&gv->new_topic_cond);
    ddsrt_mutex_unlock (&gv->new_topic_lock);
  }

  dds_entity_unpin (&pp->m_entity);
  GVTRACE ("dds_create_topic_generic: new topic %"PRId32"\n", hdl);
  return hdl;

 error:
  dds_entity_unpin (&pp->m_entity);
  dds_delete_qos (new_qos);
  return rc;
}

dds_entity_t dds_create_topic_sertype (dds_entity_t participant, const char *name, struct ddsi_sertype **sertype, const dds_qos_t *qos, const dds_listener_t *listener, const ddsi_plist_t *sedp_plist)
{
  return dds_create_topic_impl (participant, name, false, sertype, qos, listener, sedp_plist, false);
}

dds_entity_t dds_create_topic_generic (dds_entity_t participant, struct ddsi_sertopic **sertopic, const dds_qos_t *qos, const dds_listener_t *listener, const ddsi_plist_t *sedp_plist)
{
  if (sertopic == NULL || *sertopic == NULL || (*sertopic)->name == NULL)
    return DDS_RETCODE_BAD_PARAMETER;

  dds_entity_t ret;
  struct ddsi_sertype *sertype = ddsi_sertype_from_sertopic (*sertopic);
  ret = dds_create_topic_impl (participant, (*sertopic)->name, false, &sertype, qos, listener, sedp_plist, false);
  if (ret < 0)
  {
    // sertype_from_sertopic incremented the refcount, so decrementing it on failure will
    // free sertype but leave *sertopic untouched
    ddsi_sertype_unref (sertype);
  }
  else
  {
    // sertype incremented the refcount, we still need to undo it
    ddsi_sertopic_unref (*sertopic);
    // we should never have a sertype that doesn't a sertopic if we started with a sertopic
    // because the vtables are necessarily different; it may but need not be the same as
    // *sertopic on input
    assert (sertype->wrapped_sertopic != NULL);
    *sertopic = sertype->wrapped_sertopic;
  }
  return ret;
}

dds_entity_t dds_create_topic_arbitrary (dds_entity_t participant, struct ddsi_sertopic *sertopic, const dds_qos_t *qos, const dds_listener_t *listener, const ddsi_plist_t *sedp_plist)
{
  if (sertopic == NULL)
    return DDS_RETCODE_BAD_PARAMETER;

  dds_entity_t ret;
  struct ddsi_sertopic *st = sertopic;
  ddsi_sertopic_ref (st);
  if ((ret = dds_create_topic_generic (participant, &st, qos, listener, sedp_plist)) < 0)
    ddsi_sertopic_unref (st);
  return ret;
}

dds_entity_t dds_create_topic (dds_entity_t participant, const dds_topic_descriptor_t *desc, const char *name, const dds_qos_t *qos, const dds_listener_t *listener)
{
  struct ddsi_sertype_default *st;
  struct ddsi_sertype *st_tmp;
  ddsi_plist_t plist;
  dds_entity_t hdl;
  struct dds_entity *ppent;
  dds_return_t ret;

  if (desc == NULL || name == NULL)
    return DDS_RETCODE_BAD_PARAMETER;

  if ((ret = dds_entity_pin (participant, &ppent)) < 0)
    return ret;

  st = dds_alloc (sizeof (*st));

  ddsi_sertype_init (&st->c, desc->m_typename, &ddsi_sertype_ops_default, desc->m_nkeys ? &ddsi_serdata_ops_cdr : &ddsi_serdata_ops_cdr_nokey, (desc->m_nkeys == 0));
  st->native_encoding_identifier = (DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN ? CDR_LE : CDR_BE);
  st->serpool = ppent->m_domain->gv.serpool;
  st->type.size = desc->m_size;
  st->type.align = desc->m_align;
  st->type.flagset = desc->m_flagset;
  st->type.keys.nkeys = desc->m_nkeys;
  st->type.keys.keys = ddsrt_malloc (st->type.keys.nkeys  * sizeof (*st->type.keys.keys));
  for (uint32_t i = 0; i < st->type.keys.nkeys; i++)
    st->type.keys.keys[i] = desc->m_keys[i].m_index;
  st->type.ops.nops = dds_stream_countops (desc->m_ops);
  st->type.ops.ops = ddsrt_memdup (desc->m_ops, st->type.ops.nops * sizeof (*st->type.ops.ops));

  /* Check if topic cannot be optimised (memcpy marshal) */
  if (!(st->type.flagset & DDS_TOPIC_NO_OPTIMIZE)) {
    st->opt_size = dds_stream_check_optimize (&st->type);
    DDS_CTRACE (&ppent->m_domain->gv.logconfig, "Marshalling for type: %s is %soptimised\n", desc->m_typename, st->opt_size ? "" : "not ");
  }

  ddsi_plist_init_empty (&plist);
  /* Set Topic meta data (for SEDP publication) */
  plist.qos.topic_name = ddsrt_strdup (name);
  plist.qos.type_name = ddsrt_strdup (st->c.type_name);
  plist.qos.present |= (QP_TOPIC_NAME | QP_TYPE_NAME);
  if (desc->m_meta)
  {
    plist.type_description = dds_string_dup (desc->m_meta);
    plist.present |= PP_ADLINK_TYPE_DESCRIPTION;
  }
  if (desc->m_nkeys)
  {
    plist.qos.present |= QP_ADLINK_SUBSCRIPTION_KEYS;
    plist.qos.subscription_keys.use_key_list = 1;
    plist.qos.subscription_keys.key_list.n = desc->m_nkeys;
    plist.qos.subscription_keys.key_list.strs = dds_alloc (desc->m_nkeys * sizeof (char*));
    for (uint32_t index = 0; index < desc->m_nkeys; index++)
      plist.qos.subscription_keys.key_list.strs[index] = dds_string_dup (desc->m_keys[index].m_name);
  }

  st_tmp = &st->c;
  hdl = dds_create_topic_sertype (participant, name, &st_tmp, qos, listener, &plist);
  if (hdl < 0)
    ddsi_sertype_unref (st_tmp);
  dds_entity_unpin (ppent);
  ddsi_plist_fini (&plist);
  return hdl;
}

/* this function is deprecated, replaced by dds_find_topic_scoped */
dds_entity_t dds_find_topic (dds_entity_t participant, const char *name)
{
  dds_entity_t tp = dds_find_topic_scoped (DDS_FIND_SCOPE_PARTICIPANT, participant, name, 0);
  return tp != 0 ? tp : DDS_RETCODE_PRECONDITION_NOT_MET;
}

static dds_entity_t find_local_topic_pp (dds_participant *pp, const char *name, dds_participant *pp_topic)
{
  ddsrt_mutex_lock (&pp->m_entity.m_mutex);
  ddsrt_avl_iter_t it;
  for (dds_entity *e_pp_child = ddsrt_avl_iter_first (&dds_entity_children_td, &pp->m_entity.m_children, &it); e_pp_child != NULL; e_pp_child = ddsrt_avl_iter_next (&it))
  {
    if (dds_entity_kind (e_pp_child) != DDS_KIND_TOPIC)
      continue;

    /* pin to make sure that child is not closed */
    struct dds_entity *x;
    if (dds_entity_pin (e_pp_child->m_hdllink.hdl, &x) != DDS_RETCODE_OK)
      continue;

    struct dds_topic * const tp = (struct dds_topic *) e_pp_child;
    if (strcmp (tp->m_ktopic->name, name) != 0)
    {
      dds_entity_unpin (x);
      continue;
    }

    /* found a topic with the provided topic name */
    struct ddsi_sertype * const sertype = ddsi_sertype_ref (tp->m_stype);
    struct dds_ktopic * const ktp = tp->m_ktopic;
    ktp->refc++;
    dds_entity_unpin (x);

#ifdef DDS_HAS_TOPIC_DISCOVERY
    /* reference ktopic-sertype meta-data entry, this should be an existing
      entry because the topic already exists locally */
    topic_guid_map_ref (ktp, sertype);
#endif
    ddsrt_mutex_unlock (&pp->m_entity.m_mutex);

    /* create the topic on the provided pp */
    ddsrt_mutex_lock (&pp_topic->m_entity.m_mutex);
    dds_entity_t hdl = create_topic_pp_locked (pp_topic, ktp, false, name, sertype, NULL, NULL);
    ddsrt_mutex_unlock (&pp_topic->m_entity.m_mutex);
#ifdef DDS_HAS_TYPE_DISCOVERY
    struct ddsi_domaingv *gv = ddsrt_atomic_ldvoidp (&sertype->gv);
    ddsi_tl_meta_local_ref (gv, NULL, sertype);
#endif
    return hdl;
  }
  ddsrt_mutex_unlock (&pp->m_entity.m_mutex);
  return 0;
}

static dds_entity_t find_local_topic_impl (dds_find_scope_t scope, dds_participant *pp_topic, const char *name)
{
  dds_entity *e_pp, *e_domain_child;
  dds_instance_handle_t last_iid = 0;

  if (scope == DDS_FIND_SCOPE_PARTICIPANT)
    return find_local_topic_pp (pp_topic, name, pp_topic);
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
    dds_entity_t hdl = find_local_topic_pp (pp, name, pp_topic);
    dds_entity_unpin (e_pp);
    if (hdl != 0)
      return hdl;
    ddsrt_mutex_lock (&dom->m_entity.m_mutex);
  }
  ddsrt_mutex_unlock (&dom->m_entity.m_mutex);
  return 0;
}


#ifdef DDS_HAS_TOPIC_DISCOVERY

static dds_entity_t find_remote_topic_impl (dds_participant *pp_topic, const char *name, dds_duration_t timeout)
{
  dds_entity_t ret;
  struct ddsi_topic_definition *tpd;
  struct ddsi_sertype *sertype;

  if ((ret = lookup_topic_definition_by_name (&pp_topic->m_entity.m_domain->gv, name, &tpd)) != DDS_RETCODE_OK)
    return ret;
  if (tpd == NULL)
    return DDS_RETCODE_OK;
  if ((ret = dds_domain_resolve_type (pp_topic->m_entity.m_hdllink.hdl, tpd->type_id.hash, sizeof (tpd->type_id.hash), timeout, &sertype)) != DDS_RETCODE_OK)
  {
    /* if topic definition is found, but the type for this topic is not resolved
        and timeout 0 means we don't want to request and wait for the type to be retrieved */
    if (ret == DDS_RETCODE_TIMEOUT && timeout == 0)
      ret = DDS_RETCODE_OK;
    return ret;
  }
  return dds_create_topic_impl (pp_topic->m_entity.m_hdllink.hdl, name, false, &sertype, tpd->xqos, NULL, NULL, false);
}

#endif /* DDS_HAS_TOPIC_DISCOVERY */

dds_entity_t dds_find_topic_scoped (dds_find_scope_t scope, dds_entity_t participant, const char *name, dds_duration_t timeout)
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
    if ((hdl = find_local_topic_impl (scope, pp_topic, name)) == 0 && scope == DDS_FIND_SCOPE_GLOBAL)
    {
#ifdef DDS_HAS_TOPIC_DISCOVERY
      hdl = find_remote_topic_impl (pp_topic, name, timeout);
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
        f.f.sample = 0;
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
        valid = (filter->f.sample != 0);
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

static void dds_set_topic_filter_deprecated (dds_entity_t topic, dds_topic_filter_fn filter)
{
  struct dds_topic_filter f = {
    .mode = filter ? DDS_TOPIC_FILTER_SAMPLE : DDS_TOPIC_FILTER_NONE,
    .arg = NULL,
    .f = { .sample = filter }
  };
  (void) dds_set_topic_filter_extended (topic, &f);
}

void dds_set_topic_filter (dds_entity_t topic, dds_topic_filter_fn filter)
{
  dds_set_topic_filter_deprecated (topic, filter);
}

void dds_topic_set_filter (dds_entity_t topic, dds_topic_filter_fn filter)
{
  dds_set_topic_filter_deprecated (topic, filter);
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

static dds_topic_filter_fn dds_get_topic_filter_deprecated (dds_entity_t topic)
{
  struct dds_topic_filter f;
  if (dds_get_topic_filter_extended (topic, &f) != DDS_RETCODE_OK)
    return 0;
  return (f.mode == DDS_TOPIC_FILTER_SAMPLE) ? f.f.sample : 0;
}

dds_topic_filter_fn dds_get_topic_filter (dds_entity_t topic)
{
  return dds_get_topic_filter_deprecated (topic);
}

dds_topic_filter_fn dds_topic_get_filter (dds_entity_t topic)
{
  return dds_get_topic_filter_deprecated (topic);
}

dds_return_t dds_get_name (dds_entity_t topic, char *name, size_t size)
{
  dds_topic *t;
  dds_return_t ret;
  if (size <= 0 || name == NULL)
    return DDS_RETCODE_BAD_PARAMETER;
  name[0] = '\0';
  if ((ret = dds_topic_pin (topic, &t)) != DDS_RETCODE_OK)
    return ret;
  (void) snprintf (name, size, "%s", t->m_name);
  dds_topic_unpin (t);
  return DDS_RETCODE_OK;
}

dds_return_t dds_get_type_name (dds_entity_t topic, char *name, size_t size)
{
  dds_topic *t;
  dds_return_t ret;
  if (size <= 0 || name == NULL)
    return DDS_RETCODE_BAD_PARAMETER;
  name[0] = '\0';
  if ((ret = dds_topic_pin (topic, &t)) != DDS_RETCODE_OK)
    return ret;
  (void) snprintf (name, size, "%s", t->m_stype->type_name);
  dds_topic_unpin (t);
  return DDS_RETCODE_OK;
}

DDS_GET_STATUS(topic, inconsistent_topic, INCONSISTENT_TOPIC, total_count_change)
