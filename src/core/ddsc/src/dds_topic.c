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
#include "dds/ddsi/ddsi_entity_index.h"
#include "dds/ddsi/q_thread.h"
#include "dds/ddsi/ddsi_sertopic.h"
#include "dds/ddsi/q_ddsi_discovery.h"
#include "dds/ddsi/ddsi_iid.h"
#include "dds/ddsi/q_plist.h"
#include "dds/ddsi/q_globals.h"
#include "dds__serdata_builtintopic.h"

DECL_ENTITY_LOCK_UNLOCK (extern inline, dds_topic)

#define DDS_TOPIC_STATUS_MASK                                    \
                        (DDS_INCONSISTENT_TOPIC_STATUS)

struct topic_sertopic_node {
  ddsrt_avl_node_t avlnode;
  uint32_t refc;
  const struct ddsi_sertopic *st;
};

static int topic_sertopic_node_cmp (const void *va, const void *vb)
{
  const struct ddsi_sertopic *a = va;
  const struct ddsi_sertopic *b = vb;
  return strcmp (a->name, b->name);
}

const ddsrt_avl_treedef_t dds_topictree_def = DDSRT_AVL_TREEDEF_INITIALIZER_INDKEY (offsetof (struct topic_sertopic_node, avlnode), offsetof (struct topic_sertopic_node, st), topic_sertopic_node_cmp, 0);

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

struct ddsi_sertopic *dds_topic_lookup (dds_domain *domain, const char *name)
{
  const struct ddsi_sertopic key = { .name = (char *) name };
  struct ddsi_sertopic *st;
  struct topic_sertopic_node *nst;
  ddsrt_mutex_lock (&dds_global.m_mutex);
  if ((nst = ddsrt_avl_lookup (&dds_topictree_def, &domain->m_topics, &key)) == NULL)
    st = NULL;
  else
    st = ddsi_sertopic_ref (nst->st);
  ddsrt_mutex_unlock (&dds_global.m_mutex);
  return st;
}

static bool dds_find_topic_check_and_add_ref (dds_entity_t participant, dds_entity_t topic, const char *name)
{
  dds_topic *tp;
  if (dds_topic_lock (topic, &tp) != DDS_RETCODE_OK)
    return false;

  bool ret;
  if (dds_entity_participant (&tp->m_entity)->m_entity.m_hdllink.hdl != participant || strcmp (tp->m_stopic->name, name) != 0)
    ret = false;
  else
  {
    /* Simply return the same topic, though that is different to the spirit
       of the DDS specification, which gives you a unique copy.  Giving that
       unique copy means there potentially many versions of exactly the same
       topic around, and that two entities can be dealing with the same data
       even though they have different topics objects (though with the same
       name).  That I find a confusing model.

       As far as I can tell, the only benefit is the ability to set different
       listeners on the various copies of the topic.  And that seems to be a
       really small benefit. */
    ret = true;
  }
  dds_topic_unlock (tp);
  return ret;
}

dds_entity_t dds_find_topic (dds_entity_t participant, const char *name)
{
  dds_entity *pe;
  dds_return_t ret;
  dds_entity_t topic;

  if (name == NULL)
    return DDS_RETCODE_BAD_PARAMETER;

  /* claim participant handle to guarantee the handle remains valid after
     unlocking the participant prior to verifying the found topic still
     exists */
  if ((ret = dds_entity_pin (participant, &pe)) < 0)
    return ret;
  if (dds_entity_kind (pe) != DDS_KIND_PARTICIPANT)
  {
    dds_entity_unpin (pe);
    return DDS_RETCODE_ILLEGAL_OPERATION;
  }

  do {
    dds_participant *p;
    topic = DDS_RETCODE_PRECONDITION_NOT_MET;
    if ((ret = dds_participant_lock (participant, &p)) == DDS_RETCODE_OK)
    {
      ddsrt_avl_iter_t it;
      for (dds_entity *e = ddsrt_avl_iter_first (&dds_entity_children_td, &p->m_entity.m_children, &it); e != NULL; e = ddsrt_avl_iter_next (&it))
      {
        if (dds_entity_kind (e) == DDS_KIND_TOPIC && strcmp (((dds_topic *) e)->m_stopic->name, name) == 0)
        {
          topic = e->m_hdllink.hdl;
          break;
        }
      }
      dds_participant_unlock (p);
    }
  } while (topic > 0 && !dds_find_topic_check_and_add_ref (participant, topic, name));

  dds_entity_unpin (pe);
  return topic;
}

static dds_return_t dds_topic_delete (dds_entity *e) ddsrt_nonnull_all;

static dds_return_t dds_topic_delete (dds_entity *e)
{
  dds_topic *tp = (dds_topic *) e;
  dds_domain *domain = tp->m_entity.m_domain;
  ddsrt_avl_dpath_t dp;
  struct topic_sertopic_node *stn;

  ddsrt_mutex_lock (&dds_global.m_mutex);

  stn = ddsrt_avl_lookup_dpath (&dds_topictree_def, &domain->m_topics, tp->m_stopic, &dp);
  assert (stn != NULL);
  if (--stn->refc == 0)
  {
    ddsrt_avl_delete_dpath (&dds_topictree_def, &domain->m_topics, stn, &dp);
    ddsrt_free (stn);
  }

  ddsi_sertopic_unref (tp->m_stopic);
  ddsrt_mutex_unlock (&dds_global.m_mutex);
  return DDS_RETCODE_OK;
}

static dds_return_t dds_topic_qos_set (dds_entity *e, const dds_qos_t *qos, bool enabled)
{
  /* note: e->m_qos is still the old one to allow for failure here */
  (void) e; (void) qos; (void) enabled;
  return DDS_RETCODE_OK;
}

static bool dupdef_qos_ok (const dds_qos_t *qos, const dds_topic *tp)
{
  if ((qos == NULL) != (tp->m_entity.m_qos == NULL))
    return false;
  else if (qos == NULL)
    return true;
  else
    return dds_qos_equal (tp->m_entity.m_qos, qos);
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

static dds_return_t create_topic_topic_arbitrary_check_sertopic (dds_entity_t participant, dds_entity_t topic, struct ddsi_sertopic *sertopic, const dds_qos_t *qos)
{
  dds_topic *tp;
  dds_return_t ret;

  if (dds_topic_lock (topic, &tp) < 0)
    return DDS_RETCODE_NOT_FOUND;

  if (dds_entity_participant (&tp->m_entity)->m_entity.m_hdllink.hdl != participant)
    ret = DDS_RETCODE_NOT_FOUND;
  else if (!sertopic_equivalent (tp->m_stopic, sertopic))
    ret = DDS_RETCODE_PRECONDITION_NOT_MET;
  else if (!dupdef_qos_ok (qos, tp))
    ret = DDS_RETCODE_INCONSISTENT_POLICY;
  else
  {
    /* See dds_find_topic_check_and_add_ref */
    ret = DDS_RETCODE_OK;
  }
  dds_topic_unlock (tp);
  return ret;
}

const struct dds_entity_deriver dds_entity_deriver_topic = {
  .interrupt = dds_entity_deriver_dummy_interrupt,
  .close = dds_entity_deriver_dummy_close,
  .delete = dds_topic_delete,
  .set_qos = dds_topic_qos_set,
  .validate_status = dds_topic_status_validate
};

dds_entity_t dds_create_topic_arbitrary (dds_entity_t participant, struct ddsi_sertopic *sertopic, const dds_qos_t *qos, const dds_listener_t *listener, const nn_plist_t *sedp_plist)
{
  dds_return_t rc;
  dds_participant *par;
  dds_entity *par_ent;
  dds_topic *top;
  dds_qos_t *new_qos = NULL;
  dds_entity_t hdl;
  struct participant *ddsi_pp;

  if (sertopic == NULL)
    return DDS_RETCODE_BAD_PARAMETER;

  /* Claim participant handle so we can be sure the handle will not be
     reused if we temporarily unlock the participant to check the an
     existing topic's compatibility */
  if ((rc = dds_entity_pin (participant, &par_ent)) < 0)
    return rc;
  /* Verify that we've been given a participant, not strictly necessary
     because dds_participant_lock below checks it, but this is more
     obvious */
  if (dds_entity_kind (par_ent) != DDS_KIND_PARTICIPANT)
  {
    dds_entity_unpin (par_ent);
    return DDS_RETCODE_ILLEGAL_OPERATION;
  }

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
  if ((rc = nn_xqos_valid (&par_ent->m_domain->gv.logconfig, new_qos)) != DDS_RETCODE_OK)
    goto err_invalid_qos;

  /* FIXME: just mutex_lock ought to be good enough, but there is the
     pesky "closed" check still ... */
  if ((rc = dds_participant_lock (participant, &par)) != DDS_RETCODE_OK)
    goto err_lock_participant;

  bool retry_lookup;
  do {
    dds_entity_t topic;

    /* claim participant handle to guarantee the handle remains valid after
        unlocking the participant prior to verifying the found topic still
        exists */
    topic = DDS_RETCODE_PRECONDITION_NOT_MET;
    ddsrt_avl_iter_t it;
    for (dds_entity *e = ddsrt_avl_iter_first (&dds_entity_children_td, &par->m_entity.m_children, &it); e != NULL; e = ddsrt_avl_iter_next (&it))
    {
      if (dds_entity_kind (e) == DDS_KIND_TOPIC && strcmp (((dds_topic *) e)->m_stopic->name, sertopic->name) == 0)
      {
        topic = e->m_hdllink.hdl;
        break;
      }
    }
    if (topic < 0)
    {
      /* no topic with the name exists; we have locked the participant, and
         so we can proceed with creating the topic */
      retry_lookup = false;
    }
    else
    {
      /* some topic with the same name exists; need to lock the topic to
         perform the checks, but locking the topic while holding the
         participant lock violates the lock order (child -> parent).  So
         unlock that participant and check the topic while accounting
         for the various scary cases. */
      dds_participant_unlock (par);

      rc = create_topic_topic_arbitrary_check_sertopic (participant, topic, sertopic, new_qos);
      switch (rc)
      {
        case DDS_RETCODE_OK: /* duplicate definition */
          dds_entity_unpin (par_ent);
          dds_delete_qos (new_qos);
          return topic;

        case DDS_RETCODE_NOT_FOUND:
          /* either participant is now being deleted, topic was deleted, or
             topic was deleted & the handle reused for something else -- so */
          retry_lookup = true;
          break;

        case DDS_RETCODE_PRECONDITION_NOT_MET: /* incompatible sertopic */
        case DDS_RETCODE_INCONSISTENT_POLICY: /* different QoS */
          /* inconsistent definition */
          dds_entity_unpin (par_ent);
          dds_delete_qos (new_qos);
          return rc;

        default:
          abort ();
      }

      if ((rc = dds_participant_lock (participant, &par)) != DDS_RETCODE_OK)
        goto err_lock_participant;
    }
  } while (retry_lookup);

  /* FIXME: make this a function
     Add sertopic to domain -- but note that it may have been created by another thread
     on another participant that is attached to the same domain */
  {
    struct dds_domain *domain = par->m_entity.m_domain;

    ddsrt_avl_ipath_t ip;
    struct topic_sertopic_node *stn;

    ddsrt_mutex_lock (&dds_global.m_mutex);

    stn = ddsrt_avl_lookup_ipath (&dds_topictree_def, &domain->m_topics, sertopic, &ip);
    if (stn == NULL)
    {
      /* no existing definition: use new */
      stn = ddsrt_malloc (sizeof (*stn));
      stn->refc = 1;
      stn->st = ddsi_sertopic_ref (sertopic);
      ddsrt_avl_insert (&dds_topictree_def, &domain->m_topics, stn);
      ddsrt_mutex_unlock (&dds_global.m_mutex);
    }
    else if (sertopic_equivalent (stn->st, sertopic))
    {
      /* ok -- same definition, so use existing one instead */
      sertopic = ddsi_sertopic_ref (stn->st);
      stn->refc++;
      ddsrt_mutex_unlock (&dds_global.m_mutex);
    }
    else
    {
      /* bummer, delete */
      ddsrt_mutex_unlock (&dds_global.m_mutex);
      rc = DDS_RETCODE_PRECONDITION_NOT_MET;
      goto err_sertopic_reuse;
    }
  }

  /* Create topic */
  top = dds_alloc (sizeof (*top));
  /* FIXME: setting "implicit" based on sertopic->ops is a hack */
  hdl = dds_entity_init (&top->m_entity, &par->m_entity, DDS_KIND_TOPIC, (sertopic->ops == &ddsi_sertopic_ops_builtintopic), new_qos, listener, DDS_TOPIC_STATUS_MASK);
  top->m_entity.m_iid = ddsi_iid_gen ();
  dds_entity_register_child (&par->m_entity, &top->m_entity);
  top->m_stopic = sertopic;

  /* Publish Topic */
  thread_state_awake (lookup_thread_state (), &par->m_entity.m_domain->gv);
  ddsi_pp = entidx_lookup_participant_guid (par->m_entity.m_domain->gv.entity_index, &par->m_entity.m_guid);
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

  dds_entity_init_complete (&top->m_entity);
  dds_participant_unlock (par);
  dds_entity_unpin (par_ent);
  return hdl;

err_sertopic_reuse:
  dds_participant_unlock (par);
err_lock_participant:
err_invalid_qos:
  dds_delete_qos (new_qos);
  dds_entity_unpin (par_ent);
  return rc;
}

dds_entity_t dds_create_topic (dds_entity_t participant, const dds_topic_descriptor_t *desc, const char *name, const dds_qos_t *qos, const dds_listener_t *listener)
{
  struct ddsi_sertopic_default *st;
  nn_plist_t plist;
  dds_entity_t hdl;
  struct dds_entity *ppent;
  dds_return_t ret;

  if (desc == NULL || name == NULL || !is_valid_name (name))
    return DDS_RETCODE_BAD_PARAMETER;

  if ((ret = dds_entity_pin (participant, &ppent)) < 0)
    return ret;

  st = dds_alloc (sizeof (*st));

  ddsi_sertopic_init (&st->c, name, desc->m_typename, &ddsi_sertopic_ops_default, desc->m_nkeys ? &ddsi_serdata_ops_cdr : &ddsi_serdata_ops_cdr_nokey, (desc->m_nkeys == 0));
  st->native_encoding_identifier = (DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN ? CDR_LE : CDR_BE);
  st->serpool = ppent->m_domain->gv.serpool;
  st->type = (void*) desc;
  st->nkeys = desc->m_nkeys;
  st->keys = desc->m_keys;

  /* Check if topic cannot be optimised (memcpy marshal) */
  if (!(desc->m_flagset & DDS_TOPIC_NO_OPTIMIZE)) {
    st->opt_size = dds_stream_check_optimize (desc);
    DDS_CTRACE (&ppent->m_domain->gv.logconfig, "Marshalling for type: %s is %soptimised\n", desc->m_typename, st->opt_size ? "" : "not ");
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
  dds_entity_unpin (ppent);
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
