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
#include "dds/ddsrt/time.h"
#include "dds/ddsrt/sync.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/random.h"
#include "dds/ddsrt/hopscotch.h"
#include "dds/ddsi/q_thread.h"
#include "dds__handles.h"
#include "dds__types.h"

/* FIXME: this code isn't really correct when USE_CHH is set:
   - the DDS entity code doesn't really play by the awake/asleep mechanism
   - there is no provision in the code for a handle being deleted concurrent to a lookup,
     that is, deleting handle links should also go through the GC
   entity framework needs a fair bit of rewriting anyway ... */
#define USE_CHH 0

#define HDL_FLAG_CLOSED    (0x80000000u)

/* ref count: # outstanding references to this handle/object (not so sure it is
   ideal to have a one-to-one mapping between the two, but that is what the rest
   of the code assumes at the moment); so this limits one to having, e.g., no
   more than 64k endpoints referencing the same topic */
#define HDL_REFCOUNT_MASK  (0x0ffff000u)
#define HDL_REFCOUNT_UNIT  (0x00001000u)
#define HDL_REFCOUNT_SHIFT 12

/* pin count: # concurrent operations, so allowing up to 4096 threads had better
   be enough ... */
#define HDL_PINCOUNT_MASK  (0x00000fffu)
#define HDL_PINCOUNT_UNIT  (0x00000001u)

/* Maximum number of handles is INT32_MAX - 1, but as the allocator relies on a
   random generator for finding a free one, the time spent in the dds_handle_create
   increases with an increasing number of handles.  16M handles seems likely to be
   enough and makes the likely cost of allocating a new handle somewhat more
   reasonable */
#define MAX_HANDLES (INT32_MAX / 128)

struct dds_handle_server {
#if USE_CHH
  struct ddsrt_chh *ht;
#else
  struct ddsrt_hh *ht;
#endif
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
#if USE_CHH
  handles.ht = ddsrt_chh_new (128, handle_hash, handle_equal, free_via_gc);
#else
  handles.ht = ddsrt_hh_new (128, handle_hash, handle_equal);
#endif
  handles.count = 0;
  ddsrt_mutex_init (&handles.lock);
  ddsrt_cond_init (&handles.cond);
  return DDS_RETCODE_OK;
}

void dds_handle_server_fini (void)
{
#if USE_CHH
#ifndef NDEBUG
  struct ddsrt_chh_iter it;
  assert (ddsrt_chh_iter_first (handles.ht, &it) == NULL);
#endif
  ddsrt_chh_free (handles.ht);
#else /* USE_CHH */
#ifndef NDEBUG
  struct ddsrt_hh_iter it;
  for (struct dds_handle_link *link = ddsrt_hh_iter_first (handles.ht, &it); link != NULL; link = ddsrt_hh_iter_next (&it))
  {
    uint32_t cf = ddsrt_atomic_ld32 (&link->cnt_flags);
    DDS_ERROR ("handle %"PRId32" pin %"PRIu32" ref %"PRIu32" %s\n", link->hdl,
               cf & HDL_PINCOUNT_MASK, (cf & HDL_REFCOUNT_MASK) >> HDL_REFCOUNT_SHIFT,
               cf & HDL_FLAG_CLOSED ? "closed" : "open");
  }
  assert (ddsrt_hh_iter_first (handles.ht, &it) == NULL);
#endif
  ddsrt_hh_free (handles.ht);
#endif /* USE_CHH */
  ddsrt_cond_destroy (&handles.cond);
  ddsrt_mutex_destroy (&handles.lock);
  handles.ht = NULL;
}

#if USE_CHH
static bool hhadd (struct ddsrt_chh *ht, void *elem) { return ddsrt_chh_add (ht, elem); }
#else
static bool hhadd (struct ddsrt_hh *ht, void *elem) { return ddsrt_hh_add (ht, elem); }
#endif
static dds_handle_t dds_handle_create_int (struct dds_handle_link *link)
{
  ddsrt_atomic_st32 (&link->cnt_flags, HDL_REFCOUNT_UNIT);
  do {
    do {
      link->hdl = (int32_t) (ddsrt_random () & INT32_MAX);
    } while (link->hdl == 0 || link->hdl >= DDS_MIN_PSEUDO_HANDLE);
  } while (!hhadd (handles.ht, link));
  return link->hdl;
}

dds_handle_t dds_handle_create (struct dds_handle_link *link)
{
#if USE_CHH
  struct thread_state1 * const ts1 = lookup_thread_state ();
#endif
  dds_handle_t ret;
#if USE_CHH
  thread_state_awake (ts1);
#endif
  ddsrt_mutex_lock (&handles.lock);
  if (handles.count == MAX_HANDLES)
  {
    ddsrt_mutex_unlock (&handles.lock);
    ret = DDS_RETCODE_OUT_OF_RESOURCES;
  }
  else
  {
    handles.count++;
#if USE_CHH
    ddsrt_mutex_unlock (&handles.lock);
    ret = dds_handle_create_int (link);
#else
    ret = dds_handle_create_int (link);
    ddsrt_mutex_unlock (&handles.lock);
#endif
    assert (ret > 0);
  }
#if USE_CHH
  thread_state_asleep (ts1);
#endif
  return ret;
}

dds_return_t dds_handle_register_special (struct dds_handle_link *link, dds_handle_t handle)
{
#if USE_CHH
  struct thread_state1 * const ts1 = lookup_thread_state ();
#endif
  dds_return_t ret;
  if (handle <= 0)
    return DDS_RETCODE_BAD_PARAMETER;
#if USE_CHH
  thread_state_awake (ts1);
#endif
  ddsrt_mutex_lock (&handles.lock);
  if (handles.count == MAX_HANDLES)
  {
    ddsrt_mutex_unlock (&handles.lock);
    ret = DDS_RETCODE_OUT_OF_RESOURCES;
  }
  else
  {
    handles.count++;
    ddsrt_atomic_st32 (&link->cnt_flags, HDL_REFCOUNT_UNIT);
    link->hdl = handle;
#if USE_CHH
    ddsrt_mutex_unlock (&handles.lock);
    if (hhadd (handles.ht, link))
      ret = handle;
    else
      ret = DDS_RETCODE_BAD_PARAMETER;
    return link->hdl;
#else
    if (hhadd (handles.ht, link))
      ret = handle;
    else
      ret = DDS_RETCODE_BAD_PARAMETER;
    ddsrt_mutex_unlock (&handles.lock);
#endif
    assert (ret > 0);
  }
#if USE_CHH
  thread_state_asleep (ts1);
#endif
  return ret;
}

void dds_handle_close (struct dds_handle_link *link)
{
  ddsrt_atomic_or32 (&link->cnt_flags, HDL_FLAG_CLOSED);
}

int32_t dds_handle_delete (struct dds_handle_link *link, dds_duration_t timeout)
{
#if USE_CHH
  struct thread_state1 * const ts1 = lookup_thread_state ();
#endif
  assert (ddsrt_atomic_ld32 (&link->cnt_flags) & HDL_FLAG_CLOSED);
  ddsrt_mutex_lock (&handles.lock);
  if ((ddsrt_atomic_ld32 (&link->cnt_flags) & HDL_PINCOUNT_MASK) != 0)
  {
    /* FIXME: there is no sensible solution when this times out, so it must
       never do that ... */
    const dds_time_t abstimeout = dds_time () + timeout;
    while ((ddsrt_atomic_ld32 (&link->cnt_flags) & HDL_PINCOUNT_MASK) != 0)
    {
      if (!ddsrt_cond_waituntil (&handles.cond, &handles.lock, abstimeout))
      {
        ddsrt_mutex_unlock (&handles.lock);
        fprintf (stderr, "** timeout in handle_delete **\n");
        return DDS_RETCODE_TIMEOUT;
      }
    }
  }
#if USE_CHH
  thread_state_awake (ts1);
  int x = ddsrt_chh_remove (handles.ht, link);
  thread_state_asleep (ts1);
#else
  int x = ddsrt_hh_remove (handles.ht, link);
#endif
  assert(x);
  (void)x;
  assert (handles.count > 0);
  handles.count--;
  ddsrt_mutex_unlock (&handles.lock);
  return DDS_RETCODE_OK;
}

int32_t dds_handle_pin (dds_handle_t hdl, struct dds_handle_link **link)
{
#if USE_CHH
  struct thread_state1 * const ts1 = lookup_thread_state ();
#endif
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

#if USE_CHH
  thread_state_awake (ts1);
  *link = ddsrt_chh_lookup (handles.ht, &dummy);
#else
  ddsrt_mutex_lock (&handles.lock);
  *link = ddsrt_hh_lookup (handles.ht, &dummy);
#endif
  if (*link == NULL)
    rc = DDS_RETCODE_BAD_PARAMETER;
  else
  {
    uint32_t cnt_flags;
    /* Assume success; bail out if the object turns out to be in the process of
       being deleted */
    rc = DDS_RETCODE_OK;
    do {
      cnt_flags = ddsrt_atomic_ld32 (&(*link)->cnt_flags);
      if (cnt_flags & HDL_FLAG_CLOSED)
      {
        rc = DDS_RETCODE_BAD_PARAMETER;
        break;
      }
    } while (!ddsrt_atomic_cas32 (&(*link)->cnt_flags, cnt_flags, cnt_flags + HDL_PINCOUNT_UNIT));
  }
#if USE_CHH
  thread_state_asleep (ts1);
#else
  ddsrt_mutex_unlock (&handles.lock);
#endif
  return rc;
}

void dds_handle_repin (struct dds_handle_link *link)
{
  DDSRT_STATIC_ASSERT (HDL_PINCOUNT_UNIT == 1);
  uint32_t x = ddsrt_atomic_inc32_nv (&link->cnt_flags);
  assert (!(x & HDL_FLAG_CLOSED));
  (void) x;
}

void dds_handle_unpin (struct dds_handle_link *link)
{
  DDSRT_STATIC_ASSERT (HDL_PINCOUNT_UNIT == 1);
  if ((ddsrt_atomic_dec32_ov (&link->cnt_flags) & (HDL_FLAG_CLOSED | HDL_PINCOUNT_MASK)) == (HDL_FLAG_CLOSED | HDL_PINCOUNT_UNIT))
  {
    ddsrt_mutex_lock (&handles.lock);
    ddsrt_cond_broadcast (&handles.cond);
    ddsrt_mutex_unlock (&handles.lock);
  }
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
      new = (old - HDL_REFCOUNT_UNIT) | HDL_FLAG_CLOSED;
  } while (!ddsrt_atomic_cas32 (&link->cnt_flags, old, new));
  return (new & HDL_REFCOUNT_MASK) == 0;
}

bool dds_handle_is_closed (struct dds_handle_link *link)
{
  return (ddsrt_atomic_ld32 (&link->cnt_flags) & HDL_FLAG_CLOSED) != 0;
}
