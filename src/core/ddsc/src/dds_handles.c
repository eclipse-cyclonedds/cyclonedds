// Copyright(c) 2006 to 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <string.h>
#include <assert.h>
#include "dds/ddsrt/time.h"
#include "dds/ddsrt/sync.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/random.h"
#include "dds/ddsrt/hopscotch.h"
#include "dds/ddsi/ddsi_thread.h"
#include "dds__handles.h"
#include "dds__types.h"

#define HDL_REFCOUNT_MASK  (0x03fff000u)
#define HDL_REFCOUNT_UNIT  (0x00001000u)
#define HDL_REFCOUNT_SHIFT 12
#define HDL_PINCOUNT_MASK  (0x00000fffu)

/*
"regular" entities other than topics:
  - create makes it
  - delete deletes it and its children immediately
  - explicit domain: additional protection for bootstrapping complications need extra care

implicit entities other than topics (pub, sub, domain, cyclonedds):
  - created "spontaneously" as a consequence of creating the writer/reader/participant
  - delete of last child causes it to disappear
  - explicit delete treated like a delete of a "regular" entity
  - domain, cyclonedds: bootstrapping complications require additional protection

topics:
  - create makes it
  - never has children (so the handle's cnt_flags can have a different meaning)
  - readers, writers keep it in existence
  - delete deferred until no readers/writers exist
  - an attempt at deleting it fails if in "deferred delete" state (or should it simply
    return ok while doing nothing?), other operations keep going so, e.g., listeners
    remain useful

built-in topics:
  - implicit variant of a topic
*/

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
                 cf & HDL_FLAG_DELETE_DEFERRED ? " delete-deferred" : "");
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
static dds_handle_t dds_handle_create_int (struct dds_handle_link *link, bool implicit, bool refc_counts_children, bool user_access)
{
  uint32_t flags = HDL_FLAG_PENDING;
  flags |= implicit ? HDL_FLAG_IMPLICIT : HDL_REFCOUNT_UNIT;
  flags |= refc_counts_children ? HDL_FLAG_ALLOW_CHILDREN : 0;
  flags |= user_access ? 0 : HDL_FLAG_NO_USER_ACCESS;
  ddsrt_atomic_st32 (&link->cnt_flags, flags | 1u);
  do {
    do {
      link->hdl = (int32_t) (ddsrt_random () & INT32_MAX);
    } while (link->hdl == 0 || link->hdl >= DDS_MIN_PSEUDO_HANDLE);
  } while (!hhadd (handles.ht, link));
  return link->hdl;
}

dds_handle_t dds_handle_create (struct dds_handle_link *link, bool implicit, bool allow_children, bool user_access)
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
    ret = dds_handle_create_int (link, implicit, allow_children, user_access);
    ddsrt_mutex_unlock (&handles.lock);
    assert (ret > 0);
  }
  return ret;
}

dds_return_t dds_handle_register_special (struct dds_handle_link *link, bool implicit, bool allow_children, dds_handle_t handle)
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
    ddsrt_atomic_st32 (&link->cnt_flags, HDL_FLAG_PENDING | (implicit ? HDL_FLAG_IMPLICIT : HDL_REFCOUNT_UNIT) | (allow_children ? HDL_FLAG_ALLOW_CHILDREN : 0) | 1u);
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
  assert (!(cf & HDL_FLAG_DELETE_DEFERRED));
  assert (!(cf & HDL_FLAG_CLOSING));
  assert ((cf & HDL_REFCOUNT_MASK) >= HDL_REFCOUNT_UNIT || (cf & HDL_FLAG_IMPLICIT));
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
    assert ((cf & HDL_REFCOUNT_MASK) == 0u);
  }
  assert ((cf & HDL_PINCOUNT_MASK) == 1u);
#endif
  ddsrt_mutex_lock (&handles.lock);
  ddsrt_hh_remove_present (handles.ht, link);
  assert (handles.count > 0);
  handles.count--;
  ddsrt_mutex_unlock (&handles.lock);
  return DDS_RETCODE_OK;
}

static int32_t dds_handle_pin_int (dds_handle_t hdl, uint32_t delta, bool from_user, struct dds_handle_link **link)
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
      if (cf & (HDL_FLAG_CLOSING | HDL_FLAG_PENDING | HDL_FLAG_NO_USER_ACCESS))
      {
        if (cf & (HDL_FLAG_CLOSING | HDL_FLAG_PENDING))
        {
          rc = DDS_RETCODE_BAD_PARAMETER;
          break;
        }
        else if (from_user)
        {
          rc = DDS_RETCODE_BAD_PARAMETER;
          break;
        }
      }
    } while (!ddsrt_atomic_cas32 (&(*link)->cnt_flags, cf, cf + delta));
  }
  ddsrt_mutex_unlock (&handles.lock);
  return rc;
}

int32_t dds_handle_pin (dds_handle_t hdl, struct dds_handle_link **link)
{
  return dds_handle_pin_int (hdl, 1u, true, link);
}

int32_t dds_handle_pin_with_origin (dds_handle_t hdl, bool from_user, struct dds_handle_link **link)
{
  return dds_handle_pin_int (hdl, 1u, from_user, link);
}

int32_t dds_handle_pin_for_delete (dds_handle_t hdl, bool explicit, bool from_user, struct dds_handle_link **link)
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
    uint32_t cf, cf1;
    /* Assume success; bail out if the object turns out to be in the process of
       being deleted */
    do {
      cf = ddsrt_atomic_ld32 (&(*link)->cnt_flags);

      if (from_user && (cf & (HDL_FLAG_NO_USER_ACCESS)))
      {
        /* If the user isn't allowed to delete the handle, just say it doesn't exist */
        rc = DDS_RETCODE_BAD_PARAMETER;
        break;
      }
      else if (cf & (HDL_FLAG_CLOSING | HDL_FLAG_PENDING))
      {
        /* Only one can succeed (and if closing is already set, the handle's reference has
           already been dropped) */
        rc = DDS_RETCODE_BAD_PARAMETER;
        break;
      }
      else if (cf & HDL_FLAG_DELETE_DEFERRED)
      {
        /* Someone already called delete, but the operation was deferred becauses there are still
           outstanding references.  This implies that there are no children, because then the
           entire hierarchy would simply have been deleted.  */
        assert (!(cf & HDL_FLAG_ALLOW_CHILDREN));
        if (cf & HDL_REFCOUNT_MASK)
        {
          rc = DDS_RETCODE_ALREADY_DELETED;
          break;
        }
        else
        {
          /* Refcount reached zero. Pin to allow deletion. */
          cf1 = (cf + 1u) | HDL_FLAG_CLOSING;
        }
      }
      else if (explicit)
      {
        /* Explicit call to dds_delete (either by application or by parent deleting its children) */
        if (cf & HDL_FLAG_IMPLICIT)
        {
          /* Entity is implicit, so handle doesn't hold a reference */
          cf1 = (cf + 1u) | HDL_FLAG_CLOSING;
        }
        else
        {
          assert ((cf & HDL_REFCOUNT_MASK) > 0);
          if ((cf & HDL_REFCOUNT_MASK) == HDL_REFCOUNT_UNIT)
          {
            /* Last reference is closing. Pin entity and indicate that it is closing. */
            cf1 = (cf - HDL_REFCOUNT_UNIT + 1u) | HDL_FLAG_CLOSING;
          }
          else if (!(cf & HDL_FLAG_ALLOW_CHILDREN))
          {
            /* The refcnt does not contain children.
             * Indicate that the closing of the entity is deferred. */
            cf1 = (cf - HDL_REFCOUNT_UNIT) | HDL_FLAG_DELETE_DEFERRED;
          }
          else
          {
            /* Entity is explicit, so handle held a reference, refc only counts children as so is not our concern */
            cf1 = (cf - HDL_REFCOUNT_UNIT + 1u) | HDL_FLAG_CLOSING;
          }
        }
      }
      else
      {
        /* Implicit call to dds_delete (child invoking delete on its parent) */
        if (cf & HDL_FLAG_IMPLICIT)
        {
          assert ((cf & HDL_REFCOUNT_MASK) > 0);
          if ((cf & HDL_REFCOUNT_MASK) == HDL_REFCOUNT_UNIT)
          {
            /* Last reference is closing. Pin entity and indicate that it is closing. */
            cf1 = (cf - HDL_REFCOUNT_UNIT + 1u) | HDL_FLAG_CLOSING;
          }
          else if (!(cf & HDL_FLAG_ALLOW_CHILDREN))
          {
            /* The refcnt does not contain children.
             * Indicate that the closing of the entity is deferred. */
            cf1 = (cf - HDL_REFCOUNT_UNIT) | HDL_FLAG_DELETE_DEFERRED;
          }
          else
          {
            /* Just reduce the children refcount by one. */
            cf1 = (cf - HDL_REFCOUNT_UNIT);
          }
        }
        else
        {
          /* Child can't delete an explicit parent */
          rc = DDS_RETCODE_ILLEGAL_OPERATION;
          break;
        }
      }

      rc = ((cf1 & HDL_REFCOUNT_MASK) == 0 || (cf1 & HDL_FLAG_ALLOW_CHILDREN)) ? DDS_RETCODE_OK : DDS_RETCODE_TRY_AGAIN;
    } while (!ddsrt_atomic_cas32 (&(*link)->cnt_flags, cf, cf1));
  }
  ddsrt_mutex_unlock (&handles.lock);
  return rc;
}

bool dds_handle_drop_childref_and_pin (struct dds_handle_link *link, bool may_delete_parent)
{
  bool del_parent = false;
  ddsrt_mutex_lock (&handles.lock);
  uint32_t cf, cf1;
  do {
    cf = ddsrt_atomic_ld32 (&link->cnt_flags);

    if (cf & (HDL_FLAG_CLOSING | HDL_FLAG_PENDING))
    {
      /* Only one can succeed; child ref still to be removed */
      assert ((cf & HDL_REFCOUNT_MASK) > 0);
      cf1 = (cf - HDL_REFCOUNT_UNIT);
      del_parent = false;
    }
    else
    {
      if (cf & HDL_FLAG_IMPLICIT)
      {
        /* Implicit parent: delete if last ref */
        if ((cf & HDL_REFCOUNT_MASK) == HDL_REFCOUNT_UNIT && may_delete_parent)
        {
          cf1 = (cf - HDL_REFCOUNT_UNIT + 1u);
          del_parent = true;
        }
        else
        {
          assert ((cf & HDL_REFCOUNT_MASK) > 0);
          cf1 = (cf - HDL_REFCOUNT_UNIT);
          del_parent = false;
        }
      }
      else
      {
        /* Child can't delete an explicit parent; child ref still to be removed */
        assert ((cf & HDL_REFCOUNT_MASK) > 0);
        cf1 = (cf - HDL_REFCOUNT_UNIT);
        del_parent = false;
      }
    }
  } while (!ddsrt_atomic_cas32 (&link->cnt_flags, cf, cf1));
  ddsrt_mutex_unlock (&handles.lock);
  return del_parent;
}

int32_t dds_handle_pin_and_ref_with_origin (dds_handle_t hdl, bool from_user, struct dds_handle_link **link)
{
  return dds_handle_pin_int (hdl, HDL_REFCOUNT_UNIT + 1u, from_user, link);
}

void dds_handle_repin (struct dds_handle_link *link)
{
  uint32_t x = ddsrt_atomic_inc32_nv (&link->cnt_flags);
  (void) x;
}

void dds_handle_unpin (struct dds_handle_link *link)
{
#ifndef NDEBUG
  uint32_t cf = ddsrt_atomic_ld32 (&link->cnt_flags);
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
  uint32_t old, new;
  do {
    old = ddsrt_atomic_ld32 (&link->cnt_flags);
    assert ((old & HDL_REFCOUNT_MASK) > 0);
    new = old - HDL_REFCOUNT_UNIT;
  } while (!ddsrt_atomic_cas32 (&link->cnt_flags, old, new));
  ddsrt_mutex_lock (&handles.lock);
  if ((new & (HDL_FLAG_CLOSING | HDL_PINCOUNT_MASK)) == (HDL_FLAG_CLOSING | 1u))
  {
    ddsrt_cond_broadcast (&handles.cond);
  }
  ddsrt_mutex_unlock (&handles.lock);
  return ((new & HDL_REFCOUNT_MASK) == 0);
}

bool dds_handle_unpin_and_drop_ref (struct dds_handle_link *link)
{
  uint32_t old, new;
  do {
    old = ddsrt_atomic_ld32 (&link->cnt_flags);
    assert ((old & HDL_REFCOUNT_MASK) > 0);
    assert ((old & HDL_PINCOUNT_MASK) > 0);
    new = old - HDL_REFCOUNT_UNIT - 1u;
  } while (!ddsrt_atomic_cas32 (&link->cnt_flags, old, new));
  ddsrt_mutex_lock (&handles.lock);
  if ((new & (HDL_FLAG_CLOSING | HDL_PINCOUNT_MASK)) == (HDL_FLAG_CLOSING | 1u))
  {
    ddsrt_cond_broadcast (&handles.cond);
  }
  ddsrt_mutex_unlock (&handles.lock);
  return ((new & HDL_REFCOUNT_MASK) == 0);
}

bool dds_handle_close (struct dds_handle_link *link)
{
  uint32_t old = ddsrt_atomic_or32_ov (&link->cnt_flags, HDL_FLAG_CLOSING);
  return (old & HDL_REFCOUNT_MASK) == 0;
}

void dds_handle_close_wait (struct dds_handle_link *link)
{
#ifndef NDEBUG
  uint32_t cf = ddsrt_atomic_ld32 (&link->cnt_flags);
  assert ((cf & HDL_FLAG_CLOSING));
  assert ((cf & HDL_PINCOUNT_MASK) >= 1u);
#endif
  ddsrt_mutex_lock (&handles.lock);
  while ((ddsrt_atomic_ld32 (&link->cnt_flags) & HDL_PINCOUNT_MASK) != 1u)
    ddsrt_cond_wait (&handles.cond, &handles.lock);
  /* only one thread may call close_wait on a given handle */
  ddsrt_mutex_unlock (&handles.lock);
}

bool dds_handle_is_not_refd (struct dds_handle_link *link)
{
  return ((ddsrt_atomic_ld32 (&link->cnt_flags) & HDL_REFCOUNT_MASK) == 0);
}

extern inline bool dds_handle_is_closed (struct dds_handle_link *link);
