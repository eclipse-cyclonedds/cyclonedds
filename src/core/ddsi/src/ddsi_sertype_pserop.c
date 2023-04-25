// Copyright(c) 2020 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <stddef.h>
#include <ctype.h>
#include <assert.h>
#include <string.h>

#include "dds/ddsrt/md5.h"
#include "dds/ddsrt/mh3.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsi/ddsi_freelist.h"
#include "dds/ddsi/ddsi_sertype.h"
#include "ddsi__plist_generic.h"
#include "ddsi__serdata_pserop.h"
#include "ddsi__typelookup.h"

static bool sertype_pserop_equal (const struct ddsi_sertype *acmn, const struct ddsi_sertype *bcmn)
{
  const struct ddsi_sertype_pserop *a = (struct ddsi_sertype_pserop *) acmn;
  const struct ddsi_sertype_pserop *b = (struct ddsi_sertype_pserop *) bcmn;
  if (a->encoding_format != b->encoding_format)
    return false;
  if (a->memsize != b->memsize)
    return false;
  if (a->nops != b->nops)
    return false;
  assert (a->nops > 0);
  if (memcmp (a->ops, b->ops, a->nops * sizeof (*a->ops)) != 0)
    return false;
  if (a->nops_key != b->nops_key)
    return false;
  if (a->ops_key && memcmp (a->ops_key, b->ops_key, a->nops_key * sizeof (*a->ops_key)) != 0)
    return false;
  return true;
}

static uint32_t sertype_pserop_hash (const struct ddsi_sertype *tpcmn)
{
  assert (tpcmn);
  struct ddsi_sertype_pserop *tp = (struct ddsi_sertype_pserop *) tpcmn;
  unsigned char buf[16];
  ddsrt_md5_state_t md5st;
  ddsrt_md5_init (&md5st);
  ddsrt_md5_append (&md5st, (ddsrt_md5_byte_t *) &tp->encoding_format, sizeof (tp->encoding_format));
  ddsrt_md5_append (&md5st, (ddsrt_md5_byte_t *) &tp->memsize, sizeof (tp->memsize));
  ddsrt_md5_append (&md5st, (ddsrt_md5_byte_t *) &tp->nops, sizeof (tp->nops));
  ddsrt_md5_append (&md5st, (ddsrt_md5_byte_t *) tp->ops, (uint32_t) (tp->nops * sizeof (*tp->ops)));
  ddsrt_md5_append (&md5st, (ddsrt_md5_byte_t *) &tp->nops_key, sizeof (tp->nops_key));
  if (tp->ops_key)
    ddsrt_md5_append (&md5st, (ddsrt_md5_byte_t *) tp->ops_key, (uint32_t) tp->nops_key * sizeof (*tp->ops_key));
  ddsrt_md5_finish (&md5st, (ddsrt_md5_byte_t *) buf);
  return *(uint32_t *) buf;
}

static void sertype_pserop_free (struct ddsi_sertype *tpcmn)
{
  assert (tpcmn);
  struct ddsi_sertype_pserop *tp = (struct ddsi_sertype_pserop *) tpcmn;
  ddsi_sertype_fini (&tp->c);
  ddsrt_free (tp);
}

static void sertype_pserop_zero_samples (const struct ddsi_sertype *sertype_common, void *sample, size_t count)
{
  const struct ddsi_sertype_pserop *tp = (const struct ddsi_sertype_pserop *)sertype_common;
  memset (sample, 0, tp->memsize * count);
}

static void sertype_pserop_realloc_samples (void **ptrs, const struct ddsi_sertype *sertype_common, void *old, size_t oldcount, size_t count)
{
  const struct ddsi_sertype_pserop *tp = (const struct ddsi_sertype_pserop *)sertype_common;
  const size_t size = tp->memsize;
  char *new = (oldcount == count) ? old : dds_realloc (old, size * count);
  if (new && count > oldcount)
    memset (new + size * oldcount, 0, size * (count - oldcount));
  for (size_t i = 0; i < count; i++)
  {
    void *ptr = (char *) new + i * size;
    ptrs[i] = ptr;
  }
}

static void sertype_pserop_free_samples (const struct ddsi_sertype *sertype_common, void **ptrs, size_t count, dds_free_op_t op)
{
  if (count > 0)
  {
    const struct ddsi_sertype_pserop *tp = (const struct ddsi_sertype_pserop *)sertype_common;
    const size_t size = tp->memsize;
#ifndef NDEBUG
    for (size_t i = 0, off = 0; i < count; i++, off += size)
      assert ((char *)ptrs[i] == (char *)ptrs[0] + off);
#endif
    char *ptr = ptrs[0];
    for (size_t i = 0; i < count; i++)
    {
      ddsi_plist_fini_generic (ptr, tp->ops, false);
      ptr += size;
    }
    if (op & DDS_FREE_ALL_BIT)
    {
      dds_free (ptrs[0]);
    }
  }
}

const struct ddsi_sertype_ops ddsi_sertype_ops_pserop = {
  .version = ddsi_sertype_v0,
  .arg = 0,
  .equal = sertype_pserop_equal,
  .hash = sertype_pserop_hash,
  .free = sertype_pserop_free,
  .zero_samples = sertype_pserop_zero_samples,
  .realloc_samples = sertype_pserop_realloc_samples,
  .free_samples = sertype_pserop_free_samples,
  .type_id = 0,
  .type_map = 0,
  .type_info = 0,
  .get_serialized_size = 0,
  .serialize_into = 0
};
