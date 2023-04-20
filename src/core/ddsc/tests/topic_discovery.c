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
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/random.h"
#include "dds/ddsi/ddsi_entity_index.h"
#include "dds/ddsi/ddsi_entity.h"
#include "ddsi__whc.h"
#include "dds__entity.h"

#include "test_common.h"

#define DDS_DOMAINID1 0
#define DDS_DOMAINID2 1
#define DDS_CONFIG_NO_PORT_GAIN "${CYCLONEDDS_URI}${CYCLONEDDS_URI:+,}<Discovery><ExternalDomainId>0</ExternalDomainId><EnableTopicDiscoveryEndpoints>true</EnableTopicDiscoveryEndpoints></Discovery>"

static dds_entity_t g_domain1 = 0;
static dds_entity_t g_participant1 = 0;
static dds_entity_t g_subscriber1  = 0;
static dds_entity_t g_publisher1   = 0;
static dds_entity_t g_domain_remote      = 0;

static void topic_discovery_init (void)
{
  /* Domains for pub and sub use a different domain id, but the portgain setting
         * in configuration is 0, so that both domains will map to the same port number.
         * This allows to create two domains in a single test process. */
  char *conf1 = ddsrt_expand_envvars (DDS_CONFIG_NO_PORT_GAIN, DDS_DOMAINID1);
  char *conf2 = ddsrt_expand_envvars (DDS_CONFIG_NO_PORT_GAIN, DDS_DOMAINID2);
  g_domain1 = dds_create_domain (DDS_DOMAINID1, conf1);
  g_domain_remote = dds_create_domain (DDS_DOMAINID2, conf2);
  dds_free (conf1);
  dds_free (conf2);

  g_participant1 = dds_create_participant (DDS_DOMAINID1, NULL, NULL);
  CU_ASSERT_FATAL (g_participant1 > 0);
  g_subscriber1 = dds_create_subscriber (g_participant1, NULL, NULL);
  CU_ASSERT_FATAL (g_subscriber1 > 0);
  g_publisher1 = dds_create_publisher (g_participant1, NULL, NULL);
  CU_ASSERT_FATAL (g_publisher1 > 0);
}

static void topic_discovery_fini (void)
{
  dds_delete (g_domain_remote);
  /* Add a delay so that sedp dispose messages for topics (and endpoints) are sent and processed */
  dds_delete (g_domain1);
}

CU_TheoryDataPoints(ddsc_topic_discovery, remote_topics) = {
    CU_DataPoints(uint32_t,     1,     1,     5,     5,    20,     1,     1,     5,     5,    20,    1,    5), /* number of participants */
    CU_DataPoints(uint32_t,     1,     5,     1,    30,     3,     1,     5,     1,    30,     3,    1,   30), /* number of topics per participant */
    CU_DataPoints(bool,      true,  true,  true,  true,  true, false, false, false, false, false, true, true), /* test historical data for topic discovery */
    CU_DataPoints(bool,     false, false, false, false, false,  true,  true,  true,  true,  true, true, true), /* test live topic discovery */
};

CU_Theory ((uint32_t num_pp, uint32_t num_tp, bool hist_data, bool live_data), ddsc_topic_discovery, remote_topics, .init = topic_discovery_init, .fini = topic_discovery_fini, .timeout = 180)
{
  tprintf ("ddsc_topic_discovery.remote_topics: %u participants, %u topics,%s%s\n", num_pp, num_tp, hist_data ? " historical-data" : "", live_data ? " live-data" : "");

  CU_ASSERT_FATAL (num_pp > 0);
  CU_ASSERT_FATAL (num_tp > 0 && num_tp <= 64);
  CU_ASSERT_FATAL (hist_data || live_data);

  char **topic_names = ddsrt_malloc (2 * num_pp * num_tp * sizeof (char *));
  uint64_t *seen = ddsrt_malloc (2 * num_pp * sizeof (*seen));
  bool all_seen = false;
  dds_entity_t *participant_remote = ddsrt_malloc (num_pp * sizeof (*participant_remote));

  for (uint32_t p = 0; p < num_pp; p++)
  {
    participant_remote[p] = dds_create_participant (DDS_DOMAINID2, NULL, NULL);
    CU_ASSERT_FATAL (participant_remote[p] > 0);
    seen[p] = seen[num_pp + p] = 0;
  }

  /* create topics before reader has been created (will be delivered as historical data) */
  if (hist_data)
  {
    for (uint32_t p = 0; p < num_pp; p++)
      for (uint32_t t = 0; t < num_tp; t++)
      {
        topic_names[p * num_tp + t] = ddsrt_malloc (101);
        create_unique_topic_name ("ddsc_topic_discovery_rem_tp", topic_names[p * num_tp + t], 100);
        dds_entity_t topic = dds_create_topic (participant_remote[p], &Space_Type1_desc, topic_names[p * num_tp + t], NULL, NULL);
        CU_ASSERT_FATAL (topic > 0);
      }

    /* sleep for some time so that ddsi_deliver_historical_data will be used for (at least some of)
       the sedp samples for the created topics */
    dds_sleepfor (DDS_MSECS (500));
  }

  /* create reader for DCPSTopic */
  dds_entity_t topic_rd = dds_create_reader (g_participant1, DDS_BUILTIN_TOPIC_DCPSTOPIC, NULL, NULL);
  CU_ASSERT_FATAL (topic_rd > 0);

  /* create more topics after reader has been created */
  if (live_data)
  {
    uint32_t offs = num_pp * num_tp;
    for (uint32_t p = 0; p < num_pp; p++)
      for (uint32_t t = 0; t < num_tp; t++)
      {
        topic_names[offs + p * num_tp + t] = ddsrt_malloc (101);
        create_unique_topic_name ("ddsc_topic_discovery_rem2_tp", topic_names[offs + p * num_tp + t], 100);
        dds_entity_t topic = dds_create_topic (participant_remote[p], &Space_Type1_desc, topic_names[offs + p * num_tp + t], NULL, NULL);
        CU_ASSERT_FATAL (topic > 0);
      }
  }

  /* read DCPSTopic and check if all topics seen */
  dds_time_t t_exp = dds_time () + DDS_SECS (180);
  do
  {
    void *raw[1] = { 0 };
    dds_sample_info_t sample_info[1];
    dds_return_t n;
    while ((n = dds_take (topic_rd, raw, sample_info, 1, 1)) > 0)
    {
      CU_ASSERT_EQUAL_FATAL (n, 1);
      if (sample_info[0].valid_data)
      {
        dds_builtintopic_topic_t *sample = raw[0];
        // msg ("read topic: %s\n", sample->topic_name);
        for (uint32_t p = 0; p < 2 * num_pp; p++)
          for (uint32_t t = 0; t < num_tp; t++)
            if (((hist_data && p < num_pp) || (live_data && p >= num_pp)) && !strcmp (sample->topic_name, topic_names[p * num_tp + t]))
              seen[p] |= UINT64_C (1) << t;
      }
      dds_return_loan (topic_rd, raw, n);
    }
    all_seen = true;
    for (uint32_t p = 0; p < 2 * num_pp && all_seen; p++)
      if (((hist_data && p < num_pp) || (live_data && p >= num_pp)) && seen[p] != (UINT64_C (2) << (num_tp - 1)) - 1)
        all_seen = false;
    dds_sleepfor (DDS_MSECS (10));
  }
  while (!all_seen && dds_time () < t_exp);
  CU_ASSERT_FATAL (all_seen);

  /* clean up */
  for (uint32_t p = 0; p < 2 * num_pp; p++)
    for (uint32_t t = 0; t < num_tp; t++)
      if ((hist_data && p < num_pp) || (live_data && p >= num_pp))
        ddsrt_free (topic_names[p * num_tp + t]);
  ddsrt_free (seen);
  ddsrt_free (topic_names);
  ddsrt_free (participant_remote);
}

static void check_topic_samples (dds_entity_t topic_rd, char *topic_name, uint32_t exp_count, bool equal_keys, unsigned char *key, unsigned char *match_key)
{
  dds_time_t t_exp = dds_time () + DDS_SECS (1);
  uint32_t topic_seen = 0;
  unsigned char first_key[16];
  do
  {
    void *raw[1] = { 0 };
    dds_sample_info_t sample_info[1];
    dds_return_t n;
    while ((n = dds_take (topic_rd, raw, sample_info, 1, 1)) > 0)
    {
      CU_ASSERT_EQUAL_FATAL (n, 1);
      dds_builtintopic_topic_t *sample = raw[0];
      bool not_alive = sample_info->instance_state != DDS_IST_ALIVE;
      tprintf ("read topic: %s, key={", sample->topic_name);
      for (uint32_t i = 0; i < sizeof (first_key); i++)
        printf ("%02x", sample->key.d[i]);
      printf ("} %sALIVE\n", not_alive ? "NOT_" : "");
      if (!not_alive && (topic_name == NULL || !strcmp (sample->topic_name, topic_name)))
      {
        if (topic_seen == 0)
        {
          memcpy (&first_key, &sample->key, sizeof (first_key));
          if (key != NULL)
            memcpy (key, &sample->key, sizeof (first_key));
        }
        else
        {
          CU_ASSERT_EQUAL_FATAL (memcmp (&first_key, &sample->key, sizeof (first_key)) == 0, equal_keys);
        }
        if (match_key != NULL)
          CU_ASSERT_EQUAL_FATAL (memcmp (match_key, &sample->key, sizeof (first_key)) == 0, equal_keys);
        assert (topic_seen < exp_count);
        topic_seen++;
      }
      dds_return_loan (topic_rd, raw, n);
    }
    dds_sleepfor (DDS_MSECS (10));
  } while (dds_time () < t_exp);
  CU_ASSERT_FATAL (topic_seen == exp_count);
}

CU_Test (ddsc_topic_discovery, single_topic_def, .init = topic_discovery_init, .fini = topic_discovery_fini)
{
  tprintf ("ddsc_topic_discovery.single_topic_def\n");

  char topic_name[100];
  create_unique_topic_name ("ddsc_topic_discovery_test_single_def", topic_name, 100);
  dds_entity_t participant_remote = dds_create_participant (DDS_DOMAINID2, NULL, NULL);
  CU_ASSERT_FATAL (participant_remote > 0);

  /* create topic */
  dds_entity_t topic = dds_create_topic (g_participant1, &Space_Type1_desc, topic_name, NULL, NULL);
  CU_ASSERT_FATAL (topic > 0);

  /* create reader for DCPSTopic and for the application topic */
  dds_entity_t topic_rd = dds_create_reader (g_participant1, DDS_BUILTIN_TOPIC_DCPSTOPIC, NULL, NULL);
  CU_ASSERT_FATAL (topic_rd > 0);
  dds_entity_t app_rd = dds_create_reader (g_participant1, topic, NULL, NULL);
  CU_ASSERT_FATAL (app_rd > 0);

  /* create 'remote' topic and a reader and writer using this topic */
  dds_entity_t topic_remote = dds_create_topic (participant_remote, &Space_Type1_desc, topic_name, NULL, NULL);
  CU_ASSERT_FATAL (topic_remote > 0);
  dds_entity_t reader_remote = dds_create_reader (participant_remote, topic_remote, NULL, NULL);
  CU_ASSERT_FATAL (reader_remote > 0);
  dds_entity_t writer_remote = dds_create_writer (participant_remote, topic_remote, NULL, NULL);
  CU_ASSERT_FATAL (writer_remote > 0);

  /* check that a single DCPSTopic sample is received */
  unsigned char key[16];
  check_topic_samples (topic_rd, topic_name, 1, false, key, NULL);

  /* Update topic QoS (topic_data) */
  dds_qos_t *qos = dds_create_qos ();
  dds_qset_topicdata (qos, "test", 5);

  /* check that a new DCPSTopic sample is received for remote topic */
  dds_return_t ret = dds_set_qos(topic_remote, qos);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
  check_topic_samples (topic_rd, topic_name, 1, false, NULL, key);

  /* .. and for local topic: no new sample received because it is using the same topic definition as the updated remote topic */
  ret = dds_set_qos(topic, qos);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
  check_topic_samples (topic_rd, topic_name, 0, false, NULL, NULL);

  dds_delete_qos (qos);
}

CU_Test (ddsc_topic_discovery, different_type, .init = topic_discovery_init, .fini = topic_discovery_fini)
{
  tprintf ("ddsc_topic_discovery.different_type\n");

  char topic_name[100];
  create_unique_topic_name ("ddsc_topic_discovery_test_type", topic_name, 100);
  dds_entity_t participant_remote = dds_create_participant (DDS_DOMAINID2, NULL, NULL);
  CU_ASSERT_FATAL (participant_remote > 0);

  /* create topic */
  dds_entity_t topic = dds_create_topic (g_participant1, &Space_Type1_desc, topic_name, NULL, NULL);
  CU_ASSERT_FATAL (topic > 0);

  /* create reader for DCPSTopic */
  dds_entity_t topic_rd = dds_create_reader (g_participant1, DDS_BUILTIN_TOPIC_DCPSTOPIC, NULL, NULL);
  CU_ASSERT_FATAL (topic_rd > 0);

  /* create 'remote' topic with different type */
  dds_entity_t topic_remote = dds_create_topic (participant_remote, &Space_Type3_desc, topic_name, NULL, NULL);
  CU_ASSERT_FATAL (topic_remote > 0);

  /* check that a DCPSTopic sample is received for local topic and remote topic (with different key) */
  check_topic_samples (topic_rd, topic_name, 2, false, NULL, NULL);
}

#define NUM_PP 10
#define NUM_TP 30
#define DELAY_MSECS 50
static ddsrt_atomic_uint32_t terminate;
static dds_entity_t participants[NUM_PP], participants_remote[NUM_PP];
static dds_entity_t topics[NUM_PP][NUM_TP], topics_remote[NUM_PP][NUM_TP];
static dds_entity_t topic_rds[NUM_PP];

static uint32_t delete_participants_thread (void *varg)
{
  (void) varg;
  uint32_t n = NUM_PP;
  while (n-- > 0)
  {
    dds_delete (participants[n]);
    dds_delete (participants_remote[n]);
    dds_sleepfor (DDS_MSECS (DELAY_MSECS));
  }
  ddsrt_atomic_st32 (&terminate, 1);
  return 0;
}

static uint32_t delete_topics_thread (void *varg)
{
  (void) varg;
  while (!ddsrt_atomic_ld32 (&terminate))
  {
    dds_delete (((dds_entity_t *) topics)[ddsrt_random () % (NUM_PP * NUM_TP)]);
    dds_delete (((dds_entity_t *) topics_remote)[ddsrt_random () % (NUM_PP * NUM_TP)]);
    dds_sleepfor (DDS_MSECS (DELAY_MSECS / NUM_TP));
  }
  return 0;
}

static uint32_t read_topic_thread (void *varg)
{
  dds_sample_info_t sample_info[1];
  dds_return_t n;
  (void) varg;
  while (!ddsrt_atomic_ld32 (&terminate))
  {
    for (uint32_t p = 0; p < NUM_PP; p++)
    {
      dds_return_t ret;
      void *raw[1] = { NULL };
      while ((n = dds_take (topic_rds[p], raw, sample_info, 1, 1)) > 0)
      {
        ret = dds_return_loan (topic_rds[p], raw, n);
        CU_ASSERT (ret == DDS_RETCODE_OK || ret == DDS_RETCODE_BAD_PARAMETER); /* topic may be deleted */
      }
    }
  }
  return 0;
}

CU_Test (ddsc_topic_discovery, topic_qos_update, .init = topic_discovery_init, .fini = topic_discovery_fini, .timeout = 60)
{
  ddsrt_thread_t tid;
  ddsrt_threadattr_t tattr;
  ddsrt_threadattr_init (&tattr);
  dds_return_t ret;
  ddsrt_atomic_st32 (&terminate, 0);

  tprintf ("ddsc_topic_discovery.topic_qos_update\n");

  for (uint32_t p = 0; p < NUM_PP; p++)
  {
    participants[p] = dds_create_participant (DDS_DOMAINID1, NULL, NULL);
    CU_ASSERT_FATAL (participants[p] > 0);
    participants_remote[p] = dds_create_participant (DDS_DOMAINID2, NULL, NULL);
    CU_ASSERT_FATAL (participants_remote[p] > 0);
    topic_rds[p] = dds_create_reader (participants[p], DDS_BUILTIN_TOPIC_DCPSTOPIC, NULL, NULL);
    CU_ASSERT_FATAL (topic_rds[p] > 0);
  }

  ret = ddsrt_thread_create (&tid, "ddsc_topic_discovery_test_rd", &tattr, read_topic_thread, 0);
  CU_ASSERT_FATAL (ret == DDS_RETCODE_OK);

  dds_qos_t *qos = dds_create_qos ();
  for (uint32_t p = 0; p < NUM_PP; p++)
    for (uint32_t t = 0; t < NUM_TP; t++)
    {
      uint32_t v = p * NUM_TP + t;
      dds_qset_topicdata (qos, &v, sizeof (v));
      char topic_name[100];
      create_unique_topic_name ("ddsc_topic_discovery_qos_upd", topic_name, 100);
      topics[p][t] = dds_create_topic (participants[p], &Space_Type1_desc, topic_name, qos, NULL);
      CU_ASSERT_FATAL (topics[p][t] > 0);
      topics_remote[p][t] = dds_create_topic (participants_remote[p], &Space_Type1_desc, topic_name, NULL, NULL);
      CU_ASSERT_FATAL (topics_remote[p][t] > 0);
    }

  ret = ddsrt_thread_create (&tid, "ddsc_topic_discovery_test_pp", &tattr, delete_participants_thread, 0);
  CU_ASSERT_FATAL (ret == DDS_RETCODE_OK);
  ret = ddsrt_thread_create (&tid, "ddsc_topic_discovery_test_tp", &tattr, delete_topics_thread, 0);
  CU_ASSERT_FATAL (ret == DDS_RETCODE_OK);

  uint32_t v;
  uint32_t c = 0;
  while (!ddsrt_atomic_ld32 (&terminate))
  {
    for (uint32_t p = 0; p < NUM_PP; p++)
    {
      for (uint32_t t = 0; t < NUM_TP; t++)
      {
        v = ddsrt_random ();
        dds_qset_topicdata (qos, &v, sizeof (v));
        ret = dds_set_qos (topics[p][t], qos);
        CU_ASSERT_FATAL (ret == DDS_RETCODE_OK || ret == DDS_RETCODE_BAD_PARAMETER); /* topic may be deleted */

        v = ddsrt_random ();
        dds_qset_topicdata (qos, &v, sizeof (v));
        ret = dds_set_qos(topics_remote[p][t], qos);
        CU_ASSERT_FATAL(ret == DDS_RETCODE_OK || ret == DDS_RETCODE_BAD_PARAMETER); /* topic may be deleted */

        dds_sleepfor (DDS_MSECS (1));
        c++;
      }
    }
    dds_sleepfor (DDS_MSECS (DELAY_MSECS));
  }
  dds_delete_qos (qos);
  tprintf ("%u qos updates\n", c);
}
