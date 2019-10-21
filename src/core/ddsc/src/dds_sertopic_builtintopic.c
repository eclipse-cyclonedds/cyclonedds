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

#include "cyclonedds/dds.h"
#include "cyclonedds/ddsrt/heap.h"
#include "cyclonedds/ddsrt/md5.h"
#include "cyclonedds/ddsi/q_bswap.h"
#include "cyclonedds/ddsi/q_config.h"
#include "cyclonedds/ddsi/q_freelist.h"
#include "cyclonedds/ddsi/ddsi_sertopic.h"
#include "cyclonedds/ddsi/ddsi_iid.h"
#include "dds__serdata_builtintopic.h"

/* FIXME: sertopic /= ddstopic so a lot of stuff needs to be moved here from dds_topic.c and the free function needs to be implemented properly */

struct ddsi_sertopic *new_sertopic_builtintopic (enum ddsi_sertopic_builtintopic_type type, const char *name, const char *typename, struct q_globals *gv)
{
  struct ddsi_sertopic_builtintopic *tp = ddsrt_malloc (sizeof (*tp));
  ddsi_sertopic_init (&tp->c, name, typename, &ddsi_sertopic_ops_builtintopic, &ddsi_serdata_ops_builtintopic, false);
  tp->type = type;
  tp->gv = gv;
  return &tp->c;
}

static void sertopic_builtin_free (struct ddsi_sertopic *tp)
{
  ddsi_sertopic_fini (tp);
  ddsrt_free (tp);
}

static void free_pp (void *vsample)
{
  dds_builtintopic_participant_t *sample = vsample;
  dds_delete_qos (sample->qos);
  sample->qos = NULL;
}

static void free_endpoint (void *vsample)
{
  dds_builtintopic_endpoint_t *sample = vsample;
  dds_free (sample->topic_name);
  dds_free (sample->type_name);
  dds_delete_qos (sample->qos);
  sample->topic_name = sample->type_name = NULL;
  sample->qos = NULL;
}

static size_t get_size (enum ddsi_sertopic_builtintopic_type type)
{
  switch (type)
  {
    case DSBT_PARTICIPANT:
      return sizeof (dds_builtintopic_participant_t);
    case DSBT_READER:
    case DSBT_WRITER:
      return sizeof (dds_builtintopic_endpoint_t);
  }
  assert (0);
  return 0;
}

static void sertopic_builtin_zero_samples (const struct ddsi_sertopic *sertopic_common, void *samples, size_t count)
{
  const struct ddsi_sertopic_builtintopic *tp = (const struct ddsi_sertopic_builtintopic *)sertopic_common;
  size_t size = get_size (tp->type);
  memset (samples, 0, size * count);
}

static void sertopic_builtin_realloc_samples (void **ptrs, const struct ddsi_sertopic *sertopic_common, void *old, size_t oldcount, size_t count)
{
  const struct ddsi_sertopic_builtintopic *tp = (const struct ddsi_sertopic_builtintopic *)sertopic_common;
  const size_t size = get_size (tp->type);
  char *new = (oldcount == count) ? old : dds_realloc (old, size * count);
  if (new && count > oldcount)
    memset (new + size * oldcount, 0, size * (count - oldcount));
  for (size_t i = 0; i < count; i++)
  {
    void *ptr = (char *) new + i * size;
    ptrs[i] = ptr;
  }
}

static void sertopic_builtin_free_samples (const struct ddsi_sertopic *sertopic_common, void **ptrs, size_t count, dds_free_op_t op)
{
  if (count > 0)
  {
    const struct ddsi_sertopic_builtintopic *tp = (const struct ddsi_sertopic_builtintopic *)sertopic_common;
    const size_t size = get_size (tp->type);
#ifndef NDEBUG
    for (size_t i = 0, off = 0; i < count; i++, off += size)
      assert ((char *)ptrs[i] == (char *)ptrs[0] + off);
#endif
    if (op & DDS_FREE_CONTENTS_BIT)
    {
      void (*f) (void *) = 0;
      char *ptr = ptrs[0];
      switch (tp->type)
      {
        case DSBT_PARTICIPANT:
          f = free_pp;
          break;
        case DSBT_READER:
        case DSBT_WRITER:
          f = free_endpoint;
          break;
      }
      assert (f != 0);
      for (size_t i = 0; i < count; i++)
      {
        f (ptr);
        ptr += size;
      }
    }
    if (op & DDS_FREE_ALL_BIT)
    {
      dds_free (ptrs[0]);
    }
  }
}

const struct ddsi_sertopic_ops ddsi_sertopic_ops_builtintopic = {
  .free = sertopic_builtin_free,
  .zero_samples = sertopic_builtin_zero_samples,
  .realloc_samples = sertopic_builtin_realloc_samples,
  .free_samples = sertopic_builtin_free_samples
};
