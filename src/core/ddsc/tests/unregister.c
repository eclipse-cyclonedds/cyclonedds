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
#include <limits.h>

#include "dds/dds.h"
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
static dds_sample_info_t g_info[MAX_SAMPLES];

static void
unregistering_init(void)
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

    g_topic = dds_create_topic(g_participant, &Space_Type1_desc, create_unique_topic_name("ddsc_unregistering_test", name, 100), qos, NULL);
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
unregistering_fini(void)
{
    dds_delete(g_reader);
    dds_delete(g_writer);
    dds_delete(g_waitset);
    dds_delete(g_topic);
    dds_delete(g_participant);
}


#if 0
#else
/**************************************************************************************************
 *
 * These will check the dds_unregister_instance() in various ways.
 *
 *************************************************************************************************/
/*************************************************************************************************/
CU_Test(ddsc_unregister_instance, deleted, .init=unregistering_init, .fini=unregistering_fini)
{
    dds_return_t ret;
    dds_delete(g_writer);

    ret = dds_unregister_instance(g_writer, g_data);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_unregister_instance, null, .init=unregistering_init, .fini=unregistering_fini)
{
    dds_return_t ret;
    ret = dds_unregister_instance(g_writer, NULL);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_unregister_instance, invalid_writers) = {
        CU_DataPoints(dds_entity_t, -2, -1, 0, INT_MAX, INT_MIN),
};
CU_Theory((dds_entity_t writer), ddsc_unregister_instance, invalid_writers, .init=unregistering_init, .fini=unregistering_fini)
{
    dds_return_t ret;

    ret = dds_unregister_instance(writer, g_data);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_unregister_instance, non_writers) = {
        CU_DataPoints(dds_entity_t*, &g_waitset, &g_reader, &g_topic, &g_participant),
};
CU_Theory((dds_entity_t *writer), ddsc_unregister_instance, non_writers, .init=unregistering_init, .fini=unregistering_fini)
{
    dds_return_t ret;
    ret = dds_unregister_instance(*writer, g_data);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_ILLEGAL_OPERATION);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_unregister_instance, unregistering_old_instance, .init=unregistering_init, .fini=unregistering_fini)
{
    Space_Type1 oldInstance = { 0, 22, 22 };
    dds_return_t ret;

    ret = dds_unregister_instance(g_writer, &oldInstance);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    /* Read all available samples. */
    ret = dds_read(g_reader, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
    CU_ASSERT_EQUAL_FATAL(ret, 2);
    for(int i = 0; i < ret; i++) {
        Space_Type1 *sample = (Space_Type1*)g_samples[i];
        if (sample->long_1 == 0) {
            /* Check data. */
            CU_ASSERT_EQUAL_FATAL(sample->long_2, 0);
            CU_ASSERT_EQUAL_FATAL(sample->long_3, 0);

            /* Check states. */
            CU_ASSERT_EQUAL_FATAL(g_info[i].valid_data,     true);
            CU_ASSERT_EQUAL_FATAL(g_info[i].sample_state,   DDS_SST_NOT_READ);
            CU_ASSERT_EQUAL_FATAL(g_info[i].view_state,     DDS_VST_NEW);
            CU_ASSERT_EQUAL_FATAL(g_info[i].instance_state, DDS_NOT_ALIVE_NO_WRITERS_INSTANCE_STATE);
        } else if (sample->long_1 == 1) {
            /* Check data. */
            CU_ASSERT_EQUAL_FATAL(sample->long_2, sample->long_1 * 2);
            CU_ASSERT_EQUAL_FATAL(sample->long_3, sample->long_1 * 3);

            /* Check states. */
            CU_ASSERT_EQUAL_FATAL(g_info[i].valid_data,     true);
            CU_ASSERT_EQUAL_FATAL(g_info[i].sample_state,   DDS_SST_NOT_READ);
            CU_ASSERT_EQUAL_FATAL(g_info[i].view_state,     DDS_VST_NEW);
            CU_ASSERT_EQUAL_FATAL(g_info[i].instance_state, DDS_ALIVE_INSTANCE_STATE);
        } else {
            CU_FAIL_FATAL("Unknown sample read");
        }
    }
}
/*************************************************************************************************/




/**************************************************************************************************
 *
 * These will check the dds_unregister_instance_ts() in various ways.
 *
 *************************************************************************************************/
/*************************************************************************************************/
CU_Test(ddsc_unregister_instance_ts, deleted, .init=unregistering_init, .fini=unregistering_fini)
{
    dds_return_t ret;
    dds_delete(g_writer);
    ret = dds_unregister_instance_ts(g_writer, g_data, g_present);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_unregister_instance_ts, null, .init=unregistering_init, .fini=unregistering_fini)
{
    dds_return_t ret;
    ret = dds_unregister_instance_ts(g_writer, NULL, g_present);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_unregister_instance_ts, invalid_writers) = {
        CU_DataPoints(dds_entity_t, -2, -1, 0, INT_MAX, INT_MIN),
};
CU_Theory((dds_entity_t writer), ddsc_unregister_instance_ts, invalid_writers, .init=unregistering_init, .fini=unregistering_fini)
{
    dds_return_t ret;

    ret = dds_unregister_instance_ts(writer, g_data, g_present);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_unregister_instance_ts, non_writers) = {
        CU_DataPoints(dds_entity_t*, &g_waitset, &g_reader, &g_topic, &g_participant),
};
CU_Theory((dds_entity_t *writer), ddsc_unregister_instance_ts, non_writers, .init=unregistering_init, .fini=unregistering_fini)
{
    dds_return_t ret;
    ret = dds_unregister_instance_ts(*writer, g_data, g_present);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_ILLEGAL_OPERATION);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_unregister_instance_ts, unregistering_old_instance, .init=unregistering_init, .fini=unregistering_fini)
{
    Space_Type1 oldInstance = { 0, 22, 22 };
    dds_return_t ret;

    ret = dds_unregister_instance_ts(g_writer, &oldInstance, g_present);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    /* Read all available samples. */
    ret = dds_read(g_reader, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
    CU_ASSERT_EQUAL_FATAL(ret, 2);
    for(int i = 0; i < ret; i++) {
        Space_Type1 *sample = (Space_Type1*)g_samples[i];
        if (sample->long_1 == 0) {
            /* Check data (data part of unregister is not used, only the key part). */
            CU_ASSERT_EQUAL_FATAL(sample->long_2, sample->long_1 * 2);
            CU_ASSERT_EQUAL_FATAL(sample->long_3, sample->long_1 * 3);

            /* Check states. */
            CU_ASSERT_EQUAL_FATAL(g_info[i].valid_data,     true);
            CU_ASSERT_EQUAL_FATAL(g_info[i].sample_state,   DDS_SST_NOT_READ);
            CU_ASSERT_EQUAL_FATAL(g_info[i].view_state,     DDS_VST_NEW);
            CU_ASSERT_EQUAL_FATAL(g_info[i].instance_state, DDS_NOT_ALIVE_NO_WRITERS_INSTANCE_STATE);
        } else if (sample->long_1 == 1) {
            /* Check data. */
            CU_ASSERT_EQUAL_FATAL(sample->long_2, sample->long_1 * 2);
            CU_ASSERT_EQUAL_FATAL(sample->long_3, sample->long_1 * 3);

            /* Check states. */
            CU_ASSERT_EQUAL_FATAL(g_info[i].valid_data,     true);
            CU_ASSERT_EQUAL_FATAL(g_info[i].sample_state,   DDS_SST_NOT_READ);
            CU_ASSERT_EQUAL_FATAL(g_info[i].view_state,     DDS_VST_NEW);
            CU_ASSERT_EQUAL_FATAL(g_info[i].instance_state, DDS_ALIVE_INSTANCE_STATE);
        } else {
            CU_FAIL_FATAL( "Unknown sample read");
        }
    }
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_unregister_instance_ts, unregistering_past_sample, .init=unregistering_init, .fini=unregistering_fini)
{
    Space_Type1 oldInstance = { 0, 0, 0 };
    dds_attach_t triggered;
    dds_return_t ret;

    /* Unregistering a sample in the past should trigger a lost sample. */
    ret = dds_set_status_mask(g_reader, DDS_SAMPLE_LOST_STATUS);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    ret = dds_waitset_attach(g_waitset, g_reader, g_reader);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    /* Now, unregister a sample in the past. */
    ret = dds_unregister_instance_ts(g_writer, &oldInstance, g_past);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    /* Wait for 'sample lost'. */
    ret = dds_waitset_wait(g_waitset, &triggered, 1, DDS_SECS(1));
    CU_ASSERT_EQUAL_FATAL(ret, 1);

    /* Read all available samples. */
    ret = dds_read(g_reader, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
    CU_ASSERT_EQUAL_FATAL(ret, 2);
    for(int i = 0; i < ret; i++) {
        Space_Type1 *sample = (Space_Type1*)g_samples[i];
        if ((sample->long_1 == 0) || (sample->long_1 == 1)) {
            /* Check data. */
            CU_ASSERT_EQUAL_FATAL(sample->long_2, sample->long_1 * 2);
            CU_ASSERT_EQUAL_FATAL(sample->long_3, sample->long_1 * 3);

            /* Check states. */
            CU_ASSERT_EQUAL_FATAL(g_info[i].valid_data,     true);
            CU_ASSERT_EQUAL_FATAL(g_info[i].sample_state,   DDS_SST_NOT_READ);
            CU_ASSERT_EQUAL_FATAL(g_info[i].view_state,     DDS_VST_NEW);
        } else {
            CU_FAIL_FATAL("Unknown sample read");
        }
    }

}
/*************************************************************************************************/




/**************************************************************************************************
 *
 * These will check the dds_unregister_instance_ih() in various ways.
 *
 *************************************************************************************************/
/*************************************************************************************************/
CU_Test(ddsc_unregister_instance_ih, deleted, .init=unregistering_init, .fini=unregistering_fini)
{
    dds_return_t ret;
    dds_delete(g_writer);
    ret = dds_unregister_instance_ih(g_writer, DDS_HANDLE_NIL);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_unregister_instance_ih, invalid_handles) = {
        CU_DataPoints(dds_instance_handle_t, DDS_HANDLE_NIL, 0, 1, 100, UINT64_MAX),
};
CU_Theory((dds_instance_handle_t handle), ddsc_unregister_instance_ih, invalid_handles, .init=unregistering_init, .fini=unregistering_fini)
{
    dds_return_t ret;
    ret = dds_unregister_instance_ih(g_writer, handle);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_PRECONDITION_NOT_MET);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_unregister_instance_ih, invalid_writers) = {
        CU_DataPoints(dds_entity_t, -2, -1, 0, INT_MAX, INT_MIN),
};
CU_Theory((dds_entity_t writer), ddsc_unregister_instance_ih, invalid_writers, .init=unregistering_init, .fini=unregistering_fini)
{
    dds_return_t ret;

    ret = dds_unregister_instance_ih(writer, DDS_HANDLE_NIL);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_unregister_instance_ih, non_writers) = {
        CU_DataPoints(dds_entity_t*, &g_waitset, &g_reader, &g_topic, &g_participant),
};
CU_Theory((dds_entity_t *writer), ddsc_unregister_instance_ih, non_writers, .init=unregistering_init, .fini=unregistering_fini)
{
    dds_return_t ret;
    ret = dds_unregister_instance_ih(*writer, DDS_HANDLE_NIL);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_ILLEGAL_OPERATION);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_unregister_instance_ih, unregistering_old_instance, .init=unregistering_init, .fini=unregistering_fini)
{
    Space_Type1 oldInstance = { 0, 22, 22 };
    dds_instance_handle_t hdl = dds_lookup_instance(g_writer, &oldInstance);
    dds_return_t ret;

    ret = dds_unregister_instance_ih(g_writer, hdl);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    /* Read all available samples. */
    ret = dds_read(g_reader, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
    CU_ASSERT_EQUAL_FATAL(ret, 2);
    for(int i = 0; i < ret; i++) {
        Space_Type1 *sample = (Space_Type1*)g_samples[i];
        if (sample->long_1 == 0) {
            /* Check data. */
            CU_ASSERT_EQUAL_FATAL(sample->long_2, 0);
            CU_ASSERT_EQUAL_FATAL(sample->long_3, 0);

            /* Check states. */
            CU_ASSERT_EQUAL_FATAL(g_info[i].valid_data,     true);
            CU_ASSERT_EQUAL_FATAL(g_info[i].sample_state,   DDS_SST_NOT_READ);
            CU_ASSERT_EQUAL_FATAL(g_info[i].view_state,     DDS_VST_NEW);
            CU_ASSERT_EQUAL_FATAL(g_info[i].instance_state, DDS_NOT_ALIVE_NO_WRITERS_INSTANCE_STATE);
        } else if (sample->long_1 == 1) {
            /* Check data. */
            CU_ASSERT_EQUAL_FATAL(sample->long_2, sample->long_1 * 2);
            CU_ASSERT_EQUAL_FATAL(sample->long_3, sample->long_1 * 3);

            /* Check states. */
            CU_ASSERT_EQUAL_FATAL(g_info[i].valid_data,     true);
            CU_ASSERT_EQUAL_FATAL(g_info[i].sample_state,   DDS_SST_NOT_READ);
            CU_ASSERT_EQUAL_FATAL(g_info[i].view_state,     DDS_VST_NEW);
            CU_ASSERT_EQUAL_FATAL(g_info[i].instance_state, DDS_ALIVE_INSTANCE_STATE);
        } else {
            CU_FAIL_FATAL("Unknown sample read");
        }
    }
}
/*************************************************************************************************/




/**************************************************************************************************
 *
 * These will check the dds_unregister_instance_ih_ts() in various ways.
 *
 *************************************************************************************************/
/*************************************************************************************************/
CU_Test(ddsc_unregister_instance_ih_ts, deleted, .init=unregistering_init, .fini=unregistering_fini)
{
    dds_return_t ret;
    dds_delete(g_writer);
    ret = dds_unregister_instance_ih_ts(g_writer, DDS_HANDLE_NIL, g_present);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_unregister_instance_ih_ts, invalid_handles) = {
        CU_DataPoints(dds_instance_handle_t, DDS_HANDLE_NIL, 0, 1, 100, UINT64_MAX),
};
CU_Theory((dds_instance_handle_t handle), ddsc_unregister_instance_ih_ts, invalid_handles, .init=unregistering_init, .fini=unregistering_fini)
{
    dds_return_t ret;
    ret = dds_unregister_instance_ih_ts(g_writer, handle, g_present);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_PRECONDITION_NOT_MET);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_unregister_instance_ih_ts, invalid_writers) = {
        CU_DataPoints(dds_entity_t, -2, -1, 0, INT_MAX, INT_MIN),
};
CU_Theory((dds_entity_t writer), ddsc_unregister_instance_ih_ts, invalid_writers, .init=unregistering_init, .fini=unregistering_fini)
{
    dds_return_t ret;

    ret = dds_unregister_instance_ih_ts(writer, DDS_HANDLE_NIL, g_present);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_unregister_instance_ih_ts, non_writers) = {
        CU_DataPoints(dds_entity_t*, &g_waitset, &g_reader, &g_topic, &g_participant),
};
CU_Theory((dds_entity_t *writer), ddsc_unregister_instance_ih_ts, non_writers, .init=unregistering_init, .fini=unregistering_fini)
{
    dds_return_t ret;
    ret = dds_unregister_instance_ih_ts(*writer, DDS_HANDLE_NIL, g_present);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_ILLEGAL_OPERATION);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_unregister_instance_ih_ts, unregistering_old_instance, .init=unregistering_init, .fini=unregistering_fini)
{
    Space_Type1 oldInstance = { 0, 22, 22 };
    dds_instance_handle_t hdl = dds_lookup_instance(g_writer, &oldInstance);
    dds_return_t ret;

    ret = dds_unregister_instance_ih_ts(g_writer, hdl, g_present);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    /* Read all available samples. */
    ret = dds_read(g_reader, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
    CU_ASSERT_EQUAL_FATAL(ret, 2);
    for(int i = 0; i < ret; i++) {
        Space_Type1 *sample = (Space_Type1*)g_samples[i];
        if (sample->long_1 == 0) {
            /* Check data (data part of unregister is not used, only the key part). */
            CU_ASSERT_EQUAL_FATAL(sample->long_2, sample->long_1 * 2);
            CU_ASSERT_EQUAL_FATAL(sample->long_3, sample->long_1 * 3);

            /* Check states. */
            CU_ASSERT_EQUAL_FATAL(g_info[i].valid_data,     true);
            CU_ASSERT_EQUAL_FATAL(g_info[i].sample_state,   DDS_SST_NOT_READ);
            CU_ASSERT_EQUAL_FATAL(g_info[i].view_state,     DDS_VST_NEW);
            CU_ASSERT_EQUAL_FATAL(g_info[i].instance_state, DDS_NOT_ALIVE_NO_WRITERS_INSTANCE_STATE);
        } else if (sample->long_1 == 1) {
            /* Check data. */
            CU_ASSERT_EQUAL_FATAL(sample->long_2, sample->long_1 * 2);
            CU_ASSERT_EQUAL_FATAL(sample->long_3, sample->long_1 * 3);

            /* Check states. */
            CU_ASSERT_EQUAL_FATAL(g_info[i].valid_data,     true);
            CU_ASSERT_EQUAL_FATAL(g_info[i].sample_state,   DDS_SST_NOT_READ);
            CU_ASSERT_EQUAL_FATAL(g_info[i].view_state,     DDS_VST_NEW);
            CU_ASSERT_EQUAL_FATAL(g_info[i].instance_state, DDS_ALIVE_INSTANCE_STATE);
        } else {
            CU_FAIL_FATAL("Unknown sample read");
        }
    }
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_unregister_instance_ih_ts, unregistering_past_sample, .init=unregistering_init, .fini=unregistering_fini)
{
    Space_Type1 oldInstance = { 0, 0, 0 };
    dds_instance_handle_t hdl = dds_lookup_instance(g_writer, &oldInstance);
    dds_attach_t triggered;
    dds_return_t ret;

    /* Unregistering a sample in the past should trigger a lost sample. */
    ret = dds_set_status_mask(g_reader, DDS_SAMPLE_LOST_STATUS);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    ret = dds_waitset_attach(g_waitset, g_reader, g_reader);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    /* Now, unregister a sample in the past. */
    ret = dds_unregister_instance_ih_ts(g_writer, hdl, g_past);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    /* Wait for 'sample lost'. */
    ret = dds_waitset_wait(g_waitset, &triggered, 1, DDS_SECS(1));
    CU_ASSERT_EQUAL_FATAL(ret, 1);

    /* Read all available samples. */
    ret = dds_read(g_reader, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
    CU_ASSERT_EQUAL_FATAL(ret, 2);
    for(int i = 0; i < ret; i++) {
        Space_Type1 *sample = (Space_Type1*)g_samples[i];
        if ((sample->long_1 == 0) || (sample->long_1 == 1)) {
            /* Check data. */
            CU_ASSERT_EQUAL_FATAL(sample->long_2, sample->long_1 * 2);
            CU_ASSERT_EQUAL_FATAL(sample->long_3, sample->long_1 * 3);

            /* Check states. */
            CU_ASSERT_EQUAL_FATAL(g_info[i].valid_data,     true);
            CU_ASSERT_EQUAL_FATAL(g_info[i].sample_state,   DDS_SST_NOT_READ);
            CU_ASSERT_EQUAL_FATAL(g_info[i].view_state,     DDS_VST_NEW);
        } else {
            CU_FAIL_FATAL("Unknown sample read");
        }
    }

}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_unregister_instance_ih_ts, unregistering_instance)
{
    Space_Type1 testData = { 0, 22, 22 };
    dds_instance_handle_t ih = 0;
    dds_return_t ret;
    char name[100];

    /* Create a writer that WILL automatically dispose unregistered samples. */
    g_participant = dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);
    CU_ASSERT_FATAL(g_participant > 0);
    g_topic = dds_create_topic(g_participant, &Space_Type1_desc, create_unique_topic_name("ddsc_unregistering_instance_test", name, 100), NULL, NULL);
    CU_ASSERT_FATAL(g_topic > 0);
    g_writer = dds_create_writer(g_participant, g_topic, NULL, NULL);
    CU_ASSERT_FATAL(g_writer > 0);

    /* Register the instance. */
    ret = dds_register_instance(g_writer, &ih, &testData);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    CU_ASSERT_NOT_EQUAL_FATAL(ih, DDS_HANDLE_NIL);

    /* Unregister the instance. */
    ret = dds_unregister_instance_ih_ts(g_writer, ih, dds_time());
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    dds_delete(g_writer);
    dds_delete(g_topic);
    dds_delete(g_participant);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_unregister_instance, dispose_unregistered_sample, .init=unregistering_init, .fini=unregistering_fini)
{
    dds_entity_t writer;
    writer = dds_create_writer(g_participant, g_topic, NULL, NULL);
    CU_ASSERT_FATAL(g_writer > 0);

    Space_Type1 newInstance = { INITIAL_SAMPLES, 0, 0 };
    dds_return_t ret;

    ret = dds_write(writer, &newInstance);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    ret = dds_unregister_instance(writer, &newInstance);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    /* Read all available samples. */
    ret = dds_read(g_reader, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
    CU_ASSERT_EQUAL_FATAL(ret, 3);
    for(int i = 0; i < ret; i++) {
        Space_Type1 *sample = (Space_Type1*)g_samples[i];
        if (sample->long_1 <= 1) {
            /* Check data. */
            CU_ASSERT_EQUAL_FATAL(sample->long_2, sample->long_1 * 2);
            CU_ASSERT_EQUAL_FATAL(sample->long_3, sample->long_1 * 3);

            /* Check states. */
            CU_ASSERT_EQUAL_FATAL(g_info[i].valid_data,     true);
            CU_ASSERT_EQUAL_FATAL(g_info[i].sample_state,   DDS_SST_NOT_READ);
            CU_ASSERT_EQUAL_FATAL(g_info[i].view_state,     DDS_VST_NEW);
            CU_ASSERT_EQUAL_FATAL(g_info[i].instance_state, DDS_ALIVE_INSTANCE_STATE);
        } else if (sample->long_1 == 2) {
            /* Check data. */
            CU_ASSERT_EQUAL_FATAL(sample->long_2, 0);
            CU_ASSERT_EQUAL_FATAL(sample->long_3, 0);

            /* Check states. */
            CU_ASSERT_EQUAL_FATAL(g_info[i].valid_data,     true);
            CU_ASSERT_EQUAL_FATAL(g_info[i].sample_state,   DDS_SST_NOT_READ);
            CU_ASSERT_EQUAL_FATAL(g_info[i].view_state,     DDS_VST_NEW);
            CU_ASSERT_EQUAL_FATAL(g_info[i].instance_state, DDS_NOT_ALIVE_DISPOSED_INSTANCE_STATE);
        } else {
            CU_FAIL_FATAL("Unknown sample read");
        }
    }
    dds_delete(writer);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_unregister_instance_ts, dispose_unregistered_sample, .init=unregistering_init, .fini=unregistering_fini)
{
    dds_entity_t writer;
    writer = dds_create_writer(g_participant, g_topic, NULL, NULL);
    CU_ASSERT_FATAL(g_writer > 0);

    Space_Type1 newInstance = { INITIAL_SAMPLES, 0, 0 };
    dds_return_t ret;

    ret = dds_write(writer, &newInstance);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    ret = dds_unregister_instance(writer, &newInstance);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    /* Read all available samples. */
    ret = dds_read(g_reader, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
    CU_ASSERT_EQUAL_FATAL(ret, 3);
    for(int i = 0; i < ret; i++) {
        Space_Type1 *sample = (Space_Type1*)g_samples[i];
        if (sample->long_1 <= 1) {
            /* Check data. */
            CU_ASSERT_EQUAL_FATAL(sample->long_2, sample->long_1 * 2);
            CU_ASSERT_EQUAL_FATAL(sample->long_3, sample->long_1 * 3);

            /* Check states. */
            CU_ASSERT_EQUAL_FATAL(g_info[i].valid_data,     true);
            CU_ASSERT_EQUAL_FATAL(g_info[i].sample_state,   DDS_SST_NOT_READ);
            CU_ASSERT_EQUAL_FATAL(g_info[i].view_state,     DDS_VST_NEW);
            CU_ASSERT_EQUAL_FATAL(g_info[i].instance_state, DDS_ALIVE_INSTANCE_STATE);
        } else if (sample->long_1 == 2) {
            /* Check data. */
            CU_ASSERT_EQUAL_FATAL(sample->long_2, 0);
            CU_ASSERT_EQUAL_FATAL(sample->long_3, 0);

            /* Check states. */
            CU_ASSERT_EQUAL_FATAL(g_info[i].valid_data,     true);
            CU_ASSERT_EQUAL_FATAL(g_info[i].sample_state,   DDS_SST_NOT_READ);
            CU_ASSERT_EQUAL_FATAL(g_info[i].view_state,     DDS_VST_NEW);
            CU_ASSERT_EQUAL_FATAL(g_info[i].instance_state, DDS_NOT_ALIVE_DISPOSED_INSTANCE_STATE);
        } else {
            CU_FAIL_FATAL("Unknown sample read");
        }
    }
    dds_delete(writer);
}
/*************************************************************************************************/

#endif
