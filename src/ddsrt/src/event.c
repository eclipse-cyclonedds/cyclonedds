/*
 * Copyright(c) 2006 to 2020 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/event.h"
#include "dds/ddsrt/event_pipe.h"

 /**
 * @brief ddsrt_event creation function
 *
 * @param mon_type type of object being monitored
 * @param ptr_to_mon pointer to object being monitored
 * @param mon_sz size in bytes of object being monitored
 * @param evt_type type of event being monitored for
 *
 * @returns pointer to the created struct, NULL otherwise
 */
ddsrt_event_t* ddsrt_event_create(enum ddsrt_monitorable mon_type, void* ptr_to_mon, unsigned int mon_sz, int evt_type) {
  if (0 == mon_sz ||
    sizeof(void*) < mon_sz) return NULL;
  if (NULL == ptr_to_mon &&
    ddsrt_monitorable_unset != mon_type) return NULL;

  ddsrt_event_t* returnptr = ddsrt_malloc(sizeof(ddsrt_event_t));
  if (NULL == returnptr) return NULL;

  void* tmpptr = ddsrt_malloc(8);
  if (NULL == tmpptr) {
    free(returnptr);
    return NULL;
  }

  if (NULL != ptr_to_mon) memcpy(tmpptr, ptr_to_mon, mon_sz);
  else memset(tmpptr, 0x0, mon_sz);

  returnptr->mon_type = mon_type;
  returnptr->evt_type = evt_type;
  returnptr->mon_ptr = tmpptr;
  returnptr->mon_sz = mon_sz;

  return returnptr;
}

/**
* @brief ddsrt_event copy function
*
* Will also reassign memory for monitorable storage if it cannot contain the new information.
*
* @param dst destination to copy to
* @param src source to copy from
*/
void ddsrt_event_copy(ddsrt_event_t* dst, ddsrt_event_t* src) {
  dst->mon_type = src->mon_type;
  dst->evt_type = src->evt_type;
  if (dst->mon_sz < src->mon_sz) {
    free(dst->mon_ptr);
    dst->mon_ptr = ddsrt_malloc(src->mon_sz);
  }
  dst->mon_sz = src->mon_sz;
  memcpy(dst->mon_ptr, src->mon_ptr, dst->mon_sz);
}

/**
* @brief ddsrt_event cleanup function
*
* Will also free the memory for monitorable storage.
*
* @param evt event to clean up
*/
void ddsrt_event_destroy(ddsrt_event_t* evt) {
  free(evt->mon_ptr);
  free(evt);
}

/**
* @brief ddsrt_event container struct
*
* Stores the events in an array of pointers, the memory for the events is pre-assigned.
* Adding events will make use of the pre-assigned events.
* If the container can be expanded, adding events will cause a doubling of the size when nevents == cevents.
*
* events: array of pointers to stored events
* nevents: currently stored number of events
* cevents: current capacity for events
* fixedsize: whether the container can be expanded after creation
*/
struct event_container {
  ddsrt_event_t**     events;   /*events container*/
  int                 nevents;    /*number of events stored*/
  int                 cevents;    /*capacity for events at this moment*/
  int                 fixedsize;  /*whether the size of the container can be modified*/
};

/**
* @brief Creates and pre-assigns the container.
*
* Will create the event storage and pre-assign empty events, to not have to push the events in separately.
*
* @param cap initial capacity, must be > 0
* @param fixedsize whether the container can be expanded after creation
*
* @returns pointer to the constructed container in case of success, NULL otherwise
*/
static struct event_container* event_container_create(int cap, int fixedsize) {
  if (cap <= 0) return NULL;
  ddsrt_event_t** tempptr = ddsrt_malloc(sizeof(ddsrt_event_t*) * ((unsigned int)cap));
  if (NULL == tempptr) return NULL;

  struct event_container* returnptr = ddsrt_malloc(sizeof(struct event_container));
  if (NULL == returnptr) {
    free(tempptr);
    return NULL;
  }

  returnptr->cevents = cap;
  returnptr->nevents = 0;
  returnptr->events = tempptr;
  returnptr->fixedsize = fixedsize;

  /*fill the events container with pre-made events*/
  for (int i = 0; i < returnptr->cevents; i++) returnptr->events[i] = ddsrt_event_create(ddsrt_monitorable_unset, NULL, sizeof(void*), ddsrt_monitorable_event_unset);

  return returnptr;
}

/**
* @brief Frees the memory in use by the supplied container.
*
* Also frees all contained events, invalidating those pointers.
*
* @param cont container to be destroyed
*/
static void event_container_destroy(struct event_container* cont) {
  if (NULL == cont) return;

  if (NULL != cont->events) {
    for (int i = 0; i < cont->cevents; i++) {
      if (NULL != cont->events[i]) ddsrt_event_destroy(cont->events[i]);
    }
    free(cont->events);
  }
  free(cont);
}

/**
* @brief Adds an event to the container.
*
* Will expand the container if its capacity has been reached, and it can be expanded.
*
* @param cont container the events is to be added to
* @param evt the event to add
*
* @returns NULL: something went wrong, i.e.: full container that cennot be expanded further, pointer to the event added
*/
static ddsrt_event_t* event_container_push_event(struct event_container* cont, ddsrt_event_t* evt) {
  if (NULL == cont) return NULL;

  /*container is full*/
  if (cont->cevents == cont->nevents) {
    /*cannot modify container size*/
    if (0 != cont->fixedsize) return NULL;

    /*enlarge container*/
    int newcap = cont->cevents * 2;

    /*create and fill new array*/
    ddsrt_event_t** newarray = ddsrt_malloc(sizeof(ddsrt_event_t*) * ((unsigned int)newcap));
    if (NULL == newarray) return NULL;

    memcpy(newarray, cont->events, sizeof(ddsrt_event_t*) * ((unsigned int)cont->cevents));
    for (int i = 0; i < cont->cevents; i++) newarray[i + cont->cevents] = ddsrt_event_create(ddsrt_monitorable_unset, NULL, sizeof(void*), ddsrt_monitorable_event_unset);

    /*assignment and cleanup*/
    free(cont->events);
    cont->events = newarray;
    cont->cevents = newcap;
  }

  /*add the ddsrt_event to the end of the queue*/
  ddsrt_event_t* evtptr = cont->events[cont->nevents++];
  ddsrt_event_copy(evtptr, evt);

  return evtptr;
}

/**
* @brief Removes the last event from the container and returns a pointer to it.
*
* This pointer is no longer safe after another call to push_event, or changing nevents.
*
* @param cont container to take the event from
*
* @returns NULL: something went wrong, pointer to the event otherwise
*/
static ddsrt_event_t* event_container_pop_event(struct event_container* cont) {
  if (NULL == cont ||
    cont->nevents == 0) return NULL;

  return cont->events[--cont->nevents];
}

/**
* @brief Adds the properties for the event to any monitorable already stored, or adds a new event if it is not.
*
* @param cont container to add to
* @param evt event to add
*
* @returns NULL: something went wrong, pointer to the monitorable otherwise
*/
static ddsrt_event_t* event_container_register_monitorable(struct event_container* cont, ddsrt_event_t* evt) {
  if (NULL == cont) return NULL;

  for (int i = 0; i < cont->nevents; i++) {
    ddsrt_event_t* tmpptr = cont->events[i];
    if (tmpptr->mon_type == evt->mon_type &&
      tmpptr->mon_sz == evt->mon_sz &&
      0 == memcmp(tmpptr->mon_ptr, evt->mon_ptr, evt->mon_sz)) {
      tmpptr->evt_type |= evt->evt_type;
      return tmpptr;
    }
  }

  return event_container_push_event(cont, evt);
}

/**
* @brief Removes the properties of the event from any monitorable already stored, and removes the trigger if no events are left on the monitorable.
*
* @param cont container to add to
* @param evt event to add
*
* @returns -1: something went wrong, otherwise number of entries in the container
*/
static int event_container_deregister_monitorable(struct event_container* cont, ddsrt_event_t* evt) {
  if (NULL == cont) return -1;

  for (int i = 0; i < cont->nevents; i++) {
    ddsrt_event_t* tmpptr = cont->events[i];
    if (tmpptr->mon_type == evt->mon_type &&
      tmpptr->mon_sz == evt->mon_sz &&
      0 == memcmp(tmpptr->mon_ptr, evt->mon_ptr, evt->mon_sz)) {
      tmpptr->evt_type &= !(evt->evt_type);
      if (0x0 == tmpptr->evt_type) {
        cont->events[i] = cont->events[--cont->nevents];
        cont->events[cont->nevents] = tmpptr;
      }
      break;
    }
  }

  return cont->nevents;
}

/**
* @brief Implementation of the ddsrt_monitor struct
*
* events: container for the triggered events
* triggers: container for the monitored triggers
* rfds: fd_set of filedescriptors monitored for reading
* triggerfds: pipe for external triggering
*/
struct ddsrt_monitor {
  struct event_container* events;        /*container for triggered events*/
  struct event_container* triggers;      /*container for administered triggers*/

  fd_set                  rfds;           /*set of fds for reading data*/
  ddsrt_socket_t          triggerfds[2];  /*fds for external triggering*/
};

ddsrt_monitor_t* ddsrt_monitor_create(int cap, int fixedsize) {
  if (cap <= 0) return NULL;

  //try to establish pipe
  ddsrt_socket_t tmpsockets[2];
  if (0 != ddsrt_make_pipe(tmpsockets)) return NULL;

  /*create space*/
  ddsrt_monitor_t* returnptr = ddsrt_malloc(sizeof(ddsrt_monitor_t));
  returnptr->events = event_container_create(cap, fixedsize);
  returnptr->triggers = event_container_create(cap, fixedsize);
  returnptr->triggerfds[0] = tmpsockets[0];
  returnptr->triggerfds[1] = tmpsockets[1];

  return returnptr;
}

void ddsrt_monitor_destroy(ddsrt_monitor_t* mon) {
  if (NULL == mon) return;
  event_container_destroy(mon->events);
  event_container_destroy(mon->triggers);
  ddsrt_close_pipe(mon->triggerfds);

  free(mon);
}

ddsrt_event_t* ddsrt_monitor_pop_event(ddsrt_monitor_t* mon) {
  return event_container_pop_event(mon->events);
}

int ddsrt_monitor_register_trigger(ddsrt_monitor_t* mon, ddsrt_event_t* evt) {
  if (NULL == event_container_register_monitorable(mon->triggers, evt)) return -1;

  return mon->triggers->nevents;
}

int ddsrt_monitor_deregister_trigger(ddsrt_monitor_t* mon, ddsrt_event_t* evt) {
  return event_container_deregister_monitorable(mon->triggers, evt);
}

int ddsrt_monitor_start_wait(ddsrt_monitor_t* mon, int milliseconds) {
  if (NULL == mon) return -1;

  fd_set* rfds = &(mon->rfds);
  FD_ZERO(rfds);

  FD_SET(mon->triggerfds[0], rfds);
  ddsrt_socket_t maxfd = mon->triggerfds[0];
  for (int i = 0; i < mon->triggers->nevents; i++) {
    ddsrt_event_t* evtptr = mon->triggers->events[i];
    if (evtptr->mon_type != ddsrt_monitorable_socket) continue;
    ddsrt_socket_t s = *(ddsrt_socket_t*)evtptr->mon_ptr;
    if (evtptr->evt_type & ddsrt_monitorable_event_data_in) {
      FD_SET(s, rfds);
      if (s > maxfd) maxfd = s;
    }
  }

  int ready = -1;
  dds_return_t retval = ddsrt_select(maxfd + 1, rfds, NULL, NULL, (dds_duration_t)milliseconds * 1000000, &ready);

  if (retval == DDS_RETCODE_OK ||
    retval == DDS_RETCODE_TIMEOUT) {
    if (FD_ISSET(mon->triggerfds[0], rfds)) {
      ddsrt_pull_pipe(mon->triggerfds);
    }

    for (int i = 0; i < mon->triggers->nevents; i++) {
      ddsrt_event_t* evtptr = mon->triggers->events[i];
      if (evtptr->mon_type != ddsrt_monitorable_socket) continue;
      ddsrt_socket_t s = *(ddsrt_socket_t*)evtptr->mon_ptr;
      if (FD_ISSET(s, rfds)) {
        event_container_push_event(mon->events, evtptr);
      }
    }
  }
  else {
    //something else happened
  }

  return mon->events->nevents;
}

int ddsrt_monitor_interrupt_wait(ddsrt_monitor_t* mon) {
  if (NULL == mon) return -1;

  /*write to triggerfds*/
  ddsrt_push_pipe(mon->triggerfds);

  return 0;
}