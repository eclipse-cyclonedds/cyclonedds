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
#include "config_env.h"

#include "dds/version.h"
#include "dds__entity.h"
#include "dds/ddsi/q_entity.h"
#include "dds/ddsi/ddsi_serdata.h"
#include "dds/ddsi/ddsi_entity_index.h"
#include "dds/ddsrt/cdtors.h"
#include "dds/ddsrt/misc.h"
#include "dds/ddsrt/process.h"
#include "dds/ddsrt/threads.h"
#include "dds/ddsrt/environ.h"
#include "dds/ddsrt/atomics.h"
#include "dds/ddsrt/time.h"

#include "test_common.h"

#define DDS_DOMAINID_PUB 0
#define DDS_DOMAINID_SUB 1
#define DDS_CONFIG_NO_PORT_GAIN "${CYCLONEDDS_URI}${CYCLONEDDS_URI:+,}<Discovery><ExternalDomainId>0</ExternalDomainId></Discovery>"

#define NUM_READERS 2
#define NUM_WRITERS 1

static dds_entity_t g_pub_domain = 0;
static dds_entity_t g_pub_participant = 0;
static dds_entity_t g_pub_publisher = 0;

static dds_entity_t g_sub_domain = 0;
static dds_entity_t g_sub_participant = 0;
static dds_entity_t g_sub_subscriber = 0;

static void fastpath_init (void)
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

  g_pub_participant = dds_create_participant(DDS_DOMAINID_PUB, NULL, NULL);
  CU_ASSERT_FATAL (g_pub_participant > 0);
  g_sub_participant = dds_create_participant(DDS_DOMAINID_SUB, NULL, NULL);
  CU_ASSERT_FATAL (g_sub_participant > 0);

  g_pub_publisher = dds_create_publisher(g_pub_participant, NULL, NULL);
  CU_ASSERT_FATAL (g_pub_publisher > 0);
  g_sub_subscriber = dds_create_subscriber(g_sub_participant, NULL, NULL);
  CU_ASSERT_FATAL (g_sub_subscriber > 0);
}

static void fastpath_fini (void)
{
  dds_delete (g_sub_domain);
  dds_delete (g_pub_domain);
}

static bool get_and_check_writer_status (size_t nwr, const dds_entity_t *wrs, size_t nrd)
{
  dds_return_t rc;
  struct dds_publication_matched_status x;
  bool result = true;
  for (size_t i = 0; i < nwr; i++)
  {
    rc = dds_get_publication_matched_status (wrs[i], &x);
    CU_ASSERT_FATAL (rc == DDS_RETCODE_OK);
    if (x.current_count != nrd)
      result = false;
  }
  return result;
}

static bool get_and_check_reader_status (size_t nrd, const dds_entity_t *rds, size_t nwr)
{
  dds_return_t rc;
  struct dds_subscription_matched_status x;
  bool result = true;
  for (size_t i = 0; i < nrd; i++)
  {
    rc = dds_get_subscription_matched_status (rds[i], &x);
    CU_ASSERT_FATAL (rc == DDS_RETCODE_OK);
    if (x.current_count != nwr)
      result = false;
  }
  return result;
}

CU_Test(ddsc_fastpath, no_write, .init = fastpath_init, .fini = fastpath_fini)
{
  char name[100];
  dds_entity_t pub_topic, writers[NUM_WRITERS], sub_topic, readers[NUM_READERS];
  dds_entity_t waitset;
  dds_qos_t *qos;
  dds_return_t rc;

  waitset = dds_create_waitset (DDS_CYCLONEDDS_HANDLE);
  CU_ASSERT_FATAL (waitset > 0);

  qos = dds_create_qos ();
  CU_ASSERT_FATAL (qos != NULL);
  dds_qset_reliability (qos, DDS_RELIABILITY_RELIABLE, DDS_INFINITY);
  dds_qset_destination_order (qos, DDS_DESTINATIONORDER_BY_RECEPTION_TIMESTAMP);
  dds_qset_history (qos, DDS_HISTORY_KEEP_ALL, 0);
  create_unique_topic_name ("ddsc_fastpath_test", name, sizeof name);
  pub_topic = dds_create_topic (g_pub_participant, &Space_Type1_desc, name, qos, NULL);
  CU_ASSERT_FATAL (pub_topic > 0);
  sub_topic = dds_create_topic (g_sub_participant, &Space_Type1_desc, name, qos, NULL);
  CU_ASSERT_FATAL (sub_topic > 0);
  for (size_t i = 0; i < sizeof (writers) / sizeof (writers[0]); i++)
  {
    writers[i] = dds_create_writer (g_pub_participant, pub_topic, qos, NULL);
    CU_ASSERT_FATAL (writers[i] > 0);
  }

  for (size_t i = 0; i < sizeof (readers) / sizeof (readers[0]); i++)
  {
    readers[i] = dds_create_reader (g_sub_participant, sub_topic, qos, NULL);
    CU_ASSERT_FATAL (readers[i] > 0);
    rc = dds_set_status_mask (readers[i], DDS_SUBSCRIPTION_MATCHED_STATUS);
    CU_ASSERT_FATAL (rc == DDS_RETCODE_OK);
    rc = dds_waitset_attach (waitset, readers[i], (dds_attach_t)i);
    CU_ASSERT_FATAL (rc == DDS_RETCODE_OK);
  }

  for (size_t i = 0; i < sizeof (writers) / sizeof (writers[0]); i++)
  {
    rc = dds_set_status_mask (writers[i], DDS_PUBLICATION_MATCHED_STATUS);
    CU_ASSERT_FATAL (rc == DDS_RETCODE_OK);
    rc = dds_waitset_attach (waitset, writers[i], -(dds_attach_t)i - 1);
    CU_ASSERT_FATAL (rc == DDS_RETCODE_OK);
  }

  printf ("match all readers/writers\n");
  bool rds = false, wrs = false;
  do
  {
    rc = dds_waitset_wait (waitset, NULL, 0, DDS_SECS(10));
    CU_ASSERT_FATAL (rc >= 1);
    wrs = get_and_check_writer_status (sizeof (writers) / sizeof (writers[0]), writers, sizeof (readers) / sizeof (readers[0]));
    rds = get_and_check_reader_status (sizeof (readers) / sizeof (readers[0]), readers, sizeof (writers) / sizeof (writers[0]));
  }
  while (!wrs || !rds);

  printf ("set fastpath off for all readers\n");
  for (size_t i = 0; i < sizeof (readers) / sizeof (readers[0]); i++)
    waitfor_or_reset_fastpath (readers[i], false, sizeof (writers) / sizeof (writers[0]));

  printf ("wait for fastpath restored on all readers\n");
  for (size_t i = 0; i < sizeof (readers) / sizeof (readers[0]); i++)
    waitfor_or_reset_fastpath (readers[i], true, sizeof (writers) / sizeof (writers[0]));

  dds_delete_qos (qos);
}


CU_Test(ddsc_fastpath, pwr_sync, .init = fastpath_init, .fini = fastpath_fini)
{
  char name[100];
  dds_entity_t pub_topic, writer, sub_topic, reader;

  printf ("set sub domain deaf-mute\n");
  dds_domain_set_deafmute (g_sub_domain, true, true, DDS_INFINITY);

  dds_qos_t *wqos = dds_create_qos ();
  CU_ASSERT_FATAL (wqos != NULL);
  dds_qset_reliability (wqos, DDS_RELIABILITY_RELIABLE, DDS_INFINITY);
  dds_qset_history (wqos, DDS_HISTORY_KEEP_ALL, 0);
  dds_qset_durability (wqos, DDS_DURABILITY_TRANSIENT_LOCAL);
  create_unique_topic_name ("ddsc_fastpath_test2", name, sizeof name);
  pub_topic = dds_create_topic (g_pub_participant, &Space_Type1_desc, name, wqos, NULL);
  CU_ASSERT_FATAL (pub_topic > 0);

  writer = dds_create_writer (g_pub_participant, pub_topic, wqos, NULL);
  CU_ASSERT_FATAL (writer > 0);

  Space_Type1 sample = {0, 0, 0};
  for (int32_t i = 0; i < 3; i++)
  {
    sample.long_1 = i;
    dds_write (writer, &sample);
  }

  printf ("disable sub domain deaf-mute\n");
  dds_domain_set_deafmute (g_sub_domain, false, false, DDS_INFINITY);

  dds_qos_t *rqos = dds_create_qos ();
  CU_ASSERT_FATAL (rqos != NULL);
  dds_qset_reliability (rqos, DDS_RELIABILITY_RELIABLE, DDS_INFINITY);
  dds_qset_history (rqos, DDS_HISTORY_KEEP_LAST, 3);
  dds_qset_durability (rqos, DDS_DURABILITY_VOLATILE);
  sub_topic = dds_create_topic (g_sub_participant, &Space_Type1_desc, name, rqos, NULL);
  CU_ASSERT_FATAL (sub_topic > 0);
  reader = dds_create_reader (g_sub_participant, sub_topic, rqos, NULL);
  CU_ASSERT_FATAL (reader > 0);

  printf ("sync reader/writer\n");
  sync_reader_writer (g_sub_participant, reader, g_pub_participant, writer);

  sample.long_1 = 4;
  dds_write (writer, &sample);

  printf ("read data\n");
  void *raw[1] = { NULL };
  dds_sample_info_t si[1];
  int32_t n, n_read = 0;
  dds_return_t rc;
  while (n_read < 1)
  {
    n = dds_take (reader, raw, si, 1, 1);
    if (n > 0 && si[0].valid_data)
    {
      n_read += n;
      Space_Type1 const * const s = raw[0];
      CU_ASSERT_EQUAL_FATAL (s->long_1, 4);
    }
    rc = dds_return_loan (reader, raw, n);
    CU_ASSERT_FATAL (rc == 0);
    dds_sleepfor (DDS_MSECS (100));
  }

  printf ("wait for fastpath\n");
  waitfor_or_reset_fastpath (reader, true, 1);

  dds_delete_qos (wqos);
  dds_delete_qos (rqos);
}
