/*
 * Copyright(c) 2006 to 2019 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#ifndef DDS__HANDLES_H
#define DDS__HANDLES_H

#include "os/os.h"
#include "ddsc/dds.h"

#if defined (__cplusplus)
extern "C" {
#endif

struct dds_entity;

/********************************************************************************************
 *
 * TODO CHAM-138: Header file improvements
 *    - Remove some internal design decisions and masks from the header file.
 *    - Improve function headers where needed.
 *
 ********************************************************************************************/

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



/*
 * The 32 bit handle
 *      |  bits |     # values | description                                                 |
 *      --------------------------------------------------------------------------------------
 *      |    31 |            2 | positive/negative (negative can be used to indicate errors) |
 *      | 24-30 |          127 | handle kind       (value determined by client)              |
 *      |  0-23 |   16.777.215 | index or hash     (maintained by the handleserver)          |
 *
 * When the handle is negative, it'll contain a dds_handle_retcode_t error value.
 *
 * FYI: the entity id within DDSI is also 24 bits...
 */
typedef _Return_type_success_(return > 0) int32_t dds_handle_t;

/*
 * Handle bits
 *   +kkk kkkk iiii iiii iiii iiii iiii iiii
 * 31|   | 24|                            0|
 */
#define UT_HANDLE_SIGN_MASK (0x80000000)
#define UT_HANDLE_KIND_MASK (0x7F000000)
#define UT_HANDLE_IDX_MASK  (0x00FFFFFF)

#define DDS_PSEUDO_HANDLE_FLAG (0x40000000u)

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
_Return_type_success_(return != NULL) struct dds_handle_link;


/*
 * Initialize handleserver singleton.
 */
_Check_return_ DDS_EXPORT dds_return_t
dds_handle_server_init(void (*free_via_gc) (void *x));


/*
 * Destroy handleserver singleton.
 * The handleserver is destroyed when fini() is called as often as init().
 */
DDS_EXPORT void
dds_handle_server_fini(void);


/*
 * This creates a new handle that contains the given type and is linked to the
 * user data.
 *
 * A kind value != 0 has to be provided, just to make sure that no 0 handles
 * will be created. It should also fit the UT_HANDLE_KIND_MASK.
 * In other words handle creation will fail if
 * ((kind & ~UT_HANDLE_KIND_MASK != 0) || (kind & UT_HANDLE_KIND_MASK == 0)).
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
_Pre_satisfies_((kind & UT_HANDLE_KIND_MASK) && !(kind & ~UT_HANDLE_KIND_MASK))
_Post_satisfies_((return & UT_HANDLE_KIND_MASK) == kind)
_Must_inspect_result_ DDS_EXPORT dds_handle_t
dds_handle_create(
        _In_ struct dds_entity *entity,
        _Out_ struct dds_handle_link **link);


/*
 * This will close the handle. All information remains, only new claims will
 * fail.
 *
 * This is a noop on an already closed handle.
 */
DDS_EXPORT void
dds_handle_close(
        _Inout_opt_ struct dds_handle_link *link);


/*
 * This will remove the handle related information from the server administration
 * to free up space.
 *
 * This is an implicit close().
 *
 * It will delete the information when there are no more active claims. It'll
 * block when necessary to wait for all possible claims to be released.
 */
_Check_return_ DDS_EXPORT int32_t
dds_handle_delete(
        _Inout_ _Post_invalid_  struct dds_handle_link *link,
        _In_                    os_time timeout);


/*
 * If the a valid handle is given, which matches the kind and it is not closed,
 * then the related arg will be provided and the claims count is increased.
 *
 * Returns OK when succeeded.
 */
_Pre_satisfies_((kind & UT_HANDLE_KIND_MASK) && !(kind & ~UT_HANDLE_KIND_MASK))
_Check_return_ DDS_EXPORT int32_t
dds_handle_claim(
        _In_        dds_handle_t hdl,
        _Out_       struct dds_entity **entity);


DDS_EXPORT void
dds_handle_claim_inc(
        _Inout_opt_ struct dds_handle_link *link);


/*
 * The active claims count is decreased.
 */
DDS_EXPORT void
dds_handle_release(
        _Inout_ struct dds_handle_link *link);


/*
 * Check if the handle is closed.
 *
 * This is only useful when you have already claimed a handle and it is
 * possible that another thread is trying to delete the handle while you
 * were (for instance) sleeping or waiting for something. Now you can
 * break of your process and release the handle, making the deletion
 * possible.
 */
_Check_return_ DDS_EXPORT bool
dds_handle_is_closed(
        _Inout_ struct dds_handle_link *link);


#if defined (__cplusplus)
}
#endif

#endif /* DDS__HANDLES_H */
