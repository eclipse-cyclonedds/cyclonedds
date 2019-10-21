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

#include "cyclonedds/ddsrt/io.h"
#include "cyclonedds/ddsrt/heap.h"
#include "cyclonedds/ddsrt/sync.h"
#include "cyclonedds/ddsrt/threads.h"
#include "cyclonedds/ddsrt/thread_pool.h"

typedef struct ddsi_work_queue_job
{
    struct ddsi_work_queue_job * m_next_job; /* Jobs list pointer */
    void (*m_fn) (void *arg);                   /* Thread function */
    void * m_arg;                            /* Thread function argument */
}
* ddsi_work_queue_job_t;

struct ddsrt_thread_pool_s
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
    ddsrt_threadattr_t m_attr;              /* Thread creation attribute */
    ddsrt_cond_t m_cv;                    /* Thread wait semaphore */
    ddsrt_mutex_t m_mutex;                  /* Pool guard mutex */
};

static uint32_t ddsrt_thread_start_fn (void * arg)
{
    ddsi_work_queue_job_t job;
    ddsrt_thread_pool pool = (ddsrt_thread_pool) arg;

    /* Thread loops, pulling jobs from queue */

    ddsrt_mutex_lock (&pool->m_mutex);

    while (pool->m_jobs != NULL) {
        /* Wait for job */
        ddsrt_cond_wait (&pool->m_cv, &pool->m_mutex);

        /* Check if pool deleted or being purged */

        if (pool->m_jobs) {
            /* Take job from queue head */

            pool->m_waiting--;
            job = pool->m_jobs;
            pool->m_jobs = job->m_next_job;
            pool->m_job_count--;

            ddsrt_mutex_unlock (&pool->m_mutex);

            /* Do job */

            (job->m_fn) (job->m_arg);

            /* Put job back on free list */

            ddsrt_mutex_lock (&pool->m_mutex);
            pool->m_waiting++;
            job->m_next_job = pool->m_free;
            pool->m_free = job;
        }
    }

    if (--pool->m_threads) {
        /* last to leave triggers thread_pool_free */
        ddsrt_cond_broadcast (&pool->m_cv);
    }
    ddsrt_mutex_unlock (&pool->m_mutex);
    return 0;
}

static dds_return_t ddsrt_thread_pool_new_thread (ddsrt_thread_pool pool)
{
    static unsigned char pools = 0; /* Pool counter - TODO make atomic */

    char name [64];
    ddsrt_thread_t id;
    dds_return_t res;

    (void) snprintf (name, sizeof (name), "OSPL-%u-%u", pools++, pool->m_count++);
    res = ddsrt_thread_create (&id, name, &pool->m_attr, &ddsrt_thread_start_fn, pool);

    if (res == DDS_RETCODE_OK)
    {
        ddsrt_mutex_lock (&pool->m_mutex);
        pool->m_threads++;
        pool->m_waiting++;
        ddsrt_mutex_unlock (&pool->m_mutex);
    }

    return res;
}

ddsrt_thread_pool ddsrt_thread_pool_new (uint32_t threads, uint32_t max_threads, uint32_t max_queue, ddsrt_threadattr_t * attr)
{
    ddsrt_thread_pool pool;
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

    pool = ddsrt_malloc (sizeof (*pool));
    memset (pool, 0, sizeof (*pool));
    pool->m_thread_min = threads;
    pool->m_thread_max = max_threads;
    pool->m_job_max = max_queue;
    ddsrt_threadattr_init (&pool->m_attr);
    ddsrt_mutex_init (&pool->m_mutex);
    ddsrt_cond_init (&pool->m_cv);

    if (attr)
    {
        pool->m_attr = *attr;
    }

    /* Create initial threads and jobs */

    while (threads--)
    {
        if (ddsrt_thread_pool_new_thread (pool) != DDS_RETCODE_OK)
        {
            ddsrt_thread_pool_free (pool);
            pool = NULL;
            break;
        }
        job = ddsrt_malloc (sizeof (*job));
        job->m_next_job = pool->m_free;
        pool->m_free = job;
    }

    return pool;
}

void ddsrt_thread_pool_free (ddsrt_thread_pool pool)
{
    ddsi_work_queue_job_t job;

    if (pool == NULL)
    {
        return;
    }

    ddsrt_mutex_lock (&pool->m_mutex);

    /* Delete all pending jobs from queue */

    while (pool->m_jobs)
    {
        job = pool->m_jobs;
        pool->m_jobs = job->m_next_job;
        ddsrt_free (job);
    }

    /* Wake all waiting threads */

    ddsrt_cond_broadcast (&pool->m_cv);

    ddsrt_mutex_unlock (&pool->m_mutex);

    /* Wait for threads to complete */

    ddsrt_mutex_lock (&pool->m_mutex);
    while (pool->m_threads != 0)
        ddsrt_cond_wait (&pool->m_cv, &pool->m_mutex);
    ddsrt_mutex_unlock (&pool->m_mutex);

    /* Delete all free jobs from queue */

    while (pool->m_free)
    {
        job = pool->m_free;
        pool->m_free = job->m_next_job;
        ddsrt_free (job);
    }

    ddsrt_cond_destroy (&pool->m_cv);
    ddsrt_mutex_destroy (&pool->m_mutex);
    ddsrt_free (pool);
}

dds_return_t ddsrt_thread_pool_submit (ddsrt_thread_pool pool, void (*fn) (void *arg), void * arg)
{
    dds_return_t res = DDS_RETCODE_OK;
    ddsi_work_queue_job_t job;

    ddsrt_mutex_lock (&pool->m_mutex);

    if (pool->m_job_max && pool->m_job_count >= pool->m_job_max)
    {
        /* Maximum number of jobs reached */

        res = DDS_RETCODE_TRY_AGAIN;
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
            job = ddsrt_malloc (sizeof (*job));
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
                (void) ddsrt_thread_pool_new_thread (pool);
            }
        }

        /* Wakeup processing thread */

        ddsrt_cond_signal (&pool->m_cv);
    }

    ddsrt_mutex_unlock (&pool->m_mutex);

    return res;
}

void ddsrt_thread_pool_purge (ddsrt_thread_pool pool)
{
    uint32_t total;

    ddsrt_mutex_lock (&pool->m_mutex);
    total = pool->m_threads;
    while (pool->m_waiting && (total > pool->m_thread_min))
    {
        pool->m_waiting--;
        total--;
    }
    ddsrt_cond_broadcast (&pool->m_cv);
    ddsrt_mutex_unlock (&pool->m_mutex);
}
