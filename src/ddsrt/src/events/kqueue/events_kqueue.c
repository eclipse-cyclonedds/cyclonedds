/*
 * Copyright(c) 2020 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <sys/event.h>
#include <sys/time.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

#include "dds/ddsrt/events/kqueue.h"
#include "dds/ddsrt/events.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/sync.h"
#include "dds/ddsrt/log.h"

#define EVENTS_CONTAINER_DELTA 8

typedef enum event_status {
  EVENT_STATUS_UNREGISTERED,
  EVENT_STATUS_REGISTERED,
  EVENT_STATUS_DEREGISTERED
} event_status_t;

struct queued_event {
  ddsrt_event_t *external;
  struct kevent internal;
  event_status_t status;
};

typedef struct queued_event queued_event_t;

 /**
 * @brief Darwin (Apple) implementation of ddsrt_event_queue.
 *
 * This implementation uses a kqueue for monitoring a set of filedescriptors for events.
 * Using the kevent call, the kernel can be told to add/modify fd's on its list for monitoring
 * or to wait for events on the monitored fd's. Interrupts of waits are done through writes
 * to an internal pipe.
 */
struct ddsrt_event_queue
{
  queued_event_t*         events;   /**< container for stored events*/
  size_t                  nevents;  /**< number of events stored*/
  size_t                  cevents;  /**< capacity of events stored*/
  size_t                  ievents;  /**< current iterator for getting the next triggered event*/
  size_t                  ndeleted; /**< number of events monitored by kevent deleted since last call to wait function*/
  queued_event_t*         newevents;  /**< container for modified events*/
  size_t                  nnewevents; /**< number of new events added since last call to wait function*/
  size_t                  cnewevents; /**< capacity for new events*/
  int                     kq;       /**< kevent polling instance*/
  struct kevent*          kevents;  /**< array which kevent uses to write back to, has identical size as this->events*/
  ddsrt_mutex_t           lock;     /**< for keeping adds/deletes from occurring simultaneously */
  int                     interrupt[2]; /**< pipe for interrupting waits*/
};

/**
* @brief Initializes an event queue.
*
* Will set the counters to 0 and create the containers for triggers and kevents.
* Will create a kevent kernel event instance and open the interrupt pipe.
*
* @param[in,out] queue The queue to initialize.
*
* @retval DDS_RETCODE_OK
*             The queue was initialized succesfully.
* @retval DDS_RETCODE_ERROR
*             - There was an issue with reserving memory for the (k)event queue.
*             - Kevent instance or interrupt pipe could not be initialized correctly.
*/
static dds_return_t ddsrt_event_queue_init(ddsrt_event_queue_t* queue) ddsrt_nonnull_all;

dds_return_t ddsrt_event_queue_init(ddsrt_event_queue_t* queue)
{
  queue->nevents = 0;
  queue->cevents = EVENTS_CONTAINER_DELTA;
  queue->ievents = 0;
  queue->ndeleted = 0;
  queue->nnewevents = 0;
  queue->cnewevents = EVENTS_CONTAINER_DELTA;

  /*create kevents array*/
  queue->kevents = ddsrt_malloc(sizeof(struct kevent) * queue->cevents);
  if (NULL == queue->kevents)
    goto alloc_fail_0;
  queue->events = ddsrt_malloc(sizeof(queued_event_t) * queue->cevents);
  if (NULL == queue->events)
    goto alloc_fail_1;
  queue->newevents = ddsrt_malloc(sizeof(queued_event_t) * queue->cnewevents);
  if (NULL == queue->newevents)
    goto alloc_fail_2;

  /*create kevent polling instance */
  if (-1 == (queue->kq = kqueue()))
    goto kq_fail;
  else if (-1 == fcntl(queue->kq, F_SETFD, fcntl(queue->kq, F_GETFD) | FD_CLOEXEC))
    goto pipe0_fail;
  /*create interrupt pipe */
  else if (-1 == pipe(queue->interrupt))
    goto pipe0_fail;
  else if (-1 == fcntl(queue->interrupt[0], F_SETFD, fcntl(queue->interrupt[0], F_GETFD) | FD_CLOEXEC) ||
           -1 == fcntl(queue->interrupt[1], F_SETFD, fcntl(queue->interrupt[1], F_GETFD) | FD_CLOEXEC))
    goto pipe1_fail;

  /*register interrupt event*/
  struct kevent kev;
  EV_SET(&kev, queue->interrupt[0], EVFILT_READ, EV_ADD, 0, 0, 0);
  if (-1 == kevent(queue->kq, &kev, 1, NULL, 0, NULL))
    goto pipe1_fail;

  ddsrt_mutex_init(&queue->lock);

  return DDS_RETCODE_OK;

pipe1_fail:
  close(queue->interrupt[0]);
  close(queue->interrupt[1]);
pipe0_fail:
  close(queue->kq);
kq_fail:
  ddsrt_free(queue->newevents);
alloc_fail_2:
  ddsrt_free(queue->events);
alloc_fail_1:
  ddsrt_free(queue->kevents);
alloc_fail_0:
  return DDS_RETCODE_ERROR;
}

/**
* @brief Finishes an event queue.
*
* Will free created containers and close interrupt pipe and kernel event monitor.
*
* @param[in,out] queue The queue to finish.
*/
static void ddsrt_event_queue_fini(ddsrt_event_queue_t* queue) ddsrt_nonnull_all;

void ddsrt_event_queue_fini(ddsrt_event_queue_t* queue)
{
  close(queue->interrupt[0]);
  close(queue->interrupt[1]);
  close(queue->kq);

  ddsrt_mutex_destroy(&queue->lock);
  ddsrt_free(queue->events);
  ddsrt_free(queue->kevents);
  ddsrt_free(queue->newevents);
}

ddsrt_event_queue_t* ddsrt_event_queue_create(void)
{
  ddsrt_event_queue_t* returnptr = ddsrt_malloc(sizeof(ddsrt_event_queue_t));
  assert(returnptr);
  if (DDS_RETCODE_OK != ddsrt_event_queue_init(returnptr)) {
    ddsrt_free(returnptr);
    returnptr = NULL;
  }
  return returnptr;
}

void ddsrt_event_queue_delete(ddsrt_event_queue_t* queue)
{
  ddsrt_event_queue_fini(queue);
  ddsrt_free(queue);
}

size_t ddsrt_event_queue_nevents(ddsrt_event_queue_t* queue)
{
  size_t ret;
  ddsrt_mutex_lock(&queue->lock);
  ret = queue->nevents + queue->nnewevents - queue->ndeleted;
  ddsrt_mutex_unlock(&queue->lock);
  return ret;
}

dds_return_t ddsrt_event_queue_wait(ddsrt_event_queue_t* queue, dds_duration_t reltime)
{
  dds_return_t ret = DDS_RETCODE_OK;

  ddsrt_mutex_lock(&queue->lock);
  queue->ievents = 0;

  /*remove deregistered events*/
  size_t i = 0;
  while (i < queue->nevents)
  {
    queued_event_t* qe = &(queue->events[i]);
    if (EVENT_STATUS_DEREGISTERED == qe->status)
    {
      /*remove the deregistered event*/
      qe->internal.flags = EV_DELETE;
      assert(kevent(queue->kq, &qe->internal, 1, NULL, 0, NULL) != -1);
      *qe = queue->events[--queue->nevents];
    }
    else
    {
      i++;
    }
  }
  queue->ndeleted = 0;

  /*resize queue->events & queue->kevents*/
  if (queue->cevents < queue->nevents + queue->nnewevents)
  {
    queue->cevents = queue->nevents + queue->nnewevents + EVENTS_CONTAINER_DELTA;
    queue->events = ddsrt_realloc(queue->events, sizeof(queued_event_t*) * queue->cevents);
    queue->kevents = ddsrt_realloc(queue->kevents, sizeof(struct kevent) * queue->cevents);
  }

  /*move new events into queue->events*/
  for (i = 0; i < queue->nnewevents; i++)
    queue->events[queue->nevents++] = queue->newevents[i];
  queue->nnewevents = 0;

  /*register/modify events to kevent*/
  for (i = 0; i < queue->nevents; i++)
  {
    queued_event_t* qe = &(queue->events[i]);
    if (EVENT_STATUS_REGISTERED == qe->status &&
        qe == qe->internal.udata)
      continue;
    qe->status = EVENT_STATUS_REGISTERED;
    qe->internal.udata = qe;
    assert(kevent(queue->kq, &qe->internal, 1, NULL, 0, NULL) != -1);
  }

  /*unset triggered status*/
  for (i = 0; i < queue->nevents; i++)
    ddsrt_atomic_st32(&(queue->events[i].external->triggered), DDSRT_EVENT_FLAG_UNSET);
  ddsrt_mutex_unlock(&queue->lock);

  struct timespec tmout = { reltime / DDS_NSECS_IN_SEC ,
                            reltime % DDS_NSECS_IN_SEC };
  int nevs = kevent(queue->kq, NULL, 0, queue->kevents, (int)queue->nevents, &tmout);
  ddsrt_mutex_lock(&queue->lock);
  for (int j = 0; j < nevs; j++)
  {
    queued_event_t* qe = queue->kevents[j].udata;
    /*check for presence of evt in list?*/
    if (NULL != qe &&
        EVENT_STATUS_DEREGISTERED != qe->status)
    {
      ddsrt_atomic_st32(&qe->external->triggered, DDSRT_EVENT_FLAG_READ);
    }
    else
    {
      char buf = 0x0;
      if (1 != read(queue->interrupt[0], &buf, 1))
  	    ret = DDS_RETCODE_ERROR;
    }
  }
  ddsrt_mutex_unlock(&queue->lock);

  return ret;
}

void ddsrt_event_queue_add(ddsrt_event_queue_t* queue, ddsrt_event_t* evt)
{
  ddsrt_mutex_lock(&queue->lock);
  for (size_t i = 0; i < queue->nevents; i++)
  {
    if (queue->events[i].external == evt)
    {
      ddsrt_mutex_unlock(&queue->lock);
      return;
    }
  }

  if (queue->nnewevents == queue->cnewevents)
  {
    queue->cnewevents += EVENTS_CONTAINER_DELTA;
    queue->newevents = ddsrt_realloc(queue->newevents, sizeof(queued_event_t) * queue->cnewevents);
  }
  queued_event_t* qe = &(queue->newevents[queue->nnewevents++]);
  qe->status = EVENT_STATUS_UNREGISTERED;
  qe->external = evt;
  EV_SET(&qe->internal, evt->data.socket.sock, EVFILT_READ, EV_ADD, 0, 0, 0);
  ddsrt_mutex_unlock(&queue->lock);
}

dds_return_t ddsrt_event_queue_signal(ddsrt_event_queue_t* queue)
{
  char buf = 0x0;
  if (1 != write(queue->interrupt[1], &buf, 1))
    return DDS_RETCODE_ERROR;
  return DDS_RETCODE_OK;
}

dds_return_t ddsrt_event_queue_remove(ddsrt_event_queue_t* queue, ddsrt_event_t* evt)
{
  dds_return_t ret = DDS_RETCODE_ALREADY_DELETED;
  ddsrt_mutex_lock(&queue->lock);

  /*check registered events*/
  for (size_t i = 0; i < queue->nevents; i++)
  {
    queued_event_t* qe = &queue->events[i];
    if (qe->external == evt &&
        qe->status != EVENT_STATUS_DEREGISTERED)
    {
      queue->ndeleted++;
      qe->status = EVENT_STATUS_DEREGISTERED;
      ret = DDS_RETCODE_OK;
      break;
    }
  }

  /*check not yet registered events*/
  for (size_t i = 0; i < queue->nnewevents; i++)
  {
    if (queue->newevents[i].external == evt)
    {
      queue->newevents[i] = queue->newevents[--queue->nnewevents];
      ret = DDS_RETCODE_OK;
      break;
    }
  }

  ddsrt_mutex_unlock(&queue->lock);
  return ret;
}

ddsrt_event_t* ddsrt_event_queue_next(ddsrt_event_queue_t* queue)
{
  ddsrt_event_t* ptr = NULL;
  ddsrt_mutex_lock(&queue->lock);
  while (queue->ievents < queue->nevents)
  {
    queued_event_t* qe = &queue->events[queue->ievents++];
    if (EVENT_STATUS_REGISTERED == qe->status &&
        DDSRT_EVENT_FLAG_UNSET != ddsrt_atomic_ld32(&qe->external->triggered))
    {
      ptr = qe->external;
      break;
    }
  }
  ddsrt_mutex_unlock(&queue->lock);
  return ptr;
}
