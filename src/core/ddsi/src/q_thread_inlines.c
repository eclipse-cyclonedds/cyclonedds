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
#include "dds/ddsi/q_thread.h"

extern inline int vtime_awake_p (vtime_t vtime);
extern inline int vtime_asleep_p (vtime_t vtime);
extern inline int vtime_gt (vtime_t vtime1, vtime_t vtime0);

extern inline void thread_state_asleep (struct thread_state1 *ts1);
extern inline void thread_state_awake (struct thread_state1 *ts1);
extern inline void thread_state_blocked (struct thread_state1 *ts1);
extern inline void thread_state_unblocked (struct thread_state1 *ts1);
