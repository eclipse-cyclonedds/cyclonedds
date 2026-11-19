// Copyright(c) 2026 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include "CUnit/Test.h"
#include "dds/ddsi/ddsi_deadline.h"

static void check_miss (struct ddsi_deadline_adm *adm, int64_t scan_time, struct deadline_elem *expected, uint32_t count)
{
  const ddsrt_mtime_t now = { scan_time };
  void *instance;
  CU_ASSERT_EQ_FATAL (ddsi_deadline_next_missed_locked (adm, now, &instance).v, 0);
  CU_ASSERT_EQ_FATAL (instance, expected);
  CU_ASSERT_EQ (ddsi_deadline_compute_deadlines_missed (now, expected, adm->dur), count);
  ddsi_deadline_reregister_instance_locked (adm, expected, now);
}

CU_Test (ddsi_deadline, renew_preserves_miss)
{
  struct ddsi_deadline_adm adm = { .dur = 100 };
  struct deadline_elem first = { 0 }, second = { 0 };
  const ddsrt_mtime_t start = { 0 };
  /* Reregistration sets up the active instances without arming a timer. */
  ddsrt_circlist_init (&adm.list);
  ddsi_deadline_reregister_instance_locked (&adm, &first, start);
  ddsi_deadline_reregister_instance_locked (&adm, &second, start);

  ddsi_deadline_renew_instance_real (&adm, &second, (ddsrt_mtime_t) { 70 });
  ddsi_deadline_renew_instance_real (&adm, &first, (ddsrt_mtime_t) { 130 });

  /* The first instance missed its deadline at 100. The second was renewed
     on time. Neither has reached its next deadline when the monitor runs. */
  check_miss (&adm, 160, &first, 1);
  CU_ASSERT_EQ (ddsi_deadline_next_missed_locked (&adm, (ddsrt_mtime_t) { 160 }, NULL).v, 170);
  check_miss (&adm, 170, &second, 1);
  CU_ASSERT_EQ (ddsi_deadline_next_missed_locked (&adm, (ddsrt_mtime_t) { 229 }, NULL).v, 230);
  check_miss (&adm, 230, &first, 1);

  /* Each instance now misses two periods before the monitor runs again. */
  check_miss (&adm, 460, &second, 2);
  check_miss (&adm, 460, &first, 2);
  CU_ASSERT_EQ (ddsi_deadline_next_missed_locked (&adm, (ddsrt_mtime_t) { 460 }, NULL).v, 470);
}
