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
#include <assert.h>
#include <limits.h>

#include "dds/dds.h"
#include "dds/ddsrt/process.h"
#include "dds/ddsrt/threads.h"
#include "dds/ddsrt/environ.h"
#include "dds/ddsi/ddsi_entity_index.h"
#include "dds/ddsi/q_entity.h"
#include "dds/ddsi/q_whc.h"
#include "dds__entity.h"

#include "test_common.h"

#define MAX_RUNS 4
#define WRITER_DEADLINE DDS_MSECS(50)

#define DDS_DOMAINID_PUB 0
#define DDS_DOMAINID_SUB 1
#ifdef DDS_HAS_SHM
#define DDS_CONFIG_NO_PORT_GAIN "${CYCLONEDDS_URI}${CYCLONEDDS_URI:+,}<Discovery><ExternalDomainId>0</ExternalDomainId></Discovery><Domain id=\"any\"><SharedMemory><Enable>false</Enable></SharedMemory></Domain>"
#else
#define DDS_CONFIG_NO_PORT_GAIN "${CYCLONEDDS_URI}${CYCLONEDDS_URI:+,}<Discovery><ExternalDomainId>0</ExternalDomainId></Discovery>"
#endif

static dds_entity_t g_domain = 0;
static dds_entity_t g_participant = 0;
static dds_entity_t g_subscriber  = 0;
static dds_entity_t g_publisher   = 0;
static dds_entity_t g_topic       = 0;
static dds_qos_t *g_qos;
static dds_entity_t g_remote_domain      = 0;
static dds_entity_t g_remote_participant = 0;
static dds_entity_t g_remote_subscriber  = 0;
static dds_entity_t g_remote_topic       = 0;

static dds_entity_t create_and_sync_reader(dds_entity_t participant, dds_entity_t subscriber, dds_entity_t topic, dds_qos_t *qos, dds_entity_t writer)
{
  dds_entity_t reader = dds_create_reader(subscriber, topic, qos, NULL);
  CU_ASSERT_FATAL(reader > 0);
  sync_reader_writer (participant, reader, g_participant, writer);
  dds_return_t ret = dds_set_status_mask(reader, DDS_REQUESTED_DEADLINE_MISSED_STATUS);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
  return reader;
}

static void deadline_init(void)
{
  char name[100];

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

  create_unique_topic_name("ddsc_qos_deadline_test", name, sizeof name);
  g_topic = dds_create_topic(g_participant, &Space_Type1_desc, name, NULL, NULL);
  CU_ASSERT_FATAL(g_topic > 0);
  g_remote_topic = dds_create_topic(g_remote_participant, &Space_Type1_desc, name, NULL, NULL);
  CU_ASSERT_FATAL(g_remote_topic > 0);

  dds_qset_history(g_qos, DDS_HISTORY_KEEP_ALL, DDS_LENGTH_UNLIMITED);
  dds_qset_durability(g_qos, DDS_DURABILITY_TRANSIENT_LOCAL);
  dds_qset_reliability(g_qos, DDS_RELIABILITY_RELIABLE, DDS_INFINITY);
  dds_qset_writer_data_lifecycle(g_qos, false);
}

static void deadline_fini(void)
{
  dds_delete_qos(g_qos);
  dds_delete(g_subscriber);
  dds_delete(g_remote_subscriber);
  dds_delete(g_publisher);
  dds_delete(g_topic);
  dds_delete(g_participant);
  dds_delete(g_remote_participant);
  dds_delete(g_domain);
  dds_delete(g_remote_domain);
}

static void sleepfor(dds_duration_t sleep_dur)
{
  dds_sleepfor (sleep_dur);
  tprintf("after sleeping %"PRId64"\n", sleep_dur);
}

static bool check_missed_deadline_reader(dds_entity_t reader, uint32_t exp_missed_total, int32_t exp_missed_change)
{
  struct dds_requested_deadline_missed_status dstatus;
  dds_return_t ret = dds_get_requested_deadline_missed_status(reader, &dstatus);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);
  tprintf("- check reader total actual %u == expected %u / change actual %d == expected %d\n", dstatus.total_count, exp_missed_total, dstatus.total_count_change, exp_missed_change);
  return dstatus.total_count == exp_missed_total && dstatus.total_count_change == exp_missed_change;
}

static bool check_missed_deadline_writer(dds_entity_t writer, uint32_t exp_missed_total, int32_t exp_missed_change)
{
  struct dds_offered_deadline_missed_status dstatus;
  dds_return_t ret = dds_get_offered_deadline_missed_status(writer, &dstatus);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);
  tprintf("- check writer total actual %u == expected %u / change actual %d == expected %d\n", dstatus.total_count, exp_missed_total, dstatus.total_count_change, exp_missed_change);
  return dstatus.total_count == exp_missed_total && dstatus.total_count_change == exp_missed_change;
}

CU_Test(ddsc_deadline, basic, .init=deadline_init, .fini=deadline_fini)
{
  Space_Type1 sample = { 0, 0, 0 };
  dds_entity_t reader, reader_remote, reader_dl, reader_dl_remote, writer;
  dds_return_t ret;
  dds_duration_t deadline_dur = WRITER_DEADLINE;
  uint32_t run = 1;
  bool test_finished = false;

  do
  {
    tprintf("deadline test: duration %"PRId64"\n", deadline_dur);

    dds_qset_deadline(g_qos, deadline_dur);
    writer = dds_create_writer(g_publisher, g_topic, g_qos, NULL);
    CU_ASSERT_FATAL(writer > 0);

    reader_dl = create_and_sync_reader(g_participant, g_subscriber, g_topic, g_qos, writer);
    reader_dl_remote = create_and_sync_reader(g_remote_participant, g_remote_subscriber, g_remote_topic, g_qos, writer);

    dds_qset_deadline(g_qos, DDS_INFINITY);
    reader = create_and_sync_reader(g_participant, g_subscriber, g_topic, g_qos, writer);
    reader_remote = create_and_sync_reader(g_remote_participant, g_remote_subscriber, g_remote_topic, g_qos, writer);

    ret = dds_set_status_mask(writer, DDS_OFFERED_DEADLINE_MISSED_STATUS);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    /* Write first sample */
    tprintf("write sample 1\n");
    ret = dds_write (writer, &sample);
    CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);

    /* Sleep 0.5 * deadline_dur: expect no missed deadlines for reader and writer */
    sleepfor(deadline_dur / 2);
    if (!check_missed_deadline_reader(reader, 0, 0) ||
        !check_missed_deadline_reader(reader_remote, 0, 0) ||
        !check_missed_deadline_reader(reader_dl, 0, 0) ||
        !check_missed_deadline_reader(reader_dl_remote, 0, 0) ||
        !check_missed_deadline_writer(writer, 0, 0))
      deadline_dur *= 10 / (run + 1);
    else
    {
      /* Write another sample */
      tprintf("write sample 2\n");
      ret = dds_write (writer, &sample);
      CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);

      /* Sleep 0.5 * deadline_dur: expect no missed deadlines for reader and writer */
      sleepfor(deadline_dur / 2);
      if (!check_missed_deadline_reader(reader, 0, 0) ||
          !check_missed_deadline_reader(reader_remote, 0, 0) ||
          !check_missed_deadline_reader(reader_dl, 0, 0) ||
          !check_missed_deadline_reader(reader_dl_remote, 0, 0) ||
          !check_missed_deadline_writer(writer, 0, 0))
        deadline_dur *= 10 / (run + 1);
      else
      {
        /* Sleep deadline_dur: expect deadline reader to have 1 missed deadline */
        sleepfor(deadline_dur);
        if (!check_missed_deadline_reader(reader, 0, 0) ||
            !check_missed_deadline_reader(reader_remote, 0, 0) ||
            !check_missed_deadline_reader(reader_dl, 1, 1) ||
            !check_missed_deadline_reader(reader_dl_remote, 1, 1) ||
            !check_missed_deadline_writer(writer, 1, 1))
          deadline_dur *= 10 / (run + 1);
        else
        {
          /* Sleep another 2 * deadline_duration: expect 2 new triggers for missed deadline for both reader and writer */
          sleepfor(2 * deadline_dur);
          if (!check_missed_deadline_reader(reader, 0, 0) ||
              !check_missed_deadline_reader(reader_remote, 0, 0) ||
              !check_missed_deadline_reader(reader_dl, 3, 2) ||
              !check_missed_deadline_reader(reader_dl_remote, 3, 2) ||
              !check_missed_deadline_writer(writer, 3, 2))
            deadline_dur *= 10 / (run + 1);
          else
            test_finished = true;
        }
      }
    }

    dds_delete(reader);
    dds_delete(writer);

    if (!test_finished)
    {
      if (++run > MAX_RUNS)
      {
        tprintf("run limit reached, test failed\n");
        CU_FAIL_FATAL("Run limit reached");
        test_finished = true;
      }
      else
      {
        tprintf("restarting test with deadline duration %"PRId64"\n", deadline_dur);
        sleepfor(deadline_dur);
      }
    }
  } while (!test_finished);
}

#define V DDS_DURABILITY_VOLATILE
#define TL DDS_DURABILITY_TRANSIENT_LOCAL
#define R DDS_RELIABILITY_RELIABLE
#define BE DDS_RELIABILITY_BEST_EFFORT
#define KA DDS_HISTORY_KEEP_ALL
#define KL DDS_HISTORY_KEEP_LAST
CU_TheoryDataPoints(ddsc_deadline, writer_types) = {
    CU_DataPoints(dds_durability_kind_t,   V,  V,  V,  V, TL, TL, TL, TL),
    CU_DataPoints(dds_reliability_kind_t, BE, BE,  R,  R, BE, BE,  R,  R),
    CU_DataPoints(dds_history_kind_t,     KA, KL, KA, KL, KA, KL, KA, KL)
};
#undef V
#undef TL
#undef R
#undef BE
#undef KA
#undef KL
CU_Theory((dds_durability_kind_t dur_kind, dds_reliability_kind_t rel_kind, dds_history_kind_t hist_kind), ddsc_deadline, writer_types, .init = deadline_init, .fini = deadline_fini)
{
  Space_Type1 sample = { 0, 0, 0 };
  dds_entity_t reader, writer;
  dds_qos_t *qos;
  dds_return_t ret;
  void * samples[1];
  dds_sample_info_t info;
  Space_Type1 rd_sample;
  samples[0] = &rd_sample;
  struct dds_offered_deadline_missed_status dstatus;
  uint32_t run = 1;
  dds_duration_t deadline_dur = WRITER_DEADLINE;
  bool test_finished = false;

  do
  {
    tprintf("deadline test: duration %"PRId64", writer type %d %d %s\n", deadline_dur, dur_kind, rel_kind, hist_kind == DDS_HISTORY_KEEP_ALL ? "all" : "1");

    qos = dds_create_qos();
    CU_ASSERT_PTR_NOT_NULL_FATAL(qos);
    dds_qset_durability(qos, dur_kind);
    dds_qset_reliability(qos, rel_kind, DDS_INFINITY);
    dds_qset_history(qos, hist_kind, (hist_kind == DDS_HISTORY_KEEP_ALL) ? 0 : 1);
    dds_qset_deadline(qos, deadline_dur);
    writer = dds_create_writer(g_publisher, g_topic, qos, NULL);
    CU_ASSERT_FATAL(writer > 0);
    reader = create_and_sync_reader(g_participant, g_subscriber, g_topic, qos, writer);

    /* Set status mask on writer to get offered deadline missed status */
    ret = dds_set_status_mask(writer, DDS_OFFERED_DEADLINE_MISSED_STATUS);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    /* Write sample */
    ret = dds_write (writer, &sample);
    CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);

    /* Take sample */
    ret = dds_take (reader, samples, &info, 1, 1);
    CU_ASSERT_EQUAL_FATAL (ret, 1);

    /* Sleep 2 * deadline_dur: expect missed deadlines for writer */
    sleepfor(2 * deadline_dur);
    ret = dds_get_offered_deadline_missed_status(writer, &dstatus);
    CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);
    tprintf("- check writer total actual %u > 0 / change actual %d > 0\n", dstatus.total_count, dstatus.total_count_change);
    if (dstatus.total_count == 0 || dstatus.total_count_change == 0)
      deadline_dur *= 10 / (run + 1);
    else
    {
      uint32_t prev_cnt = dstatus.total_count;

      /* Sleep 3 * deadline_dur: expect more missed deadlines for writer */
      sleepfor(3 * deadline_dur);
      ret = dds_get_offered_deadline_missed_status(writer, &dstatus);
      CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);
      tprintf("- check reader total actual %u > expected %u / change actual %d > 0\n", dstatus.total_count, prev_cnt, dstatus.total_count_change);
      if (dstatus.total_count <= prev_cnt || dstatus.total_count_change == 0)
        deadline_dur *= 10 / (run + 1);
      else
        test_finished = true;
    }

    dds_delete_qos(qos);
    dds_delete(reader);
    dds_delete(writer);

    if (!test_finished)
    {
      if (++run > MAX_RUNS)
      {
        tprintf("run limit reached, test failed\n");
        CU_FAIL_FATAL("Run limit reached");
        test_finished = true;
      }
      else
      {
        tprintf("restarting test with deadline duration %"PRId64"\n", deadline_dur);
        sleepfor(deadline_dur);
      }
    }
  } while (!test_finished);
}

CU_TheoryDataPoints(ddsc_deadline, instances) = {
    CU_DataPoints(int32_t, 1, 10, 10, 100), /* instance count */
    CU_DataPoints(uint8_t, 0,  0,  4,  10), /* unregister every n-th instance */
    CU_DataPoints(uint8_t, 0,  0,  5,  20), /* dispose every n-th instance */
};
CU_Theory((int32_t n_inst, uint8_t unreg_nth, uint8_t dispose_nth), ddsc_deadline, instances, .init = deadline_init, .fini = deadline_fini, .timeout = 60)
{
  Space_Type1 sample = { 0, 0, 0 };
  dds_entity_t reader_dl, writer;
  dds_return_t ret;
  int32_t n, n_unreg, n_dispose, n_alive, run = 1;
  bool test_finished = false;
  dds_duration_t deadline_dur = WRITER_DEADLINE;

  do
  {
    tprintf("deadline test: duration %"PRId64", instance count %d, unreg %dth, dispose %dth\n", deadline_dur, n_inst, unreg_nth, dispose_nth);
    dds_qset_deadline(g_qos, deadline_dur);
    CU_ASSERT_PTR_NOT_NULL_FATAL(g_qos);

    writer = dds_create_writer(g_publisher, g_topic, g_qos, NULL);
    CU_ASSERT_FATAL(writer > 0);
    reader_dl = create_and_sync_reader(g_participant, g_subscriber, g_topic, g_qos, writer);

    /* Write first sample for each instance */
    n_unreg = n_dispose = 0;
    for (n = 1; n <= n_inst; n++)
    {
      sample.long_1 = n;
      ret = dds_write (writer, &sample);
      CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);
      if (unreg_nth && n % unreg_nth == 0)
      {
        ret = dds_unregister_instance (writer, &sample);
        CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);
        n_unreg++;
      }
      else if (dispose_nth && n % dispose_nth == 0)
      {
        ret = dds_dispose (writer, &sample);
        CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);
        n_dispose++;
      }
    }
    /* FIXME: should unregistered instances cause deadline expirations? I do think so
       and that is what it actually implemented
       if they shouldn't: n_alive = n_inst - n_dispose - n_unreg */
    n_alive = n_inst - n_dispose;

    /* Sleep deadline_dur + 50% and check missed deadline count */
    sleepfor(3 * deadline_dur / 2);
    if (!check_missed_deadline_reader(reader_dl, (uint32_t)n_alive, n_alive))
      deadline_dur *= 10 / (run + 1);
    else
    {
      /* Sleep another deadline_dur: expect new trigger for missed deadline for all non-disposed instances */
      sleepfor(deadline_dur);
      if (!check_missed_deadline_reader(reader_dl, 2 * (uint32_t)n_alive, n_alive))
        deadline_dur *= 10 / (run + 1);
      else
      {
        /* Write second sample for all (including disposed) instances */
        for (n = 1; n <= n_inst; n++)
        {
          sample.long_1 = n;
          ret = dds_write (writer, &sample);
          CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);
        }

        /* Sleep deadline_dur + 25%: expect new trigger for missed deadline for non-disposed instances */
        sleepfor(5 * deadline_dur / 4);
        if (!check_missed_deadline_reader(reader_dl, 2 * (uint32_t)n_alive + (uint32_t)n_inst, n_inst))
          deadline_dur *= 10 / (run + 1);
        else
          test_finished = true;
      }
    }

    dds_delete(reader_dl);
    dds_delete(writer);

    if (!test_finished)
    {
      if (++run > MAX_RUNS)
      {
        tprintf("run limit reached, test failed\n");
        CU_FAIL_FATAL("Run limit reached");
        test_finished = true;
      }
      else
      {
        tprintf("restarting test with deadline duration %"PRId64"\n", deadline_dur);
        sleepfor(deadline_dur);
      }
    }
  } while (!test_finished);
}
