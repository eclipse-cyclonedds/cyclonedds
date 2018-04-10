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
#include "os/os.h"
#include "util/ut_handleserver.h"
#include <criterion/criterion.h>
#include <criterion/logging.h>

/* Add --verbose command line argument to get the cr_log_info traces (if there are any). */

/*****************************************************************************************/
Test(util_handleserver, basic)
{
    const os_time zero  = { 0, 0 };
    int32_t kind = 0x10000000;
    ut_handle_retcode_t ret;
    ut_handle_t hdl;
    int arg = 1;
    void *argx;

    ret = ut_handleserver_init();
    cr_assert_eq(ret, UT_HANDLE_OK, "ut_handleserver_init");

    hdl = ut_handle_create(kind, (void*)&arg);
    cr_assert(hdl > 0, "ut_handle_create");

    ret = ut_handle_claim(hdl, NULL, kind, &argx);
    cr_assert_eq(ret, UT_HANDLE_OK, "ut_handle_claim ret");
    cr_assert_eq(argx, &arg, "ut_handle_claim arg");

    ut_handle_release(hdl, NULL);

    ret = ut_handle_delete(hdl, NULL, zero);
    cr_assert_eq(ret, UT_HANDLE_OK, "ut_handle_delete");

    ret = ut_handle_claim(hdl, NULL, kind, &argx);
    cr_assert_eq(ret, UT_HANDLE_DELETED, "ut_handle_claim ret");

    ut_handleserver_fini();
}


/*****************************************************************************************/
Test(util_handleserver, close)
{
    const os_time zero  = { 0, 0 };
    int32_t kind = 0x10000000;
    ut_handle_retcode_t ret;
    ut_handle_t hdl;
    int arg = 1;
    void *argx;
    bool closed;

    ret = ut_handleserver_init();
    cr_assert_eq(ret, UT_HANDLE_OK, "ut_handleserver_init");

    hdl = ut_handle_create(kind, (void*)&arg);
    cr_assert(hdl > 0, "ut_handle_create");

    closed = ut_handle_is_closed(hdl, NULL);
    cr_assert_eq(closed, false, "ut_handle_is_closed ret");

    ut_handle_close(hdl, NULL);

    closed = ut_handle_is_closed(hdl, NULL);
    cr_assert_eq(closed, true, "ut_handle_is_closed ret");

    ret = ut_handle_claim(hdl, NULL, kind, &argx);
    cr_assert_eq(ret, UT_HANDLE_CLOSED, "ut_handle_claim ret");

    ret = ut_handle_delete(hdl, NULL, zero);
    cr_assert_eq(ret, UT_HANDLE_OK, "ut_handle_delete");

    ret = ut_handle_claim(hdl, NULL, kind, &argx);
    cr_assert_eq(ret, UT_HANDLE_DELETED, "ut_handle_claim ret");

    ut_handleserver_fini();
}

/*****************************************************************************************/
Test(util_handleserver, link)
{
    const os_time zero  = { 0, 0 };
    int32_t kind = 0x10000000;
    ut_handle_retcode_t ret;
    struct ut_handlelink *link;
    ut_handle_t hdl;
    int arg = 1;
    void *argx;

    ret = ut_handleserver_init();
    cr_assert_eq(ret, UT_HANDLE_OK, "ut_handleserver_init");

    hdl = ut_handle_create(kind, (void*)&arg);
    cr_assert(hdl > 0, "ut_handle_create");

    link = ut_handle_get_link(hdl);
    cr_assert_neq(link, NULL, "ut_handle_get_link");

    ret = ut_handle_claim(hdl, link, kind, &argx);
    cr_assert_eq(ret, UT_HANDLE_OK, "ut_handle_claim ret");
    cr_assert_eq(argx, &arg, "ut_handle_claim arg");

    ut_handle_release(hdl, link);

    ret = ut_handle_delete(hdl, link, zero);
    cr_assert_eq(ret, UT_HANDLE_OK, "ut_handle_delete");

    link = ut_handle_get_link(hdl);
    cr_assert_eq(link, NULL, "ut_handle_get_link");

    ut_handleserver_fini();
}


/*****************************************************************************************/
Test(util_handleserver, types)
{
    const os_time zero  = { 0, 0 };
    int32_t kind1 = 0x10000000;
    int32_t kind2 = 0x20000000;
    ut_handle_retcode_t ret;
    ut_handle_t hdl1a;
    ut_handle_t hdl1b;
    ut_handle_t hdl2;
    int arg1a = (int)'a';
    int arg1b = (int)'b';
    int arg2  = (int)'2';
    void *argx;

    ret = ut_handleserver_init();
    cr_assert_eq(ret, UT_HANDLE_OK, "ut_handleserver_init");

    hdl1a = ut_handle_create(kind1, (void*)&arg1a);
    cr_assert(hdl1a > 0, "ut_handle_create");

    hdl1b = ut_handle_create(kind1, (void*)&arg1b);
    cr_assert(hdl1b > 0, "ut_handle_create");

    hdl2 = ut_handle_create(kind2, (void*)&arg2);
    cr_assert(hdl2 > 0, "ut_handle_create");

    ret = ut_handle_claim(hdl1a, NULL, kind1, &argx);
    cr_assert_eq(ret, UT_HANDLE_OK, "ut_handle_claim ret");
    cr_assert_eq(argx, &arg1a, "ut_handle_claim arg");

    ret = ut_handle_claim(hdl1b, NULL, kind1, &argx);
    cr_assert_eq(ret, UT_HANDLE_OK, "ut_handle_claim ret");
    cr_assert_eq(argx, &arg1b, "ut_handle_claim arg");

    ret = ut_handle_claim(hdl2, NULL, kind2, &argx);
    cr_assert_eq(ret, UT_HANDLE_OK, "ut_handle_claim ret");
    cr_assert_eq(argx, &arg2, "ut_handle_claim arg");

    ret = ut_handle_claim(hdl1a, NULL, kind2, &argx);
    cr_assert_eq(ret, UT_HANDLE_UNEQUAL_KIND, "ut_handle_claim ret");

    ret = ut_handle_claim(hdl1a, NULL, kind2, &argx);
    cr_assert_eq(ret, UT_HANDLE_UNEQUAL_KIND, "ut_handle_claim ret");

    ret = ut_handle_claim(hdl2, NULL, kind1, &argx);
    cr_assert_eq(ret, UT_HANDLE_UNEQUAL_KIND, "ut_handle_claim ret");

    ut_handle_release(hdl1a, NULL);
    ut_handle_release(hdl1b, NULL);
    ut_handle_release(hdl2,  NULL);

    ret = ut_handle_delete(hdl1a, NULL, zero);
    cr_assert_eq(ret, UT_HANDLE_OK, "ut_handle_delete");
    ret = ut_handle_delete(hdl1b, NULL, zero);
    cr_assert_eq(ret, UT_HANDLE_OK, "ut_handle_delete");
    ret = ut_handle_delete(hdl2,  NULL, zero);
    cr_assert_eq(ret, UT_HANDLE_OK, "ut_handle_delete");

    ut_handleserver_fini();
}


/*****************************************************************************************/
Test(util_handleserver, timeout)
{
    const os_time zero  = { 0, 0 };
    int32_t kind = 0x10000000;
    ut_handle_retcode_t ret;
    ut_handle_t hdl;
    int arg = 1;
    void *argx;

    ret = ut_handleserver_init();
    cr_assert_eq(ret, UT_HANDLE_OK, "ut_handleserver_init");

    hdl = ut_handle_create(kind, (void*)&arg);
    cr_assert(hdl > 0, "ut_handle_create");

    ret = ut_handle_claim(hdl, NULL, kind, &argx);
    cr_assert_eq(ret, UT_HANDLE_OK, "ut_handle_claim ret");
    cr_assert_eq(argx, &arg, "ut_handle_claim arg");

    ret = ut_handle_delete(hdl, NULL, zero);
    cr_assert_eq(ret, UT_HANDLE_TIMEOUT, "ut_handle_delete");

    ut_handle_release(hdl, NULL);

    ret = ut_handle_delete(hdl, NULL, zero);
    cr_assert_eq(ret, UT_HANDLE_OK, "ut_handle_delete");

    ut_handleserver_fini();
}


/*****************************************************************************************/
typedef enum thread_state_t {
    STARTING,
    DELETING,
    STOPPED
} thread_state_t;

typedef struct thread_arg_t {
    thread_state_t state;
    ut_handle_t    hdl;
} thread_arg_t;

static uint32_t
deleting_thread(void *a)
{
    thread_arg_t *arg = (thread_arg_t*)a;
    const os_time ten = { 10, 0 };
    ut_handle_t ret;

    arg->state = DELETING;
    /* This should block until the main test released all claims. */
    ret = ut_handle_delete(arg->hdl, NULL, ten);
    cr_assert_eq(ret, UT_HANDLE_OK, "ut_handle_delete ret");
    arg->state = STOPPED;

    return 0;
}

os_result
thread_reached_state(thread_state_t *actual, thread_state_t expected, int32_t msec)
{
    /* Convenience function. */
    bool stopped = false;
    os_time msec10 = { 0, 10000000 };
    while ((msec > 0) && (*actual != expected)) {
        os_nanoSleep(msec10);
        msec -= 10;
    }
    return (*actual == expected) ? os_resultSuccess : os_resultTimeout;
}

Test(util_handleserver, wakeup)
{
    const os_time zero  = {  0, 0 };
    int32_t kind = 0x10000000;
    ut_handle_retcode_t ret;
    ut_handle_t hdl;
    int arg = 1;
    void *argx;

    os_threadId   thread_id;
    thread_arg_t  thread_arg;
    os_threadAttr thread_attr;
    os_result     osr;

    ret = ut_handleserver_init();
    cr_assert_eq(ret, UT_HANDLE_OK, "ut_handleserver_init");

    hdl = ut_handle_create(kind, (void*)&arg);
    cr_assert(hdl > 0, "ut_handle_create");

    ret = ut_handle_claim(hdl, NULL, kind, &argx);
    cr_assert_eq(ret, UT_HANDLE_OK, "ut_handle_claim1 ret");

    ret = ut_handle_claim(hdl, NULL, kind, &argx);
    cr_assert_eq(ret, UT_HANDLE_OK, "ut_handle_claim2 ret");

    /* Try deleting in other thread, which should block. */
    thread_arg.hdl   = hdl;
    thread_arg.state = STARTING;
    os_threadAttrInit(&thread_attr);
    osr = os_threadCreate(&thread_id, "deleting_thread", &thread_attr, deleting_thread, (void*)&thread_arg);
    cr_assert_eq(osr, os_resultSuccess, "os_threadCreate");
    osr = thread_reached_state(&thread_arg.state, DELETING, 1000);
    cr_assert_eq(osr, os_resultSuccess, "deleting");
    osr = thread_reached_state(&thread_arg.state, STOPPED, 500);
    cr_assert_eq(osr, os_resultTimeout, "deleting");

    /* First release of the hdl should not unblock the thread. */
    ut_handle_release(hdl, NULL);
    osr = thread_reached_state(&thread_arg.state, STOPPED, 500);
    cr_assert_eq(osr, os_resultTimeout, "deleting");

    /* Second release of the hdl should unblock the thread. */
    ut_handle_release(hdl, NULL);
    osr = thread_reached_state(&thread_arg.state, STOPPED, 500);
    cr_assert_eq(osr, os_resultSuccess, "deleting");
    os_threadWaitExit(thread_id, NULL);

    /* The handle is deleted within the thread. */

    ut_handleserver_fini();
}
