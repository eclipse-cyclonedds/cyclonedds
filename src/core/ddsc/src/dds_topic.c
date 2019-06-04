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

#include "dds/ddsrt/atomics.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/string.h"
#include "dds__topic.h"
#include "dds__listener.h"
#include "dds__participant.h"
#include "dds__stream.h"
#include "dds__init.h"
#include "dds__domain.h"
#include "dds__get_status.h"
#include "dds__qos.h"
#include "dds/ddsi/q_entity.h"
#include "dds/ddsi/q_thread.h"
#include "dds/ddsi/ddsi_sertopic.h"
#include "dds/ddsi/q_ddsi_discovery.h"
#include "dds/ddsi/ddsi_iid.h"
#include "dds/ddsi/q_plist.h"
#include "dds/ddsi/q_globals.h"

DECL_ENTITY_LOCK_UNLOCK (extern inline, dds_topic)

#define DDS_TOPIC_STATUS_MASK                                    \
                        (DDS_INCONSISTENT_TOPIC_STATUS)

static int strcmp_wrapper (const void *va, const void *vb)
{
  return strcmp (va, vb);
}

const ddsrt_avl_treedef_t dds_topictree_def = DDSRT_AVL_TREEDEF_INITIALIZER_INDKEY (offsetof (struct ddsi_sertopic, avlnode), offsetof (struct ddsi_sertopic, name_type_name), strcmp_wrapper, 0);

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

/*
  Topic status change callback handler. Supports INCONSISTENT_TOPIC
  status (only defined status on a topic).
*/

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

struct ddsi_sertopic *dds_topic_lookup_locked (dds_domain *domain, const char *name) ddsrt_nonnull_all;

struct ddsi_sertopic *dds_topic_lookup_locked (dds_domain *domain, const char *name)
{
  ddsrt_avl_iter_t iter;
  for (struct ddsi_sertopic *st = ddsrt_avl_iter_first (&dds_topictree_def, &domain->m_topics, &iter); st; st = ddsrt_avl_iter_next (&iter))
    if (strcmp (st->name, name) == 0)
      return st;
  return NULL;
}

struct ddsi_sertopic *dds_topic_lookup (dds_domain *domain, const char *name)
{
  struct ddsi_sertopic *st;
  ddsrt_mutex_lock (&dds_global.m_mutex);
  st = dds_topic_lookup_locked (domain, name);
  ddsrt_mutex_unlock (&dds_global.m_mutex);
  return st;
}

void dds_topic_free (dds_domainid_t domainid, struct ddsi_sertopic *st)
{
  dds_domain *domain;
  ddsrt_mutex_lock (&dds_global.m_mutex);
  domain = ddsrt_avl_lookup (&dds_domaintree_def, &dds_global.m_domains, &domainid);
  if (domain != NULL)
  {
    assert (ddsrt_avl_lookup (&dds_topictree_def, &domain->m_topics, st->name_type_name) != NULL);
    ddsrt_avl_delete (&dds_topictree_def, &domain->m_topics, st);
  }
  ddsrt_mutex_unlock (&dds_global.m_mutex);
  st->status_cb_entity = NULL;
  ddsi_sertopic_unref (st);
}

static void dds_topic_add_locked (dds_domainid_t id, struct ddsi_sertopic *st)
{
  dds_domain *dom = dds_domain_find_locked (id);
  assert (dom);
  assert (ddsrt_avl_lookup (&dds_topictree_def, &dom->m_topics, st->name_type_name) == NULL);
  ddsrt_avl_insert (&dds_topictree_def, &dom->m_topics, st);
}

dds_entity_t dds_find_topic (dds_entity_t participant, const char *name)
{
  dds_entity_t tp;
  dds_participant *p;
  struct ddsi_sertopic *st;
  dds_return_t rc;

  if (name == NULL)
    return DDS_RETCODE_BAD_PARAMETER;

  if ((rc = dds_participant_lock (participant, &p)) != DDS_RETCODE_OK)
    return rc;

  ddsrt_mutex_lock (&dds_global.m_mutex);
  if ((st = dds_topic_lookup_locked (p->m_entity.m_domain, name)) == NULL)
    tp = DDS_RETCODE_PRECONDITION_NOT_MET;
  else
  {
    /* FIXME: calling addref is wrong because the Cyclone library has no
       knowledge of the reference and hence simply deleting the participant
       won't make the ref count drop to 0.  On the other hand, the DDS spec
       says find_topic (and a second call to create_topic) return a new
       proxy that must separately be deleted.  */
    dds_entity_add_ref (&st->status_cb_entity->m_entity);
    tp = st->status_cb_entity->m_entity.m_hdllink.hdl;
  }
  ddsrt_mutex_unlock (&dds_global.m_mutex);
  dds_participant_unlock (p);
  return tp;
}

static dds_return_t dds_topic_delete (dds_entity *e) ddsrt_nonnull_all;

static dds_return_t dds_topic_delete (dds_entity *e)
{
  dds_topic_free (e->m_domainid, ((dds_topic *) e)->m_stopic);
  return DDS_RETCODE_OK;
}

static dds_return_t dds_topic_qos_set (dds_entity *e, const dds_qos_t *qos, bool enabled)
{
  /* note: e->m_qos is still the old one to allow for failure here */
  (void) e; (void) qos; (void) enabled;
  return DDS_RETCODE_OK;
}

static bool dupdef_qos_ok (const dds_qos_t *qos, const struct ddsi_sertopic *st)
{
  if ((qos == NULL) != (st->status_cb_entity->m_entity.m_qos == NULL))
    return false;
  else if (qos == NULL)
    return true;
  else
    return dds_qos_equal (st->status_cb_entity->m_entity.m_qos, qos);
}

static bool sertopic_equivalent (const struct ddsi_sertopic *a, const struct ddsi_sertopic *b)
{
  if (strcmp (a->name_type_name, b->name_type_name) != 0)
    return false;
  if (a->serdata_basehash != b->serdata_basehash)
    return false;
  if (a->ops != b->ops)
    return false;
  if (a->serdata_ops != b->serdata_ops)
    return false;
  return true;
}

dds_entity_t dds_create_topic_arbitrary (dds_entity_t participant, struct ddsi_sertopic *sertopic, const dds_qos_t *qos, const dds_listener_t *listener, const nn_plist_t *sedp_plist)
{
  struct ddsi_sertopic *stgeneric;
  dds_return_t rc;
  dds_participant *par;
  dds_topic *top;
  dds_qos_t *new_qos = NULL;
  dds_entity_t hdl;
  struct participant *ddsi_pp;

  if (sertopic == NULL)
    return DDS_RETCODE_BAD_PARAMETER;

  new_qos = dds_create_qos ();
  if (qos)
    nn_xqos_mergein_missing (new_qos, qos, DDS_TOPIC_QOS_MASK);
  /* One would expect this:
   *
   *   nn_xqos_mergein_missing (new_qos, &gv.default_xqos_tp, ~(uint64_t)0);
   *
   * but the crazy defaults of the DDS specification has a default settings
   * for reliability that are dependent on the entity type: readers and
   * topics default to best-effort, but writers to reliable.
   *
   * Leaving the topic QoS sparse means a default-default topic QoS of
   * best-effort will do "the right thing" and let a writer still default to
   * reliable ... (and keep behaviour unchanged) */
  if ((rc = nn_xqos_valid (new_qos)) != DDS_RETCODE_OK)
    goto err_invalid_qos;

  if ((rc = dds_participant_lock (participant, &par)) != DDS_RETCODE_OK)
    goto err_lock_participant;

  /* Check if topic already exists with same name */
  ddsrt_mutex_lock (&dds_global.m_mutex);
  if ((stgeneric = dds_topic_lookup_locked (par->m_entity.m_domain, sertopic->name)) != NULL) {
    if (!sertopic_equivalent (stgeneric, sertopic)) {
      /* FIXME: should copy the type, perhaps? but then the pointers will no longer be the same */
      rc = DDS_RETCODE_PRECONDITION_NOT_MET;
      goto err_mismatch;
    } else if (!dupdef_qos_ok (new_qos, stgeneric)) {
      /* FIXME: should copy the type, perhaps? but then the pointers will no longer be the same */
      rc = DDS_RETCODE_INCONSISTENT_POLICY;
      goto err_mismatch;
    } else {
      /* FIXME: calling addref is wrong because the Cyclone library has no
         knowledge of the reference and hence simply deleting the participant
         won't make the ref count drop to 0.  On the other hand, the DDS spec
         says find_topic (and a second call to create_topic) return a new
         proxy that must separately be deleted.  */
      dds_entity_add_ref (&stgeneric->status_cb_entity->m_entity);
      hdl = stgeneric->status_cb_entity->m_entity.m_hdllink.hdl;
      dds_delete_qos (new_qos);
    }
    ddsrt_mutex_unlock (&dds_global.m_mutex);
  } else {
    /* Create topic */
    top = dds_alloc (sizeof (*top));
    hdl = dds_entity_init (&top->m_entity, &par->m_entity, DDS_KIND_TOPIC, new_qos, listener, DDS_TOPIC_STATUS_MASK);
    top->m_entity.m_deriver.delete = dds_topic_delete;
    top->m_entity.m_deriver.set_qos = dds_topic_qos_set;
    top->m_entity.m_deriver.validate_status = dds_topic_status_validate;
    top->m_stopic = ddsi_sertopic_ref (sertopic);
    sertopic->status_cb_entity = top;

    /* Add topic to extent */
    dds_topic_add_locked (par->m_entity.m_domainid, sertopic);
    ddsrt_mutex_unlock (&dds_global.m_mutex);

    /* Publish Topic */
    thread_state_awake (lookup_thread_state ());
    ddsi_pp = ephash_lookup_participant_guid (&par->m_entity.m_guid);
    assert (ddsi_pp);
    if (sedp_plist)
    {
      nn_plist_t plist;
      nn_plist_init_empty (&plist);
      nn_plist_mergein_missing (&plist, sedp_plist, ~(uint64_t)0, ~(uint64_t)0);
      nn_xqos_mergein_missing (&plist.qos, new_qos, ~(uint64_t)0);
      sedp_write_topic (ddsi_pp, &plist);
      nn_plist_fini (&plist);
    }
    thread_state_asleep (lookup_thread_state ());
  }
  dds_participant_unlock (par);
  return hdl;

err_mismatch:
  ddsrt_mutex_unlock (&dds_global.m_mutex);
  dds_participant_unlock (par);
err_lock_participant:
err_invalid_qos:
  dds_delete_qos (new_qos);
  return rc;
}

dds_entity_t dds_create_topic (dds_entity_t participant, const dds_topic_descriptor_t *desc, const char *name, const dds_qos_t *qos, const dds_listener_t *listener)
{
  char *key = NULL;
  struct ddsi_sertopic_default *st;
  const char *typename;
  nn_plist_t plist;
  dds_entity_t hdl;
  size_t keysz;

  if (desc == NULL || name == NULL || !is_valid_name (name))
    return DDS_RETCODE_BAD_PARAMETER;

  typename = desc->m_typename;
  keysz = strlen (name) + strlen (typename) + 2;
  key = dds_alloc (keysz);
  (void) snprintf (key, keysz, "%s/%s", name, typename);

  st = dds_alloc (sizeof (*st));

  ddsrt_atomic_st32 (&st->c.refc, 1);
  st->c.iid = ddsi_iid_gen ();
  st->c.status_cb = dds_topic_status_cb;
  st->c.status_cb_entity = NULL; /* set by dds_create_topic_arbitrary */
  st->c.name_type_name = key;
  st->c.name = ddsrt_strdup (name);
  st->c.type_name = ddsrt_strdup (typename);
  st->c.ops = &ddsi_sertopic_ops_default;
  st->c.serdata_ops = desc->m_nkeys ? &ddsi_serdata_ops_cdr : &ddsi_serdata_ops_cdr_nokey;
  st->c.serdata_basehash = ddsi_sertopic_compute_serdata_basehash (st->c.serdata_ops);
  st->native_encoding_identifier = (DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN ? CDR_LE : CDR_BE);

  st->type = (void*) desc;
  st->nkeys = desc->m_nkeys;
  st->keys = desc->m_keys;

  /* Check if topic cannot be optimised (memcpy marshal) */
  if (!(desc->m_flagset & DDS_TOPIC_NO_OPTIMIZE)) {
    st->opt_size = dds_stream_check_optimize (desc);
  }

  nn_plist_init_empty (&plist);
  /* Set Topic meta data (for SEDP publication) */
  plist.qos.topic_name = ddsrt_strdup (st->c.name);
  plist.qos.type_name = ddsrt_strdup (st->c.type_name);
  plist.qos.present |= (QP_TOPIC_NAME | QP_TYPE_NAME);
  if (desc->m_meta)
  {
    plist.type_description = dds_string_dup (desc->m_meta);
    plist.present |= PP_PRISMTECH_TYPE_DESCRIPTION;
  }
  if (desc->m_nkeys)
  {
    plist.qos.present |= QP_PRISMTECH_SUBSCRIPTION_KEYS;
    plist.qos.subscription_keys.use_key_list = 1;
    plist.qos.subscription_keys.key_list.n = desc->m_nkeys;
    plist.qos.subscription_keys.key_list.strs = dds_alloc (desc->m_nkeys * sizeof (char*));
    for (uint32_t index = 0; index < desc->m_nkeys; index++)
      plist.qos.subscription_keys.key_list.strs[index] = dds_string_dup (desc->m_keys[index].m_name);
  }

  hdl = dds_create_topic_arbitrary (participant, &st->c, qos, listener, &plist);
  ddsi_sertopic_unref (&st->c);
  nn_plist_fini (&plist);
  return hdl;
}

static bool dds_topic_chaining_filter (const void *sample, void *ctx)
{
  dds_topic_filter_fn realf = (dds_topic_filter_fn) ctx;
  return realf (sample);
}

static void dds_topic_mod_filter (dds_entity_t topic, dds_topic_intern_filter_fn *filter, void **ctx, bool set)
{
  dds_topic *t;
  if (dds_topic_lock (topic, &t) == DDS_RETCODE_OK)
  {
    if (set) {
      t->filter_fn = *filter;
      t->filter_ctx = *ctx;
    } else {
      *filter = t->filter_fn;
      *ctx = t->filter_ctx;
    }
    dds_topic_unlock (t);
  }
  else
  {
    *filter = 0;
    *ctx = NULL;
  }
}

void dds_set_topic_filter (dds_entity_t topic, dds_topic_filter_fn filter)
{
  dds_topic_intern_filter_fn chaining = dds_topic_chaining_filter;
  void *realf = (void *) filter;
  dds_topic_mod_filter (topic, &chaining, &realf, true);
}

void dds_topic_set_filter (dds_entity_t topic, dds_topic_filter_fn filter)
{
  dds_set_topic_filter (topic, filter);
}

dds_topic_filter_fn dds_get_topic_filter (dds_entity_t topic)
{
  dds_topic_intern_filter_fn filter;
  void *ctx;
  dds_topic_mod_filter (topic, &filter, &ctx, false);
  return (filter == dds_topic_chaining_filter) ? (dds_topic_filter_fn) ctx : 0;
}

dds_topic_filter_fn dds_topic_get_filter (dds_entity_t topic)
{
  return dds_get_topic_filter (topic);
}

void dds_topic_set_filter_with_ctx (dds_entity_t topic, dds_topic_intern_filter_fn filter, void *ctx)
{
  dds_topic_mod_filter (topic, &filter, &ctx, true);
}

dds_topic_intern_filter_fn dds_topic_get_filter_with_ctx (dds_entity_t topic)
{
  dds_topic_intern_filter_fn filter;
  void *ctx;
  dds_topic_mod_filter (topic, &filter, &ctx, false);
  return (filter == dds_topic_chaining_filter) ? 0 : filter;
}

dds_return_t dds_get_name (dds_entity_t topic, char *name, size_t size)
{
  dds_topic *t;
  dds_return_t ret;
  if (size <= 0 || name == NULL)
    return DDS_RETCODE_BAD_PARAMETER;
  name[0] = '\0';
  if ((ret = dds_topic_lock (topic, &t)) != DDS_RETCODE_OK)
    return ret;
  (void) snprintf (name, size, "%s", t->m_stopic->name);
  dds_topic_unlock (t);
  return DDS_RETCODE_OK;
}

dds_return_t dds_get_type_name (dds_entity_t topic, char *name, size_t size)
{
  dds_topic *t;
  dds_return_t ret;
  if (size <= 0 || name == NULL)
    return DDS_RETCODE_BAD_PARAMETER;
  name[0] = '\0';
  if ((ret = dds_topic_lock (topic, &t)) != DDS_RETCODE_OK)
    return ret;
  (void) snprintf (name, size, "%s", t->m_stopic->type_name);
  dds_topic_unlock (t);
  return DDS_RETCODE_OK;
}

DDS_GET_STATUS(topic, inconsistent_topic, INCONSISTENT_TOPIC, total_count_change)
