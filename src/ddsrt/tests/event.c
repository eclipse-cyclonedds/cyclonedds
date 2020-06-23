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

#include <stdio.h>
#include "CUnit/Test.h"

#include "dds/ddsrt/cdtors.h"
#include "dds/ddsrt/event.h"
#include "dds/ddsrt/sockets.h"
#include "dds/ddsrt/threads.h"

long int make_pipe(ddsrt_socket_t tomake[2]);
void close_pipe(ddsrt_socket_t toclose[2]);
long int push_pipe(ddsrt_socket_t* p);
long int pull_pipe(ddsrt_socket_t* p);
void ddsrt_sleep(int microsecs);

#if defined(_WIN32)
#include <winsock2.h>
#include <windows.h>

long int make_pipe(ddsrt_socket_t tomake[2]) {
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
    closesocket(listener);
    closesocket(s1);
    closesocket(s2);
    return -1;
  }
  closesocket(listener);
  SetHandleInformation((HANDLE)s1, HANDLE_FLAG_INHERIT, 0);
  SetHandleInformation((HANDLE)s2, HANDLE_FLAG_INHERIT, 0);
  tomake[0] = s1;
  tomake[1] = s2;
  return 0;
}

void close_pipe(ddsrt_socket_t toclose[2]) {
  closesocket(toclose[0]);
  closesocket(toclose[1]);
}

long int push_pipe(ddsrt_socket_t p[2]) {
  char dummy = 0x0;
  return send(p[1], &dummy, sizeof(dummy), 0);
}

long int pull_pipe(ddsrt_socket_t p[2]) {
  char buf = 0x0;
  return recv(p[0], &buf, sizeof(buf), 0);
}

void ddsrt_sleep(int microsecs) {
  Sleep(microsecs / 1000);
}
#else
#include <sys/select.h>
#include <unistd.h>

long int make_pipe(ddsrt_socket_t tomake[2]) {
  return pipe(tomake);
}

void close_pipe(ddsrt_socket_t toclose[2]) {
  close(toclose[0]);
  close(toclose[1]);
}

long int push_pipe(ddsrt_socket_t p[2]) {
  char dummy = 0x0;
  return write(p[1], &dummy, sizeof(dummy));
}

long int pull_pipe(ddsrt_socket_t p[2]) {
  char buf = 0x0;
  return read(p[0], &buf, sizeof(buf));
}

void ddsrt_sleep(int microsecs) {
  usleep(microsecs);
}
#endif


CU_Init(ddsrt_event) {
  ddsrt_init();
  return 0;
}

CU_Clean(ddsrt_event) {
  ddsrt_fini();
  return 0;
}

CU_Test(ddsrt_event, evt_create) {
  int fd = 123456;
  ddsrt_event_t evt1 = ddsrt_event_create(ddsrt_monitorable_unset, (void*)0x0, sizeof(long int), ddsrt_monitorable_event_unset),
    evt2 = ddsrt_event_create(ddsrt_monitorable_file, &fd, sizeof(fd), ddsrt_monitorable_event_connect);

  CU_ASSERT_EQUAL_FATAL(evt1.mon_type, ddsrt_monitorable_unset);
  CU_ASSERT_EQUAL_FATAL(evt1.mon_sz, sizeof(long int));
  CU_ASSERT_EQUAL_FATAL(evt1.evt_type, ddsrt_monitorable_event_unset);
  long int t = 0x0;
  CU_ASSERT_EQUAL_FATAL(*(long int*)evt1.mon_bytes, t);

  CU_ASSERT_EQUAL_FATAL(evt2.mon_type, ddsrt_monitorable_file);
  CU_ASSERT_EQUAL_FATAL(evt2.mon_sz, sizeof(fd));
  CU_ASSERT_EQUAL_FATAL(evt2.evt_type, ddsrt_monitorable_event_connect);
  CU_ASSERT_EQUAL_FATAL(*(int*)evt2.mon_bytes, fd);

  CU_PASS("evt_create");
}

CU_Test(ddsrt_event, evt_implicit) {
  long long int fd1 = 123456;
  int fd2 = 654321;

  ddsrt_event_t evt1 = ddsrt_event_create_val(ddsrt_monitorable_pipe, fd1, ddsrt_monitorable_event_connect),
    evt2 = ddsrt_event_create_val(ddsrt_monitorable_socket, fd2, ddsrt_monitorable_event_disconnect);

  CU_ASSERT_EQUAL_FATAL(evt1.mon_type, ddsrt_monitorable_pipe);
  CU_ASSERT_EQUAL_FATAL(evt1.mon_sz, sizeof(long long int));
  CU_ASSERT_EQUAL_FATAL(evt1.evt_type, ddsrt_monitorable_event_connect);
  CU_ASSERT_EQUAL_FATAL(*(long int*)evt1.mon_bytes, fd1);


  CU_ASSERT_EQUAL_FATAL(evt2.mon_type, ddsrt_monitorable_socket);
  CU_ASSERT_EQUAL_FATAL(evt2.mon_sz, sizeof(int));
  CU_ASSERT_EQUAL_FATAL(evt2.evt_type, ddsrt_monitorable_event_disconnect);
  CU_ASSERT_EQUAL_FATAL(*(int*)evt2.mon_bytes, fd2);

  CU_PASS("evt_implicit");
}

CU_Test(ddsrt_event, monitor_register) {
  const int startsize = 8;
  ddsrt_monitor_t* mon_nonfixedsize = ddsrt_monitor_create(startsize, 0), /*monitor of non-fixed size*/
    * mon_fixedsize = ddsrt_monitor_create(startsize, 1); /*monitor of fixed size*/
  for (int i = 0; i < startsize + 1; i++) {
    ddsrt_event_t evt = ddsrt_event_create_val(ddsrt_monitorable_pipe, i, ddsrt_monitorable_event_connect);

    /*writing triggers to monitorables*/
    int nfx = ddsrt_monitor_register_trigger(mon_nonfixedsize, evt);
    CU_ASSERT(nfx == i + 1);

    int fx = ddsrt_monitor_register_trigger(mon_fixedsize, evt);
    if (i < startsize) {
      CU_ASSERT(fx == i + 1);
    }
    else {
      CU_ASSERT(fx == -1);
    }

    /*adding to existing monitorables*/
    evt.evt_type = ddsrt_monitorable_event_disconnect;

    nfx = ddsrt_monitor_register_trigger(mon_nonfixedsize, evt);
    CU_ASSERT(nfx == i + 1);

    fx = ddsrt_monitor_register_trigger(mon_fixedsize, evt);
    if (i < startsize) {
      CU_ASSERT(fx == i + 1);
    }
    else {
      CU_ASSERT(fx == -1);
    }
  }

  for (int i = 0; i < startsize + 1; i++) {
    ddsrt_event_t evt = ddsrt_event_create_val(ddsrt_monitorable_pipe, i, ddsrt_monitorable_event_disconnect | ddsrt_monitorable_event_connect);

    /*removing from monitorables*/
    int nfx = ddsrt_monitor_deregister_trigger(mon_nonfixedsize, evt);
    CU_ASSERT(nfx == startsize - i);

    int fx = ddsrt_monitor_deregister_trigger(mon_fixedsize, evt);
    if (i < startsize) {
      CU_ASSERT(fx == startsize - i - 1);
    }
    else {
      CU_ASSERT(fx == 0);
    }
  }

  ddsrt_monitor_destroy(mon_nonfixedsize);
  ddsrt_monitor_destroy(mon_fixedsize);

  CU_PASS("monitor_register");
}

static uint32_t wait_func(void* arg) {
  printf("starting wait for event\n");
  ddsrt_monitor_start_wait((ddsrt_monitor_t*)arg, 6000);
  printf("done with wait for event\n");
  return 0;
}

static uint32_t write_func(void* arg) {
  ddsrt_socket_t* p = (ddsrt_socket_t*)arg;
  printf("starting wait for send to %d\n", (int)p[1]);
  ddsrt_sleep(250000);
  printf("sending to %d\n", (int)p[1]);
  push_pipe(p);
  printf("done with send\n");
  return 0;
}

static uint32_t interrupt_func(void* arg) {
  printf("starting wait for interrupt\n");
  ddsrt_sleep(125000);
  printf("interrupting\n");
  ddsrt_monitor_interrupt_wait((ddsrt_monitor_t*)arg);
  printf("done with interrupt\n");
  return 0;
}

CU_Test(ddsrt_event, monitor_trigger) {
  ddsrt_socket_t p[2];
  CU_ASSERT_EQUAL_FATAL(make_pipe(p), 0);

  ddsrt_monitor_t* mon = ddsrt_monitor_create(128, 0);

  ddsrt_event_t evtin = ddsrt_event_create_val(ddsrt_monitorable_socket, p[0], ddsrt_monitorable_event_data_in);
  ddsrt_monitor_register_trigger(mon, evtin);

  dds_return_t ret1 = DDS_RETCODE_OK, ret2 = DDS_RETCODE_OK;
  ddsrt_thread_t thr1, thr2;
  ddsrt_threadattr_t attr;
  uint32_t res1 = 0, res2 = 0;

  ddsrt_threadattr_init(&attr);
  attr.schedClass = DDSRT_SCHED_DEFAULT;
  attr.schedPriority = 0;

  ret1 = ddsrt_thread_create(&thr1, "reader", &attr, &wait_func, mon);
  ret2 = ddsrt_thread_create(&thr2, "writer", &attr, &write_func, p);

  if (ret1 == DDS_RETCODE_OK) {
    ret1 = ddsrt_thread_join(thr1, &res1);
    CU_ASSERT_EQUAL(ret1, DDS_RETCODE_OK);
  }

  if (ret2 == DDS_RETCODE_OK) {
    ret2 = ddsrt_thread_join(thr2, &res2);
    CU_ASSERT_EQUAL(ret2, DDS_RETCODE_OK);
  }

  /*check for for data_in event*/
  ddsrt_event_t* evtout = ddsrt_monitor_pop_event(mon);
  CU_ASSERT_PTR_NOT_EQUAL_FATAL(evtout, NULL);
  CU_ASSERT_EQUAL(evtout->mon_type, evtin.mon_type);
  CU_ASSERT_EQUAL(evtout->mon_sz, evtin.mon_sz);
  CU_ASSERT(0 == memcmp(evtout->mon_bytes, evtin.mon_bytes, evtin.mon_sz));
  CU_ASSERT_EQUAL(evtout->evt_type, evtin.evt_type);

  evtout = ddsrt_monitor_pop_event(mon);
  CU_ASSERT_EQUAL_FATAL(evtout, NULL);

  ddsrt_monitor_destroy(mon);
  close_pipe(p);

  CU_PASS("monitor_trigger");
}

CU_Test(ddsrt_event, monitor_interrupt) {
  ddsrt_socket_t p[2];
  CU_ASSERT_EQUAL_FATAL(make_pipe(p), 0);

  ddsrt_monitor_t* mon = ddsrt_monitor_create(128, 0);

  ddsrt_monitor_register_trigger(mon, ddsrt_event_create_val(ddsrt_monitorable_socket, p[0], ddsrt_monitorable_event_data_in));

  dds_return_t ret1 = DDS_RETCODE_OK, ret2 = DDS_RETCODE_OK, ret3 = DDS_RETCODE_OK;
  ddsrt_thread_t thr1, thr2, thr3;
  ddsrt_threadattr_t attr;
  uint32_t res1 = 0, res2 = 0, res3 = 0;

  ddsrt_threadattr_init(&attr);
  attr.schedClass = DDSRT_SCHED_DEFAULT;
  attr.schedPriority = 0;

  ret1 = ddsrt_thread_create(&thr1, "reader", &attr, &wait_func, mon);
  ret2 = ddsrt_thread_create(&thr2, "writer", &attr, &write_func, p);
  ret3 = ddsrt_thread_create(&thr3, "interrupter", &attr, &interrupt_func, mon);

  if (ret1 == DDS_RETCODE_OK) {
    ret1 = ddsrt_thread_join(thr1, &res1);
    CU_ASSERT_EQUAL(ret1, DDS_RETCODE_OK);
  }

  if (ret2 == DDS_RETCODE_OK) {
    ret2 = ddsrt_thread_join(thr2, &res2);
    CU_ASSERT_EQUAL(ret2, DDS_RETCODE_OK);
  }

  if (ret3 == DDS_RETCODE_OK) {
    ret3 = ddsrt_thread_join(thr3, &res3);
    CU_ASSERT_EQUAL(ret3, DDS_RETCODE_OK);
  }

  /*check for for data_in event*/
  ddsrt_event_t* evtout = ddsrt_monitor_pop_event(mon);
  CU_ASSERT_EQUAL_FATAL(evtout, NULL);

  ddsrt_monitor_destroy(mon);
  close_pipe(p);

  CU_PASS("monitor_interrupt");
}