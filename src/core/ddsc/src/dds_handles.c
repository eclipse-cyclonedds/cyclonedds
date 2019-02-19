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
#include "os/os.h"
#include "util/ut_hopscotch.h"
#include "dds__handles.h"
#include "dds__types.h"
#include "dds__err.h"
#include "ddsi/q_thread.h"

/* FIXME: this code isn't really correct:
   - the DDS entity code doesn't really play by the awake/asleep mechanism
   - there is no provision in the code for a handle being deleted concurrent to a lookup,
     that is, deleting handle links should also go through the GC
   entity framework needs a fair bit of rewriting anyway ... */

#define HDL_FLAG_CLOSED    (0x80000000u)
#define HDL_COUNT_MASK     (0x00ffffffu)

struct dds_handle_link {
  dds_handle_t hdl;
  void *arg;
  os_atomic_uint32_t cnt_flags;
};

struct dds_handle_server {
  struct ut_chh *ht;
  dds_handle_t next;
  os_mutex lock;
  os_cond cond;
};

static struct dds_handle_server handles;

static uint32_t handle_hash (const void *va)
{
  const struct dds_handle_link *a = va;
  return (uint32_t) (((uint32_t) a->hdl * UINT64_C (16292676669999574021)) >> 32);
}

static int handle_equal (const void *va, const void *vb)
{
  const struct dds_handle_link *a = va;
  const struct dds_handle_link *b = vb;
  return a->hdl == b->hdl;
}

dds_return_t dds_handle_server_init (void (*free_via_gc) (void *x))
{
  os_osInit();
  handles.ht = ut_chhNew (128, handle_hash, handle_equal, free_via_gc);
  handles.next = 1;
  os_mutexInit (&handles.lock);
  os_condInit (&handles.cond, &handles.lock);
  return DDS_RETCODE_OK;
}

void dds_handle_server_fini (void)
{
#ifndef NDEBUG
  struct ut_chhIter it;
  assert (ut_chhIterFirst (handles.ht, &it) == NULL);
#endif
  ut_chhFree (handles.ht);
  os_condDestroy (&handles.cond);
  os_mutexDestroy (&handles.lock);
  handles.ht = NULL;
  os_osExit();
}

dds_handle_t dds_handle_create (struct dds_entity *entity, struct dds_handle_link **link)
{
  struct thread_state1 * const self = lookup_thread_state ();
  const bool asleep = vtime_asleep_p (self->vtime);
  struct dds_handle_link *hl = os_malloc (sizeof (*hl));
  dds_handle_t start = handles.next;
  os_atomic_st32 (&hl->cnt_flags, 0);
  hl->arg = entity;
  if (asleep)
    thread_state_awake (self);
  os_mutexLock (&handles.lock);
  do {
    hl->hdl = handles.next;
    if (handles.next++ == UT_HANDLE_IDX_MASK) {
      handles.next = 1;
    }
    if (handles.next == start) {
      os_mutexUnlock (&handles.lock);
      return DDS_ERRNO (DDS_RETCODE_OUT_OF_RESOURCES);
    }
  } while (!ut_chhAdd(handles.ht, hl));
  os_mutexUnlock (&handles.lock);
  if (asleep)
    thread_state_asleep (self);
  *link = hl;
  return hl->hdl;
}

void dds_handle_close (struct dds_handle_link *link)
{
  os_atomic_or32 (&link->cnt_flags, HDL_FLAG_CLOSED);
}

int32_t dds_handle_delete (struct dds_handle_link *link, os_time timeout)
{
  struct thread_state1 * const self = lookup_thread_state ();
  const bool asleep = vtime_asleep_p (self->vtime);

  assert (os_atomic_ld32 (&link->cnt_flags) & HDL_FLAG_CLOSED);

  os_mutexLock (&handles.lock);
  if ((os_atomic_ld32 (&link->cnt_flags) & HDL_COUNT_MASK) != 0)
  {
    const os_time abstimeout = os_timeAdd (os_timeGetMonotonic (), timeout);
    while ((os_atomic_ld32 (&link->cnt_flags) & HDL_COUNT_MASK) != 0)
    {
      const os_time tnow = os_timeGetMonotonic ();
      if (os_timeCompare (tnow, abstimeout) >= 0)
      {
        os_mutexUnlock (&handles.lock);
        fprintf (stderr, "** timeout in handle_delete **\n");
        return DDS_RETCODE_TIMEOUT;
      }
      const os_time dt = os_timeSub (abstimeout, tnow);
      os_condTimedWait (&handles.cond, &handles.lock, &dt);
    }
  }
  if (asleep)
    thread_state_awake (self);
  int x = ut_chhRemove (handles.ht, link);
  if (asleep)
    thread_state_asleep (self);
  assert(x);
  (void)x;
  os_free (link);
  os_mutexUnlock (&handles.lock);
  return DDS_RETCODE_OK;
}

int32_t dds_handle_claim (dds_handle_t hdl, struct dds_entity **entity)
{
  struct dds_handle_link *link;
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

  struct thread_state1 * const self = lookup_thread_state ();
  const bool asleep = vtime_asleep_p (self->vtime);
  if (asleep)
    thread_state_awake (self);

  if ((link = ut_chhLookup (handles.ht, &dummy)) == NULL)
    rc = DDS_RETCODE_BAD_PARAMETER;
  else
  {
    uint32_t cnt_flags;
    /* Assume success; bail out if the object turns out to be in the process of
       being deleted */
    rc = DDS_RETCODE_OK;
    *entity = link->arg;
    do {
      cnt_flags = os_atomic_ld32 (&link->cnt_flags);
      if (cnt_flags & HDL_FLAG_CLOSED)
      {
        rc = DDS_RETCODE_BAD_PARAMETER;
        break;
      }
    } while (!os_atomic_cas32 (&link->cnt_flags, cnt_flags, cnt_flags + 1));
  }

  if (asleep)
    thread_state_asleep (self);
  return rc;
}

void dds_handle_claim_inc (struct dds_handle_link *link)
{
  uint32_t x = os_atomic_inc32_nv (&link->cnt_flags);
  assert (!(x & HDL_FLAG_CLOSED));
  (void) x;
}

void dds_handle_release (struct dds_handle_link *link)
{
  if (os_atomic_dec32_ov (&link->cnt_flags) == (HDL_FLAG_CLOSED | 1))
  {
    os_mutexLock (&handles.lock);
    os_condBroadcast (&handles.cond);
    os_mutexUnlock (&handles.lock);
  }
}

bool dds_handle_is_closed (struct dds_handle_link *link)
{
  return (os_atomic_ld32 (&link->cnt_flags) & HDL_FLAG_CLOSED) != 0;
}
