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

//#define VERBOSE_INIT
#ifdef VERBOSE_INIT
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
/*
 * By writing, disposing, unregistering, reading and re-writing, the following
 * data will be available in the reader history and thus available for the
 * condition that is under test.
 * | long_1 | long_2 | long_3 |    sst   | vst |    ist     |
 * ----------------------------------------------------------
 * |    0   |    0   |    0   |     read | old | alive      |
 * |    1   |    0   |    0   |     read | old | disposed   |
 * |    2   |    1   |    0   |     read | old | no_writers |
 * |    3   |    1   |    1   | not_read | old | alive      |
 * |    4   |    2   |    1   | not_read | new | disposed   |
 * |    5   |    2   |    1   | not_read | new | no_writers |
 * |    6   |    3   |    2   | not_read | new | alive      |
 */
#define SAMPLE_ALIVE_IST_CNT      (3)
#define SAMPLE_DISPOSED_IST_CNT   (2)
#define SAMPLE_NO_WRITER_IST_CNT  (2)
#define SAMPLE_LAST_READ_SST      (2)
#define SAMPLE_LAST_OLD_VST       (3)
#define SAMPLE_IST(idx)           (((idx % 3) == 0) ? DDS_IST_ALIVE              : \
                                   ((idx % 3) == 1) ? DDS_IST_NOT_ALIVE_DISPOSED : \
                                                      DDS_IST_NOT_ALIVE_NO_WRITERS )
#define SAMPLE_VST(idx)           ((idx <= SAMPLE_LAST_OLD_VST ) ? DDS_VST_OLD  : DDS_VST_NEW)
#define SAMPLE_SST(idx)           ((idx <= SAMPLE_LAST_READ_SST) ? DDS_SST_READ : DDS_SST_NOT_READ)


static dds_entity_t g_participant = 0;
static dds_entity_t g_topic       = 0;
static dds_entity_t g_reader      = 0;
static dds_entity_t g_writer      = 0;
static dds_entity_t g_waitset     = 0;

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
readcondition_init(void)
{
    Space_Type1 sample = { 0 };
    dds_qos_t *qos = dds_qos_create ();
    dds_attach_t triggered;
    dds_return_t ret;
    char name[100];

    g_participant = dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);
    cr_assert_gt(g_participant, 0, "Failed to create prerequisite g_participant");

    g_waitset = dds_create_waitset(g_participant);
    cr_assert_gt(g_waitset, 0, "Failed to create g_waitset");

    g_topic = dds_create_topic(g_participant, &Space_Type1_desc, create_topic_name("ddsc_readcondition_test", name, 100), NULL, NULL);
    cr_assert_gt(g_topic, 0, "Failed to create prerequisite g_topic");

    /* Create a reader that keeps last sample of all instances. */
    dds_qset_history(qos, DDS_HISTORY_KEEP_LAST, 1);
    g_reader = dds_create_reader(g_participant, g_topic, qos, NULL);
    cr_assert_gt(g_reader, 0, "Failed to create prerequisite g_reader");

    /* Create a reader that will not automatically dispose unregistered samples. */
    dds_qset_writer_data_lifecycle(qos, false);
    g_writer = dds_create_writer(g_participant, g_topic, qos, NULL);
    cr_assert_gt(g_writer, 0, "Failed to create prerequisite g_writer");

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

    /* Initialize reading buffers. */
    memset (g_data, 0, sizeof (g_data));
    for (int i = 0; i < MAX_SAMPLES; i++) {
        g_samples[i] = &g_data[i];
    }

    /* Write all samples. */
    for (int i = 0; i < MAX_SAMPLES; i++) {
        dds_instance_state_t ist = SAMPLE_IST(i);
        sample.long_1 = i;
        sample.long_2 = i/2;
        sample.long_3 = i/3;

        PRINT_SAMPLE("INIT: Write     ", sample);
        ret = dds_write(g_writer, &sample);
        cr_assert_eq(ret, DDS_RETCODE_OK, "Failed prerequisite write");

        if (ist == DDS_IST_NOT_ALIVE_DISPOSED) {
            PRINT_SAMPLE("INIT: Dispose   ", sample);
            ret = dds_dispose(g_writer, &sample);
            cr_assert_eq(ret, DDS_RETCODE_OK, "Failed prerequisite dispose");
        }
        if (ist == DDS_IST_NOT_ALIVE_NO_WRITERS) {
            PRINT_SAMPLE("INIT: Unregister", sample);
            ret = dds_unregister_instance(g_writer, &sample);
            cr_assert_eq(ret, DDS_RETCODE_OK, "Failed prerequisite unregister");
        }
    }

    /* Read samples to get read&old_view states. */
    ret = dds_read(g_reader, g_samples, g_info, MAX_SAMPLES, SAMPLE_LAST_OLD_VST + 1);
    cr_assert_eq(ret, SAMPLE_LAST_OLD_VST + 1, "Failed prerequisite read");
#ifdef VERBOSE_INIT
    for(int i = 0; i < ret; i++) {
        Space_Type1 *s = (Space_Type1*)g_samples[i];
        PRINT_SAMPLE("INIT: Read      ", (*s));
    }
#endif

    /* Re-write the samples that should be not_read&old_view. */
    for (int i = SAMPLE_LAST_READ_SST + 1; i <= SAMPLE_LAST_OLD_VST; i++) {
        dds_instance_state_t ist = SAMPLE_IST(i);
        sample.long_1 = i;
        sample.long_2 = i/2;
        sample.long_3 = i/3;

        PRINT_SAMPLE("INIT: Rewrite   ", sample);
        ret = dds_write(g_writer, &sample);
        cr_assert_eq(ret, DDS_RETCODE_OK, "Failed prerequisite write");

        if ((ist == DDS_IST_NOT_ALIVE_DISPOSED) && (i != 4)) {
            PRINT_SAMPLE("INIT: Dispose   ", sample);
            ret = dds_dispose(g_writer, &sample);
            cr_assert_eq(ret, DDS_RETCODE_OK, "Failed prerequisite dispose");
        }
        if (ist == DDS_IST_NOT_ALIVE_NO_WRITERS) {
            PRINT_SAMPLE("INIT: Unregister", sample);
            ret = dds_unregister_instance(g_writer, &sample);
            cr_assert_eq(ret, DDS_RETCODE_OK, "Failed prerequisite unregister");
        }
    }

    dds_qos_delete(qos);
}

static void
readcondition_fini(void)
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
 * These will check the readcondition creation in various ways.
 *
 *************************************************************************************************/
/*************************************************************************************************/
Test(ddsc_readcondition_create, second, .init=readcondition_init, .fini=readcondition_fini)
{
    uint32_t mask = DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
    dds_entity_t cond1;
    dds_entity_t cond2;
    dds_return_t ret;

    cond1 = dds_create_readcondition(g_reader, mask);
    cr_assert_gt(cond1, 0, "dds_create_readcondition(): returned %d", dds_err_nr(cond1));

    cond2 = dds_create_readcondition(g_reader, mask);
    cr_assert_gt(cond2, 0, "dds_create_readcondition(): returned %d", dds_err_nr(cond2));

    /* Also, we should be able to delete both. */
    ret = dds_delete(cond1);
    cr_assert_eq(ret, DDS_RETCODE_OK, "dds_delete(): returned %d", dds_err_nr(ret));

    /* And, of course, be able to delete the first one (return code isn't checked in the test fixtures). */
    ret = dds_delete(cond2);
    cr_assert_eq(ret, DDS_RETCODE_OK, "dds_delete(): returned %d", dds_err_nr(ret));
}
/*************************************************************************************************/

/*************************************************************************************************/
Test(ddsc_readcondition_create, deleted_reader, .init=readcondition_init, .fini=readcondition_fini)
{
    uint32_t mask = DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
    dds_entity_t cond;
    dds_delete(g_reader);
    cond = dds_create_readcondition(g_reader, mask);
    cr_assert_eq(dds_err_nr(cond), DDS_RETCODE_ALREADY_DELETED, "dds_create_readcondition(): returned %d", dds_err_nr(cond));
}
/*************************************************************************************************/

/*************************************************************************************************/
TheoryDataPoints(ddsc_readcondition_create, invalid_readers) = {
        DataPoints(dds_entity_t, -2, -1, 0, 1, 100, INT_MAX, INT_MIN),
};
Theory((dds_entity_t rdr), ddsc_readcondition_create, invalid_readers, .init=readcondition_init, .fini=readcondition_fini)
{
    uint32_t mask = DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
    dds_entity_t exp = DDS_RETCODE_BAD_PARAMETER * -1;
    dds_entity_t cond;

    cond = dds_create_readcondition(rdr, mask);
    cr_assert_eq(dds_err_nr(cond), dds_err_nr(exp), "returned %d != expected %d", dds_err_nr(cond), dds_err_nr(exp));
}
/*************************************************************************************************/

/*************************************************************************************************/
TheoryDataPoints(ddsc_readcondition_create, non_readers) = {
        DataPoints(dds_entity_t*, &g_topic, &g_participant),
};
Theory((dds_entity_t *rdr), ddsc_readcondition_create, non_readers, .init=readcondition_init, .fini=readcondition_fini)
{
    uint32_t mask = DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
    dds_entity_t cond;
    cond = dds_create_readcondition(*rdr, mask);
    cr_assert_eq(dds_err_nr(cond), DDS_RETCODE_ILLEGAL_OPERATION, "returned %d", dds_err_nr(cond));
}
/*************************************************************************************************/





/**************************************************************************************************
 *
 * These will check the readcondition mask acquiring in various ways.
 *
 *************************************************************************************************/
/*************************************************************************************************/
Test(ddsc_readcondition_get_mask, deleted, .init=readcondition_init, .fini=readcondition_fini)
{
    uint32_t mask = DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
    dds_entity_t condition;
    dds_return_t ret;
    condition = dds_create_readcondition(g_reader, mask);
    cr_assert_gt(condition, 0, "Failed to create prerequisite condition");
    dds_delete(condition);
    mask = 0;
    ret = dds_get_mask(condition, &mask);
    cr_assert_eq(dds_err_nr(ret), DDS_RETCODE_ALREADY_DELETED, "returned %d", dds_err_nr(ret));
}
/*************************************************************************************************/

/*************************************************************************************************/
Test(ddsc_readcondition_get_mask, null, .init=readcondition_init, .fini=readcondition_fini)
{
    uint32_t mask = DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
    dds_entity_t condition;
    dds_return_t ret;
    condition = dds_create_readcondition(g_reader, mask);
    cr_assert_gt(condition, 0, "Failed to create prerequisite condition");
    OS_WARNING_MSVC_OFF(6387); /* Disable SAL warning on intentional misuse of the API */
    ret = dds_get_mask(condition, NULL);
    OS_WARNING_MSVC_ON(6387);
    cr_assert_eq(dds_err_nr(ret), DDS_RETCODE_BAD_PARAMETER, "returned %d", dds_err_nr(ret));
    dds_delete(condition);
}
/*************************************************************************************************/

/*************************************************************************************************/
TheoryDataPoints(ddsc_readcondition_get_mask, invalid_conditions) = {
        DataPoints(dds_entity_t, -2, -1, 0, 1, 100, INT_MAX, INT_MIN),
};
Theory((dds_entity_t cond), ddsc_readcondition_get_mask, invalid_conditions, .init=readcondition_init, .fini=readcondition_fini)
{
    dds_entity_t exp = DDS_RETCODE_BAD_PARAMETER * -1;
    dds_return_t ret;
    uint32_t mask;

    ret = dds_get_mask(cond, &mask);
    cr_assert_eq(dds_err_nr(ret), dds_err_nr(exp), "returned %d != expected %d", dds_err_nr(ret), dds_err_nr(exp));
}
/*************************************************************************************************/

/*************************************************************************************************/
TheoryDataPoints(ddsc_readcondition_get_mask, non_conditions) = {
        DataPoints(dds_entity_t*, &g_reader, &g_topic, &g_participant),
};
Theory((dds_entity_t *cond), ddsc_readcondition_get_mask, non_conditions, .init=readcondition_init, .fini=readcondition_fini)
{
    dds_return_t ret;
    uint32_t mask;
    ret = dds_get_mask(*cond, &mask);
    cr_assert_eq(dds_err_nr(ret), DDS_RETCODE_ILLEGAL_OPERATION, "returned %d", dds_err_nr(ret));
}
/*************************************************************************************************/

/*************************************************************************************************/
TheoryDataPoints(ddsc_readcondition_get_mask, various_masks) = {
        DataPoints(uint32_t, DDS_ANY_SAMPLE_STATE,  DDS_READ_SAMPLE_STATE,     DDS_NOT_READ_SAMPLE_STATE),
        DataPoints(uint32_t, DDS_ANY_VIEW_STATE,     DDS_NEW_VIEW_STATE,       DDS_NOT_NEW_VIEW_STATE),
        DataPoints(uint32_t, DDS_ANY_INSTANCE_STATE, DDS_ALIVE_INSTANCE_STATE, DDS_NOT_ALIVE_DISPOSED_INSTANCE_STATE, DDS_NOT_ALIVE_NO_WRITERS_INSTANCE_STATE),
};
Theory((uint32_t ss, uint32_t vs, uint32_t is), ddsc_readcondition_get_mask, various_masks, .init=readcondition_init, .fini=readcondition_fini)
{
    uint32_t maskIn  = ss | vs | is;
    uint32_t maskOut = 0xFFFFFFFF;
    dds_entity_t condition;
    dds_return_t ret;

    condition = dds_create_readcondition(g_reader, maskIn);
    cr_assert_gt(condition, 0, "Failed to create prerequisite condition");

    ret = dds_get_mask(condition, &maskOut);
    cr_assert_eq(dds_err_nr(ret), DDS_RETCODE_OK, "returned %d", dds_err_nr(ret));
    cr_assert_eq(maskIn, maskOut);

    dds_delete(condition);
}
/*************************************************************************************************/




/**************************************************************************************************
 *
 * These will check the readcondition reading in various ways.
 *
 *************************************************************************************************/
/*************************************************************************************************/
Test(ddsc_readcondition_read, any, .init=readcondition_init, .fini=readcondition_fini)
{
    dds_entity_t condition;
    dds_return_t ret;

    /* Create condition. */
    condition = dds_create_readcondition(g_reader, DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE);
    cr_assert_gt(condition, 0, "Failed to create prerequisite condition");

    /* Read all samples. */
    ret = dds_read(condition, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
    cr_assert_eq(ret, MAX_SAMPLES, "# read %d, expected %d", ret, MAX_SAMPLES);
    for(int i = 0; i < ret; i++) {
        Space_Type1 *sample = (Space_Type1*)g_samples[i];

        /*
         * | long_1 | long_2 | long_3 |    sst   | vst |    ist     |
         * ----------------------------------------------------------
         * |    0   |    0   |    0   |     read | old | alive      | <---
         * |    1   |    0   |    0   |     read | old | disposed   | <---
         * |    2   |    1   |    0   |     read | old | no_writers | <---
         * |    3   |    1   |    1   | not_read | old | alive      | <---
         * |    4   |    2   |    1   | not_read | new | disposed   | <---
         * |    5   |    2   |    1   | not_read | new | no_writers | <---
         * |    6   |    3   |    2   | not_read | new | alive      | <---
         */
        PRINT_SAMPLE("ddsc_readcondition_read::any: Read", (*sample));

        /* Expected states. */
        int                  expected_long_1 = i;
        dds_sample_state_t   expected_sst    = SAMPLE_SST(expected_long_1);
        dds_view_state_t     expected_vst    = SAMPLE_VST(expected_long_1);
        dds_instance_state_t expected_ist    = SAMPLE_IST(expected_long_1);

        /* Check data. */
        cr_assert_eq(sample->long_1, expected_long_1  );
        cr_assert_eq(sample->long_2, expected_long_1/2);
        cr_assert_eq(sample->long_3, expected_long_1/3);

        /* Check states. */
        cr_assert_eq(g_info[i].valid_data,     true);
        cr_assert_eq(g_info[i].sample_state,   expected_sst);
        cr_assert_eq(g_info[i].view_state,     expected_vst);
        cr_assert_eq(g_info[i].instance_state, expected_ist);
    }

    dds_delete(condition);
}
/*************************************************************************************************/

/*************************************************************************************************/
Test(ddsc_readcondition_read, not_read_sample_state, .init=readcondition_init, .fini=readcondition_fini)
{
    dds_entity_t condition;
    dds_return_t ret;

    /* Create condition. */
    condition = dds_create_readcondition(g_reader, DDS_NOT_READ_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE);
    cr_assert_gt(condition, 0, "Failed to create prerequisite condition");

    /* Read all non-read samples (should be last part). */
    ret = dds_read(condition, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
    cr_assert_eq(ret, MAX_SAMPLES - (SAMPLE_LAST_READ_SST + 1), "# read %d, expected %d", ret, MAX_SAMPLES - (SAMPLE_LAST_READ_SST + 1));
    for(int i = 0; i < ret; i++) {
        Space_Type1 *sample = (Space_Type1*)g_samples[i];

        /*
         * | long_1 | long_2 | long_3 |    sst   | vst |    ist     |
         * ----------------------------------------------------------
         * |    0   |    0   |    0   |     read | old | alive      |
         * |    1   |    0   |    0   |     read | old | disposed   |
         * |    2   |    1   |    0   |     read | old | no_writers |
         * |    3   |    1   |    1   | not_read | old | alive      | <---
         * |    4   |    2   |    1   | not_read | new | disposed   | <---
         * |    5   |    2   |    1   | not_read | new | no_writers | <---
         * |    6   |    3   |    2   | not_read | new | alive      | <---
         */
        PRINT_SAMPLE("ddsc_readcondition_read::not_read_sample_state: Read", (*sample));

        /* Expected states. */
        int                  expected_long_1 = SAMPLE_LAST_READ_SST + 1 + i;
        dds_sample_state_t   expected_sst    = DDS_SST_NOT_READ;
        dds_view_state_t     expected_vst    = SAMPLE_VST(expected_long_1);
        dds_instance_state_t expected_ist    = SAMPLE_IST(expected_long_1);

        /* Check data. */
        cr_assert_eq(sample->long_1, expected_long_1  );
        cr_assert_eq(sample->long_2, expected_long_1/2);
        cr_assert_eq(sample->long_3, expected_long_1/3);

        /* Check states. */
        cr_assert_eq(g_info[i].valid_data,     true);
        cr_assert_eq(g_info[i].sample_state,   expected_sst);
        cr_assert_eq(g_info[i].view_state,     expected_vst);
        cr_assert_eq(g_info[i].instance_state, expected_ist);
    }

    dds_delete(condition);
}
/*************************************************************************************************/

/*************************************************************************************************/
Test(ddsc_readcondition_read, read_sample_state, .init=readcondition_init, .fini=readcondition_fini)
{
    dds_entity_t condition;
    dds_return_t ret;

    /* Create condition. */
    condition = dds_create_readcondition(g_reader, DDS_READ_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE);
    cr_assert_gt(condition, 0, "Failed to create prerequisite condition");

    /* Read all already read samples (should be first part). */
    ret = dds_read(condition, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
    cr_assert_eq(ret, SAMPLE_LAST_READ_SST + 1, "# read %d, expected %d", ret, SAMPLE_LAST_READ_SST + 1);
    for(int i = 0; i < ret; i++) {
        Space_Type1 *sample = (Space_Type1*)g_samples[i];

        /*
         * | long_1 | long_2 | long_3 |    sst   | vst |    ist     |
         * ----------------------------------------------------------
         * |    0   |    0   |    0   |     read | old | alive      | <---
         * |    1   |    0   |    0   |     read | old | disposed   | <---
         * |    2   |    1   |    0   |     read | old | no_writers | <---
         * |    3   |    1   |    1   | not_read | old | alive      |
         * |    4   |    2   |    1   | not_read | new | disposed   |
         * |    5   |    2   |    1   | not_read | new | no_writers |
         * |    6   |    3   |    2   | not_read | new | alive      |
         */
        PRINT_SAMPLE("ddsc_readcondition_read::read_sample_state: Read", (*sample));

        /* Expected states. */
        int                  expected_long_1 = i;
        dds_sample_state_t   expected_sst    = DDS_SST_READ;
        dds_view_state_t     expected_vst    = SAMPLE_VST(expected_long_1);
        dds_instance_state_t expected_ist    = SAMPLE_IST(expected_long_1);

        /* Check data. */
        cr_assert_eq(sample->long_1, expected_long_1  );
        cr_assert_eq(sample->long_2, expected_long_1/2);
        cr_assert_eq(sample->long_3, expected_long_1/3);

        /* Check states. */
        cr_assert_eq(g_info[i].valid_data,     true);
        cr_assert_eq(g_info[i].sample_state,   expected_sst);
        cr_assert_eq(g_info[i].view_state,     expected_vst);
        cr_assert_eq(g_info[i].instance_state, expected_ist);
    }

    dds_delete(condition);
}
/*************************************************************************************************/

/*************************************************************************************************/
Test(ddsc_readcondition_read, new_view_state, .init=readcondition_init, .fini=readcondition_fini)
{
    dds_entity_t condition;
    dds_return_t ret;

    /* Create condition. */
    condition = dds_create_readcondition(g_reader, DDS_ANY_SAMPLE_STATE | DDS_NEW_VIEW_STATE | DDS_ANY_INSTANCE_STATE);
    cr_assert_gt(condition, 0, "Failed to create prerequisite condition");

    /* Read all new-view samples (should be last part). */
    ret = dds_read(condition, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
    cr_assert_eq(ret, MAX_SAMPLES - (SAMPLE_LAST_OLD_VST + 1), "# read %d, expected %d", ret, MAX_SAMPLES - (SAMPLE_LAST_OLD_VST + 1));
    for(int i = 0; i < ret; i++) {
        Space_Type1 *sample = (Space_Type1*)g_samples[i];

        /*
         * | long_1 | long_2 | long_3 |    sst   | vst |    ist     |
         * ----------------------------------------------------------
         * |    0   |    0   |    0   |     read | old | alive      |
         * |    1   |    0   |    0   |     read | old | disposed   |
         * |    2   |    1   |    0   |     read | old | no_writers |
         * |    3   |    1   |    1   | not_read | old | alive      |
         * |    4   |    2   |    1   | not_read | new | disposed   | <---
         * |    5   |    2   |    1   | not_read | new | no_writers | <---
         * |    6   |    3   |    2   | not_read | new | alive      | <---
         */
        PRINT_SAMPLE("ddsc_readcondition_read::new_view_state: Read", (*sample));

        /* Expected states. */
        int                  expected_long_1 = SAMPLE_LAST_OLD_VST + 1 + i;
        dds_sample_state_t   expected_sst    = SAMPLE_SST(expected_long_1);
        dds_view_state_t     expected_vst    = DDS_VST_NEW;
        dds_instance_state_t expected_ist    = SAMPLE_IST(expected_long_1);

        /* Check data. */
        cr_assert_eq(sample->long_1, expected_long_1  );
        cr_assert_eq(sample->long_2, expected_long_1/2);
        cr_assert_eq(sample->long_3, expected_long_1/3);

        /* Check states. */
        cr_assert_eq(g_info[i].valid_data,     true);
        cr_assert_eq(g_info[i].sample_state,   expected_sst);
        cr_assert_eq(g_info[i].view_state,     expected_vst);
        cr_assert_eq(g_info[i].instance_state, expected_ist);
    }

    dds_delete(condition);
}
/*************************************************************************************************/

/*************************************************************************************************/
Test(ddsc_readcondition_read, not_new_view_state, .init=readcondition_init, .fini=readcondition_fini)
{
    dds_entity_t condition;
    dds_return_t ret;

    /* Create condition. */
    condition = dds_create_readcondition(g_reader, DDS_ANY_SAMPLE_STATE | DDS_NOT_NEW_VIEW_STATE | DDS_ANY_INSTANCE_STATE);
    cr_assert_gt(condition, 0, "Failed to create prerequisite condition");

    /* Read all old-view samples (should be first part). */
    ret = dds_read(condition, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
    cr_assert_eq(ret, SAMPLE_LAST_OLD_VST + 1, "# read %d, expected %d", ret, SAMPLE_LAST_OLD_VST + 1);
    for(int i = 0; i < ret; i++) {
        Space_Type1 *sample = (Space_Type1*)g_samples[i];

        /*
         * | long_1 | long_2 | long_3 |    sst   | vst |    ist     |
         * ----------------------------------------------------------
         * |    0   |    0   |    0   |     read | old | alive      | <---
         * |    1   |    0   |    0   |     read | old | disposed   | <---
         * |    2   |    1   |    0   |     read | old | no_writers | <---
         * |    3   |    1   |    1   | not_read | old | alive      | <---
         * |    4   |    2   |    1   | not_read | new | disposed   |
         * |    5   |    2   |    1   | not_read | new | no_writers |
         * |    6   |    3   |    2   | not_read | new | alive      |
         */
        PRINT_SAMPLE("ddsc_readcondition_read::not_new_view_state: Read", (*sample));

        /* Expected states. */
        int                  expected_long_1 = i;
        dds_sample_state_t   expected_sst    = SAMPLE_SST(expected_long_1);
        dds_view_state_t     expected_vst    = DDS_VST_OLD;
        dds_instance_state_t expected_ist    = SAMPLE_IST(expected_long_1);

        /* Check data. */
        cr_assert_eq(sample->long_1, expected_long_1  );
        cr_assert_eq(sample->long_2, expected_long_1/2);
        cr_assert_eq(sample->long_3, expected_long_1/3);

        /* Check states. */
        cr_assert_eq(g_info[i].valid_data,     true);
        cr_assert_eq(g_info[i].sample_state,   expected_sst);
        cr_assert_eq(g_info[i].view_state,     expected_vst);
        cr_assert_eq(g_info[i].instance_state, expected_ist);
    }

    dds_delete(condition);
}
/*************************************************************************************************/

/*************************************************************************************************/
Test(ddsc_readcondition_read, alive_instance_state, .init=readcondition_init, .fini=readcondition_fini)
{
    dds_entity_t condition;
    dds_return_t ret;

    /* Create condition. */
    condition = dds_create_readcondition(g_reader, DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ALIVE_INSTANCE_STATE);
    cr_assert_gt(condition, 0, "Failed to create prerequisite condition");

    /* Read all alive samples. */
    ret = dds_read(condition, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
    cr_assert_eq(ret, SAMPLE_ALIVE_IST_CNT, "# read %d, expected %d", ret, SAMPLE_ALIVE_IST_CNT);
    for(int i = 0; i < ret; i++) {
        Space_Type1 *sample = (Space_Type1*)g_samples[i];

        /*
         * | long_1 | long_2 | long_3 |    sst   | vst |    ist     |
         * ----------------------------------------------------------
         * |    0   |    0   |    0   |     read | old | alive      | <---
         * |    1   |    0   |    0   |     read | old | disposed   |
         * |    2   |    1   |    0   |     read | old | no_writers |
         * |    3   |    1   |    1   | not_read | old | alive      | <---
         * |    4   |    2   |    1   | not_read | new | disposed   |
         * |    5   |    2   |    1   | not_read | new | no_writers |
         * |    6   |    3   |    2   | not_read | new | alive      | <---
         */
        PRINT_SAMPLE("ddsc_readcondition_read::alive_instance_state: Read", (*sample));

        /* Expected states. */
        int                  expected_long_1 = i * 3;
        dds_sample_state_t   expected_sst    = SAMPLE_SST(expected_long_1);
        dds_view_state_t     expected_vst    = SAMPLE_VST(expected_long_1);
        dds_instance_state_t expected_ist    = DDS_IST_ALIVE;

        /* Check data. */
        cr_assert_eq(sample->long_1, expected_long_1  );
        cr_assert_eq(sample->long_2, expected_long_1/2);
        cr_assert_eq(sample->long_3, expected_long_1/3);

        /* Check states. */
        cr_assert_eq(g_info[i].valid_data,     true);
        cr_assert_eq(g_info[i].sample_state,   expected_sst);
        cr_assert_eq(g_info[i].view_state,     expected_vst);
        cr_assert_eq(g_info[i].instance_state, expected_ist);
    }

    dds_delete(condition);
}
/*************************************************************************************************/

/*************************************************************************************************/
Test(ddsc_readcondition_read, disposed_instance_state, .init=readcondition_init, .fini=readcondition_fini)
{
    dds_entity_t condition;
    dds_return_t ret;

    /* Create condition. */
    condition = dds_create_readcondition(g_reader, DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_NOT_ALIVE_DISPOSED_INSTANCE_STATE);
    cr_assert_gt(condition, 0, "Failed to create prerequisite condition");

    /* Read all disposed samples. */
    ret = dds_read(condition, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
    cr_assert_eq(ret, SAMPLE_DISPOSED_IST_CNT, "# read %d, expected %d", ret, SAMPLE_DISPOSED_IST_CNT);
    for(int i = 0; i < ret; i++) {
        Space_Type1 *sample = (Space_Type1*)g_samples[i];

        /*
         * | long_1 | long_2 | long_3 |    sst   | vst |    ist     |
         * ----------------------------------------------------------
         * |    0   |    0   |    0   |     read | old | alive      |
         * |    1   |    0   |    0   |     read | old | disposed   | <---
         * |    2   |    1   |    0   |     read | old | no_writers |
         * |    3   |    1   |    1   | not_read | old | alive      |
         * |    4   |    2   |    1   | not_read | new | disposed   | <---
         * |    5   |    2   |    1   | not_read | new | no_writers |
         * |    6   |    3   |    2   | not_read | new | alive      |
         */
        PRINT_SAMPLE("ddsc_readcondition_read::disposed_instance_state: Read", (*sample));

        /* Expected states. */
        int                  expected_long_1 = (i * 3) + 1;
        dds_sample_state_t   expected_sst    = SAMPLE_SST(expected_long_1);
        dds_view_state_t     expected_vst    = SAMPLE_VST(expected_long_1);
        dds_instance_state_t expected_ist    = DDS_IST_NOT_ALIVE_DISPOSED;

        /* Check data. */
        cr_assert_eq(sample->long_1, expected_long_1  );
        cr_assert_eq(sample->long_2, expected_long_1/2);
        cr_assert_eq(sample->long_3, expected_long_1/3);

        /* Check states. */
        cr_assert_eq(g_info[i].valid_data,     true);
        cr_assert_eq(g_info[i].sample_state,   expected_sst);
        cr_assert_eq(g_info[i].view_state,     expected_vst);
        cr_assert_eq(g_info[i].instance_state, expected_ist);
    }

    dds_delete(condition);
}
/*************************************************************************************************/

/*************************************************************************************************/
Test(ddsc_readcondition_read, no_writers_instance_state, .init=readcondition_init, .fini=readcondition_fini)
{
    dds_entity_t condition;
    dds_return_t ret;

    /* Create condition. */
    condition = dds_create_readcondition(g_reader, DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_NOT_ALIVE_NO_WRITERS_INSTANCE_STATE);
    cr_assert_gt(condition, 0, "Failed to create prerequisite condition");

    /* Read all samples without a writer. */
    ret = dds_read(condition, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
    cr_assert_eq(ret, SAMPLE_NO_WRITER_IST_CNT, "# read %d, expected %d", ret, SAMPLE_NO_WRITER_IST_CNT);
    for(int i = 0; i < ret; i++) {
        Space_Type1 *sample = (Space_Type1*)g_samples[i];

        /*
         * | long_1 | long_2 | long_3 |    sst   | vst |    ist     |
         * ----------------------------------------------------------
         * |    0   |    0   |    0   |     read | old | alive      |
         * |    1   |    0   |    0   |     read | old | disposed   |
         * |    2   |    1   |    0   |     read | old | no_writers | <---
         * |    3   |    1   |    1   | not_read | old | alive      |
         * |    4   |    2   |    1   | not_read | new | disposed   |
         * |    5   |    2   |    1   | not_read | new | no_writers | <---
         * |    6   |    3   |    2   | not_read | new | alive      |
         */
        PRINT_SAMPLE("ddsc_readcondition_read::no_writers_instance_state: Read", (*sample));

        /* Expected states. */
        int                  expected_long_1 = (i * 3) + 2;
        dds_sample_state_t   expected_sst    = SAMPLE_SST(expected_long_1);
        dds_view_state_t     expected_vst    = SAMPLE_VST(expected_long_1);
        dds_instance_state_t expected_ist    = DDS_IST_NOT_ALIVE_NO_WRITERS;

        /* Check data. */
        cr_assert_eq(sample->long_1, expected_long_1  );
        cr_assert_eq(sample->long_2, expected_long_1/2);
        cr_assert_eq(sample->long_3, expected_long_1/3);

        /* Check states. */
        cr_assert_eq(g_info[i].valid_data,     true);
        cr_assert_eq(g_info[i].sample_state,   expected_sst);
        cr_assert_eq(g_info[i].view_state,     expected_vst);
        cr_assert_eq(g_info[i].instance_state, expected_ist);
    }

    dds_delete(condition);
}
/*************************************************************************************************/

/*************************************************************************************************/
Test(ddsc_readcondition_read, combination_of_states, .init=readcondition_init, .fini=readcondition_fini)
{
    dds_entity_t condition;
    dds_return_t ret;

    /* Create condition. */
    condition = dds_create_readcondition(g_reader, DDS_NOT_READ_SAMPLE_STATE | DDS_NOT_NEW_VIEW_STATE | DDS_ALIVE_INSTANCE_STATE);
    cr_assert_gt(condition, 0, "Failed to create prerequisite condition");

    /* Read all samples that match the mask (should be only one). */
    ret = dds_read(condition, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
    cr_assert_eq(ret, 1, "# read %d, expected %d", ret, 1);
    for(int i = 0; i < ret; i++) {
        Space_Type1 *sample = (Space_Type1*)g_samples[i];

        /*
         * | long_1 | long_2 | long_3 |    sst   | vst |    ist     |
         * ----------------------------------------------------------
         * |    0   |    0   |    0   |     read | old | alive      |
         * |    1   |    0   |    0   |     read | old | disposed   |
         * |    2   |    1   |    0   |     read | old | no_writers |
         * |    3   |    1   |    1   | not_read | old | alive      | <---
         * |    4   |    2   |    1   | not_read | new | disposed   |
         * |    5   |    2   |    1   | not_read | new | no_writers |
         * |    6   |    3   |    2   | not_read | new | alive      |
         */
        PRINT_SAMPLE("ddsc_readcondition_read::combination_of_states: Read", (*sample));

        /* Expected states. */
        int                  expected_long_1 = 3;
        dds_sample_state_t   expected_sst    = DDS_SST_NOT_READ;
        dds_view_state_t     expected_vst    = DDS_VST_OLD;
        dds_instance_state_t expected_ist    = DDS_IST_ALIVE;

        /* Check data. */
        cr_assert_eq(sample->long_1, expected_long_1  );
        cr_assert_eq(sample->long_2, expected_long_1/2);
        cr_assert_eq(sample->long_3, expected_long_1/3);

        /* Check states. */
        cr_assert_eq(g_info[i].valid_data,     true);
        cr_assert_eq(g_info[i].sample_state,   expected_sst);
        cr_assert_eq(g_info[i].view_state,     expected_vst);
        cr_assert_eq(g_info[i].instance_state, expected_ist);
    }

    dds_delete(condition);
}
/*************************************************************************************************/

/*************************************************************************************************/
Test(ddsc_readcondition_read, none, .init=readcondition_init, .fini=readcondition_fini)
{
    dds_entity_t condition;
    dds_return_t ret;

    /* Create condition. */
    condition = dds_create_readcondition(g_reader, DDS_READ_SAMPLE_STATE | DDS_NEW_VIEW_STATE | DDS_ANY_INSTANCE_STATE);
    cr_assert_gt(condition, 0, "Failed to create prerequisite condition");

    /* Read all samples that match the mask (should be none). */
    ret = dds_read(condition, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
    cr_assert_eq(ret, 0, "# read %d, expected %d", ret, 0);

    /*
     * | long_1 | long_2 | long_3 |    sst   | vst |    ist     |
     * ----------------------------------------------------------
     * |    0   |    0   |    0   |     read | old | alive      |
     * |    1   |    0   |    0   |     read | old | disposed   |
     * |    2   |    1   |    0   |     read | old | no_writers |
     * |    3   |    1   |    1   | not_read | old | alive      |
     * |    4   |    2   |    1   | not_read | new | disposed   |
     * |    5   |    2   |    1   | not_read | new | no_writers |
     * |    6   |    3   |    2   | not_read | new | alive      |
     */

    dds_delete(condition);
}
/*************************************************************************************************/

/*************************************************************************************************/
Test(ddsc_readcondition_read, with_mask, .init=readcondition_init, .fini=readcondition_fini)
{
    dds_entity_t condition;
    dds_return_t ret;

    /* Create condition. */
    condition = dds_create_readcondition(g_reader, DDS_NOT_READ_SAMPLE_STATE | DDS_NEW_VIEW_STATE | DDS_ALIVE_INSTANCE_STATE);
    cr_assert_gt(condition, 0, "Failed to create prerequisite condition");

    /* Read all samples that match the or'd masks. */
    ret = dds_read_mask(condition, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES,
                        DDS_NOT_READ_SAMPLE_STATE | DDS_NOT_NEW_VIEW_STATE | DDS_NOT_ALIVE_DISPOSED_INSTANCE_STATE);
    cr_assert_eq(ret, 3, "# read %d, expected %d", ret, 3);
    for(int i = 0; i < ret; i++) {
        Space_Type1 *sample = (Space_Type1*)g_samples[i];

        /*
         * | long_1 | long_2 | long_3 |    sst   | vst |    ist     |
         * ----------------------------------------------------------
         * |    0   |    0   |    0   |     read | old | alive      |
         * |    1   |    0   |    0   |     read | old | disposed   |
         * |    2   |    1   |    0   |     read | old | no_writers |
         * |    3   |    1   |    1   | not_read | old | alive      | <---
         * |    4   |    2   |    1   | not_read | new | disposed   | <---
         * |    5   |    2   |    1   | not_read | new | no_writers |
         * |    6   |    3   |    2   | not_read | new | alive      | <---
         */
        PRINT_SAMPLE("ddsc_readcondition_read::with_mask: Read", (*sample));

        /* Expected states. */
        int                  expected_long_1 = (i == 0) ? 3 : (i == 1) ? 4 : 6;
        dds_sample_state_t   expected_sst    = DDS_SST_NOT_READ;
        dds_view_state_t     expected_vst    = SAMPLE_VST(expected_long_1);
        dds_instance_state_t expected_ist    = SAMPLE_IST(expected_long_1);

        /* Check data. */
        cr_assert_eq(sample->long_1, expected_long_1  );
        cr_assert_eq(sample->long_2, expected_long_1/2);
        cr_assert_eq(sample->long_3, expected_long_1/3);

        /* Check states. */
        cr_assert_eq(g_info[i].valid_data,     true);
        cr_assert_eq(g_info[i].sample_state,   expected_sst);
        cr_assert_eq(g_info[i].view_state,     expected_vst);
        cr_assert_eq(g_info[i].instance_state, expected_ist);
    }

    dds_delete(condition);
}
/*************************************************************************************************/

/*************************************************************************************************/
Test(ddsc_readcondition_read, already_deleted, .init=readcondition_init, .fini=readcondition_fini)
{
    dds_entity_t condition;
    dds_return_t ret;

    /* Create condition. */
    condition = dds_create_readcondition(g_reader, DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE);
    cr_assert_gt(condition, 0, "Failed to create prerequisite condition");

    /* Delete condition. */
    ret = dds_delete(condition);
    cr_assert_eq(ret, DDS_RETCODE_OK, "Failed to delete prerequisite condition");

    /* Try to read with a deleted condition. */
    ret = dds_read(condition, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
    cr_expect_eq(dds_err_nr(ret), DDS_RETCODE_ALREADY_DELETED);
}
/*************************************************************************************************/








/**************************************************************************************************
 *
 * These will check the readcondition taking in various ways.
 *
 *************************************************************************************************/
/*************************************************************************************************/
Test(ddsc_readcondition_take, any, .init=readcondition_init, .fini=readcondition_fini)
{
    dds_entity_t condition;
    dds_return_t ret;

    /* Create condition. */
    condition = dds_create_readcondition(g_reader, DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE);
    cr_assert_gt(condition, 0, "Failed to create prerequisite condition");

    /* Take all non-read samples (should be last part). */
    ret = dds_take(condition, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
    cr_assert_eq(ret, MAX_SAMPLES, "# read %d, expected %d", ret, MAX_SAMPLES);
    for(int i = 0; i < ret; i++) {
        Space_Type1 *sample = (Space_Type1*)g_samples[i];

        /*
         * | long_1 | long_2 | long_3 |    sst   | vst |    ist     |
         * ----------------------------------------------------------
         * |    0   |    0   |    0   |     read | old | alive      | <---
         * |    1   |    0   |    0   |     read | old | disposed   | <---
         * |    2   |    1   |    0   |     read | old | no_writers | <---
         * |    3   |    1   |    1   | not_read | old | alive      | <---
         * |    4   |    2   |    1   | not_read | new | disposed   | <---
         * |    5   |    2   |    1   | not_read | new | no_writers | <---
         * |    6   |    3   |    2   | not_read | new | alive      | <---
         */
        PRINT_SAMPLE("ddsc_readcondition_take::any: Take", (*sample));

        /* Expected states. */
        int                  expected_long_1 = i;
        dds_sample_state_t   expected_sst    = SAMPLE_SST(expected_long_1);
        dds_view_state_t     expected_vst    = SAMPLE_VST(expected_long_1);
        dds_instance_state_t expected_ist    = SAMPLE_IST(expected_long_1);

        /* Check data. */
        cr_assert_eq(sample->long_1, expected_long_1  );
        cr_assert_eq(sample->long_2, expected_long_1/2);
        cr_assert_eq(sample->long_3, expected_long_1/3);

        /* Check states. */
        cr_assert_eq(g_info[i].valid_data,     true);
        cr_assert_eq(g_info[i].sample_state,   expected_sst);
        cr_assert_eq(g_info[i].view_state,     expected_vst);
        cr_assert_eq(g_info[i].instance_state, expected_ist);
    }

    dds_delete(condition);
}
/*************************************************************************************************/

/*************************************************************************************************/
Test(ddsc_readcondition_take, not_read_sample_state, .init=readcondition_init, .fini=readcondition_fini)
{
    dds_entity_t condition;
    dds_return_t ret;

    /* Create condition. */
    condition = dds_create_readcondition(g_reader, DDS_NOT_READ_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE);
    cr_assert_gt(condition, 0, "Failed to create prerequisite condition");

    /* Take all non-read samples (should be last part). */
    ret = dds_take(condition, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
    cr_assert_eq(ret, MAX_SAMPLES - (SAMPLE_LAST_READ_SST + 1), "# read %d, expected %d", ret, MAX_SAMPLES - (SAMPLE_LAST_READ_SST + 1));
    for(int i = 0; i < ret; i++) {
        Space_Type1 *sample = (Space_Type1*)g_samples[i];

        /*
         * | long_1 | long_2 | long_3 |    sst   | vst |    ist     |
         * ----------------------------------------------------------
         * |    0   |    0   |    0   |     read | old | alive      |
         * |    1   |    0   |    0   |     read | old | disposed   |
         * |    2   |    1   |    0   |     read | old | no_writers |
         * |    3   |    1   |    1   | not_read | old | alive      | <---
         * |    4   |    2   |    1   | not_read | new | disposed   | <---
         * |    5   |    2   |    1   | not_read | new | no_writers | <---
         * |    6   |    3   |    2   | not_read | new | alive      | <---
         */
        PRINT_SAMPLE("ddsc_readcondition_take::not_read_sample_state: Take", (*sample));

        /* Expected states. */
        int                  expected_long_1 = SAMPLE_LAST_READ_SST + 1 + i;
        dds_sample_state_t   expected_sst    = DDS_SST_NOT_READ;
        dds_view_state_t     expected_vst    = SAMPLE_VST(expected_long_1);
        dds_instance_state_t expected_ist    = SAMPLE_IST(expected_long_1);

        /* Check data. */
        cr_assert_eq(sample->long_1, expected_long_1  );
        cr_assert_eq(sample->long_2, expected_long_1/2);
        cr_assert_eq(sample->long_3, expected_long_1/3);

        /* Check states. */
        cr_assert_eq(g_info[i].valid_data,     true);
        cr_assert_eq(g_info[i].sample_state,   expected_sst);
        cr_assert_eq(g_info[i].view_state,     expected_vst);
        cr_assert_eq(g_info[i].instance_state, expected_ist);
    }

    dds_delete(condition);
}
/*************************************************************************************************/

/*************************************************************************************************/
Test(ddsc_readcondition_take, read_sample_state, .init=readcondition_init, .fini=readcondition_fini)
{
    dds_entity_t condition;
    dds_return_t ret;

    /* Create condition. */
    condition = dds_create_readcondition(g_reader, DDS_READ_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE);
    cr_assert_gt(condition, 0, "Failed to create prerequisite condition");

    /* Take all already read samples (should be first part). */
    ret = dds_take(condition, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
    cr_assert_eq(ret, SAMPLE_LAST_READ_SST + 1, "# read %d, expected %d", ret, SAMPLE_LAST_READ_SST + 1);
    for(int i = 0; i < ret; i++) {
        Space_Type1 *sample = (Space_Type1*)g_samples[i];

        /*
         * | long_1 | long_2 | long_3 |    sst   | vst |    ist     |
         * ----------------------------------------------------------
         * |    0   |    0   |    0   |     read | old | alive      | <---
         * |    1   |    0   |    0   |     read | old | disposed   | <---
         * |    2   |    1   |    0   |     read | old | no_writers | <---
         * |    3   |    1   |    1   | not_read | old | alive      |
         * |    4   |    2   |    1   | not_read | new | disposed   |
         * |    5   |    2   |    1   | not_read | new | no_writers |
         * |    6   |    3   |    2   | not_read | new | alive      |
         */
        PRINT_SAMPLE("ddsc_readcondition_take::read_sample_state: Take", (*sample));

        /* Expected states. */
        int                  expected_long_1 = i;
        dds_sample_state_t   expected_sst    = DDS_SST_READ;
        dds_view_state_t     expected_vst    = SAMPLE_VST(expected_long_1);
        dds_instance_state_t expected_ist    = SAMPLE_IST(expected_long_1);

        /* Check data. */
        cr_assert_eq(sample->long_1, expected_long_1  );
        cr_assert_eq(sample->long_2, expected_long_1/2);
        cr_assert_eq(sample->long_3, expected_long_1/3);

        /* Check states. */
        cr_assert_eq(g_info[i].valid_data,     true);
        cr_assert_eq(g_info[i].sample_state,   expected_sst);
        cr_assert_eq(g_info[i].view_state,     expected_vst);
        cr_assert_eq(g_info[i].instance_state, expected_ist);
    }

    dds_delete(condition);
}
/*************************************************************************************************/

/*************************************************************************************************/
Test(ddsc_readcondition_take, new_view_state, .init=readcondition_init, .fini=readcondition_fini)
{
    dds_entity_t condition;
    dds_return_t ret;

    /* Create condition. */
    condition = dds_create_readcondition(g_reader, DDS_ANY_SAMPLE_STATE | DDS_NEW_VIEW_STATE | DDS_ANY_INSTANCE_STATE);
    cr_assert_gt(condition, 0, "Failed to create prerequisite condition");

    /* Take all new-view samples (should be last part). */
    ret = dds_take(condition, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
    cr_assert_eq(ret, MAX_SAMPLES - (SAMPLE_LAST_OLD_VST + 1), "# read %d, expected %d", ret, MAX_SAMPLES - (SAMPLE_LAST_OLD_VST + 1));
    for(int i = 0; i < ret; i++) {
        Space_Type1 *sample = (Space_Type1*)g_samples[i];

        /*
         * | long_1 | long_2 | long_3 |    sst   | vst |    ist     |
         * ----------------------------------------------------------
         * |    0   |    0   |    0   |     read | old | alive      |
         * |    1   |    0   |    0   |     read | old | disposed   |
         * |    2   |    1   |    0   |     read | old | no_writers |
         * |    3   |    1   |    1   | not_read | old | alive      |
         * |    4   |    2   |    1   | not_read | new | disposed   | <---
         * |    5   |    2   |    1   | not_read | new | no_writers | <---
         * |    6   |    3   |    2   | not_read | new | alive      | <---
         */
        PRINT_SAMPLE("ddsc_readcondition_take::new_view_state: Take", (*sample));

        /* Expected states. */
        int                  expected_long_1 = SAMPLE_LAST_OLD_VST + 1 + i;
        dds_sample_state_t   expected_sst    = SAMPLE_SST(expected_long_1);
        dds_view_state_t     expected_vst    = DDS_VST_NEW;
        dds_instance_state_t expected_ist    = SAMPLE_IST(expected_long_1);

        /* Check data. */
        cr_assert_eq(sample->long_1, expected_long_1  );
        cr_assert_eq(sample->long_2, expected_long_1/2);
        cr_assert_eq(sample->long_3, expected_long_1/3);

        /* Check states. */
        cr_assert_eq(g_info[i].valid_data,     true);
        cr_assert_eq(g_info[i].sample_state,   expected_sst);
        cr_assert_eq(g_info[i].view_state,     expected_vst);
        cr_assert_eq(g_info[i].instance_state, expected_ist);
    }

    dds_delete(condition);
}
/*************************************************************************************************/

/*************************************************************************************************/
Test(ddsc_readcondition_take, not_new_view_state, .init=readcondition_init, .fini=readcondition_fini)
{
    dds_entity_t condition;
    dds_return_t ret;

    /* Create condition. */
    condition = dds_create_readcondition(g_reader, DDS_ANY_SAMPLE_STATE | DDS_NOT_NEW_VIEW_STATE | DDS_ANY_INSTANCE_STATE);
    cr_assert_gt(condition, 0, "Failed to create prerequisite condition");

    /* Take all old-view samples (should be first part). */
    ret = dds_take(condition, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
    cr_assert_eq(ret, SAMPLE_LAST_OLD_VST + 1, "# read %d, expected %d", ret, SAMPLE_LAST_OLD_VST + 1);
    for(int i = 0; i < ret; i++) {
        Space_Type1 *sample = (Space_Type1*)g_samples[i];

        /*
         * | long_1 | long_2 | long_3 |    sst   | vst |    ist     |
         * ----------------------------------------------------------
         * |    0   |    0   |    0   |     read | old | alive      | <---
         * |    1   |    0   |    0   |     read | old | disposed   | <---
         * |    2   |    1   |    0   |     read | old | no_writers | <---
         * |    3   |    1   |    1   | not_read | old | alive      | <---
         * |    4   |    2   |    1   | not_read | new | disposed   |
         * |    5   |    2   |    1   | not_read | new | no_writers |
         * |    6   |    3   |    2   | not_read | new | alive      |
         */
        PRINT_SAMPLE("ddsc_readcondition_take::not_new_view_state: Take", (*sample));

        /* Expected states. */
        int                  expected_long_1 = i;
        dds_sample_state_t   expected_sst    = SAMPLE_SST(expected_long_1);
        dds_view_state_t     expected_vst    = DDS_VST_OLD;
        dds_instance_state_t expected_ist    = SAMPLE_IST(expected_long_1);

        /* Check data. */
        cr_assert_eq(sample->long_1, expected_long_1  );
        cr_assert_eq(sample->long_2, expected_long_1/2);
        cr_assert_eq(sample->long_3, expected_long_1/3);

        /* Check states. */
        cr_assert_eq(g_info[i].valid_data,     true);
        cr_assert_eq(g_info[i].sample_state,   expected_sst);
        cr_assert_eq(g_info[i].view_state,     expected_vst);
        cr_assert_eq(g_info[i].instance_state, expected_ist);
    }

    dds_delete(condition);
}
/*************************************************************************************************/

/*************************************************************************************************/
Test(ddsc_readcondition_take, alive_instance_state, .init=readcondition_init, .fini=readcondition_fini)
{
    dds_entity_t condition;
    dds_return_t ret;

    /* Create condition. */
    condition = dds_create_readcondition(g_reader, DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ALIVE_INSTANCE_STATE);
    cr_assert_gt(condition, 0, "Failed to create prerequisite condition");

    /* Take all alive samples. */
    ret = dds_take(condition, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
    cr_assert_eq(ret, SAMPLE_ALIVE_IST_CNT, "# read %d, expected %d", ret, SAMPLE_ALIVE_IST_CNT);
    for(int i = 0; i < ret; i++) {
        Space_Type1 *sample = (Space_Type1*)g_samples[i];

        /*
         * | long_1 | long_2 | long_3 |    sst   | vst |    ist     |
         * ----------------------------------------------------------
         * |    0   |    0   |    0   |     read | old | alive      | <---
         * |    1   |    0   |    0   |     read | old | disposed   |
         * |    2   |    1   |    0   |     read | old | no_writers |
         * |    3   |    1   |    1   | not_read | old | alive      | <---
         * |    4   |    2   |    1   | not_read | new | disposed   |
         * |    5   |    2   |    1   | not_read | new | no_writers |
         * |    6   |    3   |    2   | not_read | new | alive      | <---
         */
        PRINT_SAMPLE("ddsc_readcondition_take::alive_instance_state: Take", (*sample));

        /* Expected states. */
        int                  expected_long_1 = i * 3;
        dds_sample_state_t   expected_sst    = SAMPLE_SST(expected_long_1);
        dds_view_state_t     expected_vst    = SAMPLE_VST(expected_long_1);
        dds_instance_state_t expected_ist    = DDS_IST_ALIVE;

        /* Check data. */
        cr_assert_eq(sample->long_1, expected_long_1  );
        cr_assert_eq(sample->long_2, expected_long_1/2);
        cr_assert_eq(sample->long_3, expected_long_1/3);

        /* Check states. */
        cr_assert_eq(g_info[i].valid_data,     true);
        cr_assert_eq(g_info[i].sample_state,   expected_sst);
        cr_assert_eq(g_info[i].view_state,     expected_vst);
        cr_assert_eq(g_info[i].instance_state, expected_ist);
    }

    dds_delete(condition);
}
/*************************************************************************************************/

/*************************************************************************************************/
Test(ddsc_readcondition_take, disposed_instance_state, .init=readcondition_init, .fini=readcondition_fini)
{
    dds_entity_t condition;
    dds_return_t ret;

    /* Create condition. */
    condition = dds_create_readcondition(g_reader, DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_NOT_ALIVE_DISPOSED_INSTANCE_STATE);
    cr_assert_gt(condition, 0, "Failed to create prerequisite condition");

    /* Take all disposed samples. */
    ret = dds_take(condition, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
    cr_assert_eq(ret, SAMPLE_DISPOSED_IST_CNT, "# read %d, expected %d", ret, SAMPLE_DISPOSED_IST_CNT);
    for(int i = 0; i < ret; i++) {
        Space_Type1 *sample = (Space_Type1*)g_samples[i];

        /*
         * | long_1 | long_2 | long_3 |    sst   | vst |    ist     |
         * ----------------------------------------------------------
         * |    0   |    0   |    0   |     read | old | alive      |
         * |    1   |    0   |    0   |     read | old | disposed   | <---
         * |    2   |    1   |    0   |     read | old | no_writers |
         * |    3   |    1   |    1   | not_read | old | alive      |
         * |    4   |    2   |    1   | not_read | new | disposed   | <---
         * |    5   |    2   |    1   | not_read | new | no_writers |
         * |    6   |    3   |    2   | not_read | new | alive      |
         */
        PRINT_SAMPLE("ddsc_readcondition_take::disposed_instance_state: Take", (*sample));

        /* Expected states. */
        int                  expected_long_1 = (i * 3) + 1;
        dds_sample_state_t   expected_sst    = SAMPLE_SST(expected_long_1);
        dds_view_state_t     expected_vst    = SAMPLE_VST(expected_long_1);
        dds_instance_state_t expected_ist    = DDS_IST_NOT_ALIVE_DISPOSED;

        /* Check data. */
        cr_assert_eq(sample->long_1, expected_long_1  );
        cr_assert_eq(sample->long_2, expected_long_1/2);
        cr_assert_eq(sample->long_3, expected_long_1/3);

        /* Check states. */
        cr_assert_eq(g_info[i].valid_data,     true);
        cr_assert_eq(g_info[i].sample_state,   expected_sst);
        cr_assert_eq(g_info[i].view_state,     expected_vst);
        cr_assert_eq(g_info[i].instance_state, expected_ist);
    }

    dds_delete(condition);
}
/*************************************************************************************************/

/*************************************************************************************************/
Test(ddsc_readcondition_take, no_writers_instance_state, .init=readcondition_init, .fini=readcondition_fini)
{
    dds_entity_t condition;
    dds_return_t ret;

    /* Create condition. */
    condition = dds_create_readcondition(g_reader, DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_NOT_ALIVE_NO_WRITERS_INSTANCE_STATE);
    cr_assert_gt(condition, 0, "Failed to create prerequisite condition");

    /* Take all samples without a writer. */
    ret = dds_take(condition, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
    cr_assert_eq(ret, SAMPLE_NO_WRITER_IST_CNT, "# read %d, expected %d", ret, SAMPLE_NO_WRITER_IST_CNT);
    for(int i = 0; i < ret; i++) {
        Space_Type1 *sample = (Space_Type1*)g_samples[i];

        /*
         * | long_1 | long_2 | long_3 |    sst   | vst |    ist     |
         * ----------------------------------------------------------
         * |    0   |    0   |    0   |     read | old | alive      |
         * |    1   |    0   |    0   |     read | old | disposed   |
         * |    2   |    1   |    0   |     read | old | no_writers | <---
         * |    3   |    1   |    1   | not_read | old | alive      |
         * |    4   |    2   |    1   | not_read | new | disposed   |
         * |    5   |    2   |    1   | not_read | new | no_writers | <---
         * |    6   |    3   |    2   | not_read | new | alive      |
         */
        PRINT_SAMPLE("ddsc_readcondition_take::no_writers_instance_state: Take", (*sample));

        /* Expected states. */
        int                  expected_long_1 = (i * 3) + 2;
        dds_sample_state_t   expected_sst    = SAMPLE_SST(expected_long_1);
        dds_view_state_t     expected_vst    = SAMPLE_VST(expected_long_1);
        dds_instance_state_t expected_ist    = DDS_IST_NOT_ALIVE_NO_WRITERS;

        /* Check data. */
        cr_assert_eq(sample->long_1, expected_long_1  );
        cr_assert_eq(sample->long_2, expected_long_1/2);
        cr_assert_eq(sample->long_3, expected_long_1/3);

        /* Check states. */
        cr_assert_eq(g_info[i].valid_data,     true);
        cr_assert_eq(g_info[i].sample_state,   expected_sst);
        cr_assert_eq(g_info[i].view_state,     expected_vst);
        cr_assert_eq(g_info[i].instance_state, expected_ist);
    }

    dds_delete(condition);
}
/*************************************************************************************************/

/*************************************************************************************************/
Test(ddsc_readcondition_take, combination_of_states, .init=readcondition_init, .fini=readcondition_fini)
{
    dds_entity_t condition;
    dds_return_t ret;

    /* Create condition. */
    condition = dds_create_readcondition(g_reader, DDS_NOT_READ_SAMPLE_STATE | DDS_NOT_NEW_VIEW_STATE | DDS_ALIVE_INSTANCE_STATE);
    cr_assert_gt(condition, 0, "Failed to create prerequisite condition");

    /* Take all samples that match the mask (should be only one). */
    ret = dds_take(condition, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
    cr_assert_eq(ret, 1, "# read %d, expected %d", ret, 1);
    for(int i = 0; i < ret; i++) {
        Space_Type1 *sample = (Space_Type1*)g_samples[i];

        /*
         * | long_1 | long_2 | long_3 |    sst   | vst |    ist     |
         * ----------------------------------------------------------
         * |    0   |    0   |    0   |     read | old | alive      |
         * |    1   |    0   |    0   |     read | old | disposed   |
         * |    2   |    1   |    0   |     read | old | no_writers |
         * |    3   |    1   |    1   | not_read | old | alive      | <---
         * |    4   |    2   |    1   | not_read | new | disposed   |
         * |    5   |    2   |    1   | not_read | new | no_writers |
         * |    6   |    3   |    2   | not_read | new | alive      |
         */
        PRINT_SAMPLE("ddsc_readcondition_take::combination_of_states: Take", (*sample));

        /* Expected states. */
        int                  expected_long_1 = 3;
        dds_sample_state_t   expected_sst    = DDS_SST_NOT_READ;
        dds_view_state_t     expected_vst    = DDS_VST_OLD;
        dds_instance_state_t expected_ist    = DDS_IST_ALIVE;

        /* Check data. */
        cr_assert_eq(sample->long_1, expected_long_1  );
        cr_assert_eq(sample->long_2, expected_long_1/2);
        cr_assert_eq(sample->long_3, expected_long_1/3);

        /* Check states. */
        cr_assert_eq(g_info[i].valid_data,     true);
        cr_assert_eq(g_info[i].sample_state,   expected_sst);
        cr_assert_eq(g_info[i].view_state,     expected_vst);
        cr_assert_eq(g_info[i].instance_state, expected_ist);
    }

    dds_delete(condition);
}
/*************************************************************************************************/

/*************************************************************************************************/
Test(ddsc_readcondition_take, none, .init=readcondition_init, .fini=readcondition_fini)
{
    dds_entity_t condition;
    dds_return_t ret;

    /* Create condition. */
    condition = dds_create_readcondition(g_reader, DDS_READ_SAMPLE_STATE | DDS_NEW_VIEW_STATE | DDS_ANY_INSTANCE_STATE);
    cr_assert_gt(condition, 0, "Failed to create prerequisite condition");

    /* Take all samples that match the mask (should be none). */
    ret = dds_take(condition, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
    cr_assert_eq(ret, 0, "# read %d, expected %d", ret, 0);

    /*
     * | long_1 | long_2 | long_3 |    sst   | vst |    ist     |
     * ----------------------------------------------------------
     * |    0   |    0   |    0   |     read | old | alive      |
     * |    1   |    0   |    0   |     read | old | disposed   |
     * |    2   |    1   |    0   |     read | old | no_writers |
     * |    3   |    1   |    1   | not_read | old | alive      |
     * |    4   |    2   |    1   | not_read | new | disposed   |
     * |    5   |    2   |    1   | not_read | new | no_writers |
     * |    6   |    3   |    2   | not_read | new | alive      |
     */

    dds_delete(condition);
}
/*************************************************************************************************/

/*************************************************************************************************/
Test(ddsc_readcondition_take, with_mask, .init=readcondition_init, .fini=readcondition_fini)
{
    dds_entity_t condition;
    dds_return_t ret;

    /* Create condition. */
    condition = dds_create_readcondition(g_reader, DDS_NOT_READ_SAMPLE_STATE | DDS_NEW_VIEW_STATE | DDS_ALIVE_INSTANCE_STATE);
    cr_assert_gt(condition, 0, "Failed to create prerequisite condition");

    /* Take all samples that match the or'd masks. */
    ret = dds_take_mask(condition, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES,
                        DDS_NOT_READ_SAMPLE_STATE | DDS_NOT_NEW_VIEW_STATE | DDS_NOT_ALIVE_DISPOSED_INSTANCE_STATE);
    cr_assert_eq(ret, 3, "# read %d, expected %d", ret, 3);
    for(int i = 0; i < ret; i++) {
        Space_Type1 *sample = (Space_Type1*)g_samples[i];

        /*
         * | long_1 | long_2 | long_3 |    sst   | vst |    ist     |
         * ----------------------------------------------------------
         * |    0   |    0   |    0   |     read | old | alive      |
         * |    1   |    0   |    0   |     read | old | disposed   |
         * |    2   |    1   |    0   |     read | old | no_writers |
         * |    3   |    1   |    1   | not_read | old | alive      | <---
         * |    4   |    2   |    1   | not_read | new | disposed   | <---
         * |    5   |    2   |    1   | not_read | new | no_writers |
         * |    6   |    3   |    2   | not_read | new | alive      | <---
         */
        PRINT_SAMPLE("ddsc_readcondition_take::with_mask: Take", (*sample));

        /* Expected states. */
        int                  expected_long_1 = (i == 0) ? 3 : (i == 1) ? 4 : 6;
        dds_sample_state_t   expected_sst    = DDS_SST_NOT_READ;
        dds_view_state_t     expected_vst    = SAMPLE_VST(expected_long_1);
        dds_instance_state_t expected_ist    = SAMPLE_IST(expected_long_1);

        /* Check data. */
        cr_assert_eq(sample->long_1, expected_long_1  );
        cr_assert_eq(sample->long_2, expected_long_1/2);
        cr_assert_eq(sample->long_3, expected_long_1/3);

        /* Check states. */
        cr_assert_eq(g_info[i].valid_data,     true);
        cr_assert_eq(g_info[i].sample_state,   expected_sst);
        cr_assert_eq(g_info[i].view_state,     expected_vst);
        cr_assert_eq(g_info[i].instance_state, expected_ist);
    }

    dds_delete(condition);
}
/*************************************************************************************************/

/*************************************************************************************************/
Test(ddsc_readcondition_take, already_deleted, .init=readcondition_init, .fini=readcondition_fini)
{
    dds_entity_t condition;
    dds_return_t ret;

    /* Create condition. */
    condition = dds_create_readcondition(g_reader, DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE);
    cr_assert_gt(condition, 0, "Failed to create prerequisite condition");

    /* Delete condition. */
    ret = dds_delete(condition);
    cr_assert_eq(ret, DDS_RETCODE_OK, "Failed to delete prerequisite condition");

    /* Try to take with a deleted condition. */
    ret = dds_take(condition, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
    cr_expect_eq(dds_err_nr(ret), DDS_RETCODE_ALREADY_DELETED);
}
/*************************************************************************************************/



#endif
