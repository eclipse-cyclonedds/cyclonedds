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
#include "dds/ddsi/q_ephash.h"
#include "dds/ddsi/q_config.h"
#include "dds/ddsi/q_globals.h"
#include "dds/ddsi/q_entity.h"
#include "dds/ddsi/q_gc.h"
#include "dds/ddsi/q_rtps.h" /* guid_t */
#include "dds/ddsi/q_thread.h" /* for assert(thread is awake) */

struct ephash {
  struct ddsrt_chh *hash;
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

static int all_entities_compare_isbuiltin (const struct entity_common *e, nn_vendorid_t vendor)
{
  const unsigned char *guid_bytes = (const unsigned char *) &e->guid;
  if (guid_bytes[0] != 0 && guid_bytes[0] != 0xff)
    return is_builtin_endpoint (e->guid.entityid, vendor);
  else
  {
    for (size_t i = 1; i < sizeof (e->guid); i++)
      if (guid_bytes[i] != guid_bytes[0])
        return is_builtin_endpoint (e->guid.entityid, vendor) && !is_local_orphan_endpoint (e);
    return 0;
  }
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

    case EK_WRITER: {
      const struct writer *wra = va;
      const struct writer *wrb = vb;
      if (!all_entities_compare_isbuiltin (a, NN_VENDORID_ECLIPSE)) {
        assert ((wra->xqos->present & QP_TOPIC_NAME) && wra->xqos->topic_name);
        tp_a = wra->xqos->topic_name;
      }
      if (!all_entities_compare_isbuiltin (b, NN_VENDORID_ECLIPSE)) {
        assert ((wrb->xqos->present & QP_TOPIC_NAME) && wrb->xqos->topic_name);
        tp_b = wrb->xqos->topic_name;
      }
      break;
    }

    case EK_READER: {
      const struct reader *rda = va;
      const struct reader *rdb = vb;
      if (!all_entities_compare_isbuiltin (a, NN_VENDORID_ECLIPSE)) {
        assert ((rda->xqos->present & QP_TOPIC_NAME) && rda->xqos->topic_name);
        tp_a = rda->xqos->topic_name;
      }
      if (!all_entities_compare_isbuiltin (b, NN_VENDORID_ECLIPSE)) {
        assert ((rdb->xqos->present & QP_TOPIC_NAME) && rdb->xqos->topic_name);
        tp_b = rdb->xqos->topic_name;
      }
      break;
    }

    case EK_PROXY_WRITER:
    case EK_PROXY_READER: {
      const struct generic_proxy_endpoint *ga = va;
      const struct generic_proxy_endpoint *gb = vb;
      if (!all_entities_compare_isbuiltin (a, ga->c.vendor)) {
        assert ((ga->c.xqos->present & QP_TOPIC_NAME) && ga->c.xqos->topic_name);
        tp_a = ga->c.xqos->topic_name;
      }
      if (!all_entities_compare_isbuiltin (b, gb->c.vendor)) {
        assert ((gb->c.xqos->present & QP_TOPIC_NAME) && gb->c.xqos->topic_name);
        tp_b = gb->c.xqos->topic_name;
      }
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
  struct q_globals *gv = varg;
  struct gcreq *gcreq = gcreq_new (gv->gcreq_queue, gc_buckets_cb);
  gcreq->arg = bs;
  gcreq_enqueue (gcreq);
}

struct ephash *ephash_new (struct q_globals *gv)
{
  struct ephash *ephash;
  ephash = ddsrt_malloc (sizeof (*ephash));
  ephash->hash = ddsrt_chh_new (32, hash_entity_guid_wrapper, entity_guid_eq_wrapper, gc_buckets, gv);
  if (ephash->hash == NULL) {
    ddsrt_free (ephash);
    return NULL;
  } else {
    ddsrt_mutex_init (&ephash->all_entities_lock);
    ddsrt_avl_init (&all_entities_treedef, &ephash->all_entities);
    return ephash;
  }
}

void ephash_free (struct ephash *ephash)
{
  ddsrt_avl_free (&all_entities_treedef, &ephash->all_entities, 0);
  ddsrt_mutex_destroy (&ephash->all_entities_lock);
  ddsrt_chh_free (ephash->hash);
  ephash->hash = NULL;
  ddsrt_free (ephash);
}

static void add_to_all_entities (struct ephash *gh, struct entity_common *e)
{
  ddsrt_mutex_lock (&gh->all_entities_lock);
  assert (ddsrt_avl_lookup (&all_entities_treedef, &gh->all_entities, e) == NULL);
  ddsrt_avl_insert (&all_entities_treedef, &gh->all_entities, e);
  ddsrt_mutex_unlock (&gh->all_entities_lock);
}

static void remove_from_all_entities (struct ephash *gh, struct entity_common *e)
{
  ddsrt_mutex_lock (&gh->all_entities_lock);
  assert (ddsrt_avl_lookup (&all_entities_treedef, &gh->all_entities, e) != NULL);
  ddsrt_avl_delete (&all_entities_treedef, &gh->all_entities, e);
  ddsrt_mutex_unlock (&gh->all_entities_lock);
}

static void ephash_guid_insert (struct ephash *gh, struct entity_common *e)
{
  int x;
  x = ddsrt_chh_add (gh->hash, e);
  (void)x;
  assert (x);
  add_to_all_entities (gh, e);
}

static void ephash_guid_remove (struct ephash *gh, struct entity_common *e)
{
  int x;
  remove_from_all_entities (gh, e);
  x = ddsrt_chh_remove (gh->hash, e);
  (void)x;
  assert (x);
}

void *ephash_lookup_guid_untyped (const struct ephash *gh, const struct ddsi_guid *guid)
{
  /* FIXME: could (now) require guid to be first in entity_common; entity_common already is first in entity */
  struct entity_common e;
  e.guid = *guid;
  assert (thread_is_awake ());
  return ddsrt_chh_lookup (gh->hash, &e);
}

static void *ephash_lookup_guid_int (const struct ephash *gh, const struct ddsi_guid *guid, enum entity_kind kind)
{
  struct entity_common *res;
  if ((res = ephash_lookup_guid_untyped (gh, guid)) != NULL && res->kind == kind)
    return res;
  else
    return NULL;
}

void *ephash_lookup_guid (const struct ephash *gh, const struct ddsi_guid *guid, enum entity_kind kind)
{
  return ephash_lookup_guid_int (gh, guid, kind);
}

void ephash_insert_participant_guid (struct ephash *gh, struct participant *pp)
{
  ephash_guid_insert (gh, &pp->e);
}

void ephash_insert_proxy_participant_guid (struct ephash *gh, struct proxy_participant *proxypp)
{
  ephash_guid_insert (gh, &proxypp->e);
}

void ephash_insert_writer_guid (struct ephash *gh, struct writer *wr)
{
  ephash_guid_insert (gh, &wr->e);
}

void ephash_insert_reader_guid (struct ephash *gh, struct reader *rd)
{
  ephash_guid_insert (gh, &rd->e);
}

void ephash_insert_proxy_writer_guid (struct ephash *gh, struct proxy_writer *pwr)
{
  ephash_guid_insert (gh, &pwr->e);
}

void ephash_insert_proxy_reader_guid (struct ephash *gh, struct proxy_reader *prd)
{
  ephash_guid_insert (gh, &prd->e);
}

void ephash_remove_participant_guid (struct ephash *gh, struct participant *pp)
{
  ephash_guid_remove (gh, &pp->e);
}

void ephash_remove_proxy_participant_guid (struct ephash *gh, struct proxy_participant *proxypp)
{
  ephash_guid_remove (gh, &proxypp->e);
}

void ephash_remove_writer_guid (struct ephash *gh, struct writer *wr)
{
  ephash_guid_remove (gh, &wr->e);
}

void ephash_remove_reader_guid (struct ephash *gh, struct reader *rd)
{
  ephash_guid_remove (gh, &rd->e);
}

void ephash_remove_proxy_writer_guid (struct ephash *gh, struct proxy_writer *pwr)
{
  ephash_guid_remove (gh, &pwr->e);
}

void ephash_remove_proxy_reader_guid (struct ephash *gh, struct proxy_reader *prd)
{
  ephash_guid_remove (gh, &prd->e);
}

struct participant *ephash_lookup_participant_guid (const struct ephash *gh, const struct ddsi_guid *guid)
{
  assert (guid->entityid.u == NN_ENTITYID_PARTICIPANT);
  assert (offsetof (struct participant, e) == 0);
  return ephash_lookup_guid_int (gh, guid, EK_PARTICIPANT);
}

struct proxy_participant *ephash_lookup_proxy_participant_guid (const struct ephash *gh, const struct ddsi_guid *guid)
{
  assert (guid->entityid.u == NN_ENTITYID_PARTICIPANT);
  assert (offsetof (struct proxy_participant, e) == 0);
  return ephash_lookup_guid_int (gh, guid, EK_PROXY_PARTICIPANT);
}

struct writer *ephash_lookup_writer_guid (const struct ephash *gh, const struct ddsi_guid *guid)
{
  assert (is_writer_entityid (guid->entityid));
  assert (offsetof (struct writer, e) == 0);
  return ephash_lookup_guid_int (gh, guid, EK_WRITER);
}

struct reader *ephash_lookup_reader_guid (const struct ephash *gh, const struct ddsi_guid *guid)
{
  assert (is_reader_entityid (guid->entityid));
  assert (offsetof (struct reader, e) == 0);
  return ephash_lookup_guid_int (gh, guid, EK_READER);
}

struct proxy_writer *ephash_lookup_proxy_writer_guid (const struct ephash *gh, const struct ddsi_guid *guid)
{
  assert (is_writer_entityid (guid->entityid));
  assert (offsetof (struct proxy_writer, e) == 0);
  return ephash_lookup_guid_int (gh, guid, EK_PROXY_WRITER);
}

struct proxy_reader *ephash_lookup_proxy_reader_guid (const struct ephash *gh, const struct ddsi_guid *guid)
{
  assert (is_reader_entityid (guid->entityid));
  assert (offsetof (struct proxy_reader, e) == 0);
  return ephash_lookup_guid_int (gh, guid, EK_PROXY_READER);
}

/* Enumeration */

static void ephash_enum_init_minmax_int (struct ephash_enum *st, const struct ephash *gh, struct match_entities_range_key *min)
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
  st->gh = (struct ephash *) gh;
  st->kind = min->entity.e.kind;
  ddsrt_mutex_lock (&st->gh->all_entities_lock);
  st->cur = ddsrt_avl_lookup_succ_eq (&all_entities_treedef, &st->gh->all_entities, min);
  ddsrt_mutex_unlock (&st->gh->all_entities_lock);
}

void ephash_enum_init_topic (struct ephash_enum *st, const struct ephash *gh, enum entity_kind kind, const char *topic, struct match_entities_range_key *max)
{
  assert (kind == EK_READER || kind == EK_WRITER || kind == EK_PROXY_READER || kind == EK_PROXY_WRITER);
  struct match_entities_range_key min;
  match_endpoint_range (kind, topic, &min, max);
  ephash_enum_init_minmax_int (st, gh, &min);
  if (st->cur && all_entities_compare (st->cur, &max->entity) > 0)
    st->cur = NULL;
}

void ephash_enum_init (struct ephash_enum *st, const struct ephash *gh, enum entity_kind kind)
{
  struct match_entities_range_key min;
  match_entity_kind_min (kind, &min);
  ephash_enum_init_minmax_int (st, gh, &min);
  if (st->cur && st->cur->kind != st->kind)
    st->cur = NULL;
}

void ephash_enum_writer_init (struct ephash_enum_writer *st, const struct ephash *gh)
{
  ephash_enum_init (&st->st, gh, EK_WRITER);
}

void ephash_enum_reader_init (struct ephash_enum_reader *st, const struct ephash *gh)
{
  ephash_enum_init (&st->st, gh, EK_READER);
}

void ephash_enum_proxy_writer_init (struct ephash_enum_proxy_writer *st, const struct ephash *gh)
{
  ephash_enum_init (&st->st, gh, EK_PROXY_WRITER);
}

void ephash_enum_proxy_reader_init (struct ephash_enum_proxy_reader *st, const struct ephash *gh)
{
  ephash_enum_init (&st->st, gh, EK_PROXY_READER);
}

void ephash_enum_participant_init (struct ephash_enum_participant *st, const struct ephash *gh)
{
  ephash_enum_init (&st->st, gh, EK_PARTICIPANT);
}

void ephash_enum_proxy_participant_init (struct ephash_enum_proxy_participant *st, const struct ephash *gh)
{
  ephash_enum_init (&st->st, gh, EK_PROXY_PARTICIPANT);
}

void *ephash_enum_next (struct ephash_enum *st)
{
  /* st->cur can not have been freed yet, but it may have been removed from the index */
  assert (ddsrt_atomic_ld32 (&lookup_thread_state ()->vtime) == st->vtime);
  void *res = st->cur;
  if (st->cur)
  {
    ddsrt_mutex_lock (&st->gh->all_entities_lock);
    st->cur = ddsrt_avl_lookup_succ (&all_entities_treedef, &st->gh->all_entities, st->cur);
    ddsrt_mutex_unlock (&st->gh->all_entities_lock);
    if (st->cur && st->cur->kind != st->kind)
      st->cur = NULL;
  }
  return res;
}

void *ephash_enum_next_max (struct ephash_enum *st, const struct match_entities_range_key *max)
{
  void *res = ephash_enum_next (st);

  /* max may only make the bounds tighter */
  assert (max->entity.e.kind == st->kind);
  if (st->cur && all_entities_compare (st->cur, &max->entity) > 0)
    st->cur = NULL;
  return res;
}

struct writer *ephash_enum_writer_next (struct ephash_enum_writer *st)
{
  assert (offsetof (struct writer, e) == 0);
  return ephash_enum_next (&st->st);
}

struct reader *ephash_enum_reader_next (struct ephash_enum_reader *st)
{
  assert (offsetof (struct reader, e) == 0);
  return ephash_enum_next (&st->st);
}

struct proxy_writer *ephash_enum_proxy_writer_next (struct ephash_enum_proxy_writer *st)
{
  assert (offsetof (struct proxy_writer, e) == 0);
  return ephash_enum_next (&st->st);
}

struct proxy_reader *ephash_enum_proxy_reader_next (struct ephash_enum_proxy_reader *st)
{
  assert (offsetof (struct proxy_reader, e) == 0);
  return ephash_enum_next (&st->st);
}

struct participant *ephash_enum_participant_next (struct ephash_enum_participant *st)
{
  assert (offsetof (struct participant, e) == 0);
  return ephash_enum_next (&st->st);
}

struct proxy_participant *ephash_enum_proxy_participant_next (struct ephash_enum_proxy_participant *st)
{
  assert (offsetof (struct proxy_participant, e) == 0);
  return ephash_enum_next (&st->st);
}

void ephash_enum_fini (struct ephash_enum *st)
{
  assert (ddsrt_atomic_ld32 (&lookup_thread_state ()->vtime) == st->vtime);
  (void) st;
}

void ephash_enum_writer_fini (struct ephash_enum_writer *st)
{
  ephash_enum_fini (&st->st);
}

void ephash_enum_reader_fini (struct ephash_enum_reader *st)
{
  ephash_enum_fini (&st->st);
}

void ephash_enum_proxy_writer_fini (struct ephash_enum_proxy_writer *st)
{
  ephash_enum_fini (&st->st);
}

void ephash_enum_proxy_reader_fini (struct ephash_enum_proxy_reader *st)
{
  ephash_enum_fini (&st->st);
}

void ephash_enum_participant_fini (struct ephash_enum_participant *st)
{
  ephash_enum_fini (&st->st);
}

void ephash_enum_proxy_participant_fini (struct ephash_enum_proxy_participant *st)
{
  ephash_enum_fini (&st->st);
}
