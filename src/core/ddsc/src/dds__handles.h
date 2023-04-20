// Copyright(c) 2006 to 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDS__HANDLES_H
#define DDS__HANDLES_H

#include "dds/ddsrt/time.h"
#include "dds/ddsrt/retcode.h"
#include "dds/ddsrt/atomics.h"
#include "dds/dds.h"

#if defined (__cplusplus)
extern "C" {
#endif

struct dds_entity;

/*
 * Short working ponderings.
 *
 * A handle will be created with a related object. If you want to protect that object by
 * means of a mutex, it's your responsibility, not handleservers'.
 *
 * The handle server keeps an 'active claim' count. Every time a handle is claimed, the
 * count is increase and decreased when the handle is released. A handle can only be
 * deleted when there's no more active claim. You can use this to make sure that nobody
 * is using the handle anymore, which should mean that nobody is using the related object
 * anymore. So, when you can delete the handle, you can safely delete the related object.
 *
 * To prevent new claims (f.i. when you want to delete the handle), you can close the
 * handle. This will not remove any information within the handleserver, it just prevents
 * new claims. The delete will actually free handleserver internal memory.
 *
 * There's currently a global lock in the handle server that is used with every API call.
 * Maybe the internals can be improved to circumvent the need for that...
 */



typedef int32_t dds_handle_t;

/*
 * The handle link type.
 *
 * The lookup of an handle should be very quick.
 * But to increase the performance just a little bit more, it is possible to
 * acquire a handlelink. This can be used by the handleserver to circumvent
 * the lookup and go straight to the handle related information.
 *
 * Almost every handleserver function supports the handlelink. You don't have
 * to provide the link. When it is NULL, the normal lookup will be done.
 *
 * This handlelink is invalid after the related handle is deleted and should
 * never be used afterwards.
 */

/* Closing & closed can be combined, but having two gives a means for enforcing
   that close() be called first, then close_wait(), and then delete(). */
#define HDL_FLAG_CLOSING         (0x80000000u)
#define HDL_FLAG_DELETE_DEFERRED (0x40000000u)
#define HDL_FLAG_PENDING         (0x20000000u)
#define HDL_FLAG_IMPLICIT        (0x10000000u)
#define HDL_FLAG_ALLOW_CHILDREN  (0x08000000u) /* refc counts children */
#define HDL_FLAG_NO_USER_ACCESS  (0x04000000u)

struct dds_handle_link {
  dds_handle_t hdl;
  ddsrt_atomic_uint32_t cnt_flags;
};

/**
 * @brief Initialize handleserver singleton.
 * @component handles
 *
 * @return dds_return_t
 */
dds_return_t dds_handle_server_init(void);


/**
 * @brief Destroy handleserver singleton.
 * @component handles
 *
 * The handleserver is destroyed when fini() is called as often as init().
 */
void dds_handle_server_fini(void);


/**
 * @component handles
 *
 * This creates a new handle that contains the given type and is linked to the
 * user data.
 *
 * A kind value != 0 has to be provided, just to make sure that no 0 handles
 * will be created. It should also fit the DDSRT_HANDLE_KIND_MASK.
 * In other words handle creation will fail if
 * ((kind & ~DDSRT_HANDLE_KIND_MASK != 0) || (kind & DDSRT_HANDLE_KIND_MASK == 0)).
 *
 * It has to do something clever to make sure that a deleted handle is not
 * re-issued very quickly after it was deleted.
 *
 * kind - The handle kind, provided by the client.
 * arg  - The user data linked to the handle (may be NULL).
 *
 * Valid handle when returned value is positive.
 * Otherwise negative handle is returned.
 */
dds_handle_t dds_handle_create(struct dds_handle_link *link, bool implicit, bool allow_children, bool user_access);


/**
 * @brief Register a specific handle.
 * @component handles
 */
dds_return_t dds_handle_register_special (struct dds_handle_link *link, bool implicit, bool allow_children, dds_handle_t handle);

/** @component handles */
void dds_handle_unpend (struct dds_handle_link *link);

/**
 * @component handles
 *
 * This will close the handle. All information remains, only new claims will
 * fail.
 *
 * This is a noop on an already closed handle.
 */
void dds_handle_close_wait (struct dds_handle_link *link);

/**
 * @component handles
 *
 * This will remove the handle related information from the server administration
 * to free up space.
 *
 * This is an implicit close().
 *
 * It will delete the information when there are no more active claims. It'll
 * block when necessary to wait for all possible claims to be released.
 */
int32_t dds_handle_delete(struct dds_handle_link *link);


/**
 * @component handles
 *
 * If the a valid handle is given, which matches the kind and it is not closed,
 * then the related arg will be provided and the claims count is increased.
 *
 * Returns OK when succeeded.
 */
int32_t dds_handle_pin(dds_handle_t hdl, struct dds_handle_link **entity);

/** @component handles */
int32_t dds_handle_pin_with_origin(dds_handle_t hdl, bool from_user, struct dds_handle_link **entity);

/** @component handles */
int32_t dds_handle_pin_and_ref_with_origin(dds_handle_t hdl, bool from_user, struct dds_handle_link **entity);

/** @component handles */
void dds_handle_repin(struct dds_handle_link *link);


/**
 * @component handles
 *
 * The active claims count is decreased.
 */
void dds_handle_unpin(struct dds_handle_link *link);

/** @component handles */
int32_t dds_handle_pin_for_delete (dds_handle_t hdl, bool explicit, bool from_user, struct dds_handle_link **link);

/** @component handles */
bool dds_handle_drop_childref_and_pin (struct dds_handle_link *link, bool may_delete_parent);


/** @component handles */
void dds_handle_add_ref (struct dds_handle_link *link);

/** @component handles */
bool dds_handle_drop_ref (struct dds_handle_link *link);

/** @component handles */
bool dds_handle_close (struct dds_handle_link *link);

/** @component handles */
bool dds_handle_unpin_and_drop_ref (struct dds_handle_link *link);

/**
 * @brief Check if the handle is closed.
 * @component handles
 *
 * This is only useful when you have already claimed a handle and it is
 * possible that another thread is trying to delete the handle while you
 * were (for instance) sleeping or waiting for something. Now you can
 * break of your process and release the handle, making the deletion
 * possible.
 */
inline bool dds_handle_is_closed (struct dds_handle_link *link) {
  return (ddsrt_atomic_ld32 (&link->cnt_flags) & HDL_FLAG_CLOSING) != 0;
}

/** @component handles */
bool dds_handle_is_not_refd (struct dds_handle_link *link);

#if defined (__cplusplus)
}
#endif

#endif /* DDS__HANDLES_H */
