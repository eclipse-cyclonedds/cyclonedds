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

#include "os/os.h"
#include "ddsi/q_md5.h"
#include "ddsi/q_bswap.h"
#include "ddsi/q_config.h"
#include "ddsi/q_freelist.h"
#include "ddsi/ddsi_sertopic.h"
#include "ddsi/ddsi_serdata_default.h"

/* FIXME: sertopic /= ddstopic so a lot of stuff needs to be moved here from dds_topic.c and the free function needs to be implemented properly */

static void sertopic_default_deinit (struct ddsi_sertopic *tp)
{
  (void)tp;
}

static void sertopic_default_zero_samples (const struct ddsi_sertopic *sertopic_common, void *sample, size_t count)
{
  const struct ddsi_sertopic_default *tp = (const struct ddsi_sertopic_default *)sertopic_common;
  memset (sample, 0, tp->type->m_size * count);
}

static void sertopic_default_realloc_samples (void **ptrs, const struct ddsi_sertopic *sertopic_common, void *old, size_t oldcount, size_t count)
{
  const struct ddsi_sertopic_default *tp = (const struct ddsi_sertopic_default *)sertopic_common;
  const size_t size = tp->type->m_size;
  char *new = dds_realloc (old, size * count);
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
    const struct dds_topic_descriptor *type = tp->type;
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
        dds_sample_free (ptr, type, DDS_FREE_CONTENTS);
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
  .deinit = sertopic_default_deinit,
  .zero_samples = sertopic_default_zero_samples,
  .realloc_samples = sertopic_default_realloc_samples,
  .free_samples = sertopic_default_free_samples
};
