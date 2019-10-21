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
#include <string.h>
#include <assert.h>
#include "cyclonedds/ddsrt/time.h"
#include "cyclonedds/ddsrt/sync.h"
#include "cyclonedds/ddsrt/heap.h"
#include "cyclonedds/ddsrt/random.h"
#include "cyclonedds/ddsrt/hopscotch.h"
#include "cyclonedds/ddsi/q_thread.h"
#include "dds__handles.h"
#include "dds__types.h"

#define HDL_REFCOUNT_MASK  (0x0ffff000u)
#define HDL_REFCOUNT_UNIT  (0x00001000u)
#define HDL_REFCOUNT_SHIFT 12
#define HDL_PINCOUNT_MASK  (0x00000fffu)

/* Maximum number of handles is INT32_MAX - 1, but as the allocator relies on a
   random generator for finding a free one, the time spent in the dds_handle_create
   increases with an increasing number of handles.  16M handles seems likely to be
   enough and makes the likely cost of allocating a new handle somewhat more
   reasonable */
#define MAX_HANDLES (INT32_MAX / 128)

struct dds_handle_server {
  struct ddsrt_hh *ht;
  size_t count;
  ddsrt_mutex_t lock;
  ddsrt_cond_t cond;
};

static struct dds_handle_server handles;

static uint32_t handle_hash (const void *va)
{
  /* handles are already pseudo-random numbers, so not much point in hashing it again */
  const struct dds_handle_link *a = va;
  return (uint32_t) a->hdl;
}

static int handle_equal (const void *va, const void *vb)
{
  const struct dds_handle_link *a = va;
  const struct dds_handle_link *b = vb;
  return a->hdl == b->hdl;
}

dds_return_t dds_handle_server_init (void)
{
  /* called with ddsrt's singleton mutex held (see dds_init/fini) */
  if (handles.ht == NULL)
  {
    handles.ht = ddsrt_hh_new (128, handle_hash, handle_equal);
    handles.count = 0;
    ddsrt_mutex_init (&handles.lock);
    ddsrt_cond_init (&handles.cond);
  }
  return DDS_RETCODE_OK;
}

void dds_handle_server_fini (void)
{
  /* called with ddsrt's singleton mutex held (see dds_init/fini) */
  if (handles.ht != NULL)
  {
#ifndef NDEBUG
    struct ddsrt_hh_iter it;
    for (struct dds_handle_link *link = ddsrt_hh_iter_first (handles.ht, &it); link != NULL; link = ddsrt_hh_iter_next (&it))
    {
      uint32_t cf = ddsrt_atomic_ld32 (&link->cnt_flags);
      DDS_ERROR ("handle %"PRId32" pin %"PRIu32" refc %"PRIu32"%s%s%s\n", link->hdl,
                 cf & HDL_PINCOUNT_MASK, (cf & HDL_REFCOUNT_MASK) >> HDL_REFCOUNT_SHIFT,
                 cf & HDL_FLAG_PENDING ? " pending" : "",
                 cf & HDL_FLAG_CLOSING ? " closing" : "",
                 cf & HDL_FLAG_CLOSED ? " closed" : "");
    }
    assert (ddsrt_hh_iter_first (handles.ht, &it) == NULL);
#endif
    ddsrt_hh_free (handles.ht);
    ddsrt_cond_destroy (&handles.cond);
    ddsrt_mutex_destroy (&handles.lock);
    handles.ht = NULL;
  }
}

static bool hhadd (struct ddsrt_hh *ht, void *elem) { return ddsrt_hh_add (ht, elem); }
static dds_handle_t dds_handle_create_int (struct dds_handle_link *link)
{
  ddsrt_atomic_st32 (&link->cnt_flags, HDL_FLAG_PENDING | HDL_REFCOUNT_UNIT | 1u);
  do {
    do {
      link->hdl = (int32_t) (ddsrt_random () & INT32_MAX);
    } while (link->hdl == 0 || link->hdl >= DDS_MIN_PSEUDO_HANDLE);
  } while (!hhadd (handles.ht, link));
  return link->hdl;
}

dds_handle_t dds_handle_create (struct dds_handle_link *link)
{
  dds_handle_t ret;
  ddsrt_mutex_lock (&handles.lock);
  if (handles.count == MAX_HANDLES)
  {
    ddsrt_mutex_unlock (&handles.lock);
    ret = DDS_RETCODE_OUT_OF_RESOURCES;
  }
  else
  {
    handles.count++;
    ret = dds_handle_create_int (link);
    ddsrt_mutex_unlock (&handles.lock);
    assert (ret > 0);
  }
  return ret;
}

dds_return_t dds_handle_register_special (struct dds_handle_link *link, dds_handle_t handle)
{
  dds_return_t ret;
  if (handle <= 0)
    return DDS_RETCODE_BAD_PARAMETER;
  ddsrt_mutex_lock (&handles.lock);
  if (handles.count == MAX_HANDLES)
  {
    ddsrt_mutex_unlock (&handles.lock);
    ret = DDS_RETCODE_OUT_OF_RESOURCES;
  }
  else
  {
    handles.count++;
    ddsrt_atomic_st32 (&link->cnt_flags, HDL_FLAG_PENDING | HDL_REFCOUNT_UNIT | 1u);
    link->hdl = handle;
    if (hhadd (handles.ht, link))
      ret = handle;
    else
      ret = DDS_RETCODE_BAD_PARAMETER;
    ddsrt_mutex_unlock (&handles.lock);
    assert (ret > 0);
  }
  return ret;
}

void dds_handle_unpend (struct dds_handle_link *link)
{
#ifndef NDEBUG
  uint32_t cf = ddsrt_atomic_ld32 (&link->cnt_flags);
  assert ((cf & HDL_FLAG_PENDING));
  assert (!(cf & HDL_FLAG_CLOSED));
  assert (!(cf & HDL_FLAG_CLOSING));
  assert ((cf & HDL_REFCOUNT_MASK) >= HDL_REFCOUNT_UNIT);
  assert ((cf & HDL_PINCOUNT_MASK) >= 1u);
#endif
  ddsrt_atomic_and32 (&link->cnt_flags, ~HDL_FLAG_PENDING);
  dds_handle_unpin (link);
}

int32_t dds_handle_delete (struct dds_handle_link *link)
{
#ifndef NDEBUG
  uint32_t cf = ddsrt_atomic_ld32 (&link->cnt_flags);
  if (!(cf & HDL_FLAG_PENDING))
  {
    assert (cf & HDL_FLAG_CLOSING);
    assert (cf & HDL_FLAG_CLOSED);
    assert ((cf & HDL_REFCOUNT_MASK) == 0u);
  }
  assert ((cf & HDL_PINCOUNT_MASK) == 1u);
#endif
  ddsrt_mutex_lock (&handles.lock);
  int x = ddsrt_hh_remove (handles.ht, link);
  assert(x);
  (void)x;
  assert (handles.count > 0);
  handles.count--;
  ddsrt_mutex_unlock (&handles.lock);
  return DDS_RETCODE_OK;
}

static int32_t dds_handle_pin_int (dds_handle_t hdl, uint32_t delta, struct dds_handle_link **link)
{
  struct dds_handle_link dummy = { .hdl = hdl };
  int32_t rc;
  /* it makes sense to check here for initialization: the first thing any operation
     (other than create_participant) does is to call dds_handle_pin on the supplied
     entity, so checking here whether the library has been initialised helps avoid
     crashes if someone forgets to create a participant (or allows a program to
     continue after failing to create one).

     One could check that the handle is > 0, but that would catch fewer errors
     without any advantages. */
  if (handles.ht == NULL)
    return DDS_RETCODE_PRECONDITION_NOT_MET;

  ddsrt_mutex_lock (&handles.lock);
  *link = ddsrt_hh_lookup (handles.ht, &dummy);
  if (*link == NULL)
    rc = DDS_RETCODE_BAD_PARAMETER;
  else
  {
    uint32_t cf;
    /* Assume success; bail out if the object turns out to be in the process of
       being deleted */
    rc = DDS_RETCODE_OK;
    do {
      cf = ddsrt_atomic_ld32 (&(*link)->cnt_flags);
      if (cf & (HDL_FLAG_CLOSED | HDL_FLAG_CLOSING | HDL_FLAG_PENDING))
      {
        rc = DDS_RETCODE_BAD_PARAMETER;
        break;
      }
    } while (!ddsrt_atomic_cas32 (&(*link)->cnt_flags, cf, cf + delta));
  }
  ddsrt_mutex_unlock (&handles.lock);
  return rc;
}

int32_t dds_handle_pin (dds_handle_t hdl, struct dds_handle_link **link)
{
  return dds_handle_pin_int (hdl, 1u, link);
}

int32_t dds_handle_pin_and_ref (dds_handle_t hdl, struct dds_handle_link **link)
{
  return dds_handle_pin_int (hdl, HDL_REFCOUNT_UNIT + 1u, link);
}

void dds_handle_repin (struct dds_handle_link *link)
{
  uint32_t x = ddsrt_atomic_inc32_nv (&link->cnt_flags);
  assert (!(x & HDL_FLAG_CLOSED));
  (void) x;
}

void dds_handle_unpin (struct dds_handle_link *link)
{
#ifndef NDEBUG
  uint32_t cf = ddsrt_atomic_ld32 (&link->cnt_flags);
  assert (!(cf & HDL_FLAG_CLOSED));
  if (cf & HDL_FLAG_CLOSING)
    assert ((cf & HDL_PINCOUNT_MASK) > 1u);
  else
    assert ((cf & HDL_PINCOUNT_MASK) >= 1u);
#endif
  ddsrt_mutex_lock (&handles.lock);
  if ((ddsrt_atomic_dec32_nv (&link->cnt_flags) & (HDL_FLAG_CLOSING | HDL_PINCOUNT_MASK)) == (HDL_FLAG_CLOSING | 1u))
  {
    ddsrt_cond_broadcast (&handles.cond);
  }
  ddsrt_mutex_unlock (&handles.lock);
}

void dds_handle_add_ref (struct dds_handle_link *link)
{
  ddsrt_atomic_add32 (&link->cnt_flags, HDL_REFCOUNT_UNIT);
}

bool dds_handle_drop_ref (struct dds_handle_link *link)
{
  assert ((ddsrt_atomic_ld32 (&link->cnt_flags) & HDL_REFCOUNT_MASK) != 0);
  uint32_t old, new;
  do {
    old = ddsrt_atomic_ld32 (&link->cnt_flags);
    if ((old & HDL_REFCOUNT_MASK) != HDL_REFCOUNT_UNIT)
      new = old - HDL_REFCOUNT_UNIT;
    else
      new = (old - HDL_REFCOUNT_UNIT) | HDL_FLAG_CLOSING;
  } while (!ddsrt_atomic_cas32 (&link->cnt_flags, old, new));
  return (new & HDL_REFCOUNT_MASK) == 0;
}

void dds_handle_close_wait (struct dds_handle_link *link)
{
#ifndef NDEBUG
  uint32_t cf = ddsrt_atomic_ld32 (&link->cnt_flags);
  assert ((cf & HDL_FLAG_CLOSING));
  assert (!(cf & HDL_FLAG_CLOSED));
  assert ((cf & HDL_REFCOUNT_MASK) == 0u);
  assert ((cf & HDL_PINCOUNT_MASK) >= 1u);
#endif
  ddsrt_mutex_lock (&handles.lock);
  while ((ddsrt_atomic_ld32 (&link->cnt_flags) & HDL_PINCOUNT_MASK) != 1u)
    ddsrt_cond_wait (&handles.cond, &handles.lock);
  /* only one thread may call close_wait on a given handle */
  assert (!(ddsrt_atomic_ld32 (&link->cnt_flags) & HDL_FLAG_CLOSED));
  ddsrt_atomic_or32 (&link->cnt_flags, HDL_FLAG_CLOSED);
  ddsrt_mutex_unlock (&handles.lock);
}

extern inline bool dds_handle_is_closed (struct dds_handle_link *link);
