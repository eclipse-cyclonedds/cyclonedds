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
#include <stdlib.h>
#include <string.h>
#include "CUnit/Test.h"

#include "os/os.h"
#include "ddsc/dds.h"
#include "RoundTrip.h"

static dds_entity_t participant = DDS_ENTITY_NIL;
static dds_entity_t topic = DDS_ENTITY_NIL;
static dds_entity_t publisher = DDS_ENTITY_NIL;
static dds_entity_t writer = DDS_ENTITY_NIL;

static dds_instance_handle_t handle = DDS_HANDLE_NIL;

static RoundTripModule_Address data;

/* Fixture to create prerequisite entity */
static void setup(void)
{
    participant = dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);
    CU_ASSERT_FATAL(participant > 0);
    topic = dds_create_topic(participant, &RoundTripModule_Address_desc, "ddsc_instance_get_key", NULL, NULL);
    CU_ASSERT_FATAL(topic > 0);

    publisher = dds_create_publisher(participant, NULL, NULL);
    CU_ASSERT_FATAL(publisher > 0);

    writer = dds_create_writer(publisher, topic, NULL, NULL);
    CU_ASSERT_FATAL(writer > 0);

    memset(&data, 0, sizeof(data));
    data.ip = os_strdup("some data");
    CU_ASSERT_PTR_NOT_NULL_FATAL(data.ip);
    data.port = 1;
}

/* Fixture to delete prerequisite entity */
static void teardown(void)
{
    RoundTripModule_Address_free(&data, DDS_FREE_CONTENTS);

    dds_delete(writer);
    dds_delete(publisher);
    dds_delete(topic);
    dds_delete(participant);
}

CU_Test(ddsc_instance_get_key, bad_entity, .init=setup, .fini=teardown)
{
    dds_return_t ret;

    ret = dds_instance_get_key(participant, handle, &data);
    CU_ASSERT_EQUAL(dds_err_nr(ret), DDS_RETCODE_BAD_PARAMETER);
}

CU_Test(ddsc_instance_get_key, null_data, .init=setup, .fini=teardown)
{
    dds_return_t ret;

    ret = dds_register_instance(writer, &handle, NULL);
    CU_ASSERT_EQUAL_FATAL(dds_err_nr(ret), DDS_RETCODE_BAD_PARAMETER);
}

CU_Test(ddsc_instance_get_key, null_handle, .init=setup, .fini=teardown)
{
    dds_return_t ret;
    ret = dds_register_instance(writer, &handle, &data);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    ret = dds_instance_get_key(writer, DDS_HANDLE_NIL, &data);
    CU_ASSERT_EQUAL_FATAL(dds_err_nr(ret), DDS_RETCODE_BAD_PARAMETER);
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
    CU_ASSERT_STRING_EQUAL_FATAL(key_data.ip, data.ip);
    CU_ASSERT_EQUAL_FATAL(key_data.port, data.port);
    CU_ASSERT_EQUAL_FATAL(dds_err_nr(ret), DDS_RETCODE_OK);

    RoundTripModule_Address_free(&key_data, DDS_FREE_CONTENTS);
}

