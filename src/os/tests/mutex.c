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
#include "CUnit/Runner.h"
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

#define BUSYLOOP       (100000)
#define MAX_LOOPS      (20)

typedef struct {
    os_mutex global_mutex;
    os_threadId global_data;
    int nolock_corrupt_count;
    int nolock_loop_count;
    int lock_corrupt_count;
    int lock_loop_count;
    int trylock_corrupt_count;
    int trylock_loop_count;
    int trylock_busy_count;
    int stop;
} shared_data;

os_threadAttr       mutex_os_threadAttr;
os_threadId         mutex_os_threadId[4];
os_time             delay1 = { 5, 0 };
os_time             pdelay = { 1, 0 };
os_procId           mutex_os_procId;
os_procId           mutex_os_procId1;
os_procId           mutex_os_procId2;
char                buffer[512];
int                 supported_resultBusy;
int                 loop;
static shared_data *sd;
char                filePath[255];

uint32_t concurrent_lock_thread (_In_opt_ void *arg)
{
    int j;
    int loopc = 0;
    int printed = 0;

    while (!sd->stop)
    {
       if (arg) os_mutexLock (&sd->global_mutex);
       sd->global_data = os_threadIdSelf();

       FORCE_SCHEDULING();

       for (j = 0; j < BUSYLOOP; j++);
       if (os_threadIdToInteger(sd->global_data) !=
           os_threadIdToInteger(os_threadIdSelf()))
       {
          if (arg)
          {
             sd->lock_corrupt_count++;
          }
          else
          {
             sd->nolock_corrupt_count++;
          }
          if (!printed) {
              printed++;
          }
       }
       if (arg)
       {
          sd->lock_loop_count++;
          os_mutexUnlock (&sd->global_mutex);
       }
       else
       {
          sd->nolock_loop_count++;
       }

       FORCE_SCHEDULING();

       for (j = 0; j < BUSYLOOP; j++);
       loopc++;
    }
    return 0;
}

uint32_t concurrent_trylock_thread (_In_opt_ void *arg)
{
    int j;
    int loopc = 0;
    int printed = 0;
    os_result result;

    while (!sd->stop)
    {
       if (arg)
       {
          while ((result = os_mutexTryLock (&sd->global_mutex))
                 != os_resultSuccess)
          {
             if (result == os_resultBusy)
             {
                sd->trylock_busy_count++;
             }
             FORCE_SCHEDULING();
          }
       }
       sd->global_data = os_threadIdSelf();

       FORCE_SCHEDULING();

       for (j = 0; j < BUSYLOOP; j++);
       if (os_threadIdToInteger(sd->global_data) !=
           os_threadIdToInteger(os_threadIdSelf()))
       {
          if (arg)
          {
             sd->trylock_corrupt_count++;
          }
          else
          {
             sd->nolock_corrupt_count++;
          }
          if (!printed) {
              printed++;
          }
       }
       if (arg)
       {
          sd->trylock_loop_count++;
          os_mutexUnlock (&sd->global_mutex);
       }
       else
       {
          sd->nolock_loop_count++;
       }

       FORCE_SCHEDULING();

       for (j = 0; j < BUSYLOOP; j++);
       loopc++;
    }
    return 0;
}

CUnit_Suite_Initialize(os_mutex)
{
    printf ( "Run os_mutex_Initialize\n" );

    os_osInit();

    return 0;
}

CUnit_Suite_Cleanup(os_mutex)
{
    printf("Run os_mutex_Cleanup\n");

    os_osExit();

    return 0;
}

/* This test only checks a single-threaded use-case; just API availability.*/
CUnit_Test(os_mutex, basic)
{
    os_mutex m;
    os_result r;

    printf("Starting os_mutex_basic\n");

    os_mutexInit(&m);
    os_mutexLock(&m);
    os_mutexUnlock(&m);
    r = os_mutexLock_s(&m);
    CU_ASSERT_EQUAL(r, os_resultSuccess);  /* Failure can't be forced */
    os_mutexUnlock(&m);
    os_mutexDestroy(&m);
    printf("Ending os_mutex_basic\n");
}

#define RUNTIME_SEC         (4)
#define NUM_THREADS         (8)
#define OS_STRESS_STOP      (0)
#define OS_STRESS_GO        (1)
#define THREAD_NAME_LEN     (8)

struct os_mutex_stress {
    os_threadId tid;
    os_mutex m;
    os_atomic_uint32_t * flag;
    char name[THREAD_NAME_LEN];
};

static uint32_t
os_mutex_init_thr(
    void *args)
{
    struct os_mutex_stress *state = (struct os_mutex_stress *)args;
    os_result r;
    uint32_t iterations = 0;

    do {
        os_mutexInit(&state->m);
        r = os_mutexLock_s(&state->m); /* Use the mutex to check that all is OK. */
        CU_ASSERT_EQUAL(r, os_resultSuccess);  /* Failure can't be forced. */
        os_mutexUnlock(&state->m);
        os_mutexDestroy(&state->m);
        iterations++;
    } while ( os_atomic_ld32(state->flag) != OS_STRESS_STOP && r == os_resultSuccess);

    printf("%s <%"PRIxMAX">: Performed %u iterations. Stopping now.\n", state->name, os_threadIdToInteger(os_threadIdSelf()), iterations);
    return r != os_resultSuccess; /* Return true on faulure */
}

CUnit_Test(os_mutex, init_stress)
{
    struct os_mutex_stress threads[NUM_THREADS];
    os_threadAttr tattr;
    unsigned i;
    os_atomic_uint32_t flag = OS_ATOMIC_UINT32_INIT(OS_STRESS_GO);
    os_time runtime = { .tv_sec = RUNTIME_SEC, .tv_nsec = 0 };

    printf("Starting os_mutex_init_stress\n");

    os_threadAttrInit(&tattr);
    for ( i = 0; i < NUM_THREADS; i++ ) {
        (void) snprintf(&threads[i].name[0], THREAD_NAME_LEN, "thr%u", i);
        threads[i].flag = &flag;
        os_threadCreate(&threads[i].tid, threads[i].name, &tattr, &os_mutex_init_thr, &threads[i]);
        printf("main <%"PRIxMAX">: Started thread '%s' with thread-id %" PRIxMAX "\n", os_threadIdToInteger(os_threadIdSelf()), threads[i].name, os_threadIdToInteger(threads[i].tid));
    }

    printf("main <%"PRIxMAX">: Test will run for ~%ds with %d threads\n", os_threadIdToInteger(os_threadIdSelf()), RUNTIME_SEC, NUM_THREADS);
    os_nanoSleep(runtime);
    os_atomic_st32(&flag, OS_STRESS_STOP);

    for ( ; i != 0; i-- ) {
        uint32_t thread_failed;
        os_threadWaitExit(threads[i - 1].tid, &thread_failed);
        printf("main <%"PRIxMAX">: Thread %s <%" PRIxMAX "> stopped with result %s.\n", os_threadIdToInteger(os_threadIdSelf()), threads[i - 1].name, os_threadIdToInteger(threads[i - 1].tid), thread_failed ? "FAILED" : "PASS");

        CU_ASSERT_FALSE(thread_failed);
    }
    printf("Ending os_mutex_init_stress\n");
}

CUnit_Test(os_mutex, lock, false)
{
    /* Test critical section access with locking and PRIVATE scope  */
    printf ("Starting tc_os_mutex_lock_001\n");
    os_threadAttrInit (&mutex_os_threadAttr);

    FORCE_SCHEDULING();

    delay1.tv_sec = 3;
    printf ("Testing for %d.%9.9d seconds without lock\n", delay1.tv_sec, delay1.tv_nsec);
    sd->stop = 0;
    sd->nolock_corrupt_count = 0;
    sd->nolock_loop_count = 0;
    sd->lock_corrupt_count = 0;
    sd->lock_loop_count = 0;
    sd->trylock_corrupt_count = 0;
    sd->trylock_loop_count = 0;
    sd->trylock_busy_count = 0;
    os_threadCreate (&mutex_os_threadId[0], "thr0", &mutex_os_threadAttr, &concurrent_lock_thread, NULL);
    os_threadCreate (&mutex_os_threadId[1], "thr1", &mutex_os_threadAttr, &concurrent_lock_thread, NULL);
    os_threadCreate (&mutex_os_threadId[2], "thr2", &mutex_os_threadAttr, &concurrent_trylock_thread, NULL);
    os_threadCreate (&mutex_os_threadId[3], "thr3", &mutex_os_threadAttr, &concurrent_trylock_thread, NULL);
    os_nanoSleep (delay1);
    sd->stop = 1;
    os_threadWaitExit (mutex_os_threadId[0], NULL);
    os_threadWaitExit (mutex_os_threadId[1], NULL);
    os_threadWaitExit (mutex_os_threadId[2], NULL);
    os_threadWaitExit (mutex_os_threadId[3], NULL);
    printf ("All threads stopped\n");

    delay1.tv_sec = 3;
    printf ("Testing for %d.%9.9d seconds with lock\n", delay1.tv_sec, delay1.tv_nsec);
    sd->stop = 0;
    sd->nolock_corrupt_count = 0;
    sd->nolock_loop_count = 0;
    sd->lock_corrupt_count = 0;
    sd->lock_loop_count = 0;
    sd->trylock_corrupt_count = 0;
    sd->trylock_loop_count = 0;
    sd->trylock_busy_count = 0;
    os_threadCreate (&mutex_os_threadId[0], "thr0", &mutex_os_threadAttr, &concurrent_lock_thread, (void *)1);
    os_threadCreate (&mutex_os_threadId[1], "thr1", &mutex_os_threadAttr, &concurrent_lock_thread, (void *)1);
    os_threadCreate (&mutex_os_threadId[2], "thr2", &mutex_os_threadAttr, &concurrent_trylock_thread, (void *)1);
    os_threadCreate (&mutex_os_threadId[3], "thr3", &mutex_os_threadAttr, &concurrent_trylock_thread, (void *)1);
    os_nanoSleep (delay1);
    sd->stop = 1;
    os_threadWaitExit (mutex_os_threadId[0], NULL);
    os_threadWaitExit (mutex_os_threadId[1], NULL);
    os_threadWaitExit (mutex_os_threadId[2], NULL);
    os_threadWaitExit (mutex_os_threadId[3], NULL);
    printf ("All threads stopped\n");

    CU_ASSERT (sd->lock_corrupt_count == 0 || sd->lock_loop_count > 0);

    /* Lock mutex with PRIVATE scope and Success result */
    printf ("Starting tc_os_mutex_lock_002\n");
    os_mutexLock (&sd->global_mutex); //Cannot be checked
    os_mutexUnlock (&sd->global_mutex);

    /* Lock mutex with PRIVATE scope and Fail result */
    printf ("Starting tc_os_mutex_lock_003\n");
    printf ("N.A - Failure cannot be forced\n");

    /* mutexLock_s with PRIVATE scope and Success result */
    printf ("Starting tc_os_mutex_lock_004\n");
    CU_ASSERT (os_mutexLock_s (&sd->global_mutex) == os_resultSuccess);
    os_mutexUnlock (&sd->global_mutex);

    printf ("Ending os_mutex_lock\n");
}

CUnit_Test(os_mutex, trylock, false)
{
    os_result result;

    /* Test critical section access with trylocking and PRIVATE scope */
    printf ("Starting os_mutex_trylock_001\n");
    CU_ASSERT (sd->trylock_corrupt_count == 0 || sd->trylock_loop_count > 0);

    /* TryLock mutex with PRIVATE scope and Success result */
    printf ("Starting os_mutex_trylock_002\n");
    result = os_mutexTryLock (&sd->global_mutex);
    CU_ASSERT (result == os_resultSuccess);

    /* TryLock mutex with PRIVATE scope and Busy result */
    printf ("Starting os_mutex_trylock_003\n");
  #if defined(__VXWORKS__) && !defined(_WRS_KERNEL)
    printf ("N.A - Mutexes are recursive on VxWorks RTP so this test is  disabled\n");
  #endif

    result = os_mutexTryLock (&sd->global_mutex);
    CU_ASSERT (result == os_resultBusy);

    printf ("Ending os_mutex_trylock\n");
}

CUnit_Test(os_mutex, destroy, false)
{
    /* Deinitialize mutex with PRIVATE scope and Success result */
    printf ("Starting os_mutex_destroy_001\n");
    os_mutexDestroy(&sd->global_mutex); // Cannot be checked directly - Success is assumed

    /* Deinitialize mutex with PRIVATE scope and Fail result */
    printf ("Starting os_mutex_destroy_002\n");
    printf ("N.A - Failure cannot be forced\n");

    printf ("Ending os_mutex_destroy\n");
}
