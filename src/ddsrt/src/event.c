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
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/event.h"
#include "dds/ddsrt/sockets.h"
#include "dds/ddsrt/log.h"

#ifdef  __VXWORKS__
#include <pipeDrv.h>
#include <ioLib.h>
#include <string.h>
#include <selectLib.h>
#define OSPL_PIPENAMESIZE 26
#endif

#define MODE_KQUEUE 1
#define MODE_SELECT 2
#define MODE_WFMEVS 3
#define MODE_EPOLL 4

#if defined __APPLE__
#define MODE_SEL MODE_KQUEUE
#elif defined WINCE || defined _WIN32
#define MODE_SEL MODE_WFMEVS
#else
#define MODE_SEL MODE_SELECT
#endif

#if MODE_SEL == MODE_KQUEUE
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/fcntl.h>
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#define MAX_TRIGGERS 65535 /*determine maximum number of events supported by kqueue*/
#elif MODE_SEL == MODE_SELECT
#define MAX_TRIGGERS FD_SETSIZE
#if !_WIN32 && !LWIP_SOCKET

#ifndef __VXWORKS__
#include <sys/fcntl.h>
#endif /* __VXWORKS__ */

#ifndef _WRS_KERNEL
#include <sys/select.h>
#endif

#ifdef __sun
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#endif

#endif

#elif MODE_SEL == MODE_WFMEVS
#define MAX_TRIGGERS MAXIMUM_WAIT_OBJECTS
#elif MODE_SEL == MODE_EPOLL
#define MAX_TRIGGERS 1024  /*determine maximum number of events supported by epoll*/
#else
#error "no mode selected"
#endif

 /**
 * @brief ddsrt_event creation function
 *
 * @param mon_type type of object being monitored
 * @param ptr_to_mon pointer to object being monitored
 * @param mon_sz size in bytes of object being monitored
 * @param evt_type type of event being monitored for
 *
 * @returns the created struct
 */
ddsrt_event_t ddsrt_event_create(enum ddsrt_monitorable mon_type, const void* ptr_to_mon, size_t mon_sz, int evt_type) {
  ddsrt_event_t returnval;
  /*boundary check for storage size*/
  assert(0 != mon_sz &&
          DDSRT_EVENT_MONITORABLE_MAX_BYTES >= mon_sz);

  returnval.mon_type = mon_type;
  returnval.evt_type = evt_type;
  returnval.mon_sz = mon_sz;
  if (NULL != ptr_to_mon) memcpy(returnval.mon_bytes, ptr_to_mon, mon_sz);
  else memset(returnval.mon_bytes, 0x0, mon_sz);

  return returnval;
}

/**
* @brief ddsrt_event container struct
*
* Stores the events in an array of pointers, the memory for the events is pre-assigned.
* Adding events will make use of the pre-assigned events.
* If the container can be expanded, adding events will cause a doubling of the size when nevents == cevents.
*
* events: array of stored events
* nevents: currently stored number of events
*/
struct event_container {
  ddsrt_event_t       events[MAX_TRIGGERS];     /*events container*/
  size_t              nevents;    /*number of events stored*/
};

/**
* @brief Adds an event to the container.
*
* @param cont container the events is to be added to
* @param evt the event to add
*
* @returns NULL: something went wrong, i.e.: full container, pointer to the event added otherwise
*/
static ddsrt_event_t* event_container_push_event(struct event_container* cont, ddsrt_event_t evt) {
  assert(NULL != cont);

  if (cont->nevents == MAX_TRIGGERS)
    return NULL;

  /*add the ddsrt_event to the end of the queue*/
  cont->events[cont->nevents] = evt;
  return &(cont->events[cont->nevents++]);
}

/**
* @brief Removes the last event from the container and returns a pointer to it.
*
* This pointer is no longer safe after another call to push_event, or expanding the container.
*
* @param cont container to take the event from
*
* @returns NULL: container is empty, pointer to the last event otherwise
*/
static ddsrt_event_t* event_container_pop_event(struct event_container* cont) {
  assert(NULL != cont);
  if (cont->nevents == 0)
    return NULL;

  return &(cont->events[--cont->nevents]);
}

/**
* @brief Adds the properties for the event to any monitorable already stored, or adds a new event if it is not.
*
* @param cont container to add to
* @param evt event to add
*
* @returns 0: something went wrong
*          1: added a new entry
*          2: modified an existing entry
*/
static int event_container_register_monitorable(struct event_container* cont, ddsrt_event_t evt) {
  assert(NULL != cont);

  for (size_t i = 0; i < cont->nevents; i++) {
    ddsrt_event_t* tmpptr = &(cont->events[i]);
    if (tmpptr->mon_type == evt.mon_type &&
        tmpptr->mon_sz == evt.mon_sz &&
        0 == memcmp(tmpptr->mon_bytes, evt.mon_bytes, evt.mon_sz)) {
      tmpptr->evt_type |= evt.evt_type;
      return 2;
    }
  }

  if (NULL != event_container_push_event(cont, evt)) return 1;
  return 0;
}

/**
* @brief Removes the properties of the event from any monitorable already stored, and removes the trigger if no events are left on the monitorable.
*
* @param cont container to modify
* @param evt event to modify
*
* @returns modified number of entries in the container
*/
static int event_container_deregister_monitorable(struct event_container* cont, ddsrt_event_t evt) {
  assert(NULL != cont);

  for (size_t i = 0; i < cont->nevents; i++) {
    ddsrt_event_t* evtptr = &(cont->events[i]);
    if (evtptr->mon_type == evt.mon_type &&
        evtptr->mon_sz == evt.mon_sz &&
        0 == memcmp(evtptr->mon_bytes, evt.mon_bytes, evt.mon_sz)) {
      evtptr->evt_type &= ~(evt.evt_type);
      if (0x0 == evtptr->evt_type) {
        memmove(evtptr, evtptr + 1, (size_t)(cont->nevents - i - 1)*sizeof(ddsrt_event_t));
        cont->nevents--;
      }
      return 1;
    }
  }

  return 0;
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
  struct event_container  events;        /*container for triggered events*/
  struct event_container  triggers;      /*container for administered triggers*/

  ddsrt_socket_t          triggerfds[2];    /*fds for external triggering*/
  int                     pollinstance;     /*polling instance, in case select is not used*/
  int                     unchangedsincelast; /*is set to 1 when the trigger configuration has not changed since the last call to ddsrt_monitor_start_wait*/
#if MODE_SEL == MODE_KQUEUE
  kevent                  kevents_waiting[MAX_TRIGGERS];      /*events we were waiting for being triggered*/
  kevent                  kevents_triggered[MAX_TRIGGERS];    /*events that have been triggered*/
  size_t                  nkevents;                           /*number of triggers we are waiting on*/
#elif MODE_SEL == MODE_WFMEVS
  WSAEVENT                wevents_waiting[MAX_TRIGGERS];      /* events associated with sockets */
  DWORD                   nwevents;                           /* number of events waiting for trigger (excluding trigger)*/
#elif MODE_SEL == MODE_EPOLL
#elif MODE_SEL == MODE_SELECT
  fd_set                  rfds;                               /*set of fds for reading data*/
#endif
};

/**
 * @brief closes the "pipe" p
 * 
 * dependant on the platform will close the pipe itself or the sockets representing the pipe
 * 
 * @param p array of two ddsrt_socket_t's representing the pipe
 */
static void closepipe(ddsrt_socket_t p[2]) {
#if defined(__VXWORKS__) && defined(__RTP__)
  /*vxworks type pipe*/
  char nameBuf[OSPL_PIPENAMESIZE];
  ioctl(mon->triggerfds[0], FIOGETNAME, &nameBuf);
#endif
#if (defined _WIN32 || defined(_WIN64))
  /*windows type socket*/
  closesocket(p[0]);
  closesocket(p[1]);
#elif !defined(LWIP_SOCKET)
  /*linux type pipe*/
  close(p[0]);
  close(p[1]);
#endif
#if defined(__VXWORKS__) && defined(__RTP__)
  pipeDevDelete((char*)&nameBuf, 0);
#endif
}

/**
 * @brief reads a single byte from the "pipe" p
 *
 * @param p array of two ddsrt socket_t's representing the pipe
 */
static int readpipe(ddsrt_socket_t p[2]) {
  int n1;
  char buf;
#if defined(LWIP_SOCKET)
  return -1;
#elif (defined _WIN32 || defined(_WIN64))
  /*socket type*/
  n1 = recv(p[0], &buf, 1, 0);
#else
  /*pipe type*/
  n1 = (int)read(p[0], &buf, 1);
#endif
  if (n1 != 1) {
    DDS_WARNING("ddsrt_monitor: read failed on trigger pipe\n");
    return -1;
  }
  return 0;
}

ddsrt_monitor_t* ddsrt_monitor_create(void) {

  ddsrt_socket_t temppipe[2] = { -1,-1 };

  int pipe_result = 0;
  /*try to establish trigger functionality*/
#if (defined _WIN32 || defined(_WIN64))
  /*windows type socket*/
  struct sockaddr_in addr;
  socklen_t asize = sizeof(addr);
  ddsrt_socket_t listener = socket(AF_INET, SOCK_STREAM, 0);
  ddsrt_socket_t s1 = socket(AF_INET, SOCK_STREAM, 0);
  ddsrt_socket_t s2 = DDSRT_INVALID_SOCKET;

  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = 0;
  if (bind(listener, (struct sockaddr*)&addr, sizeof(addr)) == -1 ||
    getsockname(listener, (struct sockaddr*)&addr, &asize) == -1 ||
    listen(listener, 1) == -1 ||
    connect(s1, (struct sockaddr*)&addr, sizeof(addr)) == -1 ||
    (s2 = accept(listener, 0, 0)) == -1) {
    closesocket(s1);
    closesocket(s2);
    pipe_result = -1;
  }
  else {
    /* Equivalent to FD_CLOEXEC */
    SetHandleInformation((HANDLE)s1, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation((HANDLE)s2, HANDLE_FLAG_INHERIT, 0);
    temppipe[0] = s1;
    temppipe[1] = s2;
  }
  closesocket(listener);
#elif defined(__VXWORKS__)
  /*vxworks type pipe*/
  char pipename[OSPL_PIPENAMESIZE];
  int pipecount = 0;
  do {
    snprintf((char*)&pipename, sizeof(pipename), "/pipe/ospl%d", pipecount++);
  } while ((pipe_result = pipeDevCreate((char*)&pipename, 1, 1)) == -1 &&
    os_getErrno() == EINVAL);
  if (pipe_result != -1) {
    temppipe[0] = open((char*)&pipename, O_RDWR, 0644);
    temppipe[1] = open((char*)&pipename, O_RDWR, 0644);
  }
  /*the pipe was succesfully created, but one of the sockets on either end was not*/
  if (-1 != pipe_result &&
    (-1 == temppipe[0] || -1 == temppipe[1])) {
    pipeDevDelete(pipename, 0);
    if (-1 != temppipe[0])
      close(temppipe[0]);
    if (-1 != temppipe[1])
      close(temppipe[1]);
    pipe_result = -1;
  }
#elif !defined(LWIP_SOCKET)
  /*linux type pipe*/
  pipe_result = pipe(temppipe);
  (void)fcntl(temppipe[0], F_SETFD, fcntl(temppipe[0], F_GETFD) | FD_CLOEXEC);
  (void)fcntl(temppipe[1], F_SETFD, fcntl(temppipe[1], F_GETFD) | FD_CLOEXEC);
#endif

  if (-1 == pipe_result) {
    DDS_WARNING("ddsrt_monitor: failure creating trigger pipe\n");
    return NULL;
  }

  int pollresult = 0;
#if MODE_SEL == MODE_KQUEUE
  /*create kqueue*/
  int kq = kqueue();
  if (kq != -1) {
    pollresult = fcntl(kq, F_SETFD, fcntl(kq, F_GETFD) | FD_CLOEXEC);
    if (pollresult == -1) {
      close(kq);
    }
    else {
      pollresult = kq;
    }
  }
  else {
    pollresult = -1;
  }
#elif MODE_SEL == MODE_EPOLL
#endif

  if (-1 == pollresult) {
    DDS_WARNING("ddsrt_monitor: failure creating polling instance\n");
    closepipe(temppipe);
    return NULL;
  }

  /*create space*/
  ddsrt_monitor_t* returnptr = ddsrt_malloc(sizeof(ddsrt_monitor_t));
  returnptr->events.nevents = 0;
  returnptr->triggers.nevents = 0;
  returnptr->triggerfds[0] = temppipe[0];
  returnptr->triggerfds[1] = temppipe[1];
  returnptr->pollinstance = pollresult;
  returnptr->unchangedsincelast = 0;
#if !defined(LWIP_SOCKET)
  /*register pipe trigger*/
  ddsrt_monitor_register_trigger(returnptr, ddsrt_event_create_val(ddsrt_monitorable_socket, returnptr->triggerfds[0], ddsrt_monitorable_event_data_in));
#endif

#if MODE_SEL == MODE_KQUEUE
  returnptr->kevents = 0;
#elif MODE_SEL == MODE_WFMEVS
  returnptr->nwevents = 0;
  for (int i = 0; i < MAX_TRIGGERS; i++) {
    returnptr->wevents_waiting[i] = WSACreateEvent();
    /*check for errors?*/
  }
#elif MODE_SEL == MODE_EPOLL
#endif

  return returnptr;
}

void ddsrt_monitor_destroy(ddsrt_monitor_t* mon) {
  assert(NULL != mon);
  
#if MODE_SEL == MODE_KQUEUE
  close(mon->pollinstance);
#elif MODE_SEL == MODE_WFMEVS
  for (DWORD i = 0; i < mon->nwevents; i++)
    WSACloseEvent(mon->wevents_waiting[i]);
#elif MODE_SEL == MODE_EPOLL
#endif

  /*cleaning up trigger functionality*/
  closepipe(mon->triggerfds);
  
  free(mon);
}

size_t ddsrt_monitor_capacity(ddsrt_monitor_t* mon) {
  assert(NULL != mon);

  return MAX_TRIGGERS;
}

ddsrt_event_t* ddsrt_monitor_pop_event(ddsrt_monitor_t* mon) {
  assert(NULL != mon);

  return event_container_pop_event(&(mon->events));
}

int ddsrt_monitor_register_trigger(ddsrt_monitor_t* mon, ddsrt_event_t evt) {
  assert(NULL != mon);

  if (0 == event_container_register_monitorable(&(mon->triggers), evt)) return -1;
  mon->unchangedsincelast = 0;
  return (int)mon->triggers.nevents;
}

size_t ddsrt_monitor_deregister_trigger(ddsrt_monitor_t* mon, ddsrt_event_t evt) {
  assert(NULL != mon);

  if (0 != event_container_deregister_monitorable(&(mon->triggers), evt)) {
    mon->unchangedsincelast = 0;
  }
  return mon->triggers.nevents;
}

int ddsrt_monitor_start_wait(ddsrt_monitor_t* mon, int milliseconds) {
  assert(NULL != mon);

#if MODE_SEL == MODE_KQUEUE
  /*kqueue (apple) type */

  /*the events have changed*/
  if (0 == mon->unchangedsincelast) {
    /*deregister old events*/
    for (size_t i = 0; i < mon->nkevents; i++) {
      kevent* kev = &(mon->kevents_waiting[i]);
      EV_SET(kev, kev->ident, kev->filter, EV_DELETE, 0, 0, 0);
    }
    mon->nkevents = 0;

    if (kevent(mon->pollinstance, mon->kevents_waiting, mon->kevents, NULL, 0, NULL) == -1)
      abort();

    /*register new events*/
    for (size_t i = 0; i < mon->triggers->nevents; i++) {
      ddsrt_event_t evt = mon->triggers->events[i];
      if (evt.mon_type != ddsrt_monitorable_socket ||
          !(evt.evt_type & ddsrt_monitorable_event_data_in))
        continue;

      ddsrt_socket_t s = *(ddsrt_socket_t*)evt.mon_ptr;
      if (evt.evt_type & ddsrt_monitorable_event_data_in)
        EV_SET(mon->kevents_waiting + mon->nkevents++, s, EVFILT_READ, EV_ADD, 0, 0, 0);
    }
    if (-1 == kevent(mon->pollinstance, mon->kevents_waiting, mon->nkevents, NULL, 0, NULL)) {
      DDS_WARNING("ddsrt_monitor: kqueue event registration failure\n");
      abort();
    }
    mon->unchangedsincelast = 1;
  }

  /*start wait*/
  struct timespec tmout = {milliseconds/1000,     /* waittime (seconds) */
                           milliseconds*1000000}; /* waittime (nanoseconds) */
  int nevs = kevent(mon->pollinstance, NULL, 0, mon->kevents_triggered, mon->nkevents, &tmout);

  /*process events*/
  for (int i = 0; i < nevs; i++) {
    ddsrt_socket_t s = mon->kevents_triggered[i].ident;
    if (s == mon->triggerfds[0]) {
      /*no event generated from writes to the trigger pipe*/
      if (-1 == readpipe(mon->triggerfds))
        return -1;
      continue;
    }
    event_container_push_event(mon->events, ddsrt_event_create_val(ddsrt_monitorable_socket, s, ddsrt_monitorable_event_data_in));
  }
#elif MODE_SEL == MODE_WFMEVS
  /*windows wait for multiple events type*/

  /*the events have changed*/
  if (0 == mon->unchangedsincelast) {
    /*deregister old events*/
    for (DWORD i = 0; i < mon->nwevents; i++) {
      if (!WSACloseEvent(mon->wevents_waiting[i])) {
        //DDS_WARNING("ddsrt_monitor: WSACloseEvent %x failed, error %d\n", mon->wevents_waiting[i+1], os_getErrno());
        return -1;
      }
    }
    mon->nwevents = 0;

    /*register new events*/
    for (size_t i = 0; i < mon->triggers.nevents; i++) {
      ddsrt_event_t evt = mon->triggers.events[i];
      if (evt.mon_type != ddsrt_monitorable_socket ||
          !(evt.evt_type & ddsrt_monitorable_event_data_in))
        continue;

      ddsrt_socket_t s = *(ddsrt_socket_t*)evt.mon_bytes;
      if (SOCKET_ERROR == WSAEventSelect(s, mon->wevents_waiting[mon->nwevents], FD_READ)) {
        WSACloseEvent(mon->wevents_waiting[mon->nwevents]);
        return -1;
      }
      else {
        mon->nwevents++;
      }
    }
    mon->unchangedsincelast = 1;
  }
  else {
    /*reset existing events*/
    for (DWORD i = 0; i < mon->nwevents; i++)
      WSAResetEvent(mon->wevents_waiting[i]);
  }

  DWORD idx;
  if ((idx = WSAWaitForMultipleEvents(mon->nwevents, mon->wevents_waiting, FALSE, milliseconds, FALSE)) == WSA_WAIT_FAILED) {
    //DDS_WARNING("ddsrt_monitor: WSAWaitForMultipleEvents failed, error %d\n", os_getErrno());
    return -1;
  }

  if (idx >= WSA_WAIT_EVENT_0 && idx < WSA_WAIT_EVENT_0 + MAXIMUM_WAIT_OBJECTS) {
    idx -= WSA_WAIT_EVENT_0;
    if (idx == 0) {
      if (-1 == readpipe(mon->triggerfds))
        return -1;
    }
    else {
      event_container_push_event(&(mon->events), mon->triggers.events[idx]);
    }
  }
  else {
    /*something else was returned from WSAWaitForMultipleEvents*/
  }

#elif MODE_SEL == MODE_EPOLL
#elif MODE_SEL == MODE_SELECT
  /*select type*/
  fd_set* rfds = &(mon->rfds);
  FD_ZERO(rfds);

  ddsrt_socket_t maxfd = mon->triggerfds[0];
  for (size_t i = 0; i < mon->triggers.nevents; i++) {
    ddsrt_event_t evt = mon->triggers.events[i];
    if (evt.mon_type != ddsrt_monitorable_socket ||
        !(evt.evt_type & ddsrt_monitorable_event_data_in))
      continue;

    ddsrt_socket_t s = *(ddsrt_socket_t*)evt.mon_bytes;
    FD_SET(s, rfds);
    if (s > maxfd)
      maxfd = s;
  }

  int ready = -1;
  dds_return_t retval = ddsrt_select(maxfd + 1, rfds, NULL, NULL, (dds_duration_t)milliseconds * 1000000, &ready);

  if (DDS_RETCODE_OK == retval ||
      DDS_RETCODE_TIMEOUT == retval) {

    for (size_t i = 0; i < mon->triggers.nevents; i++) {
      ddsrt_event_t evt = mon->triggers.events[i];
      if (evt.mon_type != ddsrt_monitorable_socket)
        continue;

      ddsrt_socket_t s = *(ddsrt_socket_t*)evt.mon_bytes;
      if (!FD_ISSET(s, rfds))
        continue;

#if !defined(LWIP_SOCKET)
      /*skip for lwip*/
      if (s == mon->triggerfds[0]) {
        readpipe(mon->triggerfds);
        continue;
      }
#endif
      event_container_push_event(&(mon->events), evt);
    }
  } else {
    /*something else happened*/
  }
#endif
  return (int)mon->events.nevents;
}

dds_return_t ddsrt_monitor_interrupt_wait(ddsrt_monitor_t* mon) {
  if (NULL == mon)
    return DDS_RETCODE_ERROR;

  dds_return_t returnval = DDS_RETCODE_OK;
  char dummy = 0;
  /*which trigger functionality is available*/
#if (defined _WIN32 || defined(_WIN64))
  /*windows type socket*/
  if (0 != send(mon->triggerfds[1], &dummy, sizeof(dummy), 0))
    returnval = DDS_RETCODE_ERROR;
#elif defined(LWIP_SOCKET)
  /*lwip stack: trigger not available*/
  returnval = DDS_RETCODE_UNSUPPORTED;
#else
  /*linux type pipe*/
  if (0 != write(mon->triggerfds[1], &dummy, sizeof(dummy)))
    returnval = DDS_RETCODE_ERROR;
#endif

  return returnval;
}