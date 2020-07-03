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

#include <stdio.h>
#include "CUnit/Test.h"
#include "dds/ddsrt/cdtors.h"
#include "dds/ddsrt/events.h"
#include "dds/ddsrt/threads.h"

#ifdef __VXWORKS__
#include <pipeDrv.h>
#include <ioLib.h>
#include <string.h>
#include <selectLib.h>
#define OSPL_PIPENAMESIZE 26
#endif

CU_Init(ddsrt_event)
{
  ddsrt_init();
  return 0;
}

CU_Clean(ddsrt_event)
{
  ddsrt_fini();
  return 0;
}


#if defined(_WIN32)
void ddsrt_sleep(int microsecs)
{
  Sleep(microsecs / 1000);
}
#else
static void ddsrt_sleep(int microsecs)
{
  usleep((unsigned)microsecs);
}
#endif

static dds_return_t ddsrt_pipe_create(ddsrt_socket_t p[2])
{
#if defined(_WIN32)
  /*windows type sockets*/
  struct sockaddr_in addr;
  socklen_t asize = sizeof(addr);
  ddsrt_socket_t listener = socket(AF_INET, SOCK_STREAM, 0);
  p[0] = socket(AF_INET, SOCK_STREAM, 0);
  p[1] = DDSRT_INVALID_SOCKET;

  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = 0;
  if (bind(listener, (struct sockaddr*)&addr, sizeof(addr)) == -1 ||
    getsockname(listener, (struct sockaddr*)&addr, &asize) == -1 ||
    listen(listener, 1) == -1 ||
    connect(p[0], (struct sockaddr*)&addr, sizeof(addr)) == -1 ||
    (p[1] = accept(listener, 0, 0)) == -1)
  {
    closesocket(p[0]);
    closesocket(p[1]);
    return DDS_RETCODE_ERROR;
  }
  SetHandleInformation((HANDLE)p[0], HANDLE_FLAG_INHERIT, 0);
  SetHandleInformation((HANDLE)p[1], HANDLE_FLAG_INHERIT, 0);
#elif defined(__VXWORKS__)  
  /*vxworks type pipe*/
  char pipename[OSPL_PIPENAMESIZE];
  int pipecount = 0;
  int pipe_result = 0;
  do
  {
    snprintf((char*)&pipename, sizeof(pipename), "/pipe/ospl%d", pipecount++);
  } while ((pipe_result = pipeDevCreate((char*)&pipename, 1, 1)) == -1 &&
    os_getErrno() == EINVAL);
  if (pipe_result != -1)
    return DDS_RETCODE_ERROR;
  p[0] = open((char*)&pipename, O_RDWR, 0644);
  p[1] = open((char*)&pipename, O_RDWR, 0644);
  /*the pipe was succesfully created, but one of the sockets on either end was not*/
  if (-1 == p[0] || -1 == p[1])
  {
    pipeDevDelete(pipename, 0);
    if (-1 != p[0])
      close(p[0]);
    if (-1 != p[1])
      close(p[1]);
    return DDS_RETCODE_ERROR;
  }
#elif !defined(LWIP_SOCKET)
  /*simple linux type pipe*/
  if (pipe(p) == -1)
    return DDS_RETCODE_ERROR;
#endif
  return DDS_RETCODE_OK;
}

static dds_return_t ddsrt_pipe_destroy(ddsrt_socket_t p[2])
{
#if !defined(LWIP_SOCKET)
#if defined(__VXWORKS__) && defined(__RTP__)
  char nameBuf[OSPL_PIPENAMESIZE];
  ioctl(p[0], FIOGETNAME, &nameBuf);
#endif
#if defined (_WIN32)
  closesocket(p[0]);
  closesocket(p[1]);
#else
  close(p[0]);
  close(p[1]);
#endif
#if defined(__VXWORKS__) && defined(__RTP__)
  pipeDevDelete((char*)&nameBuf, 0);
#endif
#endif
  return DDS_RETCODE_OK;
}

static dds_return_t ddsrt_pipe_push(ddsrt_socket_t p[2])
{
  char buf = 0x0;
#if defined(LWIP_SOCKET)
  return DDS_RETCODE_OK;
#elif defined (_WIN32)
  if (1 != send(p[1], &buf, 1, 0))
    return DDS_RETCODE_ERROR;
#else
  if (1 != write(p[1], &buf, 1))
    return DDS_RETCODE_ERROR;
#endif
  return DDS_RETCODE_OK;
}

static dds_return_t ddsrt_pipe_pull(ddsrt_socket_t p[2])
{
  char buf;
#if defined(LWIP_SOCKET)
  return DDS_RETCODE_OK;
#elif defined (_WIN32)
  if (1 != recv(p[0], &buf, 1, 0))
    return DDS_RETCODE_ERROR;
#else
  if (1 != read(p[0], &buf, 1))
    return DDS_RETCODE_ERROR;
#endif
  return DDS_RETCODE_OK;
}



CU_Test(ddsrt_event, event_create)
{
  ddsrt_socket_t sock = 123;
  uint32_t flags = DDSRT_EVENT_FLAG_READ;
  ddsrt_event_t evt;
  CU_ASSERT_EQUAL(ddsrt_event_socket_init(&evt,sock,flags), DDS_RETCODE_OK);
  CU_ASSERT_EQUAL(evt.flags, DDSRT_EVENT_FLAG_READ);
  CU_ASSERT_EQUAL(ddsrt_atomic_ld32(&evt.triggered), DDSRT_EVENT_FLAG_UNSET);
  CU_ASSERT_EQUAL(evt.type, DDSRT_EVENT_TYPE_SOCKET);
  CU_ASSERT_EQUAL(evt.data.socket.sock, sock);

  CU_PASS("event_create");
}

CU_Test(ddsrt_event, queue_create)
{
  ddsrt_event_queue_t* q = ddsrt_event_queue_create();
  
  CU_ASSERT_PTR_NOT_EQUAL(q, NULL);
  CU_ASSERT_EQUAL(0,ddsrt_event_queue_nevents(q));
  CU_ASSERT_EQUAL(DDS_RETCODE_OK, ddsrt_event_queue_delete(q));

  CU_PASS("queue_create");
}

CU_Test(ddsrt_event, queue_add_event)
{
  ddsrt_event_queue_t* q = ddsrt_event_queue_create();

  ddsrt_socket_t p1[2];
  ddsrt_socket_t p2[2];
  ddsrt_socket_t p3[2];
  ddsrt_pipe_create(p1);
  ddsrt_pipe_create(p2);
  ddsrt_pipe_create(p3);
  
  uint32_t flags = DDSRT_EVENT_FLAG_READ;

  ddsrt_event_t evt;
  CU_ASSERT_EQUAL(DDS_RETCODE_OK,ddsrt_event_socket_init(&evt,p1[0],flags));
  CU_ASSERT_EQUAL(DDS_RETCODE_OK, ddsrt_event_queue_add(q, &evt));
  CU_ASSERT_EQUAL(1, ddsrt_event_queue_nevents(q));
  CU_ASSERT_EQUAL(DDS_RETCODE_OK,ddsrt_event_socket_init(&evt,p2[0],flags));
  CU_ASSERT_EQUAL(DDS_RETCODE_OK, ddsrt_event_queue_add(q, &evt));
  CU_ASSERT_EQUAL(2, ddsrt_event_queue_nevents(q));
  CU_ASSERT_EQUAL(DDS_RETCODE_OK,ddsrt_event_socket_init(&evt,p3[0],flags));
  CU_ASSERT_EQUAL(DDS_RETCODE_OK, ddsrt_event_queue_add(q, &evt));
  CU_ASSERT_EQUAL(3, ddsrt_event_queue_nevents(q));

  CU_ASSERT_EQUAL(DDS_RETCODE_OK, ddsrt_event_queue_delete(q));
  ddsrt_pipe_destroy(p1);
  ddsrt_pipe_destroy(p2);
  ddsrt_pipe_destroy(p3);
  CU_PASS("queue_add_event");
}

static uint32_t wait_func(void* arg)
{
  printf("starting wait for event\n");
  CU_ASSERT_EQUAL(DDS_RETCODE_OK, ddsrt_event_queue_wait((ddsrt_event_queue_t*)arg, (dds_duration_t)5e8));
  printf("done with wait for event\n");
  return 0;
}

static uint32_t write_func(void* arg)
{
  ddsrt_socket_t* p = (ddsrt_socket_t*)arg;
  printf("starting wait for send to %d\n", (int)p[1]);
  ddsrt_sleep(250000);
  printf("sending to %d\n", (int)p[1]);
  CU_ASSERT_EQUAL(DDS_RETCODE_OK, ddsrt_pipe_push(p));
  printf("done with send\n");
  return 0;
}

static uint32_t interrupt_func(void* arg)
{
  printf("starting wait for interrupt\n");
  ddsrt_sleep(125000);
  printf("interrupting\n");
  CU_ASSERT_EQUAL(DDS_RETCODE_OK, ddsrt_event_queue_signal((ddsrt_event_queue_t*)arg));
  printf("done with interrupt\n");
  return 0;
}

static void test_write(ddsrt_event_queue_t* q, ddsrt_socket_t* p, ddsrt_event_t* evt)
{
  ddsrt_thread_t thr1, thr2;
  uint32_t res1 = 0, res2 = 0;

  ddsrt_threadattr_t attr;
  ddsrt_threadattr_init(&attr);
  attr.schedClass = DDSRT_SCHED_DEFAULT;
  attr.schedPriority = 0;

  /*create thread which waits for event and one which writes to p*/
  dds_return_t ret1 = ddsrt_thread_create(&thr1, "reader", &attr, &wait_func, q);
  dds_return_t ret2 = ddsrt_thread_create(&thr2, "writer", &attr, &write_func, p);

  if (ret1 == DDS_RETCODE_OK)
  {
    ret1 = ddsrt_thread_join(thr1, &res1);
    CU_ASSERT_EQUAL(ret1, DDS_RETCODE_OK);
  }

  if (ret2 == DDS_RETCODE_OK)
  {
    ret2 = ddsrt_thread_join(thr2, &res2);
    CU_ASSERT_EQUAL(ret2, DDS_RETCODE_OK);
  }

  /*check for triggered event on p*/
  ddsrt_event_t* evtout = ddsrt_event_queue_next(q);
  CU_ASSERT_PTR_EQUAL_FATAL(evtout, evt);
  if (NULL != evtout)
    CU_ASSERT_EQUAL(ddsrt_atomic_ld32(&evtout->triggered), DDSRT_EVENT_FLAG_READ);

  /*read data from p*/
  CU_ASSERT_EQUAL_FATAL(DDS_RETCODE_OK, ddsrt_pipe_pull(p));
}

CU_Test(ddsrt_event, queue_wait)
{
  ddsrt_event_queue_t* q = ddsrt_event_queue_create();

  /*create pipe p*/
  ddsrt_socket_t p[2];
  CU_ASSERT_EQUAL_FATAL(DDS_RETCODE_OK, ddsrt_pipe_create(p));

  /*create event for p*/
  ddsrt_event_t evt;
  CU_ASSERT_EQUAL(DDS_RETCODE_OK, ddsrt_event_socket_init(&evt, p[0], DDSRT_EVENT_FLAG_READ));
  CU_ASSERT_EQUAL(DDS_RETCODE_OK, ddsrt_event_queue_add(q, &evt));

  /*single pipe generating events*/

  /*check whether writing to p generates the correct event*/
  test_write(q, p, &evt);

  /*create pipe p2*/
  ddsrt_socket_t p2[2];
  CU_ASSERT_EQUAL_FATAL(DDS_RETCODE_OK, ddsrt_pipe_create(p2));

  /*create event for p2*/
  ddsrt_event_t evt2;
  CU_ASSERT_EQUAL(DDS_RETCODE_OK, ddsrt_event_socket_init(&evt2, p2[0], DDSRT_EVENT_FLAG_READ));
  CU_ASSERT_EQUAL(DDS_RETCODE_OK, ddsrt_event_queue_add(q, &evt2));

  /*two pipes generating events*/
  test_write(q, p2, &evt2);
  test_write(q, p, &evt);

  /*remove one pipe from the queue, check that this no longer generates events,
  * but that the remaining one still does*/
  CU_ASSERT_EQUAL(DDS_RETCODE_OK, ddsrt_event_queue_remove(q,&evt));
  test_write(q, p, NULL);
  test_write(q, p2, &evt2);

  CU_ASSERT_EQUAL(DDS_RETCODE_OK, ddsrt_pipe_destroy(p));
  CU_ASSERT_EQUAL(DDS_RETCODE_OK, ddsrt_pipe_destroy(p2));
  CU_ASSERT_EQUAL(DDS_RETCODE_OK, ddsrt_event_queue_delete(q));
  CU_PASS("queue_wait");
}

CU_Test(ddsrt_event, queue_signal)
{
  ddsrt_event_queue_t* q = ddsrt_event_queue_create();

  /*create pipe p*/
  ddsrt_socket_t p[2];
  CU_ASSERT_EQUAL_FATAL(DDS_RETCODE_OK, ddsrt_pipe_create(p));

  /*create event for p*/
  ddsrt_event_t evt;
  CU_ASSERT_EQUAL(DDS_RETCODE_OK, ddsrt_event_socket_init(&evt, p[0], DDSRT_EVENT_FLAG_READ));
  CU_ASSERT_EQUAL(DDS_RETCODE_OK, ddsrt_event_queue_add(q, &evt));

  ddsrt_thread_t thr1, thr2, thr3;
  uint32_t res1 = 0, res2 = 0, res3 = 0;

  ddsrt_threadattr_t attr;
  ddsrt_threadattr_init(&attr);
  attr.schedClass = DDSRT_SCHED_DEFAULT;
  attr.schedPriority = 0;

  dds_return_t ret1 = ddsrt_thread_create(&thr1, "reader", &attr, &wait_func, q);
  dds_return_t ret2 = ddsrt_thread_create(&thr2, "writer", &attr, &write_func, p);
  dds_return_t ret3 = ddsrt_thread_create(&thr3, "interrupter", &attr, &interrupt_func, q);

  if (ret1 == DDS_RETCODE_OK)
  {
    ret1 = ddsrt_thread_join(thr1, &res1);
    CU_ASSERT_EQUAL(ret1, DDS_RETCODE_OK);
  }

  if (ret2 == DDS_RETCODE_OK)
  {
    ret2 = ddsrt_thread_join(thr2, &res2);
    CU_ASSERT_EQUAL(ret2, DDS_RETCODE_OK);
  }

  if (ret3 == DDS_RETCODE_OK)
  {
    ret3 = ddsrt_thread_join(thr3, &res3);
    CU_ASSERT_EQUAL(ret3, DDS_RETCODE_OK);
  }

  /*check for triggered event on socket*/
  ddsrt_event_t* evtout = ddsrt_event_queue_next(q);
  CU_ASSERT_PTR_EQUAL_FATAL(evtout, NULL);

  /*read data from p*/
  CU_ASSERT_EQUAL_FATAL(DDS_RETCODE_OK, ddsrt_pipe_pull(p));

  CU_ASSERT_EQUAL(DDS_RETCODE_OK, ddsrt_pipe_destroy(p));
  CU_ASSERT_EQUAL(DDS_RETCODE_OK, ddsrt_event_queue_delete(q));
  CU_PASS("queue_wait");
}
