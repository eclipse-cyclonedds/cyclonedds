/*
 * Copyright(c) 2020 ADLINK Technology Limited and others
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
#include "dds/ddsi/ddsi_plist_generic.h"
#include "dds/ddsi/ddsi_sertopic.h"
#include "dds/ddsi/ddsi_serdata_pserop.h"

static bool sertopic_pserop_equal (const struct ddsi_sertopic *acmn, const struct ddsi_sertopic *bcmn)
{
  const struct ddsi_sertopic_pserop *a = (struct ddsi_sertopic_pserop *) acmn;
  const struct ddsi_sertopic_pserop *b = (struct ddsi_sertopic_pserop *) bcmn;
  if (a->native_encoding_identifier != b->native_encoding_identifier)
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

static uint32_t sertopic_pserop_hash (const struct ddsi_sertopic *tpcmn)
{
  const struct ddsi_sertopic_pserop *tp = (struct ddsi_sertopic_pserop *) tpcmn;
  uint32_t h = 0;
  h = ddsrt_mh3 (&tp->native_encoding_identifier, sizeof (tp->native_encoding_identifier), h);
  h = ddsrt_mh3 (&tp->memsize, sizeof (tp->memsize), h);
  h = ddsrt_mh3 (&tp->nops, sizeof (tp->nops), h);
  h = ddsrt_mh3 (tp->ops, tp->nops * sizeof (*tp->ops), h);
  h = ddsrt_mh3 (&tp->nops_key, sizeof (tp->nops_key), h);
  if (tp->ops_key)
    h = ddsrt_mh3 (tp->ops_key, tp->nops_key * sizeof (*tp->ops_key), h);
  return h;
}

static void sertopic_pserop_free (struct ddsi_sertopic *tpcmn)
{
  struct ddsi_sertopic_pserop *tp = (struct ddsi_sertopic_pserop *) tpcmn;
  ddsi_sertopic_fini (&tp->c);
  ddsrt_free (tp);
}

static void sertopic_pserop_zero_samples (const struct ddsi_sertopic *sertopic_common, void *sample, size_t count)
{
  const struct ddsi_sertopic_pserop *tp = (const struct ddsi_sertopic_pserop *)sertopic_common;
  memset (sample, 0, tp->memsize * count);
}

static void sertopic_pserop_realloc_samples (void **ptrs, const struct ddsi_sertopic *sertopic_common, void *old, size_t oldcount, size_t count)
{
  const struct ddsi_sertopic_pserop *tp = (const struct ddsi_sertopic_pserop *)sertopic_common;
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

static void sertopic_pserop_free_samples (const struct ddsi_sertopic *sertopic_common, void **ptrs, size_t count, dds_free_op_t op)
{
  if (count > 0)
  {
    const struct ddsi_sertopic_pserop *tp = (const struct ddsi_sertopic_pserop *)sertopic_common;
    const size_t size = tp->memsize;
#ifndef NDEBUG
    for (size_t i = 0, off = 0; i < count; i++, off += size)
      assert ((char *)ptrs[i] == (char *)ptrs[0] + off);
#endif
    char *ptr = ptrs[0];
    for (size_t i = 0; i < count; i++)
    {
      plist_fini_generic (ptr, tp->ops, false);
      ptr += size;
    }
    if (op & DDS_FREE_ALL_BIT)
    {
      dds_free (ptrs[0]);
    }
  }
}

const struct ddsi_sertopic_ops ddsi_sertopic_ops_pserop = {
  .equal = sertopic_pserop_equal,
  .hash = sertopic_pserop_hash,
  .free = sertopic_pserop_free,
  .zero_samples = sertopic_pserop_zero_samples,
  .realloc_samples = sertopic_pserop_realloc_samples,
  .free_samples = sertopic_pserop_free_samples
};
