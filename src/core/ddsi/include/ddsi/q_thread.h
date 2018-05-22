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
#ifndef Q_THREAD_H
#define Q_THREAD_H

#include "os/os.h"

#include "ddsi/q_inline.h"

#if defined (__cplusplus)
extern "C" {
#endif

/* Things don't go wrong if CACHE_LINE_SIZE is defined incorrectly,
   they just run slower because of false cache-line sharing. It can be
   discovered at run-time, but in practice it's 64 for most CPUs and
   128 for some. */
#define CACHE_LINE_SIZE 64

typedef uint32_t vtime_t;
typedef int32_t svtime_t; /* signed version */

/* GCC has a nifty feature allowing the specification of the required
   alignment: __attribute__ ((aligned (CACHE_LINE_SIZE))) in this
   case. Many other compilers implement it as well, but it is by no
   means a standard feature.  So we do it the old-fashioned way. */


/* These strings are used to indicate the required scheduling class to the "create_thread()" */
#define Q_THREAD_SCHEDCLASS_REALTIME  "Realtime"
#define Q_THREAD_SCHEDCLASS_TIMESHARE "Timeshare"

/* When this value is used, the platform default for scheduling priority will be used */
#define Q_THREAD_SCHEDPRIO_DEFAULT 0

enum thread_state {
  THREAD_STATE_ZERO,
  THREAD_STATE_ALIVE
};

struct logbuf;

/*
 * watchdog indicates progress for the service lease liveliness mechsanism, while vtime
 * indicates progress for the Garbage collection purposes.
 *  vtime even : thread awake
 *  vtime odd  : thread asleep
 */
#define THREAD_BASE                             \
  volatile vtime_t vtime;                       \
  volatile vtime_t watchdog;                    \
  os_threadId tid;                              \
  os_threadId extTid;                           \
  enum thread_state state;                      \
  struct logbuf *lb;                            \
  char *name /* note: no semicolon! */

struct thread_state_base {
  THREAD_BASE;
};

struct thread_state1 {
  THREAD_BASE;
  char pad[CACHE_LINE_SIZE
           * ((sizeof (struct thread_state_base) + CACHE_LINE_SIZE - 1)
              / CACHE_LINE_SIZE)
           - sizeof (struct thread_state_base)];
};
#undef THREAD_BASE

struct thread_states {
  os_mutex lock;
  unsigned nthreads;
  struct thread_state1 *ts; /* [nthreads] */
};

extern struct thread_states thread_states;
extern os_threadLocal struct thread_state1 *tsd_thread_state;

void thread_states_init_static (void);
void thread_states_init (_In_ unsigned maxthreads);
void thread_states_fini (void);

void upgrade_main_thread (void);
void downgrade_main_thread (void);
const struct config_thread_properties_listelem *lookup_thread_properties (_In_z_ const char *name);
_Success_(return != NULL) _Ret_maybenull_ struct thread_state1 *create_thread (_In_z_ const char *name, _In_ uint32_t (*f) (void *arg), _In_opt_ void *arg);
_Ret_valid_ struct thread_state1 *lookup_thread_state (void);
_Success_(return != NULL) _Ret_maybenull_ struct thread_state1 *lookup_thread_state_real (void);
_Success_(return == 0) int join_thread (_Inout_ struct thread_state1 *ts1);
void log_stack_traces (void);
struct thread_state1 *get_thread_state (_In_ os_threadId id);
struct thread_state1 * init_thread_state (_In_z_ const char *tname);
void reset_thread_state (_Inout_opt_ struct thread_state1 *ts1);
int thread_exists (_In_z_ const char *name);

#if defined (__cplusplus)
}
#endif

#if NN_HAVE_C99_INLINE && !defined SUPPRESS_THREAD_INLINES
#include "q_thread_template.h"
#else
#if defined (__cplusplus)
extern "C" {
#endif
int vtime_awake_p (_In_ vtime_t vtime);
int vtime_asleep_p (_In_ vtime_t vtime);
int vtime_gt (_In_ vtime_t vtime1, _In_ vtime_t vtime0);

void thread_state_asleep (_Inout_ struct thread_state1 *ts1);
void thread_state_awake (_Inout_ struct thread_state1 *ts1);
void thread_state_blocked (_Inout_ struct thread_state1 *ts1);
void thread_state_unblocked (_Inout_ struct thread_state1 *ts1);
#if defined (__cplusplus)
}
#endif
#endif

#endif /* Q_THREAD_H */
