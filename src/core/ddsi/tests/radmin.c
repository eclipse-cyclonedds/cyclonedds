// Copyright(c) 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include "CUnit/Theory.h"

#include "dds/features.h"
#include "dds/ddsi/ddsi_iid.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsi/ddsi_init.h"
#include "ddsi__radmin.h"
#include "ddsi__thread.h"
#include "ddsi__misc.h"

static struct ddsi_domaingv gv;
static struct ddsi_thread_state *thrst;
static struct ddsi_rbufpool *rbpool;

static void null_log_sink (void *varg, const dds_log_data_t *msg)
{
  (void)varg; (void)msg;
}

static void setup (void)
{
  ddsi_iid_init ();
  ddsi_thread_states_init ();

  // register the main thread, then claim it as spawned by Cyclone because the
  // internal processing has various asserts that it isn't an application thread
  // doing the dirty work
  thrst = ddsi_lookup_thread_state ();
  // coverity[missing_lock:FALSE]
  assert (thrst->state == DDSI_THREAD_STATE_LAZILY_CREATED);
  thrst->state = DDSI_THREAD_STATE_ALIVE;
  ddsrt_atomic_stvoidp (&thrst->gv, &gv);

  memset (&gv, 0, sizeof (gv));
  ddsi_config_init_default (&gv.config);
  gv.config.transport_selector = DDSI_TRANS_NONE;

  ddsi_config_prep (&gv, NULL);
  dds_set_log_sink (null_log_sink, NULL);
  dds_set_trace_sink (null_log_sink, NULL);

  ddsi_init (&gv);
  rbpool = ddsi_rbufpool_new (&gv.logconfig, gv.config.rbuf_size, gv.config.rmsg_chunk_size);
  ddsi_rbufpool_setowner (rbpool, ddsrt_thread_self ());
}

static void teardown (void)
{
  ddsi_fini (&gv);
  ddsi_rbufpool_free (rbpool);

  // On shutdown, there is an expectation that the thread was discovered dynamically.
  // We overrode it in the setup code, we undo it now.
  // coverity[missing_lock:FALSE]
  thrst->state = DDSI_THREAD_STATE_LAZILY_CREATED;
  ddsi_thread_states_fini ();
  ddsi_iid_fini ();
}

static void insert_gap (struct ddsi_reorder *reorder, struct ddsi_rmsg *rmsg, ddsi_seqno_t seq)
{
  struct ddsi_rdata *gap = ddsi_rdata_newgap (rmsg);
  struct ddsi_rsample_chain sc;
  int refc_adjust = 0;
  ddsi_reorder_result_t res = ddsi_reorder_gap (&sc, reorder, gap, seq, seq + 1, &refc_adjust);
  CU_ASSERT_FATAL (res == DDSI_REORDER_ACCEPT);
  ddsi_fragchain_adjust_refcount (gap, refc_adjust);
}

static void check_reorder (struct ddsi_reorder *reorder, uint64_t ndiscard, ddsi_seqno_t next_exp, ddsi_seqno_t end, const ddsi_seqno_t *present)
{
  // expect to be waiting for the right sequence number
  CU_ASSERT_FATAL (ddsi_reorder_next_seq (reorder) == next_exp);
  // expect the number of discarded bytes to match
  uint64_t discarded_bytes;
  ddsi_reorder_stats (reorder, &discarded_bytes);
  CU_ASSERT_FATAL (discarded_bytes == ndiscard);
  // expect the set of present sequence numbers to match
  int i = 0, err = 0;
  printf ("check:");
  for (ddsi_seqno_t s = next_exp; s <= end; s++)
  {
    if (s < present[i] || present[i] == 0) {
      int w = ddsi_reorder_wantsample (reorder, s);
      printf (" -%"PRId64"/%d", s, w);
      if (!w) err++;
    } else {
      if (s == present[i])
        i++;
      int w = ddsi_reorder_wantsample (reorder, s);
      if (w) err++;
      printf (" +%"PRId64"/%d", s, w);
    }
  }
  printf ("\n");
  CU_ASSERT_FATAL (err == 0);
}

static void insert_sample (struct ddsi_defrag *defrag, struct ddsi_reorder *reorder, struct ddsi_rmsg *rmsg, struct ddsi_receiver_state *rst, ddsi_seqno_t seq)
{
  struct ddsi_rsample_info *si = ddsi_rmsg_alloc (rmsg, sizeof (*si));
  CU_ASSERT_FATAL (si != NULL);
  assert (si);
  // only "seq" and "size" really matter
  memset (si, 0, sizeof (*si));
  si->rst = rst;
  si->size = 1;
  si->seq = seq;
  struct ddsi_rdata *rdata = ddsi_rdata_new (rmsg, 0, si->size, 0, 0, 0);
  struct ddsi_rsample *rsample = ddsi_defrag_rsample (defrag, rdata, si);
  CU_ASSERT_FATAL (rsample != NULL);

  struct ddsi_rsample_chain sc;
  int refc_adjust = 0;
  struct ddsi_rdata *fragchain = ddsi_rsample_fragchain (rsample);
  ddsi_reorder_result_t res = ddsi_reorder_rsample (&sc, reorder, rsample, &refc_adjust, 0);
  CU_ASSERT_FATAL (res == DDSI_REORDER_ACCEPT);
  ddsi_fragchain_adjust_refcount (fragchain, refc_adjust);
}

CU_Test (ddsi_radmin, drop_gap_at_end, .init = setup, .fini = teardown)
{
  // not doing fragmented samples in this test, so defragmenter mode & size limits are irrelevant
  struct ddsi_defrag *defrag = ddsi_defrag_new (&gv.logconfig, DDSI_DEFRAG_DROP_LATEST, 1);
  struct ddsi_reorder *reorder = ddsi_reorder_new (&gv.logconfig, DDSI_REORDER_MODE_NORMAL, 3, false);
  CU_ASSERT_FATAL (ddsi_reorder_next_seq (reorder) == 1);

  // pretending that we get all the input as a single RTPSMessage
  struct ddsi_rmsg *rmsg = ddsi_rmsg_new (rbpool);
  ddsi_rmsg_setsize (rmsg, 0); // 0 isn't true, but it doesn't matter
  // actual receiver state is pretty much irrelevant to the reorder buffer
  struct ddsi_receiver_state *rst = ddsi_rmsg_alloc (rmsg, sizeof (*rst));
  memset (rst, 0, sizeof (*rst));

  // initially, we want everything
  check_reorder(reorder, 0, 1, 6, (const ddsi_seqno_t[]){0});
  // insert gap #5 => no longer want 5
  insert_gap (reorder, rmsg, 5);
  check_reorder(reorder, 0, 1, 6, (const ddsi_seqno_t[]){5,0});
  // etc. etc.
  insert_sample (defrag, reorder, rmsg, rst, 2);
  check_reorder(reorder, 0, 1, 6, (const ddsi_seqno_t[]){2,5,0});
  insert_sample (defrag, reorder, rmsg, rst, 3);
  check_reorder(reorder, 0, 1, 6, (const ddsi_seqno_t[]){2,3,5,0});
  // inserting #4 pushes gap out, so suddenly we want it again
  insert_sample (defrag, reorder, rmsg, rst, 4);
  check_reorder(reorder, 0, 1, 6, (const ddsi_seqno_t[]){2,3,4,0});

  ddsi_rmsg_commit (rmsg);
  ddsi_reorder_free (reorder);
  ddsi_defrag_free (defrag);
}
