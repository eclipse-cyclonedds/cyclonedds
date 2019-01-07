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

#include "os/os.h"

#include "util/ut_hopscotch.h"
#include "ddsi/q_ephash.h"
#include "ddsi/q_config.h"
#include "ddsi/q_globals.h"
#include "ddsi/q_entity.h"
#include "ddsi/q_gc.h"
#include "ddsi/q_rtps.h" /* guid_t */
#include "ddsi/q_thread.h" /* for assert(thread is awake) */

struct ephash {
  struct ut_chh *hash;
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
  os_free (bs);
}

static void gc_buckets (void *bs)
{
  struct gcreq *gcreq = gcreq_new (gv.gcreq_queue, gc_buckets_cb);
  gcreq->arg = bs;
  gcreq_enqueue (gcreq);
}

struct ephash *ephash_new (void)
{
  struct ephash *ephash;
  ephash = os_malloc (sizeof (*ephash));
  ephash->hash = ut_chhNew (32, hash_entity_guid_wrapper, entity_guid_eq_wrapper, gc_buckets);
  if (ephash->hash == NULL) {
    os_free (ephash);
    return NULL;
  } else {
    return ephash;
  }
}

void ephash_free (struct ephash *ephash)
{
  ut_chhFree (ephash->hash);
  ephash->hash = NULL;
  os_free (ephash);
}

static void ephash_guid_insert (struct entity_common *e)
{
  int x;
  assert(gv.guid_hash);
  assert(gv.guid_hash->hash);
  x = ut_chhAdd (gv.guid_hash->hash, e);
  (void)x;
  assert (x);
}

static void ephash_guid_remove (struct entity_common *e)
{
  int x;
  assert(gv.guid_hash);
  assert(gv.guid_hash->hash);
  x = ut_chhRemove (gv.guid_hash->hash, e);
  (void)x;
  assert (x);
}

void *ephash_lookup_guid_untyped (const struct nn_guid *guid)
{
  /* FIXME: could (now) require guid to be first in entity_common; entity_common already is first in entity */
  struct entity_common e;
  e.guid = *guid;
  return ut_chhLookup (gv.guid_hash->hash, &e);
}

static void *ephash_lookup_guid_int (const struct ephash *ephash, const struct nn_guid *guid, enum entity_kind kind)
{
  struct entity_common *res;
  (void)ephash;
  if ((res = ephash_lookup_guid_untyped (guid)) != NULL && res->kind == kind)
    return res;
  else
    return NULL;
}

void *ephash_lookup_guid (const struct nn_guid *guid, enum entity_kind kind)
{
  return ephash_lookup_guid_int (NULL, guid, kind);
}

void ephash_insert_participant_guid (struct participant *pp)
{
  ephash_guid_insert (&pp->e);
}

void ephash_insert_proxy_participant_guid (struct proxy_participant *proxypp)
{
  ephash_guid_insert (&proxypp->e);
}

void ephash_insert_writer_guid (struct writer *wr)
{
  ephash_guid_insert (&wr->e);
}

void ephash_insert_reader_guid (struct reader *rd)
{
  ephash_guid_insert (&rd->e);
}

void ephash_insert_proxy_writer_guid (struct proxy_writer *pwr)
{
  ephash_guid_insert (&pwr->e);
}

void ephash_insert_proxy_reader_guid (struct proxy_reader *prd)
{
  ephash_guid_insert (&prd->e);
}

void ephash_remove_participant_guid (struct participant *pp)
{
  ephash_guid_remove (&pp->e);
}

void ephash_remove_proxy_participant_guid (struct proxy_participant *proxypp)
{
  ephash_guid_remove (&proxypp->e);
}

void ephash_remove_writer_guid (struct writer *wr)
{
  ephash_guid_remove (&wr->e);
}

void ephash_remove_reader_guid (struct reader *rd)
{
  ephash_guid_remove (&rd->e);
}

void ephash_remove_proxy_writer_guid (struct proxy_writer *pwr)
{
  ephash_guid_remove (&pwr->e);
}

void ephash_remove_proxy_reader_guid (struct proxy_reader *prd)
{
  ephash_guid_remove (&prd->e);
}

struct participant *ephash_lookup_participant_guid (const struct nn_guid *guid)
{
  assert (guid->entityid.u == NN_ENTITYID_PARTICIPANT);
  assert (offsetof (struct participant, e) == 0);
  return ephash_lookup_guid_int (gv.guid_hash, guid, EK_PARTICIPANT);
}

struct proxy_participant *ephash_lookup_proxy_participant_guid (const struct nn_guid *guid)
{
  assert (guid->entityid.u == NN_ENTITYID_PARTICIPANT);
  assert (offsetof (struct proxy_participant, e) == 0);
  return ephash_lookup_guid_int (gv.guid_hash, guid, EK_PROXY_PARTICIPANT);
}

struct writer *ephash_lookup_writer_guid (const struct nn_guid *guid)
{
  assert (is_writer_entityid (guid->entityid));
  assert (offsetof (struct writer, e) == 0);
  return ephash_lookup_guid_int (gv.guid_hash, guid, EK_WRITER);
}

struct reader *ephash_lookup_reader_guid (const struct nn_guid *guid)
{
  assert (is_reader_entityid (guid->entityid));
  assert (offsetof (struct reader, e) == 0);
  return ephash_lookup_guid_int (gv.guid_hash, guid, EK_READER);
}

struct proxy_writer *ephash_lookup_proxy_writer_guid (const struct nn_guid *guid)
{
  assert (is_writer_entityid (guid->entityid));
  assert (offsetof (struct proxy_writer, e) == 0);
  return ephash_lookup_guid_int (gv.guid_hash, guid, EK_PROXY_WRITER);
}

struct proxy_reader *ephash_lookup_proxy_reader_guid (const struct nn_guid *guid)
{
  assert (is_reader_entityid (guid->entityid));
  assert (offsetof (struct proxy_reader, e) == 0);
  return ephash_lookup_guid_int (gv.guid_hash, guid, EK_PROXY_READER);
}

/* Enumeration */

static void ephash_enum_init_int (struct ephash_enum *st, struct ephash *ephash, enum entity_kind kind)
{
  st->kind = kind;
  st->cur = ut_chhIterFirst (ephash->hash, &st->it);
  while (st->cur && st->cur->kind != st->kind)
    st->cur = ut_chhIterNext (&st->it);
}

void ephash_enum_init (struct ephash_enum *st, enum entity_kind kind)
{
  ephash_enum_init_int(st, gv.guid_hash, kind);
}

void ephash_enum_writer_init (struct ephash_enum_writer *st)
{
  ephash_enum_init (&st->st, EK_WRITER);
}

void ephash_enum_reader_init (struct ephash_enum_reader *st)
{
  ephash_enum_init (&st->st, EK_READER);
}

void ephash_enum_proxy_writer_init (struct ephash_enum_proxy_writer *st)
{
  ephash_enum_init (&st->st, EK_PROXY_WRITER);
}

void ephash_enum_proxy_reader_init (struct ephash_enum_proxy_reader *st)
{
  ephash_enum_init (&st->st, EK_PROXY_READER);
}

void ephash_enum_participant_init (struct ephash_enum_participant *st)
{
  ephash_enum_init (&st->st, EK_PARTICIPANT);
}

void ephash_enum_proxy_participant_init (struct ephash_enum_proxy_participant *st)
{
  ephash_enum_init (&st->st, EK_PROXY_PARTICIPANT);
}

void *ephash_enum_next (struct ephash_enum *st)
{
  void *res = st->cur;
  if (st->cur)
  {
    st->cur = ut_chhIterNext (&st->it);
    while (st->cur && st->cur->kind != st->kind)
      st->cur = ut_chhIterNext (&st->it);
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
  OS_UNUSED_ARG(st);
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
