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
/* -*- c -*- */

#include "ddsi/sysdeps.h"
#include "os/os_atomics.h"
#include "ddsi/q_static_assert.h"

#if defined SUPPRESS_THREAD_INLINES && defined NN_C99_INLINE
#undef NN_C99_INLINE
#define NN_C99_INLINE
#endif

NN_C99_INLINE int vtime_awake_p (_In_ vtime_t vtime)
{
  return (vtime % 2) == 0;
}

NN_C99_INLINE int vtime_asleep_p (_In_ vtime_t vtime)
{
  return (vtime % 2) == 1;
}

NN_C99_INLINE int vtime_gt (_In_ vtime_t vtime1, _In_ vtime_t vtime0)
{
  Q_STATIC_ASSERT_CODE (sizeof (vtime_t) == sizeof (svtime_t));
  return (svtime_t) (vtime1 - vtime0) > 0;
}

NN_C99_INLINE void thread_state_asleep (_Inout_ struct thread_state1 *ts1)
{
  vtime_t vt = ts1->vtime;
  vtime_t wd = ts1->watchdog;
  if (vtime_awake_p (vt))
  {
    os_atomic_fence_rel ();
    ts1->vtime = vt + 1;
  }
  else
  {
    os_atomic_fence_rel ();
    ts1->vtime = vt + 2;
    os_atomic_fence_acq ();
  }

  if ( wd % 2 ){
    ts1->watchdog = wd + 2;
  } else {
    ts1->watchdog = wd + 1;
  }
 }

NN_C99_INLINE void thread_state_awake (_Inout_ struct thread_state1 *ts1)
{
  vtime_t vt = ts1->vtime;
  vtime_t wd = ts1->watchdog;
  if (vtime_asleep_p (vt))
    ts1->vtime = vt + 1;
  else
  {
    os_atomic_fence_rel ();
    ts1->vtime = vt + 2;
  }
  os_atomic_fence_acq ();

  if ( wd % 2 ){
    ts1->watchdog = wd + 1;
  } else {
    ts1->watchdog = wd + 2;
  }

}

NN_C99_INLINE void thread_state_blocked (_Inout_ struct thread_state1 *ts1)
{
  vtime_t wd = ts1->watchdog;
  if ( wd % 2 ){
    ts1->watchdog = wd + 2;
  } else {
    ts1->watchdog = wd + 1;
  }
}

NN_C99_INLINE void thread_state_unblocked (_Inout_ struct thread_state1 *ts1)
{
  vtime_t wd = ts1->watchdog;
  if ( wd % 2 ){
    ts1->watchdog = wd + 1;
  } else {
    ts1->watchdog = wd + 2;
  }
}
