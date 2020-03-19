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
#include "dds/ddsi/q_bswap.h"
#include "dds/ddsi/q_config.h"
#include "dds/ddsi/q_freelist.h"
#include "dds/ddsi/ddsi_cdrstream.h"
#include "dds/ddsi/ddsi_sertopic.h"
#include "dds/ddsi/ddsi_serdata_default.h"

static bool sertopic_default_equal (const struct ddsi_sertopic *acmn, const struct ddsi_sertopic *bcmn)
{
  const struct ddsi_sertopic_default *a = (struct ddsi_sertopic_default *) acmn;
  const struct ddsi_sertopic_default *b = (struct ddsi_sertopic_default *) bcmn;
  if (a->native_encoding_identifier != b->native_encoding_identifier)
    return false;
  if (a->type.m_size != b->type.m_size)
    return false;
  if (a->type.m_align != b->type.m_align)
    return false;
  if (a->type.m_flagset != b->type.m_flagset)
    return false;
  if (a->type.m_nkeys != b->type.m_nkeys)
    return false;
  if (
    (a->type.m_nkeys > 0) &&
    memcmp (a->type.m_keys, b->type.m_keys, a->type.m_nkeys * sizeof (*a->type.m_keys)) != 0)
    return false;
  if (a->type.m_nops != b->type.m_nops)
    return false;
  if (
    (a->type.m_nops > 0) &&
    memcmp (a->type.m_ops, b->type.m_ops, a->type.m_nops * sizeof (*a->type.m_ops)) != 0)
    return false;
  assert (a->opt_size == b->opt_size);
  return true;
}

static uint32_t sertopic_default_hash (const struct ddsi_sertopic *tpcmn)
{
  const struct ddsi_sertopic_default *tp = (struct ddsi_sertopic_default *) tpcmn;
  uint32_t h = 0;
  h = ddsrt_mh3 (&tp->native_encoding_identifier, sizeof (tp->native_encoding_identifier), h);
  h = ddsrt_mh3 (&tp->type.m_size, sizeof (tp->type.m_size), h);
  h = ddsrt_mh3 (&tp->type.m_align, sizeof (tp->type.m_align), h);
  h = ddsrt_mh3 (&tp->type.m_flagset, sizeof (tp->type.m_flagset), h);
  h = ddsrt_mh3 (tp->type.m_keys, tp->type.m_nkeys * sizeof (*tp->type.m_keys), h);
  h = ddsrt_mh3 (tp->type.m_ops, tp->type.m_nops * sizeof (*tp->type.m_ops), h);
  return h;
}

static void sertopic_default_free (struct ddsi_sertopic *tpcmn)
{
  struct ddsi_sertopic_default *tp = (struct ddsi_sertopic_default *) tpcmn;
  ddsrt_free (tp->type.m_keys);
  ddsrt_free (tp->type.m_ops);
  ddsi_sertopic_fini (&tp->c);
  ddsrt_free (tp);
}

static void sertopic_default_zero_samples (const struct ddsi_sertopic *sertopic_common, void *sample, size_t count)
{
  const struct ddsi_sertopic_default *tp = (const struct ddsi_sertopic_default *)sertopic_common;
  memset (sample, 0, tp->type.m_size * count);
}

static void sertopic_default_realloc_samples (void **ptrs, const struct ddsi_sertopic *sertopic_common, void *old, size_t oldcount, size_t count)
{
  const struct ddsi_sertopic_default *tp = (const struct ddsi_sertopic_default *)sertopic_common;
  const size_t size = tp->type.m_size;
  char *new = (oldcount == count) ? old : dds_realloc (old, size * count);
  if (new && count > oldcount)
    memset (new + size * oldcount, 0, size * (count - oldcount));
  for (size_t i = 0; i < count; i++)
  {
    void *ptr = (char *) new + i * size;
    ptrs[i] = ptr;
  }
}

static void sertopic_default_free_samples (const struct ddsi_sertopic *sertopic_common, void **ptrs, size_t count, dds_free_op_t op)
{
  if (count > 0)
  {
    const struct ddsi_sertopic_default *tp = (const struct ddsi_sertopic_default *)sertopic_common;
    const struct ddsi_sertopic_default_desc *type = &tp->type;
    const size_t size = type->m_size;
#ifndef NDEBUG
    for (size_t i = 0, off = 0; i < count; i++, off += size)
      assert ((char *)ptrs[i] == (char *)ptrs[0] + off);
#endif
    if (type->m_flagset & DDS_TOPIC_NO_OPTIMIZE)
    {
      char *ptr = ptrs[0];
      for (size_t i = 0; i < count; i++)
      {
        dds_stream_free_sample (ptr, type->m_ops);
        ptr += size;
      }
    }
    if (op & DDS_FREE_ALL_BIT)
    {
      dds_free (ptrs[0]);
    }
  }
}

const struct ddsi_sertopic_ops ddsi_sertopic_ops_default = {
  .equal = sertopic_default_equal,
  .hash = sertopic_default_hash,
  .free = sertopic_default_free,
  .zero_samples = sertopic_default_zero_samples,
  .realloc_samples = sertopic_default_realloc_samples,
  .free_samples = sertopic_default_free_samples
};
