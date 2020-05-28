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
#include "dds/ddsi/ddsi_plist_generic.h"
#include "dds/ddsi/ddsi_sertype.h"
#include "dds/ddsi/ddsi_serdata.h"
#include "dds/ddsi/ddsi_serdata_default.h"
#include "dds/ddsi/ddsi_serdata_pserop.h"
#include "dds/ddsi/ddsi_domaingv.h"

bool ddsi_sertype_equal (const struct ddsi_sertype *a, const struct ddsi_sertype *b)
{
  if (strcmp (a->type_name, b->type_name) != 0)
    return false;
  if (a->serdata_basehash != b->serdata_basehash)
    return false;
  if (a->ops != b->ops)
    return false;
  if (a->serdata_ops != b->serdata_ops)
    return false;
  if (a->typekind_no_key != b->typekind_no_key)
    return false;
  return a->ops->equal (a, b);
}

uint32_t ddsi_sertype_hash (const struct ddsi_sertype *a)
{
  uint32_t h;
  h = ddsrt_mh3 (a->type_name, strlen (a->type_name), a->serdata_basehash);
  h ^= a->serdata_basehash ^ (uint32_t) a->typekind_no_key;
  return h ^ a->ops->hash (a);
}

static struct ddsi_sertype *ddsi_sertype_ref_locked (const struct ddsi_sertype *sertype_const)
{
  struct ddsi_sertype *sertype = (struct ddsi_sertype *) sertype_const;
  sertype->refc++;
  return sertype;
}

struct ddsi_sertype *ddsi_sertype_ref (const struct ddsi_sertype *sertype_const)
{
  ddsrt_mutex_lock (&sertype_const->gv->sertypes_lock);
  struct ddsi_sertype *sertype = ddsi_sertype_ref_locked (sertype_const);
  ddsrt_mutex_unlock (&sertype_const->gv->sertypes_lock);
  return sertype;
}

struct ddsi_sertype *ddsi_sertype_lookup_locked (struct ddsi_domaingv *gv, const struct ddsi_sertype *sertype_template)
{
  struct ddsi_sertype *sertype = ddsrt_hh_lookup (gv->sertypes, sertype_template);
#ifndef NDEBUG
  if (sertype != NULL)
    assert (sertype->refc > 0);
#endif
  return sertype ? ddsi_sertype_ref_locked (sertype) : NULL;
}

void ddsi_sertype_register_locked (struct ddsi_sertype *sertype)
{
  assert (!sertype->registered);
  (void) ddsi_sertype_ref_locked (sertype);
  sertype->registered = true;
  int x = ddsrt_hh_add (sertype->gv->sertypes, sertype);
  assert (x);
  (void) x;
}

void ddsi_sertype_unref_locked (struct ddsi_sertype *sertype)
{
  struct ddsi_domaingv *gv = sertype->gv;
  if (--sertype->refc == 0)
  {
    if (sertype->registered)
    {
      (void) ddsrt_hh_remove (gv->sertypes, sertype);
      sertype->registered = false;
    }
    ddsi_sertype_free (sertype);
  }
}

void ddsi_sertype_unref (struct ddsi_sertype *sertype)
{
  struct ddsi_domaingv *gv = sertype->gv;
  ddsrt_mutex_lock (&gv->sertypes_lock);
  ddsi_sertype_unref_locked (sertype);
  ddsrt_mutex_unlock (&gv->sertypes_lock);
}

struct sertype_ser
{
  char *type_name;
  bool typekind_no_key;
};

const enum pserop sertype_ser_ops[] = { XS, Xb, XSTOP };

bool ddsi_sertype_serialize (const struct ddsi_sertype *tp, size_t *dst_sz, unsigned char **dst_buf)
{
  struct sertype_ser d = { tp->type_name, tp->typekind_no_key };
  size_t dst_pos = 0;

  if (!tp->ops->serialized_size || !tp->ops->serialize)
    return false;
  *dst_sz = 0;
  plist_ser_generic_size_embeddable (dst_sz, &d, 0, sertype_ser_ops);
  tp->ops->serialized_size (tp, dst_sz);
  *dst_buf = ddsrt_malloc (*dst_sz);
  if (plist_ser_generic_embeddable ((char *) *dst_buf, &dst_pos, &d, 0, sertype_ser_ops, DDSRT_BOSEL_LE) < 0) // xtypes spec (7.3.4.5) requires LE encoding for type serialization
    return false;
  if (!tp->ops->serialize (tp, &dst_pos, *dst_buf))
    return false;
  assert (dst_pos == *dst_sz);
  return true;
}

bool ddsi_sertype_deserialize (struct ddsi_domaingv *gv, struct ddsi_sertype *tp, const struct ddsi_sertype_ops *sertype_ops, size_t sz, unsigned char *serdata)
{
  struct sertype_ser d;
  size_t srcoff = 0;
  if (!sertype_ops->deserialize)
    return false;
  if (plist_deser_generic_srcoff (&d, serdata, sz, &srcoff, DDSRT_ENDIAN != DDSRT_LITTLE_ENDIAN, sertype_ser_ops) < 0)
    return false;
  tp->refc = 1;
  tp->ops = sertype_ops;
  tp->type_name = ddsrt_strdup (d.type_name);
  tp->typekind_no_key = d.typekind_no_key;
  if (!tp->ops->deserialize (gv, tp, sz, serdata, &srcoff))
  {
    ddsrt_free (tp->type_name);
    return false;
  }
  tp->serdata_basehash = ddsi_sertype_compute_serdata_basehash (tp->serdata_ops);
  tp->gv = gv;
  tp->registered = false;
  return true;
}

void ddsi_sertype_init (struct ddsi_domaingv *gv, struct ddsi_sertype *tp, const char *type_name, const struct ddsi_sertype_ops *sertype_ops, const struct ddsi_serdata_ops *serdata_ops, bool typekind_no_key)
{
  tp->refc = 1;
  tp->type_name = ddsrt_strdup (type_name);
  tp->ops = sertype_ops;
  tp->serdata_ops = serdata_ops;
  tp->serdata_basehash = ddsi_sertype_compute_serdata_basehash (tp->serdata_ops);
  tp->typekind_no_key = typekind_no_key;
  tp->gv = gv;
  tp->registered = false;
}

void ddsi_sertype_fini (struct ddsi_sertype *tp)
{
  assert (tp->refc == 0);
  ddsrt_free (tp->type_name);
}

uint32_t ddsi_sertype_compute_serdata_basehash (const struct ddsi_serdata_ops *ops)
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

extern inline void ddsi_sertype_free (struct ddsi_sertype *tp);
extern inline void ddsi_sertype_zero_samples (const struct ddsi_sertype *tp, void *samples, size_t count);
extern inline void ddsi_sertype_realloc_samples (void **ptrs, const struct ddsi_sertype *tp, void *old, size_t oldcount, size_t count);
extern inline void ddsi_sertype_free_samples (const struct ddsi_sertype *tp, void **ptrs, size_t count, dds_free_op_t op);
extern inline void ddsi_sertype_zero_sample (const struct ddsi_sertype *tp, void *sample);
extern inline void *ddsi_sertype_alloc_sample (const struct ddsi_sertype *tp);
extern inline void ddsi_sertype_free_sample (const struct ddsi_sertype *tp, void *sample, dds_free_op_t op);
extern inline bool ddsi_sertype_typeid_hash (const struct ddsi_sertype *tp, unsigned char *buf);
extern inline bool ddsi_sertype_assignable_from (const struct ddsi_sertype *type_a, const struct ddsi_sertype *type_b);
