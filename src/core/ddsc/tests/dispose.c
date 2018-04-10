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
#include <criterion/criterion.h>
#include <criterion/logging.h>
#include <criterion/theories.h>
#include "Space.h"

/* Add --verbose command line argument to get the cr_log_info traces (if there are any). */

#if 0
#define PRINT_SAMPLE(info, sample) cr_log_info("%s (%d, %d, %d)\n", info, sample.long_1, sample.long_2, sample.long_3);
#else
#define PRINT_SAMPLE(info, sample)
#endif



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
disposing_init(void)
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

    g_topic = dds_create_topic(g_participant, &Space_Type1_desc, create_topic_name("ddsc_disposing_test", name, sizeof name), qos, NULL);
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
        PRINT_SAMPLE("INIT: Write     ", sample);
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
disposing_fini(void)
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
 * These will check the dds_writedispose() in various ways.
 *
 *************************************************************************************************/
/*************************************************************************************************/
Test(ddsc_writedispose, deleted, .init=disposing_init, .fini=disposing_fini)
{
    dds_return_t ret;
    dds_delete(g_writer);
    OS_WARNING_MSVC_OFF(6387); /* Disable SAL warning on intentional misuse of the API */
    ret = dds_writedispose(g_writer, NULL);
    OS_WARNING_MSVC_ON(6387);
    cr_assert_eq(dds_err_nr(ret), DDS_RETCODE_ALREADY_DELETED, "returned %d", dds_err_nr(ret));
}
/*************************************************************************************************/

/*************************************************************************************************/
Test(ddsc_writedispose, null, .init=disposing_init, .fini=disposing_fini)
{
    dds_return_t ret;
    OS_WARNING_MSVC_OFF(6387); /* Disable SAL warning on intentional misuse of the API */
    ret = dds_writedispose(g_writer, NULL);
    OS_WARNING_MSVC_ON(6387);
    cr_assert_eq(dds_err_nr(ret), DDS_RETCODE_BAD_PARAMETER, "returned %d", dds_err_nr(ret));
}
/*************************************************************************************************/

/*************************************************************************************************/
TheoryDataPoints(ddsc_writedispose, invalid_writers) = {
        DataPoints(dds_entity_t, -2, -1, 0, 1, 100, INT_MAX, INT_MIN),
};
Theory((dds_entity_t writer), ddsc_writedispose, invalid_writers, .init=disposing_init, .fini=disposing_fini)
{
    dds_entity_t exp = DDS_RETCODE_BAD_PARAMETER * -1;
    dds_return_t ret;

    OS_WARNING_MSVC_OFF(6387); /* Disable SAL warning on intentional misuse of the API */
    ret = dds_writedispose(writer, NULL);
    OS_WARNING_MSVC_ON(6387);
    cr_assert_eq(dds_err_nr(ret), dds_err_nr(exp), "returned %d != expected %d", dds_err_nr(ret), dds_err_nr(exp));
}
/*************************************************************************************************/

/*************************************************************************************************/
TheoryDataPoints(ddsc_writedispose, non_writers) = {
        DataPoints(dds_entity_t*, &g_waitset, &g_reader, &g_topic, &g_participant),
};
Theory((dds_entity_t *writer), ddsc_writedispose, non_writers, .init=disposing_init, .fini=disposing_fini)
{
    dds_return_t ret;
    OS_WARNING_MSVC_OFF(6387); /* Disable SAL warning on intentional misuse of the API */
    ret = dds_writedispose(*writer, NULL);
    OS_WARNING_MSVC_ON(6387);
    cr_assert_eq(dds_err_nr(ret), DDS_RETCODE_ILLEGAL_OPERATION, "returned %d", dds_err_nr(ret));
}
/*************************************************************************************************/

/*************************************************************************************************/
Test(ddsc_writedispose, disposing_old_instance, .init=disposing_init, .fini=disposing_fini)
{
    Space_Type1 oldInstance = { 0, 22, 22 };
    dds_return_t ret;

    ret = dds_writedispose(g_writer, &oldInstance);
    cr_assert_eq(ret, DDS_RETCODE_OK, "Disposing old instance returned %d", dds_err_nr(ret));

    /* Read all samples that matches filter. */
    ret = dds_read(g_reader, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
    cr_assert_eq(ret, 2, "# read %d, expected %d", ret, 2);
    for(int i = 0; i < ret; i++) {
        Space_Type1 *sample = (Space_Type1*)g_samples[i];
        if (sample->long_1 == 0) {
            /* Check data. */
            cr_assert_eq(sample->long_2, 22);
            cr_assert_eq(sample->long_3, 22);

            /* Check states. */
            cr_assert_eq(g_info[i].valid_data,     true);
            cr_assert_eq(g_info[i].sample_state,   DDS_SST_NOT_READ);
            cr_assert_eq(g_info[i].view_state,     DDS_VST_NEW);
            cr_assert_eq(g_info[i].instance_state, DDS_IST_NOT_ALIVE_DISPOSED);
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
Test(ddsc_writedispose, disposing_new_instance, .init=disposing_init, .fini=disposing_fini)
{
    Space_Type1 newInstance = { INITIAL_SAMPLES, 42, 42 };
    dds_return_t ret;

    ret = dds_writedispose(g_writer, &newInstance);
    cr_assert_eq(ret, DDS_RETCODE_OK, "Disposing new instance returned %d", dds_err_nr(ret));

    /* Read all samples that matches filter. */
    ret = dds_read(g_reader, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
    cr_assert_eq(ret, 3, "# read %d, expected %d", ret, 3);
    for(int i = 0; i < ret; i++) {
        Space_Type1 *sample = (Space_Type1*)g_samples[i];
        if (sample->long_1 < INITIAL_SAMPLES) {
            /* Check data. */
            cr_assert_eq(sample->long_2, sample->long_1 * 2);
            cr_assert_eq(sample->long_3, sample->long_1 * 3);

            /* Check states. */
            cr_assert_eq(g_info[i].valid_data,     true);
            cr_assert_eq(g_info[i].sample_state,   DDS_SST_NOT_READ);
            cr_assert_eq(g_info[i].view_state,     DDS_VST_NEW);
            cr_assert_eq(g_info[i].instance_state, DDS_ALIVE_INSTANCE_STATE);
        } else if (sample->long_1 == INITIAL_SAMPLES) {
            /* Check data. */
            cr_assert_eq(sample->long_2, 42);
            cr_assert_eq(sample->long_3, 42);

            /* Check states. */
            cr_assert_eq(g_info[i].valid_data,     true);
            cr_assert_eq(g_info[i].sample_state,   DDS_SST_NOT_READ);
            cr_assert_eq(g_info[i].view_state,     DDS_VST_NEW);
            cr_assert_eq(g_info[i].instance_state, DDS_IST_NOT_ALIVE_DISPOSED);
        } else {
            cr_assert(false, "Unknown sample read");
        }
    }
}
/*************************************************************************************************/

/*************************************************************************************************/
Test(ddsc_writedispose, timeout, .init=disposing_init, .fini=disposing_fini)
{
    Space_Type1 newInstance1 = { INITIAL_SAMPLES  , 22, 22 };
    Space_Type1 newInstance2 = { INITIAL_SAMPLES+1, 42, 42 };
    dds_return_t ret;

    ret = dds_writedispose(g_writer, &newInstance1);
    cr_assert_eq(ret, DDS_RETCODE_OK, "Disposing new instance returned %d", dds_err_nr(ret));
    ret = dds_writedispose(g_writer, &newInstance2);
    cr_assert_eq(dds_err_nr(ret), DDS_RETCODE_TIMEOUT, "Disposing new instance returned %d", dds_err_nr(ret));
}
/*************************************************************************************************/




/**************************************************************************************************
 *
 * These will check the dds_writedispose_ts() in various ways.
 *
 *************************************************************************************************/
/*************************************************************************************************/
Test(ddsc_writedispose_ts, deleted, .init=disposing_init, .fini=disposing_fini)
{
    dds_return_t ret;
    dds_delete(g_writer);
    OS_WARNING_MSVC_OFF(6387); /* Disable SAL warning on intentional misuse of the API */
    ret = dds_writedispose_ts(g_writer, NULL, g_present);
    OS_WARNING_MSVC_ON(6387);
    cr_assert_eq(dds_err_nr(ret), DDS_RETCODE_ALREADY_DELETED, "returned %d", dds_err_nr(ret));
}
/*************************************************************************************************/

/*************************************************************************************************/
Test(ddsc_writedispose_ts, null, .init=disposing_init, .fini=disposing_fini)
{
    dds_return_t ret;
    OS_WARNING_MSVC_OFF(6387); /* Disable SAL warning on intentional misuse of the API */
    ret = dds_writedispose_ts(g_writer, NULL, g_present);
    OS_WARNING_MSVC_ON(6387);
    cr_assert_eq(dds_err_nr(ret), DDS_RETCODE_BAD_PARAMETER, "returned %d", dds_err_nr(ret));
}
/*************************************************************************************************/

/*************************************************************************************************/
Test(ddsc_writedispose_ts, timeout, .init=disposing_init, .fini=disposing_fini)
{
    Space_Type1 newInstance1 = { INITIAL_SAMPLES  , 22, 22 };
    Space_Type1 newInstance2 = { INITIAL_SAMPLES+1, 42, 42 };
    dds_return_t ret;

    ret = dds_writedispose_ts(g_writer, &newInstance1, g_present);
    cr_assert_eq(ret, DDS_RETCODE_OK, "Disposing new instance returned %d", dds_err_nr(ret));
    ret = dds_writedispose_ts(g_writer, &newInstance2, g_present);
    cr_assert_eq(dds_err_nr(ret), DDS_RETCODE_TIMEOUT, "Disposing new instance returned %d", dds_err_nr(ret));
}
/*************************************************************************************************/

/*************************************************************************************************/
TheoryDataPoints(ddsc_writedispose_ts, invalid_writers) = {
        DataPoints(dds_entity_t, -2, -1, 0, 1, 100, INT_MAX, INT_MIN),
};
Theory((dds_entity_t writer), ddsc_writedispose_ts, invalid_writers, .init=disposing_init, .fini=disposing_fini)
{
    dds_entity_t exp = DDS_RETCODE_BAD_PARAMETER * -1;
    dds_return_t ret;

    OS_WARNING_MSVC_OFF(6387); /* Disable SAL warning on intentional misuse of the API */
    ret = dds_writedispose_ts(writer, NULL, g_present);
    OS_WARNING_MSVC_ON(6387);
    cr_assert_eq(dds_err_nr(ret), dds_err_nr(exp), "returned %d != expected %d", dds_err_nr(ret), dds_err_nr(exp));
}
/*************************************************************************************************/

/*************************************************************************************************/
TheoryDataPoints(ddsc_writedispose_ts, non_writers) = {
        DataPoints(dds_entity_t*, &g_waitset, &g_reader, &g_topic, &g_participant),
};
Theory((dds_entity_t *writer), ddsc_writedispose_ts, non_writers, .init=disposing_init, .fini=disposing_fini)
{
    dds_return_t ret;
    OS_WARNING_MSVC_OFF(6387); /* Disable SAL warning on intentional misuse of the API */
    ret = dds_writedispose_ts(*writer, NULL, g_present);
    OS_WARNING_MSVC_ON(6387);
    cr_assert_eq(dds_err_nr(ret), DDS_RETCODE_ILLEGAL_OPERATION, "returned %d", dds_err_nr(ret));
}
/*************************************************************************************************/

/*************************************************************************************************/
Test(ddsc_writedispose_ts, disposing_old_instance, .init=disposing_init, .fini=disposing_fini)
{
    Space_Type1 oldInstance = { 0, 22, 22 };
    dds_return_t ret;

    ret = dds_writedispose_ts(g_writer, &oldInstance, g_present);
    cr_assert_eq(ret, DDS_RETCODE_OK, "Disposing old instance returned %d", dds_err_nr(ret));

    /* Read all available samples. */
    ret = dds_read(g_reader, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
    cr_assert_eq(ret, 2, "# read %d, expected %d", ret, 2);
    for(int i = 0; i < ret; i++) {
        Space_Type1 *sample = (Space_Type1*)g_samples[i];
        if (sample->long_1 == 0) {
            /* Check data. */
            cr_assert_eq(sample->long_2, 22);
            cr_assert_eq(sample->long_3, 22);

            /* Check states. */
            cr_assert_eq(g_info[i].valid_data,     true);
            cr_assert_eq(g_info[i].sample_state,   DDS_SST_NOT_READ);
            cr_assert_eq(g_info[i].view_state,     DDS_VST_NEW);
            cr_assert_eq(g_info[i].instance_state, DDS_IST_NOT_ALIVE_DISPOSED);
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
Test(ddsc_writedispose_ts, disposing_new_instance, .init=disposing_init, .fini=disposing_fini)
{
    Space_Type1 newInstance = { INITIAL_SAMPLES, 42, 42 };
    dds_return_t ret;

    ret = dds_writedispose_ts(g_writer, &newInstance, g_present);
    cr_assert_eq(ret, DDS_RETCODE_OK, "Disposing new instance returned %d", dds_err_nr(ret));

    /* Read all available samples. */
    ret = dds_read(g_reader, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
    cr_assert_eq(ret, 3, "# read %d, expected %d", ret, 3);
    for(int i = 0; i < ret; i++) {
        Space_Type1 *sample = (Space_Type1*)g_samples[i];
        if (sample->long_1 < INITIAL_SAMPLES) {
            /* Check data. */
            cr_assert_eq(sample->long_2, sample->long_1 * 2);
            cr_assert_eq(sample->long_3, sample->long_1 * 3);

            /* Check states. */
            cr_assert_eq(g_info[i].valid_data,     true);
            cr_assert_eq(g_info[i].sample_state,   DDS_SST_NOT_READ);
            cr_assert_eq(g_info[i].view_state,     DDS_VST_NEW);
            cr_assert_eq(g_info[i].instance_state, DDS_ALIVE_INSTANCE_STATE);
        } else if (sample->long_1 == INITIAL_SAMPLES) {
            /* Check data. */
            cr_assert_eq(sample->long_2, 42);
            cr_assert_eq(sample->long_3, 42);

            /* Check states. */
            cr_assert_eq(g_info[i].valid_data,     true);
            cr_assert_eq(g_info[i].sample_state,   DDS_SST_NOT_READ);
            cr_assert_eq(g_info[i].view_state,     DDS_VST_NEW);
            cr_assert_eq(g_info[i].instance_state, DDS_IST_NOT_ALIVE_DISPOSED);
        } else {
            cr_assert(false, "Unknown sample read");
        }
    }
}
/*************************************************************************************************/

/*************************************************************************************************/
Test(ddsc_writedispose_ts, disposing_past_sample, .init=disposing_init, .fini=disposing_fini)
{
    Space_Type1 oldInstance = { 0, 0, 0 };
    dds_return_t ret;

    /* Disposing a sample in the past should trigger a lost sample. */
    ret = dds_set_enabled_status(g_reader, DDS_SAMPLE_LOST_STATUS);
    cr_assert_eq(ret, DDS_RETCODE_OK, "Failed to set prerequisite g_reader status");
    ret = dds_waitset_attach(g_waitset, g_reader, g_reader);
    cr_assert_eq(ret, DDS_RETCODE_OK, "Failed to attach prerequisite g_reader");

    /* Now, dispose a sample in the past. */
    ret = dds_writedispose_ts(g_writer, &oldInstance, g_past);
    cr_assert_eq(ret, DDS_RETCODE_OK, "Disposing old instance returned %d", dds_err_nr(ret));

    /* Wait for 'sample lost'. */
    ret = dds_waitset_wait(g_waitset, NULL, 0, DDS_SECS(1));
    cr_assert_eq(ret, 1, "Disposing past sample did not trigger 'sample lost'");

    /* Read all available samples. */
    ret = dds_read(g_reader, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
    cr_assert_eq(ret, 2, "# read %d, expected %d", ret, 2);
    for(int i = 0; i < ret; i++) {
        Space_Type1 *sample = (Space_Type1*)g_samples[i];
        if (sample->long_1 < INITIAL_SAMPLES) {
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
 * These will check the dds_dispose() in various ways.
 *
 *************************************************************************************************/
/*************************************************************************************************/
Test(ddsc_dispose, deleted, .init=disposing_init, .fini=disposing_fini)
{
    dds_return_t ret;
    dds_delete(g_writer);
    OS_WARNING_MSVC_OFF(6387); /* Disable SAL warning on intentional misuse of the API */
    ret = dds_dispose(g_writer, NULL);
    OS_WARNING_MSVC_ON(6387);
    cr_assert_eq(dds_err_nr(ret), DDS_RETCODE_ALREADY_DELETED, "returned %d", dds_err_nr(ret));
}
/*************************************************************************************************/

/*************************************************************************************************/
Test(ddsc_dispose, null, .init=disposing_init, .fini=disposing_fini)
{
    dds_return_t ret;
    OS_WARNING_MSVC_OFF(6387); /* Disable SAL warning on intentional misuse of the API */
    ret = dds_dispose(g_writer, NULL);
    OS_WARNING_MSVC_ON(6387);
    cr_assert_eq(dds_err_nr(ret), DDS_RETCODE_BAD_PARAMETER, "returned %d", dds_err_nr(ret));
}
/*************************************************************************************************/

/*************************************************************************************************/
Test(ddsc_dispose, timeout, .init=disposing_init, .fini=disposing_fini)
{
    Space_Type1 newInstance1 = { INITIAL_SAMPLES  , 22, 22 };
    Space_Type1 newInstance2 = { INITIAL_SAMPLES+1, 42, 42 };
    dds_return_t ret;

    ret = dds_dispose(g_writer, &newInstance1);
    cr_assert_eq(ret, DDS_RETCODE_OK, "Disposing new instance returned %d", dds_err_nr(ret));
    ret = dds_dispose(g_writer, &newInstance2);
    cr_assert_eq(dds_err_nr(ret), DDS_RETCODE_TIMEOUT, "Disposing new instance returned %d", dds_err_nr(ret));
}
/*************************************************************************************************/

/*************************************************************************************************/
TheoryDataPoints(ddsc_dispose, invalid_writers) = {
        DataPoints(dds_entity_t, -2, -1, 0, 1, 100, INT_MAX, INT_MIN),
};
Theory((dds_entity_t writer), ddsc_dispose, invalid_writers, .init=disposing_init, .fini=disposing_fini)
{
    dds_entity_t exp = DDS_RETCODE_BAD_PARAMETER * -1;
    dds_return_t ret;

    OS_WARNING_MSVC_OFF(6387); /* Disable SAL warning on intentional misuse of the API */
    ret = dds_dispose(writer, NULL);
    OS_WARNING_MSVC_ON(6387);
    cr_assert_eq(dds_err_nr(ret), dds_err_nr(exp), "returned %d != expected %d", dds_err_nr(ret), dds_err_nr(exp));
}
/*************************************************************************************************/

/*************************************************************************************************/
TheoryDataPoints(ddsc_dispose, non_writers) = {
        DataPoints(dds_entity_t*, &g_waitset, &g_reader, &g_topic, &g_participant),
};
Theory((dds_entity_t *writer), ddsc_dispose, non_writers, .init=disposing_init, .fini=disposing_fini)
{
    dds_return_t ret;
    OS_WARNING_MSVC_OFF(6387); /* Disable SAL warning on intentional misuse of the API */
    ret = dds_dispose(*writer, NULL);
    OS_WARNING_MSVC_ON(6387);
    cr_assert_eq(dds_err_nr(ret), DDS_RETCODE_ILLEGAL_OPERATION, "returned %d", dds_err_nr(ret));
}
/*************************************************************************************************/

/*************************************************************************************************/
Test(ddsc_dispose, disposing_old_instance, .init=disposing_init, .fini=disposing_fini)
{
    Space_Type1 oldInstance = { 0, 22, 22 };
    dds_return_t ret;

    ret = dds_dispose(g_writer, &oldInstance);
    cr_assert_eq(ret, DDS_RETCODE_OK, "Disposing old instance returned %d", dds_err_nr(ret));

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
            cr_assert_eq(g_info[i].instance_state, DDS_IST_NOT_ALIVE_DISPOSED);
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
Test(ddsc_dispose, disposing_new_instance, .init=disposing_init, .fini=disposing_fini)
{
    Space_Type1 newInstance = { INITIAL_SAMPLES, 42, 42 };
    dds_return_t ret;

    ret = dds_dispose(g_writer, &newInstance);
    cr_assert_eq(ret, DDS_RETCODE_OK, "Disposing new instance returned %d", dds_err_nr(ret));

    /* Read all available samples. */
    ret = dds_read(g_reader, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
    cr_assert_eq(ret, 3, "# read %d, expected %d", ret, 3);
    for(int i = 0; i < ret; i++) {
        Space_Type1 *sample = (Space_Type1*)g_samples[i];
        if (sample->long_1 < INITIAL_SAMPLES) {
            /* Check data. */
            cr_assert_eq(sample->long_2, sample->long_1 * 2);
            cr_assert_eq(sample->long_3, sample->long_1 * 3);

            /* Check states. */
            cr_assert_eq(g_info[i].valid_data,     true);
            cr_assert_eq(g_info[i].sample_state,   DDS_SST_NOT_READ);
            cr_assert_eq(g_info[i].view_state,     DDS_VST_NEW);
            cr_assert_eq(g_info[i].instance_state, DDS_ALIVE_INSTANCE_STATE);
        } else if (sample->long_1 == INITIAL_SAMPLES) {
            /* Don't check data; it's not valid. */

            /* Check states. */
            cr_assert_eq(g_info[i].valid_data,     false);
            cr_assert_eq(g_info[i].sample_state,   DDS_SST_NOT_READ);
            cr_assert_eq(g_info[i].view_state,     DDS_VST_NEW);
            cr_assert_eq(g_info[i].instance_state, DDS_IST_NOT_ALIVE_DISPOSED);
        } else {
            cr_assert(false, "Unknown sample read");
        }
    }
}
/*************************************************************************************************/




/**************************************************************************************************
 *
 * These will check the dds_dispose_ts() in various ways.
 *
 *************************************************************************************************/
/*************************************************************************************************/
Test(ddsc_dispose_ts, deleted, .init=disposing_init, .fini=disposing_fini)
{
    dds_return_t ret;
    dds_delete(g_writer);
    OS_WARNING_MSVC_OFF(6387); /* Disable SAL warning on intentional misuse of the API */
    ret = dds_dispose_ts(g_writer, NULL, g_present);
    OS_WARNING_MSVC_ON(6387); /* Disable SAL warning on intentional misuse of the API */
    cr_assert_eq(dds_err_nr(ret), DDS_RETCODE_ALREADY_DELETED, "returned %d", dds_err_nr(ret));
}
/*************************************************************************************************/

/*************************************************************************************************/
Test(ddsc_dispose_ts, null, .init=disposing_init, .fini=disposing_fini)
{
    dds_return_t ret;
    OS_WARNING_MSVC_OFF(6387); /* Disable SAL warning on intentional misuse of the API */
    ret = dds_dispose_ts(g_writer, NULL, g_present);
    OS_WARNING_MSVC_ON(6387);
    cr_assert_eq(dds_err_nr(ret), DDS_RETCODE_BAD_PARAMETER, "returned %d", dds_err_nr(ret));
}
/*************************************************************************************************/

/*************************************************************************************************/
Test(ddsc_dispose_ts, timeout, .init=disposing_init, .fini=disposing_fini)
{
    Space_Type1 newInstance1 = { INITIAL_SAMPLES  , 22, 22 };
    Space_Type1 newInstance2 = { INITIAL_SAMPLES+1, 42, 42 };
    dds_return_t ret;

    ret = dds_dispose_ts(g_writer, &newInstance1, g_present);
    cr_assert_eq(ret, DDS_RETCODE_OK, "Disposing new instance returned %d", dds_err_nr(ret));
    ret = dds_dispose_ts(g_writer, &newInstance2, g_present);
    cr_assert_eq(dds_err_nr(ret), DDS_RETCODE_TIMEOUT, "Disposing new instance returned %d", dds_err_nr(ret));
}
/*************************************************************************************************/

/*************************************************************************************************/
TheoryDataPoints(ddsc_dispose_ts, invalid_writers) = {
        DataPoints(dds_entity_t, -2, -1, 0, 1, 100, INT_MAX, INT_MIN),
};
Theory((dds_entity_t writer), ddsc_dispose_ts, invalid_writers, .init=disposing_init, .fini=disposing_fini)
{
    dds_entity_t exp = DDS_RETCODE_BAD_PARAMETER * -1;
    dds_return_t ret;

    OS_WARNING_MSVC_OFF(6387); /* Disable SAL warning on intentional misuse of the API */
    ret = dds_dispose_ts(writer, NULL, g_present);
    OS_WARNING_MSVC_ON(6387);
    cr_assert_eq(dds_err_nr(ret), dds_err_nr(exp), "returned %d != expected %d", dds_err_nr(ret), dds_err_nr(exp));
}
/*************************************************************************************************/

/*************************************************************************************************/
TheoryDataPoints(ddsc_dispose_ts, non_writers) = {
        DataPoints(dds_entity_t*, &g_waitset, &g_reader, &g_topic, &g_participant),
};
Theory((dds_entity_t *writer), ddsc_dispose_ts, non_writers, .init=disposing_init, .fini=disposing_fini)
{
    dds_return_t ret;
    OS_WARNING_MSVC_OFF(6387); /* Disable SAL warning on intentional misuse of the API */
    ret = dds_dispose_ts(*writer, NULL, g_present);
    OS_WARNING_MSVC_ON(6387);
    cr_assert_eq(dds_err_nr(ret), DDS_RETCODE_ILLEGAL_OPERATION, "returned %d", dds_err_nr(ret));
}
/*************************************************************************************************/

/*************************************************************************************************/
Test(ddsc_dispose_ts, disposing_old_instance, .init=disposing_init, .fini=disposing_fini)
{
    Space_Type1 oldInstance = { 0, 22, 22 };
    dds_return_t ret;

    ret = dds_dispose_ts(g_writer, &oldInstance, g_present);
    cr_assert_eq(ret, DDS_RETCODE_OK, "Disposing old instance returned %d", dds_err_nr(ret));

    /* Read all available samples. */
    ret = dds_read(g_reader, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
    cr_assert_eq(ret, 2, "# read %d, expected %d", ret, 2);
    for(int i = 0; i < ret; i++) {
        Space_Type1 *sample = (Space_Type1*)g_samples[i];
        if (sample->long_1 == 0) {
            /* Check data (data part of dispose is not used, only the key part). */
            cr_assert_eq(sample->long_2, sample->long_1 * 2);
            cr_assert_eq(sample->long_3, sample->long_1 * 3);

            /* Check states. */
            cr_assert_eq(g_info[i].valid_data,     true);
            cr_assert_eq(g_info[i].sample_state,   DDS_SST_NOT_READ);
            cr_assert_eq(g_info[i].view_state,     DDS_VST_NEW);
            cr_assert_eq(g_info[i].instance_state, DDS_IST_NOT_ALIVE_DISPOSED);
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
Test(ddsc_dispose_ts, disposing_new_instance, .init=disposing_init, .fini=disposing_fini)
{
    Space_Type1 newInstance = { INITIAL_SAMPLES, 42, 42 };
    dds_return_t ret;

    ret = dds_dispose_ts(g_writer, &newInstance, g_present);
    cr_assert_eq(ret, DDS_RETCODE_OK, "Disposing new instance returned %d", dds_err_nr(ret));

    /* Read all available samples. */
    ret = dds_read(g_reader, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
    cr_assert_eq(ret, 3, "# read %d, expected %d", ret, 3);
    for(int i = 0; i < ret; i++) {
        Space_Type1 *sample = (Space_Type1*)g_samples[i];
        if (sample->long_1 < INITIAL_SAMPLES) {
            /* Check data. */
            cr_assert_eq(sample->long_2, sample->long_1 * 2);
            cr_assert_eq(sample->long_3, sample->long_1 * 3);

            /* Check states. */
            cr_assert_eq(g_info[i].valid_data,     true);
            cr_assert_eq(g_info[i].sample_state,   DDS_SST_NOT_READ);
            cr_assert_eq(g_info[i].view_state,     DDS_VST_NEW);
            cr_assert_eq(g_info[i].instance_state, DDS_ALIVE_INSTANCE_STATE);
        } else if (sample->long_1 == INITIAL_SAMPLES) {
            /* Don't check data; it's not valid. */

            /* Check states. */
            cr_assert_eq(g_info[i].valid_data,     false);
            cr_assert_eq(g_info[i].sample_state,   DDS_SST_NOT_READ);
            cr_assert_eq(g_info[i].view_state,     DDS_VST_NEW);
            cr_assert_eq(g_info[i].instance_state, DDS_IST_NOT_ALIVE_DISPOSED);
        } else {
            cr_assert(false, "Unknown sample read");
        }
    }
}
/*************************************************************************************************/

/*************************************************************************************************/
Test(ddsc_dispose_ts, disposing_past_sample, .init=disposing_init, .fini=disposing_fini)
{
    Space_Type1 oldInstance = { 0, 0, 0 };
    dds_return_t ret;

    /* Disposing a sample in the past should trigger a lost sample. */
    ret = dds_set_enabled_status(g_reader, DDS_SAMPLE_LOST_STATUS);
    cr_assert_eq(ret, DDS_RETCODE_OK, "Failed to set prerequisite g_reader status");
    ret = dds_waitset_attach(g_waitset, g_reader, g_reader);
    cr_assert_eq(ret, DDS_RETCODE_OK, "Failed to attach prerequisite g_reader");

    /* Now, dispose a sample in the past. */
    ret = dds_dispose_ts(g_writer, &oldInstance, g_past);
    cr_assert_eq(ret, DDS_RETCODE_OK, "Disposing old instance returned %d", dds_err_nr(ret));

    /* Wait for 'sample lost'. */
    ret = dds_waitset_wait(g_waitset, NULL, 0, DDS_SECS(1));
    cr_assert_eq(ret, 1, "Disposing past sample did not trigger 'sample lost'");

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
            cr_assert_eq(g_info[i].instance_state, DDS_ALIVE_INSTANCE_STATE);
        } else {
            cr_assert(false, "Unknown sample read");
        }
    }

}
/*************************************************************************************************/




/**************************************************************************************************
 *
 * These will check the dds_dispose_ih() in various ways.
 *
 *************************************************************************************************/
/*************************************************************************************************/
Test(ddsc_dispose_ih, deleted, .init=disposing_init, .fini=disposing_fini)
{
    dds_return_t ret;
    dds_delete(g_writer);
    ret = dds_dispose_ih(g_writer, DDS_HANDLE_NIL);
    cr_assert_eq(dds_err_nr(ret), DDS_RETCODE_ALREADY_DELETED, "returned %d", dds_err_nr(ret));
}
/*************************************************************************************************/

/*************************************************************************************************/
TheoryDataPoints(ddsc_dispose_ih, invalid_handles) = {
        DataPoints(dds_instance_handle_t, DDS_HANDLE_NIL, 0, 1, 100, UINT64_MAX),
};
Theory((dds_instance_handle_t handle), ddsc_dispose_ih, invalid_handles, .init=disposing_init, .fini=disposing_fini)
{
    dds_return_t ret;
    ret = dds_dispose_ih(g_writer, handle);
    cr_assert_eq(dds_err_nr(ret), DDS_RETCODE_PRECONDITION_NOT_MET, "returned %d", dds_err_nr(ret));
}
/*************************************************************************************************/

/*************************************************************************************************/
TheoryDataPoints(ddsc_dispose_ih, invalid_writers) = {
        DataPoints(dds_entity_t, -2, -1, 0, 1, 100, INT_MAX, INT_MIN),
};
Theory((dds_entity_t writer), ddsc_dispose_ih, invalid_writers, .init=disposing_init, .fini=disposing_fini)
{
    dds_entity_t exp = DDS_RETCODE_BAD_PARAMETER * -1;
    dds_return_t ret;

    ret = dds_dispose_ih(writer, DDS_HANDLE_NIL);
    cr_assert_eq(dds_err_nr(ret), dds_err_nr(exp), "returned %d != expected %d", dds_err_nr(ret), dds_err_nr(exp));
}
/*************************************************************************************************/

/*************************************************************************************************/
TheoryDataPoints(ddsc_dispose_ih, non_writers) = {
        DataPoints(dds_entity_t*, &g_waitset, &g_reader, &g_topic, &g_participant),
};
Theory((dds_entity_t *writer), ddsc_dispose_ih, non_writers, .init=disposing_init, .fini=disposing_fini)
{
    dds_return_t ret;
    ret = dds_dispose_ih(*writer, DDS_HANDLE_NIL);
    cr_assert_eq(dds_err_nr(ret), DDS_RETCODE_ILLEGAL_OPERATION, "returned %d", dds_err_nr(ret));
}
/*************************************************************************************************/

/*************************************************************************************************/
Test(ddsc_dispose_ih, disposing_old_instance, .init=disposing_init, .fini=disposing_fini)
{
    Space_Type1 oldInstance = { 0, 22, 22 };
    dds_instance_handle_t hdl = dds_instance_lookup(g_writer, &oldInstance);
    dds_return_t ret;

    ret = dds_dispose_ih(g_writer, hdl);
    cr_assert_eq(ret, DDS_RETCODE_OK, "Disposing old instance returned %d", dds_err_nr(ret));

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
            cr_assert_eq(g_info[i].instance_state, DDS_IST_NOT_ALIVE_DISPOSED);
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
 * These will check the dds_dispose_ih_ts() in various ways.
 *
 *************************************************************************************************/
/*************************************************************************************************/
Test(ddsc_dispose_ih_ts, deleted, .init=disposing_init, .fini=disposing_fini)
{
    dds_return_t ret;
    dds_delete(g_writer);
    ret = dds_dispose_ih_ts(g_writer, DDS_HANDLE_NIL, g_present);
    cr_assert_eq(dds_err_nr(ret), DDS_RETCODE_ALREADY_DELETED, "returned %d", dds_err_nr(ret));
}
/*************************************************************************************************/

/*************************************************************************************************/
TheoryDataPoints(ddsc_dispose_ih_ts, invalid_handles) = {
        DataPoints(dds_instance_handle_t, DDS_HANDLE_NIL, 0, 1, 100, UINT64_MAX),
};
Theory((dds_instance_handle_t handle), ddsc_dispose_ih_ts, invalid_handles, .init=disposing_init, .fini=disposing_fini)
{
    dds_return_t ret;
    ret = dds_dispose_ih_ts(g_writer, handle, g_present);
    cr_assert_eq(dds_err_nr(ret), DDS_RETCODE_PRECONDITION_NOT_MET, "returned %d", dds_err_nr(ret));
}
/*************************************************************************************************/

/*************************************************************************************************/
TheoryDataPoints(ddsc_dispose_ih_ts, invalid_writers) = {
        DataPoints(dds_entity_t, -2, -1, 0, 1, 100, INT_MAX, INT_MIN),
};
Theory((dds_entity_t writer), ddsc_dispose_ih_ts, invalid_writers, .init=disposing_init, .fini=disposing_fini)
{
    dds_entity_t exp = DDS_RETCODE_BAD_PARAMETER * -1;
    dds_return_t ret;

    ret = dds_dispose_ih_ts(writer, DDS_HANDLE_NIL, g_present);
    cr_assert_eq(dds_err_nr(ret), dds_err_nr(exp), "returned %d != expected %d", dds_err_nr(ret), dds_err_nr(exp));
}
/*************************************************************************************************/

/*************************************************************************************************/
TheoryDataPoints(ddsc_dispose_ih_ts, non_writers) = {
        DataPoints(dds_entity_t*, &g_waitset, &g_reader, &g_topic, &g_participant),
};
Theory((dds_entity_t *writer), ddsc_dispose_ih_ts, non_writers, .init=disposing_init, .fini=disposing_fini)
{
    dds_return_t ret;
    ret = dds_dispose_ih_ts(*writer, DDS_HANDLE_NIL, g_present);
    cr_assert_eq(dds_err_nr(ret), DDS_RETCODE_ILLEGAL_OPERATION, "returned %d", dds_err_nr(ret));
}
/*************************************************************************************************/

/*************************************************************************************************/
Test(ddsc_dispose_ih_ts, disposing_old_instance, .init=disposing_init, .fini=disposing_fini)
{
    Space_Type1 oldInstance = { 0, 22, 22 };
    dds_instance_handle_t hdl = dds_instance_lookup(g_writer, &oldInstance);
    dds_return_t ret;

    ret = dds_dispose_ih_ts(g_writer, hdl, g_present);
    cr_assert_eq(ret, DDS_RETCODE_OK, "Disposing old instance returned %d", dds_err_nr(ret));

    /* Read all available samples. */
    ret = dds_read(g_reader, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
    cr_assert_eq(ret, 2, "# read %d, expected %d", ret, 2);
    for(int i = 0; i < ret; i++) {
        Space_Type1 *sample = (Space_Type1*)g_samples[i];
        if (sample->long_1 == 0) {
            /* Check data (data part of dispose is not used, only the key part). */
            cr_assert_eq(sample->long_2, sample->long_1 * 2);
            cr_assert_eq(sample->long_3, sample->long_1 * 3);

            /* Check states. */
            cr_assert_eq(g_info[i].valid_data,     true);
            cr_assert_eq(g_info[i].sample_state,   DDS_SST_NOT_READ);
            cr_assert_eq(g_info[i].view_state,     DDS_VST_NEW);
            cr_assert_eq(g_info[i].instance_state, DDS_IST_NOT_ALIVE_DISPOSED);
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
Test(ddsc_dispose_ih_ts, disposing_past_sample, .init=disposing_init, .fini=disposing_fini)
{
    Space_Type1 oldInstance = { 0, 0, 0 };
    dds_instance_handle_t hdl = dds_instance_lookup(g_writer, &oldInstance);
    dds_return_t ret;

    /* Disposing a sample in the past should trigger a lost sample. */
    ret = dds_set_enabled_status(g_reader, DDS_SAMPLE_LOST_STATUS);
    cr_assert_eq(ret, DDS_RETCODE_OK, "Failed to set prerequisite g_reader status");
    ret = dds_waitset_attach(g_waitset, g_reader, g_reader);
    cr_assert_eq(ret, DDS_RETCODE_OK, "Failed to attach prerequisite g_reader");

    /* Now, dispose a sample in the past. */
    ret = dds_dispose_ih_ts(g_writer, hdl, g_past);
    cr_assert_eq(ret, DDS_RETCODE_OK, "Disposing old instance returned %d", dds_err_nr(ret));

    /* Wait for 'sample lost'. */
    ret = dds_waitset_wait(g_waitset, NULL, 0, DDS_SECS(1));
    cr_assert_eq(ret, 1, "Disposing past sample did not trigger 'sample lost'");

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
            cr_assert_eq(g_info[i].instance_state, DDS_ALIVE_INSTANCE_STATE);
        } else {
            cr_assert(false, "Unknown sample read");
        }
    }

}
/*************************************************************************************************/


#endif
