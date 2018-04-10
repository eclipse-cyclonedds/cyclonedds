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
#ifndef UT_HANDLESERVER_H
#define UT_HANDLESERVER_H

#include "os/os.h"
#include "util/ut_export.h"

#if defined (__cplusplus)
extern "C" {
#endif

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
 * Some error return values.
 */
typedef _Return_type_success_(return == 0) enum ut_handle_retcode_t {
    UT_HANDLE_OK               =  0,
    UT_HANDLE_ERROR            = -1,     /* General error.                                      */
    UT_HANDLE_CLOSED           = -2,     /* Handle has been previously close.                   */
    UT_HANDLE_DELETED          = -3,     /* Handle has been previously deleted.                 */
    UT_HANDLE_INVALID          = -4,     /* Handle is not a valid handle.                       */
    UT_HANDLE_UNEQUAL_KIND     = -5,     /* Handle does not contain expected kind.              */
    UT_HANDLE_TIMEOUT          = -6,     /* Operation timed out.                                */
    UT_HANDLE_OUT_OF_RESOURCES = -7,     /* Action isn't possible because of limited resources. */
    UT_HANDLE_NOT_INITALIZED   = -8      /* Not initialized.                                    */
} ut_handle_retcode_t;


/*
 * The 32 bit handle
 *      |  bits |     # values | description                                                 |
 *      --------------------------------------------------------------------------------------
 *      |    31 |            2 | positive/negative (negative can be used to indicate errors) |
 *      | 24-30 |          127 | handle kind       (value determined by client)              |
 *      |  0-23 |   16.777.215 | index or hash     (maintained by the handleserver)          |
 *
 * When the handle is negative, it'll contain a ut_handle_retcode_t error value.
 *
 * FYI: the entity id within DDSI is also 24 bits...
 */
typedef _Return_type_success_(return > 0) int32_t ut_handle_t;

/*
 * Handle bits
 *   +kkk kkkk iiii iiii iiii iiii iiii iiii
 * 31|   | 24|                            0|
 */
#define UT_HANDLE_SIGN_MASK (0x80000000)
#define UT_HANDLE_KIND_MASK (0x7F000000)
#define UT_HANDLE_IDX_MASK  (0x00FFFFFF)

#define UT_HANDLE_DONTCARE_KIND (0)

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
_Return_type_success_(return != NULL) struct ut_handlelink;


/*
 * Initialize handleserver singleton.
 */
_Check_return_ UTIL_EXPORT ut_handle_retcode_t
ut_handleserver_init(void);


/*
 * Destroy handleserver singleton.
 * The handleserver is destroyed when fini() is called as often as init().
 */
UTIL_EXPORT void
ut_handleserver_fini(void);


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
_Must_inspect_result_ UTIL_EXPORT ut_handle_t
ut_handle_create(
        _In_ int32_t kind,
        _In_ void *arg);


/*
 * This will close the handle. All information remains, only new claims will
 * fail.
 *
 * This is a noop on an already closed handle.
 */
UTIL_EXPORT void
ut_handle_close(
        _In_        ut_handle_t hdl,
        _Inout_opt_ struct ut_handlelink *link);


/*
 * This will remove the handle related information from the server administration
 * to free up space.
 *
 * This is an implicit close().
 *
 * It will delete the information when there are no more active claims. It'll
 * block when necessary to wait for all possible claims to be released.
 */
_Check_return_ UTIL_EXPORT ut_handle_retcode_t
ut_handle_delete(
        _In_                        ut_handle_t hdl,
        _Inout_opt_ _Post_invalid_  struct ut_handlelink *link,
        _In_                        os_time timeout);


/*
 * Returns the status the given handle; valid/deleted/closed/etc.
 *
 * Returns OK when valid.
 */
_Pre_satisfies_((kind & UT_HANDLE_KIND_MASK) && !(kind & ~UT_HANDLE_KIND_MASK))
_Check_return_ ut_handle_retcode_t
ut_handle_status(
        _In_        ut_handle_t hdl,
        _Inout_opt_ struct ut_handlelink *link,
        _In_        int32_t kind);


/*
 * If the a valid handle is given, which matches the kind and it is not closed,
 * then the related arg will be provided and the claims count is increased.
 *
 * Returns OK when succeeded.
 */
_Pre_satisfies_((kind & UT_HANDLE_KIND_MASK) && !(kind & ~UT_HANDLE_KIND_MASK))
_Check_return_ UTIL_EXPORT ut_handle_retcode_t
ut_handle_claim(
        _In_        ut_handle_t hdl,
        _Inout_opt_ struct ut_handlelink *link,
        _In_        int32_t kind,
        _Out_opt_   void **arg);


/*
 * The active claims count is decreased.
 */
UTIL_EXPORT void
ut_handle_release(
        _In_        ut_handle_t hdl,
        _Inout_opt_ struct ut_handlelink *link);


/*
 * Check if the handle is closed.
 *
 * This is only useful when you have already claimed a handle and it is
 * possible that another thread is trying to delete the handle while you
 * were (for instance) sleeping or waiting for something. Now you can
 * break of your process and release the handle, making the deletion
 * possible.
 */
_Check_return_ UTIL_EXPORT bool
ut_handle_is_closed(
        _In_        ut_handle_t hdl,
        _Inout_opt_ struct ut_handlelink *link);


/*
 * This will get the link of the handle, which can be used for performance
 * increase.
 */
_Must_inspect_result_ UTIL_EXPORT struct ut_handlelink*
ut_handle_get_link(
        _In_ ut_handle_t hdl);

#if defined (__cplusplus)
}
#endif

#endif /* UT_HANDLESERVER_H */
