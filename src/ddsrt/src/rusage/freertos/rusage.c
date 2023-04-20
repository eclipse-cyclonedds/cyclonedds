// Copyright(c) 2006 to 2019 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <FreeRTOS.h>
#include <task.h>
#include <string.h>

#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/rusage.h"

/* Task CPU time statistics require a high resolution timer. FreeRTOS
   recommends a time base between 10 and 100 times faster than the tick
   interrupt (https://www.freertos.org/rtos-run-time-stats.html), but does not
   define a macro or function to retrieve the base. */

/* Require time base to be defined for conversion to nanoseconds. */

#define DDSRT_NSECS_IN_RUSAGE_TIME_BASE (1) /* FIXME: Make configurable! */

#if !defined(DDSRT_NSECS_IN_RUSAGE_TIME_BASE)
#error "Time base for run time stats is not defined"
#endif

static dds_return_t
rusage_self(ddsrt_rusage_t *usage)
{
  dds_return_t rc = DDS_RETCODE_OK;
  dds_duration_t nsecs;
  UBaseType_t cnt, len;
  TaskStatus_t *states = NULL, *ptr;
  size_t size;

  do {
    len = uxTaskGetNumberOfTasks();
    size = len * sizeof(*states);
    if ((ptr = ddsrt_realloc_s(states, size)) == NULL) {
      rc = DDS_RETCODE_OUT_OF_RESOURCES;
    } else {
      states = ptr;
      /* uxTaskGetSystemState returns 0 if the TaskStatus_t buffer is not
         sufficiently large enough. */
      cnt = uxTaskGetSystemState(states, len, NULL);
    }
  } while (rc == DDS_RETCODE_OK && cnt == 0);

  if (rc == DDS_RETCODE_OK) {
    memset(usage, 0, sizeof(*usage));

    for (len = cnt, cnt = 0; cnt < len; cnt++) {
      nsecs = states[cnt].ulRunTimeCounter * DDSRT_NSECS_IN_RUSAGE_TIME_BASE;
      usage->stime += nsecs; /* FIXME: Protect against possible overflow! */
    }
  }

  ddsrt_free(states);

  return rc;
}

static dds_return_t
rusage_thread(ddsrt_thread_list_id_t tid, ddsrt_rusage_t *usage)
{
  TaskStatus_t states;

  memset(usage, 0, sizeof(*usage));
  memset(&states, 0, sizeof(states));
  vTaskGetInfo(tid, &states, pdFALSE, eInvalid);
  usage->stime = states.ulRunTimeCounter * DDSRT_NSECS_IN_RUSAGE_TIME_BASE;

  return DDS_RETCODE_OK;
}

#if ! DDSRT_HAVE_THREAD_LIST
static
#endif
dds_return_t
ddsrt_getrusage_anythread(ddsrt_thread_list_id_t tid, ddsrt_rusage_t *__restrict usage)
{
  assert(usage != NULL);
  return rusage_thread(tid, usage);
}

dds_return_t
ddsrt_getrusage(enum ddsrt_getrusage_who who, ddsrt_rusage_t *usage)
{
  dds_return_t rc;

  assert(who == DDSRT_RUSAGE_SELF || who == DDSRT_RUSAGE_THREAD);
  assert(usage != NULL);

  if (who == DDSRT_RUSAGE_THREAD) {
    rc = ddsrt_getrusage_anythread(xTaskGetCurrentTaskHandle(), usage);
  } else {
    rc = rusage_self(usage);
  }

  return rc;
}
