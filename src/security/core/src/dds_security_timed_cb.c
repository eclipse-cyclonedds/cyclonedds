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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "dds/ddsrt/atomics.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/log.h"
#include "dds/ddsrt/sync.h"
#include "dds/ddsrt/threads.h"
#include "dds/ddsrt/time.h"
#include "dds/ddsrt/fibheap.h"


#include "dds/security/core/dds_security_timed_cb.h"


struct dds_security_timed_dispatcher_t
{
    bool active;
    void *listener;
    ddsrt_fibheap_t events;
};

struct list_dispatcher_t
{
    struct list_dispatcher_t *next;
    struct dds_security_timed_dispatcher_t *dispatcher;
};

struct event_t
{
    ddsrt_fibheap_node_t heapnode;
    dds_security_timed_cb_t callback;
    dds_time_t trigger_time;
    void *arg;
};

static int compare_timed_cb_trigger_time(const void *va, const void *vb);
static const ddsrt_fibheap_def_t timed_cb_queue_fhdef = DDSRT_FIBHEAPDEF_INITIALIZER(offsetof(struct event_t, heapnode), compare_timed_cb_trigger_time);

static int compare_timed_cb_trigger_time(const void *va, const void *vb)
{
    const struct event_t *a = va;
    const struct event_t *b = vb;
    return (a->trigger_time == b->trigger_time) ? 0 : (a->trigger_time < b->trigger_time) ? -1 : 1;
}

struct dds_security_timed_cb_data {
    ddsrt_mutex_t lock;
    ddsrt_cond_t  cond;
    struct list_dispatcher_t *first_dispatcher_node;
    ddsrt_thread_t thread;
    bool terminate;
};


static uint32_t timed_dispatcher_thread(
        void *tcbv)
{
    struct list_dispatcher_t *dispatcher_node;
    struct event_t *event;
    dds_duration_t timeout;
    dds_duration_t remain_time;
    struct dds_security_timed_cb_data *tcb = (struct dds_security_timed_cb_data *)tcbv;

    ddsrt_mutex_lock(&tcb->lock);
    do
    {
        remain_time = DDS_INFINITY;
        for (dispatcher_node = tcb->first_dispatcher_node; dispatcher_node != NULL; dispatcher_node = dispatcher_node->next)
        {
            /* Just some sanity checks. */
            assert(dispatcher_node->dispatcher);
            if (dispatcher_node->dispatcher->active)
            {
                do
                {
                    timeout = DDS_INFINITY;
                    event = ddsrt_fibheap_min(&timed_cb_queue_fhdef, &dispatcher_node->dispatcher->events);
                    if (event)
                    {
                        /* Just some sanity checks. */
                        assert(event->callback);
                        /* Determine the trigger timeout of this callback. */
                        timeout = event->trigger_time - dds_time();
                        if (timeout <= 0)
                        {
                            /* Trigger callback when related dispatcher is active. */
                            event->callback(dispatcher_node->dispatcher,
                                            DDS_SECURITY_TIMED_CB_KIND_TIMEOUT,
                                            dispatcher_node->dispatcher->listener,
                                            event->arg);

                            /* Remove handled event from queue, continue with next. */
                            ddsrt_fibheap_delete(&timed_cb_queue_fhdef, &dispatcher_node->dispatcher->events, event);
                            ddsrt_free(event);
                        }
                        else if (timeout < remain_time)
                        {
                            remain_time = timeout;
                        }
                    }
                }
                while (timeout < 0);
            }
        }
        // tcb->cond condition may be triggered before this thread runs and causes
        // this waitfor to wait infinity, hence the check of the tcb->terminate
        if (((remain_time > 0) || (remain_time == DDS_INFINITY)) && !tcb->terminate)
        {
            /* Wait for new event, timeout or the end. */
            (void)ddsrt_cond_waitfor(&tcb->cond, &tcb->lock, remain_time);
        }

    } while (!tcb->terminate);

    ddsrt_mutex_unlock(&tcb->lock);

    return 0;
}

struct dds_security_timed_cb_data*
dds_security_timed_cb_new()
{
    struct dds_security_timed_cb_data *tcb = ddsrt_malloc(sizeof(*tcb));
    dds_return_t osres;
    ddsrt_threadattr_t attr;

    ddsrt_mutex_init(&tcb->lock);
    ddsrt_cond_init(&tcb->cond);
    tcb->first_dispatcher_node = NULL;
    tcb->terminate = false;

    ddsrt_threadattr_init(&attr);
    osres = ddsrt_thread_create(&tcb->thread, "security_dispatcher", &attr, timed_dispatcher_thread, (void*)tcb);
    if (osres != DDS_RETCODE_OK)
    {
        DDS_FATAL("Cannot create thread security_dispatcher");
    }

    return tcb;
}

void
dds_security_timed_cb_free(
        struct dds_security_timed_cb_data *tcb)
{
    ddsrt_mutex_lock(&tcb->lock);
    tcb->terminate = true;
    ddsrt_mutex_unlock(&tcb->lock);
    ddsrt_cond_signal(&tcb->cond);
    ddsrt_thread_join(tcb->thread, NULL);

    ddsrt_cond_destroy(&tcb->cond);
    ddsrt_mutex_destroy(&tcb->lock);
    ddsrt_free(tcb);
}


struct dds_security_timed_dispatcher_t*
dds_security_timed_dispatcher_new(
        struct dds_security_timed_cb_data *tcb)
{
    struct dds_security_timed_dispatcher_t *d;
    struct list_dispatcher_t *dispatcher_node_new;
    struct list_dispatcher_t *dispatcher_node_wrk;

    /* New dispatcher. */
    d = ddsrt_malloc(sizeof(struct dds_security_timed_dispatcher_t));
    memset(d, 0, sizeof(struct dds_security_timed_dispatcher_t));

    ddsrt_fibheap_init(&timed_cb_queue_fhdef, &d->events);

    dispatcher_node_new = ddsrt_malloc(sizeof(struct list_dispatcher_t));
    memset(dispatcher_node_new, 0, sizeof(struct list_dispatcher_t));
    dispatcher_node_new->dispatcher = d;

    ddsrt_mutex_lock(&tcb->lock);

    /* Append to list */
    if (tcb->first_dispatcher_node) {
        struct list_dispatcher_t *last = NULL;
        for (dispatcher_node_wrk = tcb->first_dispatcher_node; dispatcher_node_wrk != NULL; dispatcher_node_wrk = dispatcher_node_wrk->next) {
            last = dispatcher_node_wrk;
        }
        last->next = dispatcher_node_new;
    } else {
        /* This new event is the first one. */
        tcb->first_dispatcher_node = dispatcher_node_new;
    }

    ddsrt_mutex_unlock(&tcb->lock);


    return d;
}

void
dds_security_timed_dispatcher_free(
        struct dds_security_timed_cb_data *tcb,
        struct dds_security_timed_dispatcher_t *d)
{
    struct event_t *event;
    struct list_dispatcher_t *dispatcher_node;
    struct list_dispatcher_t *dispatcher_node_prev;

    assert(d);

    /* Remove related events from queue. */
    ddsrt_mutex_lock(&tcb->lock);

    while((event = ddsrt_fibheap_extract_min(&timed_cb_queue_fhdef, &d->events)) != NULL)
    {
        event->callback(d, DDS_SECURITY_TIMED_CB_KIND_DELETE, NULL, event->arg);
        ddsrt_free(event);
    }

    /* Remove dispatcher from list */
    dispatcher_node_prev = NULL;
    for (dispatcher_node = tcb->first_dispatcher_node; dispatcher_node != NULL; dispatcher_node = dispatcher_node->next)
    {
        if (dispatcher_node->dispatcher == d)
        {
            /* remove element */
            if (dispatcher_node_prev != NULL)
            {
                dispatcher_node_prev->next = dispatcher_node->next;
            }
            else
            {
                tcb->first_dispatcher_node = dispatcher_node->next;
            }

            ddsrt_free(dispatcher_node);
            break;
        }
        dispatcher_node_prev = dispatcher_node;
    }
    /* Free this dispatcher. */
    ddsrt_free(d);

    ddsrt_mutex_unlock(&tcb->lock);

}


void
dds_security_timed_dispatcher_enable(
        struct dds_security_timed_cb_data *tcb,
        struct dds_security_timed_dispatcher_t *d,
        void *listener)
{
    assert(d);
    assert(!(d->active));

    ddsrt_mutex_lock(&tcb->lock);

    /* Remember possible listener and activate. */
    d->listener = listener;
    d->active = true;

    /* Start thread when not running, otherwise wake it up to
     * trigger callbacks that were (possibly) previously added. */
    ddsrt_cond_signal(&tcb->cond);

    ddsrt_mutex_unlock(&tcb->lock);
}


void
dds_security_timed_dispatcher_disable(
        struct dds_security_timed_cb_data *tcb,
        struct dds_security_timed_dispatcher_t *d)
{
    assert(d);
    assert(d->active);

    ddsrt_mutex_lock(&tcb->lock);

    /* Forget listener and deactivate. */
    d->listener = NULL;
    d->active = false;

    ddsrt_mutex_unlock(&tcb->lock);
}


void
dds_security_timed_dispatcher_add(
        struct dds_security_timed_cb_data *tcb,
        struct dds_security_timed_dispatcher_t *d,
        dds_security_timed_cb_t cb,
        dds_time_t trigger_time,
        void *arg)
{
    struct event_t *event_new;

    assert(d);
    assert(cb);

    /* Create event. */
    event_new = ddsrt_malloc(sizeof(struct event_t));
    memset(event_new, 0, sizeof(struct event_t));
    event_new->trigger_time = trigger_time;
    event_new->callback = cb;
    event_new->arg = arg;

    /* Insert event based on trigger_time. */
    ddsrt_mutex_lock(&tcb->lock);
    ddsrt_fibheap_insert(&timed_cb_queue_fhdef, &d->events, event_new);
    ddsrt_mutex_unlock(&tcb->lock);

    /* Wake up thread (if it's running). */
    ddsrt_cond_signal(&tcb->cond);
}

