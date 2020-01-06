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
#include <stddef.h>
#include <string.h>

#include "dds/ddsrt/heap.h"
#include "dds/ddsi/ddsi_serdata.h"
#include "dds/ddsi/q_unused.h"
#include "dds/ddsi/q_config.h"
#include "dds/ddsi/ddsi_entity_index.h"
#include "dds/ddsi/q_entity.h"
#include "dds/ddsi/q_globals.h"
#include "dds/ddsi/ddsi_tkmap.h"
#include "dds__serdata_builtintopic.h"
#include "dds__whc_builtintopic.h"
#include "dds__builtin.h"

struct bwhc {
  struct whc common;
  enum ddsi_sertopic_builtintopic_type type;
  const struct entity_index *entidx;
};

enum bwhc_iter_state {
  BIS_INIT_LOCAL,
  BIS_LOCAL,
  BIS_INIT_PROXY,
  BIS_PROXY
};

struct bwhc_iter {
  struct whc_sample_iter_base c;
  enum bwhc_iter_state st;
  bool have_sample;
  struct entidx_enum it;
};

/* check that our definition of whc_sample_iter fits in the type that callers allocate */
DDSRT_STATIC_ASSERT (sizeof (struct bwhc_iter) <= sizeof (struct whc_sample_iter));

static void bwhc_free (struct whc *whc_generic)
{
  ddsrt_free (whc_generic);
}

static void bwhc_sample_iter_init (const struct whc *whc_generic, struct whc_sample_iter *opaque_it)
{
  struct bwhc_iter *it = (struct bwhc_iter *) opaque_it;
  it->c.whc = (struct whc *) whc_generic;
  it->st = BIS_INIT_LOCAL;
  it->have_sample = false;
}

static bool is_visible (const struct entity_common *e)
{
  const nn_vendorid_t vendorid = get_entity_vendorid (e);
  return builtintopic_is_visible (e->gv->builtin_topic_interface, &e->guid, vendorid);
}

static bool bwhc_sample_iter_borrow_next (struct whc_sample_iter *opaque_it, struct whc_borrowed_sample *sample)
{
  struct bwhc_iter * const it = (struct bwhc_iter *) opaque_it;
  struct bwhc * const whc = (struct bwhc *) it->c.whc;
  enum entity_kind kind = EK_PARTICIPANT; /* pacify gcc */
  struct entity_common *entity;

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
      switch (whc->type) {
        case DSBT_PARTICIPANT: kind = EK_PARTICIPANT; break;
        case DSBT_WRITER:      kind = EK_WRITER; break;
        case DSBT_READER:      kind = EK_READER; break;
      }
      assert (whc->type == DSBT_PARTICIPANT || kind != EK_PARTICIPANT);
      entidx_enum_init (&it->it, whc->entidx, kind);
      it->st = BIS_LOCAL;
      /* FALLS THROUGH */
    case BIS_LOCAL:
      while ((entity = entidx_enum_next (&it->it)) != NULL)
        if (is_visible (entity))
          break;
      if (entity) {
        sample->serdata = dds__builtin_make_sample (entity, entity->tupdate, true);
        it->have_sample = true;
        return true;
      } else {
        entidx_enum_fini (&it->it);
        it->st = BIS_INIT_PROXY;
      }
      /* FALLS THROUGH */
    case BIS_INIT_PROXY:
      switch (whc->type) {
        case DSBT_PARTICIPANT: kind = EK_PROXY_PARTICIPANT; break;
        case DSBT_WRITER:      kind = EK_PROXY_WRITER; break;
        case DSBT_READER:      kind = EK_PROXY_READER; break;
      }
      assert (kind != EK_PARTICIPANT);
      entidx_enum_init (&it->it, whc->entidx, kind);
      it->st = BIS_PROXY;
      /* FALLS THROUGH */
    case BIS_PROXY:
      while ((entity = entidx_enum_next (&it->it)) != NULL)
        if (is_visible (entity))
          break;
      if (entity) {
        sample->serdata = dds__builtin_make_sample (entity, entity->tupdate, true);
        it->have_sample = true;
        return true;
      } else {
        entidx_enum_fini (&it->it);
        return false;
      }
  }
  assert (0);
  return false;
}

static void bwhc_get_state (const struct whc *whc, struct whc_state *st)
{
  (void)whc;
  st->max_seq = -1;
  st->min_seq = -1;
  st->unacked_bytes = 0;
}

static int bwhc_insert (struct whc *whc, seqno_t max_drop_seq, seqno_t seq, nn_mtime_t exp, struct nn_plist *plist, struct ddsi_serdata *serdata, struct ddsi_tkmap_instance *tk)
{
  (void)whc;
  (void)max_drop_seq;
  (void)seq;
  (void)exp;
  (void)serdata;
  (void)tk;
  if (plist)
    ddsrt_free (plist);
  return 0;
}

static unsigned bwhc_downgrade_to_volatile (struct whc *whc, struct whc_state *st)
{
  (void)whc;
  (void)st;
  return 0;
}

static unsigned bwhc_remove_acked_messages (struct whc *whc, seqno_t max_drop_seq, struct whc_state *whcst, struct whc_node **deferred_free_list)
{
  (void)whc;
  (void)max_drop_seq;
  (void)whcst;
  *deferred_free_list = NULL;
  return 0;
}

static void bwhc_free_deferred_free_list (struct whc *whc, struct whc_node *deferred_free_list)
{
  (void)whc;
  (void)deferred_free_list;
}

static const struct whc_ops bwhc_ops = {
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
  .downgrade_to_volatile = bwhc_downgrade_to_volatile,
  .free = bwhc_free
};

struct whc *builtintopic_whc_new (enum ddsi_sertopic_builtintopic_type type, const struct entity_index *entidx)
{
  struct bwhc *whc = ddsrt_malloc (sizeof (*whc));
  whc->common.ops = &bwhc_ops;
  whc->type = type;
  whc->entidx = entidx;
  return (struct whc *) whc;
}
