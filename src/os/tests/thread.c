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
#include "CUnit/Test.h"
#include "os/os.h"
#include "assert.h"

#define ENABLE_TRACING 0

char          arg_result[30];
int           threadCalled;
int           startCallbackCount;
int           stopCallbackCount;
void          *returnval;

static void
sleepMsec(int32_t msec)
{
    os_time delay;

    assert(msec > 0);
    assert(msec < 1000);

    delay.tv_sec = 0;
    delay.tv_nsec = msec*1000*1000;

    os_nanoSleep(delay);
}

uint32_t new_thread (_In_ void *args)
{
    (void)snprintf (arg_result, sizeof (arg_result), "%s", (char *)args);
    sleepMsec (500);
    return 0;
}

static uintmax_t thread_id_from_thread;

uint32_t threadId_thread (_In_opt_ void *args)
{
    if (args != NULL) {
        sleepMsec (500);
    }
    thread_id_from_thread = os_threadIdToInteger (os_threadIdSelf ());
    return (uint32_t)thread_id_from_thread; /* Truncates potentially; just used for checking passing a result-value. */
}

uint32_t get_threadExit_thread (void *args)
{
    os_threadId * threadId = args;
    uint32_t id;
    (void)os_threadWaitExit (*threadId, &id);
    return id;
}

uint32_t threadMemory_thread (_In_opt_ void *args)
{
    OS_UNUSED_ARG(args);

    /* Check os_threadMemMalloc with success result for child thread */
    printf("Starting os_threadMemMalloc_003\n");
    returnval = os_threadMemMalloc (3, 100);
    CU_ASSERT (returnval != NULL);

    /* Check os_threadMemGet for child thread and non allocated index */
    printf("Starting os_threadMemGet_003\n");
    returnval = os_threadMemGet (OS_THREAD_WARNING);
    CU_ASSERT (returnval == NULL);

    /* Check os_threadMemGet for child thread and allocated index */
    printf("Starting os_threadMemGet_004\n");
    returnval = os_threadMemGet (3);
    CU_ASSERT (returnval != NULL);

    /* Check os_threadMemFree for child thread and non allocated index */
    printf("Starting os_threadMemFree_003\n");
    os_threadMemFree (OS_THREAD_WARNING);
    returnval = os_threadMemGet (OS_THREAD_WARNING);
    CU_ASSERT (returnval == NULL);

    /* Check os_threadMemFree for child thread and allocated index */
    printf("Starting os_threadMemFree_004\n");
    os_threadMemFree (3);
    returnval = os_threadMemGet (3);
    CU_ASSERT (returnval == NULL);

    return 0;
}

CU_Init(os_thread)
{
    int result = 0;
    os_osInit();
    printf("Run os_thread_Initialize\n");

    return result;
}

CU_Clean(os_thread)
{
    int result = 0;

    printf("Run os_thread_Cleanup\n");
    os_osExit();
    return result;
}

CU_Test(os_thread, create)
{
    int result;
    os_result osResult;
    os_threadId   thread_os_threadId;
    os_threadAttr thread_os_threadAttr;
#ifndef WIN32
    int           result_int;
#endif

    /* Check os_threadCreate with Success result\n\t\t
       (check thread creation and check argument passing) */
    printf ("Starting os_thread_create_001\n");
    os_threadAttrInit (&thread_os_threadAttr);
    osResult = os_threadCreate (&thread_os_threadId, "ThreadCreate1", &thread_os_threadAttr, &new_thread, "os_threadCreate");
    CU_ASSERT (osResult == os_resultSuccess);
    if (osResult == os_resultSuccess) {
#ifdef _WRS_KERNEL
        taskDelay(1 * sysClkRateGet());
#endif
        osResult = os_threadWaitExit (thread_os_threadId, NULL);
        CU_ASSERT (osResult == os_resultSuccess);

        if (osResult == os_resultSuccess) {
            result = strcmp (arg_result, "os_threadCreate");
            CU_ASSERT (result == 0);
            if (result == 0)
                printf("Thread created and argument correctly passed.\n");
            else
                printf("Thread created but argument incorrectly passed.\n");
        } else {
            printf("os_threadCreate success, failed os_threadWaitExit.\n");
        }
    }

    /* Check os_threadCreate with Failed result */
    printf ("Starting os_thread_create_002\n");
    printf ("N.A - Failure cannot be forced\n");

    /* Check os_threadCreate with scheduling class SCHED_DEFAULT */
    printf ("Starting s_thread_create_003\n");
    os_threadAttrInit (&thread_os_threadAttr);
    thread_os_threadAttr.schedClass = OS_SCHED_DEFAULT;
    osResult = os_threadCreate (&thread_os_threadId, "ThreadCreate3", &thread_os_threadAttr, &new_thread, "os_threadCreate");
    CU_ASSERT (osResult == os_resultSuccess);

#if !(defined _WRS_KERNEL || defined WIN32)
    if (osResult == os_resultSuccess) {
        int policy;
        struct sched_param sched_param;

        result_int = pthread_getschedparam (thread_os_threadId.v, &policy, &sched_param);
        CU_ASSERT (result_int == 0);

        if (result_int != 0) {
            printf ("pthread_getschedparam failed");
        } else {
            CU_ASSERT (policy == SCHED_OTHER);
        }
        osResult = os_threadWaitExit (thread_os_threadId, NULL);
        CU_ASSERT (osResult == os_resultSuccess);
    } else {
        printf ("os_threadCreate failed.\n");
    }
#endif

/* SCHED_TIMESHARE not supported by vxworks kernel */
#ifndef _WRS_KERNEL
    /* Check os_threadCreate with scheduling class SCHED_TIMESHARE */
    printf ("Starting os_thread_create_004\n");
    os_threadAttrInit (&thread_os_threadAttr);
    thread_os_threadAttr.schedClass = OS_SCHED_TIMESHARE;
    osResult = os_threadCreate (&thread_os_threadId, "ThreadCreate4", &thread_os_threadAttr, &new_thread, "os_threadCreate");
    CU_ASSERT (osResult == os_resultSuccess);
    if (osResult == os_resultSuccess) {
#ifndef WIN32
        int policy;
        struct sched_param sched_param;

        result_int = pthread_getschedparam (thread_os_threadId.v, &policy, &sched_param);
        CU_ASSERT (result_int == 0);

        if (result_int != 0) {
            printf ("pthread_getschedparam failed");
        } else {
            CU_ASSERT (policy == SCHED_OTHER);
        }
#endif /* WIN32 */

        osResult = os_threadWaitExit (thread_os_threadId, NULL);
    } else {
        printf ("os_threadCreate failed.\n");
    }
#endif

    /* Check os_threadCreate with scheduling class SCHED_REALTIME */
    printf ("Starting tc_os_thread_create_005\n");
#if ! defined WIN32 && ! defined _WRS_KERNEL
#ifndef VXWORKS_RTP
    if (getuid() != 0 && geteuid() != 0) {
        printf ("N.A - Need root privileges to do the test\n");
    }
    else
#endif /* VXWORKS_RTP */
    {
        os_threadAttrInit (&thread_os_threadAttr);
        thread_os_threadAttr.schedClass = OS_SCHED_REALTIME;
        thread_os_threadAttr.schedPriority = sched_get_priority_min (SCHED_FIFO);
        osResult = os_threadCreate (&thread_os_threadId, "ThreadCreate5", &thread_os_threadAttr, &new_thread, "os_threadCreate");
        CU_ASSERT (osResult == os_resultSuccess);
        if (osResult == os_resultSuccess) {
            int policy;
            struct sched_param sched_param;

            result_int = pthread_getschedparam (thread_os_threadId.v, &policy, &sched_param);
            CU_ASSERT (result_int == 0);

            if (result_int == 0) {
                CU_ASSERT (policy == SCHED_FIFO);
            } else {
                printf ("pthread_getschedparam failed\n");
            }
            osResult = os_threadWaitExit (thread_os_threadId, NULL);
        } else {
            printf ("os_threadCreate failed\n");
        }
    }
#else /* WIN32 */
    printf ("N.A - Not tested on Windows or vxworks kernel\n");
#endif

    /* Check os_threadCreate with scheduling class SCHED_TIMESHARE and min priority */
    printf ("Starting os_thread_create_006\n");
#ifndef WIN32
    os_threadAttrInit (&thread_os_threadAttr);
    thread_os_threadAttr.schedClass = OS_SCHED_TIMESHARE;
#ifdef _WRS_KERNEL
      thread_os_threadAttr.schedPriority = 250;
#else
      thread_os_threadAttr.schedPriority = sched_get_priority_min (SCHED_OTHER);
#endif
    osResult = os_threadCreate (&thread_os_threadId, "ThreadCreate6", &thread_os_threadAttr, &new_thread, "os_threadCreate");
#ifdef _WRS_KERNEL
    if (osResult == os_resultSuccess)
        printf ("os_threadCreate failed - Expected failure from VXWORKS\n");
    else
        printf ("OS_SCHED_TIMESHARE not supported\n");
#else
    CU_ASSERT (osResult == os_resultSuccess);

    if (osResult == os_resultSuccess) {
        int policy;
        struct sched_param sched_param;

        result_int = pthread_getschedparam (thread_os_threadId.v, &policy, &sched_param);
        CU_ASSERT (result_int == 0);

        if (result_int == 0) {
            CU_ASSERT (sched_param.sched_priority == sched_get_priority_min (SCHED_OTHER));
        } else {
            printf ("pthread_getschedparam failed\n");
        }
        osResult = os_threadWaitExit (thread_os_threadId, NULL);
    } else {
        printf ("os_threadCreate failed.\n");
    }
#endif /* _WRS_KERNEL */
#else
    printf ("N.A - Not tested on Windows.\n");
#endif /* WIN32 */

    /* Check os_threadCreate with scheduling class SCHED_TIMESHARE and max priority */
    printf ("Starting os_thread_create_007\n");
#ifndef WIN32
    os_threadAttrInit (&thread_os_threadAttr);
    thread_os_threadAttr.schedClass = OS_SCHED_TIMESHARE;
#ifdef _WRS_KERNEL
        thread_os_threadAttr.schedPriority = 60;
#else
        thread_os_threadAttr.schedPriority = sched_get_priority_max (SCHED_OTHER);
#endif
    osResult = os_threadCreate (&thread_os_threadId, "ThreadCreate7", &thread_os_threadAttr, &new_thread, "os_threadCreate");
#ifdef _WRS_KERNEL
    if (osResult == os_resultSuccess) {
        printf ("os_threadCreate failed - Expected failure from VXWORKS\n");
    } else {
        printf ("OS_SCHED_TIMESHARE not supported\n");
    }
#else
    CU_ASSERT (osResult == os_resultSuccess);

    if (osResult == os_resultSuccess) {
        int policy;
        struct sched_param sched_param;

        result_int = pthread_getschedparam (thread_os_threadId.v, &policy, &sched_param);
        CU_ASSERT (result_int == 0);

        if (result_int == 0) {
            CU_ASSERT (sched_param.sched_priority == sched_get_priority_max (SCHED_OTHER));
        } else {
            printf ("pthread_getschedparam failed\n");
        }
        osResult = os_threadWaitExit (thread_os_threadId, NULL);
    } else {
        printf ("os_threadCreate failed.\n");
    }
#endif /* _WRS_KERNEL */
#else
    printf ("N.A - Not tested on Windows.\n");
#endif /* WIN32 */

    /* Check os_threadCreate with scheduling class SCHED_REALTIME and min priority */
    printf ("Starting os_thread_create_008\n");
#ifndef WIN32
#ifndef VXWORKS_RTP
    if (getuid() != 0 && geteuid() != 0)
    {
        printf ("N.A - Need root privileges to do the test\n");
    }
    else
#endif /* VXWORKS_RTP */
    {
        os_threadAttrInit (&thread_os_threadAttr);
        thread_os_threadAttr.schedClass = OS_SCHED_REALTIME;
#ifdef _WRS_KERNEL
        thread_os_threadAttr.schedPriority = 250;
#else
        thread_os_threadAttr.schedPriority = sched_get_priority_min (SCHED_FIFO);
#endif
        osResult = os_threadCreate (&thread_os_threadId, "ThreadCreate8", &thread_os_threadAttr, &new_thread, "os_threadCreate");
        CU_ASSERT (osResult == os_resultSuccess);

        if (osResult == os_resultSuccess) {
#ifdef _WRS_KERNEL
            TASK_ID id;
            int pri;
            STATUS status;
            sleepSeconds (2);
            pri = 0;
            id = taskNameToId("ThreadCreate8");
            status = taskPriorityGet(id,&pri);
            CU_ASSERT (status == OK);
            CU_ASSERT (pri == 250);
#else
            int policy;
            struct sched_param sched_param;

            result_int = pthread_getschedparam (thread_os_threadId.v, &policy, &sched_param);
            CU_ASSERT (result_int == 0);

            if (result_int == 0) {
                CU_ASSERT (sched_param.sched_priority == sched_get_priority_min (SCHED_FIFO));
            } else {
                printf ("pthread_getschedparam failed.\n");
             }
#endif /* _WRS_KERNEL */
            osResult = os_threadWaitExit (thread_os_threadId, NULL);
        } else {
            printf ("os_threadCreate failed.\n");
        }
    }
#else /* WIN32 */
    printf ("N.A - Not tested on Windows\n");
#endif

    /* Check os_threadCreate with scheduling class SCHED_REALTIME and max priority */
    printf ("Starting os_thread_create_009\n");
#ifndef WIN32
#ifndef VXWORKS_RTP
    if (getuid() != 0 && geteuid() != 0)
    {
        printf ("N.A - Need root privileges to do the test\n");
    }
    else
#endif /* VXWORKS_RTP */
    {
        os_threadAttrInit (&thread_os_threadAttr);
        thread_os_threadAttr.schedClass = OS_SCHED_REALTIME;
#ifdef _WRS_KERNEL
        thread_os_threadAttr.schedPriority = 250;
#else
        thread_os_threadAttr.schedPriority = sched_get_priority_max (SCHED_FIFO);
#endif
        osResult = os_threadCreate (&thread_os_threadId, "ThreadCreate9", &thread_os_threadAttr, &new_thread, "os_threadCreate");
        CU_ASSERT (osResult == os_resultSuccess);

        if (osResult == os_resultSuccess) {
#ifdef _WRS_KERNEL
            int status;
            sleepSeconds (2);
            status = 0;
            taskPriorityGet(taskNameToId("ThreadCreate9"),&status);
            CU_ASSERT (status == 250);
#else
            int policy;
            struct sched_param sched_param;

            result_int = pthread_getschedparam (thread_os_threadId.v, &policy, &sched_param);
            CU_ASSERT (result_int == 0);

            if (result_int == 0) {
                CU_ASSERT (sched_param.sched_priority == sched_get_priority_max (SCHED_FIFO));
            } else {
                printf ("pthread_getschedparam failed.\n");
            }
#endif
            osResult = os_threadWaitExit (thread_os_threadId, NULL);
        } else {
            printf ("os_threadCreate failed.\n");
        }
    }
#else /* WIN32 */
    printf ("N.A - Not tested on Windows\n");
#endif

    /* Check os_threadCreate by checking scheduling scope PTHREAD_SCOPE_SYSTEM */
    printf ("Starting os_thread_create_010\n");
    printf ("N.A - No way to queuery scope from running thread");

    /* Check os_threadCreate and stacksize sttribute */
    printf ("Starting os_thread_create_011\n");
    printf ("N.A - No way to queuery scope from running thread");

    printf ("Ending os_thread_create\n");
}

CU_Test(os_thread, idself)
{
    os_threadId   thread_os_threadId;
    os_threadAttr thread_os_threadAttr;
    os_result osResult;
    uint32_t result_from_thread;

    /* Check if own thread ID is correctly provided */
    printf ("Starting tc_os_threadIdSelf_001\n");
    os_threadAttrInit (&thread_os_threadAttr);
    osResult = os_threadCreate (&thread_os_threadId, "OwnThreadId", &thread_os_threadAttr, &threadId_thread, NULL);
    CU_ASSERT (osResult == os_resultSuccess);

    if (osResult == os_resultSuccess) {
#ifdef _WRS_KERNEL
        sleepSeconds(1);
#endif
        osResult = os_threadWaitExit (thread_os_threadId, &result_from_thread);
        CU_ASSERT (osResult == os_resultSuccess);

        if (osResult == os_resultSuccess) {
            uintmax_t tmp_thread_os_threadId = os_threadIdToInteger(thread_os_threadId);
            CU_ASSERT (thread_id_from_thread == tmp_thread_os_threadId);
            CU_ASSERT (result_from_thread == (uint32_t)tmp_thread_os_threadId);
        } else {
            printf ("os_threadWaitExit failed.\n");
        }
    } else {
        printf ("os_threadCreate failed.\n");
    }

    printf ("Ending tc_threadIdSelf\n");
}

CU_Test(os_thread, join)
{
    os_threadId   thread_os_threadId;
    os_threadAttr thread_os_threadAttr;
    os_result osResult;
    uint32_t result_from_thread;

    /* Wait for thread to terminate and get the return value with Success result,
       while thread is still running */
    printf("Starting os_thread_join_001\n");
    os_threadAttrInit (&thread_os_threadAttr);
    osResult = os_threadCreate (&thread_os_threadId, "threadWaitExit", &thread_os_threadAttr, &threadId_thread, (void *)1);
    CU_ASSERT (osResult == os_resultSuccess);

    if (osResult == os_resultSuccess) {
#ifdef _WRS_KERNEL
        sleepSeconds(1);
#endif
        osResult = os_threadWaitExit (thread_os_threadId, &result_from_thread);
        CU_ASSERT (osResult == os_resultSuccess);

        if (osResult == os_resultSuccess) {
            CU_ASSERT (thread_id_from_thread == os_threadIdToInteger(thread_os_threadId));
            CU_ASSERT (result_from_thread == (uint32_t)thread_id_from_thread);
        } else {
            printf ("os_threadWaitExit failed.\n");
        }
    } else {
        printf ("os_threadCreate failed.\n");
    }

    /* Wait for thread to terminate and get the return value with Success result,
       while thread is already terminated */
    printf ("Starting os_thread_join_002\n");
    os_threadAttrInit (&thread_os_threadAttr);
    osResult = os_threadCreate (&thread_os_threadId, "threadWaitExit", &thread_os_threadAttr, &threadId_thread, NULL);
    CU_ASSERT (osResult == os_resultSuccess);

    if (osResult == os_resultSuccess) {
#ifdef _WRS_KERNEL
        sleepSeconds(1);
#endif
        osResult = os_threadWaitExit (thread_os_threadId, &result_from_thread);
        CU_ASSERT(osResult == os_resultSuccess);

        if (osResult == os_resultSuccess) {
            CU_ASSERT (thread_id_from_thread == os_threadIdToInteger(thread_os_threadId));
            CU_ASSERT (result_from_thread == (uint32_t)thread_id_from_thread);
        } else {
            printf ("os_threadWaitExit failed.\n");
        }
    } else {
        printf ("os_threadCreate failed.\n");
    }

    /* Get thread return value with Fail result because result is already read */
    printf ("Starting tc_os_thread_join_003\n");
    os_threadAttrInit (&thread_os_threadAttr);
    osResult = os_threadCreate (&thread_os_threadId, "threadWaitExit", &thread_os_threadAttr, &threadId_thread, NULL);
    CU_ASSERT (osResult == os_resultSuccess);

    if (osResult == os_resultSuccess) {
#ifdef _WRS_KERNEL
        sleepSeconds(1);
#endif
        osResult = os_threadWaitExit (thread_os_threadId, NULL);
        CU_ASSERT (osResult == os_resultSuccess);
    } else {
        printf ("os_threadCreate failed.\n");
    }

    /* Wait for thread to terminate and get the return value by multiple threads,
       one thread gets Success other Fail */
    printf ("Starting tc_os_thread_join_004\n");
#ifndef WIN32
    os_threadAttrInit (&thread_os_threadAttr);
    {
        os_threadId threadWait1;
        os_result osResult1;

        osResult = os_threadCreate (&thread_os_threadId, "threadToWaitFor", &thread_os_threadAttr, &threadId_thread, (void*) 1);
        CU_ASSERT (osResult == os_resultSuccess);
        osResult1 = os_threadCreate (&threadWait1, "waitingThread1", &thread_os_threadAttr, &get_threadExit_thread, &thread_os_threadId);
        CU_ASSERT (osResult1 == os_resultSuccess);

        if (osResult == os_resultSuccess && osResult1 == os_resultSuccess)
        {
#ifdef _WRS_KERNEL
            sleepSeconds(1);
#endif
            osResult1 = os_threadWaitExit (threadWait1, NULL);

            if (osResult1 != os_resultSuccess) {
                printf ("os_threadWaitExit 1 failed\n");
                CU_ASSERT (osResult1 == os_resultSuccess);
            }
        } else {
            printf ("os_threadCreate failed.\n");
        }
    }
#else /* WIN32 */
    printf ("N.A - Not tested on Windows.\n");
#endif

    /* Wait for thread to terminate and pass NULL for the
       return value address - not interrested */
    printf ("Starting tc_os_threadWaitExit_005\n");
    os_threadAttrInit (&thread_os_threadAttr);
    osResult = os_threadCreate (&thread_os_threadId, "threadWaitExit", &thread_os_threadAttr, &threadId_thread, NULL);
    CU_ASSERT  (osResult == os_resultSuccess);

    if (osResult == os_resultSuccess) {
#ifdef _WRS_KERNEL
        sleepSeconds(1);
#endif
        osResult = os_threadWaitExit (thread_os_threadId, NULL);
        CU_ASSERT (osResult == os_resultSuccess);
        if (osResult != os_resultSuccess)
            printf ("os_threadWaitExit failed.\n");
    } else {
        printf ("os_threadCreate failed.\n");
    }

    printf ("Ending tc_threadWaitExit\n");
}

CU_Test(os_thread, attr_init)
{
    os_threadAttr thread_os_threadAttr;
    /* Check default attributes: schedClass */
    printf ("Starting os_thread_attr_init_001\n");
    os_threadAttrInit (&thread_os_threadAttr);
    CU_ASSERT (thread_os_threadAttr.schedClass == OS_SCHED_DEFAULT);

    /* Check default attributes: schedPriority */
    printf ("Starting os_thread_attr_init_002\n");
#if !(defined _WRS_KERNEL || defined WIN32 || defined __APPLE__)
    os_threadAttrInit (&thread_os_threadAttr);
    CU_ASSERT (thread_os_threadAttr.schedPriority == ((sched_get_priority_min (SCHED_OTHER) + sched_get_priority_max (SCHED_OTHER)) / 2 ));
#else
    /* OSX priorities are different (min=15 and max=47) */
    printf ("N.A - Not tested for VxWorks, Windows and OSX\n");
#endif

    /* Check default attributes: stacksize */
    printf ("Starting os_thread_attr_init_003\n");
    os_threadAttrInit (&thread_os_threadAttr);
    CU_ASSERT (thread_os_threadAttr.stackSize == 0);

    printf ("Ending os_thread_attr_init\n");
}

CU_Test(os_thread, memmalloc)
{
    /* Check os_threadMemMalloc with success result for main thread */
    printf ("Starting os_thread_memmalloc_001\n");
    returnval = os_threadMemMalloc (3, 100);
    CU_ASSERT (returnval != NULL);

    printf ("Ending tc_thread_memmalloc\n");
}

CU_Test(os_thread, memget)
{
    /* Check os_threadMemGet for main thread and non allocated index */
    printf ("Starting os_thread_memget_001\n");
    returnval = os_threadMemGet (OS_THREAD_WARNING);
    CU_ASSERT (returnval == NULL);

    /* Check os_threadMemGet for main thread and allocated index */
    printf ("Starting os_thread_memget_002\n");
    /* FIXME: This test is no good. Apart from the fact that a valid thread
              memory index should be used (os_threadMemoryIndex), this also
              does not work if the test is executed in a self-contained
              manner using the CUnit runner. For now just work around it by
              first doing a os_threadMemMalloc. */
    (void)os_threadMemMalloc(3, 100);
    returnval = os_threadMemGet (3);
    CU_ASSERT (returnval != NULL);

    printf ("Ending tc_thread_memget\n");
}

CU_Test(os_thread, memfree)
{
    /* Check os_threadMemFree for main thread and non allocated index */
    printf ("Starting os_thread_memfree_001\n");
    os_threadMemFree (OS_THREAD_WARNING);
    returnval = os_threadMemGet (OS_THREAD_WARNING);
    CU_ASSERT (returnval == NULL);

    /* Check os_threadMemFree for main thread and allocated index */
    printf ("Starting os_thread_memfree_002\n");
    /* FIXME: See comments on memget test. */
    (void)os_threadMemMalloc(3, 100);
    returnval = os_threadMemGet(3);
    CU_ASSERT(returnval != NULL);
    os_threadMemFree (3);
    returnval = os_threadMemGet (3);
    CU_ASSERT (returnval == NULL);

    printf ("Ending os_thread_memfree\n");
}

CU_Test(os_thread, module)
{
    os_threadId tid;
    os_threadAttr tattr;
    os_result res;

    os_threadAttrInit (&tattr);
    /* Run the following tests for child thread */
    res = os_threadCreate (&tid, "ThreadMemory", &tattr, &threadMemory_thread, NULL);
    CU_ASSERT_EQUAL(res, os_resultSuccess);
    if (res == os_resultSuccess) {
#ifdef _WRS_KERNEL
        sleepSeconds(1);
#endif
        res = os_threadWaitExit (tid, NULL);
        CU_ASSERT_EQUAL(res, os_resultSuccess);
    }
}
