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

#include "dds/ddsrt/mh3.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsi/ddsi_plist.h"
#include "dds/ddsi/ddsi_sertopic.h"
#include "dds/ddsi/ddsi_serdata_plist.h"

static bool sertopic_plist_equal (const struct ddsi_sertopic *acmn, const struct ddsi_sertopic *bcmn)
{
  const struct ddsi_sertopic_plist *a = (struct ddsi_sertopic_plist *) acmn;
  const struct ddsi_sertopic_plist *b = (struct ddsi_sertopic_plist *) bcmn;
  if (a->native_encoding_identifier != b->native_encoding_identifier)
    return false;
  if (a->keyparam != b->keyparam)
    return false;
  return true;
}

static uint32_t sertopic_plist_hash (const struct ddsi_sertopic *tpcmn)
{
  const struct ddsi_sertopic_plist *tp = (struct ddsi_sertopic_plist *) tpcmn;
  uint32_t h = 0;
  h = ddsrt_mh3 (&tp->native_encoding_identifier, sizeof (tp->native_encoding_identifier), h);
  h = ddsrt_mh3 (&tp->keyparam, sizeof (tp->keyparam), h);
  return h;
}

static void sertopic_plist_free (struct ddsi_sertopic *tpcmn)
{
  struct ddsi_sertopic_plist *tp = (struct ddsi_sertopic_plist *) tpcmn;
  ddsi_sertopic_fini (&tp->c);
  ddsrt_free (tp);
}

static void sertopic_plist_zero_samples (const struct ddsi_sertopic *sertopic_common, void *sample, size_t count)
{
  (void) sertopic_common;
  ddsi_plist_t *xs = sample;
  for (size_t i = 0; i < count; i++)
    ddsi_plist_init_empty (&xs[i]);
}

static void sertopic_plist_realloc_samples (void **ptrs, const struct ddsi_sertopic *sertopic_common, void *old, size_t oldcount, size_t count)
{
  (void) sertopic_common;
  ddsi_plist_t *new = (oldcount == count) ? old : dds_realloc (old, count * sizeof (ddsi_plist_t));
  if (new)
  {
    for (size_t i = count; i < oldcount; i++)
      ddsi_plist_init_empty (&new[i]);
    for (size_t i = 0; i < count; i++)
      ptrs[i] = &new[i];
  }
}

static void sertopic_plist_free_samples (const struct ddsi_sertopic *sertopic_common, void **ptrs, size_t count, dds_free_op_t op)
{
  (void) sertopic_common;
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

const struct ddsi_sertopic_ops ddsi_sertopic_ops_plist = {
  .equal = sertopic_plist_equal,
  .hash = sertopic_plist_hash,
  .free = sertopic_plist_free,
  .zero_samples = sertopic_plist_zero_samples,
  .realloc_samples = sertopic_plist_realloc_samples,
  .free_samples = sertopic_plist_free_samples
};
