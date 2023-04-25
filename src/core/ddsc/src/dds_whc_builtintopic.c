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
#include <stddef.h>
#include <string.h>

#include "dds/ddsrt/heap.h"
#include "dds/ddsi/ddsi_serdata.h"
#include "dds/ddsi/ddsi_unused.h"
#include "dds/ddsi/ddsi_entity_index.h"
#include "dds/ddsi/ddsi_entity.h"
#include "dds/ddsi/ddsi_proxy_participant.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsi/ddsi_tkmap.h"
#include "dds__serdata_builtintopic.h"
#include "dds__whc_builtintopic.h"
#include "dds__builtin.h"

struct bwhc {
  struct ddsi_whc common;
  enum ddsi_sertype_builtintopic_entity_kind entity_kind;
  const struct ddsi_entity_index *entidx;
};

enum bwhc_iter_state {
  BIS_INIT_LOCAL,
  BIS_LOCAL,
  BIS_INIT_PROXY,
  BIS_PROXY
};

struct bwhc_iter {
  struct ddsi_whc_sample_iter_base c;
  enum bwhc_iter_state st;
  bool have_sample;
  struct ddsi_entity_enum it;
#ifdef DDS_HAS_TOPIC_DISCOVERY
  struct ddsi_proxy_participant *cur_proxypp;
  ddsi_entityid_t proxytp_eid;
#endif
};

/* check that our definition of whc_sample_iter fits in the type that callers allocate */
DDSRT_STATIC_ASSERT (sizeof (struct bwhc_iter) <= sizeof (struct ddsi_whc_sample_iter));

static void bwhc_free (struct ddsi_whc *whc_generic)
{
  ddsrt_free (whc_generic);
}

static void bwhc_sample_iter_init (const struct ddsi_whc *whc_generic, struct ddsi_whc_sample_iter *opaque_it)
{
  struct bwhc_iter *it = (struct bwhc_iter *) opaque_it;
  it->c.whc = (struct ddsi_whc *) whc_generic;
  it->st = BIS_INIT_LOCAL;
  it->have_sample = false;
}

static bool is_visible (const struct ddsi_entity_common *e)
{
  const ddsi_vendorid_t vendorid = ddsi_get_entity_vendorid (e);
  return ddsi_builtintopic_is_visible (e->gv->builtin_topic_interface, &e->guid, vendorid);
}

static bool bwhc_sample_iter_borrow_next_proxy_topic (struct bwhc_iter * const it, struct ddsi_whc_borrowed_sample *sample)
{
#ifdef DDS_HAS_TOPIC_DISCOVERY
  struct ddsi_proxy_topic *proxytp = NULL;

  /* If not first proxypp: get lock and get next topic from this proxypp */
  if (it->cur_proxypp != NULL)
  {
    ddsrt_mutex_lock (&it->cur_proxypp->e.lock);
    do
    {
      proxytp = ddsrt_avl_lookup_succ (&ddsi_proxypp_proxytp_treedef, &it->cur_proxypp->topics, &it->proxytp_eid);
      if (proxytp != NULL)
        it->proxytp_eid = proxytp->entityid;
    } while (proxytp != NULL && proxytp->deleted);
  }
  while (proxytp == NULL)
  {
    /* no next topic available for this proxypp: if not first proxypp, return lock */
    if (it->cur_proxypp != NULL)
      ddsrt_mutex_unlock (&it->cur_proxypp->e.lock);

    /* enum next proxypp (if available) and get lock */
    if ((it->cur_proxypp = (struct ddsi_proxy_participant *) ddsi_entidx_enum_next (&it->it)) == NULL)
      return false;
    ddsrt_mutex_lock (&it->cur_proxypp->e.lock);

    /* get first (non-deleted) topic for this proxypp */
    ddsi_entityid_t eid = { .u = 0 };
    proxytp = ddsrt_avl_lookup_succ (&ddsi_proxypp_proxytp_treedef, &it->cur_proxypp->topics, &eid);
    while (proxytp != NULL && proxytp->deleted)
      proxytp = ddsrt_avl_lookup_succ (&ddsi_proxypp_proxytp_treedef, &it->cur_proxypp->topics, &proxytp->entityid);
    if (proxytp != NULL)
      it->proxytp_eid = proxytp->entityid;
  }
  /* next topic found, make sample and release proxypp lock */
  sample->serdata = dds__builtin_make_sample_proxy_topic (proxytp, proxytp->tupdate, true);
  it->have_sample = true;
  ddsrt_mutex_unlock (&it->cur_proxypp->e.lock);
#else
  (void) it; (void) sample;
#endif
  return true;
}

static void init_proxy_topic_iteration (struct bwhc_iter * const it)
{
#ifdef DDS_HAS_TOPIC_DISCOVERY
  struct bwhc * const whc = (struct bwhc *) it->c.whc;
  /* proxy topics are not stored in entity index as these are not real
     entities. For proxy topics loop over all proxy participants and
     iterate all proxy topics for each proxy participant*/
  ddsi_entidx_enum_init (&it->it, whc->entidx, DDSI_EK_PROXY_PARTICIPANT);
  it->cur_proxypp = NULL;
#else
  (void) it;
#endif
}

static struct ddsi_serdata *make_sample (struct ddsi_entity_common *entity)
{
  if (entity->kind == DDSI_EK_TOPIC)
  {
#ifdef DDS_HAS_TOPIC_DISCOVERY
    return dds__builtin_make_sample_topic (entity, entity->tupdate, true);
#else
    assert (0);
    return NULL;
#endif
  }
  else
  {
    return dds__builtin_make_sample_endpoint (entity, entity->tupdate, true);
  }
}

static bool bwhc_sample_iter_borrow_next (struct ddsi_whc_sample_iter *opaque_it, struct ddsi_whc_borrowed_sample *sample)
{
  struct bwhc_iter * const it = (struct bwhc_iter *) opaque_it;
  struct bwhc * const whc = (struct bwhc *) it->c.whc;
  enum ddsi_entity_kind kind = DDSI_EK_PARTICIPANT; /* pacify gcc */
  struct ddsi_entity_common *entity = NULL;

  if (it->have_sample)
  {
    ddsi_serdata_unref (sample->serdata);
    it->have_sample = false;
  }

  /* most fields really don't matter, so memset */
  memset (sample, 0, sizeof (*sample));

  switch (it->st)
  {
    case BIS_INIT_LOCAL:
      switch (whc->entity_kind) {
        case DSBT_PARTICIPANT: kind = DDSI_EK_PARTICIPANT; break;
        case DSBT_TOPIC:       kind = DDSI_EK_TOPIC; break;
        case DSBT_WRITER:      kind = DDSI_EK_WRITER; break;
        case DSBT_READER:      kind = DDSI_EK_READER; break;
      }
      assert (whc->entity_kind == DSBT_PARTICIPANT || kind != DDSI_EK_PARTICIPANT);
      ddsi_entidx_enum_init (&it->it, whc->entidx, kind);
      it->st = BIS_LOCAL;
      /* FALLS THROUGH */
    case BIS_LOCAL:
      while ((entity = ddsi_entidx_enum_next (&it->it)) != NULL)
        if (is_visible (entity))
          break;
      if (entity)
      {
        sample->serdata = make_sample (entity);
        it->have_sample = true;
        return true;
      }
      ddsi_entidx_enum_fini (&it->it);
      it->st = BIS_INIT_PROXY;
      /* FALLS THROUGH */
    case BIS_INIT_PROXY:
      if (whc->entity_kind == DSBT_TOPIC)
        init_proxy_topic_iteration (it);
      else
      {
        switch (whc->entity_kind)
        {
          case DSBT_PARTICIPANT: kind = DDSI_EK_PROXY_PARTICIPANT; break;
          case DSBT_TOPIC:       assert (0); break;
          case DSBT_WRITER:      kind = DDSI_EK_PROXY_WRITER; break;
          case DSBT_READER:      kind = DDSI_EK_PROXY_READER; break;
        }
        assert (kind != DDSI_EK_PARTICIPANT);
        ddsi_entidx_enum_init (&it->it, whc->entidx, kind);
      }

      it->st = BIS_PROXY;
      /* FALLS THROUGH */
    case BIS_PROXY:
      if (whc->entity_kind == DSBT_TOPIC)
        return bwhc_sample_iter_borrow_next_proxy_topic (it, sample);
      else
      {
        while ((entity = ddsi_entidx_enum_next (&it->it)) != NULL)
          if (is_visible (entity))
            break;
        if (!entity)
        {
          ddsi_entidx_enum_fini (&it->it);
          return false;
        }
        sample->serdata = dds__builtin_make_sample_endpoint (entity, entity->tupdate, true);
        it->have_sample = true;
        return true;
      }
  }
  assert (0);
  return false;
}

static void bwhc_get_state (const struct ddsi_whc *whc, struct ddsi_whc_state *st)
{
  (void)whc;
  st->max_seq = 0;
  st->min_seq = 0;
  st->unacked_bytes = 0;
}

static int bwhc_insert (struct ddsi_whc *whc, ddsi_seqno_t max_drop_seq, ddsi_seqno_t seq, ddsrt_mtime_t exp, struct ddsi_serdata *serdata, struct ddsi_tkmap_instance *tk)
{
  (void)whc;
  (void)max_drop_seq;
  (void)seq;
  (void)exp;
  (void)serdata;
  (void)tk;
  return 0;
}

static uint32_t bwhc_remove_acked_messages (struct ddsi_whc *whc, ddsi_seqno_t max_drop_seq, struct ddsi_whc_state *whcst, struct ddsi_whc_node **deferred_free_list)
{
  (void)whc;
  (void)max_drop_seq;
  (void)whcst;
  *deferred_free_list = NULL;
  return 0;
}

static void bwhc_free_deferred_free_list (struct ddsi_whc *whc, struct ddsi_whc_node *deferred_free_list)
{
  (void)whc;
  (void)deferred_free_list;
}

static const struct ddsi_whc_ops bwhc_ops = {
  .insert = bwhc_insert,
  .remove_acked_messages = bwhc_remove_acked_messages,
  .free_deferred_free_list = bwhc_free_deferred_free_list,
  .get_state = bwhc_get_state,
  .next_seq = 0,
  .borrow_sample = 0,
  .borrow_sample_key = 0,
  .return_sample = 0,
  .sample_iter_init = bwhc_sample_iter_init,
  .sample_iter_borrow_next = bwhc_sample_iter_borrow_next,
  .free = bwhc_free
};

struct ddsi_whc *dds_builtintopic_whc_new (enum ddsi_sertype_builtintopic_entity_kind entity_kind, const struct ddsi_entity_index *entidx)
{
  struct bwhc *whc = ddsrt_malloc (sizeof (*whc));
  whc->common.ops = &bwhc_ops;
  whc->entity_kind = entity_kind;
  whc->entidx = entidx;
  return (struct ddsi_whc *) whc;
}
