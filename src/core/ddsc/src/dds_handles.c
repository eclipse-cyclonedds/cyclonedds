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
#define HDL_COUNT_MASK     (0x00ffffffu)

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

dds_return_t dds_handle_server_init (void (*free_via_gc) (void *x))
{
#if USE_CHH
  handles.ht = ddsrt_chh_new (128, handle_hash, handle_equal, free_via_gc);
#else
  handles.ht = ddsrt_hh_new (128, handle_hash, handle_equal);
  (void) free_via_gc;
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
  ddsrt_atomic_st32 (&link->cnt_flags, 0);
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
  if ((ddsrt_atomic_ld32 (&link->cnt_flags) & HDL_COUNT_MASK) != 0)
  {
    /* FIXME: */
    const dds_time_t abstimeout = dds_time () + timeout;
    while ((ddsrt_atomic_ld32 (&link->cnt_flags) & HDL_COUNT_MASK) != 0)
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

int32_t dds_handle_claim (dds_handle_t hdl, struct dds_handle_link **link)
{
#if USE_CHH
  struct thread_state1 * const ts1 = lookup_thread_state ();
#endif
  struct dds_handle_link dummy = { .hdl = hdl };
  int32_t rc;
  /* it makes sense to check here for initialization: the first thing any operation
     (other than create_participant) does is to call dds_handle_claim on the supplied
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
    } while (!ddsrt_atomic_cas32 (&(*link)->cnt_flags, cnt_flags, cnt_flags + 1));
  }
#if USE_CHH
  thread_state_asleep (ts1);
#else
  ddsrt_mutex_unlock (&handles.lock);
#endif
  return rc;
}

void dds_handle_claim_inc (struct dds_handle_link *link)
{
  uint32_t x = ddsrt_atomic_inc32_nv (&link->cnt_flags);
  assert (!(x & HDL_FLAG_CLOSED));
  (void) x;
}

void dds_handle_release (struct dds_handle_link *link)
{
  if (ddsrt_atomic_dec32_ov (&link->cnt_flags) == (HDL_FLAG_CLOSED | 1))
  {
    ddsrt_mutex_lock (&handles.lock);
    ddsrt_cond_broadcast (&handles.cond);
    ddsrt_mutex_unlock (&handles.lock);
  }
}

bool dds_handle_is_closed (struct dds_handle_link *link)
{
  return (ddsrt_atomic_ld32 (&link->cnt_flags) & HDL_FLAG_CLOSED) != 0;
}
