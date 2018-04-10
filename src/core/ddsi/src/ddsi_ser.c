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
#include "ddsi/ddsi_ser.h"

#include <stddef.h>
#include <ctype.h>
#include <assert.h>
#include <string.h>

#include "os/os_stdlib.h"
#include "os/os_defs.h"
#include "os/os_thread.h"
#include "os/os_heap.h"
#include "os/os_atomics.h"
#include "ddsi/sysdeps.h"
#include "ddsi/q_md5.h"
#include "ddsi/q_bswap.h"
#include "ddsi/q_config.h"
#include "ddsi/q_freelist.h"
#include "q__osplser.h"

#define MAX_POOL_SIZE 16384

#ifndef NDEBUG
static int ispowerof2_size (size_t x)
{
  return x > 0 && !(x & (x-1));
}
#endif

static size_t alignup_size (size_t x, size_t a);
static serstate_t serstate_allocnew (serstatepool_t pool, const struct sertopic * topic);

serstatepool_t ddsi_serstatepool_new (void)
{
  serstatepool_t pool;
  pool = os_malloc (sizeof (*pool));
  nn_freelist_init (&pool->freelist, MAX_POOL_SIZE, offsetof (struct serstate, next));
  return pool;
}

static void serstate_free_wrap (void *elem)
{
  serstate_free (elem);
}

void ddsi_serstatepool_free (serstatepool_t pool)
{
  nn_freelist_fini (&pool->freelist, serstate_free_wrap);
  TRACE (("ddsi_serstatepool_free(%p)\n", pool));
  os_free (pool);
}

int ddsi_serdata_refcount_is_1 (serdata_t serdata)
{
  return (os_atomic_ld32 (&serdata->v.st->refcount) == 1);
}

serdata_t ddsi_serdata_ref (serdata_t serdata)
{
  os_atomic_inc32 (&serdata->v.st->refcount);
  return serdata;
}

void ddsi_serdata_unref (serdata_t serdata)
{
  ddsi_serstate_release (serdata->v.st);
}

nn_mtime_t ddsi_serdata_twrite (const struct serdata *serdata)
{
  return ddsi_serstate_twrite (serdata->v.st);
}

void ddsi_serdata_set_twrite (serdata_t serdata, nn_mtime_t twrite)
{
  ddsi_serstate_set_twrite (serdata->v.st, twrite);
}

serstate_t ddsi_serstate_new (serstatepool_t pool, const struct sertopic * topic)
{
  serstate_t st;
  if ((st = nn_freelist_pop (&pool->freelist)) != NULL)
    serstate_init (st, topic);
  else
    st = serstate_allocnew (pool, topic);
  return st;
}

serdata_t ddsi_serstate_fix (serstate_t st)
{
  /* see serialize_raw_private() */
  ddsi_serstate_append_aligned (st, 0, 4);
  return st->data;
}

nn_mtime_t ddsi_serstate_twrite (const struct serstate *serstate)
{
  assert (serstate->twrite.v >= 0);
  return serstate->twrite;
}

void ddsi_serstate_set_twrite (serstate_t st, nn_mtime_t twrite)
{
  st->twrite = twrite;
}

void ddsi_serstate_append_blob (serstate_t st, size_t align, size_t sz, const void *data)
{
  char *p = ddsi_serstate_append_aligned (st, sz, align);
  memcpy (p, data, sz);
}

void ddsi_serstate_set_msginfo
(
  serstate_t st, unsigned statusinfo, nn_wctime_t timestamp,
  void * dummy
)
{
  serdata_t d = st->data;
  d->v.msginfo.statusinfo = statusinfo;
  d->v.msginfo.timestamp = timestamp;
}

uint32_t ddsi_serdata_size (const struct serdata *serdata)
{
  const struct serstate *st = serdata->v.st;
  if (serdata->v.st->kind == STK_EMPTY)
    return 0;
  else
    return (uint32_t) (sizeof (struct CDRHeader) + st->pos);
}

int ddsi_serdata_is_key (const struct serdata * serdata)
{
  return serdata->v.st->kind == STK_KEY;
}

int ddsi_serdata_is_empty (const struct serdata * serdata)
{
  return serdata->v.st->kind == STK_EMPTY;
}

/* Internal static functions */

static serstate_t serstate_allocnew (serstatepool_t pool, const struct sertopic * topic)
{
  serstate_t st = os_malloc (sizeof (*st));
  size_t size;

  memset (st, 0, sizeof (*st));

  st->size = 128;
  st->pool = pool;

  size = offsetof (struct serdata, data) + st->size;
  st->data = os_malloc (size);
  memset (st->data, 0, size);
  st->data->v.st = st;
  serstate_init (st, topic);
  return st;
}

void * ddsi_serstate_append (serstate_t st, size_t n)
{
  char *p;
  if (st->pos + n > st->size)
  {
    size_t size1 = alignup_size (st->pos + n, 128);
    serdata_t data1 = os_realloc (st->data, offsetof (struct serdata, data) + size1);
    st->data = data1;
    st->size = size1;
  }
  assert (st->pos + n <= st->size);
  p = st->data->data + st->pos;
  st->pos += n;
  return p;
}

void ddsi_serstate_release (serstate_t st)
{
  if (os_atomic_dec32_ov (&st->refcount) == 1)
  {
    serstatepool_t pool = st->pool;
    sertopic_free ((sertopic_t) st->topic);
    if (!nn_freelist_push (&pool->freelist, st))
      serstate_free (st);
  }
}

void * ddsi_serstate_append_align (serstate_t st, size_t sz)
{
  return ddsi_serstate_append_aligned (st, sz, sz);
}

void * ddsi_serstate_append_aligned (serstate_t st, size_t n, size_t a)
{
  /* Simply align st->pos, without verifying it fits in the allocated
     buffer: ddsi_serstate_append() is called immediately afterward and will
     grow the buffer as soon as the end of the requested space no
     longer fits. */
  size_t pos0 = st->pos;
  char *p;
  assert (ispowerof2_size (a));
  st->pos = alignup_size (st->pos, a);
  p = ddsi_serstate_append (st, n);
  if (p && st->pos > pos0)
    memset (st->data->data + pos0, 0, st->pos - pos0);
  return p;
}

static size_t alignup_size (size_t x, size_t a)
{
  size_t m = a-1;
  assert (ispowerof2_size (a));
  return (x+m) & ~m;
}
