// Copyright(c) 2006 to 2021 ZettaScale Technology and others
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

static dds_entity_t g_domain1 = 0;
static dds_entity_t g_participant1 = 0;
static dds_entity_t g_participant2 = 0;
static dds_entity_t g_topic1 = 0;
static dds_typeinfo_t *g_type_info = NULL;

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

#ifdef DDS_HAS_TOPIC_DISCOVERY
  dds_return_t ret = dds_get_typeinfo (g_topic1, &g_type_info);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);
#endif
}

static void topic_find_local_fini (void)
{
#ifdef DDS_HAS_TOPIC_DISCOVERY
  dds_free_typeinfo (g_type_info);
#endif
  dds_delete (g_domain1);
}

enum topic_find_local_domain_impl_delete_what {
  TFLDIDW_TOPIC,
  TFLDIDW_PARTICIPANT
};

static void topic_find_local_domain_impl (enum topic_find_local_domain_impl_delete_what what, int dir)
{
  assert (dir == -1 || dir == 1);
  dds_return_t ret;
  dds_entity_t topic[2];
  topic[0] = dds_find_topic (DDS_FIND_SCOPE_LOCAL_DOMAIN, g_participant1, g_topic_name_local, g_type_info, 0);
  CU_ASSERT_FATAL (topic[0] > 0);
  CU_ASSERT_NOT_EQUAL_FATAL (topic[0], g_topic1);
  CU_ASSERT_EQUAL_FATAL (dds_get_participant (topic[0]), g_participant1);
  topic[1] = dds_find_topic (DDS_FIND_SCOPE_LOCAL_DOMAIN, g_participant2, g_topic_name_local, g_type_info, 0);
  CU_ASSERT_FATAL (topic[1] > 0);
  CU_ASSERT_NOT_EQUAL_FATAL (topic[1], topic[0]);
  CU_ASSERT_NOT_EQUAL_FATAL (topic[1], g_topic1);
  CU_ASSERT_EQUAL_FATAL (dds_get_participant (topic[1]), g_participant2);
  // The topics are in different participants and so may not have any dependencies. There
  // is no way to directly observe this in the API but deleting the entities in various
  // orders, and deleting the topics explicitly vs letting it be done implicitly by
  // deleting the participants allows us to catch at least some of these.
  dds_entity_t to_delete[] = { topic[dir > 0 ? 0 : 1], topic[dir > 0 ? 1 : 0] };
  if (what == TFLDIDW_PARTICIPANT)
  {
    // we already checked dds_get_participant returns the expected result
    for (size_t i = 0; i < sizeof (to_delete) / sizeof (to_delete[0]); i++)
      to_delete[i] = dds_get_participant (to_delete[i]);
  }
  ret = dds_delete (to_delete[0]);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);
  ret = dds_delete (to_delete[1]);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);
}

CU_Test(ddsc_topic_find_local, domain, .init = topic_find_local_init, .fini = topic_find_local_fini)
{
  topic_find_local_domain_impl (TFLDIDW_TOPIC, 1);
}

CU_Test(ddsc_topic_find_local, domain_delete_reversed, .init = topic_find_local_init, .fini = topic_find_local_fini)
{
  topic_find_local_domain_impl (TFLDIDW_TOPIC, -1);
}

CU_Test(ddsc_topic_find_local, domain_delete_pp, .init = topic_find_local_init, .fini = topic_find_local_fini)
{
  topic_find_local_domain_impl (TFLDIDW_PARTICIPANT, 1);
}

CU_Test(ddsc_topic_find_local, domain_delete_pp_reversed, .init = topic_find_local_init, .fini = topic_find_local_fini)
{
  topic_find_local_domain_impl (TFLDIDW_PARTICIPANT, -1);
}

CU_Test(ddsc_topic_find_local, participant, .init = topic_find_local_init, .fini = topic_find_local_fini)
{
  dds_entity_t topic = dds_find_topic (DDS_FIND_SCOPE_PARTICIPANT, g_participant1, g_topic_name_local, g_type_info, 0);
  CU_ASSERT_FATAL (topic > 0);
  CU_ASSERT_NOT_EQUAL_FATAL (topic, g_topic1);
}

CU_Test(ddsc_topic_find_local, non_participants, .init = topic_find_local_init, .fini = topic_find_local_fini)
{
  dds_entity_t topic = dds_find_topic (DDS_FIND_SCOPE_PARTICIPANT, g_topic1, "non_participant", g_type_info, 0);
  CU_ASSERT_EQUAL_FATAL (topic, DDS_RETCODE_BAD_PARAMETER);
}

CU_Test(ddsc_topic_find_local, null, .init = topic_find_local_init, .fini = topic_find_local_fini)
{
  DDSRT_WARNING_MSVC_OFF (6387); /* Disable SAL warning on intentional misuse of the API */
  dds_entity_t topic = dds_find_topic (DDS_FIND_SCOPE_PARTICIPANT, g_participant1, NULL, g_type_info, 0);
  DDSRT_WARNING_MSVC_ON (6387);
  CU_ASSERT_EQUAL_FATAL (topic, DDS_RETCODE_BAD_PARAMETER);
}

CU_Test(ddsc_topic_find_local, unknown, .init = topic_find_local_init, .fini = topic_find_local_fini)
{
  dds_entity_t topic = dds_find_topic (DDS_FIND_SCOPE_PARTICIPANT, g_participant1, "unknown", g_type_info, 0);
  CU_ASSERT_EQUAL_FATAL (topic, 0);
}

CU_Test(ddsc_topic_find_local, deleted, .init = topic_find_local_init, .fini = topic_find_local_fini)
{
  dds_delete (g_topic1);
  dds_entity_t topic = dds_find_topic (DDS_FIND_SCOPE_PARTICIPANT, g_participant1, g_topic_name_local, g_type_info, 0);
  CU_ASSERT_EQUAL_FATAL (topic, 0);
}
