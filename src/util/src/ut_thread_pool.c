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
#include <string.h>
#include "os/os.h"
#include "util/ut_thread_pool.h"

typedef struct ddsi_work_queue_job
{
    struct ddsi_work_queue_job * m_next_job; /* Jobs list pointer */
    void (*m_fn) (void *arg);                   /* Thread function */
    void * m_arg;                            /* Thread function argument */
}
* ddsi_work_queue_job_t;

struct ut_thread_pool_s
{
    ddsi_work_queue_job_t m_jobs;      /* Job queue */
    ddsi_work_queue_job_t m_jobs_tail; /* Tail of job queue */
    ddsi_work_queue_job_t m_free;      /* Job free list */
    uint32_t m_thread_max;            /* Maximum number of threads */
    uint32_t m_thread_min;            /* Minimum number of threads */
    uint32_t m_threads;               /* Current number of threads */
    uint32_t m_waiting;               /* Number of threads waiting for a job */
    uint32_t m_job_count;             /* Number of queued jobs */
    uint32_t m_job_max;               /* Maximum number of jobs to queue */
    unsigned short m_count;            /* Counter for thread name */
    os_threadAttr m_attr;              /* Thread creation attribute */
    os_cond m_cv;                    /* Thread wait semaphore */
    os_mutex m_mutex;                  /* Pool guard mutex */
};

static uint32_t ut_thread_start_fn (_In_ void * arg)
{
    ddsi_work_queue_job_t job;
    ut_thread_pool pool = (ut_thread_pool) arg;

    /* Thread loops, pulling jobs from queue */

    os_mutexLock (&pool->m_mutex);

    while (pool->m_jobs != NULL) {
        /* Wait for job */
        os_condWait (&pool->m_cv, &pool->m_mutex);

        /* Check if pool deleted or being purged */

        if (pool->m_jobs) {
            /* Take job from queue head */

            pool->m_waiting--;
            job = pool->m_jobs;
            pool->m_jobs = job->m_next_job;
            pool->m_job_count--;

            os_mutexUnlock (&pool->m_mutex);

            /* Do job */

            (job->m_fn) (job->m_arg);

            /* Put job back on free list */

            os_mutexLock (&pool->m_mutex);
            pool->m_waiting++;
            job->m_next_job = pool->m_free;
            pool->m_free = job;
        }
    }

    if (--pool->m_threads) {
        /* last to leave triggers thread_pool_free */
        os_condBroadcast (&pool->m_cv);
    }
    os_mutexUnlock (&pool->m_mutex);
    return 0;
}

static os_result ut_thread_pool_new_thread (ut_thread_pool pool)
{
    static unsigned char pools = 0; /* Pool counter - TODO make atomic */

    char name [64];
    os_threadId id;
    os_result res;

    (void) snprintf (name, sizeof (name), "OSPL-%u-%u", pools++, pool->m_count++);
    res = os_threadCreate (&id, name, &pool->m_attr, &ut_thread_start_fn, pool);

    if (res == os_resultSuccess)
    {
        os_mutexLock (&pool->m_mutex);
        pool->m_threads++;
        pool->m_waiting++;
        os_mutexUnlock (&pool->m_mutex);
    }

    return res;
}

ut_thread_pool ut_thread_pool_new (uint32_t threads, uint32_t max_threads, uint32_t max_queue, os_threadAttr * attr)
{
    ut_thread_pool pool;
    ddsi_work_queue_job_t job;

    /* Sanity check QoS */

    if (max_threads && (max_threads < threads))
    {
        max_threads = threads;
    }
    if (max_queue && (max_queue < threads))
    {
        max_queue = threads;
    }

    pool = os_malloc (sizeof (*pool));
    memset (pool, 0, sizeof (*pool));
    pool->m_thread_min = threads;
    pool->m_thread_max = max_threads;
    pool->m_job_max = max_queue;
    os_threadAttrInit (&pool->m_attr);
    os_mutexInit (&pool->m_mutex);
    os_condInit (&pool->m_cv, &pool->m_mutex);

    if (attr)
    {
        pool->m_attr = *attr;
    }

    /* Create initial threads and jobs */

    while (threads--)
    {
        if (ut_thread_pool_new_thread (pool) != os_resultSuccess)
        {
            ut_thread_pool_free (pool);
            pool = NULL;
            break;
        }
        job = os_malloc (sizeof (*job));
        job->m_next_job = pool->m_free;
        pool->m_free = job;
    }

    return pool;
}

void ut_thread_pool_free (ut_thread_pool pool)
{
    ddsi_work_queue_job_t job;

    if (pool == NULL)
    {
        return;
    }

    os_mutexLock (&pool->m_mutex);

    /* Delete all pending jobs from queue */

    while (pool->m_jobs)
    {
        job = pool->m_jobs;
        pool->m_jobs = job->m_next_job;
        os_free (job);
    }

    /* Wake all waiting threads */

    os_condBroadcast (&pool->m_cv);

    os_mutexUnlock (&pool->m_mutex);

    /* Wait for threads to complete */

    os_mutexLock (&pool->m_mutex);
    while (pool->m_threads != 0)
        os_condWait (&pool->m_cv, &pool->m_mutex);
    os_mutexUnlock (&pool->m_mutex);

    /* Delete all free jobs from queue */

    while (pool->m_free)
    {
        job = pool->m_free;
        pool->m_free = job->m_next_job;
        os_free (job);
    }

    os_condDestroy (&pool->m_cv);
    os_mutexDestroy (&pool->m_mutex);
    os_free (pool);
}

os_result ut_thread_pool_submit (ut_thread_pool pool, void (*fn) (void *arg), void * arg)
{
    os_result res = os_resultSuccess;
    ddsi_work_queue_job_t job;

    os_mutexLock (&pool->m_mutex);

    if (pool->m_job_max && pool->m_job_count >= pool->m_job_max)
    {
        /* Maximum number of jobs reached */

        res = os_resultBusy;
    }
    else
    {
        /* Get or create new job */

        if (pool->m_free)
        {
            job = pool->m_free;
            pool->m_free = job->m_next_job;
        }
        else
        {
            job = os_malloc (sizeof (*job));
        }
        job->m_next_job = NULL;
        job->m_fn = fn;
        job->m_arg = arg;

        /* Add new job to end of queue */

        if (pool->m_jobs)
        {
            pool->m_jobs_tail->m_next_job = job;
        }
        else
        {
            pool->m_jobs = job;
        }
        pool->m_jobs_tail = job;
        pool->m_job_count++;

        /* Allocate thread if more jobs than waiting threads and within maximum */

        if (pool->m_waiting < pool->m_job_count)
        {
            if ((pool->m_thread_max == 0) || (pool->m_threads < pool->m_thread_max))
            {
                /* OK if fails as have queued job */
                (void) ut_thread_pool_new_thread (pool);
            }
        }

        /* Wakeup processing thread */

        os_condSignal (&pool->m_cv);
    }

    os_mutexUnlock (&pool->m_mutex);

    return res;
}

void ut_thread_pool_purge (ut_thread_pool pool)
{
    uint32_t total;

    os_mutexLock (&pool->m_mutex);
    total = pool->m_threads;
    while (pool->m_waiting && (total > pool->m_thread_min))
    {
        pool->m_waiting--;
        total--;
    }
    os_condBroadcast (&pool->m_cv);
    os_mutexUnlock (&pool->m_mutex);
}
