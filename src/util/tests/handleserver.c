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
#include "CUnit/Test.h"

/*****************************************************************************************/
CU_Test(util_handleserver, basic)
{
    const os_time zero  = { 0, 0 };
    int32_t kind = 0x10000000;
    ut_handle_retcode_t ret;
    ut_handle_t hdl;
    int arg = 1;
    void *argx;

    ret = ut_handleserver_init();
    CU_ASSERT_EQUAL_FATAL(ret, UT_HANDLE_OK);

    hdl = ut_handle_create(kind, (void*)&arg);
    CU_ASSERT_FATAL(hdl > 0);

    ret = ut_handle_claim(hdl, NULL, kind, &argx);
    CU_ASSERT_EQUAL_FATAL(ret, UT_HANDLE_OK);
    CU_ASSERT_EQUAL_FATAL(argx, &arg);

    ut_handle_release(hdl, NULL);

    ret = ut_handle_delete(hdl, NULL, zero);
    CU_ASSERT_EQUAL_FATAL(ret, UT_HANDLE_OK);

    ret = ut_handle_claim(hdl, NULL, kind, &argx);
    CU_ASSERT_EQUAL_FATAL(ret, UT_HANDLE_DELETED);

    ut_handleserver_fini();
}


/*****************************************************************************************/
CU_Test(util_handleserver, close)
{
    const os_time zero  = { 0, 0 };
    int32_t kind = 0x10000000;
    ut_handle_retcode_t ret;
    ut_handle_t hdl;
    int arg = 1;
    void *argx;
    bool closed;

    ret = ut_handleserver_init();
    CU_ASSERT_EQUAL_FATAL(ret, UT_HANDLE_OK);

    hdl = ut_handle_create(kind, (void*)&arg);
    CU_ASSERT_FATAL(hdl > 0);

    closed = ut_handle_is_closed(hdl, NULL);
    CU_ASSERT_EQUAL_FATAL(closed, false);

    ut_handle_close(hdl, NULL);

    closed = ut_handle_is_closed(hdl, NULL);
    CU_ASSERT_EQUAL_FATAL(closed, true);

    ret = ut_handle_claim(hdl, NULL, kind, &argx);
    CU_ASSERT_EQUAL_FATAL(ret, UT_HANDLE_CLOSED);

    ret = ut_handle_delete(hdl, NULL, zero);
    CU_ASSERT_EQUAL_FATAL(ret, UT_HANDLE_OK);

    ret = ut_handle_claim(hdl, NULL, kind, &argx);
    CU_ASSERT_EQUAL_FATAL(ret, UT_HANDLE_DELETED);

    ut_handleserver_fini();
}

/*****************************************************************************************/
CU_Test(util_handleserver, link)
{
    const os_time zero  = { 0, 0 };
    int32_t kind = 0x10000000;
    ut_handle_retcode_t ret;
    struct ut_handlelink *link;
    ut_handle_t hdl;
    int arg = 1;
    void *argx;

    ret = ut_handleserver_init();
    CU_ASSERT_EQUAL_FATAL(ret, UT_HANDLE_OK);

    hdl = ut_handle_create(kind, (void*)&arg);
    CU_ASSERT_FATAL(hdl > 0);

    link = ut_handle_get_link(hdl);
    CU_ASSERT_NOT_EQUAL_FATAL(link, NULL);

    ret = ut_handle_claim(hdl, link, kind, &argx);
    CU_ASSERT_EQUAL_FATAL(ret, UT_HANDLE_OK);
    CU_ASSERT_EQUAL_FATAL(argx, &arg);

    ut_handle_release(hdl, link);

    ret = ut_handle_delete(hdl, link, zero);
    CU_ASSERT_EQUAL_FATAL(ret, UT_HANDLE_OK);

    link = ut_handle_get_link(hdl);
    CU_ASSERT_EQUAL_FATAL(link, NULL);

    ut_handleserver_fini();
}


/*****************************************************************************************/
CU_Test(util_handleserver, types)
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
    CU_ASSERT_EQUAL_FATAL(ret, UT_HANDLE_OK);

    hdl1a = ut_handle_create(kind1, (void*)&arg1a);
    CU_ASSERT_FATAL(hdl1a > 0);

    hdl1b = ut_handle_create(kind1, (void*)&arg1b);
    CU_ASSERT_FATAL(hdl1b > 0);

    hdl2 = ut_handle_create(kind2, (void*)&arg2);
    CU_ASSERT_FATAL(hdl2 > 0);

    ret = ut_handle_claim(hdl1a, NULL, kind1, &argx);
    CU_ASSERT_EQUAL_FATAL(ret, UT_HANDLE_OK);
    CU_ASSERT_EQUAL_FATAL(argx, &arg1a);

    ret = ut_handle_claim(hdl1b, NULL, kind1, &argx);
    CU_ASSERT_EQUAL_FATAL(ret, UT_HANDLE_OK);
    CU_ASSERT_EQUAL_FATAL(argx, &arg1b);

    ret = ut_handle_claim(hdl2, NULL, kind2, &argx);
    CU_ASSERT_EQUAL_FATAL(ret, UT_HANDLE_OK);
    CU_ASSERT_EQUAL_FATAL(argx, &arg2);

    ret = ut_handle_claim(hdl1a, NULL, kind2, &argx);
    CU_ASSERT_EQUAL_FATAL(ret, UT_HANDLE_UNEQUAL_KIND);

    ret = ut_handle_claim(hdl1a, NULL, kind2, &argx);
    CU_ASSERT_EQUAL_FATAL(ret, UT_HANDLE_UNEQUAL_KIND);

    ret = ut_handle_claim(hdl2, NULL, kind1, &argx);
    CU_ASSERT_EQUAL_FATAL(ret, UT_HANDLE_UNEQUAL_KIND);

    ut_handle_release(hdl1a, NULL);
    ut_handle_release(hdl1b, NULL);
    ut_handle_release(hdl2,  NULL);

    ret = ut_handle_delete(hdl1a, NULL, zero);
    CU_ASSERT_EQUAL_FATAL(ret, UT_HANDLE_OK);
    ret = ut_handle_delete(hdl1b, NULL, zero);
    CU_ASSERT_EQUAL_FATAL(ret, UT_HANDLE_OK);
    ret = ut_handle_delete(hdl2,  NULL, zero);
    CU_ASSERT_EQUAL_FATAL(ret, UT_HANDLE_OK);

    ut_handleserver_fini();
}


/*****************************************************************************************/
CU_Test(util_handleserver, timeout)
{
    const os_time zero  = { 0, 0 };
    int32_t kind = 0x10000000;
    ut_handle_retcode_t ret;
    ut_handle_t hdl;
    int arg = 1;
    void *argx;

    ret = ut_handleserver_init();
    CU_ASSERT_EQUAL_FATAL(ret, UT_HANDLE_OK);

    hdl = ut_handle_create(kind, (void*)&arg);
    CU_ASSERT_FATAL(hdl > 0);

    ret = ut_handle_claim(hdl, NULL, kind, &argx);
    CU_ASSERT_EQUAL_FATAL(ret, UT_HANDLE_OK);
    CU_ASSERT_EQUAL_FATAL(argx, &arg);

    ret = ut_handle_delete(hdl, NULL, zero);
    CU_ASSERT_EQUAL_FATAL(ret, UT_HANDLE_TIMEOUT);

    ut_handle_release(hdl, NULL);

    ret = ut_handle_delete(hdl, NULL, zero);
    CU_ASSERT_EQUAL_FATAL(ret, UT_HANDLE_OK);

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
    CU_ASSERT_EQUAL_FATAL(ret, UT_HANDLE_OK);
    arg->state = STOPPED;

    return 0;
}

os_result
thread_reached_state(thread_state_t *actual, thread_state_t expected, int32_t msec)
{
    /* Convenience function. */
    os_time msec10 = { 0, 10000000 };
    while ((msec > 0) && (*actual != expected)) {
        os_nanoSleep(msec10);
        msec -= 10;
    }
    return (*actual == expected) ? os_resultSuccess : os_resultTimeout;
}

CU_Test(util_handleserver, wakeup)
{
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
    CU_ASSERT_EQUAL_FATAL(ret, UT_HANDLE_OK);

    hdl = ut_handle_create(kind, (void*)&arg);
    CU_ASSERT_FATAL(hdl > 0);

    ret = ut_handle_claim(hdl, NULL, kind, &argx);
    CU_ASSERT_EQUAL_FATAL(ret, UT_HANDLE_OK);

    ret = ut_handle_claim(hdl, NULL, kind, &argx);
    CU_ASSERT_EQUAL_FATAL(ret, UT_HANDLE_OK);

    /* Try deleting in other thread, which should block. */
    thread_arg.hdl   = hdl;
    thread_arg.state = STARTING;
    os_threadAttrInit(&thread_attr);
    osr = os_threadCreate(&thread_id, "deleting_thread", &thread_attr, deleting_thread, (void*)&thread_arg);
    CU_ASSERT_EQUAL_FATAL(osr, os_resultSuccess);
    osr = thread_reached_state(&thread_arg.state, DELETING, 1000);
    CU_ASSERT_EQUAL_FATAL(osr, os_resultSuccess);
    osr = thread_reached_state(&thread_arg.state, STOPPED, 500);
    CU_ASSERT_EQUAL_FATAL(osr, os_resultTimeout);

    /* First release of the hdl should not unblock the thread. */
    ut_handle_release(hdl, NULL);
    osr = thread_reached_state(&thread_arg.state, STOPPED, 500);
    CU_ASSERT_EQUAL_FATAL(osr, os_resultTimeout);

    /* Second release of the hdl should unblock the thread. */
    ut_handle_release(hdl, NULL);
    osr = thread_reached_state(&thread_arg.state, STOPPED, 500);
    CU_ASSERT_EQUAL_FATAL(osr, os_resultSuccess);
    os_threadWaitExit(thread_id, NULL);

    /* The handle is deleted within the thread. */

    ut_handleserver_fini();
}
