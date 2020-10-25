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
#include <ctype.h>
#include <assert.h>
#include <string.h>

#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/md5.h"
#include "dds/ddsrt/mh3.h"
#include "dds/ddsrt/hopscotch.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsi/q_bswap.h"
#include "dds/ddsi/q_config.h"
#include "dds/ddsi/q_freelist.h"
#include "dds/ddsi/ddsi_iid.h"
#include "dds/ddsi/ddsi_sertopic.h"
#include "dds/ddsi/ddsi_serdata.h"
#include "dds/ddsi/ddsi_domaingv.h"

bool ddsi_sertopic_equal (const struct ddsi_sertopic *a, const struct ddsi_sertopic *b)
{
  if (strcmp (a->name, b->name) != 0)
    return false;
  if (strcmp (a->type_name, b->type_name) != 0)
    return false;
  if (a->serdata_basehash != b->serdata_basehash)
    return false;
  if (a->ops != b->ops)
    return false;
  if (a->serdata_ops != b->serdata_ops)
    return false;
  if (a->topickind_no_key != b->topickind_no_key)
    return false;
  return a->ops->equal (a, b);
}

uint32_t ddsi_sertopic_hash (const struct ddsi_sertopic *a)
{
  uint32_t h;
  h = ddsrt_mh3 (a->name, strlen (a->name), a->serdata_basehash);
  h = ddsrt_mh3 (a->type_name, strlen (a->type_name), h);
  h ^= a->serdata_basehash ^ (uint32_t) a->topickind_no_key;
  return h ^ a->ops->hash (a);
}

struct ddsi_sertopic *ddsi_sertopic_ref (const struct ddsi_sertopic *sertopic_const)
{
  struct ddsi_sertopic *sertopic = (struct ddsi_sertopic *) sertopic_const;
  ddsrt_atomic_inc32 (&sertopic->refc);
  return sertopic;
}

struct ddsi_sertopic *ddsi_sertopic_lookup_locked (struct ddsi_domaingv *gv, const struct ddsi_sertopic *sertopic_template)
{
  struct ddsi_sertopic *sertopic = ddsrt_hh_lookup (gv->sertopics, sertopic_template);
#ifndef NDEBUG
  if (sertopic != NULL)
    assert (sertopic->gv != NULL);
#endif
  return sertopic ? ddsi_sertopic_ref (sertopic) : NULL;
}

void ddsi_sertopic_register_locked (struct ddsi_domaingv *gv, struct ddsi_sertopic *sertopic)
{
  assert (sertopic->gv == NULL);

  (void) ddsi_sertopic_ref (sertopic);
  sertopic->gv = gv;
  int x = ddsrt_hh_add (gv->sertopics, sertopic);
  assert (x);
  (void) x;
}

void ddsi_sertopic_unref (struct ddsi_sertopic *sertopic)
{
  if (ddsrt_atomic_dec32_ov (&sertopic->refc) == 1)
  {
    /* if registered, drop from set of registered sertopics */
    if (sertopic->gv)
    {
      ddsrt_mutex_lock (&sertopic->gv->sertopics_lock);
      (void) ddsrt_hh_remove (sertopic->gv->sertopics, sertopic);
      ddsrt_mutex_unlock (&sertopic->gv->sertopics_lock);
      sertopic->gv = NULL;
    }

    ddsi_sertopic_free (sertopic);
  }
}

void ddsi_sertopic_init (struct ddsi_sertopic *tp, const char *name, const char *type_name, const struct ddsi_sertopic_ops *sertopic_ops, const struct ddsi_serdata_ops *serdata_ops, bool topickind_no_key)
{
  ddsrt_atomic_st32 (&tp->refc, 1);
  tp->name = ddsrt_strdup (name);
  tp->type_name = ddsrt_strdup (type_name);
  tp->ops = sertopic_ops;
  tp->serdata_ops = serdata_ops;
  tp->serdata_basehash = ddsi_sertopic_compute_serdata_basehash (tp->serdata_ops);
  tp->topickind_no_key = topickind_no_key;
  /* set later, on registration */
  tp->gv = NULL;
}

void ddsi_sertopic_fini (struct ddsi_sertopic *tp)
{
  ddsrt_free (tp->name);
  ddsrt_free (tp->type_name);
}

uint32_t ddsi_sertopic_compute_serdata_basehash (const struct ddsi_serdata_ops *ops)
{
  ddsrt_md5_state_t md5st;
  ddsrt_md5_byte_t digest[16];
  uint32_t res;
  ddsrt_md5_init (&md5st);
  ddsrt_md5_append (&md5st, (const ddsrt_md5_byte_t *) &ops, sizeof (ops));
  ddsrt_md5_append (&md5st, (const ddsrt_md5_byte_t *) ops, sizeof (*ops));
  ddsrt_md5_finish (&md5st, digest);
  memcpy (&res, digest, sizeof (res));
  return res;
}

extern inline void ddsi_sertopic_free (struct ddsi_sertopic *tp);
extern inline void ddsi_sertopic_zero_samples (const struct ddsi_sertopic *tp, void *samples, size_t count);
extern inline void ddsi_sertopic_realloc_samples (void **ptrs, const struct ddsi_sertopic *tp, void *old, size_t oldcount, size_t count);
extern inline void ddsi_sertopic_free_samples (const struct ddsi_sertopic *tp, void **ptrs, size_t count, dds_free_op_t op);
extern inline void ddsi_sertopic_zero_sample (const struct ddsi_sertopic *tp, void *sample);
extern inline void ddsi_sertopic_free_sample (const struct ddsi_sertopic *tp, void *sample, dds_free_op_t op);
extern inline void *ddsi_sertopic_alloc_sample (const struct ddsi_sertopic *tp);
