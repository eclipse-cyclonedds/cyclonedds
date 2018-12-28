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
#include "ddsc/dds.h"
#include "os/os.h"
#include "RoundTrip.h"
#include "CUnit/Test.h"
#include "CUnit/Theory.h"

/**************************************************************************************************
 *
 * Test fixtures
 *
 *************************************************************************************************/
static dds_entity_t g_participant      = 0;
static dds_entity_t g_topicRtmAddress  = 0;
static dds_entity_t g_topicRtmDataType = 0;

static dds_qos_t    *g_qos        = NULL;
static dds_qos_t    *g_qos_null   = NULL;
static dds_listener_t *g_listener = NULL;
static dds_listener_t *g_list_null= NULL;

#define MAX_NAME_SIZE (100)
char g_topicRtmAddressName[MAX_NAME_SIZE];
char g_topicRtmDataTypeName[MAX_NAME_SIZE];
char g_nameBuffer[MAX_NAME_SIZE];

static char*
create_topic_name(const char *prefix, char *name, size_t size)
{
    /* Get semi random g_topic name. */
    os_procId pid = os_getpid();
    uintmax_t tid = os_threadIdToInteger(os_threadIdSelf());
    (void) snprintf(name, size, "%s_pid%"PRIprocId"_tid%"PRIuMAX"", prefix, pid, tid);
    return name;
}

static void
ddsc_topic_init(void)
{
    create_topic_name("ddsc_topic_test_rtm_address",  g_topicRtmAddressName,  MAX_NAME_SIZE);
    create_topic_name("ddsc_topic_test_rtm_datatype", g_topicRtmDataTypeName, MAX_NAME_SIZE);

    g_participant = dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);
    CU_ASSERT_FATAL(g_participant > 0);

    g_topicRtmAddress = dds_create_topic(g_participant, &RoundTripModule_Address_desc,  g_topicRtmAddressName,  NULL, NULL);
    CU_ASSERT_FATAL(g_topicRtmAddress > 0);

    g_topicRtmDataType = dds_create_topic(g_participant, &RoundTripModule_DataType_desc, g_topicRtmDataTypeName, NULL, NULL);
    CU_ASSERT_FATAL(g_topicRtmDataType > 0);

    g_qos = dds_create_qos();
    g_listener = dds_create_listener(NULL);
}


static void
ddsc_topic_fini(void)
{
    dds_delete_qos(g_qos);
    dds_delete_listener(g_listener);
    dds_delete(g_topicRtmDataType);
    dds_delete(g_topicRtmAddress);
    dds_delete(g_participant);
}


/**************************************************************************************************
 *
 * These will check the topic creation in various ways.
 *
 *************************************************************************************************/
/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_topic_create, valid) = {
        CU_DataPoints(char *,           "valid",     "_VALID",     "Val1d",      "valid_",     "vA_1d"),
        CU_DataPoints(dds_qos_t**,      &g_qos_null,  &g_qos,      &g_qos_null,  &g_qos_null,  &g_qos_null),
        CU_DataPoints(dds_listener_t**, &g_list_null, &g_listener, &g_list_null, &g_list_null, &g_list_null),
};
CU_Theory((char *name, dds_qos_t **qos, dds_listener_t **listener), ddsc_topic_create, valid, .init=ddsc_topic_init, .fini=ddsc_topic_fini)
{
    dds_entity_t topic;
    dds_return_t ret;
    topic = dds_create_topic(g_participant, &RoundTripModule_DataType_desc, name, *qos, *listener);
    CU_ASSERT_FATAL(topic > 0);
    ret = dds_delete(topic);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_topic_create, invalid_qos, .init=ddsc_topic_init, .fini=ddsc_topic_fini)
{
    dds_entity_t topic;
    dds_qos_t *qos = dds_create_qos();
    OS_WARNING_MSVC_OFF(28020); /* Disable SAL warning on intentional misuse of the API */
    dds_qset_lifespan(qos, DDS_SECS(-1));
    OS_WARNING_MSVC_OFF(28020);
    topic = dds_create_topic(g_participant, &RoundTripModule_DataType_desc, "inconsistent", qos, NULL);
    CU_ASSERT_EQUAL_FATAL(dds_err_nr(topic), DDS_RETCODE_INCONSISTENT_POLICY);
    dds_delete_qos(qos);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_topic_create, non_participants, .init=ddsc_topic_init, .fini=ddsc_topic_fini)
{
    dds_entity_t topic;
    topic = dds_create_topic(g_topicRtmDataType, &RoundTripModule_DataType_desc, "non_participant", NULL, NULL);
    CU_ASSERT_EQUAL_FATAL(dds_err_nr(topic), DDS_RETCODE_ILLEGAL_OPERATION);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_topic_create, duplicate, .init=ddsc_topic_init, .fini=ddsc_topic_fini)
{
    dds_entity_t topic;
    dds_return_t ret;
    /* Creating the same topic should succeed.  */
    topic = dds_create_topic(g_participant, &RoundTripModule_DataType_desc, g_topicRtmDataTypeName, NULL, NULL);
    CU_ASSERT_FATAL(topic > 0);
    ret = dds_delete(topic);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_topic_create, same_name, .init=ddsc_topic_init, .fini=ddsc_topic_fini)
{
    dds_entity_t topic;
    /* Creating the different topic with same name should fail.  */
    topic = dds_create_topic(g_participant, &RoundTripModule_Address_desc, g_topicRtmDataTypeName, NULL, NULL);
    CU_ASSERT_EQUAL_FATAL(dds_err_nr(topic), DDS_RETCODE_PRECONDITION_NOT_MET);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_topic_create, recreate, .init=ddsc_topic_init, .fini=ddsc_topic_fini)
{
    dds_entity_t topic;
    dds_return_t ret;

    /* Re-creating previously created topic should succeed.  */
    ret = dds_delete(g_topicRtmDataType);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    topic = dds_create_topic (g_participant, &RoundTripModule_DataType_desc, g_topicRtmDataTypeName, NULL, NULL);
    CU_ASSERT_FATAL(topic > 0);

    ret = dds_delete(topic);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_topic_create, desc_null, .init=ddsc_topic_init, .fini=ddsc_topic_fini)
{
    dds_entity_t topic;
    OS_WARNING_MSVC_OFF(6387); /* Disable SAL warning on intentional misuse of the API */
    topic = dds_create_topic (g_participant, NULL, "desc_null", NULL, NULL);
    OS_WARNING_MSVC_ON(6387);
    CU_ASSERT_EQUAL_FATAL(dds_err_nr(topic), DDS_RETCODE_BAD_PARAMETER);
}
/*************************************************************************************************/

/*************************************************************************************************/


CU_TheoryDataPoints(ddsc_topic_create, invalid_names) = {
        CU_DataPoints(char *, NULL, "", "mi-dle", "-start", "end-", "1st", "Thus$", "pl+s", "t(4)"),
};
CU_Theory((char *name), ddsc_topic_create, invalid_names, .init=ddsc_topic_init, .fini=ddsc_topic_fini)
{
    dds_entity_t topic;
    topic = dds_create_topic(g_participant, &RoundTripModule_DataType_desc, name, NULL, NULL);
    CU_ASSERT_EQUAL_FATAL(dds_err_nr(topic), DDS_RETCODE_BAD_PARAMETER);
}
/*************************************************************************************************/



/**************************************************************************************************
 *
 * These will check the topic finding in various ways.
 *
 *************************************************************************************************/
/*************************************************************************************************/
CU_Test(ddsc_topic_find, valid, .init=ddsc_topic_init, .fini=ddsc_topic_fini)
{
    dds_entity_t topic;
    dds_return_t ret;

    topic = dds_find_topic(g_participant, g_topicRtmDataTypeName);
    CU_ASSERT_EQUAL_FATAL(topic, g_topicRtmDataType);

    ret = dds_delete(topic);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_topic_find, non_participants, .init=ddsc_topic_init, .fini=ddsc_topic_fini)
{
    dds_entity_t topic;
    topic = dds_find_topic(g_topicRtmDataType, "non_participant");
    CU_ASSERT_EQUAL_FATAL(dds_err_nr(topic), DDS_RETCODE_ILLEGAL_OPERATION);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_topic_find, null, .init=ddsc_topic_init, .fini=ddsc_topic_fini)
{
    dds_entity_t topic;
    OS_WARNING_MSVC_OFF(6387); /* Disable SAL warning on intentional misuse of the API */
    topic = dds_find_topic(g_participant, NULL);
    OS_WARNING_MSVC_ON(6387);
    CU_ASSERT_EQUAL_FATAL(dds_err_nr(topic), DDS_RETCODE_BAD_PARAMETER);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_topic_find, unknown, .init=ddsc_topic_init, .fini=ddsc_topic_fini)
{
    dds_entity_t topic;
    topic = dds_find_topic(g_participant, "unknown");
    CU_ASSERT_EQUAL_FATAL(dds_err_nr(topic), DDS_RETCODE_PRECONDITION_NOT_MET);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_topic_find, deleted, .init=ddsc_topic_init, .fini=ddsc_topic_fini)
{
    dds_entity_t topic;
    dds_delete(g_topicRtmDataType);
    topic = dds_find_topic(g_participant, g_topicRtmDataTypeName);
    CU_ASSERT_EQUAL_FATAL(dds_err_nr(topic), DDS_RETCODE_PRECONDITION_NOT_MET);
}
/*************************************************************************************************/



/**************************************************************************************************
 *
 * These will check getting the topic name in various ways.
 *
 *************************************************************************************************/
/*************************************************************************************************/
CU_Test(ddsc_topic_get_name, valid, .init=ddsc_topic_init, .fini=ddsc_topic_fini)
{
    char name[MAX_NAME_SIZE];
    dds_return_t ret;

    ret = dds_get_name(g_topicRtmDataType, name, MAX_NAME_SIZE);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    CU_ASSERT_STRING_EQUAL_FATAL(name, g_topicRtmDataTypeName);

    ret = dds_get_name(g_topicRtmAddress, name, MAX_NAME_SIZE);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    CU_ASSERT_STRING_EQUAL_FATAL(name, g_topicRtmAddressName);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_topic_get_name, too_small, .init=ddsc_topic_init, .fini=ddsc_topic_fini)
{
    char name[10];
    dds_return_t ret;

    ret = dds_get_name(g_topicRtmDataType, name, 10);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    g_topicRtmDataTypeName[9] = '\0';
    CU_ASSERT_STRING_EQUAL_FATAL(name, g_topicRtmDataTypeName);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_topic_get_name, non_topic, .init=ddsc_topic_init, .fini=ddsc_topic_fini)
{
    char name[MAX_NAME_SIZE];
    dds_return_t ret;
    ret = dds_get_name(g_participant, name, MAX_NAME_SIZE);
    CU_ASSERT_EQUAL_FATAL(dds_err_nr(ret), DDS_RETCODE_ILLEGAL_OPERATION);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_topic_get_name, invalid_params) = {
        CU_DataPoints(char*,  g_nameBuffer, NULL),
        CU_DataPoints(size_t, 0,            MAX_NAME_SIZE),
};
CU_Theory((char *name, size_t size), ddsc_topic_get_name, invalid_params, .init=ddsc_topic_init, .fini=ddsc_topic_fini)
{
    dds_return_t ret;
    CU_ASSERT_FATAL((name != g_nameBuffer) || (size != MAX_NAME_SIZE));
    ret = dds_get_name(g_topicRtmDataType, name, size);
    CU_ASSERT_EQUAL_FATAL(dds_err_nr(ret), DDS_RETCODE_BAD_PARAMETER);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_topic_get_name, deleted, .init=ddsc_topic_init, .fini=ddsc_topic_fini)
{
    char name[MAX_NAME_SIZE];
    dds_return_t ret;
    dds_delete(g_topicRtmDataType);
    ret = dds_get_name(g_topicRtmDataType, name, MAX_NAME_SIZE);
    CU_ASSERT_EQUAL_FATAL(dds_err_nr(ret), DDS_RETCODE_ALREADY_DELETED);
}
/*************************************************************************************************/



/**************************************************************************************************
 *
 * These will check getting the type name in various ways.
 *
 *************************************************************************************************/
/*************************************************************************************************/
CU_Test(ddsc_topic_get_type_name, valid, .init=ddsc_topic_init, .fini=ddsc_topic_fini)
{
    const char *rtmDataTypeType = "RoundTripModule::DataType";
    const char *rtmAddressType  = "RoundTripModule::Address";
    char name[MAX_NAME_SIZE];
    dds_return_t ret;

    ret = dds_get_type_name(g_topicRtmDataType, name, MAX_NAME_SIZE);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    CU_ASSERT_STRING_EQUAL_FATAL(name, rtmDataTypeType);

    ret = dds_get_type_name(g_topicRtmAddress, name, MAX_NAME_SIZE);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    CU_ASSERT_STRING_EQUAL_FATAL(name, rtmAddressType);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_topic_get_type_name, too_small, .init=ddsc_topic_init, .fini=ddsc_topic_fini)
{
    const char *rtmDataTypeType = "RoundTrip";
    char name[10];
    dds_return_t ret;

    ret = dds_get_type_name(g_topicRtmDataType, name, 10);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    CU_ASSERT_STRING_EQUAL_FATAL(name, rtmDataTypeType);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_topic_get_type_name, non_topic, .init=ddsc_topic_init, .fini=ddsc_topic_fini)
{
    char name[MAX_NAME_SIZE];
    dds_return_t ret;
    ret = dds_get_type_name(g_participant, name, MAX_NAME_SIZE);
    CU_ASSERT_EQUAL_FATAL(dds_err_nr(ret), DDS_RETCODE_ILLEGAL_OPERATION);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_topic_get_type_name, invalid_params) = {
        CU_DataPoints(char*,  g_nameBuffer, NULL),
        CU_DataPoints(size_t, 0,            MAX_NAME_SIZE),
};
CU_Theory((char *name, size_t size), ddsc_topic_get_type_name, invalid_params, .init=ddsc_topic_init, .fini=ddsc_topic_fini)
{
    dds_return_t ret;
    CU_ASSERT_FATAL((name != g_nameBuffer) || (size != MAX_NAME_SIZE));
    ret = dds_get_type_name(g_topicRtmDataType, name, size);
    CU_ASSERT_EQUAL_FATAL(dds_err_nr(ret), DDS_RETCODE_BAD_PARAMETER);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_topic_get_type_name, deleted, .init=ddsc_topic_init, .fini=ddsc_topic_fini)
{
    char name[MAX_NAME_SIZE];
    dds_return_t ret;
    dds_delete(g_topicRtmDataType);
    ret = dds_get_type_name(g_topicRtmDataType, name, MAX_NAME_SIZE);
    CU_ASSERT_EQUAL_FATAL(dds_err_nr(ret), DDS_RETCODE_ALREADY_DELETED);
}
/*************************************************************************************************/



/**************************************************************************************************
 *
 * These will set the topic qos in various ways.
 *
 *************************************************************************************************/
/*************************************************************************************************/
CU_Test(ddsc_topic_set_qos, valid, .init=ddsc_topic_init, .fini=ddsc_topic_fini)
{
    dds_return_t ret;
    /* Latency is the only one allowed to change. */
    dds_qset_latency_budget(g_qos, DDS_SECS(1));
    ret = dds_set_qos(g_topicRtmDataType, g_qos);
    CU_ASSERT_EQUAL_FATAL(dds_err_nr(ret), DDS_RETCODE_UNSUPPORTED);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_topic_set_qos, inconsistent, .init=ddsc_topic_init, .fini=ddsc_topic_fini)
{
    dds_return_t ret;
    OS_WARNING_MSVC_OFF(28020); /* Disable SAL warning on intentional misuse of the API */
    dds_qset_lifespan(g_qos, DDS_SECS(-1));
    OS_WARNING_MSVC_ON(28020);
    ret = dds_set_qos(g_topicRtmDataType, g_qos);
    CU_ASSERT_EQUAL_FATAL(dds_err_nr(ret), DDS_RETCODE_INCONSISTENT_POLICY);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_topic_set_qos, immutable, .init=ddsc_topic_init, .fini=ddsc_topic_fini)
{
    dds_return_t ret;
    dds_qset_destination_order(g_qos, DDS_DESTINATIONORDER_BY_SOURCE_TIMESTAMP); /* Immutable */
    ret = dds_set_qos(g_topicRtmDataType, g_qos);
    CU_ASSERT_EQUAL_FATAL(dds_err_nr(ret), DDS_RETCODE_IMMUTABLE_POLICY);
}
/*************************************************************************************************/
