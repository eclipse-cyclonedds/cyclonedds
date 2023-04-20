// Copyright(c) 2006 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <stddef.h>
#include <assert.h>
#include <string.h>

#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/misc.h"
#include "dds/ddsrt/hopscotch.h"
#include "dds/ddsrt/avl.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsi/ddsi_proxy_participant.h"
#include "dds/ddsi/ddsi_proxy_endpoint.h"
#include "dds/ddsi/ddsi_protocol.h"
#include "ddsi__entity_index.h"
#include "ddsi__entity.h"
#include "ddsi__participant.h"
#include "ddsi__thread.h" /* for assert(thread is awake) */
#include "ddsi__endpoint.h"
#include "ddsi__gc.h"
#include "ddsi__topic.h"
#include "ddsi__vendor.h"

struct ddsi_entity_index {
  struct ddsrt_chh *guid_hash;
  ddsrt_mutex_t all_entities_lock;
  ddsrt_avl_tree_t all_entities;
};

static const uint64_t unihashconsts[] = {
  UINT64_C (16292676669999574021),
  UINT64_C (10242350189706880077),
  UINT64_C (12844332200329132887),
  UINT64_C (16728792139623414127)
};

static int all_entities_compare (const void *va, const void *vb);
static const ddsrt_avl_treedef_t all_entities_treedef =
  DDSRT_AVL_TREEDEF_INITIALIZER (offsetof (struct ddsi_entity_common, all_entities_avlnode), 0, all_entities_compare, 0);

static uint32_t hash_entity_guid (const struct ddsi_entity_common *c)
{
  return
    (uint32_t) (((((uint32_t) c->guid.prefix.u[0] + unihashconsts[0]) *
                  ((uint32_t) c->guid.prefix.u[1] + unihashconsts[1])) +
                 (((uint32_t) c->guid.prefix.u[2] + unihashconsts[2]) *
                  ((uint32_t) c->guid.entityid.u  + unihashconsts[3])))
                >> 32);
}

static uint32_t hash_entity_guid_wrapper (const void *c)
{
  return hash_entity_guid (c);
}

static int entity_guid_eq (const struct ddsi_entity_common *a, const struct ddsi_entity_common *b)
{
  return
    a->guid.prefix.u[0] == b->guid.prefix.u[0] && a->guid.prefix.u[1] == b->guid.prefix.u[1] &&
    a->guid.prefix.u[2] == b->guid.prefix.u[2] && a->guid.entityid.u == b->guid.entityid.u;
}

static int entity_guid_eq_wrapper (const void *a, const void *b)
{
  return entity_guid_eq (a, b);
}

static int all_entities_compare (const void *va, const void *vb)
{
  const struct ddsi_entity_common *a = va;
  const struct ddsi_entity_common *b = vb;
  const char *tp_a = "";
  const char *tp_b = "";
  int cmpres;

  if (a->kind != b->kind)
    return (int) a->kind - (int) b->kind;

  switch (a->kind)
  {
    case DDSI_EK_PARTICIPANT:
    case DDSI_EK_PROXY_PARTICIPANT:
      break;

    case DDSI_EK_TOPIC: {
#ifdef DDS_HAS_TOPIC_DISCOVERY
      const struct ddsi_topic *tpa = va;
      const struct ddsi_topic *tpb = vb;
      assert ((tpa->definition->xqos->present & DDSI_QP_TOPIC_NAME) && tpa->definition->xqos->topic_name);
      assert ((tpb->definition->xqos->present & DDSI_QP_TOPIC_NAME) && tpb->definition->xqos->topic_name);
      tp_a = tpa->definition->xqos->topic_name;
      tp_b = tpb->definition->xqos->topic_name;
      break;
#endif
    }

    case DDSI_EK_WRITER: {
      const struct ddsi_writer *wra = va;
      const struct ddsi_writer *wrb = vb;
      assert ((wra->xqos->present & DDSI_QP_TOPIC_NAME) && wra->xqos->topic_name);
      assert ((wrb->xqos->present & DDSI_QP_TOPIC_NAME) && wrb->xqos->topic_name);
      tp_a = wra->xqos->topic_name;
      tp_b = wrb->xqos->topic_name;
      break;
    }

    case DDSI_EK_READER: {
      const struct ddsi_reader *rda = va;
      const struct ddsi_reader *rdb = vb;
      assert ((rda->xqos->present & DDSI_QP_TOPIC_NAME) && rda->xqos->topic_name);
      assert ((rdb->xqos->present & DDSI_QP_TOPIC_NAME) && rdb->xqos->topic_name);
      tp_a = rda->xqos->topic_name;
      tp_b = rdb->xqos->topic_name;
      break;
    }

    case DDSI_EK_PROXY_WRITER:
    case DDSI_EK_PROXY_READER: {
      const struct ddsi_generic_proxy_endpoint *ga = va;
      const struct ddsi_generic_proxy_endpoint *gb = vb;
      assert ((ga->c.xqos->present & DDSI_QP_TOPIC_NAME) && ga->c.xqos->topic_name);
      assert ((gb->c.xqos->present & DDSI_QP_TOPIC_NAME) && gb->c.xqos->topic_name);
      tp_a = ga->c.xqos->topic_name;
      tp_b = gb->c.xqos->topic_name;
      break;
    }
  }

  if ((cmpres = strcmp (tp_a, tp_b)) != 0)
    return cmpres;
  else
    return memcmp (&a->guid, &b->guid, sizeof (a->guid));
}

static void match_endpoint_range (enum ddsi_entity_kind kind, const char *tp, struct ddsi_match_entities_range_key *min, struct ddsi_match_entities_range_key *max)
{
  /* looking for entities of kind KIND; initialize fake entities such that they are
     valid input to all_entities_compare and that span the range of all possibly
     matching endpoints. */
  min->entity.e.kind = max->entity.e.kind = kind;
  memset (&min->entity.e.guid, 0x00, sizeof (min->entity.e.guid));
  memset (&max->entity.e.guid, 0xff, sizeof (max->entity.e.guid));
  min->xqos.present = max->xqos.present = DDSI_QP_TOPIC_NAME;
  min->xqos.topic_name = max->xqos.topic_name = (char *) tp;
  switch (kind)
  {
    case DDSI_EK_PARTICIPANT:
    case DDSI_EK_PROXY_PARTICIPANT:
      break;
    case DDSI_EK_TOPIC:
#ifdef DDS_HAS_TOPIC_DISCOVERY
      min->entity.tp.definition = &min->tpdef;
      max->entity.tp.definition = &max->tpdef;
      min->entity.tp.definition->xqos = &min->xqos;
      max->entity.tp.definition->xqos = &max->xqos;
#endif
      break;
    case DDSI_EK_WRITER:
      min->entity.wr.xqos = &min->xqos;
      max->entity.wr.xqos = &max->xqos;
      break;
    case DDSI_EK_READER:
      min->entity.rd.xqos = &min->xqos;
      max->entity.rd.xqos = &max->xqos;
      break;
    case DDSI_EK_PROXY_WRITER:
    case DDSI_EK_PROXY_READER:
      min->entity.gpe.c.vendor = max->entity.gpe.c.vendor = DDSI_VENDORID_ECLIPSE;
      min->entity.gpe.c.xqos = &min->xqos;
      max->entity.gpe.c.xqos = &max->xqos;
      break;
  }
}

static void match_entity_kind_min (enum ddsi_entity_kind kind, struct ddsi_match_entities_range_key *min)
{
  /* looking for entities of kind KIND; initialize fake entities such that they are
     valid input to all_entities_compare and that span the range of all possibly
     matching endpoints. */
  min->entity.e.kind = kind;
  memset (&min->entity.e.guid, 0x00, sizeof (min->entity.e.guid));
  min->xqos.present = DDSI_QP_TOPIC_NAME;
  min->xqos.topic_name = "";
  switch (kind)
  {
    case DDSI_EK_PARTICIPANT:
    case DDSI_EK_PROXY_PARTICIPANT:
      break;
    case DDSI_EK_TOPIC:
#ifdef DDS_HAS_TOPIC_DISCOVERY
      min->entity.tp.definition = &min->tpdef;
      min->entity.tp.definition->xqos = &min->xqos;
#endif
      break;
    case DDSI_EK_WRITER:
      min->entity.wr.xqos = &min->xqos;
      break;
    case DDSI_EK_READER:
      min->entity.rd.xqos = &min->xqos;
      break;
    case DDSI_EK_PROXY_WRITER:
    case DDSI_EK_PROXY_READER:
      min->entity.gpe.c.vendor = DDSI_VENDORID_ECLIPSE;
      min->entity.gpe.c.xqos = &min->xqos;
      break;
    default:
      assert (0);
      break;
  }
}

static void gc_buckets_cb (struct ddsi_gcreq *gcreq)
{
  void *bs = gcreq->arg;
  ddsi_gcreq_free (gcreq);
  ddsrt_free (bs);
}

static void gc_buckets (void *bs, void *varg)
{
  struct ddsi_domaingv *gv = varg;
  struct ddsi_gcreq *gcreq = ddsi_gcreq_new (gv->gcreq_queue, gc_buckets_cb);
  gcreq->arg = bs;
  ddsi_gcreq_enqueue (gcreq);
}

struct ddsi_entity_index *ddsi_entity_index_new (struct ddsi_domaingv *gv)
{
  struct ddsi_entity_index *entidx;
  entidx = ddsrt_malloc (sizeof (*entidx));
  entidx->guid_hash = ddsrt_chh_new (32, hash_entity_guid_wrapper, entity_guid_eq_wrapper, gc_buckets, gv);
  if (entidx->guid_hash == NULL) {
    ddsrt_free (entidx);
    return NULL;
  } else {
    ddsrt_mutex_init (&entidx->all_entities_lock);
    ddsrt_avl_init (&all_entities_treedef, &entidx->all_entities);
    return entidx;
  }
}

void ddsi_entity_index_free (struct ddsi_entity_index *entidx)
{
  ddsrt_avl_free (&all_entities_treedef, &entidx->all_entities, 0);
  ddsrt_mutex_destroy (&entidx->all_entities_lock);
  ddsrt_chh_free (entidx->guid_hash);
  entidx->guid_hash = NULL;
  ddsrt_free (entidx);
}

static void add_to_all_entities (struct ddsi_entity_index *ei, struct ddsi_entity_common *e)
{
  ddsrt_mutex_lock (&ei->all_entities_lock);
  assert (ddsrt_avl_lookup (&all_entities_treedef, &ei->all_entities, e) == NULL);
  ddsrt_avl_insert (&all_entities_treedef, &ei->all_entities, e);
  ddsrt_mutex_unlock (&ei->all_entities_lock);
}

static void remove_from_all_entities (struct ddsi_entity_index *ei, struct ddsi_entity_common *e)
{
  ddsrt_mutex_lock (&ei->all_entities_lock);
  assert (ddsrt_avl_lookup (&all_entities_treedef, &ei->all_entities, e) != NULL);
  ddsrt_avl_delete (&all_entities_treedef, &ei->all_entities, e);
  ddsrt_mutex_unlock (&ei->all_entities_lock);
}

static void entity_index_insert (struct ddsi_entity_index *ei, struct ddsi_entity_common *e)
{
  int x;
  x = ddsrt_chh_add (ei->guid_hash, e);
  (void)x;
  assert (x);
  add_to_all_entities (ei, e);
}

static void entity_index_remove (struct ddsi_entity_index *ei, struct ddsi_entity_common *e)
{
  int x;
  remove_from_all_entities (ei, e);
  x = ddsrt_chh_remove (ei->guid_hash, e);
  (void)x;
  assert (x);
}

void *ddsi_entidx_lookup_guid_untyped (const struct ddsi_entity_index *ei, const struct ddsi_guid *guid)
{
  /* FIXME: could (now) require guid to be first in entity_common; entity_common already is first in entity */
  struct ddsi_entity_common e;
  e.guid = *guid;
  assert (ddsi_thread_is_awake ());
  return ddsrt_chh_lookup (ei->guid_hash, &e);
}

static void *entidx_lookup_guid_int (const struct ddsi_entity_index *ei, const struct ddsi_guid *guid, enum ddsi_entity_kind kind)
{
  struct ddsi_entity_common *res;
  if ((res = ddsi_entidx_lookup_guid_untyped (ei, guid)) != NULL && res->kind == kind)
    return res;
  else
    return NULL;
}

void *ddsi_entidx_lookup_guid (const struct ddsi_entity_index *ei, const struct ddsi_guid *guid, enum ddsi_entity_kind kind)
{
  return entidx_lookup_guid_int (ei, guid, kind);
}

void ddsi_entidx_insert_participant_guid (struct ddsi_entity_index *ei, struct ddsi_participant *pp)
{
  entity_index_insert (ei, &pp->e);
}

void ddsi_entidx_insert_proxy_participant_guid (struct ddsi_entity_index *ei, struct ddsi_proxy_participant *proxypp)
{
  entity_index_insert (ei, &proxypp->e);
}

void ddsi_entidx_insert_writer_guid (struct ddsi_entity_index *ei, struct ddsi_writer *wr)
{
  entity_index_insert (ei, &wr->e);
}

void ddsi_entidx_insert_reader_guid (struct ddsi_entity_index *ei, struct ddsi_reader *rd)
{
  entity_index_insert (ei, &rd->e);
}

void ddsi_entidx_insert_proxy_writer_guid (struct ddsi_entity_index *ei, struct ddsi_proxy_writer *pwr)
{
  entity_index_insert (ei, &pwr->e);
}

void ddsi_entidx_insert_proxy_reader_guid (struct ddsi_entity_index *ei, struct ddsi_proxy_reader *prd)
{
  entity_index_insert (ei, &prd->e);
}

void ddsi_entidx_remove_participant_guid (struct ddsi_entity_index *ei, struct ddsi_participant *pp)
{
  entity_index_remove (ei, &pp->e);
}

void ddsi_entidx_remove_proxy_participant_guid (struct ddsi_entity_index *ei, struct ddsi_proxy_participant *proxypp)
{
  entity_index_remove (ei, &proxypp->e);
}

void ddsi_entidx_remove_writer_guid (struct ddsi_entity_index *ei, struct ddsi_writer *wr)
{
  entity_index_remove (ei, &wr->e);
}

void ddsi_entidx_remove_reader_guid (struct ddsi_entity_index *ei, struct ddsi_reader *rd)
{
  entity_index_remove (ei, &rd->e);
}

void ddsi_entidx_remove_proxy_writer_guid (struct ddsi_entity_index *ei, struct ddsi_proxy_writer *pwr)
{
  entity_index_remove (ei, &pwr->e);
}

void ddsi_entidx_remove_proxy_reader_guid (struct ddsi_entity_index *ei, struct ddsi_proxy_reader *prd)
{
  entity_index_remove (ei, &prd->e);
}

struct ddsi_participant *ddsi_entidx_lookup_participant_guid (const struct ddsi_entity_index *ei, const struct ddsi_guid *guid)
{
  DDSRT_STATIC_ASSERT (offsetof (struct ddsi_participant, e) == 0);
  assert (guid->entityid.u == DDSI_ENTITYID_PARTICIPANT);
  return entidx_lookup_guid_int (ei, guid, DDSI_EK_PARTICIPANT);
}

struct ddsi_proxy_participant *ddsi_entidx_lookup_proxy_participant_guid (const struct ddsi_entity_index *ei, const struct ddsi_guid *guid)
{
  DDSRT_STATIC_ASSERT (offsetof (struct ddsi_proxy_participant, e) == 0);
  assert (guid->entityid.u == DDSI_ENTITYID_PARTICIPANT);
  return entidx_lookup_guid_int (ei, guid, DDSI_EK_PROXY_PARTICIPANT);
}

struct ddsi_writer *ddsi_entidx_lookup_writer_guid (const struct ddsi_entity_index *ei, const struct ddsi_guid *guid)
{
  DDSRT_STATIC_ASSERT (offsetof (struct ddsi_writer, e) == 0);
  assert (ddsi_is_writer_entityid (guid->entityid));
  return entidx_lookup_guid_int (ei, guid, DDSI_EK_WRITER);
}

struct ddsi_reader *ddsi_entidx_lookup_reader_guid (const struct ddsi_entity_index *ei, const struct ddsi_guid *guid)
{
  DDSRT_STATIC_ASSERT (offsetof (struct ddsi_reader, e) == 0);
  assert (ddsi_is_reader_entityid (guid->entityid));
  return entidx_lookup_guid_int (ei, guid, DDSI_EK_READER);
}

struct ddsi_proxy_writer *ddsi_entidx_lookup_proxy_writer_guid (const struct ddsi_entity_index *ei, const struct ddsi_guid *guid)
{
  DDSRT_STATIC_ASSERT (offsetof (struct ddsi_proxy_writer, e) == 0);
  assert (ddsi_is_writer_entityid (guid->entityid));
  return entidx_lookup_guid_int (ei, guid, DDSI_EK_PROXY_WRITER);
}

struct ddsi_proxy_reader *ddsi_entidx_lookup_proxy_reader_guid (const struct ddsi_entity_index *ei, const struct ddsi_guid *guid)
{
  DDSRT_STATIC_ASSERT (offsetof (struct ddsi_proxy_reader, e) == 0);
  assert (ddsi_is_reader_entityid (guid->entityid));
  return entidx_lookup_guid_int (ei, guid, DDSI_EK_PROXY_READER);
}

/* Enumeration */

static void entidx_enum_init_minmax_int (struct ddsi_entity_enum *st, const struct ddsi_entity_index *ei, const struct ddsi_match_entities_range_key *min)
{
  /* Use a lock to protect against concurrent modification and rely on the GC not deleting
     any entities while enumerating so we can rely on the (kind, topic, GUID) triple to
     remain valid for looking up the next entity.  With a bit of additional effort it would
     be possible to allow the GC to reclaim any entities already visited, but I don't think
     that additional effort is worth it. */
#ifndef NDEBUG
  assert (ddsi_thread_is_awake ());
  st->vtime = ddsrt_atomic_ld32 (&ddsi_lookup_thread_state ()->vtime);
#endif
  st->entidx = (struct ddsi_entity_index *) ei;
  st->kind = min->entity.e.kind;
  ddsrt_mutex_lock (&st->entidx->all_entities_lock);
  st->cur = ddsrt_avl_lookup_succ_eq (&all_entities_treedef, &st->entidx->all_entities, min);
  ddsrt_mutex_unlock (&st->entidx->all_entities_lock);
}

void ddsi_entidx_enum_init_topic (struct ddsi_entity_enum *st, const struct ddsi_entity_index *ei, enum ddsi_entity_kind kind, const char *topic, struct ddsi_match_entities_range_key *max)
{
  assert (kind == DDSI_EK_READER || kind == DDSI_EK_WRITER || kind == DDSI_EK_PROXY_READER || kind == DDSI_EK_PROXY_WRITER);
  struct ddsi_match_entities_range_key min;
  match_endpoint_range (kind, topic, &min, max);
  entidx_enum_init_minmax_int (st, ei, &min);
  if (st->cur && all_entities_compare (st->cur, &max->entity) > 0)
    st->cur = NULL;
}

void ddsi_entidx_enum_init_topic_w_prefix (struct ddsi_entity_enum *st, const struct ddsi_entity_index *ei, enum ddsi_entity_kind kind, const char *topic, const ddsi_guid_prefix_t *prefix, struct ddsi_match_entities_range_key *max)
{
  assert (kind == DDSI_EK_READER || kind == DDSI_EK_WRITER || kind == DDSI_EK_PROXY_READER || kind == DDSI_EK_PROXY_WRITER);
  struct ddsi_match_entities_range_key min;
  match_endpoint_range (kind, topic, &min, max);
  min.entity.e.guid.prefix = *prefix;
  max->entity.e.guid.prefix = *prefix;
  entidx_enum_init_minmax_int (st, ei, &min);
  if (st->cur && all_entities_compare (st->cur, &max->entity) > 0)
    st->cur = NULL;
}

void ddsi_entidx_enum_init (struct ddsi_entity_enum *st, const struct ddsi_entity_index *ei, enum ddsi_entity_kind kind)
{
  struct ddsi_match_entities_range_key min;
  match_entity_kind_min (kind, &min);
  entidx_enum_init_minmax_int (st, ei, &min);
  if (st->cur && st->cur->kind != st->kind)
    st->cur = NULL;
}

void ddsi_entidx_enum_writer_init (struct ddsi_entity_enum_writer *st, const struct ddsi_entity_index *ei)
{
  ddsi_entidx_enum_init (&st->st, ei, DDSI_EK_WRITER);
}

void ddsi_entidx_enum_reader_init (struct ddsi_entity_enum_reader *st, const struct ddsi_entity_index *ei)
{
  ddsi_entidx_enum_init (&st->st, ei, DDSI_EK_READER);
}

void ddsi_entidx_enum_proxy_writer_init (struct ddsi_entity_enum_proxy_writer *st, const struct ddsi_entity_index *ei)
{
  ddsi_entidx_enum_init (&st->st, ei, DDSI_EK_PROXY_WRITER);
}

void ddsi_entidx_enum_proxy_reader_init (struct ddsi_entity_enum_proxy_reader *st, const struct ddsi_entity_index *ei)
{
  ddsi_entidx_enum_init (&st->st, ei, DDSI_EK_PROXY_READER);
}

void ddsi_entidx_enum_participant_init (struct ddsi_entity_enum_participant *st, const struct ddsi_entity_index *ei)
{
  ddsi_entidx_enum_init (&st->st, ei, DDSI_EK_PARTICIPANT);
}

void ddsi_entidx_enum_proxy_participant_init (struct ddsi_entity_enum_proxy_participant *st, const struct ddsi_entity_index *ei)
{
  ddsi_entidx_enum_init (&st->st, ei, DDSI_EK_PROXY_PARTICIPANT);
}

void *ddsi_entidx_enum_next (struct ddsi_entity_enum *st)
{
  /* st->cur can not have been freed yet, but it may have been removed from the index */
  assert (ddsrt_atomic_ld32 (&ddsi_lookup_thread_state ()->vtime) == st->vtime);
  void *res = st->cur;
  if (st->cur)
  {
    ddsrt_mutex_lock (&st->entidx->all_entities_lock);
    st->cur = ddsrt_avl_lookup_succ (&all_entities_treedef, &st->entidx->all_entities, st->cur);
    ddsrt_mutex_unlock (&st->entidx->all_entities_lock);
    if (st->cur && st->cur->kind != st->kind)
      st->cur = NULL;
  }
  return res;
}

void *ddsi_entidx_enum_next_max (struct ddsi_entity_enum *st, const struct ddsi_match_entities_range_key *max)
{
  void *res = ddsi_entidx_enum_next (st);

  /* max may only make the bounds tighter */
  assert (max->entity.e.kind == st->kind);
  if (st->cur && all_entities_compare (st->cur, &max->entity) > 0)
    st->cur = NULL;
  return res;
}

struct ddsi_writer *ddsi_entidx_enum_writer_next (struct ddsi_entity_enum_writer *st)
{
  DDSRT_STATIC_ASSERT (offsetof (struct ddsi_writer, e) == 0);
  return ddsi_entidx_enum_next (&st->st);
}

struct ddsi_reader *ddsi_entidx_enum_reader_next (struct ddsi_entity_enum_reader *st)
{
  DDSRT_STATIC_ASSERT (offsetof (struct ddsi_reader, e) == 0);
  return ddsi_entidx_enum_next (&st->st);
}

struct ddsi_proxy_writer *ddsi_entidx_enum_proxy_writer_next (struct ddsi_entity_enum_proxy_writer *st)
{
  DDSRT_STATIC_ASSERT (offsetof (struct ddsi_proxy_writer, e) == 0);
  return ddsi_entidx_enum_next (&st->st);
}

struct ddsi_proxy_reader *ddsi_entidx_enum_proxy_reader_next (struct ddsi_entity_enum_proxy_reader *st)
{
  DDSRT_STATIC_ASSERT (offsetof (struct ddsi_proxy_reader, e) == 0);
  return ddsi_entidx_enum_next (&st->st);
}

struct ddsi_participant *ddsi_entidx_enum_participant_next (struct ddsi_entity_enum_participant *st)
{
  DDSRT_STATIC_ASSERT (offsetof (struct ddsi_participant, e) == 0);
  return ddsi_entidx_enum_next (&st->st);
}

struct ddsi_proxy_participant *ddsi_entidx_enum_proxy_participant_next (struct ddsi_entity_enum_proxy_participant *st)
{
  DDSRT_STATIC_ASSERT (offsetof (struct ddsi_proxy_participant, e) == 0);
  return ddsi_entidx_enum_next (&st->st);
}

void ddsi_entidx_enum_fini (struct ddsi_entity_enum *st)
{
  assert (ddsrt_atomic_ld32 (&ddsi_lookup_thread_state ()->vtime) == st->vtime);
  (void) st;
}

void ddsi_entidx_enum_writer_fini (struct ddsi_entity_enum_writer *st)
{
  ddsi_entidx_enum_fini (&st->st);
}

void ddsi_entidx_enum_reader_fini (struct ddsi_entity_enum_reader *st)
{
  ddsi_entidx_enum_fini (&st->st);
}

void ddsi_entidx_enum_proxy_writer_fini (struct ddsi_entity_enum_proxy_writer *st)
{
  ddsi_entidx_enum_fini (&st->st);
}

void ddsi_entidx_enum_proxy_reader_fini (struct ddsi_entity_enum_proxy_reader *st)
{
  ddsi_entidx_enum_fini (&st->st);
}

void ddsi_entidx_enum_participant_fini (struct ddsi_entity_enum_participant *st)
{
  ddsi_entidx_enum_fini (&st->st);
}

void ddsi_entidx_enum_proxy_participant_fini (struct ddsi_entity_enum_proxy_participant *st)
{
  ddsi_entidx_enum_fini (&st->st);
}

#ifdef DDS_HAS_TOPIC_DISCOVERY

void ddsi_entidx_insert_topic_guid (struct ddsi_entity_index *ei, struct ddsi_topic *tp)
{
  entity_index_insert (ei, &tp->e);
}

void ddsi_entidx_remove_topic_guid (struct ddsi_entity_index *ei, struct ddsi_topic *tp)
{
  entity_index_remove (ei, &tp->e);
}

struct ddsi_topic *ddsi_entidx_lookup_topic_guid (const struct ddsi_entity_index *ei, const struct ddsi_guid *guid)
{
  DDSRT_STATIC_ASSERT (offsetof (struct ddsi_topic, e) == 0);
  assert (ddsi_is_topic_entityid (guid->entityid));
  return entidx_lookup_guid_int (ei, guid, DDSI_EK_TOPIC);
}

void ddsi_entidx_enum_topic_init (struct ddsi_entity_enum_topic *st, const struct ddsi_entity_index *ei)
{
  ddsi_entidx_enum_init (&st->st, ei, DDSI_EK_TOPIC);
}

struct ddsi_topic *ddsi_entidx_enum_topic_next (struct ddsi_entity_enum_topic *st)
{
  DDSRT_STATIC_ASSERT (offsetof (struct ddsi_topic, e) == 0);
  return ddsi_entidx_enum_next (&st->st);
}

void ddsi_entidx_enum_topic_fini (struct ddsi_entity_enum_topic *st)
{
  ddsi_entidx_enum_fini (&st->st);
}

#endif /* DDS_HAS_TOPIC_DISCOVERY */
