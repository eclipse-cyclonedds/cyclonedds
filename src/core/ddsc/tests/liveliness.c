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

#include "dds/ddsrt/cdtors.h"
#include "dds/ddsrt/misc.h"
#include "dds/ddsrt/process.h"
#include "dds/ddsrt/threads.h"
#include "dds/ddsrt/environ.h"
#include "dds/ddsrt/atomics.h"
#include "dds/ddsrt/time.h"
#include "dds/ddsi/ddsi_entity.h"
#include "dds/ddsi/ddsi_entity_index.h"
#include "ddsi__participant.h"
#include "dds/dds.h"
#include "dds/version.h"
#include "dds__entity.h"
#include "config_env.h"
#include "test_common.h"

#define DDS_DOMAINID_PUB 0
#define DDS_DOMAINID_SUB 1
#ifdef DDS_HAS_SHM
#define DDS_CONFIG_NO_PORT_GAIN "${CYCLONEDDS_URI}${CYCLONEDDS_URI:+,}<Discovery><ExternalDomainId>0</ExternalDomainId></Discovery><Domain id=\"any\"><SharedMemory><Enable>false</Enable></SharedMemory></Domain>"
#define DDS_CONFIG_NO_PORT_GAIN_LOG "${CYCLONEDDS_URI}${CYCLONEDDS_URI:+,}<Tracing><OutputFile>cyclonedds_liveliness_tests.${CYCLONEDDS_DOMAIN_ID}.${CYCLONEDDS_PID}.log</OutputFile><Verbosity>finest</Verbosity></Tracing><Discovery><ExternalDomainId>0</ExternalDomainId></Discovery><Domain id=\"any\"><SharedMemory><Enable>false</Enable></SharedMemory></Domain>"
#else
#define DDS_CONFIG_NO_PORT_GAIN "${CYCLONEDDS_URI}${CYCLONEDDS_URI:+,}<Discovery><ExternalDomainId>0</ExternalDomainId></Discovery>"
#define DDS_CONFIG_NO_PORT_GAIN_LOG "${CYCLONEDDS_URI}${CYCLONEDDS_URI:+,}<Tracing><OutputFile>cyclonedds_liveliness_tests.${CYCLONEDDS_DOMAIN_ID}.${CYCLONEDDS_PID}.log</OutputFile><Verbosity>finest</Verbosity></Tracing><Discovery><ExternalDomainId>0</ExternalDomainId></Discovery>"
#endif

static dds_entity_t g_pub_domain = 0;
static dds_entity_t g_pub_participant = 0;
static dds_entity_t g_pub_publisher = 0;

static dds_entity_t g_sub_domain = 0;
static dds_entity_t g_sub_participant = 0;
static dds_entity_t g_sub_subscriber = 0;

static void liveliness_init(void)
{
  /* Domains for pub and sub use a different domain id, but the portgain setting
         * in configuration is 0, so that both domains will map to the same port number.
         * This allows to create two domains in a single test process. */
  char *conf_pub = ddsrt_expand_envvars(DDS_CONFIG_NO_PORT_GAIN, DDS_DOMAINID_PUB);
  char *conf_sub = ddsrt_expand_envvars(DDS_CONFIG_NO_PORT_GAIN, DDS_DOMAINID_SUB);
  g_pub_domain = dds_create_domain(DDS_DOMAINID_PUB, conf_pub);
  g_sub_domain = dds_create_domain(DDS_DOMAINID_SUB, conf_sub);
  dds_free(conf_pub);
  dds_free(conf_sub);

  g_pub_participant = dds_create_participant(DDS_DOMAINID_PUB, NULL, NULL);
  CU_ASSERT_FATAL(g_pub_participant > 0);
  g_sub_participant = dds_create_participant(DDS_DOMAINID_SUB, NULL, NULL);
  CU_ASSERT_FATAL(g_sub_participant > 0);

  g_pub_publisher = dds_create_publisher(g_pub_participant, NULL, NULL);
  CU_ASSERT_FATAL(g_pub_publisher > 0);
  g_sub_subscriber = dds_create_subscriber(g_sub_participant, NULL, NULL);
  CU_ASSERT_FATAL(g_sub_subscriber > 0);
}

static void liveliness_fini(void)
{
  dds_delete(g_sub_domain);
  dds_delete(g_pub_domain);
}

/**
 * Gets the current PMD sequence number for the participant. This
 * can be used to count the number of PMD messages that is sent by
 * the participant.
 */
static ddsi_seqno_t get_pmd_seqno(dds_entity_t participant)
{
  ddsi_seqno_t seqno;
  struct dds_entity *pp_entity;
  struct ddsi_participant *pp;
  struct ddsi_writer *wr;
  CU_ASSERT_EQUAL_FATAL(dds_entity_pin(participant, &pp_entity), 0);
  ddsi_thread_state_awake(ddsi_lookup_thread_state(), &pp_entity->m_domain->gv);
  pp = ddsi_entidx_lookup_participant_guid(pp_entity->m_domain->gv.entity_index, &pp_entity->m_guid);
  wr = ddsi_get_builtin_writer (pp, DDSI_ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_WRITER);
  CU_ASSERT_FATAL(wr != NULL);
  assert(wr != NULL); /* for Clang's static analyzer */
  seqno = wr->seq;
  ddsi_thread_state_asleep(ddsi_lookup_thread_state());
  dds_entity_unpin(pp_entity);
  return seqno;
}

/**
 * Gets the current PMD interval for the participant
 */
static dds_duration_t get_pmd_interval(dds_entity_t participant)
{
  dds_duration_t intv;
  struct dds_entity *pp_entity;
  struct ddsi_participant *pp;
  CU_ASSERT_EQUAL_FATAL(dds_entity_pin(participant, &pp_entity), 0);
  ddsi_thread_state_awake(ddsi_lookup_thread_state(), &pp_entity->m_domain->gv);
  pp = ddsi_entidx_lookup_participant_guid(pp_entity->m_domain->gv.entity_index, &pp_entity->m_guid);
  intv = ddsi_participant_get_pmd_interval(pp);
  ddsi_thread_state_asleep(ddsi_lookup_thread_state());
  dds_entity_unpin(pp_entity);
  return intv;
}

/**
 * Gets the current lease duration for the participant
 */
static dds_duration_t get_ldur_config(dds_entity_t participant)
{
  struct dds_entity *pp_entity;
  dds_duration_t ldur;
  CU_ASSERT_EQUAL_FATAL(dds_entity_pin(participant, &pp_entity), 0);
  ldur = (dds_duration_t)pp_entity->m_domain->gv.config.lease_duration;
  dds_entity_unpin(pp_entity);
  return ldur;
}

/**
 * Test that the correct number of PMD messages is sent for
 * the various liveliness kinds.
 */
#define A DDS_LIVELINESS_AUTOMATIC
#define MP DDS_LIVELINESS_MANUAL_BY_PARTICIPANT
#define MT DDS_LIVELINESS_MANUAL_BY_TOPIC
CU_TheoryDataPoints(ddsc_liveliness, pmd_count) = {
    CU_DataPoints(dds_liveliness_kind_t,   A,   A,  MP,  MT), /* liveliness kind */
    CU_DataPoints(uint32_t,              200, 500, 100, 100), /* lease duration */
    CU_DataPoints(double,                 10,   5,   5,   5), /* delay (n times lease duration) */
};
#undef MT
#undef MP
#undef A

static void test_pmd_count(dds_liveliness_kind_t kind, uint32_t ldur, double mult, bool remote_reader)
{
  dds_entity_t pub_topic;
  dds_entity_t sub_topic = 0;
  dds_entity_t reader;
  dds_entity_t writer;
  ddsi_seqno_t start_seqno, end_seqno;
  dds_qos_t *rqos;
  dds_qos_t *wqos;
  dds_entity_t waitset;
  dds_attach_t triggered;
  uint32_t status;
  char name[100];

  tprintf("running test: kind %s, lease duration %"PRIu32", delay %d, %s reader\n",
          kind == 0 ? "A" : "MP", ldur, (int32_t)(mult * ldur), remote_reader ? "remote" : "local");

  /* wait for initial PMD to be sent by the participant */
  while (get_pmd_seqno(g_pub_participant) < 1)
    dds_sleepfor(DDS_MSECS(50));

  /* topics */
  create_unique_topic_name("ddsc_liveliness_pmd_count", name, sizeof name);
  CU_ASSERT_FATAL((pub_topic = dds_create_topic(g_pub_participant, &Space_Type1_desc, name, NULL, NULL)) > 0);
  if (remote_reader)
    CU_ASSERT_FATAL((sub_topic = dds_create_topic(g_sub_participant, &Space_Type1_desc, name, NULL, NULL)) > 0);

  /* reader */
  CU_ASSERT_FATAL((rqos = dds_create_qos()) != NULL);
  dds_qset_liveliness(rqos, DDS_LIVELINESS_AUTOMATIC, DDS_INFINITY);
  CU_ASSERT_FATAL((reader = dds_create_reader(remote_reader ? g_sub_participant : g_pub_participant, remote_reader ? sub_topic : pub_topic, rqos, NULL)) > 0);
  dds_delete_qos(rqos);
  CU_ASSERT_EQUAL_FATAL(dds_set_status_mask(reader, DDS_LIVELINESS_CHANGED_STATUS), DDS_RETCODE_OK);

  /* waitset on reader */
  CU_ASSERT_FATAL((waitset = dds_create_waitset(remote_reader ? g_sub_participant : g_pub_participant)) > 0);
  CU_ASSERT_EQUAL_FATAL(dds_waitset_attach(waitset, reader, reader), DDS_RETCODE_OK);

  /* writer */
  CU_ASSERT_FATAL((wqos = dds_create_qos()) != NULL);
  dds_qset_liveliness(wqos, kind, DDS_MSECS(ldur));
  CU_ASSERT_FATAL((writer = dds_create_writer(g_pub_participant, pub_topic, wqos, NULL)) > 0);
  dds_delete_qos(wqos);

  /* wait for writer to be alive */
  CU_ASSERT_EQUAL_FATAL(dds_waitset_wait(waitset, &triggered, 1, DDS_SECS(1)), 1);
  CU_ASSERT_EQUAL_FATAL(dds_take_status(reader, &status, DDS_LIVELINESS_CHANGED_STATUS), DDS_RETCODE_OK);

  /* check no of PMD messages sent */
  start_seqno = get_pmd_seqno(g_pub_participant);
  dds_sleepfor(DDS_MSECS((dds_duration_t)(mult * ldur)));
  end_seqno = get_pmd_seqno(g_pub_participant);

  tprintf("PMD sequence no: start %" PRIu64 " -> end %" PRIu64 "\n", start_seqno, end_seqno);

  /* End-start should be mult - 1 under ideal circumstances, but consider the test successful
           when at least 50% of the expected PMD's was sent. This checks that the frequency for sending
           PMDs was increased when the writer was added. */
  CU_ASSERT_FATAL((double) (end_seqno - start_seqno) >= (kind == DDS_LIVELINESS_AUTOMATIC ? (50 * (mult - 1)) / 100 : 0))
  if (kind != DDS_LIVELINESS_AUTOMATIC)
    CU_ASSERT_FATAL((double) (get_pmd_seqno(g_pub_participant) - start_seqno) < mult)

  /* cleanup */
  if (remote_reader)
    CU_ASSERT_EQUAL_FATAL(dds_delete(sub_topic), DDS_RETCODE_OK);
  CU_ASSERT_EQUAL_FATAL(dds_delete(pub_topic), DDS_RETCODE_OK);
  CU_ASSERT_EQUAL_FATAL(dds_delete(writer), DDS_RETCODE_OK);
  CU_ASSERT_EQUAL_FATAL(dds_delete(reader), DDS_RETCODE_OK);
}

CU_Theory((dds_liveliness_kind_t kind, uint32_t ldur, double mult), ddsc_liveliness, pmd_count, .init = liveliness_init, .fini = liveliness_fini, .timeout = 30)
{
  test_pmd_count(kind, ldur, mult, false);
  test_pmd_count(kind, ldur, mult, true);
}

/**
 * Test that the expected number of proxy writers expires (set to not-alive)
 * after a certain delay for various combinations of writers with different
 * liveliness kinds.
 */
CU_TheoryDataPoints(ddsc_liveliness, expire_liveliness_kinds) = {
    CU_DataPoints(uint32_t, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200), /* lease duration for initial test run (increased for each retry when test fails) */
    CU_DataPoints(double,   0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 2.1, 2.1, 2.1, 2.1, 2.1, 2.1, 2.1, 2.1, 2.1), /* delay (n times lease duration) */
    CU_DataPoints(uint32_t,   1,   0,   2,   0,   1,   0,   0,   1,   1,   2,   0,   5,   0,  15,  15), /* number of writers with automatic liveliness */
    CU_DataPoints(uint32_t,   1,   1,   2,   2,   0,   0,   0,   1,   0,   2,   2,   5,  10,   0,  15), /* number of writers with manual-by-participant liveliness */
    CU_DataPoints(uint32_t,   1,   1,   2,   2,   1,   1,   1,   1,   0,   1,   1,   2,   5,   0,  10), /* number of writers with manual-by-topic liveliness */
};

static void test_expire_liveliness_kinds(uint32_t ldur, double mult, uint32_t wr_cnt_auto, uint32_t wr_cnt_man_pp, uint32_t wr_cnt_man_tp, bool remote_reader)
{
  dds_entity_t pub_topic;
  dds_entity_t sub_topic = 0;
  dds_entity_t reader;
  dds_entity_t *writers;
  dds_qos_t *rqos, *wqos_auto, *wqos_man_pp, *wqos_man_tp;
  dds_entity_t waitset;
  dds_attach_t triggered;
  struct dds_liveliness_changed_status lstatus;
  uint32_t status, n, run = 1, wr_cnt = wr_cnt_auto + wr_cnt_man_pp + wr_cnt_man_tp;
  char name[100];
  dds_time_t tstart, t;
  bool test_finished = false;

  do
  {
    tstart = dds_time();
    tprintf("running test: lease duration %"PRIu32", delay %f, auto/man-by-part/man-by-topic %"PRIu32"/%"PRIu32"/%"PRIu32", %s reader\n",
            ldur, mult, wr_cnt_auto, wr_cnt_man_pp, wr_cnt_man_tp, remote_reader ? "remote" : "local");

    /* topics */
    create_unique_topic_name("ddsc_liveliness_expire_kinds", name, sizeof name);
    CU_ASSERT_FATAL((pub_topic = dds_create_topic(g_pub_participant, &Space_Type1_desc, name, NULL, NULL)) > 0);
    if (remote_reader)
      CU_ASSERT_FATAL((sub_topic = dds_create_topic(g_sub_participant, &Space_Type1_desc, name, NULL, NULL)) > 0);

    /* reader */
    CU_ASSERT_FATAL((rqos = dds_create_qos()) != NULL);
    dds_qset_liveliness(rqos, DDS_LIVELINESS_AUTOMATIC, DDS_INFINITY);
    CU_ASSERT_FATAL((reader = dds_create_reader(remote_reader ? g_sub_participant : g_pub_participant, remote_reader ? sub_topic : pub_topic, rqos, NULL)) > 0);
    dds_delete_qos(rqos);
    CU_ASSERT_EQUAL_FATAL(dds_set_status_mask(reader, DDS_LIVELINESS_CHANGED_STATUS), DDS_RETCODE_OK);

    /* writers */
    CU_ASSERT_FATAL((wqos_auto = dds_create_qos()) != NULL);
    dds_qset_liveliness(wqos_auto, DDS_LIVELINESS_AUTOMATIC, DDS_MSECS(ldur));
    CU_ASSERT_FATAL((wqos_man_pp = dds_create_qos()) != NULL);
    dds_qset_liveliness(wqos_man_pp, DDS_LIVELINESS_MANUAL_BY_PARTICIPANT, DDS_MSECS(ldur));
    CU_ASSERT_FATAL((wqos_man_tp = dds_create_qos()) != NULL);
    dds_qset_liveliness(wqos_man_tp, DDS_LIVELINESS_MANUAL_BY_TOPIC, DDS_MSECS(ldur));

    CU_ASSERT_FATAL((waitset = dds_create_waitset(remote_reader ? g_sub_participant : g_pub_participant)) > 0);
    CU_ASSERT_EQUAL_FATAL(dds_waitset_attach(waitset, reader, reader), DDS_RETCODE_OK);

    writers = dds_alloc(wr_cnt * sizeof(dds_entity_t));
    for (n = 0; n < wr_cnt; n++)
    {
      dds_qos_t *wqos;
      wqos = n < wr_cnt_auto ? wqos_auto : (n < (wr_cnt_auto + wr_cnt_man_pp) ? wqos_man_pp : wqos_man_tp);
      CU_ASSERT_FATAL((writers[n] = dds_create_writer(g_pub_participant, pub_topic, wqos, NULL)) > 0);
      CU_ASSERT_EQUAL_FATAL(dds_waitset_wait(waitset, &triggered, 1, DDS_SECS(5)), 1);
      CU_ASSERT_EQUAL_FATAL(dds_take_status(reader, &status, DDS_LIVELINESS_CHANGED_STATUS), DDS_RETCODE_OK);
    }
    dds_delete_qos(wqos_auto);
    dds_delete_qos(wqos_man_pp);
    dds_delete_qos(wqos_man_tp);

    t = dds_time();
    if (t - tstart > DDS_MSECS(ldur) / 2)
    {
      ldur *= 10 / (run + 1);
      tprintf("failed to create writers in time\n");
    }
    else
    {
      /* check alive count before proxy writers are expired */
      dds_get_liveliness_changed_status(reader, &lstatus);
      tprintf("writers alive: %"PRIu32"\n", lstatus.alive_count);
      CU_ASSERT_EQUAL_FATAL(lstatus.alive_count, wr_cnt);

      dds_time_t tstop = tstart + DDS_MSECS((dds_duration_t)(mult * ldur));
      uint32_t stopped = 0;
      do
      {
        dds_duration_t w = tstop - dds_time();
        CU_ASSERT_FATAL((dds_waitset_wait(waitset, &triggered, 1, w > 0 ? w : 0)) >= 0);
        CU_ASSERT_EQUAL_FATAL(dds_get_liveliness_changed_status(reader, &lstatus), DDS_RETCODE_OK);
        stopped += (uint32_t)lstatus.not_alive_count_change;
      } while (dds_time() < tstop);
      tprintf("writers stopped: %u\n", stopped);

      size_t exp_stopped = mult < 1 ? 0 : (wr_cnt_man_pp + wr_cnt_man_tp);
      size_t exp_alive = mult < 1 ? wr_cnt : wr_cnt_auto;
      CU_ASSERT_EQUAL_FATAL(dds_get_liveliness_changed_status(reader, &lstatus), DDS_RETCODE_OK);
      tprintf("writers alive: %u (exp: %u) not-alive: %u (exp %u)\n",
              lstatus.alive_count, (unsigned) exp_alive,
              lstatus.not_alive_count, (unsigned) exp_stopped);
      if (stopped == exp_stopped && lstatus.alive_count == exp_alive)
        test_finished = true;
      else
      {
        ldur *= 10 / (run + 1);
        tprintf("incorrect number of stopped/alive writers\n");
      }
    }

    /* cleanup */
    CU_ASSERT_EQUAL_FATAL(dds_waitset_detach(waitset, reader), DDS_RETCODE_OK);
    CU_ASSERT_EQUAL_FATAL(dds_delete(waitset), DDS_RETCODE_OK);

    for (n = 0; n < wr_cnt; n++)
      CU_ASSERT_EQUAL_FATAL(dds_delete(writers[n]), DDS_RETCODE_OK);
    dds_free(writers);
    if (remote_reader)
      CU_ASSERT_EQUAL_FATAL(dds_delete(sub_topic), DDS_RETCODE_OK);
    CU_ASSERT_EQUAL_FATAL(dds_delete(pub_topic), DDS_RETCODE_OK);
    CU_ASSERT_EQUAL_FATAL(dds_delete(reader), DDS_RETCODE_OK);

    if (!test_finished)
    {
      if (++run > 3)
      {
        tprintf("run limit reached, test failed\n");
        CU_FAIL_FATAL("Run limit reached");
        test_finished = true;
        continue;
      }
      else
      {
        tprintf("restarting test with ldur %"PRIu32"\n", ldur);
      }
    }
  } while (!test_finished);
}

CU_Theory((uint32_t ldur, double mult, uint32_t wr_cnt_auto, uint32_t wr_cnt_man_pp, uint32_t wr_cnt_man_tp), ddsc_liveliness, expire_liveliness_kinds, .init = liveliness_init, .fini = liveliness_fini, .timeout = 120)
{
  test_expire_liveliness_kinds (ldur, mult, wr_cnt_auto, wr_cnt_man_pp, wr_cnt_man_tp, false);
  test_expire_liveliness_kinds (ldur, mult, wr_cnt_auto, wr_cnt_man_pp, wr_cnt_man_tp, true);
}


static void add_and_check_writer(dds_liveliness_kind_t kind, dds_duration_t ldur, dds_entity_t *writer, dds_entity_t topic, dds_entity_t reader, bool remote_reader)
{
  dds_entity_t waitset;
  dds_qos_t *wqos;
  dds_attach_t triggered;
  uint32_t status;

  CU_ASSERT_FATAL((waitset = dds_create_waitset(remote_reader ? g_sub_participant : g_pub_participant)) > 0);
  CU_ASSERT_EQUAL_FATAL(dds_waitset_attach(waitset, reader, reader), DDS_RETCODE_OK);

  CU_ASSERT_FATAL((wqos = dds_create_qos()) != NULL);
  dds_qset_liveliness(wqos, kind, ldur);
  CU_ASSERT_FATAL((*writer = dds_create_writer(g_pub_participant, topic, wqos, NULL)) > 0);
  dds_delete_qos(wqos);

  /* wait for writer to be alive */
  CU_ASSERT_EQUAL_FATAL(dds_waitset_wait(waitset, &triggered, 1, DDS_MSECS(1000)), 1);
  CU_ASSERT_EQUAL_FATAL(dds_take_status(reader, &status, DDS_LIVELINESS_CHANGED_STATUS), DDS_RETCODE_OK);

  CU_ASSERT_EQUAL_FATAL(dds_waitset_detach(waitset, reader), DDS_RETCODE_OK);
  CU_ASSERT_EQUAL_FATAL(dds_delete(waitset), DDS_RETCODE_OK);
}

/**
 * Test that the correct PMD interval is set for the participant
 * based on the lease duration of the writers.
 */
#define MAX_WRITERS 10
CU_Test(ddsc_liveliness, lease_duration, .init = liveliness_init, .fini = liveliness_fini)
{
  dds_entity_t pub_topic;
  dds_entity_t sub_topic;
  dds_entity_t reader;
  dds_entity_t writers[MAX_WRITERS];
  uint32_t wr_cnt = 0;
  char name[100];
  dds_qos_t *rqos;
  uint32_t n;

  /* topics */
  create_unique_topic_name("ddsc_liveliness_ldur", name, sizeof name);
  CU_ASSERT_FATAL((pub_topic = dds_create_topic(g_pub_participant, &Space_Type1_desc, name, NULL, NULL)) > 0);
  CU_ASSERT_FATAL((sub_topic = dds_create_topic(g_sub_participant, &Space_Type1_desc, name, NULL, NULL)) > 0);

  /* reader and waitset */
  CU_ASSERT_FATAL((rqos = dds_create_qos()) != NULL);
  dds_qset_liveliness(rqos, DDS_LIVELINESS_AUTOMATIC, DDS_INFINITY);
  CU_ASSERT_FATAL((reader = dds_create_reader(g_sub_participant, sub_topic, rqos, NULL)) > 0);
  dds_delete_qos(rqos);
  CU_ASSERT_EQUAL_FATAL(dds_set_status_mask(reader, DDS_LIVELINESS_CHANGED_STATUS), DDS_RETCODE_OK);

  /* check if pmd defaults to configured duration */
  CU_ASSERT_EQUAL_FATAL(get_pmd_interval(g_pub_participant), get_ldur_config(g_pub_participant));

  /* create writers and check pmd interval in publishing participant */
  add_and_check_writer(DDS_LIVELINESS_AUTOMATIC, DDS_MSECS(1000), &writers[wr_cnt++], pub_topic, reader, true);
  CU_ASSERT_EQUAL_FATAL(get_pmd_interval(g_pub_participant), DDS_MSECS(1000));

  add_and_check_writer(DDS_LIVELINESS_AUTOMATIC, DDS_MSECS(2000), &writers[wr_cnt++], pub_topic, reader, true);
  CU_ASSERT_EQUAL_FATAL(get_pmd_interval(g_pub_participant), DDS_MSECS(1000));

  add_and_check_writer(DDS_LIVELINESS_MANUAL_BY_PARTICIPANT, DDS_MSECS(2000), &writers[wr_cnt++], pub_topic, reader, true);
  CU_ASSERT_EQUAL_FATAL(get_pmd_interval(g_pub_participant), DDS_MSECS(1000));

  add_and_check_writer(DDS_LIVELINESS_AUTOMATIC, DDS_MSECS(500), &writers[wr_cnt++], pub_topic, reader, true);
  CU_ASSERT_EQUAL_FATAL(get_pmd_interval(g_pub_participant), DDS_MSECS(500));

  add_and_check_writer(DDS_LIVELINESS_MANUAL_BY_PARTICIPANT, DDS_MSECS(100), &writers[wr_cnt++], pub_topic, reader, true);
  CU_ASSERT_EQUAL_FATAL(get_pmd_interval(g_pub_participant), DDS_MSECS(500));

  add_and_check_writer(DDS_LIVELINESS_MANUAL_BY_TOPIC, DDS_MSECS(100), &writers[wr_cnt++], pub_topic, reader, true);
  CU_ASSERT_EQUAL_FATAL(get_pmd_interval(g_pub_participant), DDS_MSECS(500));

  /* cleanup */
  for (n = 0; n < wr_cnt; n++)
    CU_ASSERT_EQUAL_FATAL(dds_delete(writers[n]), DDS_RETCODE_OK);
  CU_ASSERT_EQUAL_FATAL(dds_delete(sub_topic), DDS_RETCODE_OK);
  CU_ASSERT_EQUAL_FATAL(dds_delete(pub_topic), DDS_RETCODE_OK);
  CU_ASSERT_EQUAL_FATAL(dds_delete(reader), DDS_RETCODE_OK);
}
#undef MAX_WRITERS

/**
 * Check that the correct lease duration is set in the matched
 * publications in the readers. */
static void test_lease_duration_pwr(bool remote_reader)
{
  dds_entity_t pub_topic;
  dds_entity_t sub_topic = 0;
  dds_entity_t reader;
  dds_entity_t writer;
  char name[100];
  dds_qos_t *rqos, *wqos;
  dds_entity_t waitset;
  dds_attach_t triggered;
  uint32_t status;
  dds_duration_t ldur;

  tprintf("running test lease_duration_pwr: %s reader\n", remote_reader ? "remote" : "local");

  /* topics */
  create_unique_topic_name("ddsc_liveliness_ldurpwr", name, sizeof name);
  CU_ASSERT_FATAL((pub_topic = dds_create_topic(g_pub_participant, &Space_Type1_desc, name, NULL, NULL)) > 0);
  if (remote_reader)
    CU_ASSERT_FATAL((sub_topic = dds_create_topic(g_sub_participant, &Space_Type1_desc, name, NULL, NULL)) > 0);

  /* reader */
  CU_ASSERT_FATAL((rqos = dds_create_qos()) != NULL);
  dds_qset_liveliness(rqos, DDS_LIVELINESS_AUTOMATIC, DDS_INFINITY);
  CU_ASSERT_FATAL((reader = dds_create_reader(remote_reader ? g_sub_participant : g_pub_participant, remote_reader ? sub_topic : pub_topic, rqos, NULL)) > 0);
  dds_delete_qos(rqos);
  CU_ASSERT_EQUAL_FATAL(dds_set_status_mask(reader, DDS_LIVELINESS_CHANGED_STATUS), DDS_RETCODE_OK);

  /* writer */
  ldur = 1000;
  CU_ASSERT_FATAL((wqos = dds_create_qos()) != NULL);
  dds_qset_liveliness(wqos, DDS_LIVELINESS_AUTOMATIC, DDS_MSECS(ldur));
  CU_ASSERT_FATAL((writer = dds_create_writer(g_pub_participant, pub_topic, wqos, NULL)) > 0);

  /* wait for writer to be alive */
  CU_ASSERT_FATAL((waitset = dds_create_waitset(remote_reader ? g_sub_participant : g_pub_participant)) > 0);
  CU_ASSERT_EQUAL_FATAL(dds_waitset_attach(waitset, reader, reader), DDS_RETCODE_OK);
  CU_ASSERT_EQUAL_FATAL(dds_waitset_wait(waitset, &triggered, 1, DDS_MSECS(1000)), 1);
  CU_ASSERT_EQUAL_FATAL(dds_take_status(reader, &status, DDS_LIVELINESS_CHANGED_STATUS), DDS_RETCODE_OK);

  /* check pwr lease duration in matched publication */
  dds_instance_handle_t wrs[1];
  CU_ASSERT_EQUAL_FATAL(dds_get_matched_publications(reader, wrs, 1), 1);
  dds_builtintopic_endpoint_t *ep;
  ep = dds_get_matched_publication_data(reader, wrs[0]);
  CU_ASSERT_FATAL(ep != NULL);
  assert(ep != NULL); /* for Clang's static analyzer */
  CU_ASSERT_EQUAL_FATAL(ep->qos->liveliness.lease_duration, DDS_MSECS(ldur));
  dds_builtintopic_free_endpoint (ep);

  /* cleanup */
  dds_delete_qos(wqos);
  CU_ASSERT_EQUAL_FATAL(dds_waitset_detach(waitset, reader), DDS_RETCODE_OK);
  CU_ASSERT_EQUAL_FATAL(dds_delete(waitset), DDS_RETCODE_OK);
  CU_ASSERT_EQUAL_FATAL(dds_delete(writer), DDS_RETCODE_OK);
  if (remote_reader)
    CU_ASSERT_EQUAL_FATAL(dds_delete(sub_topic), DDS_RETCODE_OK);
  CU_ASSERT_EQUAL_FATAL(dds_delete(pub_topic), DDS_RETCODE_OK);
  CU_ASSERT_EQUAL_FATAL(dds_delete(reader), DDS_RETCODE_OK);
}

CU_Test(ddsc_liveliness, lease_duration_pwr, .init = liveliness_init, .fini = liveliness_fini)
{
  test_lease_duration_pwr(false);
  test_lease_duration_pwr(true);
}

/**
 * Create a relative large number of writers with liveliness kinds automatic and
 * manual-by-participant and with decreasing lease duration, and check that all
 * writers become alive. During the writer creation loop, every third writer
 * is deleted immediately after creating.
 */
#define MAX_WRITERS 100

static void test_create_delete_writer_stress(bool remote_reader)
{
  dds_entity_t pub_topic;
  dds_entity_t sub_topic = 0;
  dds_entity_t reader;
  dds_entity_t writers[MAX_WRITERS];
  dds_entity_t waitset;
  dds_qos_t *wqos;
  struct dds_liveliness_changed_status lstatus;
  uint32_t alive_writers_auto = 0, alive_writers_man = 0;
  char name[100];
  dds_qos_t *rqos;
  dds_attach_t triggered;
  uint32_t n;
  Space_Type1 sample = {0, 0, 0};
  int64_t ldur = 1000;

  tprintf("running test create_delete_writer_stress: %s reader\n", remote_reader ? "remote" : "local");

  /* topics */
  create_unique_topic_name("ddsc_liveliness_wr_stress", name, sizeof name);
  CU_ASSERT_FATAL((pub_topic = dds_create_topic(g_pub_participant, &Space_Type1_desc, name, NULL, NULL)) > 0);
  if (remote_reader)
    CU_ASSERT_FATAL((sub_topic = dds_create_topic(g_sub_participant, &Space_Type1_desc, name, NULL, NULL)) > 0);

  /* reader and waitset */
  CU_ASSERT_FATAL((rqos = dds_create_qos()) != NULL);
  dds_qset_liveliness(rqos, DDS_LIVELINESS_AUTOMATIC, DDS_INFINITY);
  CU_ASSERT_FATAL((reader = dds_create_reader(remote_reader ? g_sub_participant : g_pub_participant, remote_reader ? sub_topic : pub_topic, rqos, NULL)) > 0);
  dds_delete_qos(rqos);
  CU_ASSERT_EQUAL_FATAL(dds_set_status_mask(reader, DDS_LIVELINESS_CHANGED_STATUS), DDS_RETCODE_OK);
  CU_ASSERT_FATAL((waitset = dds_create_waitset(remote_reader ? g_sub_participant : g_pub_participant)) > 0);
  CU_ASSERT_EQUAL_FATAL(dds_waitset_attach(waitset, reader, reader), DDS_RETCODE_OK);

  /* create 1st writer and wait for it to become alive */
  CU_ASSERT_FATAL((wqos = dds_create_qos()) != NULL);
  dds_qset_liveliness(wqos, DDS_LIVELINESS_MANUAL_BY_PARTICIPANT, DDS_MSECS(ldur));
  CU_ASSERT_FATAL((writers[0] = dds_create_writer(g_pub_participant, pub_topic, wqos, NULL)) > 0);
  CU_ASSERT_EQUAL_FATAL(dds_waitset_wait(waitset, &triggered, 1, DDS_MSECS(1000)), 1);
  alive_writers_man++;

  /* create writers */
  for (n = 1; n < MAX_WRITERS; n++)
  {
    dds_qset_liveliness(wqos, (n % 2) ? DDS_LIVELINESS_AUTOMATIC : DDS_LIVELINESS_MANUAL_BY_PARTICIPANT, DDS_MSECS((n % 3) ? ldur + n : ldur - n) + ((n % 3) == 2 ? 1 : 0));
    CU_ASSERT_FATAL((writers[n] = dds_create_writer(g_pub_participant, pub_topic, wqos, NULL)) > 0);
    CU_ASSERT_EQUAL_FATAL(dds_write(writers[n], &sample), DDS_RETCODE_OK);
    if (n % 3 == 2)
      dds_delete(writers[n]);
    else if (n % 2)
      alive_writers_auto++;
    else
      alive_writers_man++;
  }
  dds_delete_qos(wqos);
  tprintf("%"PRId64" alive_writers_auto: %"PRIu32", alive_writers_man: %"PRIu32"\n", dds_time(), alive_writers_auto, alive_writers_man);

  /* wait for auto liveliness writers to become alive and manual-by-pp writers to become not-alive */
  do
  {
    CU_ASSERT_EQUAL_FATAL(dds_get_liveliness_changed_status(reader, &lstatus), DDS_RETCODE_OK);
    tprintf("%"PRId64" alive: %"PRIu32", not-alive: %"PRIu32"\n", dds_time(), lstatus.alive_count, lstatus.not_alive_count);
    dds_sleepfor(DDS_MSECS(50));
  } while (lstatus.alive_count != alive_writers_auto || lstatus.not_alive_count != alive_writers_man);

  /* check that counts are stable after a delay */
  tprintf("%"PRId64" wait for half ldur (%"PRId64"ms)\n", dds_time(), ldur);
  dds_sleepfor(DDS_MSECS(ldur / 2));
  CU_ASSERT_EQUAL_FATAL(dds_get_liveliness_changed_status(reader, &lstatus), DDS_RETCODE_OK);
  tprintf("%"PRId64" alive: %"PRIu32", not-alive: %"PRIu32"\n", dds_time(), lstatus.alive_count, lstatus.not_alive_count);
  CU_ASSERT_FATAL(lstatus.alive_count == alive_writers_auto && lstatus.not_alive_count == alive_writers_man);

  /* cleanup remaining writers */
  for (n = 0; n < MAX_WRITERS; n++)
  {
    if (n % 3 != 2)
      CU_ASSERT_EQUAL_FATAL(dds_delete(writers[n]), DDS_RETCODE_OK);
  }
  /* wait for alive_count and not_alive_count to become 0 */
  do
  {
    CU_ASSERT_EQUAL_FATAL(dds_get_liveliness_changed_status(reader, &lstatus), DDS_RETCODE_OK);
    tprintf("%"PRId64" alive: %"PRIu32", not: %"PRIu32"\n", dds_time(), lstatus.alive_count, lstatus.not_alive_count);
    dds_sleepfor(DDS_MSECS(ldur / 10));
  } while (lstatus.alive_count > 0 || lstatus.not_alive_count > 0);
  CU_ASSERT_EQUAL_FATAL(dds_waitset_detach(waitset, reader), DDS_RETCODE_OK);
  CU_ASSERT_EQUAL_FATAL(dds_delete(waitset), DDS_RETCODE_OK);
  CU_ASSERT_EQUAL_FATAL(dds_delete(reader), DDS_RETCODE_OK);
  if (remote_reader)
    CU_ASSERT_EQUAL_FATAL(dds_delete(sub_topic), DDS_RETCODE_OK);
  CU_ASSERT_EQUAL_FATAL(dds_delete(pub_topic), DDS_RETCODE_OK);
}

CU_Test(ddsc_liveliness, create_delete_writer_stress, .init = liveliness_init, .fini = liveliness_fini, .timeout = 15)
{
  test_create_delete_writer_stress(false);
  test_create_delete_writer_stress(true);
}
#undef MAX_WRITERS

/**
 * Check the counts in liveliness_changed_status result.
 */
static void test_status_counts(bool remote_reader)
{
  dds_entity_t pub_topic;
  dds_entity_t sub_topic = 0;
  dds_entity_t reader;
  dds_entity_t writer;
  dds_entity_t waitset;
  dds_qos_t *rqos;
  dds_qos_t *wqos;
  dds_attach_t triggered;
  struct dds_liveliness_changed_status lcstatus;
  struct dds_liveliness_lost_status llstatus;
  struct dds_subscription_matched_status sstatus;
  char name[100];
  dds_duration_t ldur = DDS_MSECS(500);
  Space_Type1 sample = {1, 0, 0};

  tprintf("running test status_counts: %s reader\n", remote_reader ? "remote" : "local");

  /* topics */
  create_unique_topic_name("ddsc_liveliness_status_counts", name, sizeof name);
  CU_ASSERT_FATAL((pub_topic = dds_create_topic(g_pub_participant, &Space_Type1_desc, name, NULL, NULL)) > 0);
  if (remote_reader)
    CU_ASSERT_FATAL((sub_topic = dds_create_topic(g_sub_participant, &Space_Type1_desc, name, NULL, NULL)) > 0);

  /* reader */
  CU_ASSERT_FATAL((rqos = dds_create_qos()) != NULL);
  dds_qset_liveliness(rqos, DDS_LIVELINESS_AUTOMATIC, DDS_INFINITY);
  CU_ASSERT_FATAL((reader = dds_create_reader(remote_reader ? g_sub_participant : g_pub_participant, remote_reader ? sub_topic : pub_topic, rqos, NULL)) > 0);
  dds_delete_qos(rqos);
  CU_ASSERT_EQUAL_FATAL(dds_set_status_mask(reader, DDS_LIVELINESS_CHANGED_STATUS), DDS_RETCODE_OK);
  CU_ASSERT_FATAL((waitset = dds_create_waitset(remote_reader ? g_sub_participant : g_pub_participant)) > 0);
  CU_ASSERT_EQUAL_FATAL(dds_waitset_attach(waitset, reader, reader), DDS_RETCODE_OK);

  /* writer */
  CU_ASSERT_FATAL((wqos = dds_create_qos()) != NULL);
  dds_qset_liveliness(wqos, DDS_LIVELINESS_MANUAL_BY_PARTICIPANT, ldur);
  CU_ASSERT_FATAL((writer = dds_create_writer(g_pub_participant, pub_topic, wqos, NULL)) > 0);
  dds_delete_qos(wqos);
  CU_ASSERT_EQUAL_FATAL(dds_set_status_mask(writer, DDS_LIVELINESS_LOST_STATUS), DDS_RETCODE_OK);

  /* wait for writer to be alive */
  CU_ASSERT_EQUAL_FATAL(dds_waitset_wait(waitset, &triggered, 1, DDS_SECS(5)), 1);

  /* check status counts before proxy writer is expired */
  dds_get_liveliness_changed_status(reader, &lcstatus);
  CU_ASSERT_EQUAL_FATAL(lcstatus.alive_count, 1);
  dds_get_subscription_matched_status(reader, &sstatus);
  CU_ASSERT_EQUAL_FATAL(sstatus.current_count, 1);
  dds_get_liveliness_lost_status(writer, &llstatus);
  CU_ASSERT_EQUAL_FATAL(llstatus.total_count, 0);

  /* sleep for more than lease duration, writer should be set not-alive but subscription still matched */
  dds_sleepfor(ldur + DDS_MSECS(100));
  CU_ASSERT_EQUAL_FATAL(dds_waitset_wait(waitset, &triggered, 1, DDS_SECS(5)), 1);

  dds_get_liveliness_changed_status(reader, &lcstatus);
  CU_ASSERT_EQUAL_FATAL(lcstatus.alive_count, 0);
  dds_get_subscription_matched_status(reader, &sstatus);
  CU_ASSERT_EQUAL_FATAL(sstatus.current_count, 1);
  dds_get_liveliness_lost_status(writer, &llstatus);
  CU_ASSERT_EQUAL_FATAL(llstatus.total_count, 1);
  CU_ASSERT_EQUAL_FATAL(llstatus.total_count_change, 1);

  /* write sample and re-check status counts */
  CU_ASSERT_EQUAL_FATAL(dds_write(writer, &sample), DDS_RETCODE_OK);
  CU_ASSERT_EQUAL_FATAL(dds_waitset_wait(waitset, &triggered, 1, DDS_SECS(5)), 1);

  dds_get_liveliness_changed_status(reader, &lcstatus);
  CU_ASSERT_EQUAL_FATAL(lcstatus.alive_count, 1);
  dds_get_subscription_matched_status(reader, &sstatus);
  CU_ASSERT_EQUAL_FATAL(sstatus.current_count, 1);
  dds_get_liveliness_lost_status(writer, &llstatus);
  CU_ASSERT_EQUAL_FATAL(llstatus.total_count_change, 0);

  /* cleanup */
  CU_ASSERT_EQUAL_FATAL(dds_waitset_detach(waitset, reader), DDS_RETCODE_OK);
  CU_ASSERT_EQUAL_FATAL(dds_delete(waitset), DDS_RETCODE_OK);
  CU_ASSERT_EQUAL_FATAL(dds_delete(reader), DDS_RETCODE_OK);
  CU_ASSERT_EQUAL_FATAL(dds_delete(writer), DDS_RETCODE_OK);
  if (remote_reader)
    CU_ASSERT_EQUAL_FATAL(dds_delete(sub_topic), DDS_RETCODE_OK);
  CU_ASSERT_EQUAL_FATAL(dds_delete(pub_topic), DDS_RETCODE_OK);
}

CU_Test(ddsc_liveliness, status_counts, .init = liveliness_init, .fini = liveliness_fini)
{
  test_status_counts(false);
  test_status_counts(true);
}

/**
 * Test that dds_assert_liveliness works as expected for liveliness
 * kinds manual-by-participant and manual-by-topic.
 */
#define MAX_WRITERS 100
CU_TheoryDataPoints(ddsc_liveliness, assert_liveliness) = {
    CU_DataPoints(uint32_t, 1, 0, 0, 1, 0, 1, 2), /* number of writers with automatic liveliness */
    CU_DataPoints(uint32_t, 0, 1, 0, 1, 1, 0, 2), /* number of writers with manual-by-participant liveliness */
    CU_DataPoints(uint32_t, 0, 0, 1, 1, 2, 2, 0), /* number of writers with manual-by-topic liveliness */
};

static void test_assert_liveliness(uint32_t wr_cnt_auto, uint32_t wr_cnt_man_pp, uint32_t wr_cnt_man_tp, bool remote_reader)
{
  dds_entity_t pub_topic, sub_topic = 0, reader, writers[MAX_WRITERS];
  dds_qos_t *rqos;
  struct dds_liveliness_changed_status lstatus;
  char name[100];
  uint32_t ldur = 100, wr_cnt, run = 1, stopped;
  dds_time_t tstart, tstop, t;
  bool test_finished = false;

  do
  {
    wr_cnt = 0;
    assert(wr_cnt_auto + wr_cnt_man_pp + wr_cnt_man_tp < MAX_WRITERS);
    tprintf("running test assert_liveliness: auto/man-by-part/man-by-topic %"PRIu32"/%"PRIu32"/%"PRIu32" with ldur %"PRIu32", %s reader\n",
            wr_cnt_auto, wr_cnt_man_pp, wr_cnt_man_tp, ldur, remote_reader ? "remote" : "local");

    /* topics */
    create_unique_topic_name("ddsc_liveliness_assert", name, sizeof name);
    CU_ASSERT_FATAL((pub_topic = dds_create_topic(g_pub_participant, &Space_Type1_desc, name, NULL, NULL)) > 0);
    if (remote_reader)
      CU_ASSERT_FATAL((sub_topic = dds_create_topic(g_sub_participant, &Space_Type1_desc, name, NULL, NULL)) > 0);

    /* reader */
    CU_ASSERT_FATAL((rqos = dds_create_qos()) != NULL);
    dds_qset_liveliness(rqos, DDS_LIVELINESS_AUTOMATIC, DDS_INFINITY);
    CU_ASSERT_FATAL((reader = dds_create_reader(remote_reader ? g_sub_participant : g_pub_participant, remote_reader ? sub_topic : pub_topic, rqos, NULL)) > 0);
    dds_delete_qos(rqos);
    CU_ASSERT_EQUAL_FATAL(dds_set_status_mask(reader, DDS_LIVELINESS_CHANGED_STATUS), DDS_RETCODE_OK);

    /* writers */
    for (size_t n = 0; n < wr_cnt_auto; n++)
      add_and_check_writer(DDS_LIVELINESS_AUTOMATIC, DDS_MSECS(ldur), &writers[wr_cnt++], pub_topic, reader, remote_reader);
    tstart = dds_time();
    for (size_t n = 0; n < wr_cnt_man_pp; n++)
      add_and_check_writer(DDS_LIVELINESS_MANUAL_BY_PARTICIPANT, DDS_MSECS(ldur), &writers[wr_cnt++], pub_topic, reader, remote_reader);
    for (size_t n = 0; n < wr_cnt_man_tp; n++)
      add_and_check_writer(DDS_LIVELINESS_MANUAL_BY_TOPIC, DDS_MSECS(ldur), &writers[wr_cnt++], pub_topic, reader, remote_reader);
    t = dds_time();
    if (t - tstart > DDS_MSECS(ldur) / 2)
    {
      ldur *= 10 / (run + 1);
      tprintf("failed to create writers with non-automatic liveliness kind in time\n");
    }
    else
    {
      /* check status counts before proxy writer is expired */
      dds_get_liveliness_changed_status(reader, &lstatus);
      CU_ASSERT_EQUAL_FATAL(lstatus.alive_count, wr_cnt_auto + wr_cnt_man_pp + wr_cnt_man_tp);

      /* delay for more than lease duration and assert liveliness on writers:
                        all writers (including man-by-pp) should be kept alive */
      tstop = dds_time() + 4 * DDS_MSECS(ldur) / 3;
      stopped = 0;
      do
      {
        for (size_t n = wr_cnt_auto; n < wr_cnt; n++)
          CU_ASSERT_EQUAL_FATAL(dds_assert_liveliness(writers[n]), DDS_RETCODE_OK);
        CU_ASSERT_EQUAL_FATAL(dds_get_liveliness_changed_status(reader, &lstatus), DDS_RETCODE_OK);
        stopped += (uint32_t)lstatus.not_alive_count_change;
        dds_sleepfor(DDS_MSECS(50));
      } while (dds_time() < tstop);
      CU_ASSERT_EQUAL_FATAL(dds_get_liveliness_changed_status(reader, &lstatus), DDS_RETCODE_OK);
      tprintf("writers alive with dds_assert_liveliness on all writers: %"PRIu32", writers stopped: %"PRIu32"\n", lstatus.alive_count, stopped);
      if (lstatus.alive_count != wr_cnt_auto + wr_cnt_man_pp + wr_cnt_man_tp || stopped != 0)
      {
        ldur *= 10 / (run + 1);
        tprintf("incorrect number of writers alive or stopped writers\n");
      }
      else
      {
        /* delay for more than lease duration and assert liveliness on participant:
                                writers with liveliness man-by-pp should be kept alive, man-by-topic writers
                                should stop */
        tstop = dds_time() + 4 * DDS_MSECS(ldur) / 3;
        stopped = 0;
        do
        {
          dds_assert_liveliness(g_pub_participant);
          CU_ASSERT_EQUAL_FATAL(dds_get_liveliness_changed_status(reader, &lstatus), DDS_RETCODE_OK);
          stopped += (uint32_t)lstatus.not_alive_count_change;
          dds_sleepfor(DDS_MSECS(50));
        } while (dds_time() < tstop);
        dds_get_liveliness_changed_status(reader, &lstatus);
        tprintf("writers alive with dds_assert_liveliness on participant: %"PRIu32", writers stopped: %"PRIu32"\n", lstatus.alive_count, stopped);
        if (lstatus.alive_count != wr_cnt_auto + wr_cnt_man_pp || stopped != wr_cnt_man_tp)
        {
          ldur *= 10 / (run + 1);
          tprintf("incorrect number of writers alive or stopped writers\n");
        }
        else
        {
          test_finished = true;
        }
      }
    }

    /* cleanup */
    CU_ASSERT_EQUAL_FATAL(dds_delete(reader), DDS_RETCODE_OK);
    for (size_t n = 0; n < wr_cnt; n++)
      CU_ASSERT_EQUAL_FATAL(dds_delete(writers[n]), DDS_RETCODE_OK);
    if (remote_reader)
      CU_ASSERT_EQUAL_FATAL(dds_delete(sub_topic), DDS_RETCODE_OK);
    CU_ASSERT_EQUAL_FATAL(dds_delete(pub_topic), DDS_RETCODE_OK);

    if (!test_finished)
    {
      if (++run > 3)
      {
        CU_FAIL_FATAL("Run limit reached");
        test_finished = true;
        continue;
      }
      else
      {
        tprintf("restarting test with ldur %"PRIu32"\n", ldur);
      }
    }
  } while (!test_finished);
}

CU_Theory((uint32_t wr_cnt_auto, uint32_t wr_cnt_man_pp, uint32_t wr_cnt_man_tp), ddsc_liveliness, assert_liveliness, .init = liveliness_init, .fini = liveliness_fini, .timeout = 60)
{
  test_assert_liveliness(wr_cnt_auto, wr_cnt_man_pp, wr_cnt_man_tp, false);
  test_assert_liveliness(wr_cnt_auto, wr_cnt_man_pp, wr_cnt_man_tp, true);
}
#undef MAX_WRITERS

/**
 * Check that manual-by-participant/topic writers with lease duration 0ns and 1ns work.
 */
struct liveliness_changed_state {
  ddsrt_mutex_t lock;
  dds_instance_handle_t w0_handle;
  bool weirdness;
  uint32_t w0_alive, w0_not_alive;
};

static void liveliness_changed_listener (dds_entity_t rd, const dds_liveliness_changed_status_t status, void *arg)
{
  struct liveliness_changed_state *st = arg;
  (void) rd;

  ddsrt_mutex_lock (&st->lock);
  if (status.last_publication_handle != st->w0_handle)
  {
    if (st->w0_handle == 0)
    {
      tprintf ("liveliness_changed_listener: w0 = %"PRIx64"\n", status.last_publication_handle);
      st->w0_handle = status.last_publication_handle;
    }
    else
    {
      tprintf ("liveliness_changed_listener: too many writer handles\n");
      st->weirdness = true;
    }
  }

  if (status.alive_count_change != 0 || status.not_alive_count_change != 0)
  {
    switch (status.alive_count_change)
    {
      case -1:
        break;
      case 1:
        if (status.last_publication_handle == st->w0_handle)
          st->w0_alive++;
        else
        {
          tprintf ("liveliness_changed_listener: alive_count_change = %d: unrecognized writer\n", status.alive_count_change);
          st->weirdness = true;
        }
        break;
      default:
        tprintf ("liveliness_changed_listener: alive_count_change = %d\n", status.alive_count_change);
        st->weirdness = true;
    }

    switch (status.not_alive_count_change)
    {
      case -1:
        break;
      case 1:
        if (status.last_publication_handle == st->w0_handle)
          st->w0_not_alive++;
        else
        {
          tprintf ("liveliness_changed_listener: not_alive_count_change = %d: unrecognized writer\n", status.not_alive_count_change);
          st->weirdness = true;
        }
        break;
      default:
        tprintf ("liveliness_changed_listener: not_alive_count_change = %d\n", status.not_alive_count_change);
        st->weirdness = true;
    }
  }
  else
  {
    tprintf ("liveliness_changed_listener: alive_count_change = 0 && not_alive_count_change = 0\n");
    st->weirdness = true;
  }
  ddsrt_mutex_unlock (&st->lock);
}

#define STATUS_UNSYNCED 0
#define STATUS_SYNCED 1
#define STATUS_DATA 2
static unsigned get_and_check_status (dds_entity_t reader, dds_entity_t writer_active)
{
  struct dds_liveliness_changed_status lstatus;
  struct dds_subscription_matched_status sstatus;
  struct dds_publication_matched_status pstatus;
  uint32_t dstatus;
  uint32_t result = STATUS_UNSYNCED;
  dds_return_t rc;
  rc = dds_get_subscription_matched_status(reader, &sstatus);
  CU_ASSERT_FATAL(rc == DDS_RETCODE_OK);
  rc = dds_get_liveliness_changed_status(reader, &lstatus);
  CU_ASSERT_FATAL(rc == DDS_RETCODE_OK);
  rc = dds_take_status(reader, &dstatus, DDS_DATA_AVAILABLE_STATUS);
  CU_ASSERT_EQUAL_FATAL(rc, DDS_RETCODE_OK);
  rc = dds_get_publication_matched_status(writer_active, &pstatus);
  CU_ASSERT_FATAL(rc == DDS_RETCODE_OK);
  CU_ASSERT_FATAL(lstatus.alive_count + lstatus.not_alive_count <= 2);
  tprintf ("sub %d | alive %d | not-alive %d | pub %d | data %d\n", (int)sstatus.current_count, (int)lstatus.alive_count, (int)lstatus.not_alive_count, (int)pstatus.current_count, dstatus != 0);
  if (dstatus)
    result |= STATUS_DATA;
  if (sstatus.current_count == 2 && lstatus.not_alive_count == 2 && pstatus.current_count == 1)
    result |= STATUS_SYNCED;
  return result;
}

static void setup_reader_zero_or_one (dds_entity_t *reader, dds_entity_t *writer_active, dds_entity_t *waitset, dds_liveliness_kind_t lkind, dds_duration_t ldur, bool remote_reader, struct liveliness_changed_state *listener_state)
{
  dds_entity_t pub_topic;
  dds_entity_t sub_topic = 0;
  dds_entity_t writer_inactive; /* not writing, liveliness should still toggle */
  dds_qos_t *qos;
  dds_return_t rc;
  char name[100];

  *waitset = dds_create_waitset(DDS_CYCLONEDDS_HANDLE);
  CU_ASSERT_FATAL(*waitset > 0);

  qos = dds_create_qos();
  CU_ASSERT_FATAL(qos != NULL);
  dds_qset_reliability(qos, DDS_RELIABILITY_RELIABLE, DDS_INFINITY);
  dds_qset_history(qos, DDS_HISTORY_KEEP_ALL, 0);

  create_unique_topic_name("ddsc_liveliness_lease_duration_zero", name, sizeof name);
  pub_topic = dds_create_topic(g_pub_participant, &Space_Type1_desc, name, qos, NULL);
  CU_ASSERT_FATAL(pub_topic > 0);
  if (remote_reader)
  {
    sub_topic = dds_create_topic(g_sub_participant, &Space_Type1_desc, name, qos, NULL);
    CU_ASSERT_FATAL(sub_topic > 0);
  }

  /* reader liveliness is always automatic/infinity */
  dds_qset_liveliness(qos, DDS_LIVELINESS_AUTOMATIC, DDS_INFINITY);
  *reader = dds_create_reader(remote_reader ? g_sub_participant : g_pub_participant, remote_reader ? sub_topic : pub_topic, qos, NULL);
  CU_ASSERT_FATAL(*reader > 0);
  rc = dds_set_status_mask(*reader, DDS_LIVELINESS_CHANGED_STATUS | DDS_SUBSCRIPTION_MATCHED_STATUS | DDS_DATA_AVAILABLE_STATUS);
  CU_ASSERT_FATAL(rc == DDS_RETCODE_OK);
  rc = dds_waitset_attach(*waitset, *reader, *reader);
  CU_ASSERT_FATAL(rc == DDS_RETCODE_OK);

  /* writer liveliness varies */
  dds_qset_liveliness(qos, lkind, ldur);
  *writer_active = dds_create_writer(g_pub_participant, pub_topic, qos, NULL);
  CU_ASSERT_FATAL(*writer_active > 0);
  writer_inactive = dds_create_writer(g_pub_participant, pub_topic, qos, NULL);
  CU_ASSERT_FATAL(writer_inactive > 0);
  rc = dds_set_status_mask(*writer_active, DDS_PUBLICATION_MATCHED_STATUS);
  CU_ASSERT_FATAL(rc == DDS_RETCODE_OK);
  rc = dds_waitset_attach(*waitset, *writer_active, *writer_active);
  CU_ASSERT_FATAL(rc == DDS_RETCODE_OK);

  dds_delete_qos(qos);

  /* wait for writers to be discovered and to have lost their liveliness, and for
     writer_active to have discovered the reader */
  unsigned status = STATUS_UNSYNCED;
  bool initial_sample_written = false, initial_sample_received = false;
  do
  {
    status = get_and_check_status (*reader, *writer_active);
    if (status & STATUS_DATA)
      initial_sample_received = true;
    if (status & STATUS_SYNCED && !initial_sample_written)
    {
      Space_Type1 sample = {1, 0, 0};
      rc = dds_write(*writer_active, &sample);
      CU_ASSERT_FATAL(rc == DDS_RETCODE_OK);
      initial_sample_written = true;
    }
    if (status & STATUS_SYNCED && initial_sample_received)
      break;

    rc = dds_waitset_wait(*waitset, NULL, 0, DDS_SECS(5));
    if (rc < 1)
    {
      get_and_check_status (*reader, *writer_active);
      CU_ASSERT_FATAL(rc >= 1);
    }
  } while (1);

  /* switch to using a listener: those allow us to observe all events */
  dds_listener_t *listener;
  listener = dds_create_listener (listener_state);
  dds_lset_liveliness_changed(listener, liveliness_changed_listener);
  rc = dds_set_listener (*reader, listener);
  CU_ASSERT_FATAL(rc == DDS_RETCODE_OK);
  dds_delete_listener (listener);
}

static void wait_for_notalive (dds_entity_t reader, struct liveliness_changed_state *listener_state)
{
  struct dds_liveliness_changed_status lstatus;
  int retries = 100;
  dds_return_t rc;
  rc = dds_get_liveliness_changed_status(reader, &lstatus);
  CU_ASSERT_FATAL(rc == DDS_RETCODE_OK);
  tprintf("early liveliness changed status: alive %"PRIu32" not-alive %"PRIu32"\n", lstatus.alive_count, lstatus.not_alive_count);

  ddsrt_mutex_lock (&listener_state->lock);
  tprintf("early w0 %"PRIx64" alive %"PRIu32" not-alive %"PRIu32"\n", listener_state->w0_handle, listener_state->w0_alive, listener_state->w0_not_alive);
  CU_ASSERT_FATAL(!listener_state->weirdness);
  CU_ASSERT_FATAL(listener_state->w0_handle != 0);
  while (listener_state->w0_not_alive < listener_state->w0_alive && retries-- > 0)
  {
    ddsrt_mutex_unlock(&listener_state->lock);
    dds_sleepfor(DDS_MSECS(10));
    rc = dds_get_liveliness_changed_status(reader, &lstatus);
    CU_ASSERT_FATAL(rc == DDS_RETCODE_OK);
    ddsrt_mutex_lock(&listener_state->lock);
  }

  tprintf("late liveliness changed status: alive %"PRIu32" not-alive %"PRIu32"\n", lstatus.alive_count, lstatus.not_alive_count);
  tprintf("final w0 %"PRIx64" alive %"PRIu32" not-alive %"PRIu32"\n", listener_state->w0_handle, listener_state->w0_alive, listener_state->w0_not_alive);
  CU_ASSERT_FATAL(listener_state->w0_alive == listener_state->w0_not_alive);
  ddsrt_mutex_unlock(&listener_state->lock);
}

static void lease_duration_zero_or_one_impl (dds_duration_t sleep, dds_liveliness_kind_t lkind, dds_duration_t ldur, bool remote_reader)
{
  const uint32_t nsamples = (sleep <= DDS_MSECS(10)) ? 50 : 5;
  dds_entity_t reader;
  dds_entity_t writer_active;
  dds_entity_t waitset;
  dds_return_t rc;
  Space_Type1 sample = {1, 0, 0};
  struct liveliness_changed_state listener_state = {
    .weirdness = false,
    .w0_handle = 0,
    .w0_alive = 0,
    .w0_not_alive = 0,
  };
  ddsrt_mutex_init (&listener_state.lock);
  setup_reader_zero_or_one (&reader, &writer_active, &waitset, lkind, ldur, remote_reader, &listener_state);

  /* write as fast as possible - we don't expect this to cause the writers
     to gain and lose liveliness once for each sample, but it should have
     become alive at least once and fall back to not alive afterward */
  for (uint32_t i = 0; i < nsamples; i++)
  {
    rc = dds_write(writer_active, &sample);
    CU_ASSERT_FATAL(rc == DDS_RETCODE_OK);
    if (sleep && i < nsamples - 1)
      dds_sleepfor(sleep);
  }

  rc = dds_wait_for_acks(writer_active, DDS_SECS(5));
  CU_ASSERT_FATAL(rc == DDS_RETCODE_OK);

  /* verify the reader received all samples */
  void *raw[] = { &sample };
  dds_sample_info_t si;
  uint32_t cnt = 0;
  do
  {
    rc = dds_waitset_wait(waitset, NULL, 0, DDS_SECS(5));
    CU_ASSERT_FATAL(rc >= 1);
    while (dds_take(reader, raw, &si, 1, 1) == 1 && si.valid_data)
      cnt++;
  }
  while (cnt < nsamples + 1);
  CU_ASSERT_FATAL(cnt == nsamples + 1);

  /* transition to not alive is not necessarily immediate */
  wait_for_notalive (reader, &listener_state);

  {
    uint32_t exp_alive;
    if (sleep == 0)
      exp_alive = 1; /* if not sleeping, it's ok if the transition happens only once */
    else if (sleep <= DDS_MSECS(10))
      exp_alive = nsamples / 3; /* if sleeping briefly, expect the a good number of writes to toggle liveliness */
    else
      exp_alive = nsamples - nsamples / 5; /* if sleeping, expect the vast majority (80%) of the writes to toggle liveliness */
    ddsrt_mutex_lock(&listener_state.lock);
    tprintf("check w0_alive %"PRIu32" >= %"PRIu32"\n", listener_state.w0_alive, exp_alive);
    CU_ASSERT_FATAL(listener_state.w0_alive >= exp_alive);
    ddsrt_mutex_unlock(&listener_state.lock);
  }

  rc = dds_delete(waitset);
  CU_ASSERT_FATAL(rc == DDS_RETCODE_OK);
  dds_set_listener (reader, NULL); // listener must not be invoked anymore
  ddsrt_mutex_destroy(&listener_state.lock);
}

CU_Test(ddsc_liveliness, lease_duration_zero_or_one, .init = liveliness_init, .fini = liveliness_fini, .timeout = 30)
{
  static const bool remote_rd[] = { false, true };
  static const dds_duration_t sleep[] = { 0, DDS_MSECS(10), DDS_MSECS(100) };
  static const dds_liveliness_kind_t lkind[] = { DDS_LIVELINESS_MANUAL_BY_PARTICIPANT, DDS_LIVELINESS_MANUAL_BY_TOPIC };
  static const dds_duration_t ldur[] = { 0, 1 };
  for (size_t remote_rd_idx = 0; remote_rd_idx < sizeof (remote_rd) / sizeof (remote_rd[0]); remote_rd_idx++)
  {
    for (size_t sleep_idx = 0; sleep_idx < sizeof (sleep) / sizeof (sleep[0]); sleep_idx++)
    {
      for (size_t lkind_idx = 0; lkind_idx < sizeof (lkind) / sizeof (lkind[0]); lkind_idx++)
      {
        for (size_t ldur_idx = 0; ldur_idx < sizeof (ldur) / sizeof (ldur[0]); ldur_idx++)
        {
          bool rrd = remote_rd[remote_rd_idx];
          dds_duration_t s = sleep[sleep_idx];
          dds_liveliness_kind_t k = lkind[lkind_idx];
          dds_duration_t d = ldur[ldur_idx];
          tprintf ("### lease_duration_zero_or_one: sleep = %"PRId64" lkind = %d ldur = %"PRId64" reader = %s\n", s, (int) k, d, rrd ? "remote" : "local");
          lease_duration_zero_or_one_impl (s, k, d, rrd);
          printf ("\n");
        }
      }
    }
  }
}

struct getstatus_thread_arg {
  dds_entity_t rd;
  ddsrt_atomic_uint32_t stop;
};

static uint32_t getstatus_thread (void *varg)
{
  struct getstatus_thread_arg *arg = varg;
  while (!ddsrt_atomic_ld32 (&arg->stop))
  {
    dds_liveliness_changed_status_t s;
    dds_return_t rc;
    rc = dds_get_liveliness_changed_status (arg->rd, &s);
    CU_ASSERT_FATAL (rc == DDS_RETCODE_OK);
    /* change counts must be 0 because the listener gets invoked all the time */
    if (s.alive_count_change != 0 || s.not_alive_count_change != 0)
    {
      ddsrt_atomic_st32 (&arg->stop, 1);
      return 0;
    }
  }
  return 1;
}

CU_Test(ddsc_liveliness, listener_vs_getstatus, .init = liveliness_init, .fini = liveliness_fini, .timeout = 30)
{
  dds_entity_t reader;
  dds_entity_t writer_active;
  dds_entity_t waitset;
  dds_return_t rc;
  Space_Type1 sample = {1, 0, 0};
  struct liveliness_changed_state listener_state = {
    .weirdness = false,
    .w0_handle = 0,
    .w0_alive = 0,
    .w0_not_alive = 0,
  };
  ddsrt_mutex_init (&listener_state.lock);
  setup_reader_zero_or_one (&reader, &writer_active, &waitset, DDS_LIVELINESS_MANUAL_BY_TOPIC, 1, false, &listener_state);

  /* start a thread that continually calls dds_get_liveliness_changed_status: that resets
     the change counters, but that activity should not be visible in the listener argument */
  ddsrt_thread_t tid;
  ddsrt_threadattr_t tattr;
  ddsrt_threadattr_init(&tattr);
  struct getstatus_thread_arg targ = { .rd = reader, .stop = DDSRT_ATOMIC_UINT32_INIT (0) };
  rc = ddsrt_thread_create(&tid, "getstatus", &tattr, getstatus_thread, &targ);
  CU_ASSERT_FATAL(rc == DDS_RETCODE_OK);

  /* write as fast as possible - we don't expect this to cause the writers
     to gain and lose liveliness once for each sample, but it should have
     become alive at least once and fall back to not alive afterward */
  dds_time_t tnow = dds_time ();
  const dds_time_t tend = tnow + DDS_SECS(3);
  while (tnow < tend && !ddsrt_atomic_ld32 (&targ.stop))
  {
    rc = dds_write(writer_active, &sample);
    CU_ASSERT_FATAL(rc == DDS_RETCODE_OK);
    tnow = dds_time ();
  }

  ddsrt_atomic_st32 (&targ.stop, 1);
  uint32_t get_status_ok;
  rc = ddsrt_thread_join (tid, &get_status_ok);
  CU_ASSERT_FATAL (rc == DDS_RETCODE_OK);
  CU_ASSERT_FATAL (get_status_ok != 0);

  /* transition to not alive is not necessarily immediate */
  wait_for_notalive (reader, &listener_state);

  rc = dds_delete(waitset);
  CU_ASSERT_FATAL(rc == DDS_RETCODE_OK);

  dds_set_listener (reader, NULL); // listener must not be invoked anymore
  ddsrt_mutex_destroy(&listener_state.lock);
}
