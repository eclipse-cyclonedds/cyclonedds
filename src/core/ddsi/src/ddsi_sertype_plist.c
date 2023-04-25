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
#include "dds/ddsi/ddsi_plist.h"
#include "dds/ddsi/ddsi_sertype.h"
#include "ddsi__serdata_plist.h"
#include "ddsi__typelookup.h"

static bool sertype_plist_equal (const struct ddsi_sertype *acmn, const struct ddsi_sertype *bcmn)
{
  const struct ddsi_sertype_plist *a = (struct ddsi_sertype_plist *) acmn;
  const struct ddsi_sertype_plist *b = (struct ddsi_sertype_plist *) bcmn;
  if (a->encoding_format != b->encoding_format)
    return false;
  if (a->keyparam != b->keyparam)
    return false;
  return true;
}

static uint32_t sertype_plist_hash (const struct ddsi_sertype *tpcmn)
{
  unsigned char buf[16];
  const struct ddsi_sertype_plist *tp = (struct ddsi_sertype_plist *) tpcmn;
  ddsrt_md5_state_t md5st;
  ddsrt_md5_init (&md5st);
  ddsrt_md5_append (&md5st, (ddsrt_md5_byte_t *) &tp->encoding_format, sizeof (tp->encoding_format));
  ddsrt_md5_append (&md5st, (ddsrt_md5_byte_t *) &tp->keyparam, sizeof (tp->keyparam));
  ddsrt_md5_finish (&md5st, (ddsrt_md5_byte_t *) buf);
  return *(uint32_t *) buf;
}

static void sertype_plist_free (struct ddsi_sertype *tpcmn)
{
  struct ddsi_sertype_plist *tp = (struct ddsi_sertype_plist *) tpcmn;
  ddsi_sertype_fini (&tp->c);
  ddsrt_free (tp);
}

static void sertype_plist_zero_samples (const struct ddsi_sertype *sertype_common, void *sample, size_t count)
{
  (void) sertype_common;
  ddsi_plist_t *xs = sample;
  for (size_t i = 0; i < count; i++)
    ddsi_plist_init_empty (&xs[i]);
}

static void sertype_plist_realloc_samples (void **ptrs, const struct ddsi_sertype *sertype_common, void *old, size_t oldcount, size_t count)
{
  (void) sertype_common;
  ddsi_plist_t *new = (oldcount == count) ? old : dds_realloc (old, count * sizeof (ddsi_plist_t));
  if (new)
  {
    for (size_t i = count; i < oldcount; i++)
      ddsi_plist_init_empty (&new[i]);
    for (size_t i = 0; i < count; i++)
      ptrs[i] = &new[i];
  }
}

static void sertype_plist_free_samples (const struct ddsi_sertype *sertype_common, void **ptrs, size_t count, dds_free_op_t op)
{
  (void) sertype_common;
  if (count > 0)
  {
#ifndef NDEBUG
    for (size_t i = 0, off = 0; i < count; i++, off += sizeof (ddsi_plist_t))
      assert ((char *)ptrs[i] == (char *)ptrs[0] + off);
#endif
    ddsi_plist_t *xs = ptrs[0];
    for (size_t i = 0; i < count; i++)
      ddsi_plist_fini (&xs[i]);
    if (op & DDS_FREE_ALL_BIT)
      dds_free (ptrs[0]);
  }
}

const struct ddsi_sertype_ops ddsi_sertype_ops_plist = {
  .version = ddsi_sertype_v0,
  .arg = 0,
  .equal = sertype_plist_equal,
  .hash = sertype_plist_hash,
  .free = sertype_plist_free,
  .zero_samples = sertype_plist_zero_samples,
  .realloc_samples = sertype_plist_realloc_samples,
  .free_samples = sertype_plist_free_samples,
  .type_id = 0,
  .type_map = 0,
  .type_info = 0,
  .get_serialized_size = 0,
  .serialize_into = 0
};
