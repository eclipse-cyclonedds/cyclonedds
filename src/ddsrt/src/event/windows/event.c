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

#include "dds/ddsrt/event_pipe.h"
#include <winsock2.h>
#include <windows.h>

long int ddsrt_make_pipe(ddsrt_socket_t tomake[2]) {
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
  /* Equivalent to FD_CLOEXEC */
  SetHandleInformation((HANDLE)s1, HANDLE_FLAG_INHERIT, 0);
  SetHandleInformation((HANDLE)s2, HANDLE_FLAG_INHERIT, 0);
  tomake[0] = s1;
  tomake[1] = s2;
  return 0;
}

void ddsrt_close_pipe(ddsrt_socket_t toclose[2]) {
  closesocket(toclose[0]);
  closesocket(toclose[1]);
}

long int ddsrt_push_pipe(ddsrt_socket_t p[2]) {
  char dummy = 0x0;
  return send(p[1], &dummy, sizeof(dummy), 0);
}

long int ddsrt_pull_pipe(ddsrt_socket_t p[2]) {
  char buf = 0x0;
  return recv(p[0], &buf, sizeof(buf), 0);
}