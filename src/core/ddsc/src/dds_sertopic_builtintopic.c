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
#include "ddsc/dds.h"
#include "dds__serdata_builtintopic.h"

/* FIXME: sertopic /= ddstopic so a lot of stuff needs to be moved here from dds_topic.c and the free function needs to be implemented properly */

struct ddsi_sertopic *new_sertopic_builtintopic (enum ddsi_sertopic_builtintopic_type type, const char *name, const char *typename)
{
  struct ddsi_sertopic_builtintopic *tp = os_malloc (sizeof (*tp));
  tp->c.iid = ddsi_iid_gen();
  tp->c.name = dds_string_dup (name);
  tp->c.typename = dds_string_dup (typename);
  const size_t name_typename_size = strlen (tp->c.name) + 1 + strlen (tp->c.typename) + 1;
  tp->c.name_typename = dds_alloc (name_typename_size);
  snprintf (tp->c.name_typename, name_typename_size, "%s/%s", tp->c.name, tp->c.typename);
  tp->c.ops = &ddsi_sertopic_ops_builtintopic;
  tp->c.serdata_ops = &ddsi_serdata_ops_builtintopic;
  tp->c.serdata_basehash = ddsi_sertopic_compute_serdata_basehash (tp->c.serdata_ops);
  tp->c.status_cb = 0;
  tp->c.status_cb_entity = NULL;
  os_atomic_st32 (&tp->c.refc, 1);
  tp->type = type;
  return &tp->c;
}

static void sertopic_builtin_deinit (struct ddsi_sertopic *tp)
{
  (void)tp;
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
  char *new = dds_realloc (old, size * count);
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
  .deinit = sertopic_builtin_deinit,
  .zero_samples = sertopic_builtin_zero_samples,
  .realloc_samples = sertopic_builtin_realloc_samples,
  .free_samples = sertopic_builtin_free_samples
};
