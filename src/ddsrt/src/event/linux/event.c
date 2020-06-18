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
#include <sys/select.h>
#include <unistd.h>

/*posix implementation of ddsrt_make_pipe*/
long int ddsrt_make_pipe(ddsrt_socket_t tomake[2]) {
	return pipe(tomake);
}

/*posix implementation of ddsrt_close_pipe*/
void ddsrt_close_pipe(ddsrt_socket_t toclose[2]) {
	close(toclose[0]);
	close(toclose[1]);
}

/*posix implementation of ddsrt_push_pipe*/
long int ddsrt_push_pipe(ddsrt_socket_t p) {
	char dummy = 0x0;
	return write(p, &dummy, sizeof(dummy));
}

/*posix implementation of ddsrt_pull_pipe*/
long int ddsrt_pull_pipe(ddsrt_socket_t p) {
	char buf = 0x0;
	return read(p, &buf, sizeof(buf));
}