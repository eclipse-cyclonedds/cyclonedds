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

#ifdef __VXWORKS__
#   ifdef _WRS_KERNEL
#       define FORCE_SCHEDULING() taskDelay(1)
#   else
#       define FORCE_SCHEDULING() sched_yield()
#   endif
#else
#   define FORCE_SCHEDULING()
#endif

#define ENABLE_TRACING  0
#define BUSYLOOP        (100000)
#define MAX_LOOPS       (20)
#define RWLOCK_THREADS  12

os_time rwlock_delay = { 0, 500 };

typedef struct Par {
    int lock;
    int index;
    int read_access;
    int concurrent_read_access;
} Par;

typedef struct {
    os_rwlock global_rwlock;
    os_threadId global_data;
    os_threadId read_thread[RWLOCK_THREADS];
    int read_corrupt_count;
    int write_corrupt_count;
    int concurrent_read_access;
    int concurrent_write_access;
    int tryread_corrupt_count;
    int trywrite_corrupt_count;
    int concurrent_tryread_access;
    int concurrent_trywrite_access;
    int tryread_busy_count;
    int trywrite_busy_count;
    int stop;
} shared_data;

os_threadAttr       rwlock_os_threadAttr;
os_threadId         rwlock_os_threadId[RWLOCK_THREADS];
static int          i;
char                buffer[512];
int                 supported_resultBusy;
int                 loop;
static shared_data sd;

uint32_t concurrent_write_thread (_In_ void *arg)
{
    struct Par *par = (struct Par *)arg;
    os_threadId myid = os_threadIdSelf();
    int printed = 0;

    while (!sd.stop) {
        if (par->lock) {
            os_rwlockWrite (&sd.global_rwlock);
        }
        sd.global_data = myid;

        FORCE_SCHEDULING();

        os_nanoSleep( rwlock_delay );
        if (os_threadIdToInteger(sd.global_data) != os_threadIdToInteger(myid)) {
            sd.write_corrupt_count++;
            if (!printed) {
                /* printf ("Critical section corrupted during write [%d]\n", par->index); */
                printed++;
            }
        }
        sd.concurrent_write_access++;
        if (par->lock) {
            os_rwlockUnlock (&sd.global_rwlock);
        }

        FORCE_SCHEDULING();

        os_nanoSleep( rwlock_delay );
    }
    return 0;
}

uint32_t concurrent_read_thread (_In_ void *arg)
{
    int j;
    os_threadId prevId;
    struct Par *par = (struct Par *)arg;
    int printed = 0;

    while (!sd.stop) {
        if (par->lock) {
            os_rwlockRead (&sd.global_rwlock);
        }
        sd.read_thread[par->index] = os_threadIdSelf();
        par->read_access++;
        prevId = sd.global_data;

        FORCE_SCHEDULING();

        for (j = 0; j < BUSYLOOP/2; j++) {
            if (os_threadIdToInteger(sd.global_data) !=
           os_threadIdToInteger(prevId)) {
                sd.read_corrupt_count++;
                if (!printed) {
                    /* printf ("Critical section corrupted during read [%d]\n", par->index); */
                    printed++;
                }
                prevId = sd.global_data;
            }

            FORCE_SCHEDULING();
        }
        if (os_threadIdToInteger(sd.read_thread[0]) ||
            os_threadIdToInteger(sd.read_thread[1]) ||
            os_threadIdToInteger(sd.read_thread[2]) ||
            os_threadIdToInteger(sd.read_thread[3]) ||
            os_threadIdToInteger(sd.read_thread[4]) ||
            os_threadIdToInteger(sd.read_thread[5]) ||
            os_threadIdToInteger(sd.read_thread[6]) ||
            os_threadIdToInteger(sd.read_thread[7]) ||
            os_threadIdToInteger(sd.read_thread[8]) ||
            os_threadIdToInteger(sd.read_thread[9]) ||
            os_threadIdToInteger(sd.read_thread[10]) ||
            os_threadIdToInteger(sd.read_thread[11])) {
            par->concurrent_read_access++;
        }
        sd.concurrent_read_access++;
        if (par->lock) {
            os_rwlockUnlock (&sd.global_rwlock);
        }

        FORCE_SCHEDULING();

        os_nanoSleep( rwlock_delay );
    }
    return 0;
}

uint32_t concurrent_trywrite_thread (_In_ void *arg)
{
    struct Par *par = (struct Par *)arg;
    os_result result;
    os_threadId myid = os_threadIdSelf();
    int printed = 0;

    while (!sd.stop) {
        if (par->lock) {
            while ((result = os_rwlockTryWrite (&sd.global_rwlock)) != os_resultSuccess) {
                if (result == os_resultBusy) {
                    sd.trywrite_busy_count++;
                }

                FORCE_SCHEDULING();
            }
        }
        sd.global_data = os_threadIdSelf();

        FORCE_SCHEDULING();

        os_nanoSleep( rwlock_delay );
        if (os_threadIdToInteger(sd.global_data) != os_threadIdToInteger(myid)) {
            sd.trywrite_corrupt_count++;
       if (!printed) {
          /* printf ("Critical section corrupted during trywrite [%d]\n", par->index); */
          printed++;
       }
        }
        sd.concurrent_trywrite_access++;
        if (par->lock) {
            os_rwlockUnlock (&sd.global_rwlock);
        }

        FORCE_SCHEDULING();

        os_nanoSleep( rwlock_delay );
    }
    return 0;
}

uint32_t concurrent_tryread_thread (_In_ void *arg)
{
    int j;
    os_threadId prevId;
    struct Par *par = (struct Par *)arg;
    os_result result;
    int printed = 0;

    while (!sd.stop) {
        if (par->lock) {
            while ((result = os_rwlockTryRead (&sd.global_rwlock)) != os_resultSuccess) {
                if (result == os_resultBusy) {
                    sd.tryread_busy_count++;
                }

                FORCE_SCHEDULING();
            }
        }
        sd.read_thread[par->index] = os_threadIdSelf();
        par->read_access++;
        prevId = sd.global_data;

        FORCE_SCHEDULING();

        for (j = 0; j < BUSYLOOP/2; j++) {
            if (os_threadIdToInteger(sd.global_data) !=
           os_threadIdToInteger(prevId)) {
                sd.tryread_corrupt_count++;
                if (!printed) {
                    /* printf ("Critical section corrupted during read [%d]\n", par->index); */
                    printed++;
                }
                prevId = sd.global_data;
            }

            FORCE_SCHEDULING();
        }
        if (os_threadIdToInteger(sd.read_thread[0]) ||
            os_threadIdToInteger(sd.read_thread[1]) ||
            os_threadIdToInteger(sd.read_thread[2]) ||
            os_threadIdToInteger(sd.read_thread[3]) ||
            os_threadIdToInteger(sd.read_thread[4]) ||
            os_threadIdToInteger(sd.read_thread[5]) ||
            os_threadIdToInteger(sd.read_thread[6]) ||
            os_threadIdToInteger(sd.read_thread[7]) ||
            os_threadIdToInteger(sd.read_thread[8]) ||
            os_threadIdToInteger(sd.read_thread[9]) ||
            os_threadIdToInteger(sd.read_thread[10]) ||
            os_threadIdToInteger(sd.read_thread[11])) {
            par->concurrent_read_access++;
        }
        sd.concurrent_tryread_access++;
        if (par->lock) {
            os_rwlockUnlock (&sd.global_rwlock);
        }

        FORCE_SCHEDULING();

        os_nanoSleep( rwlock_delay );
    }
    return 0;
}

CU_Init(os_rwlock)
{
    int result = 0;
    os_osInit();
    printf("Run os_rwlock_Initialize\n");
  #ifdef OS_LINUX_RWLOCK_H149C
    supported_resultBusy = 1;
  #else
    supported_resultBusy = 0;
  #endif

    return result;
}

CU_Clean(os_rwlock)
{
    int result = 0;

    printf("Run os_rwlock_Cleanup\n");
    os_osExit();
    return result;
}

CU_Test(os_rwlock, init)
{
    /* Initilalize reader/writer lock with PRIVATE scope and Success result */
    printf ("Starting os_rwlock_init_001\n");
    os_rwlockInit (&sd.global_rwlock);

    /* Initilalize reader/writer lock with Fail result */
    printf ("Starting os_rwlock_init_001\n");
    printf ("N.A - Failure cannot be forced\n");

    printf ("Ending os_rwlock_init\n");
}

CU_Test(os_rwlock, read, false)
{
    os_time rdelay = { 3, 0 };
    struct Par par[RWLOCK_THREADS];
    /* Test critical section access without locking to show problem */
    printf ("Starting os_rwlock_read_001\n");
    rdelay.tv_sec = 3;
    printf ("Testing for %d.%9.9d seconds without lock\n", rdelay.tv_sec, rdelay.tv_nsec);
    sd.read_corrupt_count = 0;
    sd.write_corrupt_count = 0;
    sd.concurrent_read_access = 0;
    sd.concurrent_write_access = 0;
    sd.tryread_corrupt_count = 0;
    sd.trywrite_corrupt_count = 0;
    sd.concurrent_tryread_access = 0;
    sd.concurrent_trywrite_access = 0;
    sd.tryread_busy_count = 0;
    sd.trywrite_busy_count = 0;
    sd.stop = 0;
    for (i = 0; i < RWLOCK_THREADS;  i++) {
        par[i].concurrent_read_access = 0;
        par[i].read_access = 0;
        par[i].lock = 0;
        par[i].index = i;
    }
    os_threadAttrInit (&rwlock_os_threadAttr);
    os_threadCreate (&rwlock_os_threadId[0], "thr0", &rwlock_os_threadAttr, &concurrent_write_thread, (void *)&par[0]);
    os_threadCreate (&rwlock_os_threadId[1], "thr1", &rwlock_os_threadAttr, &concurrent_write_thread, (void *)&par[1]);
    os_threadCreate (&rwlock_os_threadId[2], "thr2", &rwlock_os_threadAttr, &concurrent_read_thread, (void *)&par[2]);
    os_threadCreate (&rwlock_os_threadId[3], "thr3", &rwlock_os_threadAttr, &concurrent_read_thread, (void *)&par[3]);
    os_threadCreate (&rwlock_os_threadId[4], "thr4", &rwlock_os_threadAttr, &concurrent_trywrite_thread, (void *)&par[4]);
    os_threadCreate (&rwlock_os_threadId[5], "thr5", &rwlock_os_threadAttr, &concurrent_trywrite_thread, (void *)&par[5]);
    os_threadCreate (&rwlock_os_threadId[6], "thr6", &rwlock_os_threadAttr, &concurrent_tryread_thread, (void *)&par[6]);
    os_threadCreate (&rwlock_os_threadId[7], "thr7", &rwlock_os_threadAttr, &concurrent_tryread_thread, (void *)&par[7]);
    os_nanoSleep (rdelay);
    sd.stop = 1;
    os_threadWaitExit (rwlock_os_threadId[0], NULL);
    os_threadWaitExit (rwlock_os_threadId[1], NULL);
    os_threadWaitExit (rwlock_os_threadId[2], NULL);
    os_threadWaitExit (rwlock_os_threadId[3], NULL);
    os_threadWaitExit (rwlock_os_threadId[4], NULL);
    os_threadWaitExit (rwlock_os_threadId[5], NULL);
    os_threadWaitExit (rwlock_os_threadId[6], NULL);
    os_threadWaitExit (rwlock_os_threadId[7], NULL);

    printf ("All threads stopped\n");
    for (i = 2; i < 4;  i++) {
        printf ("total read access %d, concurrent read access %d for thread %d\n",
            par[i].read_access, par[i].concurrent_read_access, i);
    }
    for (i = 6; i < 8;  i++) {
        printf ("total try read access %d, concurrent try read access %d for thread %d\n",
            par[i].read_access, par[i].concurrent_read_access, i);
    }
    printf ("read_corrupt_count = %d\n", sd.read_corrupt_count);
    printf ("write_corrupt_count = %d\n", sd.write_corrupt_count);
    printf ("tryread_corrupt_count = %d\n", sd.tryread_corrupt_count);
    printf ("trywrite_corrupt_count = %d\n", sd.trywrite_corrupt_count);
    printf ("concurrent_read_access = %d\n", sd.concurrent_read_access);
    printf ("concurrent_write_access = %d\n", sd.concurrent_write_access);
    printf ("concurrent_tryread_access = %d\n", sd.concurrent_tryread_access);
    printf ("concurrent_trywrite_access = %d\n", sd.concurrent_trywrite_access);

    snprintf (buffer, sizeof(buffer), "Corrupt counter = %d, Loop counter is %d",
        sd.read_corrupt_count + sd.write_corrupt_count + sd.tryread_corrupt_count + sd.trywrite_corrupt_count,
        sd.concurrent_read_access + sd.concurrent_write_access + sd.concurrent_tryread_access + sd.concurrent_trywrite_access);

    CU_ASSERT((sd.read_corrupt_count > 0 ||
               sd.write_corrupt_count > 0 ||
               sd.tryread_corrupt_count > 0 ||
               sd.trywrite_corrupt_count > 0) &&
              sd.concurrent_read_access > 0 &&
              sd.concurrent_write_access > 0 &&
              sd.concurrent_tryread_access > 0 &&
              sd.concurrent_trywrite_access > 0);

    /* Test critical section READ access with locking and PRIVATE scope */
    printf ("Starting os_rwlock_read_002\n");
    rdelay.tv_sec = 3;
    printf ("Testing for %d.%9.9d seconds with lock\n", rdelay.tv_sec, rdelay.tv_nsec);
    sd.read_corrupt_count = 0;
    sd.write_corrupt_count = 0;
    sd.concurrent_read_access = 0;
    sd.concurrent_write_access = 0;
    sd.tryread_corrupt_count = 0;
    sd.trywrite_corrupt_count = 0;
    sd.concurrent_tryread_access = 0;
    sd.concurrent_trywrite_access = 0;
    sd.tryread_busy_count = 0;
    sd.trywrite_busy_count = 0;
    sd.stop = 0;
    for (i = 0; i < RWLOCK_THREADS;  i++) {
        par[i].concurrent_read_access = 0;
        par[i].read_access = 0;
        par[i].lock = 1;
        par[i].index = i;
    }
    os_threadAttrInit (&rwlock_os_threadAttr);
    os_threadCreate (&rwlock_os_threadId[0], "thr0", &rwlock_os_threadAttr, &concurrent_write_thread, (void *)&par[0]);
    os_threadCreate (&rwlock_os_threadId[1], "thr1", &rwlock_os_threadAttr, &concurrent_write_thread, (void *)&par[1]);
    os_threadCreate (&rwlock_os_threadId[2], "thr2", &rwlock_os_threadAttr, &concurrent_read_thread, (void *)&par[2]);
    os_threadCreate (&rwlock_os_threadId[3], "thr3", &rwlock_os_threadAttr, &concurrent_read_thread, (void *)&par[3]);
    os_threadCreate (&rwlock_os_threadId[4], "thr4", &rwlock_os_threadAttr, &concurrent_trywrite_thread, (void *)&par[4]);
    os_threadCreate (&rwlock_os_threadId[5], "thr5", &rwlock_os_threadAttr, &concurrent_trywrite_thread, (void *)&par[5]);
    os_threadCreate (&rwlock_os_threadId[6], "thr6", &rwlock_os_threadAttr, &concurrent_tryread_thread, (void *)&par[6]);
    os_threadCreate (&rwlock_os_threadId[7], "thr7", &rwlock_os_threadAttr, &concurrent_tryread_thread, (void *)&par[7]);
    os_nanoSleep (rdelay);
    sd.stop = 1;
    os_threadWaitExit (rwlock_os_threadId[0], NULL);
    os_threadWaitExit (rwlock_os_threadId[1], NULL);
    os_threadWaitExit (rwlock_os_threadId[2], NULL);
    os_threadWaitExit (rwlock_os_threadId[3], NULL);
    os_threadWaitExit (rwlock_os_threadId[4], NULL);
    os_threadWaitExit (rwlock_os_threadId[5], NULL);
    os_threadWaitExit (rwlock_os_threadId[6], NULL);
    os_threadWaitExit (rwlock_os_threadId[7], NULL);

    printf ("All threads stopped\n");
    for (i = 2; i < 4;  i++) {
        printf ("total read access %d, concurrent read access %d for thread %d\n",
            par[i].read_access, par[i].concurrent_read_access, i);
    }
    for (i = 6; i < 8;  i++) {
        printf ("total try read access %d, concurrent try read access %d for thread %d\n",
            par[i].read_access, par[i].concurrent_read_access, i);
    }

    snprintf (buffer, sizeof(buffer), "Corrupt read counter = %d, Read loop counter is %d", sd.read_corrupt_count, sd.concurrent_read_access);
    CU_ASSERT (sd.read_corrupt_count == 0 && sd.concurrent_read_access > 0);

    /* Test read on rwlock with PRIVATE scope and Success result & not locked */
    printf ("Starting os_rwlock_read_003\n");
    os_rwlockRead (&sd.global_rwlock); // Cannot be checked
    os_rwlockUnlock (&sd.global_rwlock);

    /* Test read on rwlock with PRIVATE scope and Success result & locked by read */
    printf ("Starting os_rwlock_read_004\n");
    printf ("N.A - Not implemented\n");

    /* Test read on rwlock with PRIVATE scope and Fail result */
    printf ("Starting os_rwlock_read_005\n");
    printf ("N.A - Failure cannot be forced\n");

    printf ("Ending os_rwlock_read\n");
}

CU_Test(os_rwlock, write, false)
{
    /* Test critical section WRITE access with locking and PRIVATE scope */
    printf ("Starting os_rwlock_write_001\n");

    snprintf (buffer, sizeof(buffer), "Corrupt write counter = %d, Write loop counter is %d", sd.write_corrupt_count, sd.concurrent_write_access);
    CU_ASSERT (sd.write_corrupt_count == 0 && sd.concurrent_write_access > 0);

    /* Test write on rwlock with PRIVATE scope and Success result */
    printf ("Starting os_rwlock_write_002\n");
    os_rwlockWrite (&sd.global_rwlock); //Cannot be checked
    os_rwlockUnlock (&sd.global_rwlock);

    /* Test write on rwlock with PRIVATE scope and Fail result */
    printf ("Starting os_rwlock_write_003\n");
    printf ("N.A - Failure cannot be forced\n");

    printf ("Ending tc_rwlockWrite\n");
}

CU_Test(rwlock, tryread, false)
{
    os_result result;
    /* Test critical section READ access with trylocking and PRIVATE scope */
    printf ("Starting os_rwlock_tryread_001\n");

    snprintf (buffer, sizeof(buffer), "Corrupt tryread counter = %d, Tryread loop counter is %d, Busy counter = %d", sd.tryread_corrupt_count, sd.concurrent_tryread_access, sd.tryread_busy_count);
    CU_ASSERT (sd.tryread_corrupt_count == 0 && sd.concurrent_tryread_access > 0);

    /* Test try read on rwlock with PRIVATE scope and Success result & not locked */
    printf ("Starting os_rwlock_tryread_002\n");
    result = os_rwlockTryRead (&sd.global_rwlock);
    CU_ASSERT (result == os_resultSuccess);
    os_rwlockUnlock (&sd.global_rwlock);

    /* Test try read on rwlock with PRIVATE scope and Success result & locked by read */
    printf ("Starting os_rwlock_tryread_003\n");
    printf ("N.A - Not implemented\n");

    /* Test try read on rwlock with PRIVATE scope and Busy result & locked by write */
    printf ("Starting os_rwlock_tryread_004\n");
    printf ("N.A - Not implemented\n");

    /* Test try read on rwlock with PRIVATE scope and Fail result */
    printf ("Starting os_rwlock_tryread_005\n");
    printf ("N.A - Failure cannot be forced\n");

    printf ("Ending os_rwlock_tryread\n");
}

CU_Test(os_rwlock, trywrite, false)
{
    os_result result;
    /* Test critical section WRITE access with trylocking and PRIVATE scope */
    printf ("Starting os_rwlock_trywrite_001\n");

    snprintf (buffer, sizeof(buffer), "Corrupt trywrite counter = %d, Trywrite loop counter is %d, Busy counter = %d", sd.trywrite_corrupt_count, sd.concurrent_trywrite_access, sd.trywrite_busy_count);
    CU_ASSERT (sd.trywrite_corrupt_count == 0 && sd.concurrent_trywrite_access > 0);

    /* Test try write on rwlock with PRIVATE scope and Success result */
    printf ("Starting os_rwlock_trywrite_002\n");
    result = os_rwlockTryWrite (&sd.global_rwlock);
    CU_ASSERT (result == os_resultSuccess);
    os_rwlockUnlock (&sd.global_rwlock);

    /* Test try write on rwlock with PRIVATE scope and Busy result & locked by read */
    printf ("Starting os_rwlock_trywrite_003\n");
    printf ("N.A - Not implemented\n");

    /* Test try write on rwlock with PRIVATE scope and Busy result & locked by write */
    printf ("Starting os_rwlock_trywrite_004\n");
    printf ("N.A - Not implemented\n");

    /* Test try write on rwlock with PRIVATE scope and Fail result */
    printf ("Starting os_rwlock_trywrite_005\n");
    printf ("N.A - Failure cannot be forced\n");

    printf ("Ending os_rwlock_trywrite\n");
}

CU_Test(os_rwlock, unlock, false)
{
    os_result result;
    /* Unlock rwlock with PRIVATE scope and Success result and claimed with read */
    printf ("Starting os_rwlock_unlock_001\n");
    os_rwlockRead (&sd.global_rwlock);
    os_rwlockUnlock (&sd.global_rwlock); //Cannot be checked

    /* Unlock rwlock with PRIVATE scope and Success result and claimed with try read */
    printf ("Starting os_rwlock_unlock_002\n");
    result = os_rwlockTryRead (&sd.global_rwlock);
    CU_ASSERT (result == os_resultSuccess);
    os_rwlockUnlock (&sd.global_rwlock);

    /* Unlock rwlock with PRIVATE scope and Success result and claimed with write */
    printf ("Starting os_rwlock_unlock_003\n");
    os_rwlockWrite (&sd.global_rwlock);
    os_rwlockUnlock (&sd.global_rwlock); //Cannot be checked

    /* Unlock rwlock with PRIVATE scope and Success result and claimed with try write */
    printf ("Starting os_rwlock_unlock_004\n");
    result = os_rwlockTryWrite (&sd.global_rwlock);
    CU_ASSERT (result == os_resultSuccess);
    os_rwlockUnlock (&sd.global_rwlock);

    /* Unlock rwlock with PRIVATE scope and Fail result */
    printf ("Starting os_rwlock_unlock_005\n");
    printf ("N.A - Failure cannot be forced\n");

    printf ("Ending tc_rwlockUnlock\n");
}

CU_Test(os_rwlock, destroy, false)
{
    /* Deinitialize rwlock with PRIVATE scope and Success result */
    printf ("Starting os_rwlock_destroy_001\n");
    os_rwlockDestroy (&sd.global_rwlock); //Cannot be checked

    /* Deinitialize rwlock with PRIVATE scope and Fail result */
    printf ("Starting os_rwlock_destroy_002\n");
    printf ("N.A - Failure cannot be forced\n");

    printf ("Ending tc_rwlockDestroy\n");
}

CU_Test(os_rwlock, destroy_shared)
{
    os_rwlock mylock;

    os_rwlockInit(&mylock);
    os_rwlockRead(&mylock);
    os_rwlockUnlock(&mylock);
    /* This test used to assert on a debug-build on Windows. Failure can't be forced, but if
     * someone adds the naively sensible assert again, this test won't run on a debug-build. */
    os_rwlockDestroy(&mylock);
    CU_PASS("os_rwlockDestroy succeeds after releasing a shared lock");
}
