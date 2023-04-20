// Copyright(c) 2006 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <assert.h>
#include <string.h>

#include "dds/ddsrt/heap.h"
#include "dds/cdr/dds_cdrstream.h"

static dds_allocator_t dds_allocator_fns = { ddsrt_malloc, ddsrt_realloc, ddsrt_free };

const struct dds_cdrstream_allocator dds_cdrstream_default_allocator = { ddsrt_malloc, ddsrt_realloc, ddsrt_free };

void * dds_alloc (size_t size)
{
  void * ret = (dds_allocator_fns.malloc) (size);
  if (ret == NULL) {
    DDS_FATAL("dds_alloc");
  } else {
    memset (ret, 0, size);
  }
  return ret;
}

void * dds_realloc (void * ptr, size_t size)
{
  void * ret = (dds_allocator_fns.realloc) (ptr, size);
  if (ret == NULL)
    DDS_FATAL("dds_realloc");
  return ret;
}

void * dds_realloc_zero (void * ptr, size_t size)
{
  void * ret = dds_realloc (ptr, size);
  if (ret)
  {
    memset (ret, 0, size);
  }
  return ret;
}

void dds_free (void * ptr)
{
  if (ptr) (dds_allocator_fns.free) (ptr);
}

char * dds_string_alloc (size_t size)
{
  return (char*) dds_alloc (size + 1);
}

char * dds_string_dup (const char * str)
{
  char * ret = NULL;
  if (str)
  {
    size_t sz = strlen (str) + 1;
    ret = dds_alloc (sz);
    memcpy (ret, str, sz);
  }
  return ret;
}

void dds_string_free (char * str)
{
  dds_free (str);
}

static void dds_sample_free_key (void *vsample, const struct dds_topic_descriptor * desc)
{
  char *sample = vsample;
  for (uint32_t i = 0; i < desc->m_nkeys; i++)
  {
    const uint32_t *op = desc->m_ops + desc->m_keys[i].m_offset;
    if (DDS_OP_TYPE (*op) == DDS_OP_VAL_STR)
      dds_free (*(char **) (sample + op[1]));
  }
}

void dds_sample_free (void * sample, const struct dds_topic_descriptor * desc, dds_free_op_t op)
{
  /* external API, so can't replace the dds_topic_decsriptor type ... */
  assert (desc);

  if (sample)
  {
    if (op & DDS_FREE_CONTENTS_BIT)
      dds_stream_free_sample (sample, &dds_cdrstream_default_allocator, desc->m_ops);
    else if (op & DDS_FREE_KEY_BIT)
      dds_sample_free_key (sample, desc);

    if (op & DDS_FREE_ALL_BIT)
      dds_free (sample);
  }
}
