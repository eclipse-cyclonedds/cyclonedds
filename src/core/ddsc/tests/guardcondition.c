// Copyright(c) 2006 to 2019 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <assert.h>
#include <limits.h>

#include "dds/dds.h"
#include "CUnit/Theory.h"
#include "RoundTrip.h"

#include "dds/ddsrt/cdtors.h"
#include "dds/ddsrt/misc.h"
#include "dds/ddsrt/process.h"
#include "dds/ddsrt/threads.h"
#include "dds/ddsrt/atomics.h"
#include "dds/ddsrt/time.h"

CU_Test (ddsc_guardcond_create, cyclonedds)
{
  dds_entity_t gc;
  dds_return_t rc;
  /* Expect an uninitialised library */
  rc = dds_get_parent (DDS_CYCLONEDDS_HANDLE);
  CU_ASSERT_FATAL (rc == DDS_RETCODE_PRECONDITION_NOT_MET);
  gc = dds_create_guardcondition (DDS_CYCLONEDDS_HANDLE);
  CU_ASSERT_FATAL (gc > 0);
  rc = dds_delete (DDS_CYCLONEDDS_HANDLE);
  CU_ASSERT_FATAL (rc == 0);
  /* And the same afterward */
  rc = dds_get_parent (DDS_CYCLONEDDS_HANDLE);
  CU_ASSERT_FATAL (rc == DDS_RETCODE_PRECONDITION_NOT_MET);
}

CU_Test (ddsc_guardcond_create, domain)
{
  dds_entity_t par, dom, gc;
  dds_return_t rc;
  par = dds_create_participant (0, NULL, NULL);
  CU_ASSERT_FATAL (par > 0);
  dom = dds_get_parent (par);
  CU_ASSERT_FATAL (dom > 0);
  gc = dds_create_guardcondition (dom);
  CU_ASSERT_FATAL (gc > 0);
  rc = dds_delete (dom);
  CU_ASSERT_FATAL (rc == 0);
}

CU_Test (ddsc_guardcond_create, participant)
{
  dds_entity_t par, gc;
  dds_return_t rc;
  par = dds_create_participant (0, NULL, NULL);
  CU_ASSERT_FATAL (par > 0);
  gc = dds_create_guardcondition (par);
  CU_ASSERT_FATAL (gc > 0);
  rc = dds_delete (par);
  CU_ASSERT_FATAL (rc == 0);
}

CU_Test (ddsc_guardcond, set_trigger)
{
  dds_entity_t par, gc;
  dds_return_t rc;
  bool trig;
  par = dds_create_participant (0, NULL, NULL);
  CU_ASSERT_FATAL (par > 0);
  gc = dds_create_guardcondition (par);
  CU_ASSERT_FATAL (gc > 0);
  rc = dds_read_guardcondition (gc, &trig);
  CU_ASSERT_FATAL (rc == 0);
  CU_ASSERT (!trig);
  rc = dds_set_guardcondition (gc, true);
  CU_ASSERT_FATAL (rc == 0);
  rc = dds_read_guardcondition (gc, &trig);
  CU_ASSERT_FATAL (rc == 0);
  CU_ASSERT (trig);
  rc = dds_delete (par);
  CU_ASSERT_FATAL (rc == 0);
}

CU_Test (ddsc_guardcond, take_trigger)
{
  dds_entity_t par, gc;
  dds_return_t rc;
  bool trig;
  par = dds_create_participant (0, NULL, NULL);
  CU_ASSERT_FATAL (par > 0);
  gc = dds_create_guardcondition (par);
  CU_ASSERT_FATAL (gc > 0);
  rc = dds_read_guardcondition (gc, &trig);
  CU_ASSERT_FATAL (rc == 0);
  CU_ASSERT (!trig);
  rc = dds_set_guardcondition (gc, true);
  CU_ASSERT_FATAL (rc == 0);
  rc = dds_take_guardcondition (gc, &trig);
  CU_ASSERT_FATAL (rc == 0);
  CU_ASSERT (trig);
  rc = dds_read_guardcondition (gc, &trig);
  CU_ASSERT_FATAL (rc == 0);
  CU_ASSERT (!trig);
  rc = dds_delete (par);
  CU_ASSERT_FATAL (rc == 0);
}

CU_Test (ddsc_guardcond, waitset)
{
  dds_entity_t par, gc, ws;
  dds_attach_t xs[1];
  dds_return_t rc;
  par = dds_create_participant (0, NULL, NULL);
  CU_ASSERT_FATAL (par > 0);
  gc = dds_create_guardcondition (par);
  CU_ASSERT_FATAL (gc > 0);
  ws = dds_create_waitset (par);
  CU_ASSERT_FATAL (ws > 0);
  rc = dds_waitset_attach (ws, gc, gc);
  CU_ASSERT_FATAL (rc == 0);
  /* guard cond not triggered: waitset should return 0 */
  rc = dds_waitset_wait (ws, xs, 1, 0);
  CU_ASSERT (rc == 0);
  rc = dds_set_guardcondition (gc, true);
  CU_ASSERT_FATAL (rc == 0);
  /* guard triggered: waitset should return it */
  rc = dds_waitset_wait (ws, xs, 1, 0);
  CU_ASSERT (rc == 1);
  CU_ASSERT (xs[0] == gc);
  rc = dds_delete (par);
  CU_ASSERT_FATAL (rc == 0);
}

struct guardcond_thread_arg {
  dds_entity_t gc;
  dds_return_t ret;
};

static uint32_t guardcond_thread (void *varg)
{
  struct guardcond_thread_arg *arg = varg;
  /* 200ms sleep is hopefully always long enough for the main thread to
     enter wait() and block; a further 1800ms (see wait call) similarly
     for the guard condition to actually trigger it. */
  dds_sleepfor (DDS_MSECS (200));
  arg->ret = dds_set_guardcondition (arg->gc, true);
  return 0;
}

CU_Test (ddsc_guardcond, waitset_thread)
{
  dds_entity_t par, gc, ws;
  dds_attach_t xs[1];
  dds_return_t rc;
  ddsrt_thread_t tid;
  ddsrt_threadattr_t tattr;

  par = dds_create_participant (0, NULL, NULL);
  CU_ASSERT_FATAL (par > 0);
  gc = dds_create_guardcondition (par);
  CU_ASSERT_FATAL (gc > 0);
  ws = dds_create_waitset (par);
  CU_ASSERT_FATAL (ws > 0);
  rc = dds_waitset_attach (ws, gc, gc);
  CU_ASSERT_FATAL (rc == 0);

  struct guardcond_thread_arg arg = { .gc = gc };
  ddsrt_threadattr_init (&tattr);
  rc = ddsrt_thread_create (&tid, "guardcond_thread", &tattr, guardcond_thread, &arg);
  CU_ASSERT_FATAL (rc == 0);

  rc = dds_waitset_wait (ws, xs, 1, DDS_SECS (2));
  CU_ASSERT (rc == 1);
  CU_ASSERT (xs[0] == gc);

  rc = ddsrt_thread_join (tid, NULL);
  CU_ASSERT_FATAL (rc == 0);
  CU_ASSERT_FATAL (arg.ret == 0);

  rc = dds_delete (par);
  CU_ASSERT_FATAL (rc == 0);
}
