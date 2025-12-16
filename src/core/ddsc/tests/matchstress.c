// Copyright(c) 2025 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <stdio.h>

#include "dds/dds.h"
#include "dds/ddsrt/threads.h"
#include "test_common.h"
#include "build_options.h"

struct create_reader_thread_arg {
  dds_entity_t dp;
  dds_entity_t tp;
  dds_listener_t *listener;
};

static void on_pub_matched (dds_entity_t wr, const dds_publication_matched_status_t st, void *arg)
{
  (void) wr; (void) st; (void) arg;
}

static void on_sub_matched (dds_entity_t rd, const dds_subscription_matched_status_t st, void *arg)
{
  (void) rd; (void) st; (void) arg;
}

static uint32_t create_reader_thread (void *varg)
{
  struct create_reader_thread_arg *arg = varg;
  const dds_entity_t rd = dds_create_reader (arg->dp, arg->tp, NULL, arg->listener);
  if (rd < 0)
    return __LINE__;
  if (dds_set_listener (rd, NULL) != 0)
    return __LINE__;
  if (dds_delete (rd) != 0)
    return __LINE__;
  return 0;
}

static void do_ddsc_match_stress_single_writer_many_readers (void)
{
  const dds_entity_t dp = dds_create_participant (0, NULL, NULL);
  CU_ASSERT_GT_FATAL (dp, 0);
  char topicname[100];
  create_unique_topic_name ("ddsc_match_stress_single_writer_many_readers", topicname, sizeof (topicname));
  const dds_entity_t tp = dds_create_topic (dp, &Space_Type1_desc, topicname, NULL, NULL);
  CU_ASSERT_GT_FATAL (tp, 0);
  dds_listener_t *listener = dds_create_listener (NULL);
  dds_lset_publication_matched (listener, on_pub_matched);
  dds_lset_subscription_matched (listener, on_sub_matched);
  const dds_entity_t wr = dds_create_writer (dp, tp, NULL, listener);
  CU_ASSERT_GT_FATAL (wr, 0);
  ddsrt_threadattr_t tattr;
  ddsrt_threadattr_init (&tattr);
  ddsrt_thread_t tids[100];
  struct create_reader_thread_arg crtarg = { .dp = dp, .tp = tp, .listener = listener };
  dds_return_t rc;
  for (size_t i = 0; i < sizeof (tids) / sizeof (tids[0]); i++)
  {
    char threadname[100];
    snprintf (threadname, sizeof (threadname), "thr%zu", i);
    rc = ddsrt_thread_create (&tids[i], threadname, &tattr, create_reader_thread, &crtarg);
    CU_ASSERT_EQ_FATAL (rc, 0);
  }
  rc = dds_set_listener (wr, NULL);
  CU_ASSERT_EQ_FATAL (rc, 0);
  rc = dds_delete (wr);
  CU_ASSERT_EQ_FATAL (rc, 0);
  for (size_t i = 0; i < sizeof (tids) / sizeof (tids[0]); i++)
  {
    uint32_t res;
    rc = ddsrt_thread_join (tids[i], &res);
    CU_ASSERT_EQ_FATAL (rc, 0);
    CU_ASSERT_EQ_FATAL (res, 0);
  }
  dds_delete_listener (listener);
  rc = dds_delete (dp);
  CU_ASSERT_EQ_FATAL (rc, 0);
}

CU_Test(ddsc_match_stress, single_writer_many_readers)
{
  // Needs many runs to hit the rare race condition that used to trigger
  //
  // 10 is not nearly enough, 1000 seems to be getting there on macOS on an M1
  for (int i = 0; i < 10; i++)
    do_ddsc_match_stress_single_writer_many_readers ();
}
