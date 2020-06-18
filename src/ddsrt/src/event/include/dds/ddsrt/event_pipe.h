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
#ifndef DDSRT_EVENT_EVENT_PIPE_H
#define DDSRT_EVENT_EVENT_PIPE_H

#include "dds/ddsrt/sockets.h"

#if defined (__cplusplus)
extern "C" {
#endif

/**
* Creates a pipe and stores the result in tomake.
*
* tomake: array for sockets describing the pipe
* 
* returns:
*		-1: something went wrong
*		0: success
*/
long int ddsrt_make_pipe(ddsrt_socket_t tomake[2]);

/**
* Closes a pipe.
*
* tomake: array for sockets describing the pipe
*/
void ddsrt_close_pipe(ddsrt_socket_t toclose[2]);

/**
* Pushes a null byte to a socket.
*
* p: the socket to push to
*
* returns:
*/
long int ddsrt_push_pipe(ddsrt_socket_t p);

/**
* Pulls one byte from a socket.
*
* p: the socket to pull from
*
* returns:
*/
long int ddsrt_pull_pipe(ddsrt_socket_t p);

#if defined (__cplusplus)
}
#endif

#endif /* DDSRT_EVENT_EVENT_PIPE_H */