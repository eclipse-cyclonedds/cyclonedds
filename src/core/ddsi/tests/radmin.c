/*
 * Copyright(c) 2022 ZettaScale Technology and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */

#include "CUnit/Theory.h"

#include "dds/features.h"
#include "dds/ddsi/ddsi_iid.h"
#include "dds/ddsi/ddsi_config_impl.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsi/q_radmin.h"
#include "dds/ddsi/q_thread.h"
#include "dds/ddsi/q_misc.h"

static struct ddsi_domaingv gv;
static struct thread_state *thrst;
static struct nn_rbufpool *rbpool;

static void null_log_sink (void *varg, const dds_log_data_t *msg)
{
  (void)varg; (void)msg;
}

static void setup (void)
{
  ddsi_iid_init ();
  thread_states_init ();

  // register the main thread, then claim it as spawned by Cyclone because the
  // internal processing has various asserts that it isn't an application thread
  // doing the dirty work
  thrst = lookup_thread_state ();
  // coverity[missing_lock:FALSE]
  assert (thrst->state == THREAD_STATE_LAZILY_CREATED);
  thrst->state = THREAD_STATE_ALIVE;
  ddsrt_atomic_stvoidp (&thrst->gv, &gv);

  memset (&gv, 0, sizeof (gv));
  ddsi_config_init_default (&gv.config);
  gv.config.transport_selector = DDSI_TRANS_NONE;

  rtps_config_prep (&gv, NULL);
  dds_set_log_sink (null_log_sink, NULL);
  dds_set_trace_sink (null_log_sink, NULL);

  rtps_init (&gv);
  rbpool = nn_rbufpool_new (&gv.logconfig, gv.config.rbuf_size, gv.config.rmsg_chunk_size);
  nn_rbufpool_setowner (rbpool, ddsrt_thread_self ());
}

static void teardown (void)
{
  rtps_fini (&gv);
  nn_rbufpool_free (rbpool);

  // On shutdown, there is an expectation that the thread was discovered dynamically.
  // We overrode it in the setup code, we undo it now.
  // coverity[missing_lock:FALSE]
  thrst->state = THREAD_STATE_LAZILY_CREATED;
  thread_states_fini ();
  ddsi_iid_fini ();
}

static void insert_gap (struct nn_reorder *reorder, struct nn_rmsg *rmsg, seqno_t seq)
{
  struct nn_rdata *gap = nn_rdata_newgap (rmsg);
  struct nn_rsample_chain sc;
  int refc_adjust = 0;
  nn_reorder_result_t res = nn_reorder_gap (&sc, reorder, gap, seq, seq + 1, &refc_adjust);
  CU_ASSERT_FATAL (res == NN_REORDER_ACCEPT);
  nn_fragchain_adjust_refcount (gap, refc_adjust);
}

static void check_reorder (struct nn_reorder *reorder, uint64_t ndiscard, seqno_t next_exp, seqno_t end, const seqno_t *present)
{
  // expect to be waiting for the right sequence number
  CU_ASSERT_FATAL (nn_reorder_next_seq (reorder) == next_exp);
  // expect the number of discarded bytes to match
  uint64_t discarded_bytes;
  nn_reorder_stats (reorder, &discarded_bytes);
  CU_ASSERT_FATAL (discarded_bytes == ndiscard);
  // expect the set of present sequence numbers to match
  int i = 0, err = 0;
  printf ("check:");
  for (seqno_t s = next_exp; s <= end; s++)
  {
    if (s < present[i] || present[i] == 0) {
      int w = nn_reorder_wantsample (reorder, s);
      printf (" -%"PRId64"/%d", s, w);
      if (!w) err++;
    } else {
      if (s == present[i])
        i++;
      int w = nn_reorder_wantsample (reorder, s);
      if (w) err++;
      printf (" +%"PRId64"/%d", s, w);
    }
  }
  printf ("\n");
  CU_ASSERT_FATAL (err == 0);
}

static void insert_sample (struct nn_defrag *defrag, struct nn_reorder *reorder, struct nn_rmsg *rmsg, struct receiver_state *rst, seqno_t seq)
{
  struct nn_rsample_info *si = nn_rmsg_alloc (rmsg, sizeof (*si));
  CU_ASSERT_FATAL (si != NULL);
  assert (si);
  // only "seq" and "size" really matter
  memset (si, 0, sizeof (*si));
  si->rst = rst;
  si->size = 1;
  si->seq = seq;
  struct nn_rdata *rdata = nn_rdata_new (rmsg, 0, si->size, 0, 0, 0);
  struct nn_rsample *rsample = nn_defrag_rsample (defrag, rdata, si);
  CU_ASSERT_FATAL (rsample != NULL);

  struct nn_rsample_chain sc;
  int refc_adjust = 0;
  struct nn_rdata *fragchain = nn_rsample_fragchain (rsample);
  nn_reorder_result_t res = nn_reorder_rsample (&sc, reorder, rsample, &refc_adjust, 0);
  CU_ASSERT_FATAL (res == NN_REORDER_ACCEPT);
  nn_fragchain_adjust_refcount (fragchain, refc_adjust);
}

CU_Test (ddsi_radmin, drop_gap_at_end, .init = setup, .fini = teardown)
{
  // not doing fragmented samples in this test, so defragmenter mode & size limits are irrelevant
  struct nn_defrag *defrag = nn_defrag_new (&gv.logconfig, NN_DEFRAG_DROP_LATEST, 1);
  struct nn_reorder *reorder = nn_reorder_new (&gv.logconfig, NN_REORDER_MODE_NORMAL, 3, false);
  CU_ASSERT_FATAL (nn_reorder_next_seq (reorder) == 1);

  // pretending that we get all the input as a single RTPSMessage
  struct nn_rmsg *rmsg = nn_rmsg_new (rbpool);
  nn_rmsg_setsize (rmsg, 0); // 0 isn't true, but it doesn't matter
  // actual receiver state is pretty much irrelevant to the reorder buffer
  struct receiver_state *rst = nn_rmsg_alloc (rmsg, sizeof (*rst));
  memset (rst, 0, sizeof (*rst));

  // initially, we want everything
  check_reorder(reorder, 0, 1, 6, (const seqno_t[]){0});
  // insert gap #5 => no longer want 5
  insert_gap (reorder, rmsg, 5);
  check_reorder(reorder, 0, 1, 6, (const seqno_t[]){5,0});
  // etc. etc.
  insert_sample (defrag, reorder, rmsg, rst, 2);
  check_reorder(reorder, 0, 1, 6, (const seqno_t[]){2,5,0});
  insert_sample (defrag, reorder, rmsg, rst, 3);
  check_reorder(reorder, 0, 1, 6, (const seqno_t[]){2,3,5,0});
  // inserting #4 pushes gap out, so suddenly we want it again
  insert_sample (defrag, reorder, rmsg, rst, 4);
  check_reorder(reorder, 0, 1, 6, (const seqno_t[]){2,3,4,0});

  nn_rmsg_commit (rmsg);
  nn_reorder_free (reorder);
  nn_defrag_free (defrag);
}
