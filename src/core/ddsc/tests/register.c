// Copyright(c) 2006 to 2020 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <assert.h>

#include "dds/dds.h"
#include "dds/ddsrt/misc.h"
#include "dds/ddsrt/process.h"
#include "dds/ddsrt/threads.h"

#include "test_common.h"

/**************************************************************************************************
 *
 * Test fixtures
 *
 *************************************************************************************************/
#define MAX_SAMPLES                 7
#define INITIAL_SAMPLES             2


static dds_entity_t g_participant = 0;
static dds_entity_t g_topic       = 0;
static dds_entity_t g_reader      = 0;
static dds_entity_t g_writer      = 0;
static dds_entity_t g_waitset     = 0;

static dds_time_t   g_past        = 0;
static dds_time_t   g_present     = 0;

static void*             g_samples[MAX_SAMPLES];
static Space_Type1       g_data[MAX_SAMPLES];

static void
registering_init(void)
{
    Space_Type1 sample = { 0, 0, 0 };
    dds_qos_t *qos = dds_create_qos ();
    dds_attach_t triggered;
    dds_return_t ret;
    char name[100];

    /* Use by source timestamp to be able to check the time related funtions. */
    dds_qset_destination_order(qos, DDS_DESTINATIONORDER_BY_SOURCE_TIMESTAMP);

    g_participant = dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);
    CU_ASSERT_FATAL(g_participant > 0);

    g_waitset = dds_create_waitset(g_participant);
    CU_ASSERT_FATAL(g_waitset > 0);

    g_topic = dds_create_topic(g_participant, &Space_Type1_desc, create_unique_topic_name("ddsc_registering_test", name, sizeof name), qos, NULL);
    CU_ASSERT_FATAL(g_topic > 0);

    /* Create a reader that keeps one sample on three instances. */
    dds_qset_reliability(qos, DDS_RELIABILITY_RELIABLE, DDS_MSECS(100));
    dds_qset_resource_limits(qos, DDS_LENGTH_UNLIMITED, 3, 1);
    g_reader = dds_create_reader(g_participant, g_topic, qos, NULL);
    CU_ASSERT_FATAL(g_reader > 0);

    /* Create a writer that will not automatically dispose unregistered samples. */
    dds_qset_writer_data_lifecycle(qos, false);
    g_writer = dds_create_writer(g_participant, g_topic, qos, NULL);
    CU_ASSERT_FATAL(g_writer > 0);

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

    /* Write initial samples. */
    for (int i = 0; i < INITIAL_SAMPLES; i++) {
        sample.long_1 = i;
        sample.long_2 = i*2;
        sample.long_3 = i*3;
        ret = dds_write(g_writer, &sample);
        CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    }

    /* Initialize reading buffers. */
    memset (g_data, 0, sizeof (g_data));
    for (int i = 0; i < MAX_SAMPLES; i++) {
        g_samples[i] = &g_data[i];
    }

    /* Initialize times. */
    g_present = dds_time();
    g_past    = g_present - DDS_SECS(1);

    dds_delete_qos(qos);
}

static void
registering_fini(void)
{
    dds_delete(g_reader);
    dds_delete(g_writer);
    dds_delete(g_waitset);
    dds_delete(g_topic);
    dds_delete(g_participant);
}


/**************************************************************************************************
 *
 * These will check the dds_register_instance() in various ways.
 *
 *************************************************************************************************/
/*************************************************************************************************/
CU_Test(ddsc_register_instance, deleted_entity, .init=registering_init, .fini=registering_fini)
{
    dds_return_t ret;
    dds_instance_handle_t handle;
    dds_delete(g_writer);
    ret = dds_register_instance(g_writer, &handle, g_data);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
}

static dds_instance_handle_t hndle = 0;
static Space_Type1           data;
CU_TheoryDataPoints(ddsc_register_instance, invalid_params) = {
        CU_DataPoints(dds_instance_handle_t *, &hndle,  NULL),
        CU_DataPoints(void*,                    NULL,  &data)
};
CU_Theory((dds_instance_handle_t *hndl2, void *datap), ddsc_register_instance, invalid_params, .init=registering_init, .fini=registering_fini)
{
    dds_return_t ret;

    /* Only test when the combination of parameters is actually invalid.*/
    CU_ASSERT_FATAL((hndl2 == NULL) || (datap == NULL));

    DDSRT_WARNING_MSVC_OFF(6387); /* Disable SAL warning on intentional misuse of the API */
    ret = dds_register_instance(g_writer, hndl2, datap);
    DDSRT_WARNING_MSVC_ON(6387);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
}

CU_TheoryDataPoints(ddsc_register_instance, invalid_writers) = {
        CU_DataPoints(dds_entity_t, -2, -1, 0, INT_MAX, INT_MIN),
};
CU_Theory((dds_entity_t writer), ddsc_register_instance, invalid_writers, .init=registering_init, .fini=registering_fini)
{
    dds_return_t ret;
    dds_instance_handle_t handle;

    ret = dds_register_instance(writer, &handle, g_data);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
}

CU_TheoryDataPoints(ddsc_register_instance, non_writers) = {
        CU_DataPoints(dds_entity_t*, &g_waitset, &g_reader, &g_topic, &g_participant),
};
CU_Theory((dds_entity_t *writer), ddsc_register_instance, non_writers, .init=registering_init, .fini=registering_fini)
{
    dds_return_t ret;
    dds_instance_handle_t handle;
    ret = dds_register_instance(*writer, &handle, g_data);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_ILLEGAL_OPERATION);
}

CU_Test(ddsc_register_instance, registering_new_instance, .init=registering_init, .fini=registering_fini)
{
    dds_instance_handle_t instHndl, instHndl2;
    dds_return_t ret;
    Space_Type1 newInstance = { INITIAL_SAMPLES, 0, 0 };
    instHndl = dds_lookup_instance(g_writer, &newInstance);
    CU_ASSERT_EQUAL_FATAL(instHndl, DDS_HANDLE_NIL);
    ret = dds_register_instance(g_writer, &instHndl2, &newInstance);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    instHndl = dds_lookup_instance(g_writer, &newInstance);
    CU_ASSERT_EQUAL_FATAL(instHndl, instHndl2);
}

CU_Test(ddsc_register_instance, data_already_available, .init=registering_init, .fini=registering_fini)
{
    dds_instance_handle_t instHndl, instHndl2;
    dds_return_t ret;
    instHndl = dds_lookup_instance(g_writer, &g_data);
    CU_ASSERT_NOT_EQUAL_FATAL(instHndl, DDS_HANDLE_NIL);
    ret = dds_register_instance(g_writer, &instHndl2, &g_data);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    CU_ASSERT_EQUAL_FATAL(instHndl2, instHndl);
}
