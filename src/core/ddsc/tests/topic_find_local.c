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
#include "dds/ddsrt/heap.h"
#include "dds/ddsi/ddsi_entity_index.h"
#include "dds/ddsi/q_entity.h"
#include "dds/ddsi/q_whc.h"
#include "dds__entity.h"

#include "test_common.h"

#define DDS_DOMAINID1 0

static dds_entity_t g_domain1 = 0;
static dds_entity_t g_participant1 = 0;
static dds_entity_t g_participant2 = 0;
static dds_entity_t g_topic1 = 0;

#define MAX_NAME_SIZE (100)
char g_topic_name_local[MAX_NAME_SIZE];

static void topic_find_local_init (void)
{
  const char *cyclonedds_uri = "";
  (void) ddsrt_getenv ("CYCLONEDDS_URI", &cyclonedds_uri);
  g_domain1 = dds_create_domain (DDS_DOMAINID1, cyclonedds_uri);
  CU_ASSERT_FATAL (g_domain1 > 0);

  g_participant1 = dds_create_participant (DDS_DOMAINID1, NULL, NULL);
  CU_ASSERT_FATAL (g_participant1 > 0);

  g_participant2 = dds_create_participant (DDS_DOMAINID1, NULL, NULL);
  CU_ASSERT_FATAL (g_participant2 > 0);

  create_unique_topic_name("ddsc_topic_find_test1", g_topic_name_local, MAX_NAME_SIZE);
  g_topic1 = dds_create_topic (g_participant1, &Space_Type1_desc, g_topic_name_local, NULL, NULL);
  CU_ASSERT_FATAL (g_topic1 > 0);
}

static void topic_find_local_fini (void)
{
  dds_delete (g_domain1);
}

CU_Test(ddsc_topic_find_local, domain, .init = topic_find_local_init, .fini = topic_find_local_fini)
{
  dds_return_t ret;
  dds_entity_t topic1 = dds_find_topic_scoped (DDS_FIND_SCOPE_LOCAL_DOMAIN, g_participant1, g_topic_name_local, 0);
  CU_ASSERT_FATAL (topic1 > 0);
  CU_ASSERT_NOT_EQUAL_FATAL (topic1, g_topic1);
  dds_entity_t topic2 = dds_find_topic_scoped (DDS_FIND_SCOPE_LOCAL_DOMAIN, g_participant2, g_topic_name_local, 0);
  CU_ASSERT_FATAL (topic2 > 0);
  CU_ASSERT_NOT_EQUAL_FATAL (topic2, topic1);
  CU_ASSERT_NOT_EQUAL_FATAL (topic2, g_topic1);
  ret = dds_delete (topic1);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);
  ret = dds_delete (topic2);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);
}

CU_Test(ddsc_topic_find_local, participant, .init = topic_find_local_init, .fini = topic_find_local_fini)
{
  dds_entity_t topic = dds_find_topic_scoped (DDS_FIND_SCOPE_PARTICIPANT, g_participant1, g_topic_name_local, 0);
  CU_ASSERT_FATAL (topic > 0);
  CU_ASSERT_NOT_EQUAL_FATAL (topic, g_topic1);
}

CU_Test(ddsc_topic_find_local, non_participants, .init = topic_find_local_init, .fini = topic_find_local_fini)
{
  dds_entity_t topic = dds_find_topic_scoped (DDS_FIND_SCOPE_PARTICIPANT, g_topic1, "non_participant", 0);
  CU_ASSERT_EQUAL_FATAL (topic, DDS_RETCODE_BAD_PARAMETER);
}

CU_Test(ddsc_topic_find_local, null, .init = topic_find_local_init, .fini = topic_find_local_fini)
{
  DDSRT_WARNING_MSVC_OFF (6387); /* Disable SAL warning on intentional misuse of the API */
  dds_entity_t topic = dds_find_topic_scoped (DDS_FIND_SCOPE_PARTICIPANT, g_participant1, NULL, 0);
  DDSRT_WARNING_MSVC_ON (6387);
  CU_ASSERT_EQUAL_FATAL (topic, DDS_RETCODE_BAD_PARAMETER);
}

CU_Test(ddsc_topic_find_local, unknown, .init = topic_find_local_init, .fini = topic_find_local_fini)
{
  dds_entity_t topic = dds_find_topic_scoped (DDS_FIND_SCOPE_PARTICIPANT, g_participant1, "unknown", 0);
  CU_ASSERT_EQUAL_FATAL (topic, 0);
}

CU_Test(ddsc_topic_find_local, deleted, .init = topic_find_local_init, .fini = topic_find_local_fini)
{
  dds_delete (g_topic1);
  dds_entity_t topic = dds_find_topic_scoped (DDS_FIND_SCOPE_PARTICIPANT, g_participant1, g_topic_name_local, 0);
  CU_ASSERT_EQUAL_FATAL (topic, 0);
}
