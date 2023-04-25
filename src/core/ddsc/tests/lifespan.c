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
#include "dds/ddsi/ddsi_entity_index.h"
#include "dds/ddsi/ddsi_entity.h"
#include "ddsi__whc.h"
#include "dds__entity.h"

#include "test_common.h"

static dds_entity_t g_participant = 0;
static dds_entity_t g_subscriber  = 0;
static dds_entity_t g_publisher   = 0;
static dds_entity_t g_topic       = 0;
static dds_entity_t g_reader      = 0;
static dds_entity_t g_writer      = 0;
static dds_entity_t g_waitset     = 0;
static dds_entity_t g_rcond       = 0;
static dds_entity_t g_qcond       = 0;

static void ddsi_lifespan_init(void)
{
  dds_attach_t triggered;
  dds_return_t ret;
  char name[100];
  dds_qos_t *qos;

  qos = dds_create_qos();
  CU_ASSERT_PTR_NOT_NULL_FATAL(qos);

  g_participant = dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);
  CU_ASSERT_FATAL(g_participant > 0);

  g_subscriber = dds_create_subscriber(g_participant, NULL, NULL);
  CU_ASSERT_FATAL(g_subscriber > 0);

  g_publisher = dds_create_publisher(g_participant, NULL, NULL);
  CU_ASSERT_FATAL(g_publisher > 0);

  g_waitset = dds_create_waitset(g_participant);
  CU_ASSERT_FATAL(g_waitset > 0);

  g_topic = dds_create_topic(g_participant, &Space_Type1_desc, create_unique_topic_name("ddsc_qos_lifespan_test", name, sizeof name), NULL, NULL);
  CU_ASSERT_FATAL(g_topic > 0);

  dds_qset_history(qos, DDS_HISTORY_KEEP_ALL, DDS_LENGTH_UNLIMITED);
  dds_qset_durability(qos, DDS_DURABILITY_TRANSIENT_LOCAL);
  dds_qset_reliability(qos, DDS_RELIABILITY_RELIABLE, DDS_INFINITY);
  g_writer = dds_create_writer(g_publisher, g_topic, qos, NULL);
  CU_ASSERT_FATAL(g_writer > 0);
  g_reader = dds_create_reader(g_subscriber, g_topic, qos, NULL);
  CU_ASSERT_FATAL(g_reader > 0);

  /* Sync g_reader to g_writer. */
  ret = dds_set_status_mask(g_reader, DDS_SUBSCRIPTION_MATCHED_STATUS);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
  ret = dds_waitset_attach(g_waitset, g_reader, g_reader);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
  ret = dds_waitset_wait(g_waitset, &triggered, 1, DDS_SECS(1));
  CU_ASSERT_EQUAL_FATAL(ret, 1);
  CU_ASSERT_EQUAL_FATAL(g_reader, (dds_entity_t)(intptr_t)triggered);
  ret = dds_waitset_detach(g_waitset, g_reader);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

  /* Sync g_writer to g_reader. */
  ret = dds_set_status_mask(g_writer, DDS_PUBLICATION_MATCHED_STATUS);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
  ret = dds_waitset_attach(g_waitset, g_writer, g_writer);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
  ret = dds_waitset_wait(g_waitset, &triggered, 1, DDS_SECS(1));
  CU_ASSERT_EQUAL_FATAL(ret, 1);
  CU_ASSERT_EQUAL_FATAL(g_writer, (dds_entity_t)(intptr_t)triggered);
  ret = dds_waitset_detach(g_waitset, g_writer);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

  dds_delete_qos(qos);
}

static void ddsi_lifespan_fini(void)
{
  dds_delete(g_rcond);
  dds_delete(g_qcond);
  dds_delete(g_reader);
  dds_delete(g_writer);
  dds_delete(g_subscriber);
  dds_delete(g_publisher);
  dds_delete(g_waitset);
  dds_delete(g_topic);
  dds_delete(g_participant);
}

static void check_whc_state(dds_entity_t writer, ddsi_seqno_t exp_min, ddsi_seqno_t exp_max)
{
  struct dds_entity *wr_entity;
  struct ddsi_writer *wr;
  struct ddsi_whc_state whcst;
  CU_ASSERT_EQUAL_FATAL(dds_entity_pin(writer, &wr_entity), 0);
  ddsi_thread_state_awake(ddsi_lookup_thread_state(), &wr_entity->m_domain->gv);
  wr = ddsi_entidx_lookup_writer_guid (wr_entity->m_domain->gv.entity_index, &wr_entity->m_guid);
  CU_ASSERT_FATAL(wr != NULL);
  assert(wr != NULL); /* for Clang's static analyzer */
  ddsi_whc_get_state(wr->whc, &whcst);
  ddsi_thread_state_asleep(ddsi_lookup_thread_state());
  dds_entity_unpin(wr_entity);

  CU_ASSERT_EQUAL_FATAL (whcst.min_seq, exp_min);
  CU_ASSERT_EQUAL_FATAL (whcst.max_seq, exp_max);
}

CU_Test(ddsc_lifespan, basic, .init=ddsi_lifespan_init, .fini=ddsi_lifespan_fini)
{
  Space_Type1 sample = { 0, 0, 0 };
  dds_return_t ret;
  dds_duration_t exp = DDS_MSECS(500);
  dds_qos_t *qos;

  qos = dds_create_qos();
  CU_ASSERT_PTR_NOT_NULL_FATAL(qos);

  /* Write with default qos: lifespan inifinite */
  ret = dds_write (g_writer, &sample);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);
  check_whc_state(g_writer, 1, 1);

  dds_sleepfor (2 * exp);
  check_whc_state(g_writer, 1, 1);

  dds_qset_lifespan(qos, exp);
  ret = dds_set_qos(g_writer, qos);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);
  ret = dds_write (g_writer, &sample);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);
  check_whc_state(g_writer, 2, 2);

  dds_sleepfor (2 * exp);
  check_whc_state(g_writer, 0, 0);

  dds_delete_qos(qos);
}
