// Copyright(c) 2026 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include "dds/dds.h"
#include "dds/ddsc/dds_statistics.h"
#include "dds/ddsrt/environ.h"
#include "test_common.h"

static void assert_stat_kind (const struct dds_statistics *stat, const char *name, enum dds_stat_kind kind)
{
  const struct dds_stat_keyvalue *kv = dds_lookup_statistic (stat, name);
  CU_ASSERT_NEQ_FATAL (kv, NULL);
  CU_ASSERT_STREQ (kv->name, name);
  CU_ASSERT_EQ (kv->kind, kind);
}

CU_Test(ddsc_statistics, create_refresh_lookup_delete)
{
  const char *config = "${CYCLONEDDS_URI}${CYCLONEDDS_URI:+,}<Discovery><ExternalDomainId>0</ExternalDomainId></Discovery>";
  char *rd_conf = ddsrt_expand_envvars (config, 0);
  char *wr_conf = ddsrt_expand_envvars (config, 1);
  char name[100];
  dds_entity_t rd_domain = 0;
  dds_entity_t wr_domain = 0;
  dds_entity_t rd_participant = 0;
  dds_entity_t wr_participant = 0;
  dds_entity_t rd_topic = 0;
  dds_entity_t wr_topic = 0;
  dds_entity_t writer = 0;
  dds_entity_t reader = 0;
  struct dds_statistics *wstat = NULL;
  struct dds_statistics *rstat = NULL;
  uint64_t opaque;

  CU_ASSERT_EQ (dds_refresh_statistics (NULL), DDS_RETCODE_BAD_PARAMETER);
  CU_ASSERT_EQ (dds_lookup_statistic (NULL, "anything"), NULL);
  CU_ASSERT_EQ (dds_create_statistics (0), NULL);

  CU_ASSERT_NEQ_FATAL (rd_conf, NULL);
  CU_ASSERT_NEQ_FATAL (wr_conf, NULL);
  rd_domain = dds_create_domain (0, rd_conf);
  CU_ASSERT_GT_FATAL (rd_domain, 0);
  wr_domain = dds_create_domain (1, wr_conf);
  CU_ASSERT_GT_FATAL (wr_domain, 0);
  ddsrt_free (rd_conf);
  ddsrt_free (wr_conf);

  rd_participant = dds_create_participant (0, NULL, NULL);
  CU_ASSERT_GT_FATAL (rd_participant, 0);
  wr_participant = dds_create_participant (1, NULL, NULL);
  CU_ASSERT_GT_FATAL (wr_participant, 0);
  create_unique_topic_name ("ddsc_statistics", name, sizeof (name));
  rd_topic = dds_create_topic (rd_participant, &RoundTripModule_DataType_desc, name, NULL, NULL);
  CU_ASSERT_GT_FATAL (rd_topic, 0);
  wr_topic = dds_create_topic (wr_participant, &RoundTripModule_DataType_desc, name, NULL, NULL);
  CU_ASSERT_GT_FATAL (wr_topic, 0);
  writer = dds_create_writer (wr_participant, wr_topic, NULL, NULL);
  CU_ASSERT_GT_FATAL (writer, 0);
  reader = dds_create_reader (rd_participant, rd_topic, NULL, NULL);
  CU_ASSERT_GT_FATAL (reader, 0);
  sync_reader_writer (rd_participant, reader, wr_participant, writer);

  CU_ASSERT_EQ (dds_create_statistics (rd_participant), NULL);
  CU_ASSERT_EQ (dds_create_statistics (rd_topic), NULL);

  wstat = dds_create_statistics (writer);
  CU_ASSERT_NEQ_FATAL (wstat, NULL);
  CU_ASSERT_EQ (wstat->entity, writer);
  CU_ASSERT_EQ (wstat->time, 0);
  CU_ASSERT_EQ (wstat->count, 4);
  assert_stat_kind (wstat, "rexmit_bytes", DDS_STAT_KIND_UINT64);
  assert_stat_kind (wstat, "throttle_count", DDS_STAT_KIND_UINT32);
  assert_stat_kind (wstat, "time_throttle", DDS_STAT_KIND_UINT64);
  assert_stat_kind (wstat, "time_rexmit", DDS_STAT_KIND_UINT64);
  CU_ASSERT_EQ (dds_lookup_statistic (wstat, "missing"), NULL);
  CU_ASSERT_EQ (dds_refresh_statistics (wstat), DDS_RETCODE_OK);
  CU_ASSERT_NEQ (wstat->time, 0);

  opaque = wstat->opaque;
  wstat->opaque++;
  CU_ASSERT_EQ (dds_refresh_statistics (wstat), DDS_RETCODE_BAD_PARAMETER);
  wstat->opaque = opaque;

  rstat = dds_create_statistics (reader);
  CU_ASSERT_NEQ_FATAL (rstat, NULL);
  CU_ASSERT_EQ (rstat->entity, reader);
  CU_ASSERT_EQ (rstat->count, 1);
  assert_stat_kind (rstat, "discarded_bytes", DDS_STAT_KIND_UINT64);
  CU_ASSERT_EQ (dds_refresh_statistics (rstat), DDS_RETCODE_OK);
  CU_ASSERT_NEQ (rstat->time, 0);

  CU_ASSERT_EQ (dds_delete (writer), DDS_RETCODE_OK);
  CU_ASSERT_NEQ (dds_refresh_statistics (wstat), DDS_RETCODE_OK);

  dds_delete_statistics (wstat);
  dds_delete_statistics (rstat);
  dds_delete_statistics (NULL);
  CU_ASSERT_EQ (dds_delete (rd_domain), DDS_RETCODE_OK);
  CU_ASSERT_EQ (dds_delete (wr_domain), DDS_RETCODE_OK);
}
