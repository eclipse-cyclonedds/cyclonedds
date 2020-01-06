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
#include "CUnit/Theory.h"
#include "Space.h"
#include "config_env.h"

#include "dds/version.h"
#include "dds__entity.h"
#include "dds/ddsi/q_entity.h"
#include "dds/ddsi/ddsi_entity_index.h"
#include "dds/ddsrt/cdtors.h"
#include "dds/ddsrt/misc.h"
#include "dds/ddsrt/process.h"
#include "dds/ddsrt/threads.h"
#include "dds/ddsrt/environ.h"
#include "dds/ddsrt/atomics.h"
#include "dds/ddsrt/time.h"

#define DDS_DOMAINID_PUB 0
#define DDS_DOMAINID_SUB 1
#define DDS_CONFIG_NO_PORT_GAIN "${CYCLONEDDS_URI}${CYCLONEDDS_URI:+,}<Discovery><ExternalDomainId>0</ExternalDomainId></Discovery>"
#define DDS_CONFIG_NO_PORT_GAIN_LOG "${CYCLONEDDS_URI}${CYCLONEDDS_URI:+,}<Tracing><OutputFile>cyclonedds_liveliness_tests.${CYCLONEDDS_DOMAIN_ID}.${CYCLONEDDS_PID}.log</OutputFile><Verbosity>finest</Verbosity></Tracing><Discovery><ExternalDomainId>0</ExternalDomainId></Discovery>"

uint32_t g_topic_nr = 0;
static dds_entity_t g_pub_domain = 0;
static dds_entity_t g_pub_participant = 0;
static dds_entity_t g_pub_publisher = 0;

static dds_entity_t g_sub_domain = 0;
static dds_entity_t g_sub_participant = 0;
static dds_entity_t g_sub_subscriber = 0;

static char *create_topic_name(const char *prefix, uint32_t nr, char *name, size_t size)
{
  /* Get unique g_topic name. */
  ddsrt_pid_t pid = ddsrt_getpid();
  ddsrt_tid_t tid = ddsrt_gettid();
  (void)snprintf(name, size, "%s%d_pid%" PRIdPID "_tid%" PRIdTID "", prefix, nr, pid, tid);
  return name;
}

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
  dds_delete(g_sub_subscriber);
  dds_delete(g_pub_publisher);
  dds_delete(g_sub_participant);
  dds_delete(g_pub_participant);
  dds_delete(g_sub_domain);
  dds_delete(g_pub_domain);
}

/**
 * Gets the current PMD sequence number for the participant. This
 * can be used to count the number of PMD messages that is sent by
 * the participant.
 */
static seqno_t get_pmd_seqno(dds_entity_t participant)
{
  seqno_t seqno;
  struct dds_entity *pp_entity;
  struct participant *pp;
  struct writer *wr;
  CU_ASSERT_EQUAL_FATAL(dds_entity_pin(participant, &pp_entity), 0);
  thread_state_awake(lookup_thread_state(), &pp_entity->m_domain->gv);
  pp = entidx_lookup_participant_guid(pp_entity->m_domain->gv.entity_index, &pp_entity->m_guid);
  wr = get_builtin_writer(pp, NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_WRITER);
  CU_ASSERT_FATAL(wr != NULL);
  assert(wr != NULL); /* for Clang's static analyzer */
  seqno = wr->seq;
  thread_state_asleep(lookup_thread_state());
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
  struct participant *pp;
  CU_ASSERT_EQUAL_FATAL(dds_entity_pin(participant, &pp_entity), 0);
  thread_state_awake(lookup_thread_state(), &pp_entity->m_domain->gv);
  pp = entidx_lookup_participant_guid(pp_entity->m_domain->gv.entity_index, &pp_entity->m_guid);
  intv = pp_get_pmd_interval(pp);
  thread_state_asleep(lookup_thread_state());
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
CU_Theory((dds_liveliness_kind_t kind, uint32_t ldur, double mult), ddsc_liveliness, pmd_count, .init = liveliness_init, .fini = liveliness_fini, .timeout = 30)
{
  dds_entity_t pub_topic;
  dds_entity_t sub_topic;
  dds_entity_t reader;
  dds_entity_t writer;
  seqno_t start_seqno, end_seqno;
  dds_qos_t *rqos;
  dds_qos_t *wqos;
  dds_entity_t waitset;
  dds_attach_t triggered;
  uint32_t status;
  char name[100];
  dds_time_t t;

  t = dds_time();
  printf("%d.%06d running test: kind %s, lease duration %d, delay %d\n",
         (int32_t)(t / DDS_NSECS_IN_SEC), (int32_t)(t % DDS_NSECS_IN_SEC) / 1000,
         kind == 0 ? "A" : "MP", ldur, (int32_t)(mult * ldur));

  /* wait for initial PMD to be sent by the participant */
  while (get_pmd_seqno(g_pub_participant) < 1)
    dds_sleepfor(DDS_MSECS(50));

  /* topics */
  create_topic_name("ddsc_liveliness_pmd_count", g_topic_nr++, name, sizeof name);
  CU_ASSERT_FATAL((pub_topic = dds_create_topic(g_pub_participant, &Space_Type1_desc, name, NULL, NULL)) > 0);
  CU_ASSERT_FATAL((sub_topic = dds_create_topic(g_sub_participant, &Space_Type1_desc, name, NULL, NULL)) > 0);

  /* reader */
  CU_ASSERT_FATAL((rqos = dds_create_qos()) != NULL);
  dds_qset_liveliness(rqos, DDS_LIVELINESS_AUTOMATIC, DDS_INFINITY);
  CU_ASSERT_FATAL((reader = dds_create_reader(g_sub_participant, sub_topic, rqos, NULL)) > 0);
  dds_delete_qos(rqos);
  CU_ASSERT_EQUAL_FATAL(dds_set_status_mask(reader, DDS_LIVELINESS_CHANGED_STATUS), DDS_RETCODE_OK);

  /* waitset on reader */
  CU_ASSERT_FATAL((waitset = dds_create_waitset(g_sub_participant)) > 0);
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

  t = dds_time();
  printf("%d.%06d PMD sequence no: start %" PRId64 " -> end %" PRId64 "\n",
         (int32_t)(t / DDS_NSECS_IN_SEC), (int32_t)(t % DDS_NSECS_IN_SEC) / 1000,
         start_seqno, end_seqno);

  /* End-start should be mult - 1 under ideal circumstances, but consider the test successful
	   when at least 50% of the expected PMD's was sent. This checks that the frequency for sending
	   PMDs was increased when the writer was added. */
  CU_ASSERT(end_seqno - start_seqno >= (kind == DDS_LIVELINESS_AUTOMATIC ? (50 * (mult - 1)) / 100 : 0))
  if (kind != DDS_LIVELINESS_AUTOMATIC)
    CU_ASSERT(get_pmd_seqno(g_pub_participant) - start_seqno < mult)

  /* cleanup */
  CU_ASSERT_EQUAL_FATAL(dds_delete(sub_topic), DDS_RETCODE_OK);
  CU_ASSERT_EQUAL_FATAL(dds_delete(pub_topic), DDS_RETCODE_OK);
  CU_ASSERT_EQUAL_FATAL(dds_delete(writer), DDS_RETCODE_OK);
  CU_ASSERT_EQUAL_FATAL(dds_delete(reader), DDS_RETCODE_OK);
}

/**
 * Test that the expected number of proxy writers expires (set to not-alive)
 * after a certain delay for various combinations of writers with different
 * liveliness kinds.
 */
CU_TheoryDataPoints(ddsc_liveliness, expire_liveliness_kinds) = {
    CU_DataPoints(uint32_t, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200), /* lease duration for initial test run (increased for each retry when test fails) */
    CU_DataPoints(double,   0.3, 0.3, 0.3, 0.3, 0.3, 0.3,   2,   2,   2,   2,   2,   2,   2,   2,   2), /* delay (n times lease duration) */
    CU_DataPoints(uint32_t,   1,   0,   2,   0,   1,   0,   0,   1,   1,   2,   0,   5,   0,  15,  15), /* number of writers with automatic liveliness */
    CU_DataPoints(uint32_t,   1,   1,   2,   2,   0,   0,   0,   1,   0,   2,   2,   5,  10,   0,  15), /* number of writers with manual-by-participant liveliness */
    CU_DataPoints(uint32_t,   1,   1,   2,   2,   1,   1,   1,   1,   0,   1,   1,   2,   5,   0,  10), /* number of writers with manual-by-topic liveliness */
};
CU_Theory((uint32_t ldur, double mult, uint32_t wr_cnt_auto, uint32_t wr_cnt_man_pp, uint32_t wr_cnt_man_tp), ddsc_liveliness, expire_liveliness_kinds, .init = liveliness_init, .fini = liveliness_fini, .timeout = 120)
{
  dds_entity_t pub_topic;
  dds_entity_t sub_topic;
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
    printf("%d.%06d running test: lease duration %d, delay %f, auto/man-by-part/man-by-topic %u/%u/%u\n",
           (int32_t)(tstart / DDS_NSECS_IN_SEC), (int32_t)(tstart % DDS_NSECS_IN_SEC) / 1000,
           ldur, mult, wr_cnt_auto, wr_cnt_man_pp, wr_cnt_man_tp);

    /* topics */
    create_topic_name("ddsc_liveliness_expire_kinds", g_topic_nr++, name, sizeof name);
    CU_ASSERT_FATAL((pub_topic = dds_create_topic(g_pub_participant, &Space_Type1_desc, name, NULL, NULL)) > 0);
    CU_ASSERT_FATAL((sub_topic = dds_create_topic(g_sub_participant, &Space_Type1_desc, name, NULL, NULL)) > 0);

    /* reader */
    CU_ASSERT_FATAL((rqos = dds_create_qos()) != NULL);
    dds_qset_liveliness(rqos, DDS_LIVELINESS_AUTOMATIC, DDS_INFINITY);
    CU_ASSERT_FATAL((reader = dds_create_reader(g_sub_participant, sub_topic, rqos, NULL)) > 0);
    dds_delete_qos(rqos);
    CU_ASSERT_EQUAL_FATAL(dds_set_status_mask(reader, DDS_LIVELINESS_CHANGED_STATUS), DDS_RETCODE_OK);

    /* writers */
    CU_ASSERT_FATAL((wqos_auto = dds_create_qos()) != NULL);
    dds_qset_liveliness(wqos_auto, DDS_LIVELINESS_AUTOMATIC, DDS_MSECS(ldur));
    CU_ASSERT_FATAL((wqos_man_pp = dds_create_qos()) != NULL);
    dds_qset_liveliness(wqos_man_pp, DDS_LIVELINESS_MANUAL_BY_PARTICIPANT, DDS_MSECS(ldur));
    CU_ASSERT_FATAL((wqos_man_tp = dds_create_qos()) != NULL);
    dds_qset_liveliness(wqos_man_tp, DDS_LIVELINESS_MANUAL_BY_TOPIC, DDS_MSECS(ldur));

    CU_ASSERT_FATAL((waitset = dds_create_waitset(g_sub_participant)) > 0);
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
    if (t - tstart > DDS_MSECS(0.5 * ldur))
    {
      ldur *= 10 / (run + 1);
      printf("%d.%06d failed to create writers in time\n",
             (int32_t)(t / DDS_NSECS_IN_SEC), (int32_t)(t % DDS_NSECS_IN_SEC) / 1000);
    }
    else
    {
      /* check alive count before proxy writers are expired */
      dds_get_liveliness_changed_status(reader, &lstatus);
      printf("%d.%06d writers alive: %d\n", (int32_t)(t / DDS_NSECS_IN_SEC), (int32_t)(t % DDS_NSECS_IN_SEC) / 1000, lstatus.alive_count);
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
      t = dds_time();
      printf("%d.%06d writers stopped: %u\n",
             (int32_t)(t / DDS_NSECS_IN_SEC), (int32_t)(t % DDS_NSECS_IN_SEC) / 1000, stopped);

      size_t exp_stopped = mult < 1 ? 0 : (wr_cnt_man_pp + wr_cnt_man_tp);
      if (stopped != exp_stopped)
      {
        ldur *= 10 / (run + 1);
        printf("%d.%06d incorrect number of stopped writers\n",
               (int32_t)(t / DDS_NSECS_IN_SEC), (int32_t)(t % DDS_NSECS_IN_SEC) / 1000);
      }
      else
      {
        /* check alive count */
        CU_ASSERT_EQUAL_FATAL(dds_get_liveliness_changed_status(reader, &lstatus), DDS_RETCODE_OK);
        CU_ASSERT_EQUAL(lstatus.alive_count, mult < 1 ? wr_cnt : wr_cnt_auto);
        test_finished = true;
      }
    }

    /* cleanup */
    CU_ASSERT_EQUAL_FATAL(dds_waitset_detach(waitset, reader), DDS_RETCODE_OK);
    CU_ASSERT_EQUAL_FATAL(dds_delete(waitset), DDS_RETCODE_OK);

    for (n = 0; n < wr_cnt; n++)
      CU_ASSERT_EQUAL_FATAL(dds_delete(writers[n]), DDS_RETCODE_OK);
    dds_free(writers);
    CU_ASSERT_EQUAL_FATAL(dds_delete(sub_topic), DDS_RETCODE_OK);
    CU_ASSERT_EQUAL_FATAL(dds_delete(pub_topic), DDS_RETCODE_OK);
    CU_ASSERT_EQUAL_FATAL(dds_delete(reader), DDS_RETCODE_OK);

    if (!test_finished)
    {
      if (++run > 3)
      {
        printf("%d.%06d run limit reached, test failed\n", (int32_t)(tstart / DDS_NSECS_IN_SEC), (int32_t)(tstart % DDS_NSECS_IN_SEC) / 1000);
        CU_FAIL_FATAL("Run limit reached");
        test_finished = true;
        continue;
      }
      else
      {
        printf("%d.%06d restarting test with ldur %d\n",
               (int32_t)(t / DDS_NSECS_IN_SEC), (int32_t)(t % DDS_NSECS_IN_SEC) / 1000, ldur);
      }
    }
  } while (!test_finished);
}

static void add_and_check_writer(dds_liveliness_kind_t kind, dds_duration_t ldur, dds_entity_t *writer, dds_entity_t topic, dds_entity_t reader)
{
  dds_entity_t waitset;
  dds_qos_t *wqos;
  dds_attach_t triggered;
  uint32_t status;

  CU_ASSERT_FATAL((waitset = dds_create_waitset(g_sub_participant)) > 0);
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
  create_topic_name("ddsc_liveliness_ldur", 1, name, sizeof name);
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
  add_and_check_writer(DDS_LIVELINESS_AUTOMATIC, DDS_MSECS(1000), &writers[wr_cnt++], pub_topic, reader);
  CU_ASSERT_EQUAL_FATAL(get_pmd_interval(g_pub_participant), DDS_MSECS(1000));

  add_and_check_writer(DDS_LIVELINESS_AUTOMATIC, DDS_MSECS(2000), &writers[wr_cnt++], pub_topic, reader);
  CU_ASSERT_EQUAL_FATAL(get_pmd_interval(g_pub_participant), DDS_MSECS(1000));

  add_and_check_writer(DDS_LIVELINESS_MANUAL_BY_PARTICIPANT, DDS_MSECS(2000), &writers[wr_cnt++], pub_topic, reader);
  CU_ASSERT_EQUAL_FATAL(get_pmd_interval(g_pub_participant), DDS_MSECS(1000));

  add_and_check_writer(DDS_LIVELINESS_AUTOMATIC, DDS_MSECS(500), &writers[wr_cnt++], pub_topic, reader);
  CU_ASSERT_EQUAL_FATAL(get_pmd_interval(g_pub_participant), DDS_MSECS(500));

  add_and_check_writer(DDS_LIVELINESS_MANUAL_BY_PARTICIPANT, DDS_MSECS(100), &writers[wr_cnt++], pub_topic, reader);
  CU_ASSERT_EQUAL_FATAL(get_pmd_interval(g_pub_participant), DDS_MSECS(500));

  add_and_check_writer(DDS_LIVELINESS_MANUAL_BY_TOPIC, DDS_MSECS(100), &writers[wr_cnt++], pub_topic, reader);
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
CU_Test(ddsc_liveliness, lease_duration_pwr, .init = liveliness_init, .fini = liveliness_fini)
{
  dds_entity_t pub_topic;
  dds_entity_t sub_topic;
  dds_entity_t reader;
  dds_entity_t writer;
  char name[100];
  dds_qos_t *rqos, *wqos;
  dds_entity_t waitset;
  dds_attach_t triggered;
  uint32_t status;
  dds_duration_t ldur;

  /* topics */
  create_topic_name("ddsc_liveliness_ldurpwr", 1, name, sizeof name);
  CU_ASSERT_FATAL((pub_topic = dds_create_topic(g_pub_participant, &Space_Type1_desc, name, NULL, NULL)) > 0);
  CU_ASSERT_FATAL((sub_topic = dds_create_topic(g_sub_participant, &Space_Type1_desc, name, NULL, NULL)) > 0);

  /* reader */
  CU_ASSERT_FATAL((rqos = dds_create_qos()) != NULL);
  dds_qset_liveliness(rqos, DDS_LIVELINESS_AUTOMATIC, DDS_INFINITY);
  CU_ASSERT_FATAL((reader = dds_create_reader(g_sub_participant, sub_topic, rqos, NULL)) > 0);
  dds_delete_qos(rqos);
  CU_ASSERT_EQUAL_FATAL(dds_set_status_mask(reader, DDS_LIVELINESS_CHANGED_STATUS), DDS_RETCODE_OK);

  /* writer */
  ldur = 1000;
  CU_ASSERT_FATAL((wqos = dds_create_qos()) != NULL);
  dds_qset_liveliness(wqos, DDS_LIVELINESS_AUTOMATIC, DDS_MSECS(ldur));
  CU_ASSERT_FATAL((writer = dds_create_writer(g_pub_participant, pub_topic, wqos, NULL)) > 0);

  /* wait for writer to be alive */
  CU_ASSERT_FATAL((waitset = dds_create_waitset(g_sub_participant)) > 0);
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
  dds_delete_qos(ep->qos);
  dds_free(ep->topic_name);
  dds_free(ep->type_name);
  dds_free(ep);

  /* cleanup */
  dds_delete_qos(wqos);
  CU_ASSERT_EQUAL_FATAL(dds_waitset_detach(waitset, reader), DDS_RETCODE_OK);
  CU_ASSERT_EQUAL_FATAL(dds_delete(waitset), DDS_RETCODE_OK);
  CU_ASSERT_EQUAL_FATAL(dds_delete(writer), DDS_RETCODE_OK);
  CU_ASSERT_EQUAL_FATAL(dds_delete(sub_topic), DDS_RETCODE_OK);
  CU_ASSERT_EQUAL_FATAL(dds_delete(pub_topic), DDS_RETCODE_OK);
  CU_ASSERT_EQUAL_FATAL(dds_delete(reader), DDS_RETCODE_OK);
}

/**
 * Create a relative large number of writers with liveliness kinds automatic and
 * manual-by-participant and with decreasing lease duration, and check that all
 * writers become alive. During the writer creation loop, every third writer
 * is deleted immediately after creating.
 */
#define MAX_WRITERS 100
CU_Test(ddsc_liveliness, create_delete_writer_stress, .init = liveliness_init, .fini = liveliness_fini, .timeout = 15)
{
  dds_entity_t pub_topic;
  dds_entity_t sub_topic;
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

  /* topics */
  create_topic_name("ddsc_liveliness_wr_stress", 1, name, sizeof name);
  CU_ASSERT_FATAL((pub_topic = dds_create_topic(g_pub_participant, &Space_Type1_desc, name, NULL, NULL)) > 0);
  CU_ASSERT_FATAL((sub_topic = dds_create_topic(g_sub_participant, &Space_Type1_desc, name, NULL, NULL)) > 0);

  /* reader and waitset */
  CU_ASSERT_FATAL((rqos = dds_create_qos()) != NULL);
  dds_qset_liveliness(rqos, DDS_LIVELINESS_AUTOMATIC, DDS_INFINITY);
  CU_ASSERT_FATAL((reader = dds_create_reader(g_sub_participant, sub_topic, rqos, NULL)) > 0);
  dds_delete_qos(rqos);
  CU_ASSERT_EQUAL_FATAL(dds_set_status_mask(reader, DDS_LIVELINESS_CHANGED_STATUS), DDS_RETCODE_OK);
  CU_ASSERT_FATAL((waitset = dds_create_waitset(g_sub_participant)) > 0);
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
    dds_qset_liveliness(wqos, n % 2 ? DDS_LIVELINESS_AUTOMATIC : DDS_LIVELINESS_MANUAL_BY_PARTICIPANT, DDS_MSECS(n % 3 ? ldur + n : ldur - n) + ((n % 3) == 2 ? 1 : 0));
    CU_ASSERT_FATAL((writers[n] = dds_create_writer(g_pub_participant, pub_topic, wqos, NULL)) > 0);
    dds_write(writers[n], &sample);
    if (n % 3 == 2)
      dds_delete(writers[n]);
    else if (n % 2)
      alive_writers_auto++;
    else
      alive_writers_man++;
  }
  dds_delete_qos(wqos);
  printf("alive_writers_auto: %d, alive_writers_man: %d\n", alive_writers_auto, alive_writers_man);

  /* wait for auto liveliness writers to become alive and manual-by-pp writers to become not-alive */
  do
  {
    CU_ASSERT_EQUAL_FATAL(dds_get_liveliness_changed_status(reader, &lstatus), DDS_RETCODE_OK);
    printf("alive: %d, not-alive: %d\n", lstatus.alive_count, lstatus.not_alive_count);
    dds_sleepfor(DDS_MSECS(50));
  } while (lstatus.alive_count != alive_writers_auto || lstatus.not_alive_count != alive_writers_man);

  /* check that counts are stable after a delay */
  dds_sleepfor(DDS_MSECS(ldur / 2));
  CU_ASSERT_EQUAL_FATAL(dds_get_liveliness_changed_status(reader, &lstatus), DDS_RETCODE_OK);
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
    printf("alive: %d, not: %d\n", lstatus.alive_count, lstatus.not_alive_count);
    dds_sleepfor(DDS_MSECS(ldur / 10));
  } while (lstatus.alive_count > 0 || lstatus.not_alive_count > 0);
  CU_ASSERT_EQUAL_FATAL(dds_waitset_detach(waitset, reader), DDS_RETCODE_OK);
  CU_ASSERT_EQUAL_FATAL(dds_delete(waitset), DDS_RETCODE_OK);
  CU_ASSERT_EQUAL_FATAL(dds_delete(reader), DDS_RETCODE_OK);
  CU_ASSERT_EQUAL_FATAL(dds_delete(sub_topic), DDS_RETCODE_OK);
  CU_ASSERT_EQUAL_FATAL(dds_delete(pub_topic), DDS_RETCODE_OK);
}
#undef MAX_WRITERS

/**
 * Check the counts in liveliness_changed_status result.
 */
CU_Test(ddsc_liveliness, status_counts, .init = liveliness_init, .fini = liveliness_fini)
{
  dds_entity_t pub_topic;
  dds_entity_t sub_topic;
  dds_entity_t reader;
  dds_entity_t writer;
  dds_entity_t waitset;
  dds_qos_t *rqos;
  dds_qos_t *wqos;
  dds_attach_t triggered;
  struct dds_liveliness_changed_status lstatus;
  struct dds_subscription_matched_status sstatus;
  char name[100];
  dds_duration_t ldur = DDS_MSECS(500);
  Space_Type1 sample = {1, 0, 0};

  /* topics */
  create_topic_name("ddsc_liveliness_status_counts", g_topic_nr++, name, sizeof name);
  CU_ASSERT_FATAL((pub_topic = dds_create_topic(g_pub_participant, &Space_Type1_desc, name, NULL, NULL)) > 0);
  CU_ASSERT_FATAL((sub_topic = dds_create_topic(g_sub_participant, &Space_Type1_desc, name, NULL, NULL)) > 0);

  /* reader */
  CU_ASSERT_FATAL((rqos = dds_create_qos()) != NULL);
  dds_qset_liveliness(rqos, DDS_LIVELINESS_AUTOMATIC, DDS_INFINITY);
  CU_ASSERT_FATAL((reader = dds_create_reader(g_sub_participant, sub_topic, rqos, NULL)) > 0);
  dds_delete_qos(rqos);
  CU_ASSERT_EQUAL_FATAL(dds_set_status_mask(reader, DDS_LIVELINESS_CHANGED_STATUS), DDS_RETCODE_OK);
  CU_ASSERT_FATAL((waitset = dds_create_waitset(g_sub_participant)) > 0);
  CU_ASSERT_EQUAL_FATAL(dds_waitset_attach(waitset, reader, reader), DDS_RETCODE_OK);

  /* writer */
  CU_ASSERT_FATAL((wqos = dds_create_qos()) != NULL);
  dds_qset_liveliness(wqos, DDS_LIVELINESS_MANUAL_BY_PARTICIPANT, ldur);
  CU_ASSERT_FATAL((writer = dds_create_writer(g_pub_participant, pub_topic, wqos, NULL)) > 0);
  dds_delete_qos(wqos);

  /* wait for writer to be alive */
  CU_ASSERT_EQUAL_FATAL(dds_waitset_wait(waitset, &triggered, 1, DDS_SECS(5)), 1);

  /* check status counts before proxy writer is expired */
  dds_get_liveliness_changed_status(reader, &lstatus);
  CU_ASSERT_EQUAL_FATAL(lstatus.alive_count, 1);
  dds_get_subscription_matched_status(reader, &sstatus);
  CU_ASSERT_EQUAL_FATAL(sstatus.current_count, 1);

  /* sleep for more than lease duration, writer should be set not-alive but subscription still matched */
  dds_sleepfor(ldur + DDS_MSECS(100));
  CU_ASSERT_EQUAL_FATAL(dds_waitset_wait(waitset, &triggered, 1, DDS_SECS(5)), 1);

  dds_get_liveliness_changed_status(reader, &lstatus);
  CU_ASSERT_EQUAL_FATAL(lstatus.alive_count, 0);
  dds_get_subscription_matched_status(reader, &sstatus);
  CU_ASSERT_EQUAL_FATAL(sstatus.current_count, 1);

  /* write sample and re-check status counts */
  dds_write(writer, &sample);
  CU_ASSERT_EQUAL_FATAL(dds_waitset_wait(waitset, &triggered, 1, DDS_SECS(5)), 1);

  dds_get_liveliness_changed_status(reader, &lstatus);
  CU_ASSERT_EQUAL_FATAL(lstatus.alive_count, 1);
  dds_get_subscription_matched_status(reader, &sstatus);
  CU_ASSERT_EQUAL_FATAL(sstatus.current_count, 1);

  /* cleanup */
  CU_ASSERT_EQUAL_FATAL(dds_waitset_detach(waitset, reader), DDS_RETCODE_OK);
  CU_ASSERT_EQUAL_FATAL(dds_delete(waitset), DDS_RETCODE_OK);
  CU_ASSERT_EQUAL_FATAL(dds_delete(reader), DDS_RETCODE_OK);
  CU_ASSERT_EQUAL_FATAL(dds_delete(writer), DDS_RETCODE_OK);
  CU_ASSERT_EQUAL_FATAL(dds_delete(sub_topic), DDS_RETCODE_OK);
  CU_ASSERT_EQUAL_FATAL(dds_delete(pub_topic), DDS_RETCODE_OK);
}

/**
 * Test that dds_assert_liveliness works as expected for liveliness
 * kinds manual-by-participant and manual-by-topic.
 */
#define MAX_WRITERS 100
CU_TheoryDataPoints(ddsc_liveliness, assert_liveliness) = {
    CU_DataPoints(uint32_t, 1, 0, 0, 1), /* number of writers with automatic liveliness */
    CU_DataPoints(uint32_t, 1, 1, 0, 0), /* number of writers with manual-by-participant liveliness */
    CU_DataPoints(uint32_t, 1, 1, 1, 2), /* number of writers with manual-by-topic liveliness */
};
CU_Theory((uint32_t wr_cnt_auto, uint32_t wr_cnt_man_pp, uint32_t wr_cnt_man_tp), ddsc_liveliness, assert_liveliness, .init = liveliness_init, .fini = liveliness_fini, .timeout = 60)
{
  dds_entity_t pub_topic, sub_topic, reader, writers[MAX_WRITERS];
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
    printf("running test assert_liveliness: auto/man-by-part/man-by-topic %u/%u/%u with ldur %d\n",
           wr_cnt_auto, wr_cnt_man_pp, wr_cnt_man_tp, ldur);

    /* topics */
    create_topic_name("ddsc_liveliness_assert", g_topic_nr++, name, sizeof name);
    CU_ASSERT_FATAL((pub_topic = dds_create_topic(g_pub_participant, &Space_Type1_desc, name, NULL, NULL)) > 0);
    CU_ASSERT_FATAL((sub_topic = dds_create_topic(g_sub_participant, &Space_Type1_desc, name, NULL, NULL)) > 0);

    /* reader */
    CU_ASSERT_FATAL((rqos = dds_create_qos()) != NULL);
    dds_qset_liveliness(rqos, DDS_LIVELINESS_AUTOMATIC, DDS_INFINITY);
    CU_ASSERT_FATAL((reader = dds_create_reader(g_sub_participant, sub_topic, rqos, NULL)) > 0);
    dds_delete_qos(rqos);
    CU_ASSERT_EQUAL_FATAL(dds_set_status_mask(reader, DDS_LIVELINESS_CHANGED_STATUS), DDS_RETCODE_OK);

    /* writers */
    for (size_t n = 0; n < wr_cnt_auto; n++)
      add_and_check_writer(DDS_LIVELINESS_AUTOMATIC, DDS_MSECS(ldur), &writers[wr_cnt++], pub_topic, reader);
    tstart = dds_time();
    for (size_t n = 0; n < wr_cnt_man_pp; n++)
      add_and_check_writer(DDS_LIVELINESS_MANUAL_BY_PARTICIPANT, DDS_MSECS(ldur), &writers[wr_cnt++], pub_topic, reader);
    for (size_t n = 0; n < wr_cnt_man_tp; n++)
      add_and_check_writer(DDS_LIVELINESS_MANUAL_BY_TOPIC, DDS_MSECS(ldur), &writers[wr_cnt++], pub_topic, reader);
    t = dds_time();
    if (t - tstart > DDS_MSECS(0.5 * ldur))
    {
      ldur *= 10 / (run + 1);
      printf("%d.%06d failed to create writers with non-automatic liveliness kind in time\n",
             (int32_t)(t / DDS_NSECS_IN_SEC), (int32_t)(t % DDS_NSECS_IN_SEC) / 1000);
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
        for (size_t n = wr_cnt - wr_cnt_man_tp; n < wr_cnt; n++)
          dds_assert_liveliness(writers[n]);
        CU_ASSERT_EQUAL_FATAL(dds_get_liveliness_changed_status(reader, &lstatus), DDS_RETCODE_OK);
        stopped += (uint32_t)lstatus.not_alive_count_change;
        dds_sleepfor(DDS_MSECS(50));
      } while (dds_time() < tstop);
      dds_get_liveliness_changed_status(reader, &lstatus);
      printf("writers alive with dds_assert_liveliness on all writers: %d, writers stopped: %d\n", lstatus.alive_count, stopped);
      if (lstatus.alive_count != wr_cnt_auto + wr_cnt_man_pp + wr_cnt_man_tp || stopped != 0)
      {
        ldur *= 10 / (run + 1);
        printf("incorrect number of writers alive or stopped writers\n");
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
        printf("writers alive with dds_assert_liveliness on participant: %d, writers stopped: %d\n", lstatus.alive_count, stopped);
        if (lstatus.alive_count != wr_cnt_auto + wr_cnt_man_pp || stopped != wr_cnt_man_tp)
        {
          ldur *= 10 / (run + 1);
          printf("incorrect number of writers alive or stopped writers\n");
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
        printf("restarting test with ldur %d\n", ldur);
      }
    }
  } while (!test_finished);
}
#undef MAX_WRITERS
