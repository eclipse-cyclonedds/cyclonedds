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

#include "ddsc/dds.h"
#include "os/os.h"
#include <criterion/criterion.h>
#include <criterion/logging.h>
#include <criterion/theories.h>
#include "Space.h"


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

static char*
create_topic_name(const char *prefix, char *name, size_t size)
{
    /* Get semi random g_topic name. */
    os_procId pid = os_procIdSelf();
    uintmax_t tid = os_threadIdToInteger(os_threadIdSelf());
    (void) snprintf(name, size, "%s_pid%"PRIprocId"_tid%"PRIuMAX"", prefix, pid, tid);
    return name;
}

static void
unregistering_init(void)
{
    Space_Type1 sample = { 0 };
    dds_qos_t *qos = dds_qos_create ();
    dds_attach_t triggered;
    dds_return_t ret;
    char name[100];

    /* Use by source timestamp to be able to check the time related funtions. */
    dds_qset_destination_order(qos, DDS_DESTINATIONORDER_BY_SOURCE_TIMESTAMP);

    g_participant = dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);
    cr_assert_gt(g_participant, 0, "Failed to create prerequisite g_participant");

    g_waitset = dds_create_waitset(g_participant);
    cr_assert_gt(g_waitset, 0, "Failed to create g_waitset");

    g_topic = dds_create_topic(g_participant, &Space_Type1_desc, create_topic_name("ddsc_unregistering_test", name, 100), qos, NULL);
    cr_assert_gt(g_topic, 0, "Failed to create prerequisite g_topic");

    /* Create a reader that keeps one sample on three instances. */
    dds_qset_reliability(qos, DDS_RELIABILITY_RELIABLE, DDS_MSECS(100));
    dds_qset_resource_limits(qos, DDS_LENGTH_UNLIMITED, 3, 1);
    g_reader = dds_create_reader(g_participant, g_topic, qos, NULL);
    cr_assert_gt(g_reader, 0, "Failed to create prerequisite g_reader");

    /* Create a writer that will not automatically dispose unregistered samples. */
    dds_qset_writer_data_lifecycle(qos, false);
    g_writer = dds_create_writer(g_participant, g_topic, qos, NULL);
    cr_assert_gt(g_writer, 0, "Failed to create prerequisite g_writer");

    /* Sync g_writer to g_reader. */
    ret = dds_set_enabled_status(g_writer, DDS_PUBLICATION_MATCHED_STATUS);
    cr_assert_eq(ret, DDS_RETCODE_OK, "Failed to set prerequisite g_writer status");
    ret = dds_waitset_attach(g_waitset, g_writer, g_writer);
    cr_assert_eq(ret, DDS_RETCODE_OK, "Failed to attach prerequisite g_writer");
    ret = dds_waitset_wait(g_waitset, &triggered, 1, DDS_SECS(1));
    cr_assert_eq(ret, 1, "Failed prerequisite dds_waitset_wait g_writer r");
    cr_assert_eq(g_writer, (dds_entity_t)(intptr_t)triggered, "Failed prerequisite dds_waitset_wait g_writer a");
    ret = dds_waitset_detach(g_waitset, g_writer);
    cr_assert_eq(ret, DDS_RETCODE_OK, "Failed to detach prerequisite g_writer");

    /* Sync g_reader to g_writer. */
    ret = dds_set_enabled_status(g_reader, DDS_SUBSCRIPTION_MATCHED_STATUS);
    cr_assert_eq(ret, DDS_RETCODE_OK, "Failed to set prerequisite g_reader status");
    ret = dds_waitset_attach(g_waitset, g_reader, g_reader);
    cr_assert_eq(ret, DDS_RETCODE_OK, "Failed to attach prerequisite g_reader");
    ret = dds_waitset_wait(g_waitset, &triggered, 1, DDS_SECS(1));
    cr_assert_eq(ret, 1, "Failed prerequisite dds_waitset_wait g_reader r");
    cr_assert_eq(g_reader, (dds_entity_t)(intptr_t)triggered, "Failed prerequisite dds_waitset_wait g_reader a");
    ret = dds_waitset_detach(g_waitset, g_reader);
    cr_assert_eq(ret, DDS_RETCODE_OK, "Failed to detach prerequisite g_reader");

    /* Write initial samples. */
    for (int i = 0; i < INITIAL_SAMPLES; i++) {
        sample.long_1 = i;
        sample.long_2 = i*2;
        sample.long_3 = i*3;
        ret = dds_write(g_writer, &sample);
        cr_assert_eq(ret, DDS_RETCODE_OK, "Failed prerequisite write");
    }

    /* Initialize reading buffers. */
    memset (g_data, 0, sizeof (g_data));
    for (int i = 0; i < MAX_SAMPLES; i++) {
        g_samples[i] = &g_data[i];
    }

    /* Initialize times. */
    g_present = dds_time();
    g_past    = g_present - DDS_SECS(1);

    dds_qos_delete(qos);
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
Test(ddsc_unregister_instance, deleted, .init=unregistering_init, .fini=unregistering_fini)
{
    dds_return_t ret;
    dds_delete(g_writer);

    ret = dds_unregister_instance(g_writer, g_data);
    cr_assert_eq(dds_err_nr(ret), DDS_RETCODE_ALREADY_DELETED, "returned %d", dds_err_nr(ret));
}
/*************************************************************************************************/

/*************************************************************************************************/
Test(ddsc_unregister_instance, null, .init=unregistering_init, .fini=unregistering_fini)
{
    dds_return_t ret;
    ret = dds_unregister_instance(g_writer, NULL);
    cr_assert_eq(dds_err_nr(ret), DDS_RETCODE_BAD_PARAMETER, "returned %d", dds_err_nr(ret));
}
/*************************************************************************************************/

/*************************************************************************************************/
TheoryDataPoints(ddsc_unregister_instance, invalid_writers) = {
        DataPoints(dds_entity_t, -2, -1, 0, 1, 100, INT_MAX, INT_MIN),
};
Theory((dds_entity_t writer), ddsc_unregister_instance, invalid_writers, .init=unregistering_init, .fini=unregistering_fini)
{
    dds_entity_t exp = DDS_RETCODE_BAD_PARAMETER * -1;
    dds_return_t ret;

    ret = dds_unregister_instance(writer, g_data);
    cr_assert_eq(dds_err_nr(ret), dds_err_nr(exp), "returned %d != expected %d", dds_err_nr(ret), dds_err_nr(exp));
}
/*************************************************************************************************/

/*************************************************************************************************/
TheoryDataPoints(ddsc_unregister_instance, non_writers) = {
        DataPoints(dds_entity_t*, &g_waitset, &g_reader, &g_topic, &g_participant),
};
Theory((dds_entity_t *writer), ddsc_unregister_instance, non_writers, .init=unregistering_init, .fini=unregistering_fini)
{
    dds_return_t ret;
    ret = dds_unregister_instance(*writer, g_data);
    cr_assert_eq(dds_err_nr(ret), DDS_RETCODE_ILLEGAL_OPERATION, "returned %d", dds_err_nr(ret));
}
/*************************************************************************************************/

/*************************************************************************************************/
Test(ddsc_unregister_instance, unregistering_old_instance, .init=unregistering_init, .fini=unregistering_fini)
{
    Space_Type1 oldInstance = { 0, 22, 22 };
    dds_return_t ret;

    ret = dds_unregister_instance(g_writer, &oldInstance);
    cr_assert_eq(ret, DDS_RETCODE_OK, "Unregistering old instance returned %d", dds_err_nr(ret));

    /* Read all available samples. */
    ret = dds_read(g_reader, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
    cr_assert_eq(ret, 2, "# read %d, expected %d", ret, 2);
    for(int i = 0; i < ret; i++) {
        Space_Type1 *sample = (Space_Type1*)g_samples[i];
        if (sample->long_1 == 0) {
            /* Check data. */
            cr_assert_eq(sample->long_2, 0);
            cr_assert_eq(sample->long_3, 0);

            /* Check states. */
            cr_assert_eq(g_info[i].valid_data,     true);
            cr_assert_eq(g_info[i].sample_state,   DDS_SST_NOT_READ);
            cr_assert_eq(g_info[i].view_state,     DDS_VST_NEW);
            cr_assert_eq(g_info[i].instance_state, DDS_NOT_ALIVE_NO_WRITERS_INSTANCE_STATE);
        } else if (sample->long_1 == 1) {
            /* Check data. */
            cr_assert_eq(sample->long_2, sample->long_1 * 2);
            cr_assert_eq(sample->long_3, sample->long_1 * 3);

            /* Check states. */
            cr_assert_eq(g_info[i].valid_data,     true);
            cr_assert_eq(g_info[i].sample_state,   DDS_SST_NOT_READ);
            cr_assert_eq(g_info[i].view_state,     DDS_VST_NEW);
            cr_assert_eq(g_info[i].instance_state, DDS_ALIVE_INSTANCE_STATE);
        } else {
            cr_assert(false, "Unknown sample read");
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
Test(ddsc_unregister_instance_ts, deleted, .init=unregistering_init, .fini=unregistering_fini)
{
    dds_return_t ret;
    dds_delete(g_writer);
    ret = dds_unregister_instance_ts(g_writer, g_data, g_present);
    cr_assert_eq(dds_err_nr(ret), DDS_RETCODE_ALREADY_DELETED, "returned %d", dds_err_nr(ret));
}
/*************************************************************************************************/

/*************************************************************************************************/
Test(ddsc_unregister_instance_ts, null, .init=unregistering_init, .fini=unregistering_fini)
{
    dds_return_t ret;
    ret = dds_unregister_instance_ts(g_writer, NULL, g_present);
    cr_assert_eq(dds_err_nr(ret), DDS_RETCODE_BAD_PARAMETER, "returned %d", dds_err_nr(ret));
}
/*************************************************************************************************/

/*************************************************************************************************/
TheoryDataPoints(ddsc_unregister_instance_ts, invalid_writers) = {
        DataPoints(dds_entity_t, -2, -1, 0, 1, 100, INT_MAX, INT_MIN),
};
Theory((dds_entity_t writer), ddsc_unregister_instance_ts, invalid_writers, .init=unregistering_init, .fini=unregistering_fini)
{
    dds_entity_t exp = DDS_RETCODE_BAD_PARAMETER * -1;
    dds_return_t ret;

    ret = dds_unregister_instance_ts(writer, g_data, g_present);
    cr_assert_eq(dds_err_nr(ret), dds_err_nr(exp), "returned %d != expected %d", dds_err_nr(ret), dds_err_nr(exp));
}
/*************************************************************************************************/

/*************************************************************************************************/
TheoryDataPoints(ddsc_unregister_instance_ts, non_writers) = {
        DataPoints(dds_entity_t*, &g_waitset, &g_reader, &g_topic, &g_participant),
};
Theory((dds_entity_t *writer), ddsc_unregister_instance_ts, non_writers, .init=unregistering_init, .fini=unregistering_fini)
{
    dds_return_t ret;
    ret = dds_unregister_instance_ts(*writer, g_data, g_present);
    cr_assert_eq(dds_err_nr(ret), DDS_RETCODE_ILLEGAL_OPERATION, "returned %d", dds_err_nr(ret));
}
/*************************************************************************************************/

/*************************************************************************************************/
Test(ddsc_unregister_instance_ts, unregistering_old_instance, .init=unregistering_init, .fini=unregistering_fini)
{
    Space_Type1 oldInstance = { 0, 22, 22 };
    dds_return_t ret;

    ret = dds_unregister_instance_ts(g_writer, &oldInstance, g_present);
    cr_assert_eq(ret, DDS_RETCODE_OK, "Unregistering old instance returned %d", dds_err_nr(ret));

    /* Read all available samples. */
    ret = dds_read(g_reader, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
    cr_assert_eq(ret, 2, "# read %d, expected %d", ret, 2);
    for(int i = 0; i < ret; i++) {
        Space_Type1 *sample = (Space_Type1*)g_samples[i];
        if (sample->long_1 == 0) {
            /* Check data (data part of unregister is not used, only the key part). */
            cr_assert_eq(sample->long_2, sample->long_1 * 2);
            cr_assert_eq(sample->long_3, sample->long_1 * 3);

            /* Check states. */
            cr_assert_eq(g_info[i].valid_data,     true);
            cr_assert_eq(g_info[i].sample_state,   DDS_SST_NOT_READ);
            cr_assert_eq(g_info[i].view_state,     DDS_VST_NEW);
            cr_assert_eq(g_info[i].instance_state, DDS_NOT_ALIVE_NO_WRITERS_INSTANCE_STATE);
        } else if (sample->long_1 == 1) {
            /* Check data. */
            cr_assert_eq(sample->long_2, sample->long_1 * 2);
            cr_assert_eq(sample->long_3, sample->long_1 * 3);

            /* Check states. */
            cr_assert_eq(g_info[i].valid_data,     true);
            cr_assert_eq(g_info[i].sample_state,   DDS_SST_NOT_READ);
            cr_assert_eq(g_info[i].view_state,     DDS_VST_NEW);
            cr_assert_eq(g_info[i].instance_state, DDS_ALIVE_INSTANCE_STATE);
        } else {
            cr_assert(false, "Unknown sample read");
        }
    }
}
/*************************************************************************************************/

/*************************************************************************************************/
Test(ddsc_unregister_instance_ts, unregistering_past_sample, .init=unregistering_init, .fini=unregistering_fini)
{
    Space_Type1 oldInstance = { 0, 0, 0 };
    dds_attach_t triggered;
    dds_return_t ret;

    /* Unregistering a sample in the past should trigger a lost sample. */
    ret = dds_set_enabled_status(g_reader, DDS_SAMPLE_LOST_STATUS);
    cr_assert_eq(ret, DDS_RETCODE_OK, "Failed to set prerequisite g_reader status");
    ret = dds_waitset_attach(g_waitset, g_reader, g_reader);
    cr_assert_eq(ret, DDS_RETCODE_OK, "Failed to attach prerequisite g_reader");

    /* Now, unregister a sample in the past. */
    ret = dds_unregister_instance_ts(g_writer, &oldInstance, g_past);
    cr_assert_eq(ret, DDS_RETCODE_OK, "Unregistering old instance returned %d", dds_err_nr(ret));

    /* Wait for 'sample lost'. */
    ret = dds_waitset_wait(g_waitset, &triggered, 1, DDS_SECS(1));
    cr_assert_eq(ret, 1, "Unregistering past sample did not trigger 'sample lost'");

    /* Read all available samples. */
    ret = dds_read(g_reader, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
    cr_assert_eq(ret, 2, "# read %d, expected %d", ret, 2);
    for(int i = 0; i < ret; i++) {
        Space_Type1 *sample = (Space_Type1*)g_samples[i];
        if ((sample->long_1 == 0) || (sample->long_1 == 1)) {
            /* Check data. */
            cr_assert_eq(sample->long_2, sample->long_1 * 2);
            cr_assert_eq(sample->long_3, sample->long_1 * 3);

            /* Check states. */
            cr_assert_eq(g_info[i].valid_data,     true);
            cr_assert_eq(g_info[i].sample_state,   DDS_SST_NOT_READ);
            cr_assert_eq(g_info[i].view_state,     DDS_VST_NEW);
        } else {
            cr_assert(false, "Unknown sample read");
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
Test(ddsc_unregister_instance_ih, deleted, .init=unregistering_init, .fini=unregistering_fini)
{
    dds_return_t ret;
    dds_delete(g_writer);
    ret = dds_unregister_instance_ih(g_writer, DDS_HANDLE_NIL);
    cr_assert_eq(dds_err_nr(ret), DDS_RETCODE_ALREADY_DELETED, "returned %d", dds_err_nr(ret));
}
/*************************************************************************************************/

/*************************************************************************************************/
TheoryDataPoints(ddsc_unregister_instance_ih, invalid_handles) = {
        DataPoints(dds_instance_handle_t, DDS_HANDLE_NIL, 0, 1, 100, UINT64_MAX),
};
Theory((dds_instance_handle_t handle), ddsc_unregister_instance_ih, invalid_handles, .init=unregistering_init, .fini=unregistering_fini)
{
    dds_return_t ret;
    ret = dds_unregister_instance_ih(g_writer, handle);
    cr_assert_eq(dds_err_nr(ret), DDS_RETCODE_PRECONDITION_NOT_MET, "returned %d", dds_err_nr(ret));
}
/*************************************************************************************************/

/*************************************************************************************************/
TheoryDataPoints(ddsc_unregister_instance_ih, invalid_writers) = {
        DataPoints(dds_entity_t, -2, -1, 0, 1, 100, INT_MAX, INT_MIN),
};
Theory((dds_entity_t writer), ddsc_unregister_instance_ih, invalid_writers, .init=unregistering_init, .fini=unregistering_fini)
{
    dds_entity_t exp = DDS_RETCODE_BAD_PARAMETER * -1;
    dds_return_t ret;

    ret = dds_unregister_instance_ih(writer, DDS_HANDLE_NIL);
    cr_assert_eq(dds_err_nr(ret), dds_err_nr(exp), "returned %d != expected %d", dds_err_nr(ret), dds_err_nr(exp));
}
/*************************************************************************************************/

/*************************************************************************************************/
TheoryDataPoints(ddsc_unregister_instance_ih, non_writers) = {
        DataPoints(dds_entity_t*, &g_waitset, &g_reader, &g_topic, &g_participant),
};
Theory((dds_entity_t *writer), ddsc_unregister_instance_ih, non_writers, .init=unregistering_init, .fini=unregistering_fini)
{
    dds_return_t ret;
    ret = dds_unregister_instance_ih(*writer, DDS_HANDLE_NIL);
    cr_assert_eq(dds_err_nr(ret), DDS_RETCODE_ILLEGAL_OPERATION, "returned %d", dds_err_nr(ret));
}
/*************************************************************************************************/

/*************************************************************************************************/
Test(ddsc_unregister_instance_ih, unregistering_old_instance, .init=unregistering_init, .fini=unregistering_fini)
{
    Space_Type1 oldInstance = { 0, 22, 22 };
    dds_instance_handle_t hdl = dds_instance_lookup(g_writer, &oldInstance);
    dds_return_t ret;

    ret = dds_unregister_instance_ih(g_writer, hdl);
    cr_assert_eq(ret, DDS_RETCODE_OK, "Unregistering old instance returned %d", dds_err_nr(ret));

    /* Read all available samples. */
    ret = dds_read(g_reader, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
    cr_assert_eq(ret, 2, "# read %d, expected %d", ret, 2);
    for(int i = 0; i < ret; i++) {
        Space_Type1 *sample = (Space_Type1*)g_samples[i];
        if (sample->long_1 == 0) {
            /* Check data. */
            cr_assert_eq(sample->long_2, 0);
            cr_assert_eq(sample->long_3, 0);

            /* Check states. */
            cr_assert_eq(g_info[i].valid_data,     true);
            cr_assert_eq(g_info[i].sample_state,   DDS_SST_NOT_READ);
            cr_assert_eq(g_info[i].view_state,     DDS_VST_NEW);
            cr_assert_eq(g_info[i].instance_state, DDS_NOT_ALIVE_NO_WRITERS_INSTANCE_STATE);
        } else if (sample->long_1 == 1) {
            /* Check data. */
            cr_assert_eq(sample->long_2, sample->long_1 * 2);
            cr_assert_eq(sample->long_3, sample->long_1 * 3);

            /* Check states. */
            cr_assert_eq(g_info[i].valid_data,     true);
            cr_assert_eq(g_info[i].sample_state,   DDS_SST_NOT_READ);
            cr_assert_eq(g_info[i].view_state,     DDS_VST_NEW);
            cr_assert_eq(g_info[i].instance_state, DDS_ALIVE_INSTANCE_STATE);
        } else {
            cr_assert(false, "Unknown sample read");
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
Test(ddsc_unregister_instance_ih_ts, deleted, .init=unregistering_init, .fini=unregistering_fini)
{
    dds_return_t ret;
    dds_delete(g_writer);
    ret = dds_unregister_instance_ih_ts(g_writer, DDS_HANDLE_NIL, g_present);
    cr_assert_eq(dds_err_nr(ret), DDS_RETCODE_ALREADY_DELETED, "returned %d", dds_err_nr(ret));
}
/*************************************************************************************************/

/*************************************************************************************************/
TheoryDataPoints(ddsc_unregister_instance_ih_ts, invalid_handles) = {
        DataPoints(dds_instance_handle_t, DDS_HANDLE_NIL, 0, 1, 100, UINT64_MAX),
};
Theory((dds_instance_handle_t handle), ddsc_unregister_instance_ih_ts, invalid_handles, .init=unregistering_init, .fini=unregistering_fini)
{
    dds_return_t ret;
    ret = dds_unregister_instance_ih_ts(g_writer, handle, g_present);
    cr_assert_eq(dds_err_nr(ret), DDS_RETCODE_PRECONDITION_NOT_MET, "returned %d", dds_err_nr(ret));
}
/*************************************************************************************************/

/*************************************************************************************************/
TheoryDataPoints(ddsc_unregister_instance_ih_ts, invalid_writers) = {
        DataPoints(dds_entity_t, -2, -1, 0, 1, 100, INT_MAX, INT_MIN),
};
Theory((dds_entity_t writer), ddsc_unregister_instance_ih_ts, invalid_writers, .init=unregistering_init, .fini=unregistering_fini)
{
    dds_entity_t exp = DDS_RETCODE_BAD_PARAMETER * -1;
    dds_return_t ret;

    ret = dds_unregister_instance_ih_ts(writer, DDS_HANDLE_NIL, g_present);
    cr_assert_eq(dds_err_nr(ret), dds_err_nr(exp), "returned %d != expected %d", dds_err_nr(ret), dds_err_nr(exp));
}
/*************************************************************************************************/

/*************************************************************************************************/
TheoryDataPoints(ddsc_unregister_instance_ih_ts, non_writers) = {
        DataPoints(dds_entity_t*, &g_waitset, &g_reader, &g_topic, &g_participant),
};
Theory((dds_entity_t *writer), ddsc_unregister_instance_ih_ts, non_writers, .init=unregistering_init, .fini=unregistering_fini)
{
    dds_return_t ret;
    ret = dds_unregister_instance_ih_ts(*writer, DDS_HANDLE_NIL, g_present);
    cr_assert_eq(dds_err_nr(ret), DDS_RETCODE_ILLEGAL_OPERATION, "returned %d", dds_err_nr(ret));
}
/*************************************************************************************************/

/*************************************************************************************************/
Test(ddsc_unregister_instance_ih_ts, unregistering_old_instance, .init=unregistering_init, .fini=unregistering_fini)
{
    Space_Type1 oldInstance = { 0, 22, 22 };
    dds_instance_handle_t hdl = dds_instance_lookup(g_writer, &oldInstance);
    dds_return_t ret;

    ret = dds_unregister_instance_ih_ts(g_writer, hdl, g_present);
    cr_assert_eq(ret, DDS_RETCODE_OK, "Unregistering old instance returned %d", dds_err_nr(ret));

    /* Read all available samples. */
    ret = dds_read(g_reader, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
    cr_assert_eq(ret, 2, "# read %d, expected %d", ret, 2);
    for(int i = 0; i < ret; i++) {
        Space_Type1 *sample = (Space_Type1*)g_samples[i];
        if (sample->long_1 == 0) {
            /* Check data (data part of unregister is not used, only the key part). */
            cr_assert_eq(sample->long_2, sample->long_1 * 2);
            cr_assert_eq(sample->long_3, sample->long_1 * 3);

            /* Check states. */
            cr_assert_eq(g_info[i].valid_data,     true);
            cr_assert_eq(g_info[i].sample_state,   DDS_SST_NOT_READ);
            cr_assert_eq(g_info[i].view_state,     DDS_VST_NEW);
            cr_assert_eq(g_info[i].instance_state, DDS_NOT_ALIVE_NO_WRITERS_INSTANCE_STATE);
        } else if (sample->long_1 == 1) {
            /* Check data. */
            cr_assert_eq(sample->long_2, sample->long_1 * 2);
            cr_assert_eq(sample->long_3, sample->long_1 * 3);

            /* Check states. */
            cr_assert_eq(g_info[i].valid_data,     true);
            cr_assert_eq(g_info[i].sample_state,   DDS_SST_NOT_READ);
            cr_assert_eq(g_info[i].view_state,     DDS_VST_NEW);
            cr_assert_eq(g_info[i].instance_state, DDS_ALIVE_INSTANCE_STATE);
        } else {
            cr_assert(false, "Unknown sample read");
        }
    }
}
/*************************************************************************************************/

/*************************************************************************************************/
Test(ddsc_unregister_instance_ih_ts, unregistering_past_sample, .init=unregistering_init, .fini=unregistering_fini)
{
    Space_Type1 oldInstance = { 0, 0, 0 };
    dds_instance_handle_t hdl = dds_instance_lookup(g_writer, &oldInstance);
    dds_attach_t triggered;
    dds_return_t ret;

    /* Unregistering a sample in the past should trigger a lost sample. */
    ret = dds_set_enabled_status(g_reader, DDS_SAMPLE_LOST_STATUS);
    cr_assert_eq(ret, DDS_RETCODE_OK, "Failed to set prerequisite g_reader status");
    ret = dds_waitset_attach(g_waitset, g_reader, g_reader);
    cr_assert_eq(ret, DDS_RETCODE_OK, "Failed to attach prerequisite g_reader");

    /* Now, unregister a sample in the past. */
    ret = dds_unregister_instance_ih_ts(g_writer, hdl, g_past);
    cr_assert_eq(ret, DDS_RETCODE_OK, "Unregistering old instance returned %d", dds_err_nr(ret));

    /* Wait for 'sample lost'. */
    ret = dds_waitset_wait(g_waitset, &triggered, 1, DDS_SECS(1));
    cr_assert_eq(ret, 1, "Unregistering past sample did not trigger 'sample lost'");

    /* Read all available samples. */
    ret = dds_read(g_reader, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
    cr_assert_eq(ret, 2, "# read %d, expected %d", ret, 2);
    for(int i = 0; i < ret; i++) {
        Space_Type1 *sample = (Space_Type1*)g_samples[i];
        if ((sample->long_1 == 0) || (sample->long_1 == 1)) {
            /* Check data. */
            cr_assert_eq(sample->long_2, sample->long_1 * 2);
            cr_assert_eq(sample->long_3, sample->long_1 * 3);

            /* Check states. */
            cr_assert_eq(g_info[i].valid_data,     true);
            cr_assert_eq(g_info[i].sample_state,   DDS_SST_NOT_READ);
            cr_assert_eq(g_info[i].view_state,     DDS_VST_NEW);
        } else {
            cr_assert(false, "Unknown sample read");
        }
    }

}
/*************************************************************************************************/

/*************************************************************************************************/
Test(ddsc_unregister_instance, dispose_unregistered_sample, .init=unregistering_init, .fini=unregistering_fini)
{
    dds_entity_t writer;
    writer = dds_create_writer(g_participant, g_topic, NULL, NULL);
    cr_assert_gt(g_writer, 0, "Failed to create writer");

    Space_Type1 newInstance = { INITIAL_SAMPLES, 0, 0 };
    dds_return_t ret;

    ret = dds_write(writer, &newInstance);
    cr_assert_eq(ret, DDS_RETCODE_OK, "Failed write");

    ret = dds_unregister_instance(writer, &newInstance);
    cr_assert_eq(ret, DDS_RETCODE_OK, "Disposing unregistered sample returned %d", dds_err_nr(ret));

    /* Read all available samples. */
    ret = dds_read(g_reader, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
    cr_assert_eq(ret, 3, "# read %d, expected %d", ret, 3);
    for(int i = 0; i < ret; i++) {
        Space_Type1 *sample = (Space_Type1*)g_samples[i];
        if (sample->long_1 <= 1) {
            /* Check data. */
            cr_assert_eq(sample->long_2, sample->long_1 * 2);
            cr_assert_eq(sample->long_3, sample->long_1 * 3);

            /* Check states. */
            cr_assert_eq(g_info[i].valid_data,     true);
            cr_assert_eq(g_info[i].sample_state,   DDS_SST_NOT_READ);
            cr_assert_eq(g_info[i].view_state,     DDS_VST_NEW);
            cr_assert_eq(g_info[i].instance_state, DDS_ALIVE_INSTANCE_STATE);
        } else if (sample->long_1 == 2) {
            /* Check data. */
            cr_assert_eq(sample->long_2, 0);
            cr_assert_eq(sample->long_3, 0);

            /* Check states. */
            cr_assert_eq(g_info[i].valid_data,     true);
            cr_assert_eq(g_info[i].sample_state,   DDS_SST_NOT_READ);
            cr_assert_eq(g_info[i].view_state,     DDS_VST_NEW);
            cr_assert_eq(g_info[i].instance_state, DDS_NOT_ALIVE_DISPOSED_INSTANCE_STATE);
        } else {
            cr_assert(false, "Unknown sample read");
        }
    }
    dds_delete(writer);
}
/*************************************************************************************************/

/*************************************************************************************************/
Test(ddsc_unregister_instance_ts, dispose_unregistered_sample, .init=unregistering_init, .fini=unregistering_fini)
{
    dds_entity_t writer;
    writer = dds_create_writer(g_participant, g_topic, NULL, NULL);
    cr_assert_gt(g_writer, 0, "Failed to create writer");

    Space_Type1 newInstance = { INITIAL_SAMPLES, 0, 0 };
    dds_return_t ret;

    ret = dds_write(writer, &newInstance);
    cr_assert_eq(ret, DDS_RETCODE_OK, "Failed write");

    ret = dds_unregister_instance(writer, &newInstance);
    cr_assert_eq(ret, DDS_RETCODE_OK, "Disposing unregistered sample returned %d", dds_err_nr(ret));

    /* Read all available samples. */
    ret = dds_read(g_reader, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
    cr_assert_eq(ret, 3, "# read %d, expected %d", ret, 3);
    for(int i = 0; i < ret; i++) {
        Space_Type1 *sample = (Space_Type1*)g_samples[i];
        if (sample->long_1 <= 1) {
            /* Check data. */
            cr_assert_eq(sample->long_2, sample->long_1 * 2);
            cr_assert_eq(sample->long_3, sample->long_1 * 3);

            /* Check states. */
            cr_assert_eq(g_info[i].valid_data,     true);
            cr_assert_eq(g_info[i].sample_state,   DDS_SST_NOT_READ);
            cr_assert_eq(g_info[i].view_state,     DDS_VST_NEW);
            cr_assert_eq(g_info[i].instance_state, DDS_ALIVE_INSTANCE_STATE);
        } else if (sample->long_1 == 2) {
            /* Check data. */
            cr_assert_eq(sample->long_2, 0);
            cr_assert_eq(sample->long_3, 0);

            /* Check states. */
            cr_assert_eq(g_info[i].valid_data,     true);
            cr_assert_eq(g_info[i].sample_state,   DDS_SST_NOT_READ);
            cr_assert_eq(g_info[i].view_state,     DDS_VST_NEW);
            cr_assert_eq(g_info[i].instance_state, DDS_NOT_ALIVE_DISPOSED_INSTANCE_STATE);
        } else {
            cr_assert(false, "Unknown sample read");
        }
    }
    dds_delete(writer);
}
/*************************************************************************************************/

#endif
