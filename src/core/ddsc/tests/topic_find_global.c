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
#include "dds/ddsi/ddsi_entity_index.h"
#include "dds/ddsi/ddsi_entity.h"
#include "ddsi__whc.h"
#include "dds__entity.h"

#include "test_common.h"

#define DDS_DOMAINID1 0
#define DDS_DOMAINID2 1
#define DDS_DOMAINID3 2

#define DDS_CONFIG_NO_PORT_GAIN "${CYCLONEDDS_URI}${CYCLONEDDS_URI:+,}<Discovery><ExternalDomainId>0</ExternalDomainId><EnableTopicDiscoveryEndpoints>true</EnableTopicDiscoveryEndpoints></Discovery>"

static dds_entity_t g_domain1 = 0;
static dds_entity_t g_participant1 = 0;
static dds_entity_t g_domain_remote1 = 0;
static dds_entity_t g_domain_remote2 = 0;

ddsrt_atomic_uint32_t g_stop;

#define MAX_NAME_SIZE (100)

static void topic_find_global_init (void)
{
  /* Domains for pub and sub use a different domain id, but the portgain setting
         * in configuration is 0, so that both domains will map to the same port number.
         * This allows to create two domains in a single test process. */
  char * conf = ddsrt_expand_envvars (DDS_CONFIG_NO_PORT_GAIN, DDS_DOMAINID1);
  g_domain1 = dds_create_domain (DDS_DOMAINID1, conf);
  CU_ASSERT_FATAL (g_domain1 > 0);
  dds_free (conf);
  conf = ddsrt_expand_envvars (DDS_CONFIG_NO_PORT_GAIN, DDS_DOMAINID2);
  g_domain_remote1 = dds_create_domain (DDS_DOMAINID2, conf);
  CU_ASSERT_FATAL (g_domain_remote1 > 0);
  dds_free (conf);
  conf = ddsrt_expand_envvars (DDS_CONFIG_NO_PORT_GAIN, DDS_DOMAINID3);
  g_domain_remote2 = dds_create_domain (DDS_DOMAINID3, conf);
  CU_ASSERT_FATAL (g_domain_remote2 > 0);
  dds_free (conf);

  g_participant1 = dds_create_participant (DDS_DOMAINID1, NULL, NULL);
  CU_ASSERT_FATAL (g_participant1 > 0);
}

static void topic_find_global_fini (void)
{
  dds_delete (g_domain1);
  dds_delete (g_domain_remote1);
  dds_delete (g_domain_remote2);
}

static void create_remote_topic (char * topic_name_remote)
{
  dds_entity_t participant_remote = dds_create_participant (DDS_DOMAINID2, NULL, NULL);
  CU_ASSERT_FATAL (participant_remote > 0);

  create_unique_topic_name ("ddsc_topic_find_remote", topic_name_remote, MAX_NAME_SIZE);
  dds_entity_t topic_remote = dds_create_topic (participant_remote, &Space_Type1_desc, topic_name_remote, NULL, NULL);
  CU_ASSERT_FATAL (topic_remote > 0);
}

static void wait_for_remote_topic (char * topic_name_remote)
{
  dds_entity_t topic_rd = dds_create_reader (g_participant1, DDS_BUILTIN_TOPIC_DCPSTOPIC, NULL, NULL);
  CU_ASSERT_FATAL (topic_rd > 0);
  dds_time_t t_exp = dds_time () + DDS_SECS (10);
  bool seen = false;
  do
  {
    void *raw[1] = { 0 };
    dds_sample_info_t sample_info[1];
    dds_return_t n;
    while ((n = dds_take (topic_rd, raw, sample_info, 1, 1)) > 0)
    {
      if (sample_info[0].valid_data && !strcmp (((dds_builtintopic_topic_t *) raw[0])->topic_name, topic_name_remote))
        seen = true;
      dds_return_loan (topic_rd, raw, n);
    }
    dds_sleepfor (DDS_MSECS (10));
  }
  while (!seen && dds_time () < t_exp);
}

static dds_typeinfo_t *get_desc_typeinfo (const dds_topic_descriptor_t *desc)
{
  ddsi_typeinfo_t *type_info = ddsi_typeinfo_deser (desc->type_information.data, desc->type_information.sz);
  CU_ASSERT_FATAL (type_info != NULL);
  return (dds_typeinfo_t *) type_info;
}

CU_Test(ddsc_topic_find_global, domain, .init = topic_find_global_init, .fini = topic_find_global_fini)
{
  char topic_name_remote[MAX_NAME_SIZE];
  create_remote_topic (topic_name_remote);
  wait_for_remote_topic (topic_name_remote);

  dds_typeinfo_t *type_info = get_desc_typeinfo (&Space_Type1_desc);
  dds_entity_t topic = dds_find_topic (DDS_FIND_SCOPE_GLOBAL, g_domain1, topic_name_remote, type_info, DDS_SECS (10));
  CU_ASSERT_EQUAL_FATAL (topic, DDS_RETCODE_BAD_PARAMETER);
  dds_free_typeinfo (type_info);
}

CU_Test(ddsc_topic_find_global, participant, .init = topic_find_global_init, .fini = topic_find_global_fini)
{
  char topic_name_remote[MAX_NAME_SIZE];
  create_remote_topic (topic_name_remote);
  wait_for_remote_topic (topic_name_remote);

  dds_typeinfo_t *type_info = get_desc_typeinfo (&Space_Type1_desc);
  dds_entity_t topic = dds_find_topic (DDS_FIND_SCOPE_GLOBAL, g_participant1, topic_name_remote, type_info, DDS_SECS (10));
  CU_ASSERT_FATAL (topic > 0);
  dds_free_typeinfo (type_info);
}

enum topic_thread_state {
  INIT,
  DONE,
  STOPPED
};

struct create_topic_thread_arg
{
  bool remote;
  ddsrt_atomic_uint32_t state;
  uint32_t num_tp;
  dds_entity_t pp;
  char topic_name_prefix[MAX_NAME_SIZE];
  const dds_topic_descriptor_t *topic_desc;
};

static void set_topic_name (char *name, size_t size, const char *prefix, uint32_t index)
{
  snprintf (name, size, "%s_%u", prefix, index);
}

static uint32_t topics_thread (void *a)
{
  char topic_name[MAX_NAME_SIZE + 11];
  struct create_topic_thread_arg *arg = (struct create_topic_thread_arg *) a;
  dds_entity_t *topics = ddsrt_malloc (arg->num_tp * sizeof (*topics));

  /* create topics */
  tprintf ("%s topics thread: creating %u topics with prefix %s\n", arg->remote ? "remote" : "local", arg->num_tp, arg->topic_name_prefix);
  for (uint32_t t = 0; t < arg->num_tp; t++)
  {
    set_topic_name (topic_name, sizeof (topic_name), arg->topic_name_prefix, t);
    topics[t] = dds_create_topic (arg->pp, arg->topic_desc, topic_name, NULL, NULL);
    CU_ASSERT_FATAL (topics[t] > 0);
  }
  ddsrt_atomic_st32 (&arg->state, DONE);
  tprintf ("%s topics thread: finished creating topics with prefix %s\n", arg->remote ? "remote" : "local", arg->topic_name_prefix);

  /* wait for stop signal */
  while (!ddsrt_atomic_ld32 (&g_stop))
    dds_sleepfor (DDS_MSECS (10));

  /* delete topics */
  for (uint32_t t = 0; t < arg->num_tp; t++)
  {
    dds_return_t ret = dds_delete (topics[t]);
    CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);
    dds_sleepfor (DDS_MSECS (1));
  }
  ddsrt_atomic_st32 (&arg->state, STOPPED);
  tprintf ("%s topics thread: deleted topics with prefix %s\n", arg->remote ? "remote" : "local", arg->topic_name_prefix);
  ddsrt_free (topics);
  return 0;
}

static dds_return_t topics_thread_state (struct create_topic_thread_arg *arg, uint32_t desired_state, dds_duration_t timeout)
{
  const dds_time_t abstimeout = dds_time () + timeout;
  while (dds_time () < abstimeout && ddsrt_atomic_ld32 (&arg->state) != desired_state)
    dds_sleepfor (DDS_MSECS (10));
  return ddsrt_atomic_ld32 (&arg->state) == desired_state ? DDS_RETCODE_OK : DDS_RETCODE_TIMEOUT;
}

CU_TheoryDataPoints (ddsc_topic_find_global, find_delete_topics) = {
    CU_DataPoints (uint32_t,     1,  5,  0,  5), /* number of local participants */
    CU_DataPoints (uint32_t,     1,  0,  5,  5), /* number of remote participants */
    CU_DataPoints (uint32_t,     1, 50, 50, 50), /* number of topics per participant */
};

CU_Theory ((uint32_t num_local_pp, uint32_t num_remote_pp, uint32_t num_tp), ddsc_topic_find_global, find_delete_topics, .init = topic_find_global_init, .fini = topic_find_global_fini, .timeout = 60)
{
  tprintf("ddsc_topic_find_global.find_delete_topics: %u/%u local/remote participants, %u topics\n", num_local_pp, num_remote_pp, num_tp);
  dds_return_t ret;
  dds_entity_t participant_remote = dds_create_participant (DDS_DOMAINID2, NULL, NULL);
  CU_ASSERT_FATAL (participant_remote > 0);
  ddsrt_atomic_st32 (&g_stop, 0);
  char topic_name[MAX_NAME_SIZE + 10];

  /* Start threads that create topics on local and remote participant] */
  struct create_topic_thread_arg *create_args = ddsrt_malloc ((num_local_pp + num_remote_pp) * sizeof (*create_args));
  for (uint32_t n = 0; n < num_local_pp + num_remote_pp; n++)
  {
    bool remote = n >= num_local_pp;
    create_args[n].remote = remote;
    ddsrt_atomic_st32 (&create_args[n].state, INIT);
    create_args[n].num_tp = num_tp;
    create_args[n].pp = remote ? participant_remote : g_participant1;
    create_unique_topic_name ("ddsc_topic_find_global", create_args[n].topic_name_prefix, MAX_NAME_SIZE);
    create_args[n].topic_desc = (n % 3) ? (n % 3 == 1 ? &Space_Type2_desc : &Space_Type3_desc) : &Space_Type1_desc;

    ddsrt_thread_t thread_id;
    ddsrt_threadattr_t thread_attr;
    ddsrt_threadattr_init (&thread_attr);
    ret = ddsrt_thread_create (&thread_id, "create_topic", &thread_attr, topics_thread, &create_args[n]);
    CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);
  }

  /* wait for all created topics to be found */
  tprintf ("find topics\n");
  for (uint32_t n = 0; n < num_local_pp + num_remote_pp; n++)
  {
    // wait for thread to finish creating topic
    ret = topics_thread_state (&create_args[n], DONE, DDS_MSECS (10000));
    CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);

    dds_typeinfo_t *type_info = get_desc_typeinfo (create_args[n].topic_desc);
    for (uint32_t t = 0; t < create_args[n].num_tp; t++)
    {
      set_topic_name (topic_name, sizeof (topic_name), create_args[n].topic_name_prefix, t);
      dds_entity_t topic = dds_find_topic (DDS_FIND_SCOPE_GLOBAL, g_participant1, topic_name, (dds_typeinfo_t *) type_info, DDS_SECS (5));
      CU_ASSERT_FATAL (topic > 0);
    }

    ddsi_typeinfo_fini (type_info);
    ddsrt_free (type_info);
  }

  /* Stop threads (which will delete their topics) and keep looking
     for these topics (we're not interested in the result) */
  ddsrt_atomic_st32 (&g_stop, 1);
  const dds_time_t abstimeout = dds_time () + DDS_MSECS (500);
  uint32_t t = 0;
  do
  {
    set_topic_name (topic_name, sizeof (topic_name), create_args->topic_name_prefix, t);
    (void) dds_find_topic (DDS_FIND_SCOPE_PARTICIPANT, g_participant1, topic_name, NULL, 0);
    (void) dds_find_topic (DDS_FIND_SCOPE_PARTICIPANT, g_participant1, topic_name, NULL, DDS_MSECS (1));
    (void) dds_find_topic (DDS_FIND_SCOPE_GLOBAL, g_participant1, topic_name, NULL, 0);
    (void) dds_find_topic (DDS_FIND_SCOPE_GLOBAL, g_participant1, topic_name, NULL, DDS_MSECS (1));
    dds_sleepfor (DDS_MSECS (1));
    if (++t == num_local_pp + num_remote_pp)
      t = 0;
  } while (dds_time () < abstimeout);

  /* Cleanup */
  for (uint32_t n = 0; n < num_local_pp + num_remote_pp; n++)
  {
    ret = topics_thread_state (&create_args[n], STOPPED, DDS_SECS (20));
    CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);
  }
  ddsrt_free (create_args);
}

CU_Test (ddsc_topic_find_global, same_name, .init = topic_find_global_init, .fini = topic_find_global_fini, .timeout = 30)
{
  dds_entity_t participant_remote1 = dds_create_participant (DDS_DOMAINID2, NULL, NULL);
  CU_ASSERT_FATAL (participant_remote1 > 0);
  dds_entity_t participant_remote2 = dds_create_participant (DDS_DOMAINID3, NULL, NULL);
  CU_ASSERT_FATAL (participant_remote2 > 0);

  /* create 2 topics with same name, different type */
  char topic_name[MAX_NAME_SIZE];
  create_unique_topic_name ("ddsc_topic_find_global_same_name", topic_name, MAX_NAME_SIZE);
  dds_entity_t topic_remote1 = dds_create_topic (participant_remote1, &Space_Type1_desc, topic_name, NULL, NULL);
  CU_ASSERT_FATAL (topic_remote1 > 0);
  dds_entity_t topic_remote2 = dds_create_topic (participant_remote2, &Space_Type2_desc, topic_name, NULL, NULL);
  CU_ASSERT_FATAL (topic_remote2 > 0);

  /* Wait for both topics to be discovered */
  dds_entity_t topic_rd = dds_create_reader (g_participant1, DDS_BUILTIN_TOPIC_DCPSTOPIC, NULL, NULL);
  CU_ASSERT_FATAL (topic_rd > 0);
  dds_time_t t_exp = dds_time () + DDS_SECS (10);
  uint32_t seen = 0;
  do
  {
    void *raw[1] = { 0 };
    dds_sample_info_t sample_info[1];
    dds_return_t n;
    while ((n = dds_take (topic_rd, raw, sample_info, 1, 1)) > 0)
    {
      if (sample_info[0].valid_data && !strcmp (((dds_builtintopic_topic_t *) raw[0])->topic_name, topic_name))
        seen++;
      dds_return_loan (topic_rd, raw, n);
    }
    dds_sleepfor (DDS_MSECS (10));
  }
  while (seen < 2 && dds_time () < t_exp);

  ddsi_typeinfo_t *typeinfo1, *typeinfo2;
  dds_return_t ret;
  ret = dds_get_typeinfo (topic_remote1, &typeinfo1);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);
  ret = dds_get_typeinfo (topic_remote2, &typeinfo2);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);

  dds_entity_t topic1 = dds_find_topic (DDS_FIND_SCOPE_GLOBAL, g_participant1, topic_name, typeinfo1, DDS_SECS (5));
  CU_ASSERT_FATAL (topic1 > 0);
  dds_entity_t topic2 = dds_find_topic (DDS_FIND_SCOPE_GLOBAL, g_participant1, topic_name, typeinfo2, DDS_SECS (5));
  CU_ASSERT_FATAL (topic2 > 0);

  dds_free_typeinfo (typeinfo1);
  dds_free_typeinfo (typeinfo2);
}
