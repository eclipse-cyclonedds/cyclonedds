// Copyright(c) 2006 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include "sockets_priv.h"
#include "dds/ddsrt/cdtors.h"
#include "dds/ddsrt/threads.h"
#include "CUnit/Theory.h"

CU_Init(ddsrt_select)
{
  ddsrt_init();
  return 0;
}

CU_Clean(ddsrt_select)
{
  ddsrt_fini();
  return 0;
}

static struct timeval tv_init = { .tv_sec = -2, .tv_usec = -2 };

#define CU_ASSERT_TIMEVAL_EQUAL(tv, secs, usecs) \
  CU_ASSERT((tv.tv_sec == secs) && (tv.tv_usec == usecs))

/* Simple test to validate that duration to timeval conversion is correct. */
CU_Test(ddsrt_select, duration_to_timeval)
{
  struct timeval tv, *tvptr;
  dds_duration_t nsecs_max;
  dds_duration_t usecs_max = 999999;
  dds_duration_t secs_max;
  DDSRT_STATIC_ASSERT (CHAR_BIT * sizeof (ddsrt_tv_sec_t) == 32 || CHAR_BIT * sizeof (ddsrt_tv_sec_t) == 64);
  DDSRT_WARNING_MSVC_OFF(6326)
  if (CHAR_BIT * sizeof (ddsrt_tv_sec_t) == 32)
    secs_max = INT32_MAX;
  else
    secs_max = INT64_MAX;
  DDSRT_WARNING_MSVC_ON(6326)

  if (DDS_INFINITY > secs_max) {
    CU_ASSERT_EQUAL_FATAL(secs_max, INT32_MAX);
    nsecs_max = DDS_INFINITY * secs_max;
  } else {
    CU_ASSERT_EQUAL_FATAL(secs_max, DDS_INFINITY);
    nsecs_max = DDS_INFINITY / DDS_NSECS_IN_SEC;
  }

  tv = tv_init;
  tvptr = ddsrt_duration_to_timeval_ceil(INT64_MIN, &tv);
  CU_ASSERT_PTR_EQUAL(tvptr, &tv);
  CU_ASSERT_TIMEVAL_EQUAL(tv, 0, 0);

  tv = tv_init;
  tvptr = ddsrt_duration_to_timeval_ceil(INT64_MIN + 1, &tv);
  CU_ASSERT_PTR_EQUAL(tvptr, &tv);
  CU_ASSERT_TIMEVAL_EQUAL(tv, 0, 0);

  tv = tv_init;
  tvptr = ddsrt_duration_to_timeval_ceil(-2, &tv);
  CU_ASSERT_PTR_EQUAL(tvptr, &tv);
  CU_ASSERT_TIMEVAL_EQUAL(tv, 0, 0);

  tv = tv_init;
  tvptr = ddsrt_duration_to_timeval_ceil(-1, &tv);
  CU_ASSERT_PTR_EQUAL(tvptr, &tv);
  CU_ASSERT_TIMEVAL_EQUAL(tv, 0, 0);

  tv = tv_init;
  tvptr = ddsrt_duration_to_timeval_ceil(0, &tv);
  CU_ASSERT_PTR_EQUAL(tvptr, &tv);
  CU_ASSERT_TIMEVAL_EQUAL(tv, 0, 0);

  tv = tv_init;
  tvptr = ddsrt_duration_to_timeval_ceil(nsecs_max - 1, &tv);
  CU_ASSERT_PTR_EQUAL(tvptr, &tv);
  CU_ASSERT_TIMEVAL_EQUAL(tv, secs_max, usecs_max);

  tv = tv_init;
  tvptr = ddsrt_duration_to_timeval_ceil(nsecs_max, &tv);
  CU_ASSERT_PTR_EQUAL(tvptr, &tv);
  CU_ASSERT_TIMEVAL_EQUAL(tv, secs_max, usecs_max);

  tv = tv_init;
  tvptr = ddsrt_duration_to_timeval_ceil(nsecs_max + 1, &tv);
  CU_ASSERT_PTR_EQUAL(tvptr, &tv);
  CU_ASSERT_TIMEVAL_EQUAL(tv, secs_max, usecs_max);

  tv = tv_init;
  tvptr = ddsrt_duration_to_timeval_ceil(DDS_INFINITY - 1, &tv);
  CU_ASSERT_PTR_EQUAL(tvptr, &tv);
  CU_ASSERT_TIMEVAL_EQUAL(tv, secs_max, usecs_max);

  tv = tv_init;
  tvptr = ddsrt_duration_to_timeval_ceil(DDS_INFINITY, &tv);
  CU_ASSERT_PTR_EQUAL(tvptr, NULL);
  CU_ASSERT_TIMEVAL_EQUAL(tv, 0, 0);
}

typedef struct {
  dds_duration_t delay;
  dds_duration_t skew;
  ddsrt_socket_t sock;
} thread_arg_t;

static void
sockets_pipe(ddsrt_socket_t socks[2])
{
  dds_return_t rc;
  ddsrt_socket_t sock;

  socklen_t addrlen;
  struct sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = 0;

  fprintf (stderr, "sockets_pipe ... begin\n");
  CU_ASSERT_PTR_NOT_NULL_FATAL(socks);
  rc = ddsrt_socket(&sock, AF_INET, SOCK_STREAM, 0);
  CU_ASSERT_EQUAL_FATAL(rc, DDS_RETCODE_OK);
  rc = ddsrt_socket(&socks[1], AF_INET, SOCK_STREAM, 0);
  CU_ASSERT_EQUAL_FATAL(rc, DDS_RETCODE_OK);
  rc = ddsrt_bind(sock, (struct sockaddr *)&addr, sizeof(addr));
  CU_ASSERT_EQUAL_FATAL(rc, DDS_RETCODE_OK);
  addrlen = (socklen_t) sizeof(addr);
  rc = ddsrt_getsockname(sock, (struct sockaddr *)&addr, &addrlen);
  CU_ASSERT_EQUAL_FATAL(rc, DDS_RETCODE_OK);
  fprintf (stderr, "sockets_pipe ... listen\n");
  rc = ddsrt_listen(sock, 1);
  CU_ASSERT_EQUAL_FATAL(rc, DDS_RETCODE_OK);
  fprintf (stderr, "sockets_pipe ... connect\n");
  rc = ddsrt_connect(socks[1], (struct sockaddr *)&addr, sizeof(addr));
  CU_ASSERT_EQUAL_FATAL(rc, DDS_RETCODE_OK);
  fprintf (stderr, "sockets_pipe ... accept\n");
  rc = ddsrt_accept(sock, NULL, NULL, &socks[0]);
  CU_ASSERT_EQUAL_FATAL(rc, DDS_RETCODE_OK);
  ddsrt_close(sock);
  fprintf (stderr, "sockets_pipe ... done\n");
}

static const char mesg[] = "foobar";

static uint32_t select_timeout_routine(void *ptr)
{
  dds_return_t rc;
  dds_time_t before, after;
  dds_duration_t delay;
  fd_set rdset;
  thread_arg_t *arg = (thread_arg_t *)ptr;
  uint32_t res = 0;

  FD_ZERO(&rdset);
#if LWIP_SOCKET
  DDSRT_WARNING_GNUC_OFF(sign-conversion)
#endif
  FD_SET(arg->sock, &rdset);
#if LWIP_SOCKET
  DDSRT_WARNING_GNUC_ON(sign-conversion)
#endif

  before = dds_time();
  rc = ddsrt_select(arg->sock + 1, &rdset, NULL, NULL, arg->delay);
  after = dds_time();
  delay = after - before;

  fprintf(stderr, "Waited for %"PRId64" (nanoseconds)\n", delay);
  fprintf(stderr, "Expected to wait %"PRId64" (nanoseconds)\n", arg->delay);
  fprintf(stderr, "ddsrt_select returned %"PRId32"\n", rc);

  if (rc == DDS_RETCODE_TIMEOUT)
    res = ((after - delay) >= (arg->delay - arg->skew));

  return res;
}

CU_Test(ddsrt_select, timeout)
{
  dds_return_t rc;
  ddsrt_socket_t socks[2];
  ddsrt_thread_t thr;
  ddsrt_threadattr_t attr;
  thread_arg_t arg;
  uint32_t res = 0;

  sockets_pipe(socks);

  arg.delay = DDS_MSECS(2000);
  /* Allow the delay to be off by x microseconds (arbitrarily chosen) for
     systems with a really poor clock. This test is just to get some
     confidence that time calculation is not completely broken, it is by
     no means proof that time calculation is entirely correct! */
  arg.skew = DDS_MSECS(1000);
  arg.sock = socks[0];

  fprintf (stderr, "create thread\n");
  ddsrt_threadattr_init(&attr);
  rc = ddsrt_thread_create(&thr, "select_timeout", &attr, &select_timeout_routine, &arg);
  CU_ASSERT_EQUAL_FATAL(rc, DDS_RETCODE_OK);
  /* Allow the thread some time to get ready. */
  dds_sleepfor(arg.delay * 2);
  /* Send data to the read socket to avoid blocking indefinitely. */
  fprintf (stderr, "write data\n");
  ssize_t sent = 0;
  rc = ddsrt_send(socks[1], mesg, sizeof(mesg), 0, &sent);
  CU_ASSERT_EQUAL_FATAL(rc, DDS_RETCODE_OK);
  fprintf (stderr, "join thread\n");
  rc = ddsrt_thread_join(thr, &res);
  CU_ASSERT_EQUAL_FATAL(rc, DDS_RETCODE_OK);
  CU_ASSERT_EQUAL(res, 1);

  (void)ddsrt_close(socks[0]);
  (void)ddsrt_close(socks[1]);
}

static uint32_t recv_routine(void *ptr)
{
  thread_arg_t *arg = (thread_arg_t*)ptr;

  fd_set rdset;
  ssize_t rcvd = -1;
  char buf[sizeof(mesg)];

  FD_ZERO(&rdset);
#if LWIP_SOCKET
  DDSRT_WARNING_GNUC_OFF(sign-conversion)
#endif
  FD_SET(arg->sock, &rdset);
#if LWIP_SOCKET
  DDSRT_WARNING_GNUC_ON(sign-conversion)
#endif

  (void)ddsrt_select(arg->sock + 1, &rdset, NULL, NULL, arg->delay);

  if (ddsrt_recv(arg->sock, buf, sizeof(buf), 0, &rcvd) == DDS_RETCODE_OK) {
    return (rcvd == sizeof(mesg) && memcmp(buf, mesg, sizeof(mesg)) == 0);
  }

  return 0;
}

CU_Test(ddsrt_select, send_recv)
{
  dds_return_t rc;
  ddsrt_socket_t socks[2];
  ddsrt_thread_t thr;
  ddsrt_threadattr_t attr;
  thread_arg_t arg;
  uint32_t res = 0;

  sockets_pipe(socks);

  arg.delay = DDS_SECS(1);
  arg.skew = 0;
  arg.sock = socks[0];

  ddsrt_threadattr_init(&attr);
  rc = ddsrt_thread_create(&thr, "recv", &attr, &recv_routine, &arg);
  CU_ASSERT_EQUAL_FATAL(rc, DDS_RETCODE_OK);

  ssize_t sent = 0;
  rc = ddsrt_send(socks[1], mesg, sizeof(mesg), 0, &sent);
  CU_ASSERT_EQUAL(rc, DDS_RETCODE_OK);
  CU_ASSERT_EQUAL(sent, sizeof(mesg));

  rc = ddsrt_thread_join(thr, &res);
  CU_ASSERT_EQUAL(rc, DDS_RETCODE_OK);
  CU_ASSERT_EQUAL(res, 1);

  (void)ddsrt_close(socks[0]);
  (void)ddsrt_close(socks[1]);
}

static uint32_t recvmsg_routine(void *ptr)
{
  thread_arg_t *arg = (thread_arg_t*)ptr;

  fd_set rdset;
  ssize_t rcvd = -1;
  char buf[sizeof(mesg)];
  ddsrt_msghdr_t msg;
  ddsrt_iovec_t iov;
  memset(&msg, 0, sizeof(msg));

  iov.iov_base = buf;
  iov.iov_len = sizeof(buf);
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;

  FD_ZERO(&rdset);
#if LWIP_SOCKET
  DDSRT_WARNING_GNUC_OFF(sign-conversion)
#endif
  FD_SET(arg->sock, &rdset);
#if LWIP_SOCKET
  DDSRT_WARNING_GNUC_ON(sign-conversion)
#endif

  (void)ddsrt_select(arg->sock + 1, &rdset, NULL, NULL, arg->delay);

  if (ddsrt_recvmsg(arg->sock, &msg, 0, &rcvd) == DDS_RETCODE_OK) {
    return (rcvd == sizeof(mesg) && memcmp(buf, mesg, sizeof(mesg)) == 0);
  }

  return 0;
}

CU_Test(ddsrt_select, sendmsg_recvmsg)
{
  dds_return_t rc;
  ddsrt_socket_t socks[2];
  ddsrt_thread_t thr;
  ddsrt_threadattr_t attr;
  thread_arg_t arg;
  uint32_t res = 0;

  sockets_pipe(socks);

  memset(&arg, 0, sizeof(arg));
  arg.sock = socks[0];

  ddsrt_threadattr_init(&attr);
  rc = ddsrt_thread_create(&thr, "recvmsg", &attr, &recvmsg_routine, &arg);
  CU_ASSERT_EQUAL_FATAL(rc, DDS_RETCODE_OK);

  ssize_t sent = 0;
  ddsrt_msghdr_t msg;
  ddsrt_iovec_t iov;
  memset(&msg, 0, sizeof(msg));
  iov.iov_base = (void*)mesg;
  iov.iov_len = (ddsrt_iov_len_t)sizeof(mesg);
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;

  rc = ddsrt_sendmsg(socks[1], &msg, 0, &sent);
  CU_ASSERT_EQUAL(rc, DDS_RETCODE_OK);
  CU_ASSERT_EQUAL(sent, sizeof(mesg));

  rc = ddsrt_thread_join(thr, &res);
  CU_ASSERT_EQUAL_FATAL(rc, DDS_RETCODE_OK);
  CU_ASSERT_EQUAL(res, 1);

  (void)ddsrt_close(socks[0]);
  (void)ddsrt_close(socks[1]);
}
