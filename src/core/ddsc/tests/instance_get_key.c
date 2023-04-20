// Copyright(c) 2006 to 2019 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "CUnit/Test.h"
#include "dds/dds.h"
#include "dds/ddsrt/string.h"
#include "RoundTrip.h"

#define MAX_SAMPLES 10

static dds_entity_t participant = DDS_ENTITY_NIL;
static dds_entity_t waitset = DDS_ENTITY_NIL;
static dds_entity_t topic = DDS_ENTITY_NIL;
static dds_entity_t publisher = DDS_ENTITY_NIL;
static dds_entity_t subscriber = DDS_ENTITY_NIL;
static dds_entity_t writer = DDS_ENTITY_NIL;
static dds_entity_t reader = DDS_ENTITY_NIL;
static dds_entity_t readcondition = DDS_ENTITY_NIL;
static dds_entity_t querycondition = DDS_ENTITY_NIL;
static dds_instance_handle_t handle = DDS_HANDLE_NIL;

static bool
filter(const void * sample)
{
    const RoundTripModule_Address *s = sample;
    return (s->port == 1);
}

static RoundTripModule_Address data;

/* Fixture to create prerequisite entity */
static void setup(void)
{
    uint32_t mask = DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
    dds_return_t ret;

    participant = dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);
    CU_ASSERT_FATAL(participant > 0);

    waitset = dds_create_waitset(participant);
    CU_ASSERT_FATAL(waitset > 0);

    topic = dds_create_topic(participant, &RoundTripModule_Address_desc, "ddsc_instance_get_key", NULL, NULL);
    CU_ASSERT_FATAL(topic > 0);

    publisher = dds_create_publisher(participant, NULL, NULL);
    CU_ASSERT_FATAL(publisher > 0);

    writer = dds_create_writer(publisher, topic, NULL, NULL);
    CU_ASSERT_FATAL(writer > 0);

    subscriber = dds_create_subscriber(participant, NULL, NULL);
    CU_ASSERT_FATAL(subscriber > 0);

    reader = dds_create_reader(subscriber, topic, NULL, NULL);
    CU_ASSERT_FATAL(reader > 0);

    readcondition = dds_create_readcondition(reader, mask);
    CU_ASSERT_FATAL(readcondition > 0);

    ret = dds_waitset_attach(waitset, readcondition, readcondition);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    querycondition = dds_create_querycondition(reader, mask, filter);
    CU_ASSERT_FATAL(querycondition > 0);

    ret = dds_waitset_attach(waitset, querycondition, querycondition);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    memset(&data, 0, sizeof(data));
    data.ip = ddsrt_strdup("some data");
    CU_ASSERT_PTR_NOT_NULL_FATAL(data.ip);
    data.port = 1;
}

/* Fixture to delete prerequisite entity */
static void teardown(void)
{
    RoundTripModule_Address_free(&data, DDS_FREE_CONTENTS);

    dds_delete(participant);
}

CU_Test(ddsc_instance_get_key, bad_entity, .init=setup, .fini=teardown)
{
    dds_return_t ret;

    ret = dds_instance_get_key(participant, handle, &data);
    CU_ASSERT_EQUAL(ret, DDS_RETCODE_ILLEGAL_OPERATION);
}

CU_Test(ddsc_instance_get_key, null_data, .init=setup, .fini=teardown)
{
    dds_return_t ret;

    ret = dds_register_instance(writer, &handle, NULL);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
}

CU_Test(ddsc_instance_get_key, null_handle, .init=setup, .fini=teardown)
{
    dds_return_t ret;
    ret = dds_register_instance(writer, &handle, &data);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    ret = dds_instance_get_key(writer, DDS_HANDLE_NIL, &data);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
}

CU_Test(ddsc_instance_get_key, registered_instance, .init=setup, .fini=teardown)
{
    dds_return_t ret;
    RoundTripModule_Address key_data;

    ret = dds_register_instance(writer, &handle, &data);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    memset(&key_data, 0, sizeof(key_data));

    ret = dds_instance_get_key(writer, handle, &key_data);

    CU_ASSERT_PTR_NOT_NULL_FATAL(key_data.ip);
    assert (key_data.ip != NULL); /* for the benefit of clang's static analyzer */
    CU_ASSERT_STRING_EQUAL_FATAL(key_data.ip, data.ip);
    CU_ASSERT_EQUAL_FATAL(key_data.port, data.port);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    RoundTripModule_Address_free(&key_data, DDS_FREE_CONTENTS);
}

CU_Test(ddsc_instance_get_key, readcondition, .init=setup, .fini=teardown)
{
    dds_return_t ret;
    RoundTripModule_Address key_data;

    /* The instance handle of a successful write is by
     * design the same as the instance handle for the
     * readers,readconditions and queryconditions.
     * For that reason there is no need to actually read
     * the data. It is sufficient to do a successful write
     * and use the instance handle to obtain the key_data
     * for the readcondition. */
    ret = dds_write(writer, &data);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    handle = dds_lookup_instance (writer, &data);
    CU_ASSERT_PTR_NOT_NULL_FATAL(handle);

    memset(&key_data, 0, sizeof(key_data));

    ret = dds_instance_get_key(readcondition, handle, &key_data);

    CU_ASSERT_PTR_NOT_NULL_FATAL(key_data.ip);
    assert (key_data.ip != NULL); /* for the benefit of clang's static analyzer */
    CU_ASSERT_STRING_EQUAL_FATAL(key_data.ip, data.ip);
    CU_ASSERT_EQUAL_FATAL(key_data.port, data.port);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    RoundTripModule_Address_free(&key_data, DDS_FREE_CONTENTS);
}

CU_Test(ddsc_instance_get_key, querycondition, .init=setup, .fini=teardown)
{
    dds_return_t ret;
    RoundTripModule_Address key_data;

    /* The instance handle of a successful write is by
     * design the same as the instance handle for the
     * readers,readconditions and queryconditions.
     * For that reason there is no need to actually read
     * the data. It is sufficient to do a successful write
     * and use the instance handle to obtain the key_data
     * for the querycondition. */
    ret = dds_write(writer, &data);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    handle = dds_lookup_instance (writer, &data);
    CU_ASSERT_PTR_NOT_NULL_FATAL(handle);

    memset(&key_data, 0, sizeof(key_data));

    ret = dds_instance_get_key(querycondition, handle, &key_data);

    CU_ASSERT_PTR_NOT_NULL_FATAL(key_data.ip);
    assert (key_data.ip != NULL); /* for the benefit of clang's static analyzer */
    CU_ASSERT_STRING_EQUAL_FATAL(key_data.ip, data.ip);
    CU_ASSERT_EQUAL_FATAL(key_data.port, data.port);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    RoundTripModule_Address_free(&key_data, DDS_FREE_CONTENTS);
}
