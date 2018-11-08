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

/* Suite os_once*/
CU_Init(os_once)
{
    printf("Run os_once_Initialize\n");

    os_osInit();

    return 0;
}

CU_Clean(os_once)
{
    printf("Run os_once_Cleanup\n");

    os_osExit();

    return 0;
}

static uint32_t counter;

static void once_func(void)
{
    counter++;
    printf("Counter increased to %u\n", counter);
}

/* This test only checks a single-threaded use-case; mostly API availability and mere basics.*/
CU_Test(os_once, basic)
{
    static os_once_t init = OS_ONCE_T_STATIC_INIT;

    printf("Starting os_once_basic\n");

    os_once(&init, &once_func);
    CU_ASSERT(counter == 1);
    os_once(&init, &once_func);
    CU_ASSERT(counter == 1);

    printf("Ending os_once_basic\n");
}

/* Update atomically, so that a failing os_once can be detected and isn't potentially lost due to a race. */
static os_atomic_uint32_t counter1 = OS_ATOMIC_UINT32_INIT(0);
static void once1_func(void)
{
    os_time delay = { .tv_sec = 0,.tv_nsec = 250000000 }; /* 250ms */
    os_nanoSleep(delay);
    printf("%"PRIxMAX": Counter 1 increased to %u\n", os_threadIdToInteger(os_threadIdSelf()), os_atomic_inc32_nv(&counter1));
}

/* Update atomically, so that a failing os_once can be detected and isn't potentially lost due to a race. */
static os_atomic_uint32_t counter2 = OS_ATOMIC_UINT32_INIT(0);
static void once2_func(void)
{
    os_time delay = { .tv_sec = 0, .tv_nsec = 500000000 }; /* 500ms */
    os_nanoSleep(delay);
    printf("%"PRIxMAX": Counter 2 increased to %u\n", os_threadIdToInteger(os_threadIdSelf()), os_atomic_inc32_nv(&counter2));
}

#define OS_ONCE_NUM_THREADS     (20)
#define OS_ONCE_STATE_STARTUP   (0)
#define OS_ONCE_STATE_GO        (1)

struct os_once_parallel {
    os_mutex m;
    os_cond c;
    os_atomic_uint32_t flag;
    os_atomic_uint32_t started;
    os_once_t init1;
    os_once_t init2;
};

static uint32_t
os_once_parallel_thr(
    void *args)
{
    const os_time sched_delay = { .tv_sec = 0,.tv_nsec = 25000000 }; /* 25ms */
    const os_time poll_delay = { .tv_sec = 0,.tv_nsec = 5000000 }; /* 5ms */
    struct os_once_parallel *state = (struct os_once_parallel *)args;
    bool done = false;
    bool started = false;

    while (!done) {
        switch (os_atomic_ld32(&state->flag)) {
        case OS_ONCE_STATE_STARTUP:
            if (!started && (os_atomic_inc32_nv(&state->started) == OS_ONCE_NUM_THREADS)) {
                printf("%"PRIxMAX": Started. Signalling GO.\n", os_threadIdToInteger(os_threadIdSelf()));
                os_atomic_st32(&state->flag, OS_ONCE_STATE_GO);
                os_mutexLock(&state->m);
                os_condBroadcast(&state->c);
                os_mutexUnlock(&state->m);
                os_nanoSleep(sched_delay);
            }
            else {
                if(!started ) printf("%"PRIxMAX": Started. Awaiting GO.\n", os_threadIdToInteger(os_threadIdSelf()));
                os_mutexLock(&state->m);
                (void) os_condTimedWait(&state->c, &state->m, &poll_delay);
                os_mutexUnlock(&state->m);
            }
            started = true;
            break;
        case OS_ONCE_STATE_GO:
            os_once(&state->init1, &once1_func);
            os_once(&state->init2, &once2_func);
            /* FALLS THROUGH */
        default:
            done = true;
            break;
        }
    }

    return 0;
}

CU_Test(os_once, parallel)
{
    os_threadId threads[OS_ONCE_NUM_THREADS];
    struct os_once_parallel state = {
        .init1 = OS_ONCE_T_STATIC_INIT,
        .init2 = OS_ONCE_T_STATIC_INIT,
        .started = OS_ATOMIC_UINT32_INIT(0),
        .flag = OS_ATOMIC_UINT32_INIT(OS_ONCE_STATE_STARTUP)
    };
    os_threadAttr tattr;
    unsigned i;

    printf("Starting os_once_parallel\n");

    os_mutexInit(&state.m);
    os_condInit(&state.c, &state.m);

    os_threadAttrInit(&tattr);
    for (i = 0; i < OS_ONCE_NUM_THREADS; i++) {
        char thrname[16];
        (void) snprintf(thrname, sizeof thrname, "thr%u", i);
        os_threadCreate(&threads[i], thrname, &tattr, &os_once_parallel_thr, &state);
        printf("%"PRIxMAX": Started thread '%s' with thread-id %" PRIxMAX "\n", os_threadIdToInteger(os_threadIdSelf()), thrname, os_threadIdToInteger(threads[i]));
    }

    for (; i != 0; i--) {
        os_threadWaitExit(threads[i - 1], NULL);
        printf("%"PRIxMAX": Thread with thread-id %" PRIxMAX " stopped.\n", os_threadIdToInteger(os_threadIdSelf()), os_threadIdToInteger(threads[i - 1]));
    }

    CU_ASSERT(os_atomic_ld32(&counter1) == 1);
    CU_ASSERT(os_atomic_ld32(&counter2) == 1);

    os_condDestroy(&state.c);
    os_mutexDestroy(&state.m);

    printf("Ending os_once_parallel\n");
}
