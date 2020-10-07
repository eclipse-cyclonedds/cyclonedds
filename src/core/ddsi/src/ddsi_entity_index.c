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
#include <stddef.h>
#include <assert.h>
#include <string.h>

#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/misc.h"

#include "dds/ddsrt/hopscotch.h"
#include "dds/ddsrt/avl.h"
#include "dds/ddsi/ddsi_entity_index.h"
#include "dds/ddsi/q_config.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsi/q_entity.h"
#include "dds/ddsi/q_gc.h"
#include "dds/ddsi/q_rtps.h" /* guid_t */
#include "dds/ddsi/q_thread.h" /* for assert(thread is awake) */

struct entity_index {
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
  DDSRT_AVL_TREEDEF_INITIALIZER (offsetof (struct entity_common, all_entities_avlnode), 0, all_entities_compare, 0);

static uint32_t hash_entity_guid (const struct entity_common *c)
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

static int entity_guid_eq (const struct entity_common *a, const struct entity_common *b)
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
  const struct entity_common *a = va;
  const struct entity_common *b = vb;
  const char *tp_a = "";
  const char *tp_b = "";
  int cmpres;

  if (a->kind != b->kind)
    return (int) a->kind - (int) b->kind;

  switch (a->kind)
  {
    case EK_PARTICIPANT:
    case EK_PROXY_PARTICIPANT:
      break;

    case EK_TOPIC: {
#ifdef DDS_HAS_TOPIC_DISCOVERY
      const struct topic *tpa = va;
      const struct topic *tpb = vb;
      assert ((tpa->definition->xqos->present & QP_TOPIC_NAME) && tpa->definition->xqos->topic_name);
      assert ((tpb->definition->xqos->present & QP_TOPIC_NAME) && tpb->definition->xqos->topic_name);
      tp_a = tpa->definition->xqos->topic_name;
      tp_b = tpb->definition->xqos->topic_name;
      break;
#endif
    }

    case EK_WRITER: {
      const struct writer *wra = va;
      const struct writer *wrb = vb;
      assert ((wra->xqos->present & QP_TOPIC_NAME) && wra->xqos->topic_name);
      assert ((wrb->xqos->present & QP_TOPIC_NAME) && wrb->xqos->topic_name);
      tp_a = wra->xqos->topic_name;
      tp_b = wrb->xqos->topic_name;
      break;
    }

    case EK_READER: {
      const struct reader *rda = va;
      const struct reader *rdb = vb;
      assert ((rda->xqos->present & QP_TOPIC_NAME) && rda->xqos->topic_name);
      assert ((rdb->xqos->present & QP_TOPIC_NAME) && rdb->xqos->topic_name);
      tp_a = rda->xqos->topic_name;
      tp_b = rdb->xqos->topic_name;
      break;
    }

    case EK_PROXY_WRITER:
    case EK_PROXY_READER: {
      const struct generic_proxy_endpoint *ga = va;
      const struct generic_proxy_endpoint *gb = vb;
      assert ((ga->c.xqos->present & QP_TOPIC_NAME) && ga->c.xqos->topic_name);
      assert ((gb->c.xqos->present & QP_TOPIC_NAME) && gb->c.xqos->topic_name);
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

static void match_endpoint_range (enum entity_kind kind, const char *tp, struct match_entities_range_key *min, struct match_entities_range_key *max)
{
  /* looking for entities of kind KIND; initialize fake entities such that they are
     valid input to all_entities_compare and that span the range of all possibly
     matching endpoints. */
  min->entity.e.kind = max->entity.e.kind = kind;
  memset (&min->entity.e.guid, 0x00, sizeof (min->entity.e.guid));
  memset (&max->entity.e.guid, 0xff, sizeof (max->entity.e.guid));
  min->xqos.present = max->xqos.present = QP_TOPIC_NAME;
  min->xqos.topic_name = max->xqos.topic_name = (char *) tp;
  switch (kind)
  {
    case EK_PARTICIPANT:
    case EK_PROXY_PARTICIPANT:
      break;
    case EK_TOPIC:
#ifdef DDS_HAS_TOPIC_DISCOVERY
      min->entity.tp.definition = &min->tpdef;
      max->entity.tp.definition = &max->tpdef;
      min->entity.tp.definition->xqos = &min->xqos;
      max->entity.tp.definition->xqos = &max->xqos;
#endif
      break;
    case EK_WRITER:
      min->entity.wr.xqos = &min->xqos;
      max->entity.wr.xqos = &max->xqos;
      break;
    case EK_READER:
      min->entity.rd.xqos = &min->xqos;
      max->entity.rd.xqos = &max->xqos;
      break;
    case EK_PROXY_WRITER:
    case EK_PROXY_READER:
      min->entity.gpe.c.vendor = max->entity.gpe.c.vendor = NN_VENDORID_ECLIPSE;
      min->entity.gpe.c.xqos = &min->xqos;
      max->entity.gpe.c.xqos = &max->xqos;
      break;
  }
}

static void match_entity_kind_min (enum entity_kind kind, struct match_entities_range_key *min)
{
  /* looking for entities of kind KIND; initialize fake entities such that they are
     valid input to all_entities_compare and that span the range of all possibly
     matching endpoints. */
  min->entity.e.kind = kind;
  memset (&min->entity.e.guid, 0x00, sizeof (min->entity.e.guid));
  min->xqos.present = QP_TOPIC_NAME;
  min->xqos.topic_name = "";
  switch (kind)
  {
    case EK_PARTICIPANT:
    case EK_PROXY_PARTICIPANT:
      break;
    case EK_TOPIC:
#ifdef DDS_HAS_TOPIC_DISCOVERY
      min->entity.tp.definition = &min->tpdef;
      min->entity.tp.definition->xqos = &min->xqos;
#endif
      break;
    case EK_WRITER:
      min->entity.wr.xqos = &min->xqos;
      break;
    case EK_READER:
      min->entity.rd.xqos = &min->xqos;
      break;
    case EK_PROXY_WRITER:
    case EK_PROXY_READER:
      min->entity.gpe.c.vendor = NN_VENDORID_ECLIPSE;
      min->entity.gpe.c.xqos = &min->xqos;
      break;
    default:
      assert (0);
      break;
  }
}

static void gc_buckets_cb (struct gcreq *gcreq)
{
  void *bs = gcreq->arg;
  gcreq_free (gcreq);
  ddsrt_free (bs);
}

static void gc_buckets (void *bs, void *varg)
{
  struct ddsi_domaingv *gv = varg;
  struct gcreq *gcreq = gcreq_new (gv->gcreq_queue, gc_buckets_cb);
  gcreq->arg = bs;
  gcreq_enqueue (gcreq);
}

struct entity_index *entity_index_new (struct ddsi_domaingv *gv)
{
  struct entity_index *entidx;
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

void entity_index_free (struct entity_index *entidx)
{
  ddsrt_avl_free (&all_entities_treedef, &entidx->all_entities, 0);
  ddsrt_mutex_destroy (&entidx->all_entities_lock);
  ddsrt_chh_free (entidx->guid_hash);
  entidx->guid_hash = NULL;
  ddsrt_free (entidx);
}

static void add_to_all_entities (struct entity_index *ei, struct entity_common *e)
{
  ddsrt_mutex_lock (&ei->all_entities_lock);
  assert (ddsrt_avl_lookup (&all_entities_treedef, &ei->all_entities, e) == NULL);
  ddsrt_avl_insert (&all_entities_treedef, &ei->all_entities, e);
  ddsrt_mutex_unlock (&ei->all_entities_lock);
}

static void remove_from_all_entities (struct entity_index *ei, struct entity_common *e)
{
  ddsrt_mutex_lock (&ei->all_entities_lock);
  assert (ddsrt_avl_lookup (&all_entities_treedef, &ei->all_entities, e) != NULL);
  ddsrt_avl_delete (&all_entities_treedef, &ei->all_entities, e);
  ddsrt_mutex_unlock (&ei->all_entities_lock);
}

static void entity_index_insert (struct entity_index *ei, struct entity_common *e)
{
  int x;
  x = ddsrt_chh_add (ei->guid_hash, e);
  (void)x;
  assert (x);
  add_to_all_entities (ei, e);
}

static void entity_index_remove (struct entity_index *ei, struct entity_common *e)
{
  int x;
  remove_from_all_entities (ei, e);
  x = ddsrt_chh_remove (ei->guid_hash, e);
  (void)x;
  assert (x);
}

void *entidx_lookup_guid_untyped (const struct entity_index *ei, const struct ddsi_guid *guid)
{
  /* FIXME: could (now) require guid to be first in entity_common; entity_common already is first in entity */
  struct entity_common e;
  e.guid = *guid;
  assert (thread_is_awake ());
  return ddsrt_chh_lookup (ei->guid_hash, &e);
}

static void *entidx_lookup_guid_int (const struct entity_index *ei, const struct ddsi_guid *guid, enum entity_kind kind)
{
  struct entity_common *res;
  if ((res = entidx_lookup_guid_untyped (ei, guid)) != NULL && res->kind == kind)
    return res;
  else
    return NULL;
}

void *entidx_lookup_guid (const struct entity_index *ei, const struct ddsi_guid *guid, enum entity_kind kind)
{
  return entidx_lookup_guid_int (ei, guid, kind);
}

void entidx_insert_participant_guid (struct entity_index *ei, struct participant *pp)
{
  entity_index_insert (ei, &pp->e);
}

void entidx_insert_proxy_participant_guid (struct entity_index *ei, struct proxy_participant *proxypp)
{
  entity_index_insert (ei, &proxypp->e);
}

void entidx_insert_writer_guid (struct entity_index *ei, struct writer *wr)
{
  entity_index_insert (ei, &wr->e);
}

void entidx_insert_reader_guid (struct entity_index *ei, struct reader *rd)
{
  entity_index_insert (ei, &rd->e);
}

void entidx_insert_proxy_writer_guid (struct entity_index *ei, struct proxy_writer *pwr)
{
  entity_index_insert (ei, &pwr->e);
}

void entidx_insert_proxy_reader_guid (struct entity_index *ei, struct proxy_reader *prd)
{
  entity_index_insert (ei, &prd->e);
}

void entidx_remove_participant_guid (struct entity_index *ei, struct participant *pp)
{
  entity_index_remove (ei, &pp->e);
}

void entidx_remove_proxy_participant_guid (struct entity_index *ei, struct proxy_participant *proxypp)
{
  entity_index_remove (ei, &proxypp->e);
}

void entidx_remove_writer_guid (struct entity_index *ei, struct writer *wr)
{
  entity_index_remove (ei, &wr->e);
}

void entidx_remove_reader_guid (struct entity_index *ei, struct reader *rd)
{
  entity_index_remove (ei, &rd->e);
}

void entidx_remove_proxy_writer_guid (struct entity_index *ei, struct proxy_writer *pwr)
{
  entity_index_remove (ei, &pwr->e);
}

void entidx_remove_proxy_reader_guid (struct entity_index *ei, struct proxy_reader *prd)
{
  entity_index_remove (ei, &prd->e);
}

struct participant *entidx_lookup_participant_guid (const struct entity_index *ei, const struct ddsi_guid *guid)
{
  DDSRT_STATIC_ASSERT (offsetof (struct participant, e) == 0);
  assert (guid->entityid.u == NN_ENTITYID_PARTICIPANT);
  return entidx_lookup_guid_int (ei, guid, EK_PARTICIPANT);
}

struct proxy_participant *entidx_lookup_proxy_participant_guid (const struct entity_index *ei, const struct ddsi_guid *guid)
{
  DDSRT_STATIC_ASSERT (offsetof (struct proxy_participant, e) == 0);
  assert (guid->entityid.u == NN_ENTITYID_PARTICIPANT);
  return entidx_lookup_guid_int (ei, guid, EK_PROXY_PARTICIPANT);
}

struct writer *entidx_lookup_writer_guid (const struct entity_index *ei, const struct ddsi_guid *guid)
{
  DDSRT_STATIC_ASSERT (offsetof (struct writer, e) == 0);
  assert (is_writer_entityid (guid->entityid));
  return entidx_lookup_guid_int (ei, guid, EK_WRITER);
}

struct reader *entidx_lookup_reader_guid (const struct entity_index *ei, const struct ddsi_guid *guid)
{
  DDSRT_STATIC_ASSERT (offsetof (struct reader, e) == 0);
  assert (is_reader_entityid (guid->entityid));
  return entidx_lookup_guid_int (ei, guid, EK_READER);
}

struct proxy_writer *entidx_lookup_proxy_writer_guid (const struct entity_index *ei, const struct ddsi_guid *guid)
{
  DDSRT_STATIC_ASSERT (offsetof (struct proxy_writer, e) == 0);
  assert (is_writer_entityid (guid->entityid));
  return entidx_lookup_guid_int (ei, guid, EK_PROXY_WRITER);
}

struct proxy_reader *entidx_lookup_proxy_reader_guid (const struct entity_index *ei, const struct ddsi_guid *guid)
{
  DDSRT_STATIC_ASSERT (offsetof (struct proxy_reader, e) == 0);
  assert (is_reader_entityid (guid->entityid));
  return entidx_lookup_guid_int (ei, guid, EK_PROXY_READER);
}

/* Enumeration */

static void entidx_enum_init_minmax_int (struct entidx_enum *st, const struct entity_index *ei, const struct match_entities_range_key *min)
{
  /* Use a lock to protect against concurrent modification and rely on the GC not deleting
     any entities while enumerating so we can rely on the (kind, topic, GUID) triple to
     remain valid for looking up the next entity.  With a bit of additional effort it would
     be possible to allow the GC to reclaim any entities already visited, but I don't think
     that additional effort is worth it. */
#ifndef NDEBUG
  assert (thread_is_awake ());
  st->vtime = ddsrt_atomic_ld32 (&lookup_thread_state ()->vtime);
#endif
  st->entidx = (struct entity_index *) ei;
  st->kind = min->entity.e.kind;
  ddsrt_mutex_lock (&st->entidx->all_entities_lock);
  st->cur = ddsrt_avl_lookup_succ_eq (&all_entities_treedef, &st->entidx->all_entities, min);
  ddsrt_mutex_unlock (&st->entidx->all_entities_lock);
}

void entidx_enum_init_topic (struct entidx_enum *st, const struct entity_index *ei, enum entity_kind kind, const char *topic, struct match_entities_range_key *max)
{
  assert (kind == EK_READER || kind == EK_WRITER || kind == EK_PROXY_READER || kind == EK_PROXY_WRITER);
  struct match_entities_range_key min;
  match_endpoint_range (kind, topic, &min, max);
  entidx_enum_init_minmax_int (st, ei, &min);
  if (st->cur && all_entities_compare (st->cur, &max->entity) > 0)
    st->cur = NULL;
}

void entidx_enum_init_topic_w_prefix (struct entidx_enum *st, const struct entity_index *ei, enum entity_kind kind, const char *topic, const ddsi_guid_prefix_t *prefix, struct match_entities_range_key *max)
{
  assert (kind == EK_READER || kind == EK_WRITER || kind == EK_PROXY_READER || kind == EK_PROXY_WRITER);
  struct match_entities_range_key min;
  match_endpoint_range (kind, topic, &min, max);
  min.entity.e.guid.prefix = *prefix;
  max->entity.e.guid.prefix = *prefix;
  entidx_enum_init_minmax_int (st, ei, &min);
  if (st->cur && all_entities_compare (st->cur, &max->entity) > 0)
    st->cur = NULL;
}

void entidx_enum_init (struct entidx_enum *st, const struct entity_index *ei, enum entity_kind kind)
{
  struct match_entities_range_key min;
  match_entity_kind_min (kind, &min);
  entidx_enum_init_minmax_int (st, ei, &min);
  if (st->cur && st->cur->kind != st->kind)
    st->cur = NULL;
}

void entidx_enum_writer_init (struct entidx_enum_writer *st, const struct entity_index *ei)
{
  entidx_enum_init (&st->st, ei, EK_WRITER);
}

void entidx_enum_reader_init (struct entidx_enum_reader *st, const struct entity_index *ei)
{
  entidx_enum_init (&st->st, ei, EK_READER);
}

void entidx_enum_proxy_writer_init (struct entidx_enum_proxy_writer *st, const struct entity_index *ei)
{
  entidx_enum_init (&st->st, ei, EK_PROXY_WRITER);
}

void entidx_enum_proxy_reader_init (struct entidx_enum_proxy_reader *st, const struct entity_index *ei)
{
  entidx_enum_init (&st->st, ei, EK_PROXY_READER);
}

void entidx_enum_participant_init (struct entidx_enum_participant *st, const struct entity_index *ei)
{
  entidx_enum_init (&st->st, ei, EK_PARTICIPANT);
}

void entidx_enum_proxy_participant_init (struct entidx_enum_proxy_participant *st, const struct entity_index *ei)
{
  entidx_enum_init (&st->st, ei, EK_PROXY_PARTICIPANT);
}

void *entidx_enum_next (struct entidx_enum *st)
{
  /* st->cur can not have been freed yet, but it may have been removed from the index */
  assert (ddsrt_atomic_ld32 (&lookup_thread_state ()->vtime) == st->vtime);
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

void *entidx_enum_next_max (struct entidx_enum *st, const struct match_entities_range_key *max)
{
  void *res = entidx_enum_next (st);

  /* max may only make the bounds tighter */
  assert (max->entity.e.kind == st->kind);
  if (st->cur && all_entities_compare (st->cur, &max->entity) > 0)
    st->cur = NULL;
  return res;
}

struct writer *entidx_enum_writer_next (struct entidx_enum_writer *st)
{
  DDSRT_STATIC_ASSERT (offsetof (struct writer, e) == 0);
  return entidx_enum_next (&st->st);
}

struct reader *entidx_enum_reader_next (struct entidx_enum_reader *st)
{
  DDSRT_STATIC_ASSERT (offsetof (struct reader, e) == 0);
  return entidx_enum_next (&st->st);
}

struct proxy_writer *entidx_enum_proxy_writer_next (struct entidx_enum_proxy_writer *st)
{
  DDSRT_STATIC_ASSERT (offsetof (struct proxy_writer, e) == 0);
  return entidx_enum_next (&st->st);
}

struct proxy_reader *entidx_enum_proxy_reader_next (struct entidx_enum_proxy_reader *st)
{
  DDSRT_STATIC_ASSERT (offsetof (struct proxy_reader, e) == 0);
  return entidx_enum_next (&st->st);
}

struct participant *entidx_enum_participant_next (struct entidx_enum_participant *st)
{
  DDSRT_STATIC_ASSERT (offsetof (struct participant, e) == 0);
  return entidx_enum_next (&st->st);
}

struct proxy_participant *entidx_enum_proxy_participant_next (struct entidx_enum_proxy_participant *st)
{
  DDSRT_STATIC_ASSERT (offsetof (struct proxy_participant, e) == 0);
  return entidx_enum_next (&st->st);
}

void entidx_enum_fini (struct entidx_enum *st)
{
  assert (ddsrt_atomic_ld32 (&lookup_thread_state ()->vtime) == st->vtime);
  (void) st;
}

void entidx_enum_writer_fini (struct entidx_enum_writer *st)
{
  entidx_enum_fini (&st->st);
}

void entidx_enum_reader_fini (struct entidx_enum_reader *st)
{
  entidx_enum_fini (&st->st);
}

void entidx_enum_proxy_writer_fini (struct entidx_enum_proxy_writer *st)
{
  entidx_enum_fini (&st->st);
}

void entidx_enum_proxy_reader_fini (struct entidx_enum_proxy_reader *st)
{
  entidx_enum_fini (&st->st);
}

void entidx_enum_participant_fini (struct entidx_enum_participant *st)
{
  entidx_enum_fini (&st->st);
}

void entidx_enum_proxy_participant_fini (struct entidx_enum_proxy_participant *st)
{
  entidx_enum_fini (&st->st);
}

#ifdef DDS_HAS_TOPIC_DISCOVERY

void entidx_insert_topic_guid (struct entity_index *ei, struct topic *tp)
{
  entity_index_insert (ei, &tp->e);
}

void entidx_remove_topic_guid (struct entity_index *ei, struct topic *tp)
{
  entity_index_remove (ei, &tp->e);
}

struct topic *entidx_lookup_topic_guid (const struct entity_index *ei, const struct ddsi_guid *guid)
{
  DDSRT_STATIC_ASSERT (offsetof (struct topic, e) == 0);
  assert (is_topic_entityid (guid->entityid));
  return entidx_lookup_guid_int (ei, guid, EK_TOPIC);
}

void entidx_enum_topic_init (struct entidx_enum_topic *st, const struct entity_index *ei)
{
  entidx_enum_init (&st->st, ei, EK_TOPIC);
}

struct topic *entidx_enum_topic_next (struct entidx_enum_topic *st)
{
  DDSRT_STATIC_ASSERT (offsetof (struct topic, e) == 0);
  return entidx_enum_next (&st->st);
}

void entidx_enum_topic_fini (struct entidx_enum_topic *st)
{
  entidx_enum_fini (&st->st);
}

#endif /* DDS_HAS_TOPIC_DISCOVERY */
