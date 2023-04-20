// Copyright(c) 2006 to 2020 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <limits.h>

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

static bool
filter_mod2(const void * sample)
{
    const Space_Type1 *s = sample;
    return (s->long_1 % 2 == 0);
}

static void
querycondition_init_hdepth(int hdepth)
{
    Space_Type1 sample = { 0, 0, 0 };
    dds_qos_t *qos = dds_create_qos ();
    dds_attach_t triggered;
    dds_return_t ret;
    char name[100];

    g_participant = dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);
    CU_ASSERT_FATAL(g_participant > 0);

    g_waitset = dds_create_waitset(g_participant);
    CU_ASSERT_FATAL(g_waitset > 0);

    g_topic = dds_create_topic(g_participant, &Space_Type1_desc, create_unique_topic_name("ddsc_querycondition_test", name, sizeof name), NULL, NULL);
    CU_ASSERT_FATAL(g_topic > 0);

    /* Create a reader that keeps last sample of all instances. */
    dds_qset_history(qos, DDS_HISTORY_KEEP_LAST, hdepth);
    g_reader = dds_create_reader(g_participant, g_topic, qos, NULL);
    CU_ASSERT_FATAL(g_reader > 0);

    /* Create a reader that will not automatically dispose unregistered samples. */
    dds_qset_writer_data_lifecycle(qos, false);
    g_writer = dds_create_writer(g_participant, g_topic, qos, NULL);
    CU_ASSERT_FATAL(g_writer > 0);

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

        ret = dds_write(g_writer, &sample);
        CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

        if (ist == DDS_IST_NOT_ALIVE_DISPOSED) {
            ret = dds_dispose(g_writer, &sample);
            CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
        }
        if (ist == DDS_IST_NOT_ALIVE_NO_WRITERS) {
            ret = dds_unregister_instance(g_writer, &sample);
            CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
        }
    }

    /* Read samples to get read&old_view states. */
    ret = dds_read(g_reader, g_samples, g_info, MAX_SAMPLES, SAMPLE_LAST_OLD_VST + 1);
    CU_ASSERT_EQUAL_FATAL(ret, SAMPLE_LAST_OLD_VST + 1);
#ifdef VERBOSE_INIT
    for(int i = 0; i < ret; i++) {
        Space_Type1 *s = (Space_Type1*)g_samples[i];
    }
#endif

    /* Re-write the samples that should be not_read&old_view. */
    for (int i = SAMPLE_LAST_READ_SST + 1; i <= SAMPLE_LAST_OLD_VST; i++) {
        dds_instance_state_t ist = SAMPLE_IST(i);
        sample.long_1 = i;
        sample.long_2 = i/2;
        sample.long_3 = i/3;

        ret = dds_write(g_writer, &sample);
        CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

        if ((ist == DDS_IST_NOT_ALIVE_DISPOSED) && (i != 4)) {
            ret = dds_dispose(g_writer, &sample);
            CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
        }
        if (ist == DDS_IST_NOT_ALIVE_NO_WRITERS) {
            ret = dds_unregister_instance(g_writer, &sample);
            CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
        }
    }

    dds_delete_qos(qos);
}

static void
querycondition_init(void)
{
    querycondition_init_hdepth(1);
}

static void
querycondition_init_deephist(void)
{
    querycondition_init_hdepth(333);
}

static void
querycondition_fini(void)
{
    dds_delete(g_reader);
    dds_delete(g_writer);
    dds_delete(g_waitset);
    dds_delete(g_topic);
    dds_delete(g_participant);
}


/**************************************************************************************************
 *
 * These will check the querycondition creation in various ways.
 *
 *************************************************************************************************/
/*************************************************************************************************/
CU_Test(ddsc_querycondition_create, second, .init=querycondition_init, .fini=querycondition_fini)
{
    uint32_t mask = DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
    dds_entity_t cond1;
    dds_entity_t cond2;
    dds_return_t ret;

    cond1 = dds_create_querycondition(g_reader, mask, filter_mod2);
    CU_ASSERT_FATAL(cond1 > 0);

    cond2 = dds_create_querycondition(g_reader, mask, filter_mod2);
    CU_ASSERT_FATAL(cond2 > 0);

    /* Also, we should be able to delete both. */
    ret = dds_delete(cond1);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    /* And, of course, be able to delete the first one (return code isn't checked in the test fixtures). */
    ret = dds_delete(cond2);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_querycondition_create, deleted_reader, .init=querycondition_init, .fini=querycondition_fini)
{
    uint32_t mask = DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
    dds_entity_t cond;
    dds_delete(g_reader);
    cond = dds_create_querycondition(g_reader, mask, filter_mod2);
    CU_ASSERT_EQUAL_FATAL(cond, DDS_RETCODE_BAD_PARAMETER);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_querycondition_create, invalid_readers) = {
        CU_DataPoints(dds_entity_t, -2, -1, 0, INT_MAX, INT_MIN),
};
CU_Theory((dds_entity_t rdr), ddsc_querycondition_create, invalid_readers, .init=querycondition_init, .fini=querycondition_fini)
{
    uint32_t mask = DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
    dds_entity_t cond;

    cond = dds_create_querycondition(rdr, mask, filter_mod2);
    CU_ASSERT_EQUAL_FATAL(cond, DDS_RETCODE_BAD_PARAMETER);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_querycondition_create, non_readers) = {
        CU_DataPoints(dds_entity_t*, &g_topic, &g_participant),
};
CU_Theory((dds_entity_t *rdr), ddsc_querycondition_create, non_readers, .init=querycondition_init, .fini=querycondition_fini)
{
    uint32_t mask = DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
    dds_entity_t cond;
    cond = dds_create_querycondition(*rdr, mask, filter_mod2);
    CU_ASSERT_EQUAL_FATAL(cond, DDS_RETCODE_ILLEGAL_OPERATION);
}
/*************************************************************************************************/





/**************************************************************************************************
 *
 * These will check the querycondition mask acquiring in various ways.
 *
 *************************************************************************************************/
/*************************************************************************************************/
CU_Test(ddsc_querycondition_get_mask, deleted, .init=querycondition_init, .fini=querycondition_fini)
{
    uint32_t mask = DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
    dds_entity_t condition;
    dds_return_t ret;
    condition = dds_create_querycondition(g_reader, mask, filter_mod2);
    CU_ASSERT_FATAL(condition > 0);
    dds_delete(condition);
    mask = 0;
    ret = dds_get_mask(condition, &mask);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_querycondition_get_mask, null, .init=querycondition_init, .fini=querycondition_fini)
{
    uint32_t mask = DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
    dds_entity_t condition;
    dds_return_t ret;
    condition = dds_create_querycondition(g_reader, mask, filter_mod2);
    CU_ASSERT_FATAL(condition > 0);
    DDSRT_WARNING_MSVC_OFF(6387); /* Disable SAL warning on intentional misuse of the API */
    ret = dds_get_mask(condition, NULL);
    DDSRT_WARNING_MSVC_ON(6387);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
    dds_delete(condition);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_querycondition_get_mask, invalid_conditions) = {
        CU_DataPoints(dds_entity_t, -2, -1, 0, INT_MAX, INT_MIN),
};
CU_Theory((dds_entity_t cond), ddsc_querycondition_get_mask, invalid_conditions, .init=querycondition_init, .fini=querycondition_fini)
{
    dds_return_t ret;
    uint32_t mask;

    ret = dds_get_mask(cond, &mask);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_querycondition_get_mask, non_conditions) = {
        CU_DataPoints(dds_entity_t*, &g_reader, &g_topic, &g_participant),
};
CU_Theory((dds_entity_t *cond), ddsc_querycondition_get_mask, non_conditions, .init=querycondition_init, .fini=querycondition_fini)
{
    dds_return_t ret;
    uint32_t mask;
    ret = dds_get_mask(*cond, &mask);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_ILLEGAL_OPERATION);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_querycondition_get_mask, various_masks) = {
        CU_DataPoints(uint32_t, DDS_ANY_SAMPLE_STATE,  DDS_READ_SAMPLE_STATE,     DDS_NOT_READ_SAMPLE_STATE),
        CU_DataPoints(uint32_t, DDS_ANY_VIEW_STATE,     DDS_NEW_VIEW_STATE,       DDS_NOT_NEW_VIEW_STATE),
        CU_DataPoints(uint32_t, DDS_ANY_INSTANCE_STATE, DDS_ALIVE_INSTANCE_STATE, DDS_NOT_ALIVE_DISPOSED_INSTANCE_STATE, DDS_NOT_ALIVE_NO_WRITERS_INSTANCE_STATE),
};
CU_Theory((uint32_t ss, uint32_t vs, uint32_t is), ddsc_querycondition_get_mask, various_masks, .init=querycondition_init, .fini=querycondition_fini)
{
    uint32_t maskIn  = ss | vs | is;
    uint32_t maskOut = 0xFFFFFFFF;
    dds_entity_t condition;
    dds_return_t ret;

    condition = dds_create_querycondition(g_reader, maskIn, filter_mod2);
    CU_ASSERT_FATAL(condition > 0);

    ret = dds_get_mask(condition, &maskOut);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    CU_ASSERT_EQUAL_FATAL(maskIn, maskOut);

    dds_delete(condition);
}
/*************************************************************************************************/




/**************************************************************************************************
 *
 * These will check the querycondition reading in various ways.
 *
 *************************************************************************************************/
/*************************************************************************************************/
CU_Test(ddsc_querycondition_read, any, .init=querycondition_init, .fini=querycondition_fini)
{
    dds_entity_t condition;
    dds_return_t ret;

    /* Create condition. */
    condition = dds_create_querycondition(g_reader, DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE, filter_mod2);
    CU_ASSERT_FATAL(condition > 0);

    /* Read all samples that matches filter. */
    ret = dds_read(condition, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
    CU_ASSERT_EQUAL_FATAL(ret, 4);
    for(int i = 0; i < ret; i++) {
        Space_Type1 *sample = (Space_Type1*)g_samples[i];

        /*
         * | long_1 | long_2 | long_3 |    sst   | vst |    ist     |
         * ----------------------------------------------------------
         * |    0   |    0   |    0   |     read | old | alive      | <---
         * |    1   |    0   |    0   |     read | old | disposed   |
         * |    2   |    1   |    0   |     read | old | no_writers | <---
         * |    3   |    1   |    1   | not_read | old | alive      |
         * |    4   |    2   |    1   | not_read | new | disposed   | <---
         * |    5   |    2   |    1   | not_read | new | no_writers |
         * |    6   |    3   |    2   | not_read | new | alive      | <---
         */

        /* Expected states. */
        int                  expected_long_1 = i * 2;
        dds_sample_state_t   expected_sst    = SAMPLE_SST(expected_long_1);
        dds_view_state_t     expected_vst    = SAMPLE_VST(expected_long_1);
        dds_instance_state_t expected_ist    = SAMPLE_IST(expected_long_1);

        /* Check data. */
        CU_ASSERT_EQUAL_FATAL(sample->long_1, expected_long_1  );
        CU_ASSERT_EQUAL_FATAL(sample->long_2, expected_long_1/2);
        CU_ASSERT_EQUAL_FATAL(sample->long_3, expected_long_1/3);

        /* Check states. */
        CU_ASSERT_EQUAL_FATAL(g_info[i].valid_data,     true);
        CU_ASSERT_EQUAL_FATAL(g_info[i].sample_state,   expected_sst);
        CU_ASSERT_EQUAL_FATAL(g_info[i].view_state,     expected_vst);
        CU_ASSERT_EQUAL_FATAL(g_info[i].instance_state, expected_ist);
    }

    dds_delete(condition);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_querycondition_read, not_read_sample_state, .init=querycondition_init, .fini=querycondition_fini)
{
    dds_entity_t condition;
    dds_return_t ret;

    /* Create condition. */
    condition = dds_create_querycondition(g_reader, DDS_NOT_READ_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE, filter_mod2);
    CU_ASSERT_FATAL(condition > 0);

    /* Read all non-read samples and matches filter. */
    ret = dds_read(condition, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
    CU_ASSERT_EQUAL_FATAL(ret, 2);
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
         * |    5   |    2   |    1   | not_read | new | no_writers |
         * |    6   |    3   |    2   | not_read | new | alive      | <---
         */

        /* Expected states. */
        int                  expected_long_1 = (i == 0) ? 4 : 6;
        dds_sample_state_t   expected_sst    = DDS_SST_NOT_READ;
        dds_view_state_t     expected_vst    = SAMPLE_VST(expected_long_1);
        dds_instance_state_t expected_ist    = SAMPLE_IST(expected_long_1);

        /* Check data. */
        CU_ASSERT_EQUAL_FATAL(sample->long_1, expected_long_1  );
        CU_ASSERT_EQUAL_FATAL(sample->long_2, expected_long_1/2);
        CU_ASSERT_EQUAL_FATAL(sample->long_3, expected_long_1/3);

        /* Check states. */
        CU_ASSERT_EQUAL_FATAL(g_info[i].valid_data,     true);
        CU_ASSERT_EQUAL_FATAL(g_info[i].sample_state,   expected_sst);
        CU_ASSERT_EQUAL_FATAL(g_info[i].view_state,     expected_vst);
        CU_ASSERT_EQUAL_FATAL(g_info[i].instance_state, expected_ist);
    }

    dds_delete(condition);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_querycondition_read, read_sample_state, .init=querycondition_init, .fini=querycondition_fini)
{
    dds_entity_t condition;
    dds_return_t ret;

    /* Create condition. */
    condition = dds_create_querycondition(g_reader, DDS_READ_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE, filter_mod2);
    CU_ASSERT_FATAL(condition > 0);

    /* Read all already read samples and matches filter. */
    ret = dds_read(condition, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
    CU_ASSERT_EQUAL_FATAL(ret, 2);
    for(int i = 0; i < ret; i++) {
        Space_Type1 *sample = (Space_Type1*)g_samples[i];

        /*
         * | long_1 | long_2 | long_3 |    sst   | vst |    ist     |
         * ----------------------------------------------------------
         * |    0   |    0   |    0   |     read | old | alive      | <---
         * |    1   |    0   |    0   |     read | old | disposed   |
         * |    2   |    1   |    0   |     read | old | no_writers | <---
         * |    3   |    1   |    1   | not_read | old | alive      |
         * |    4   |    2   |    1   | not_read | new | disposed   |
         * |    5   |    2   |    1   | not_read | new | no_writers |
         * |    6   |    3   |    2   | not_read | new | alive      |
         */

        /* Expected states. */
        int                  expected_long_1 = i * 2;
        dds_sample_state_t   expected_sst    = DDS_SST_READ;
        dds_view_state_t     expected_vst    = SAMPLE_VST(expected_long_1);
        dds_instance_state_t expected_ist    = SAMPLE_IST(expected_long_1);

        /* Check data. */
        CU_ASSERT_EQUAL_FATAL(sample->long_1, expected_long_1  );
        CU_ASSERT_EQUAL_FATAL(sample->long_2, expected_long_1/2);
        CU_ASSERT_EQUAL_FATAL(sample->long_3, expected_long_1/3);

        /* Check states. */
        CU_ASSERT_EQUAL_FATAL(g_info[i].valid_data,     true);
        CU_ASSERT_EQUAL_FATAL(g_info[i].sample_state,   expected_sst);
        CU_ASSERT_EQUAL_FATAL(g_info[i].view_state,     expected_vst);
        CU_ASSERT_EQUAL_FATAL(g_info[i].instance_state, expected_ist);
    }

    dds_delete(condition);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_querycondition_read, new_view_state, .init=querycondition_init, .fini=querycondition_fini)
{
    dds_entity_t condition;
    dds_return_t ret;

    /* Create condition. */
    condition = dds_create_querycondition(g_reader, DDS_ANY_SAMPLE_STATE | DDS_NEW_VIEW_STATE | DDS_ANY_INSTANCE_STATE, filter_mod2);
    CU_ASSERT_FATAL(condition > 0);

    /* Read all new-view samples and matches filter. */
    ret = dds_read(condition, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
    CU_ASSERT_EQUAL_FATAL(ret, 2);
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
         * |    5   |    2   |    1   | not_read | new | no_writers |
         * |    6   |    3   |    2   | not_read | new | alive      | <---
         */

        /* Expected states. */
        int                  expected_long_1 = (i == 0) ? 4 : 6;
        dds_sample_state_t   expected_sst    = SAMPLE_SST(expected_long_1);
        dds_view_state_t     expected_vst    = DDS_VST_NEW;
        dds_instance_state_t expected_ist    = SAMPLE_IST(expected_long_1);

        /* Check data. */
        CU_ASSERT_EQUAL_FATAL(sample->long_1, expected_long_1  );
        CU_ASSERT_EQUAL_FATAL(sample->long_2, expected_long_1/2);
        CU_ASSERT_EQUAL_FATAL(sample->long_3, expected_long_1/3);

        /* Check states. */
        CU_ASSERT_EQUAL_FATAL(g_info[i].valid_data,     true);
        CU_ASSERT_EQUAL_FATAL(g_info[i].sample_state,   expected_sst);
        CU_ASSERT_EQUAL_FATAL(g_info[i].view_state,     expected_vst);
        CU_ASSERT_EQUAL_FATAL(g_info[i].instance_state, expected_ist);
    }

    dds_delete(condition);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_querycondition_read, not_new_view_state, .init=querycondition_init, .fini=querycondition_fini)
{
    dds_entity_t condition;
    dds_return_t ret;

    /* Create condition. */
    condition = dds_create_querycondition(g_reader, DDS_ANY_SAMPLE_STATE | DDS_NOT_NEW_VIEW_STATE | DDS_ANY_INSTANCE_STATE, filter_mod2);
    CU_ASSERT_FATAL(condition > 0);

    /* Read all old-view samples and matches filter. */
    ret = dds_read(condition, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
    CU_ASSERT_EQUAL_FATAL(ret, 2);
    for(int i = 0; i < ret; i++) {
        Space_Type1 *sample = (Space_Type1*)g_samples[i];

        /*
         * | long_1 | long_2 | long_3 |    sst   | vst |    ist     |
         * ----------------------------------------------------------
         * |    0   |    0   |    0   |     read | old | alive      | <---
         * |    1   |    0   |    0   |     read | old | disposed   |
         * |    2   |    1   |    0   |     read | old | no_writers | <---
         * |    3   |    1   |    1   | not_read | old | alive      |
         * |    4   |    2   |    1   | not_read | new | disposed   |
         * |    5   |    2   |    1   | not_read | new | no_writers |
         * |    6   |    3   |    2   | not_read | new | alive      |
         */

        /* Expected states. */
        int                  expected_long_1 = i * 2;
        dds_sample_state_t   expected_sst    = SAMPLE_SST(expected_long_1);
        dds_view_state_t     expected_vst    = DDS_VST_OLD;
        dds_instance_state_t expected_ist    = SAMPLE_IST(expected_long_1);

        /* Check data. */
        CU_ASSERT_EQUAL_FATAL(sample->long_1, expected_long_1  );
        CU_ASSERT_EQUAL_FATAL(sample->long_2, expected_long_1/2);
        CU_ASSERT_EQUAL_FATAL(sample->long_3, expected_long_1/3);

        /* Check states. */
        CU_ASSERT_EQUAL_FATAL(g_info[i].valid_data,     true);
        CU_ASSERT_EQUAL_FATAL(g_info[i].sample_state,   expected_sst);
        CU_ASSERT_EQUAL_FATAL(g_info[i].view_state,     expected_vst);
        CU_ASSERT_EQUAL_FATAL(g_info[i].instance_state, expected_ist);
    }

    dds_delete(condition);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_querycondition_read, alive_instance_state, .init=querycondition_init, .fini=querycondition_fini)
{
    dds_entity_t condition;
    dds_return_t ret;

    /* Create condition. */
    condition = dds_create_querycondition(g_reader, DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ALIVE_INSTANCE_STATE, filter_mod2);
    CU_ASSERT_FATAL(condition > 0);

    /* Read all alive samples and matches filter. */
    ret = dds_read(condition, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
    CU_ASSERT_EQUAL_FATAL(ret, 2);
    for(int i = 0; i < ret; i++) {
        Space_Type1 *sample = (Space_Type1*)g_samples[i];

        /*
         * | long_1 | long_2 | long_3 |    sst   | vst |    ist     |
         * ----------------------------------------------------------
         * |    0   |    0   |    0   |     read | old | alive      | <---
         * |    1   |    0   |    0   |     read | old | disposed   |
         * |    2   |    1   |    0   |     read | old | no_writers |
         * |    3   |    1   |    1   | not_read | old | alive      |
         * |    4   |    2   |    1   | not_read | new | disposed   |
         * |    5   |    2   |    1   | not_read | new | no_writers |
         * |    6   |    3   |    2   | not_read | new | alive      | <---
         */

        /* Expected states. */
        int                  expected_long_1 = (i == 0) ? 0 : 6;
        dds_sample_state_t   expected_sst    = SAMPLE_SST(expected_long_1);
        dds_view_state_t     expected_vst    = SAMPLE_VST(expected_long_1);
        dds_instance_state_t expected_ist    = DDS_IST_ALIVE;

        /* Check data. */
        CU_ASSERT_EQUAL_FATAL(sample->long_1, expected_long_1  );
        CU_ASSERT_EQUAL_FATAL(sample->long_2, expected_long_1/2);
        CU_ASSERT_EQUAL_FATAL(sample->long_3, expected_long_1/3);

        /* Check states. */
        CU_ASSERT_EQUAL_FATAL(g_info[i].valid_data,     true);
        CU_ASSERT_EQUAL_FATAL(g_info[i].sample_state,   expected_sst);
        CU_ASSERT_EQUAL_FATAL(g_info[i].view_state,     expected_vst);
        CU_ASSERT_EQUAL_FATAL(g_info[i].instance_state, expected_ist);
    }

    dds_delete(condition);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_querycondition_read, disposed_instance_state, .init=querycondition_init, .fini=querycondition_fini)
{
    dds_entity_t condition;
    dds_return_t ret;

    /* Create condition. */
    condition = dds_create_querycondition(g_reader, DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_NOT_ALIVE_DISPOSED_INSTANCE_STATE, filter_mod2);
    CU_ASSERT_FATAL(condition > 0);

    /* Read all disposed samples and matches filter. */
    ret = dds_read(condition, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
    CU_ASSERT_EQUAL_FATAL(ret, 1);
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
         * |    5   |    2   |    1   | not_read | new | no_writers |
         * |    6   |    3   |    2   | not_read | new | alive      |
         */

        /* Expected states. */
        int                  expected_long_1 = 4;
        dds_sample_state_t   expected_sst    = SAMPLE_SST(expected_long_1);
        dds_view_state_t     expected_vst    = SAMPLE_VST(expected_long_1);
        dds_instance_state_t expected_ist    = DDS_IST_NOT_ALIVE_DISPOSED;

        /* Check data. */
        CU_ASSERT_EQUAL_FATAL(sample->long_1, expected_long_1  );
        CU_ASSERT_EQUAL_FATAL(sample->long_2, expected_long_1/2);
        CU_ASSERT_EQUAL_FATAL(sample->long_3, expected_long_1/3);

        /* Check states. */
        CU_ASSERT_EQUAL_FATAL(g_info[i].valid_data,     true);
        CU_ASSERT_EQUAL_FATAL(g_info[i].sample_state,   expected_sst);
        CU_ASSERT_EQUAL_FATAL(g_info[i].view_state,     expected_vst);
        CU_ASSERT_EQUAL_FATAL(g_info[i].instance_state, expected_ist);
    }

    dds_delete(condition);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_querycondition_read, no_writers_instance_state, .init=querycondition_init, .fini=querycondition_fini)
{
    dds_entity_t condition;
    dds_return_t ret;

    /* Create condition. */
    condition = dds_create_querycondition(g_reader, DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_NOT_ALIVE_NO_WRITERS_INSTANCE_STATE, filter_mod2);
    CU_ASSERT_FATAL(condition > 0);

    /* Read all samples without a writer and matches filter. */
    ret = dds_read(condition, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
    CU_ASSERT_EQUAL_FATAL(ret, 1);
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
         * |    5   |    2   |    1   | not_read | new | no_writers |
         * |    6   |    3   |    2   | not_read | new | alive      |
         */

        /* Expected states. */
        int                  expected_long_1 = 2;
        dds_sample_state_t   expected_sst    = SAMPLE_SST(expected_long_1);
        dds_view_state_t     expected_vst    = SAMPLE_VST(expected_long_1);
        dds_instance_state_t expected_ist    = DDS_IST_NOT_ALIVE_NO_WRITERS;

        /* Check data. */
        CU_ASSERT_EQUAL_FATAL(sample->long_1, expected_long_1  );
        CU_ASSERT_EQUAL_FATAL(sample->long_2, expected_long_1/2);
        CU_ASSERT_EQUAL_FATAL(sample->long_3, expected_long_1/3);

        /* Check states. */
        CU_ASSERT_EQUAL_FATAL(g_info[i].valid_data,     true);
        CU_ASSERT_EQUAL_FATAL(g_info[i].sample_state,   expected_sst);
        CU_ASSERT_EQUAL_FATAL(g_info[i].view_state,     expected_vst);
        CU_ASSERT_EQUAL_FATAL(g_info[i].instance_state, expected_ist);
    }

    dds_delete(condition);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_querycondition_read, combination_of_states, .init=querycondition_init, .fini=querycondition_fini)
{
    dds_entity_t condition;
    dds_return_t ret;

    /* Create condition. */
    condition = dds_create_querycondition(g_reader, DDS_NOT_READ_SAMPLE_STATE | DDS_NEW_VIEW_STATE | DDS_ALIVE_INSTANCE_STATE, filter_mod2);
    CU_ASSERT_FATAL(condition > 0);

    /* Read all samples that match the mask and filter (should be only one). */
    ret = dds_read(condition, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
    CU_ASSERT_EQUAL_FATAL(ret, 1);
    for(int i = 0; i < ret; i++) {
        Space_Type1 *sample = (Space_Type1*)g_samples[i];

        /*
         * | long_1 | long_2 | long_3 |    sst   | vst |    ist     |
         * ----------------------------------------------------------
         * |    0   |    0   |    0   |     read | old | alive      |
         * |    1   |    0   |    0   |     read | old | disposed   |
         * |    2   |    1   |    0   |     read | old | no_writers |
         * |    3   |    1   |    1   | not_read | old | alive      |
         * |    4   |    2   |    1   | not_read | new | disposed   |
         * |    5   |    2   |    1   | not_read | new | no_writers |
         * |    6   |    3   |    2   | not_read | new | alive      | <---
         */

        /* Expected states. */
        int                  expected_long_1 = 6;
        dds_sample_state_t   expected_sst    = DDS_SST_NOT_READ;
        dds_view_state_t     expected_vst    = DDS_VST_NEW;
        dds_instance_state_t expected_ist    = DDS_IST_ALIVE;

        /* Check data. */
        CU_ASSERT_EQUAL_FATAL(sample->long_1, expected_long_1  );
        CU_ASSERT_EQUAL_FATAL(sample->long_2, expected_long_1/2);
        CU_ASSERT_EQUAL_FATAL(sample->long_3, expected_long_1/3);

        /* Check states. */
        CU_ASSERT_EQUAL_FATAL(g_info[i].valid_data,     true);
        CU_ASSERT_EQUAL_FATAL(g_info[i].sample_state,   expected_sst);
        CU_ASSERT_EQUAL_FATAL(g_info[i].view_state,     expected_vst);
        CU_ASSERT_EQUAL_FATAL(g_info[i].instance_state, expected_ist);
    }

    dds_delete(condition);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_querycondition_read, none, .init=querycondition_init, .fini=querycondition_fini)
{
    dds_entity_t condition;
    dds_return_t ret;

    /* Create condition. */
    condition = dds_create_querycondition(g_reader, DDS_NOT_READ_SAMPLE_STATE | DDS_NOT_NEW_VIEW_STATE | DDS_ALIVE_INSTANCE_STATE, filter_mod2);
    CU_ASSERT_FATAL(condition > 0);

    /* Read all samples that match the mask AND filter (should be none). */
    ret = dds_read(condition, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
    CU_ASSERT_EQUAL_FATAL(ret, 0);

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
CU_Test(ddsc_querycondition_read, with_mask, .init=querycondition_init, .fini=querycondition_fini)
{
    dds_entity_t condition;
    dds_return_t ret;

    /* Create condition. */
    condition = dds_create_querycondition(g_reader, DDS_NOT_READ_SAMPLE_STATE | DDS_NEW_VIEW_STATE | DDS_ALIVE_INSTANCE_STATE, filter_mod2);
    CU_ASSERT_FATAL(condition > 0);

    /* Read all samples that match the or'd masks. */
    ret = dds_read_mask(condition, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES,
                        DDS_NOT_READ_SAMPLE_STATE | DDS_NOT_NEW_VIEW_STATE | DDS_NOT_ALIVE_DISPOSED_INSTANCE_STATE);
    CU_ASSERT_EQUAL_FATAL(ret, 2);
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
         * |    5   |    2   |    1   | not_read | new | no_writers |
         * |    6   |    3   |    2   | not_read | new | alive      | <---
         */

        /* Expected states. */
        int                  expected_long_1 = (i == 0) ? 4 : 6;
        dds_sample_state_t   expected_sst    = DDS_SST_NOT_READ;
        dds_view_state_t     expected_vst    = SAMPLE_VST(expected_long_1);
        dds_instance_state_t expected_ist    = SAMPLE_IST(expected_long_1);

        /* Check data. */
        CU_ASSERT_EQUAL_FATAL(sample->long_1, expected_long_1  );
        CU_ASSERT_EQUAL_FATAL(sample->long_2, expected_long_1/2);
        CU_ASSERT_EQUAL_FATAL(sample->long_3, expected_long_1/3);

        /* Check states. */
        CU_ASSERT_EQUAL_FATAL(g_info[i].valid_data,     true);
        CU_ASSERT_EQUAL_FATAL(g_info[i].sample_state,   expected_sst);
        CU_ASSERT_EQUAL_FATAL(g_info[i].view_state,     expected_vst);
        CU_ASSERT_EQUAL_FATAL(g_info[i].instance_state, expected_ist);
    }

    dds_delete(condition);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_querycondition_read, already_deleted, .init=querycondition_init, .fini=querycondition_fini)
{
    dds_entity_t condition;
    dds_return_t ret;

    /* Create condition. */
    condition = dds_create_querycondition(g_reader, DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE, filter_mod2);
    CU_ASSERT_FATAL(condition > 0);

    /* Delete condition. */
    ret = dds_delete(condition);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    /* Try to read with a deleted condition. */
    ret = dds_read(condition, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
}
/*************************************************************************************************/








/**************************************************************************************************
 *
 * These will check the querycondition taking in various ways.
 *
 *************************************************************************************************/
/*************************************************************************************************/
CU_Test(ddsc_querycondition_take, any, .init=querycondition_init, .fini=querycondition_fini)
{
    dds_entity_t condition;
    dds_return_t ret;

    /* Create condition. */
    condition = dds_create_querycondition(g_reader, DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE, filter_mod2);
    CU_ASSERT_FATAL(condition > 0);

    /* Take all samples that match the filter. */
    ret = dds_take(condition, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
    CU_ASSERT_EQUAL_FATAL(ret, 4);
    for(int i = 0; i < ret; i++) {
        Space_Type1 *sample = (Space_Type1*)g_samples[i];

        /*
         * | long_1 | long_2 | long_3 |    sst   | vst |    ist     |
         * ----------------------------------------------------------
         * |    0   |    0   |    0   |     read | old | alive      | <---
         * |    1   |    0   |    0   |     read | old | disposed   |
         * |    2   |    1   |    0   |     read | old | no_writers | <---
         * |    3   |    1   |    1   | not_read | old | alive      |
         * |    4   |    2   |    1   | not_read | new | disposed   | <---
         * |    5   |    2   |    1   | not_read | new | no_writers |
         * |    6   |    3   |    2   | not_read | new | alive      | <---
         */

        /* Expected states. */
        int                  expected_long_1 = i * 2;
        dds_sample_state_t   expected_sst    = SAMPLE_SST(expected_long_1);
        dds_view_state_t     expected_vst    = SAMPLE_VST(expected_long_1);
        dds_instance_state_t expected_ist    = SAMPLE_IST(expected_long_1);

        /* Check data. */
        CU_ASSERT_EQUAL_FATAL(sample->long_1, expected_long_1  );
        CU_ASSERT_EQUAL_FATAL(sample->long_2, expected_long_1/2);
        CU_ASSERT_EQUAL_FATAL(sample->long_3, expected_long_1/3);

        /* Check states. */
        CU_ASSERT_EQUAL_FATAL(g_info[i].valid_data,     true);
        CU_ASSERT_EQUAL_FATAL(g_info[i].sample_state,   expected_sst);
        CU_ASSERT_EQUAL_FATAL(g_info[i].view_state,     expected_vst);
        CU_ASSERT_EQUAL_FATAL(g_info[i].instance_state, expected_ist);
    }

    dds_delete(condition);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_querycondition_take, not_read_sample_state, .init=querycondition_init, .fini=querycondition_fini)
{
    dds_entity_t condition;
    dds_return_t ret;

    /* Create condition. */
    condition = dds_create_querycondition(g_reader, DDS_NOT_READ_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE, filter_mod2);
    CU_ASSERT_FATAL(condition > 0);

    /* Take all non-read samples that match the filter. */
    ret = dds_take(condition, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
    CU_ASSERT_EQUAL_FATAL(ret, 2);
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
         * |    5   |    2   |    1   | not_read | new | no_writers |
         * |    6   |    3   |    2   | not_read | new | alive      | <---
         */

        /* Expected states. */
        int                  expected_long_1 = (i == 0) ? 4 : 6;
        dds_sample_state_t   expected_sst    = DDS_SST_NOT_READ;
        dds_view_state_t     expected_vst    = SAMPLE_VST(expected_long_1);
        dds_instance_state_t expected_ist    = SAMPLE_IST(expected_long_1);

        /* Check data. */
        CU_ASSERT_EQUAL_FATAL(sample->long_1, expected_long_1  );
        CU_ASSERT_EQUAL_FATAL(sample->long_2, expected_long_1/2);
        CU_ASSERT_EQUAL_FATAL(sample->long_3, expected_long_1/3);

        /* Check states. */
        CU_ASSERT_EQUAL_FATAL(g_info[i].valid_data,     true);
        CU_ASSERT_EQUAL_FATAL(g_info[i].sample_state,   expected_sst);
        CU_ASSERT_EQUAL_FATAL(g_info[i].view_state,     expected_vst);
        CU_ASSERT_EQUAL_FATAL(g_info[i].instance_state, expected_ist);
    }

    dds_delete(condition);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_querycondition_take, read_sample_state, .init=querycondition_init, .fini=querycondition_fini)
{
    dds_entity_t condition;
    dds_return_t ret;

    /* Create condition. */
    condition = dds_create_querycondition(g_reader, DDS_READ_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE, filter_mod2);
    CU_ASSERT_FATAL(condition > 0);

    /* Take all already read samples that match the filter. */
    ret = dds_take(condition, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
    CU_ASSERT_EQUAL_FATAL(ret, 2);
    for(int i = 0; i < ret; i++) {
        Space_Type1 *sample = (Space_Type1*)g_samples[i];

        /*
         * | long_1 | long_2 | long_3 |    sst   | vst |    ist     |
         * ----------------------------------------------------------
         * |    0   |    0   |    0   |     read | old | alive      | <---
         * |    1   |    0   |    0   |     read | old | disposed   |
         * |    2   |    1   |    0   |     read | old | no_writers | <---
         * |    3   |    1   |    1   | not_read | old | alive      |
         * |    4   |    2   |    1   | not_read | new | disposed   |
         * |    5   |    2   |    1   | not_read | new | no_writers |
         * |    6   |    3   |    2   | not_read | new | alive      |
         */

        /* Expected states. */
        int                  expected_long_1 = i * 2;
        dds_sample_state_t   expected_sst    = DDS_SST_READ;
        dds_view_state_t     expected_vst    = SAMPLE_VST(expected_long_1);
        dds_instance_state_t expected_ist    = SAMPLE_IST(expected_long_1);

        /* Check data. */
        CU_ASSERT_EQUAL_FATAL(sample->long_1, expected_long_1  );
        CU_ASSERT_EQUAL_FATAL(sample->long_2, expected_long_1/2);
        CU_ASSERT_EQUAL_FATAL(sample->long_3, expected_long_1/3);

        /* Check states. */
        CU_ASSERT_EQUAL_FATAL(g_info[i].valid_data,     true);
        CU_ASSERT_EQUAL_FATAL(g_info[i].sample_state,   expected_sst);
        CU_ASSERT_EQUAL_FATAL(g_info[i].view_state,     expected_vst);
        CU_ASSERT_EQUAL_FATAL(g_info[i].instance_state, expected_ist);
    }

    dds_delete(condition);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_querycondition_take, new_view_state, .init=querycondition_init, .fini=querycondition_fini)
{
    dds_entity_t condition;
    dds_return_t ret;

    /* Create condition. */
    condition = dds_create_querycondition(g_reader, DDS_ANY_SAMPLE_STATE | DDS_NEW_VIEW_STATE | DDS_ANY_INSTANCE_STATE, filter_mod2);
    CU_ASSERT_FATAL(condition > 0);

    /* Take all new-view samples that match the filter. */
    ret = dds_take(condition, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
    CU_ASSERT_EQUAL_FATAL(ret, 2);
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
         * |    5   |    2   |    1   | not_read | new | no_writers |
         * |    6   |    3   |    2   | not_read | new | alive      | <---
         */

        /* Expected states. */
        int                  expected_long_1 = (i == 0) ? 4 : 6;
        dds_sample_state_t   expected_sst    = SAMPLE_SST(expected_long_1);
        dds_view_state_t     expected_vst    = DDS_VST_NEW;
        dds_instance_state_t expected_ist    = SAMPLE_IST(expected_long_1);

        /* Check data. */
        CU_ASSERT_EQUAL_FATAL(sample->long_1, expected_long_1  );
        CU_ASSERT_EQUAL_FATAL(sample->long_2, expected_long_1/2);
        CU_ASSERT_EQUAL_FATAL(sample->long_3, expected_long_1/3);

        /* Check states. */
        CU_ASSERT_EQUAL_FATAL(g_info[i].valid_data,     true);
        CU_ASSERT_EQUAL_FATAL(g_info[i].sample_state,   expected_sst);
        CU_ASSERT_EQUAL_FATAL(g_info[i].view_state,     expected_vst);
        CU_ASSERT_EQUAL_FATAL(g_info[i].instance_state, expected_ist);
    }

    dds_delete(condition);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_querycondition_take, not_new_view_state, .init=querycondition_init, .fini=querycondition_fini)
{
    dds_entity_t condition;
    dds_return_t ret;

    /* Create condition. */
    condition = dds_create_querycondition(g_reader, DDS_ANY_SAMPLE_STATE | DDS_NOT_NEW_VIEW_STATE | DDS_ANY_INSTANCE_STATE, filter_mod2);
    CU_ASSERT_FATAL(condition > 0);

    /* Take all old-view samples that match the filter. */
    ret = dds_take(condition, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
    CU_ASSERT_EQUAL_FATAL(ret, 2);
    for(int i = 0; i < ret; i++) {
        Space_Type1 *sample = (Space_Type1*)g_samples[i];

        /*
         * | long_1 | long_2 | long_3 |    sst   | vst |    ist     |
         * ----------------------------------------------------------
         * |    0   |    0   |    0   |     read | old | alive      | <---
         * |    1   |    0   |    0   |     read | old | disposed   |
         * |    2   |    1   |    0   |     read | old | no_writers | <---
         * |    3   |    1   |    1   | not_read | old | alive      |
         * |    4   |    2   |    1   | not_read | new | disposed   |
         * |    5   |    2   |    1   | not_read | new | no_writers |
         * |    6   |    3   |    2   | not_read | new | alive      |
         */

        /* Expected states. */
        int                  expected_long_1 = i * 2;
        dds_sample_state_t   expected_sst    = SAMPLE_SST(expected_long_1);
        dds_view_state_t     expected_vst    = DDS_VST_OLD;
        dds_instance_state_t expected_ist    = SAMPLE_IST(expected_long_1);

        /* Check data. */
        CU_ASSERT_EQUAL_FATAL(sample->long_1, expected_long_1  );
        CU_ASSERT_EQUAL_FATAL(sample->long_2, expected_long_1/2);
        CU_ASSERT_EQUAL_FATAL(sample->long_3, expected_long_1/3);

        /* Check states. */
        CU_ASSERT_EQUAL_FATAL(g_info[i].valid_data,     true);
        CU_ASSERT_EQUAL_FATAL(g_info[i].sample_state,   expected_sst);
        CU_ASSERT_EQUAL_FATAL(g_info[i].view_state,     expected_vst);
        CU_ASSERT_EQUAL_FATAL(g_info[i].instance_state, expected_ist);
    }

    dds_delete(condition);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_querycondition_take, alive_instance_state, .init=querycondition_init, .fini=querycondition_fini)
{
    dds_entity_t condition;
    dds_return_t ret;

    /* Create condition. */
    condition = dds_create_querycondition(g_reader, DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ALIVE_INSTANCE_STATE, filter_mod2);
    CU_ASSERT_FATAL(condition > 0);

    /* Take all alive samples that match the filter. */
    ret = dds_take(condition, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
    CU_ASSERT_EQUAL_FATAL(ret, 2);
    for(int i = 0; i < ret; i++) {
        Space_Type1 *sample = (Space_Type1*)g_samples[i];

        /*
         * | long_1 | long_2 | long_3 |    sst   | vst |    ist     |
         * ----------------------------------------------------------
         * |    0   |    0   |    0   |     read | old | alive      | <---
         * |    1   |    0   |    0   |     read | old | disposed   |
         * |    2   |    1   |    0   |     read | old | no_writers |
         * |    3   |    1   |    1   | not_read | old | alive      |
         * |    4   |    2   |    1   | not_read | new | disposed   |
         * |    5   |    2   |    1   | not_read | new | no_writers |
         * |    6   |    3   |    2   | not_read | new | alive      | <---
         */

        /* Expected states. */
        int                  expected_long_1 = i * 6;
        dds_sample_state_t   expected_sst    = SAMPLE_SST(expected_long_1);
        dds_view_state_t     expected_vst    = SAMPLE_VST(expected_long_1);
        dds_instance_state_t expected_ist    = DDS_IST_ALIVE;

        /* Check data. */
        CU_ASSERT_EQUAL_FATAL(sample->long_1, expected_long_1  );
        CU_ASSERT_EQUAL_FATAL(sample->long_2, expected_long_1/2);
        CU_ASSERT_EQUAL_FATAL(sample->long_3, expected_long_1/3);

        /* Check states. */
        CU_ASSERT_EQUAL_FATAL(g_info[i].valid_data,     true);
        CU_ASSERT_EQUAL_FATAL(g_info[i].sample_state,   expected_sst);
        CU_ASSERT_EQUAL_FATAL(g_info[i].view_state,     expected_vst);
        CU_ASSERT_EQUAL_FATAL(g_info[i].instance_state, expected_ist);
    }

    dds_delete(condition);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_querycondition_take, disposed_instance_state, .init=querycondition_init, .fini=querycondition_fini)
{
    dds_entity_t condition;
    dds_return_t ret;

    /* Create condition. */
    condition = dds_create_querycondition(g_reader, DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_NOT_ALIVE_DISPOSED_INSTANCE_STATE, filter_mod2);
    CU_ASSERT_FATAL(condition > 0);

    /* Take all disposed samples that match the filter. */
    ret = dds_take(condition, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
    CU_ASSERT_EQUAL_FATAL(ret, 1);
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
         * |    5   |    2   |    1   | not_read | new | no_writers |
         * |    6   |    3   |    2   | not_read | new | alive      |
         */

        /* Expected states. */
        int                  expected_long_1 = 4;
        dds_sample_state_t   expected_sst    = SAMPLE_SST(expected_long_1);
        dds_view_state_t     expected_vst    = SAMPLE_VST(expected_long_1);
        dds_instance_state_t expected_ist    = DDS_IST_NOT_ALIVE_DISPOSED;

        /* Check data. */
        CU_ASSERT_EQUAL_FATAL(sample->long_1, expected_long_1  );
        CU_ASSERT_EQUAL_FATAL(sample->long_2, expected_long_1/2);
        CU_ASSERT_EQUAL_FATAL(sample->long_3, expected_long_1/3);

        /* Check states. */
        CU_ASSERT_EQUAL_FATAL(g_info[i].valid_data,     true);
        CU_ASSERT_EQUAL_FATAL(g_info[i].sample_state,   expected_sst);
        CU_ASSERT_EQUAL_FATAL(g_info[i].view_state,     expected_vst);
        CU_ASSERT_EQUAL_FATAL(g_info[i].instance_state, expected_ist);
    }

    dds_delete(condition);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_querycondition_take, no_writers_instance_state, .init=querycondition_init, .fini=querycondition_fini)
{
    dds_entity_t condition;
    dds_return_t ret;

    /* Create condition. */
    condition = dds_create_querycondition(g_reader, DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_NOT_ALIVE_NO_WRITERS_INSTANCE_STATE, filter_mod2);
    CU_ASSERT_FATAL(condition > 0);

    /* Take all samples without a writer that match the filter. */
    ret = dds_take(condition, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
    CU_ASSERT_EQUAL_FATAL(ret, 1);
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
         * |    5   |    2   |    1   | not_read | new | no_writers |
         * |    6   |    3   |    2   | not_read | new | alive      |
         */

        /* Expected states. */
        int                  expected_long_1 = 2;
        dds_sample_state_t   expected_sst    = SAMPLE_SST(expected_long_1);
        dds_view_state_t     expected_vst    = SAMPLE_VST(expected_long_1);
        dds_instance_state_t expected_ist    = DDS_IST_NOT_ALIVE_NO_WRITERS;

        /* Check data. */
        CU_ASSERT_EQUAL_FATAL(sample->long_1, expected_long_1  );
        CU_ASSERT_EQUAL_FATAL(sample->long_2, expected_long_1/2);
        CU_ASSERT_EQUAL_FATAL(sample->long_3, expected_long_1/3);

        /* Check states. */
        CU_ASSERT_EQUAL_FATAL(g_info[i].valid_data,     true);
        CU_ASSERT_EQUAL_FATAL(g_info[i].sample_state,   expected_sst);
        CU_ASSERT_EQUAL_FATAL(g_info[i].view_state,     expected_vst);
        CU_ASSERT_EQUAL_FATAL(g_info[i].instance_state, expected_ist);
    }

    dds_delete(condition);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_querycondition_take, combination_of_states, .init=querycondition_init, .fini=querycondition_fini)
{
    dds_entity_t condition;
    dds_return_t ret;

    /* Create condition. */
    condition = dds_create_querycondition(g_reader, DDS_NOT_READ_SAMPLE_STATE | DDS_NEW_VIEW_STATE | DDS_ALIVE_INSTANCE_STATE, filter_mod2);
    CU_ASSERT_FATAL(condition > 0);

    /* Take all samples that match the mask and the filter. */
    ret = dds_take(condition, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
    CU_ASSERT_EQUAL_FATAL(ret, 1);
    for(int i = 0; i < ret; i++) {
        Space_Type1 *sample = (Space_Type1*)g_samples[i];

        /*
         * | long_1 | long_2 | long_3 |    sst   | vst |    ist     |
         * ----------------------------------------------------------
         * |    0   |    0   |    0   |     read | old | alive      |
         * |    1   |    0   |    0   |     read | old | disposed   |
         * |    2   |    1   |    0   |     read | old | no_writers |
         * |    3   |    1   |    1   | not_read | old | alive      |
         * |    4   |    2   |    1   | not_read | new | disposed   |
         * |    5   |    2   |    1   | not_read | new | no_writers |
         * |    6   |    3   |    2   | not_read | new | alive      | <---
         */

        /* Expected states. */
        int                  expected_long_1 = 6;
        dds_sample_state_t   expected_sst    = DDS_SST_NOT_READ;
        dds_view_state_t     expected_vst    = DDS_VST_NEW;
        dds_instance_state_t expected_ist    = DDS_IST_ALIVE;

        /* Check data. */
        CU_ASSERT_EQUAL_FATAL(sample->long_1, expected_long_1  );
        CU_ASSERT_EQUAL_FATAL(sample->long_2, expected_long_1/2);
        CU_ASSERT_EQUAL_FATAL(sample->long_3, expected_long_1/3);

        /* Check states. */
        CU_ASSERT_EQUAL_FATAL(g_info[i].valid_data,     true);
        CU_ASSERT_EQUAL_FATAL(g_info[i].sample_state,   expected_sst);
        CU_ASSERT_EQUAL_FATAL(g_info[i].view_state,     expected_vst);
        CU_ASSERT_EQUAL_FATAL(g_info[i].instance_state, expected_ist);
    }

    dds_delete(condition);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_querycondition_take, none, .init=querycondition_init, .fini=querycondition_fini)
{
    dds_entity_t condition;
    dds_return_t ret;

    /* Create condition. */
    condition = dds_create_querycondition(g_reader, DDS_NOT_READ_SAMPLE_STATE | DDS_NOT_NEW_VIEW_STATE | DDS_ALIVE_INSTANCE_STATE, filter_mod2);
    CU_ASSERT_FATAL(condition > 0);

    /* Take all samples that match the mask AND filter (should be none). */
    ret = dds_take(condition, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
    CU_ASSERT_EQUAL_FATAL(ret, 0);

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
CU_Test(ddsc_querycondition_take, with_mask, .init=querycondition_init, .fini=querycondition_fini)
{
    dds_entity_t condition;
    dds_return_t ret;

    /* Create condition. */
    condition = dds_create_querycondition(g_reader, DDS_NOT_READ_SAMPLE_STATE | DDS_NEW_VIEW_STATE | DDS_ALIVE_INSTANCE_STATE, filter_mod2);
    CU_ASSERT_FATAL(condition > 0);

    /* Take all samples that match the or'd masks and match the filter. */
    ret = dds_take_mask(condition, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES,
                        DDS_NOT_READ_SAMPLE_STATE | DDS_NOT_NEW_VIEW_STATE | DDS_NOT_ALIVE_DISPOSED_INSTANCE_STATE);
    CU_ASSERT_EQUAL(ret, 2);
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
         * |    5   |    2   |    1   | not_read | new | no_writers |
         * |    6   |    3   |    2   | not_read | new | alive      | <---
         */

        /* Expected states. */
        int                  expected_long_1 = (i == 0) ? 4 : 6;
        dds_sample_state_t   expected_sst    = DDS_SST_NOT_READ;
        dds_view_state_t     expected_vst    = SAMPLE_VST(expected_long_1);
        dds_instance_state_t expected_ist    = SAMPLE_IST(expected_long_1);

        /* Check data. */
        CU_ASSERT_EQUAL_FATAL(sample->long_1, expected_long_1  );
        CU_ASSERT_EQUAL_FATAL(sample->long_2, expected_long_1/2);
        CU_ASSERT_EQUAL_FATAL(sample->long_3, expected_long_1/3);

        /* Check states. */
        CU_ASSERT_EQUAL_FATAL(g_info[i].valid_data,     true);
        CU_ASSERT_EQUAL_FATAL(g_info[i].sample_state,   expected_sst);
        CU_ASSERT_EQUAL_FATAL(g_info[i].view_state,     expected_vst);
        CU_ASSERT_EQUAL_FATAL(g_info[i].instance_state, expected_ist);
    }

    dds_delete(condition);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_querycondition_take, already_deleted, .init=querycondition_init, .fini=querycondition_fini)
{
    dds_entity_t condition;
    dds_return_t ret;

    /* Create condition. */
    condition = dds_create_querycondition(g_reader, DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE, filter_mod2);
    CU_ASSERT_FATAL(condition > 0);

    /* Delete condition. */
    ret = dds_delete(condition);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    /* Try to take with a deleted condition. */
    ret = dds_take(condition, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
    CU_ASSERT_EQUAL(ret, DDS_RETCODE_BAD_PARAMETER);
}
/*************************************************************************************************/

/*************************************************************************************************/
static bool
filter_k333_s1(const void * sample)
{
    const Space_Type1 *s = sample;
    return (s->long_1 == 333 && s->long_2 == 1);
}

CU_Test(ddsc_querycondition_take, some_from_instance, .init=querycondition_init_deephist, .fini=querycondition_fini)
{
    static const int sched[] = { 1, 0, 1, 0, 1, 1, 0, 1, 1, 1 };
    static const int nsched = (int) (sizeof (sched) / sizeof (sched[0]));
    dds_entity_t condition;
    dds_return_t ret;
    int idx, run;

    condition = dds_create_querycondition (g_reader, DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE, filter_k333_s1);
    CU_ASSERT_FATAL (condition > 0);

    for (int i = 0; i < nsched; i++) {
        const Space_Type1 sample = { 333, sched[i], i };
        ret = dds_write (g_writer, &sample);
        CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);
    }

    /* Try taking consecutive runs of ones, so that we take first, intermediate and final ones */
    idx = 0;
    while (idx < nsched) {
        CU_ASSERT_FATAL (sched[idx] == 1);
        run = 1;
        while (idx + run < nsched && sched[idx + run] == 1) {
            run++;
        }
        ret = dds_take (condition, g_samples, g_info, (size_t)run, (uint32_t)run);
        CU_ASSERT_EQUAL (ret, run);
        for (int i = 0; i < ret; i++) {
            CU_ASSERT_EQUAL (g_data[i].long_1, 333);
            CU_ASSERT_EQUAL (g_data[i].long_2, 1);
            CU_ASSERT_EQUAL (g_data[i].long_3, idx + i);
        }
        idx += run;
        while (idx < nsched && sched[idx] != 1) {
            idx++;
        }
    }

    /* Take all remaining samples from the instance */
    for (idx = 0, run = 0; idx < nsched; idx++) {
        run += (sched[idx] != 1);
    }
    CU_ASSERT_FATAL (run > 0);
    ret = dds_take_instance (g_reader, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES, g_info[0].instance_handle);
    CU_ASSERT_EQUAL (ret, run);
    idx = 0;
    while (sched[idx] != 0) {
        idx++;
    }
    for (int i = 0; i < ret; i++) {
        CU_ASSERT_EQUAL (g_data[i].long_1, 333);
        CU_ASSERT_NOT_EQUAL (g_data[i].long_2, 1);
        CU_ASSERT_EQUAL (g_data[i].long_3, idx);
        idx++;
        while (idx < nsched && sched[idx] == 1) {
            idx++;
        }
    }

    ret = dds_delete(condition);
    CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);
}
/*************************************************************************************************/
