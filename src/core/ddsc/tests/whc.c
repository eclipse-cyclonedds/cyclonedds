// Copyright(c) 2006 to 2022 ZettaScale Technology and others
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
#include "dds/ddsrt/process.h"
#include "dds/ddsrt/threads.h"
#include "dds/ddsrt/environ.h"
#include "dds/ddsi/ddsi_entity_index.h"
#include "dds/ddsi/ddsi_entity.h"
#include "ddsi__whc.h"
#include "dds__entity.h"

#include "test_common.h"

#define DDS_DOMAINID_PUB 0
#define DDS_DOMAINID_SUB 1
#define DDS_CONFIG_NO_PORT_GAIN "${CYCLONEDDS_URI}${CYCLONEDDS_URI:+,}<Discovery><ExternalDomainId>0</ExternalDomainId></Discovery>"
#define DDS_CONFIG_NO_PORT_GAIN_LOG "${CYCLONEDDS_URI}${CYCLONEDDS_URI:+,}<Tracing><OutputFile>cyclonedds_whc_test.${CYCLONEDDS_DOMAIN_ID}.${CYCLONEDDS_PID}.log</OutputFile><Verbosity>finest</Verbosity></Tracing><Discovery><ExternalDomainId>0</ExternalDomainId></Discovery>"

#define SAMPLE_COUNT 5
#define DEADLINE_DURATION DDS_MSECS(1)

static dds_entity_t g_domain = 0;
static dds_entity_t g_participant   = 0;
static dds_entity_t g_subscriber    = 0;
static dds_entity_t g_publisher     = 0;
static dds_qos_t *g_qos;
static dds_entity_t g_remote_domain        = 0;
static dds_entity_t g_remote_participant   = 0;
static dds_entity_t g_remote_subscriber    = 0;

static void whc_init(void)
{
  /* Domains for pub and sub use a different domain id, but the portgain setting
         * in configuration is 0, so that both domains will map to the same port number.
         * This allows to create two domains in a single test process. */
  char *conf_pub = ddsrt_expand_envvars(DDS_CONFIG_NO_PORT_GAIN, DDS_DOMAINID_PUB);
  char *conf_sub = ddsrt_expand_envvars(DDS_CONFIG_NO_PORT_GAIN, DDS_DOMAINID_SUB);
  g_domain = dds_create_domain(DDS_DOMAINID_PUB, conf_pub);
  g_remote_domain = dds_create_domain(DDS_DOMAINID_SUB, conf_sub);
  dds_free(conf_pub);
  dds_free(conf_sub);

  g_qos = dds_create_qos();
  CU_ASSERT_PTR_NOT_NULL_FATAL(g_qos);

  g_participant = dds_create_participant(DDS_DOMAINID_PUB, NULL, NULL);
  CU_ASSERT_FATAL(g_participant > 0);
  g_remote_participant = dds_create_participant(DDS_DOMAINID_SUB, NULL, NULL);
  CU_ASSERT_FATAL(g_remote_participant > 0);

  g_subscriber = dds_create_subscriber(g_participant, NULL, NULL);
  CU_ASSERT_FATAL(g_subscriber > 0);

  g_remote_subscriber = dds_create_subscriber(g_remote_participant, NULL, NULL);
  CU_ASSERT_FATAL(g_remote_subscriber > 0);

  g_publisher = dds_create_publisher(g_participant, NULL, NULL);
  CU_ASSERT_FATAL(g_publisher > 0);
}

static void whc_fini (void)
{
  dds_delete_qos(g_qos);
  dds_delete(g_subscriber);
  dds_delete(g_remote_subscriber);
  dds_delete(g_publisher);
  dds_delete(g_participant);
  dds_delete(g_remote_participant);
  dds_delete(g_domain);
  dds_delete(g_remote_domain);
}

static dds_entity_t create_and_sync_reader(dds_entity_t subscriber, dds_entity_t topic, dds_qos_t *qos, dds_entity_t writer)
{
  dds_return_t ret;
  dds_entity_t reader = dds_create_reader(subscriber, topic, qos, NULL);
  CU_ASSERT_FATAL(reader > 0);
  while (1)
  {
    dds_publication_matched_status_t st;
    ret = dds_get_publication_matched_status (writer, &st);
    CU_ASSERT_FATAL (ret == DDS_RETCODE_OK);
    if (st.current_count_change == 1)
      break;
    dds_sleepfor (DDS_MSECS (1));
  }
  return reader;
}

static void get_writer_whc_state (dds_entity_t writer, struct ddsi_whc_state *whcst)
{
  struct dds_entity *wr_entity;
  struct ddsi_writer *wr;
  CU_ASSERT_EQUAL_FATAL(dds_entity_pin(writer, &wr_entity), 0);
  ddsi_thread_state_awake(ddsi_lookup_thread_state(), &wr_entity->m_domain->gv);
  wr = ddsi_entidx_lookup_writer_guid (wr_entity->m_domain->gv.entity_index, &wr_entity->m_guid);
  CU_ASSERT_FATAL(wr != NULL);
  assert(wr != NULL); /* for Clang's static analyzer */
  ddsi_whc_get_state(wr->whc, whcst);
  ddsi_thread_state_asleep(ddsi_lookup_thread_state());
  dds_entity_unpin(wr_entity);
}

static void check_intermediate_whc_state(dds_entity_t writer, ddsi_seqno_t exp_min, ddsi_seqno_t exp_max)
{
  struct ddsi_whc_state whcst;
  get_writer_whc_state (writer, &whcst);
  /* WHC must not contain any samples < exp_min and must contain at least exp_max if it
     contains at least one sample.  (We never know for certain when ACKs arrive.) */
  printf(" -- intermediate state: unacked: %zu; min %"PRIu64" (exp %"PRIu64"); max %"PRIu64" (exp %"PRIu64")\n", whcst.unacked_bytes, whcst.min_seq, exp_min, whcst.max_seq, exp_max);
  CU_ASSERT_FATAL (whcst.min_seq >= exp_min || (whcst.min_seq == 0 && whcst.max_seq == 0));
  CU_ASSERT_FATAL (whcst.max_seq == exp_max || (whcst.min_seq == 0 && whcst.max_seq == 0));
}

static void check_whc_state(dds_entity_t writer, ddsi_seqno_t exp_min, ddsi_seqno_t exp_max)
{
  struct ddsi_whc_state whcst;
  get_writer_whc_state (writer, &whcst);
  printf(" -- final state: unacked: %zu; min %"PRIu64" (exp %"PRIu64"); max %"PRIu64" (exp %"PRIu64")\n", whcst.unacked_bytes, whcst.min_seq, exp_min, whcst.max_seq, exp_max);
  CU_ASSERT_EQUAL_FATAL (whcst.unacked_bytes, 0);
  CU_ASSERT_EQUAL_FATAL (whcst.min_seq, exp_min);
  CU_ASSERT_EQUAL_FATAL (whcst.max_seq, exp_max);
}

#define V DDS_DURABILITY_VOLATILE
#define TL DDS_DURABILITY_TRANSIENT_LOCAL
#define R DDS_RELIABILITY_RELIABLE
#define BE DDS_RELIABILITY_BEST_EFFORT
#define KA DDS_HISTORY_KEEP_ALL
#define KL DDS_HISTORY_KEEP_LAST
static void test_whc_end_state(dds_durability_kind_t d, dds_reliability_kind_t r, dds_history_kind_t h, int32_t hd, dds_history_kind_t dh,
    int32_t dhd, bool lrd, bool rrd, int32_t ni, bool k, bool dl)
{
  char name[100];
  Space_Type1 sample = { 0, 0, 0 };
  Space_Type3 sample_keyless = { 0, 0, 0 };
  dds_entity_t reader, reader_remote, writer;
  dds_entity_t topic;
  dds_entity_t remote_topic;
  dds_return_t ret;
  int32_t s, i;

  printf ("test_whc_end_state: %s, %s, %s(%d), durability %s(%d), readers: %u local, %u remote, instances: %"PRId32", key %u, deadline %"PRId64"\n",
      d == V ? "volatile" : "TL",
      r == BE ? "best-effort" : "reliable",
      h == KA ? "keep-all" : "keep-last", h == KA ? 0 : hd,
      dh == KA ? "keep-all" : "keep-last", dh == KA ? 0 : dhd,
      (unsigned)lrd, (unsigned)rrd, ni, (unsigned)k,
      dl ? DEADLINE_DURATION : INT64_C(-1));

  dds_qset_durability (g_qos, d);
  dds_qset_reliability (g_qos, r, DDS_INFINITY);
  dds_qset_history (g_qos, h, h == KA ? 0 : hd);
  dds_qset_deadline (g_qos, dl ? DEADLINE_DURATION : DDS_INFINITY);
  dds_qset_durability_service (g_qos, 0, dh, dh == KA ? 0 : dhd, DDS_LENGTH_UNLIMITED, DDS_LENGTH_UNLIMITED, DDS_LENGTH_UNLIMITED);

  create_unique_topic_name ("ddsc_whc_end_state_test", name, sizeof name);
  topic = dds_create_topic (g_participant, k ? &Space_Type1_desc : &Space_Type3_desc, name, NULL, NULL);
  CU_ASSERT_FATAL(topic > 0);
  remote_topic = dds_create_topic (g_remote_participant, k ? &Space_Type1_desc : &Space_Type3_desc, name, NULL, NULL);
  CU_ASSERT_FATAL(remote_topic > 0);

  writer = dds_create_writer (g_publisher, topic, g_qos, NULL);
  CU_ASSERT_FATAL(writer > 0);
  ret = dds_set_status_mask(writer, DDS_PUBLICATION_MATCHED_STATUS);
  CU_ASSERT_FATAL (ret == DDS_RETCODE_OK)

  reader = lrd ? create_and_sync_reader (g_subscriber, topic, g_qos, writer) : 0;
  reader_remote = rrd ? create_and_sync_reader (g_remote_subscriber, remote_topic, g_qos, writer) : 0;

  for (s = 0; s < SAMPLE_COUNT; s++)
  {
    if (k)
      for (i = 0; i < ni; i++)
      {
        sample.long_1 = (int32_t)i;
        ret = dds_write (writer, &sample);
        CU_ASSERT_FATAL (ret == DDS_RETCODE_OK);
      }
    else
    {
      ret = dds_write (writer, &sample_keyless);
      CU_ASSERT_FATAL (ret == DDS_RETCODE_OK);
    }

    /* if history is truly keep last, there may never be more data present than the max of the
       history depth(s) */
    if (r == R && h != KA && (d == V || dh != KA))
    {
      if (rrd || d != V)
      {
        int32_t depth = (d == V || hd >= dhd) ? hd : dhd;
        int32_t exp_max = ni * (s + 1);
        int32_t exp_min = exp_max - ni * (depth - 1) - (ni - 1);
        // exp_min <= 0 can occur with exp_max > 0 (i.e., non-empty, so a non-sensical exp_min)
        // the check accepts this, treating everything <= 0 the same
        // change to unsigned means we need to clamp it
        if (exp_min < 0)
          exp_min = 0;
        check_intermediate_whc_state (writer, (uint32_t)exp_min, (uint32_t)exp_max);
      }
      else
      {
        check_intermediate_whc_state (writer, 0, 0);
      }
    }
  }

  /* delete readers, wait until no matching reader */
  if (rrd)
  {
    ret = dds_delete (reader_remote);
    CU_ASSERT_FATAL (ret == DDS_RETCODE_OK);
  }
  if (lrd)
  {
    ret = dds_delete (reader);
    CU_ASSERT_FATAL (ret == DDS_RETCODE_OK);
  }
  while (1)
  {
    dds_publication_matched_status_t st;
    ret = dds_get_publication_matched_status (writer, &st);
    CU_ASSERT_FATAL (ret == DDS_RETCODE_OK);
    if (st.current_count == 0)
      break;
    dds_sleepfor (DDS_MSECS (1));
  }

  /* check whc state */
  int32_t exp_max = (d == TL) ? ni * SAMPLE_COUNT : 0;
  int32_t exp_min = (d == TL) ? ((dh == KA) ? 1 : exp_max - dhd * ni + 1) : 0;
  check_whc_state (writer, (uint32_t)exp_min, (uint32_t)exp_max);

  dds_delete (writer);
  dds_delete (remote_topic);
  dds_delete (topic);
}

#define ARRAY_LEN(A) ((int32_t)(sizeof(A) / sizeof(A[0])))
CU_Test(ddsc_whc, check_end_state, .init=whc_init, .fini=whc_fini, .timeout=30)
{
  dds_durability_kind_t dur[] = {V, TL};
  dds_reliability_kind_t rel[] = {BE, R};
  dds_history_kind_t hist[] = {KA, KL};
  dds_history_kind_t dhist[] = {KA, KL};
  int32_t hist_depth[] = {1, 3};
  int32_t dhist_depth[] = {1, 3};
  bool loc_rd[] = {false, true};
  bool rem_rd[] = {false, true};
  int32_t n_inst[] = {1, 3};
  bool keyed[] = {false, true};
#ifdef DDS_HAS_DEADLINE_MISSED
  bool deadline[] = {false, true};
#else
  bool deadline[] = {false};
#endif
  int32_t i_d, i_r, i_h, i_hd, i_dh, i_dhd, i_lrd, i_rrd, i_ni, i_k, i_dl;

  for (i_d = 0; i_d < ARRAY_LEN(dur); i_d++)
    for (i_r = 0; i_r < ARRAY_LEN(rel); i_r++)
      for (i_h = 0; i_h < ARRAY_LEN(hist); i_h++)
        for (i_hd = 0; i_hd < ARRAY_LEN(hist_depth); i_hd++)
          for (i_dh = 0; i_dh < ARRAY_LEN(dhist); i_dh++)
            for (i_dhd = 0; i_dhd < ARRAY_LEN(dhist_depth); i_dhd++)
              for (i_lrd = 0; i_lrd < ARRAY_LEN(loc_rd); i_lrd++)
                for (i_rrd = 0; i_rrd < ARRAY_LEN(rem_rd); i_rrd++)
                  for (i_ni = 0; i_ni < ARRAY_LEN(n_inst); i_ni++)
                    for (i_k = 0; i_k < ARRAY_LEN(keyed); i_k++)
                      for (i_dl = 0; i_dl < ARRAY_LEN(deadline); i_dl++)
                      {
                        if (rel[i_r] == BE && dur[i_d] == TL)
                          continue;
                        else if (hist[i_h] == KA && i_hd > 0)
                          continue;
                        else if (dhist[i_dh] == KA && i_dhd > 0)
                          continue;
                        else
                        {
                          test_whc_end_state (dur[i_d], rel[i_r], hist[i_h], hist_depth[i_hd], dhist[i_dh], dhist_depth[i_dhd],
                              loc_rd[i_lrd], rem_rd[i_rrd], keyed[i_k] ? n_inst[i_ni] : 1, keyed[i_k], deadline[i_dl]);
                        }
                      }
}

#undef ARRAY_LEN
#undef V
#undef TL
#undef R
#undef BE
#undef KA
#undef KL
