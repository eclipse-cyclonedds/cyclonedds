// Copyright(c) 2006 to 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include "dds/dds.h"
#include "dds/ddsrt/misc.h"
#include "dds/ddsrt/process.h"
#include "dds/ddsrt/threads.h"

#include "test_common.h"

/* Test fixtures */
static dds_entity_t g_participant = 0;
static dds_entity_t g_topic_rtmaddr = 0;
static dds_entity_t g_topic_rtmdt = 0;

static dds_qos_t *g_qos = NULL;
static dds_qos_t *g_qos_null = NULL;
static dds_listener_t *g_listener = NULL;
static dds_listener_t *g_list_null = NULL;

#define MAX_NAME_SIZE (100)
char g_topic_rtmaddr_name[MAX_NAME_SIZE];
char g_topic_rtmdt_name[MAX_NAME_SIZE];
char g_name_buf[MAX_NAME_SIZE];

static void
ddsc_topic_init(void)
{
  create_unique_topic_name("ddsc_topic_test_rtm_address", g_topic_rtmaddr_name, MAX_NAME_SIZE);
  create_unique_topic_name("ddsc_topic_test_rtm_datatype", g_topic_rtmdt_name, MAX_NAME_SIZE);

  g_participant = dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);
  CU_ASSERT_FATAL(g_participant > 0);
  g_topic_rtmaddr = dds_create_topic(g_participant, &RoundTripModule_Address_desc, g_topic_rtmaddr_name, NULL, NULL);
  CU_ASSERT_FATAL(g_topic_rtmaddr > 0);
  g_topic_rtmdt = dds_create_topic(g_participant, &RoundTripModule_DataType_desc, g_topic_rtmdt_name, NULL, NULL);
  CU_ASSERT_FATAL(g_topic_rtmdt > 0);
  g_qos = dds_create_qos();
  g_listener = dds_create_listener(NULL);
}

static void
ddsc_topic_fini(void)
{
  dds_delete_qos(g_qos);
  dds_delete_listener(g_listener);
  dds_delete(g_topic_rtmdt);
  dds_delete(g_topic_rtmaddr);
  dds_delete(g_participant);
}

/* These will check the topic creation in various ways */
CU_TheoryDataPoints(ddsc_topic_create, valid) = {
    CU_DataPoints(char *, "valid", "_VALID", "Val1d", "valid_", "vA_1d", "1valid", "valid::topic::name", "val-id", "val.id", "val/id" ),
    CU_DataPoints(dds_qos_t **, &g_qos_null, &g_qos, &g_qos_null, &g_qos_null, &g_qos_null, &g_qos_null, &g_qos_null, &g_qos_null, &g_qos_null, &g_qos_null),
    CU_DataPoints(dds_listener_t **, &g_list_null, &g_listener, &g_list_null, &g_list_null, &g_list_null, &g_list_null, &g_list_null, &g_list_null, &g_list_null, &g_list_null),
};
CU_Theory((char *name, dds_qos_t **qos, dds_listener_t **listener), ddsc_topic_create, valid, .init = ddsc_topic_init, .fini = ddsc_topic_fini)
{
  dds_entity_t topic = dds_create_topic(g_participant, &RoundTripModule_DataType_desc, name, *qos, *listener);
  CU_ASSERT_FATAL(topic > 0);
  dds_return_t ret = dds_delete(topic);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
}

CU_Test(ddsc_topic_create, invalid_qos, .init = ddsc_topic_init, .fini = ddsc_topic_fini)
{
  dds_qos_t *qos = dds_create_qos();
  DDSRT_WARNING_MSVC_OFF(28020); /* Disable SAL warning on intentional misuse of the API */
  dds_qset_resource_limits(qos, 1, 1, 2);
  DDSRT_WARNING_MSVC_OFF(28020);
  dds_entity_t topic = dds_create_topic(g_participant, &RoundTripModule_DataType_desc, "inconsistent", qos, NULL);
  CU_ASSERT_EQUAL_FATAL(topic, DDS_RETCODE_INCONSISTENT_POLICY);
  dds_delete_qos(qos);
}

CU_Test(ddsc_topic_create, non_participants, .init = ddsc_topic_init, .fini = ddsc_topic_fini)
{
  dds_entity_t topic = dds_create_topic(g_topic_rtmdt, &RoundTripModule_DataType_desc, "non_participant", NULL, NULL);
  CU_ASSERT_EQUAL_FATAL(topic, DDS_RETCODE_ILLEGAL_OPERATION);
}

CU_Test(ddsc_topic_create, duplicate, .init = ddsc_topic_init, .fini = ddsc_topic_fini)
{
  /* Creating the same topic should succeed.  */
  dds_entity_t topic = dds_create_topic(g_participant, &RoundTripModule_DataType_desc, g_topic_rtmdt_name, NULL, NULL);
  CU_ASSERT_FATAL(topic > 0);
  CU_ASSERT_FATAL(topic != g_topic_rtmdt);
  dds_return_t ret = dds_delete(topic);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
  /* Old topic entity should remain in existence */
  ret = dds_get_parent(g_topic_rtmdt);
  CU_ASSERT(ret > 0);
}

CU_Test(ddsc_topic_create, same_name, .init = ddsc_topic_init, .fini = ddsc_topic_fini)
{
  /* Creating the topic with same name and different type should succeed.  */
  dds_entity_t topic = dds_create_topic(g_participant, &RoundTripModule_Address_desc, g_topic_rtmdt_name, NULL, NULL);
  CU_ASSERT_FATAL(topic > 0);
}

CU_Test(ddsc_topic_create, recreate, .init = ddsc_topic_init, .fini = ddsc_topic_fini)
{
  /* Re-creating previously created topic should succeed.  */
  dds_return_t ret = dds_delete(g_topic_rtmdt);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
  dds_entity_t topic = dds_create_topic(g_participant, &RoundTripModule_DataType_desc, g_topic_rtmdt_name, NULL, NULL);
  CU_ASSERT_FATAL(topic > 0);

  ret = dds_delete(topic);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
}

CU_Test(ddsc_topic_create, desc_null, .init = ddsc_topic_init, .fini = ddsc_topic_fini)
{
  DDSRT_WARNING_MSVC_OFF(6387); /* Disable SAL warning on intentional misuse of the API */
  dds_entity_t topic = dds_create_topic(g_participant, NULL, "desc_null", NULL, NULL);
  DDSRT_WARNING_MSVC_ON(6387);
  CU_ASSERT_EQUAL_FATAL(topic, DDS_RETCODE_BAD_PARAMETER);
}

CU_TheoryDataPoints(ddsc_topic_create, invalid_names) = {
    CU_DataPoints(char *, NULL, "", "DCPSmytopic", "sp ace", " space", "space ","cr\r\nlf", "t\tabbed", "d\"quote", "s\'quote","select*from#table","[mytopic]"),
};
CU_Theory((char *name), ddsc_topic_create, invalid_names, .init = ddsc_topic_init, .fini = ddsc_topic_fini)
{
  dds_entity_t topic = dds_create_topic(g_participant, &RoundTripModule_DataType_desc, name, NULL, NULL);
  CU_ASSERT_EQUAL_FATAL(topic, DDS_RETCODE_BAD_PARAMETER);
}

/* These will check getting the topic name in various ways */
CU_Test(ddsc_topic_get_name, valid, .init = ddsc_topic_init, .fini = ddsc_topic_fini)
{
  char name[MAX_NAME_SIZE];
  dds_return_t ret = dds_get_name(g_topic_rtmdt, name, MAX_NAME_SIZE);
  CU_ASSERT_EQUAL_FATAL(ret, (dds_return_t) strlen (g_topic_rtmdt_name));
  CU_ASSERT_STRING_EQUAL_FATAL(name, g_topic_rtmdt_name);

  ret = dds_get_name(g_topic_rtmaddr, name, MAX_NAME_SIZE);
  CU_ASSERT_EQUAL_FATAL(ret, (dds_return_t) strlen (g_topic_rtmaddr_name));
  CU_ASSERT_STRING_EQUAL_FATAL(name, g_topic_rtmaddr_name);
}

CU_Test(ddsc_topic_get_name, too_small, .init = ddsc_topic_init, .fini = ddsc_topic_fini)
{
  char name[10];
  assert (strlen (g_topic_rtmdt_name) >= sizeof (name));
  dds_return_t ret = dds_get_name(g_topic_rtmdt, name, 10);
  CU_ASSERT_FATAL (name[sizeof (name) - 1] == 0);
  CU_ASSERT_FATAL (ret == (dds_return_t) strlen (g_topic_rtmdt_name));
  CU_ASSERT_FATAL (strncmp (g_topic_rtmdt_name, name, sizeof (name) - 1) == 0);
}

CU_Test(ddsc_topic_get_name, non_topic, .init = ddsc_topic_init, .fini = ddsc_topic_fini)
{
  char name[MAX_NAME_SIZE];
  dds_return_t ret = dds_get_name(g_participant, name, MAX_NAME_SIZE);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_ILLEGAL_OPERATION);
}

CU_TheoryDataPoints(ddsc_topic_get_name, invalid_params) = {
    CU_DataPoints(char *, g_name_buf, NULL),
    CU_DataPoints(size_t, 0, MAX_NAME_SIZE),
};
CU_Theory((char *name, size_t size), ddsc_topic_get_name, invalid_params, .init = ddsc_topic_init, .fini = ddsc_topic_fini)
{
  CU_ASSERT_FATAL((name != g_name_buf) || (size != MAX_NAME_SIZE));
  dds_return_t ret = dds_get_name(g_topic_rtmdt, name, size);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
}

CU_Test(ddsc_topic_get_name, deleted, .init = ddsc_topic_init, .fini = ddsc_topic_fini)
{
  char name[MAX_NAME_SIZE];
  dds_delete(g_topic_rtmdt);
  dds_return_t ret = dds_get_name(g_topic_rtmdt, name, MAX_NAME_SIZE);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
}

/* These will check getting the type name in various ways */
CU_Test(ddsc_topic_get_type_name, valid, .init = ddsc_topic_init, .fini = ddsc_topic_fini)
{
  const char *rtmDataTypeType = "RoundTripModule::DataType";
  const char *rtmAddressType = "RoundTripModule::Address";
  char name[MAX_NAME_SIZE];
  dds_return_t ret = dds_get_type_name(g_topic_rtmdt, name, sizeof (name));
  CU_ASSERT_EQUAL_FATAL(ret, (dds_return_t) strlen (rtmDataTypeType));
  CU_ASSERT_STRING_EQUAL_FATAL(name, rtmDataTypeType);

  ret = dds_get_type_name(g_topic_rtmaddr, name, sizeof (name));
  CU_ASSERT_EQUAL_FATAL(ret, (dds_return_t) strlen (rtmAddressType));
  CU_ASSERT_STRING_EQUAL_FATAL(name, rtmAddressType);
}

CU_Test(ddsc_topic_get_type_name, too_small, .init = ddsc_topic_init, .fini = ddsc_topic_fini)
{
  const char *rtmDataTypeType = "RoundTripModule::DataType";
  char name[10];
  assert (strlen (rtmDataTypeType) >= sizeof (name));
  dds_return_t ret = dds_get_type_name(g_topic_rtmdt, name, sizeof (name));
  CU_ASSERT_FATAL (name[sizeof (name) - 1] == 0);
  CU_ASSERT_FATAL (ret == (dds_return_t) strlen (rtmDataTypeType));
  CU_ASSERT_FATAL (strncmp (rtmDataTypeType, name, sizeof (name) - 1) == 0);
}

CU_Test(ddsc_topic_get_type_name, non_topic, .init = ddsc_topic_init, .fini = ddsc_topic_fini)
{
  char name[MAX_NAME_SIZE];
  dds_return_t ret = dds_get_type_name(g_participant, name, MAX_NAME_SIZE);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_ILLEGAL_OPERATION);
}

CU_TheoryDataPoints(ddsc_topic_get_type_name, invalid_params) = {
    CU_DataPoints(char *, g_name_buf, NULL),
    CU_DataPoints(size_t, 0, MAX_NAME_SIZE),
};
CU_Theory((char *name, size_t size), ddsc_topic_get_type_name, invalid_params, .init = ddsc_topic_init, .fini = ddsc_topic_fini)
{
  CU_ASSERT_FATAL((name != g_name_buf) || (size != MAX_NAME_SIZE));
  dds_return_t ret = dds_get_type_name(g_topic_rtmdt, name, size);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
}

CU_Test(ddsc_topic_get_type_name, deleted, .init = ddsc_topic_init, .fini = ddsc_topic_fini)
{
  char name[MAX_NAME_SIZE];
  dds_delete(g_topic_rtmdt);
  dds_return_t ret = dds_get_type_name(g_topic_rtmdt, name, MAX_NAME_SIZE);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
}

/* These will set the topic qos in various ways */
CU_Test(ddsc_topic_set_qos, valid, .init = ddsc_topic_init, .fini = ddsc_topic_fini)
{
  dds_return_t ret;
  char data[10] = { 0 };
  dds_qset_topicdata(g_qos, &data, 10);
  ret = dds_set_qos(g_topic_rtmdt, g_qos);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

  dds_qset_latency_budget(g_qos, DDS_SECS(1));
  ret = dds_set_qos(g_topic_rtmdt, g_qos);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_UNSUPPORTED);
}

CU_Test(ddsc_topic_set_qos, inconsistent, .init = ddsc_topic_init, .fini = ddsc_topic_fini)
{
  DDSRT_WARNING_MSVC_OFF(28020); /* Disable SAL warning on intentional misuse of the API */
  dds_qset_lifespan(g_qos, DDS_SECS(-1));
  DDSRT_WARNING_MSVC_ON(28020);
  dds_return_t ret = dds_set_qos(g_topic_rtmdt, g_qos);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
}

CU_Test(ddsc_topic_set_qos, immutable, .init = ddsc_topic_init, .fini = ddsc_topic_fini)
{
  dds_qset_destination_order(g_qos, DDS_DESTINATIONORDER_BY_SOURCE_TIMESTAMP); /* Immutable */
  dds_return_t ret = dds_set_qos(g_topic_rtmdt, g_qos);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_IMMUTABLE_POLICY);
}
