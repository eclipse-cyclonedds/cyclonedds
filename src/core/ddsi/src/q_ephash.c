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

#include "cyclonedds/ddsrt/heap.h"
#include "cyclonedds/ddsrt/misc.h"

#include "cyclonedds/ddsrt/hopscotch.h"
#include "cyclonedds/ddsi/q_ephash.h"
#include "cyclonedds/ddsi/q_config.h"
#include "cyclonedds/ddsi/q_globals.h"
#include "cyclonedds/ddsi/q_entity.h"
#include "cyclonedds/ddsi/q_gc.h"
#include "cyclonedds/ddsi/q_rtps.h" /* guid_t */
#include "cyclonedds/ddsi/q_thread.h" /* for assert(thread is awake) */

struct ephash {
  struct ddsrt_chh *hash;
};

static const uint64_t unihashconsts[] = {
  UINT64_C (16292676669999574021),
  UINT64_C (10242350189706880077),
  UINT64_C (12844332200329132887),
  UINT64_C (16728792139623414127)
};

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
    return ephash;
  }
}

void ephash_free (struct ephash *ephash)
{
  ddsrt_chh_free (ephash->hash);
  ephash->hash = NULL;
  ddsrt_free (ephash);
}

static void ephash_guid_insert (struct ephash *gh, struct entity_common *e)
{
  int x;
  x = ddsrt_chh_add (gh->hash, e);
  (void)x;
  assert (x);
}

static void ephash_guid_remove (struct ephash *gh, struct entity_common *e)
{
  int x;
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

static void ephash_enum_init_int (struct ephash_enum *st, const struct ephash *gh, enum entity_kind kind)
{
  st->kind = kind;
  st->cur = ddsrt_chh_iter_first (gh->hash, &st->it);
  while (st->cur && st->cur->kind != st->kind)
    st->cur = ddsrt_chh_iter_next (&st->it);
}

void ephash_enum_init (struct ephash_enum *st, const struct ephash *gh, enum entity_kind kind)
{
  ephash_enum_init_int(st, gh, kind);
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
  void *res = st->cur;
  if (st->cur)
  {
    st->cur = ddsrt_chh_iter_next (&st->it);
    while (st->cur && st->cur->kind != st->kind)
      st->cur = ddsrt_chh_iter_next (&st->it);
  }
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
  DDSRT_UNUSED_ARG(st);
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
