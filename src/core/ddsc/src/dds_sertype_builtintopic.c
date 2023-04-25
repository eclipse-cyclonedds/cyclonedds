// Copyright(c) 2006 to 2022 ZettaScale Technology and others
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

#include "dds/dds.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/md5.h"
#include "dds/ddsi/ddsi_freelist.h"
#include "dds/ddsi/ddsi_sertype.h"
#include "dds/ddsi/ddsi_iid.h"
#include "dds__serdata_builtintopic.h"

static struct ddsi_sertype *new_sertype_builtintopic_impl (
    enum ddsi_sertype_builtintopic_entity_kind entity_kind,
    const char *typename,
    const struct ddsi_serdata_ops *serdata_ops)
{
  struct ddsi_sertype_builtintopic *tp = ddsrt_malloc (sizeof (*tp));
  ddsi_sertype_init (&tp->c, typename, &ddsi_sertype_ops_builtintopic, serdata_ops, false);
  tp->entity_kind = entity_kind;
  return &tp->c;
}

struct ddsi_sertype *dds_new_sertype_builtintopic (enum ddsi_sertype_builtintopic_entity_kind entity_kind, const char *typename)
{
  return new_sertype_builtintopic_impl (entity_kind, typename, &ddsi_serdata_ops_builtintopic);
}

static void sertype_builtin_free (struct ddsi_sertype *tp)
{
  ddsi_sertype_fini (tp);
  ddsrt_free (tp);
}

static bool sertype_builtin_equal (const struct ddsi_sertype *acmn, const struct ddsi_sertype *bcmn)
{
  const struct ddsi_sertype_builtintopic *a = (struct ddsi_sertype_builtintopic *) acmn;
  const struct ddsi_sertype_builtintopic *b = (struct ddsi_sertype_builtintopic *) bcmn;
  return a->entity_kind == b->entity_kind;
}

static uint32_t sertype_builtin_hash (const struct ddsi_sertype *tpcmn)
{
  const struct ddsi_sertype_builtintopic *tp = (struct ddsi_sertype_builtintopic *) tpcmn;
  return (uint32_t) tp->entity_kind;
}

static void free_pp (void *vsample)
{
  dds_builtintopic_participant_t *sample = vsample;
  dds_delete_qos (sample->qos);
  sample->qos = NULL;
}

#ifdef DDS_HAS_TOPIC_DISCOVERY

struct ddsi_sertype *dds_new_sertype_builtintopic_topic (enum ddsi_sertype_builtintopic_entity_kind entity_kind, const char *typename)
{
  return new_sertype_builtintopic_impl (entity_kind, typename, &ddsi_serdata_ops_builtintopic_topic);
}

static void free_topic (void *vsample)
{
  dds_builtintopic_topic_t *sample = vsample;
  dds_free (sample->topic_name);
  dds_free (sample->type_name);
  dds_delete_qos (sample->qos);
  sample->topic_name = sample->type_name = NULL;
  sample->qos = NULL;
}

#endif /* DDS_HAS_TOPIC_DISCOVERY */

static void free_endpoint (void *vsample)
{
  dds_builtintopic_endpoint_t *sample = vsample;
  dds_free (sample->topic_name);
  dds_free (sample->type_name);
  dds_delete_qos (sample->qos);
  sample->topic_name = sample->type_name = NULL;
  sample->qos = NULL;
}

static size_t get_size (enum ddsi_sertype_builtintopic_entity_kind entity_kind)
{
  switch (entity_kind)
  {
    case DSBT_PARTICIPANT:
      return sizeof (dds_builtintopic_participant_t);
    case DSBT_TOPIC:
#ifdef DDS_HAS_TOPIC_DISCOVERY
      return sizeof (dds_builtintopic_topic_t);
#else
      break;
#endif
    case DSBT_READER:
    case DSBT_WRITER:
      return sizeof (dds_builtintopic_endpoint_t);
  }
  assert (0);
  return 0;
}

static void sertype_builtin_zero_samples (const struct ddsi_sertype *sertype_common, void *samples, size_t count)
{
  const struct ddsi_sertype_builtintopic *tp = (const struct ddsi_sertype_builtintopic *)sertype_common;
  size_t size = get_size (tp->entity_kind);
  memset (samples, 0, size * count);
}

static void sertype_builtin_realloc_samples (void **ptrs, const struct ddsi_sertype *sertype_common, void *old, size_t oldcount, size_t count)
{
  const struct ddsi_sertype_builtintopic *tp = (const struct ddsi_sertype_builtintopic *)sertype_common;
  const size_t size = get_size (tp->entity_kind);
  char *new = (oldcount == count) ? old : dds_realloc (old, size * count);
  if (new && count > oldcount)
    memset (new + size * oldcount, 0, size * (count - oldcount));
  for (size_t i = 0; i < count; i++)
  {
    void *ptr = (char *) new + i * size;
    ptrs[i] = ptr;
  }
}

static void sertype_builtin_free_samples (const struct ddsi_sertype *sertype_common, void **ptrs, size_t count, dds_free_op_t op)
{
  if (count > 0)
  {
    const struct ddsi_sertype_builtintopic *tp = (const struct ddsi_sertype_builtintopic *)sertype_common;
    const size_t size = get_size (tp->entity_kind);
#ifndef NDEBUG
    for (size_t i = 0, off = 0; i < count; i++, off += size)
      assert ((char *)ptrs[i] == (char *)ptrs[0] + off);
#endif
    if (op & DDS_FREE_CONTENTS_BIT)
    {
      void (*f) (void *) = 0;
      char *ptr = ptrs[0];
      switch (tp->entity_kind)
      {
        case DSBT_PARTICIPANT:
          f = free_pp;
          break;
        case DSBT_TOPIC:
#ifdef DDS_HAS_TOPIC_DISCOVERY
          f = free_topic;
#endif
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

const struct ddsi_sertype_ops ddsi_sertype_ops_builtintopic = {
  .version = ddsi_sertype_v0,
  .arg = 0,
  .equal = sertype_builtin_equal,
  .hash = sertype_builtin_hash,
  .free = sertype_builtin_free,
  .zero_samples = sertype_builtin_zero_samples,
  .realloc_samples = sertype_builtin_realloc_samples,
  .free_samples = sertype_builtin_free_samples,
  .type_id = 0,
  .type_map = 0,
  .type_info = 0,
  .get_serialized_size = 0,
  .serialize_into = 0
};
