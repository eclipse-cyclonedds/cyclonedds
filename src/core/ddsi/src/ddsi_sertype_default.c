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

#include "dds/ddsrt/md5.h"
#include "dds/ddsrt/mh3.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsi/q_bswap.h"
#include "dds/ddsi/q_config.h"
#include "dds/ddsi/q_freelist.h"
#include "dds/ddsi/ddsi_cdrstream.h"
#include "dds/ddsi/ddsi_sertype.h"
#include "dds/ddsi/ddsi_serdata_default.h"
#include "dds/ddsi/ddsi_typelookup.h"

static bool sertype_default_equal (const struct ddsi_sertype *acmn, const struct ddsi_sertype *bcmn)
{
  const struct ddsi_sertype_default *a = (struct ddsi_sertype_default *) acmn;
  const struct ddsi_sertype_default *b = (struct ddsi_sertype_default *) bcmn;
  if (a->native_encoding_identifier != b->native_encoding_identifier)
    return false;
  if (a->type.size != b->type.size)
    return false;
  if (a->type.align != b->type.align)
    return false;
  if (a->type.flagset != b->type.flagset)
    return false;
  if (a->type.keys.nkeys != b->type.keys.nkeys)
    return false;
  if (
    (a->type.keys.nkeys > 0) &&
    memcmp (a->type.keys.keys, b->type.keys.keys, a->type.keys.nkeys * sizeof (*a->type.keys.keys)) != 0)
    return false;
  if (a->type.ops.nops != b->type.ops.nops)
    return false;
  if (
    (a->type.ops.nops > 0) &&
    memcmp (a->type.ops.ops, b->type.ops.ops, a->type.ops.nops * sizeof (*a->type.ops.ops)) != 0)
    return false;
  assert (a->opt_size == b->opt_size);
  return true;
}

static bool sertype_default_typeid_hash (const struct ddsi_sertype *tpcmn, unsigned char *buf)
{
  assert (tpcmn);
  const struct ddsi_sertype_default *tp = (struct ddsi_sertype_default *) tpcmn;

  ddsrt_md5_state_t md5st;
  ddsrt_md5_init (&md5st);
  ddsrt_md5_append (&md5st, (ddsrt_md5_byte_t *) tp->c.type_name, (uint32_t) strlen (tp->c.type_name));
  ddsrt_md5_append (&md5st, (ddsrt_md5_byte_t *) &tp->native_encoding_identifier, sizeof (tp->native_encoding_identifier));
  ddsrt_md5_append (&md5st, (ddsrt_md5_byte_t *) &tp->type.size, sizeof (tp->type.size));
  ddsrt_md5_append (&md5st, (ddsrt_md5_byte_t *) &tp->type.align, sizeof (tp->type.align));
  ddsrt_md5_append (&md5st, (ddsrt_md5_byte_t *) &tp->type.flagset, sizeof (tp->type.flagset));
  ddsrt_md5_append (&md5st, (ddsrt_md5_byte_t *) tp->type.keys.keys, (uint32_t) (tp->type.keys.nkeys * sizeof (*tp->type.keys.keys)));
  ddsrt_md5_append (&md5st, (ddsrt_md5_byte_t *) tp->type.ops.ops, (uint32_t) (tp->type.ops.nops * sizeof (*tp->type.ops.ops)));
  ddsrt_md5_finish (&md5st, (ddsrt_md5_byte_t *) buf);
  return true;
}

static uint32_t sertype_default_hash (const struct ddsi_sertype *tpcmn)
{
  unsigned char buf[16];
  sertype_default_typeid_hash (tpcmn, buf);
  return *(uint32_t *) buf;
}

static void sertype_default_free (struct ddsi_sertype *tpcmn)
{
  struct ddsi_sertype_default *tp = (struct ddsi_sertype_default *) tpcmn;
  ddsrt_free (tp->type.keys.keys);
  ddsrt_free (tp->type.ops.ops);
  ddsi_sertype_fini (&tp->c);
  ddsrt_free (tp);
}

static void sertype_default_zero_samples (const struct ddsi_sertype *sertype_common, void *sample, size_t count)
{
  const struct ddsi_sertype_default *tp = (const struct ddsi_sertype_default *)sertype_common;
  memset (sample, 0, tp->type.size * count);
}

static void sertype_default_realloc_samples (void **ptrs, const struct ddsi_sertype *sertype_common, void *old, size_t oldcount, size_t count)
{
  const struct ddsi_sertype_default *tp = (const struct ddsi_sertype_default *)sertype_common;
  const size_t size = tp->type.size;
  char *new = (oldcount == count) ? old : dds_realloc (old, size * count);
  if (new && count > oldcount)
    memset (new + size * oldcount, 0, size * (count - oldcount));
  for (size_t i = 0; i < count; i++)
  {
    void *ptr = (char *) new + i * size;
    ptrs[i] = ptr;
  }
}

static void sertype_default_free_samples (const struct ddsi_sertype *sertype_common, void **ptrs, size_t count, dds_free_op_t op)
{
  if (count > 0)
  {
    const struct ddsi_sertype_default *tp = (const struct ddsi_sertype_default *)sertype_common;
    const struct ddsi_sertype_default_desc *type = &tp->type;
    const size_t size = type->size;
#ifndef NDEBUG
    for (size_t i = 0, off = 0; i < count; i++, off += size)
      assert ((char *)ptrs[i] == (char *)ptrs[0] + off);
#endif
    if (type->flagset & DDS_TOPIC_NO_OPTIMIZE)
    {
      char *ptr = ptrs[0];
      for (size_t i = 0; i < count; i++)
      {
        dds_stream_free_sample (ptr, type->ops.ops);
        ptr += size;
      }
    }
    if (op & DDS_FREE_ALL_BIT)
    {
      dds_free (ptrs[0]);
    }
  }
}

const enum pserop ddsi_sertype_default_desc_ops[] = { Xux3, XQ, Xu, XSTOP, XQ, Xu, XSTOP, XSTOP };

static void sertype_default_serialized_size (const struct ddsi_sertype *stc, size_t *dst_offset)
{
  const struct ddsi_sertype_default *st = (const struct ddsi_sertype_default *) stc;
  plist_ser_generic_size_embeddable (dst_offset, &st->type, 0, ddsi_sertype_default_desc_ops);
}

static bool sertype_default_serialize (const struct ddsi_sertype *stc, size_t *dst_offset, unsigned char *dst_buf)
{
  const struct ddsi_sertype_default *st = (const struct ddsi_sertype_default *) stc;
  return (plist_ser_generic_embeddable ((char *) dst_buf, dst_offset, &st->type, 0, ddsi_sertype_default_desc_ops, DDSRT_BOSEL_LE) >= 0); // xtypes spec (7.3.4.5) requires LE encoding for type serialization
}

static bool sertype_default_deserialize (struct ddsi_domaingv *gv, struct ddsi_sertype *stc, size_t src_sz, const unsigned char *src_data, size_t *src_offset)
{
  struct ddsi_sertype_default *st = (struct ddsi_sertype_default *) stc;
  st->native_encoding_identifier = (DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN ? CDR_LE : CDR_BE);
  st->serpool = gv->serpool;
  st->c.serdata_ops = st->c.typekind_no_key ? &ddsi_serdata_ops_cdr_nokey : &ddsi_serdata_ops_cdr;
  if (plist_deser_generic_srcoff (&st->type, src_data, src_sz, src_offset, DDSRT_ENDIAN != DDSRT_LITTLE_ENDIAN, ddsi_sertype_default_desc_ops) < 0)
    return false;
  st->opt_size = (st->type.flagset & DDS_TOPIC_NO_OPTIMIZE) ? 0 : dds_stream_check_optimize (&st->type);
  return true;
}

static bool sertype_default_assignable_from (const struct ddsi_sertype *type_a, const struct ddsi_sertype *type_b)
{
#ifdef DDS_HAS_TYPE_DISCOVERY
  struct ddsi_sertype_default *a = (struct ddsi_sertype_default *) type_a;
  struct ddsi_sertype_default *b = (struct ddsi_sertype_default *) type_b;

  // If receiving type disables type checking, type b is assignable
  if (a->type.flagset & DDS_TOPIC_DISABLE_TYPECHECK)
    return true;

  // For now, the assignable check is just comparing the type-ids for a and b, so only equal types will match
  type_identifier_t *typeid_a = ddsi_typeid_from_sertype (&a->c);
  type_identifier_t *typeid_b = ddsi_typeid_from_sertype (&b->c);
  // this sertype always provides a typeid
  assert (typeid_a && typeid_b);
  bool assignable = ddsi_typeid_equal (typeid_a, typeid_b);
  ddsrt_free (typeid_a);
  ddsrt_free (typeid_b);
  return assignable;
#else
  DDSRT_UNUSED_ARG (type_a);
  DDSRT_UNUSED_ARG (type_b);
  return false;
#endif
}

const struct ddsi_sertype_ops ddsi_sertype_ops_default = {
  .version = ddsi_sertype_v0,
  .arg = 0,
  .equal = sertype_default_equal,
  .hash = sertype_default_hash,
  .typeid_hash = sertype_default_typeid_hash,
  .free = sertype_default_free,
  .zero_samples = sertype_default_zero_samples,
  .realloc_samples = sertype_default_realloc_samples,
  .free_samples = sertype_default_free_samples,
  .serialized_size = sertype_default_serialized_size,
  .serialize = sertype_default_serialize,
  .deserialize = sertype_default_deserialize,
  .assignable_from = sertype_default_assignable_from
};
