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
#include "WriteTypes.h"

#include "dds/ddsrt/process.h"
#include "dds/ddsrt/threads.h"
#include "dds/ddsrt/environ.h"

#define DDS_DOMAINID_PUB 0
#define DDS_DOMAINID_SUB 1
#define DDS_CONFIG_NO_PORT_GAIN "${CYCLONEDDS_URI}${CYCLONEDDS_URI:+,}<Discovery><ExternalDomainId>0</ExternalDomainId></Discovery>"
#define DDS_CONFIG_NO_PORT_GAIN_LOG "${CYCLONEDDS_URI}${CYCLONEDDS_URI:+,}<Tracing><OutputFile>cyclonedds_writetypes_various.${CYCLONEDDS_DOMAIN_ID}.${CYCLONEDDS_PID}.log</OutputFile><Verbosity>finest</Verbosity></Tracing><Discovery><ExternalDomainId>0</ExternalDomainId></Discovery>"

static uint32_t g_topic_nr = 0;
static dds_entity_t g_pub_domain = 0;
static dds_entity_t g_pub_participant = 0;
static dds_entity_t g_pub_publisher = 0;

static dds_entity_t g_sub_domain = 0;
static dds_entity_t g_sub_participant = 0;
static dds_entity_t g_sub_subscriber = 0;

static char *create_topic_name (const char *prefix, uint32_t nr, char *name, size_t size)
{
  /* Get unique g_topic name. */
  ddsrt_pid_t pid = ddsrt_getpid ();
  ddsrt_tid_t tid = ddsrt_gettid ();
  (void) snprintf (name, size, "%s%d_pid%" PRIdPID "_tid%" PRIdTID "", prefix, nr, pid, tid);
  return name;
}

static void writetypes_init(void)
{
  /* Domains for pub and sub use a different domain id, but the portgain setting
   * in configuration is 0, so that both domains will map to the same port number.
   * This allows to create two domains in a single test process. */
  char *conf_pub = ddsrt_expand_envvars (DDS_CONFIG_NO_PORT_GAIN, DDS_DOMAINID_PUB);
  char *conf_sub = ddsrt_expand_envvars (DDS_CONFIG_NO_PORT_GAIN, DDS_DOMAINID_SUB);
  g_pub_domain = dds_create_domain (DDS_DOMAINID_PUB, conf_pub);
  g_sub_domain = dds_create_domain (DDS_DOMAINID_SUB, conf_sub);
  dds_free (conf_pub);
  dds_free (conf_sub);

  g_pub_participant = dds_create_participant (DDS_DOMAINID_PUB, NULL, NULL);
  CU_ASSERT_FATAL (g_pub_participant > 0);
  g_sub_participant = dds_create_participant (DDS_DOMAINID_SUB, NULL, NULL);
  CU_ASSERT_FATAL (g_sub_participant > 0);

  g_pub_publisher = dds_create_publisher (g_pub_participant, NULL, NULL);
  CU_ASSERT_FATAL (g_pub_publisher > 0);
  g_sub_subscriber = dds_create_subscriber (g_sub_participant, NULL, NULL);
  CU_ASSERT_FATAL (g_sub_subscriber > 0);
}

static void writetypes_fini (void)
{
  dds_delete (g_sub_subscriber);
  dds_delete (g_pub_publisher);
  dds_delete (g_sub_participant);
  dds_delete (g_pub_participant);
  dds_delete (g_sub_domain);
  dds_delete (g_pub_domain);
}

typedef bool (*compare_fn_t) (const void *a, const void *b);

#define ABCD_CMP(typ_)                                                  \
  static bool typ_##_cmp (const void *va, const void *vb)               \
  {                                                                     \
    const struct WriteTypes_##typ_ *a = va;                             \
    const struct WriteTypes_##typ_ *b = vb;                             \
    return a->k[0] == b->k[0] && a->k[1] == b->k[1] && a->k[2] == b->k[2] && a->ll == b->ll; \
  }
ABCD_CMP (a)
ABCD_CMP (b)
ABCD_CMP (c)
ABCD_CMP (d)
#undef ABCD_CMP

struct sample {
  bool in_result;
  const void *data;
};
#define S(n) &(struct WriteTypes_##n)
static const struct sample a_samples[] = {
  { 1, S(a) { .k={1,2,3}, .ll = UINT64_C (0x1234567890abcdef) } },
  { 0, S(a) { .k={3,2,1}, .ll = UINT64_C (0) } },
  { 1, S(a) { .k={3,2,1}, .ll = UINT64_C (1) } },
};
static const struct sample b_samples[] = {
  { 1, S(b) { .k={1001,1002,1003}, .ll = UINT64_C (0x1234567890abcdef) } },
  { 0, S(b) { .k={1003,1002,1001}, .ll = UINT64_C (0) } },
  { 1, S(b) { .k={1003,1002,1001}, .ll = UINT64_C (1) } },
};
static const struct sample c_samples[] = {
  { 1, S(c) { .k={12340001,12340002,12340003}, .ll = UINT64_C (0x1234567890abcdef) } },
  { 0, S(c) { .k={12340003,12340002,12340001}, .ll = UINT64_C (0) } },
  { 1, S(c) { .k={12340003,12340002,12340001}, .ll = UINT64_C (1) } },
};
static const struct sample d_samples[] = {
  { 1, S(d) { .k={123400056780001,2,3}, .ll = UINT64_C (0x1234567890abcdef) } },
  { 0, S(d) { .k={123400056780003,2,1}, .ll = UINT64_C (0) } },
  { 1, S(d) { .k={123400056780003,2,1}, .ll = UINT64_C (1) } },
};
#undef S

#define T(n) &WriteTypes_##n##_desc
#define C(n) &n##_cmp
#define N(n) (sizeof (n##_samples) / sizeof (n##_samples[0]))
#define S(n) n##_samples
CU_TheoryDataPoints(ddsc_writetypes, various) = {
  CU_DataPoints(const dds_topic_descriptor_t *, T(a), T(b), T(c), T(d)),
  CU_DataPoints(compare_fn_t,                   C(a), C(b), C(c), C(d)),
  CU_DataPoints(size_t,                         N(a), N(b), N(c), N(d)),
  CU_DataPoints(const struct sample *,          S(a), S(b), S(c), S(d)),
};
#undef S
#undef N
#undef C
#undef T

#define MAX_SAMPLES 5

CU_Theory((const dds_topic_descriptor_t *desc, compare_fn_t cmp, size_t nsamples, const struct sample *samples), ddsc_writetypes, various, .init = writetypes_init, .fini = writetypes_fini, .timeout = 10)
{
  dds_entity_t pub_topic;
  dds_entity_t sub_topic;
  dds_entity_t reader;
  dds_entity_t writer;
  dds_qos_t *qos;
  dds_return_t rc;
  char name[100];

  /* nsamples < MAX_SAMPLES so there is room for an invalid sample if we need it */
  CU_ASSERT_FATAL (nsamples < MAX_SAMPLES);

  qos = dds_create_qos ();
  CU_ASSERT_FATAL (qos != NULL);
  dds_qset_reliability (qos, DDS_RELIABILITY_RELIABLE, DDS_SECS (1));
  dds_qset_writer_data_lifecycle (qos, false);
  create_topic_name ("ddsc_writetypes_various", g_topic_nr++, name, sizeof name);
  pub_topic = dds_create_topic (g_pub_participant, desc, name, qos, NULL);
  CU_ASSERT_FATAL (pub_topic > 0);
  sub_topic = dds_create_topic (g_sub_participant, desc, name, qos, NULL);
  CU_ASSERT_FATAL (sub_topic > 0);
  dds_delete_qos (qos);

  reader = dds_create_reader (g_sub_participant, sub_topic, NULL, NULL);
  CU_ASSERT_FATAL (reader > 0);
  writer = dds_create_writer (g_pub_participant, pub_topic, NULL, NULL);
  CU_ASSERT_FATAL (writer > 0);

  /* simple-minded polling until reader/writer have matched each other */
  while (1)
  {
    dds_publication_matched_status_t st;
    rc = dds_get_publication_matched_status (writer, &st);
    CU_ASSERT_FATAL (rc == DDS_RETCODE_OK);
    if (st.current_count > 0)
      break;
    dds_sleepfor (DDS_MSECS (1));
  }
  while (1)
  {
    dds_subscription_matched_status_t st;
    rc = dds_get_subscription_matched_status (reader, &st);
    CU_ASSERT_FATAL (rc == DDS_RETCODE_OK);
    if (st.current_count > 0)
      break;
    dds_sleepfor (DDS_MSECS (1));
  }

  /* write samples */
  for (size_t i = 0; i < nsamples; i++) {
    rc = dds_write (writer, samples[i].data);
    CU_ASSERT_FATAL (rc == DDS_RETCODE_OK);
  }

  /* delete writer, wait until no matching writer: writer lingering should ensure the data
     has been delivered at that point */
  rc = dds_delete (writer);
  CU_ASSERT_FATAL (rc == DDS_RETCODE_OK);
  while (1)
  {
    dds_subscription_matched_status_t st;
    rc = dds_get_subscription_matched_status (reader, &st);
    CU_ASSERT_FATAL (rc == DDS_RETCODE_OK);
    if (st.current_count == 0)
      break;
    dds_sleepfor (DDS_MSECS (1));
  }

  /* instances are unordered; this is a woefully inefficient way of comparing the sets,
     but for the numbers of samples we do here, it really doesn't matter */
  dds_sample_info_t si[MAX_SAMPLES];
  void *xs[MAX_SAMPLES];
  xs[0] = NULL;
  int32_t n;
  n = dds_read (reader, xs, si, MAX_SAMPLES, MAX_SAMPLES);
  CU_ASSERT_FATAL (n > 0);

  size_t nvalid = 0;
  for (int32_t j = 0; j < n; j++)
  {
    if (si[j].valid_data)
      nvalid++;
  }
  for (size_t i = 0; i < nsamples; i++)
  {
    if (samples[i].in_result)
    {
      /* sample must be present, erase it by marking it invalid */
      int32_t j;
      for (j = 0; j < n; j++)
        if (si[j].valid_data && cmp (samples[i].data, xs[j]))
          break;
      CU_ASSERT (j < n);
      si[j].valid_data = 0;
      nvalid--;
    }
  }
  /* all valid samples must be accounted for */
  CU_ASSERT_FATAL (nvalid == 0);

  rc = dds_return_loan (reader, xs, n);
  CU_ASSERT_FATAL (rc == DDS_RETCODE_OK);

  /* cleanup */
  rc = dds_delete (reader);
  CU_ASSERT_FATAL (rc == DDS_RETCODE_OK);
  rc = dds_delete (sub_topic);
  CU_ASSERT_FATAL (rc == DDS_RETCODE_OK);
  rc = dds_delete (pub_topic);
  CU_ASSERT_FATAL (rc == DDS_RETCODE_OK);
}
