// Copyright(c) 2006 to 2019 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDSI__SOCKWAITSET_H
#define DDSI__SOCKWAITSET_H

#include "dds/ddsi/ddsi_sockwaitset.h"

#if defined (__cplusplus)
extern "C" {
#endif

struct ddsi_tran_conn;

/**
 * @brief Allocates a new connection waitset.
 * @component socket_waitset
 *
 * The waitset is thread-safe in that multiple threads may add and remove
 * connections from the wait set or trigger it. However only a single thread
 * may process events from the wait set using the Wait and NextEvent functions
 * in a single handling loop.
 *
 * @return struct ddsi_sock_waitset*
 */
struct ddsi_sock_waitset * ddsi_sock_waitset_new (void);

/**
 * @brief Frees the waitset
 * @component socket_waitset
 *
 * Any connections associated with it will be closed.
 *
 * @param ws    The socket waitset
 */
void ddsi_sock_waitset_free (struct ddsi_sock_waitset * ws);

/**
 * @brief Triggers the waitset
 * @component socket_waitset
 *
 * Triggers the waitset, from any thread. It is level triggered, when
 * called while no thread is waiting in ddsi_sock_waitset_wait the
 * trigger will cause an (early) wakeup on the next call to
 * ddsi_sock_waitset_wait. Returns DDS_RETCODE_OK if successfully
 * triggered, DDS_RETCODE_BAD_PARAMETER if an error occurs.
 *
 * Triggering a waitset may require resources and they may be counted.
 * Do not trigger a waitset arbitrarily often without ensuring
 * ddsi_sock_waitset_wait is called often enough to let it release any
 * resources used.
 *
 * Shared state updates preceding struct ddsi_sock_waitset *rigger are
 * visible following ddsi_sock_waitset_wait.
 *
 * @param ws    The socket waitset
 */
void ddsi_sock_waitset_trigger (struct ddsi_sock_waitset * ws);

/**
 * @component socket_waitset
 *
 * A connection may be associated with only one waitset at any time, and
 * may be added to the waitset only once.  Failure to comply with this
 * restriction results in undefined behaviour.
 *
 * Closing a connection associated with a waitset is handled gracefully:
 * no operations will signal errors because of it.
 *
 * @param ws    The socket waitset
 * @param conn  Connection
 * @returns Returns < 0 on error, 0 if already present, 1 if added
 */
int ddsi_sock_waitset_add (struct ddsi_sock_waitset * ws, struct ddsi_tran_conn * conn);

/**
 * @brief Drops all connections from the waitset from index onwards.
 * @component socket_waitset
 *
 * Index 0 corresponds to the first connection added to the waitset, index 1 to the
 * second, etc. Behaviour is undefined when called after a successful wait but before
 * all events had been enumerated.
 *
 * @param ws        The socket waitset
 * @param index     Index of first connection to be dropped
 */
void ddsi_sock_waitset_purge (struct ddsi_sock_waitset * ws, unsigned index);

/**
 * @brief Waits until some of the connections in WS have data to be read.
 * @component socket_waitset
 *
 * Returns a new wait set context if one or more connections have data to read.
 * However, the return may be spurious (NULL) (i.e., no events)
 *
 * If a context is returned it must be enumerated before ddsi_sock_waitset_wait
 * may be called again.
 *
 * Shared state updates preceding struct ddsi_sock_waitset *rigger are visible
 * following ddsi_sock_waitset_wait.
 *
 * @param ws The socket waitset
 * @return struct ddsi_sock_waitset_ctx*
 */
struct ddsi_sock_waitset_ctx * ddsi_sock_waitset_wait (struct ddsi_sock_waitset * ws);

/**
 * @component socket_waitset
 *
 * Returns the index of the next triggered connection in the waitset contect
 * ctx, or -1 if the set of available events has been exhausted. Index 0 is
 * the first connection added to the waitset, index 1 the second, &c.
 *
 * Following a call to ddsi_sock_waitset_wait on waitset that returned a context,
 * one MUST enumerate all available events before ddsi_sock_waitset_wait may be
 * called again.
 *
 * If the return value is >= 0, *conn contains the connection on which data is
 * available.
 *
 * @param ctx   Socket waitset context
 * @param conn  Connection
 * @return int
 */
int ddsi_sock_waitset_next_event (struct ddsi_sock_waitset_ctx * ctx, struct ddsi_tran_conn ** conn);

/**
 * @brief Remove connection
 * @component socket_waitset
 *
 * @param ws    The socket waitset
 * @param conn  Connection
 */
void ddsi_sock_waitset_remove (struct ddsi_sock_waitset * ws, struct ddsi_tran_conn * conn);

#if defined (__cplusplus)
}
#endif
#endif /* DDSI__SOCKWAITSET_H */
