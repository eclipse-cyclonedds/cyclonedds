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
 * data will be available in the reader history.
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
#define SAMPLE_LAST_READ_SST      (2)
#define SAMPLE_LAST_OLD_VST       (3)
#define SAMPLE_IST(idx)           (((idx % 3) == 0) ? DDS_IST_ALIVE              : \
                                   ((idx % 3) == 1) ? DDS_IST_NOT_ALIVE_DISPOSED : \
                                                      DDS_IST_NOT_ALIVE_NO_WRITERS )
#define SAMPLE_VST(idx)           ((idx <= SAMPLE_LAST_OLD_VST ) ? DDS_VST_OLD  : DDS_VST_NEW)
#define SAMPLE_SST(idx)           ((idx <= SAMPLE_LAST_READ_SST) ? DDS_SST_READ : DDS_SST_NOT_READ)

static dds_entity_t g_participant = 0;
static dds_entity_t g_subscriber  = 0;
static dds_entity_t g_topic       = 0;
static dds_entity_t g_reader      = 0;
static dds_entity_t g_writer      = 0;
static dds_entity_t g_waitset     = 0;
static dds_qos_t    *g_qos        = NULL;
static dds_qos_t    *g_qos_null   = NULL;
static dds_listener_t *g_listener = NULL;
static dds_listener_t *g_list_null= NULL;

static void*              g_loans[MAX_SAMPLES];
static void*              g_samples[MAX_SAMPLES];
static Space_Type1        g_data[MAX_SAMPLES];
static dds_sample_info_t  g_info[MAX_SAMPLES];

static void
reader_init(void)
{
    Space_Type1 sample = { 0, 0, 0 };
    dds_attach_t triggered;
    dds_return_t ret;
    char name[100];

    g_qos = dds_create_qos();
    g_listener = dds_create_listener(NULL);

    g_participant = dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);
    CU_ASSERT_FATAL(g_participant > 0);

    g_subscriber = dds_create_subscriber(g_participant, NULL, NULL);
    CU_ASSERT_FATAL(g_subscriber > 0);

    g_waitset = dds_create_waitset(g_participant);
    CU_ASSERT_FATAL(g_waitset > 0);

    g_topic = dds_create_topic(g_participant, &Space_Type1_desc, create_unique_topic_name("ddsc_reader_test", name, sizeof name), NULL, NULL);
    CU_ASSERT_FATAL(g_topic > 0);

    /* Create a reader that keeps last sample of all instances. */
    dds_qset_history(g_qos, DDS_HISTORY_KEEP_LAST, 1);
    g_reader = dds_create_reader(g_participant, g_topic, g_qos, NULL);
    CU_ASSERT_FATAL(g_reader > 0);

    /* Create a reader that will not automatically dispose unregistered samples. */
    dds_qset_writer_data_lifecycle(g_qos, false);
    g_writer = dds_create_writer(g_participant, g_topic, g_qos, NULL);
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
    for (int i = 0; i < MAX_SAMPLES; i++) {
        g_loans[i] = NULL;
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
    for(int i = 0; i < ret; i++) {
        Space_Type1 *s = (Space_Type1*)g_samples[i];
        (void)s;
    }

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

}

static void
reader_fini(void)
{
    dds_delete_qos(g_qos);
    dds_delete_listener(g_listener);
    dds_delete(g_reader);
    dds_delete(g_writer);
    dds_delete(g_waitset);
    dds_delete(g_topic);
    dds_delete(g_participant);
}

static dds_return_t
samples_cnt(void)
{
    dds_return_t ret;
    ret = dds_read(g_reader, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
    CU_ASSERT_FATAL(ret >= 0);
    return ret;
}

/**************************************************************************************************
 *
 * These will check the reader creation in various ways.
 *
 *************************************************************************************************/
/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_reader_create, valid) = {
        CU_DataPoints(dds_entity_t*,    &g_subscriber, &g_participant),
        CU_DataPoints(dds_qos_t**,      &g_qos_null,   &g_qos        ),
        CU_DataPoints(dds_listener_t**, &g_list_null,  &g_listener   ),
};
CU_Theory((dds_entity_t *ent, dds_qos_t **qos, dds_listener_t **listener), ddsc_reader_create, valid, .init=reader_init, .fini=reader_fini)
{
    dds_entity_t rdr;
    dds_return_t ret;
    rdr = dds_create_reader(*ent, g_topic, *qos, *listener);
    CU_ASSERT_FATAL(rdr > 0);
    ret = dds_delete(rdr);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_reader_create, invalid_qos_participant, .init=reader_init, .fini=reader_fini)
{
    dds_entity_t rdr;
    dds_qos_t *qos = dds_create_qos();
    /* Set invalid reader data lifecycle policy */
    DDSRT_WARNING_MSVC_OFF(28020); /* Disable SAL warning on intentional misuse of the API */
    dds_qset_reader_data_lifecycle(qos, DDS_SECS(-1), DDS_SECS(-1));
    DDSRT_WARNING_MSVC_ON(28020);
    rdr = dds_create_reader(g_participant, g_topic, qos, NULL);
    CU_ASSERT_EQUAL_FATAL(rdr, DDS_RETCODE_BAD_PARAMETER);
    dds_delete_qos(qos);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_reader_create, invalid_qos_subscriber, .init=reader_init, .fini=reader_fini)
{
    dds_entity_t rdr;
    dds_qos_t *qos = dds_create_qos();
    /* Set invalid reader data lifecycle policy */
    DDSRT_WARNING_MSVC_OFF(28020); /* Disable SAL warning on intentional misuse of the API */
    dds_qset_reader_data_lifecycle(qos, DDS_SECS(-1), DDS_SECS(-1));
    DDSRT_WARNING_MSVC_ON(28020);
    rdr = dds_create_reader(g_subscriber, g_topic, qos, NULL);
    CU_ASSERT_EQUAL_FATAL(rdr, DDS_RETCODE_BAD_PARAMETER);
    dds_delete_qos(qos);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_reader_create, non_participants_non_topics) = {
        CU_DataPoints(dds_entity_t*, &g_participant, &g_topic, &g_writer, &g_reader, &g_waitset),
        CU_DataPoints(dds_entity_t*, &g_participant, &g_topic, &g_writer, &g_reader, &g_waitset),
};
CU_Theory((dds_entity_t *par, dds_entity_t *top), ddsc_reader_create, non_participants_non_topics, .init=reader_init, .fini=reader_fini)
{
    dds_entity_t rdr;
    /* The only valid permutation is when par is actual the participant and top is
     * actually the topic. So, don't test that permutation. */
    CU_ASSERT_FATAL((par != &g_participant) || (top != &g_topic));
    rdr = dds_create_reader(*par, *top, NULL, NULL);
    CU_ASSERT_EQUAL_FATAL(rdr, DDS_RETCODE_ILLEGAL_OPERATION);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_reader_create, wrong_participant, .init=reader_init, .fini=reader_fini)
{
    dds_entity_t participant2 = dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);
    CU_ASSERT_FATAL(participant2 > 0);
    dds_entity_t reader = dds_create_reader(participant2, g_topic, NULL, NULL);
    CU_ASSERT_EQUAL_FATAL(reader, DDS_RETCODE_BAD_PARAMETER);
    dds_delete(participant2);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_reader_create, participant_mismatch)
{
    dds_entity_t par1 = 0;
    dds_entity_t par2 = 0;
    dds_entity_t sub1 = 0;
    dds_entity_t top2 = 0;
    dds_entity_t reader = 0;
    char name[100];

    par1 = dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);
    CU_ASSERT_FATAL(par1 > 0);
    par2 = dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);
    CU_ASSERT_FATAL(par2 > 0);

    sub1 = dds_create_subscriber(par1, NULL, NULL);
    CU_ASSERT_FATAL(sub1 > 0);

    top2 = dds_create_topic(par2, &Space_Type1_desc, create_unique_topic_name("ddsc_reader_participant_mismatch", name, sizeof name), NULL, NULL);
    CU_ASSERT_FATAL(top2 > 0);

    /* Create reader with participant mismatch. */
    reader = dds_create_reader(sub1, top2, NULL, NULL);

    /* Expect the creation to have failed. */
    CU_ASSERT_FATAL(reader <= 0);

    dds_delete(top2);
    dds_delete(sub1);
    dds_delete(par2);
    dds_delete(par1);
}
/*************************************************************************************************/



/**************************************************************************************************
 *
 * These will check the read in various ways.
 *
 *************************************************************************************************/
/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_read, invalid_buffers) = {
        CU_DataPoints(void**,             g_samples, g_loans, (void**)0),
        CU_DataPoints(dds_sample_info_t*, g_info,    NULL,    NULL),
        CU_DataPoints(size_t,             0,         3,       MAX_SAMPLES),
        CU_DataPoints(uint32_t,           0,         3,       MAX_SAMPLES),
};
CU_Theory((void **buf, dds_sample_info_t *si, size_t bufsz, uint32_t maxs), ddsc_read, invalid_buffers, .init=reader_init, .fini=reader_fini)
{
    dds_return_t ret;
    /* The only valid permutation is when non of the buffer values are
     * invalid. So, don't test that. */
    CU_ASSERT_FATAL((buf != g_samples) || (si != g_info) || (bufsz == 0) || (maxs == 0) || (bufsz < maxs));
    /* TODO: CHAM-306, currently, a buffer is automatically 'promoted' to a loan when a buffer is
     * provided with NULL pointers. So, in fact, there's currently no real difference between calling
     * dds_read() dds_read_wl() (except for the provided bufsz). This will change, which means that
     * the given buffer should contain valid pointers, which again means that 'loan intended' buffer
     * should result in bad_parameter.
     * However, that's not the case yet. So don't test it. */
    if (buf != g_loans) {
        ret = dds_read(g_reader, buf, si, bufsz, maxs);
        CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
    } else {
        CU_PASS("Skipped");
    }
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_read, invalid_readers) = {
        CU_DataPoints(dds_entity_t, -2, -1, 0, INT_MAX, INT_MIN),
};
CU_Theory((dds_entity_t rdr), ddsc_read, invalid_readers, .init=reader_init, .fini=reader_fini)
{
    dds_return_t ret;

    ret = dds_read(rdr, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_read, non_readers) = {
        CU_DataPoints(dds_entity_t*, &g_participant, &g_topic, &g_writer, &g_subscriber, &g_waitset),
};
CU_Theory((dds_entity_t *rdr), ddsc_read, non_readers, .init=reader_init, .fini=reader_fini)
{
    dds_return_t ret;
    ret = dds_read(*rdr, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_ILLEGAL_OPERATION);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_read, already_deleted, .init=reader_init, .fini=reader_fini)
{
    dds_return_t ret;
    /* Try to read with a deleted reader. */
    dds_delete(g_reader);
    ret = dds_read(g_reader, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
    CU_ASSERT_EQUAL(ret, DDS_RETCODE_BAD_PARAMETER);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_read, valid, .init=reader_init, .fini=reader_fini)
{
    dds_return_t ret;

    ret = dds_read(g_reader, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
    CU_ASSERT_EQUAL_FATAL(ret, MAX_SAMPLES);
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

        /* Expected states. */
        int                  expected_long_1 = i;
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

    /* All samples should still be available. */
    ret = samples_cnt();
    CU_ASSERT_EQUAL_FATAL(ret, MAX_SAMPLES);
}
/*************************************************************************************************/






/**************************************************************************************************
 *
 * These will check the read_wl in various ways.
 *
 *************************************************************************************************/
/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_read_wl, invalid_buffers) = {
        CU_DataPoints(void**,             g_loans, (void**)0),
        CU_DataPoints(dds_sample_info_t*, g_info,  (dds_sample_info_t*)0   ),
        CU_DataPoints(uint32_t,           0,        3,  MAX_SAMPLES   ),
};
CU_Theory((void **buf, dds_sample_info_t *si, uint32_t maxs), ddsc_read_wl, invalid_buffers, .init=reader_init, .fini=reader_fini)
{
    dds_return_t ret;
    /* The only valid permutation is when non of the buffer values are
     * invalid. So, don't test that. */
    CU_ASSERT_FATAL((buf != g_loans) || (si != g_info) || (maxs == 0));
    ret = dds_read_wl(g_reader, buf, si, maxs);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_read_wl, invalid_readers) = {
        CU_DataPoints(dds_entity_t, -2, -1, 0, INT_MAX, INT_MIN),
};
CU_Theory((dds_entity_t rdr), ddsc_read_wl, invalid_readers, .init=reader_init, .fini=reader_fini)
{
    dds_return_t ret;

    ret = dds_read_wl(rdr, g_loans, g_info, MAX_SAMPLES);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_read_wl, non_readers) = {
        CU_DataPoints(dds_entity_t*, &g_participant, &g_topic, &g_writer, &g_subscriber, &g_waitset),
};
CU_Theory((dds_entity_t *rdr), ddsc_read_wl, non_readers, .init=reader_init, .fini=reader_fini)
{
    dds_return_t ret;
    ret = dds_read_wl(*rdr, g_loans, g_info, MAX_SAMPLES);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_ILLEGAL_OPERATION);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_read_wl, already_deleted, .init=reader_init, .fini=reader_fini)
{
    dds_return_t ret;
    /* Try to read with a deleted reader. */
    dds_delete(g_reader);
    ret = dds_read_wl(g_reader, g_loans, g_info, MAX_SAMPLES);
    CU_ASSERT_EQUAL(ret, DDS_RETCODE_BAD_PARAMETER);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_read_wl, valid, .init=reader_init, .fini=reader_fini)
{
    dds_return_t ret;

    ret = dds_read_wl(g_reader, g_loans, g_info, MAX_SAMPLES);
    CU_ASSERT_EQUAL_FATAL(ret, MAX_SAMPLES);
    for(int i = 0; i < ret; i++) {
        Space_Type1 *sample = (Space_Type1*)g_loans[i];

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

        /* Expected states. */
        int                  expected_long_1 = i;
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

    ret = dds_return_loan(g_reader, g_loans, ret);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    /* All samples should still be available. */
    ret = samples_cnt();
    CU_ASSERT_EQUAL_FATAL(ret, MAX_SAMPLES);
}
/*************************************************************************************************/





/**************************************************************************************************
 *
 * These will check the read_mask in various ways.
 *
 *************************************************************************************************/
/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_read_mask, invalid_buffers) = {
    CU_DataPoints(void**,             g_samples, g_loans, (void**)0),
    CU_DataPoints(dds_sample_info_t*, g_info,    NULL,    NULL),
    CU_DataPoints(size_t,             0,         3,       MAX_SAMPLES),
    CU_DataPoints(uint32_t,           0,         3,       MAX_SAMPLES),
};
CU_Theory((void **buf, dds_sample_info_t *si, size_t bufsz, uint32_t maxs), ddsc_read_mask, invalid_buffers, .init=reader_init, .fini=reader_fini)
{
    uint32_t mask = DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
    dds_return_t ret;
    /* The only valid permutation is when non of the buffer values are
     * invalid. So, don't test that. */
    CU_ASSERT_FATAL((buf != g_samples) || (si != g_info) || (bufsz == 0) || (maxs == 0) || (bufsz < maxs));
    /* TODO: CHAM-306, currently, a buffer is automatically 'promoted' to a loan when a buffer is
     * provided with NULL pointers. So, in fact, there's currently no real difference between calling
     * dds_read_mask() dds_read_mask_wl() (except for the provided bufsz). This will change, which means that
     * the given buffer should contain valid pointers, which again means that 'loan intended' buffer
     * should result in bad_parameter.
     * However, that's not the case yet. So don't test it. */
    if (buf != g_loans) {
        ret = dds_read_mask(g_reader, buf, si, bufsz, maxs, mask);
        CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
    } else {
        CU_PASS("Skipped");
    }
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_read_mask, invalid_readers) = {
        CU_DataPoints(dds_entity_t, -2, -1, 0, INT_MAX, INT_MIN),
};
CU_Theory((dds_entity_t rdr), ddsc_read_mask, invalid_readers, .init=reader_init, .fini=reader_fini)
{
    uint32_t mask = DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
    dds_return_t ret;

    ret = dds_read_mask(rdr, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES, mask);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_read_mask, non_readers) = {
        CU_DataPoints(dds_entity_t*, &g_participant, &g_topic, &g_writer, &g_subscriber, &g_waitset),
};
CU_Theory((dds_entity_t *rdr), ddsc_read_mask, non_readers, .init=reader_init, .fini=reader_fini)
{
    uint32_t mask = DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
    dds_return_t ret;
    ret = dds_read_mask(*rdr, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES, mask);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_ILLEGAL_OPERATION);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_read_mask, already_deleted, .init=reader_init, .fini=reader_fini)
{
    uint32_t mask = DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
    dds_return_t ret;
    /* Try to read with a deleted reader. */
    dds_delete(g_reader);
    ret = dds_read_mask(g_reader, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES, mask);
    CU_ASSERT_EQUAL(ret, DDS_RETCODE_BAD_PARAMETER);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_read_mask, any, .init=reader_init, .fini=reader_fini)
{
    uint32_t mask = DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
    dds_return_t ret;

    /* Read all samples that matches the mask. */
    ret = dds_read_mask(g_reader, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES, mask);
    CU_ASSERT_EQUAL_FATAL(ret, MAX_SAMPLES);
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

        /* Expected states. */
        int                  expected_long_1 = i;
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

    /* All samples should still be available. */
    ret = samples_cnt();
    CU_ASSERT_EQUAL_FATAL(ret, MAX_SAMPLES);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_read_mask, not_read_sample_state, .init=reader_init, .fini=reader_fini)
{
    uint32_t mask = DDS_NOT_READ_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
    dds_return_t ret;

    /* Read all samples that matches the mask. */
    ret = dds_read_mask(g_reader, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES, mask);
    CU_ASSERT_EQUAL_FATAL(ret, MAX_SAMPLES - (SAMPLE_LAST_READ_SST + 1));
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

        /* Expected states. */
        int                  expected_long_1 = SAMPLE_LAST_READ_SST + 1 + i;
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

    /* All samples should still be available. */
    ret = samples_cnt();
    CU_ASSERT_EQUAL_FATAL(ret, MAX_SAMPLES);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_read_mask, read_sample_state, .init=reader_init, .fini=reader_fini)
{
    uint32_t mask = DDS_READ_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
    dds_return_t ret;

    /* Read all samples that matches the mask. */
    ret = dds_read_mask(g_reader, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES, mask);
    CU_ASSERT_EQUAL_FATAL(ret, SAMPLE_LAST_READ_SST + 1);
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

        /* Expected states. */
        int                  expected_long_1 = i;
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

    /* All samples should still be available. */
    ret = samples_cnt();
    CU_ASSERT_EQUAL_FATAL(ret, MAX_SAMPLES);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_read_mask, new_view_state, .init=reader_init, .fini=reader_fini)
{
    uint32_t mask = DDS_ANY_SAMPLE_STATE | DDS_NEW_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
    dds_return_t ret;

    /* Read all samples that matches the mask. */
    ret = dds_read_mask(g_reader, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES, mask);
    CU_ASSERT_EQUAL_FATAL(ret, MAX_SAMPLES - (SAMPLE_LAST_OLD_VST + 1));
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

        /* Expected states. */
        int                  expected_long_1 = SAMPLE_LAST_OLD_VST + 1 + i;
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

    /* All samples should still be available. */
    ret = samples_cnt();
    CU_ASSERT_EQUAL_FATAL(ret, MAX_SAMPLES);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_read_mask, not_new_view_state, .init=reader_init, .fini=reader_fini)
{
    uint32_t mask = DDS_ANY_SAMPLE_STATE | DDS_NOT_NEW_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
    dds_return_t ret;

    /* Read all samples that matches the mask. */
    ret = dds_read_mask(g_reader, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES, mask);
    CU_ASSERT_EQUAL_FATAL(ret, SAMPLE_LAST_OLD_VST + 1);
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

        /* Expected states. */
        int                  expected_long_1 = i;
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

    /* All samples should still be available. */
    ret = samples_cnt();
    CU_ASSERT_EQUAL_FATAL(ret, MAX_SAMPLES);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_read_mask, alive_instance_state, .init=reader_init, .fini=reader_fini)
{
    uint32_t mask = DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ALIVE_INSTANCE_STATE;
    dds_return_t ret;

    /* Read all samples that matches the mask. */
    ret = dds_read_mask(g_reader, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES, mask);
    CU_ASSERT_EQUAL_FATAL(ret, 3);
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

        /* Expected states. */
        int                  expected_long_1 = i * 3;
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

    /* All samples should still be available. */
    ret = samples_cnt();
    CU_ASSERT_EQUAL_FATAL(ret, MAX_SAMPLES);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_read_mask, not_alive_instance_state, .init=reader_init, .fini=reader_fini)
{
    uint32_t mask = DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_NOT_ALIVE_DISPOSED_INSTANCE_STATE | DDS_NOT_ALIVE_NO_WRITERS_INSTANCE_STATE;
    dds_return_t ret;

    /* Read all samples that matches the mask. */
    ret = dds_read_mask(g_reader, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES, mask);
    CU_ASSERT_EQUAL_FATAL(ret, 4);
    for(int i = 0; i < ret; i++) {
        Space_Type1 *sample = (Space_Type1*)g_samples[i];

        /*
         * | long_1 | long_2 | long_3 |    sst   | vst |    ist     |
         * ----------------------------------------------------------
         * |    0   |    0   |    0   |     read | old | alive      |
         * |    1   |    0   |    0   |     read | old | disposed   | <---
         * |    2   |    1   |    0   |     read | old | no_writers | <---
         * |    3   |    1   |    1   | not_read | old | alive      |
         * |    4   |    2   |    1   | not_read | new | disposed   | <---
         * |    5   |    2   |    1   | not_read | new | no_writers | <---
         * |    6   |    3   |    2   | not_read | new | alive      |
         */

        /* Expected states. */
        int                  expected_long_1 = (i <= 1) ? i + 1 : i + 2;
        dds_sample_state_t   expected_sst    = SAMPLE_SST(expected_long_1);
        dds_view_state_t     expected_vst    = SAMPLE_VST(expected_long_1);
        dds_instance_state_t expected_ist    = SAMPLE_IST(expected_long_1);

        /* Check data. */
        CU_ASSERT_EQUAL_FATAL(sample->long_1, expected_long_1  );
        CU_ASSERT_EQUAL_FATAL(sample->long_2, expected_long_1/2);
        CU_ASSERT_EQUAL_FATAL(sample->long_3, expected_long_1/3);

        /* Check states. */
        CU_ASSERT_EQUAL_FATAL(g_info[i].valid_data,      true);
        CU_ASSERT_EQUAL_FATAL(g_info[i].sample_state,    expected_sst);
        CU_ASSERT_EQUAL_FATAL(g_info[i].view_state,      expected_vst);
        CU_ASSERT_EQUAL_FATAL(g_info[i].instance_state,  expected_ist);
        CU_ASSERT_NOT_EQUAL_FATAL(g_info[i].instance_state, DDS_IST_ALIVE);
    }

    /* All samples should still be available. */
    ret = samples_cnt();
    CU_ASSERT_EQUAL_FATAL(ret, MAX_SAMPLES);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_read_mask, disposed_instance_state, .init=reader_init, .fini=reader_fini)
{
    uint32_t mask = DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_NOT_ALIVE_DISPOSED_INSTANCE_STATE;
    dds_return_t ret;

    /* Read all samples that matches the mask. */
    ret = dds_read_mask(g_reader, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES, mask);
    CU_ASSERT_EQUAL_FATAL(ret, 2);
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

        /* Expected states. */
        int                  expected_long_1 = (i * 3) + 1;
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

    /* All samples should still be available. */
    ret = samples_cnt();
    CU_ASSERT_EQUAL_FATAL(ret, MAX_SAMPLES);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_read_mask, no_writers_instance_state, .init=reader_init, .fini=reader_fini)
{
    uint32_t mask = DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_NOT_ALIVE_NO_WRITERS_INSTANCE_STATE;
    dds_return_t ret;

    /* Read all samples that matches the mask. */
    ret = dds_read_mask(g_reader, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES, mask);
    CU_ASSERT_EQUAL_FATAL(ret, 2);
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

        /* Expected states. */
        int                  expected_long_1 = (i * 3) + 2;
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

    /* All samples should still be available. */
    ret = samples_cnt();
    CU_ASSERT_EQUAL_FATAL(ret, MAX_SAMPLES);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_read_mask, combination_of_states, .init=reader_init, .fini=reader_fini)
{
    uint32_t mask = DDS_NOT_READ_SAMPLE_STATE | DDS_NOT_NEW_VIEW_STATE | DDS_ALIVE_INSTANCE_STATE;
    dds_return_t ret;

    /* Read all samples that matches the mask. */
    ret = dds_read_mask(g_reader, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES, mask);
    CU_ASSERT_EQUAL_FATAL(ret, 1);
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

        /* Expected states. */
        int                  expected_long_1 = 3;
        dds_sample_state_t   expected_sst    = DDS_SST_NOT_READ;
        dds_view_state_t     expected_vst    = DDS_VST_OLD;
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

    /* All samples should still be available. */
    ret = samples_cnt();
    CU_ASSERT_EQUAL_FATAL(ret, MAX_SAMPLES);
}
/*************************************************************************************************/





/**************************************************************************************************
 *
 * These will check the read_mask_wl in various ways.
 *
 *************************************************************************************************/
/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_read_mask_wl, invalid_buffers) = {
        CU_DataPoints(void**,             g_loans, (void**)0),
        CU_DataPoints(dds_sample_info_t*, g_info,  (dds_sample_info_t*)0   ),
        CU_DataPoints(uint32_t,           0,        3,  MAX_SAMPLES   ),
};
CU_Theory((void **buf, dds_sample_info_t *si, uint32_t maxs), ddsc_read_mask_wl, invalid_buffers, .init=reader_init, .fini=reader_fini)
{
    uint32_t mask = DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
    dds_return_t ret;
    /* The only valid permutation is when non of the buffer values are
     * invalid. So, don't test that. */
    CU_ASSERT_FATAL((buf != g_loans) || (si != g_info) || (maxs == 0));
    ret = dds_read_mask_wl(g_reader, buf, si, maxs, mask);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_read_mask_wl, invalid_readers) = {
        CU_DataPoints(dds_entity_t, -2, -1, 0, INT_MAX, INT_MIN),
};
CU_Theory((dds_entity_t rdr), ddsc_read_mask_wl, invalid_readers, .init=reader_init, .fini=reader_fini)
{
    uint32_t mask = DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
    dds_return_t ret;

    ret = dds_read_mask_wl(rdr, g_loans, g_info, MAX_SAMPLES, mask);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_read_mask_wl, non_readers) = {
        CU_DataPoints(dds_entity_t*, &g_participant, &g_topic, &g_writer, &g_subscriber, &g_waitset),
};
CU_Theory((dds_entity_t *rdr), ddsc_read_mask_wl, non_readers, .init=reader_init, .fini=reader_fini)
{
    uint32_t mask = DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
    dds_return_t ret;
    ret = dds_read_mask_wl(*rdr, g_loans, g_info, MAX_SAMPLES, mask);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_ILLEGAL_OPERATION);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_read_mask_wl, already_deleted, .init=reader_init, .fini=reader_fini)
{
    uint32_t mask = DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
    dds_return_t ret;
    /* Try to read with a deleted reader. */
    dds_delete(g_reader);
    ret = dds_read_mask_wl(g_reader, g_loans, g_info, MAX_SAMPLES, mask);
    CU_ASSERT_EQUAL(ret, DDS_RETCODE_BAD_PARAMETER);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_read_mask_wl, any, .init=reader_init, .fini=reader_fini)
{
    uint32_t mask = DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
    dds_return_t ret;

    /* Read all samples that matches the mask. */
    ret = dds_read_mask_wl(g_reader, g_loans, g_info, MAX_SAMPLES, mask);
    CU_ASSERT_EQUAL_FATAL(ret, MAX_SAMPLES);
    for(int i = 0; i < ret; i++) {
        Space_Type1 *sample = (Space_Type1*)g_loans[i];

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

        /* Expected states. */
        int                  expected_long_1 = i;
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

    ret = dds_return_loan(g_reader, g_loans, ret);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    /* All samples should still be available. */
    ret = samples_cnt();
    CU_ASSERT_EQUAL_FATAL(ret, MAX_SAMPLES);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_read_mask_wl, not_read_sample_state, .init=reader_init, .fini=reader_fini)
{
    uint32_t mask = DDS_NOT_READ_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
    dds_return_t ret;

    /* Read all samples that matches the mask. */
    ret = dds_read_mask_wl(g_reader, g_loans, g_info, MAX_SAMPLES, mask);
    CU_ASSERT_EQUAL_FATAL(ret, MAX_SAMPLES - (SAMPLE_LAST_READ_SST + 1));
    for(int i = 0; i < ret; i++) {
        Space_Type1 *sample = (Space_Type1*)g_loans[i];

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

        /* Expected states. */
        int                  expected_long_1 = SAMPLE_LAST_READ_SST + 1 + i;
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

    ret = dds_return_loan(g_reader, g_loans, ret);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    /* All samples should still be available. */
    ret = samples_cnt();
    CU_ASSERT_EQUAL_FATAL(ret, MAX_SAMPLES);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_read_mask_wl, read_sample_state, .init=reader_init, .fini=reader_fini)
{
    uint32_t mask = DDS_READ_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
    dds_return_t ret;

    /* Read all samples that matches the mask. */
    ret = dds_read_mask_wl(g_reader, g_loans, g_info, MAX_SAMPLES, mask);
    CU_ASSERT_EQUAL_FATAL(ret, SAMPLE_LAST_READ_SST + 1);
    for(int i = 0; i < ret; i++) {
        Space_Type1 *sample = (Space_Type1*)g_loans[i];

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

        /* Expected states. */
        int                  expected_long_1 = i;
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

    ret = dds_return_loan(g_reader, g_loans, ret);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    /* All samples should still be available. */
    ret = samples_cnt();
    CU_ASSERT_EQUAL_FATAL(ret, MAX_SAMPLES);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_read_mask_wl, new_view_state, .init=reader_init, .fini=reader_fini)
{
    uint32_t mask = DDS_ANY_SAMPLE_STATE | DDS_NEW_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
    dds_return_t ret;

    /* Read all samples that matches the mask. */
    ret = dds_read_mask_wl(g_reader, g_loans, g_info, MAX_SAMPLES, mask);
    CU_ASSERT_EQUAL_FATAL(ret, MAX_SAMPLES - (SAMPLE_LAST_OLD_VST + 1));
    for(int i = 0; i < ret; i++) {
        Space_Type1 *sample = (Space_Type1*)g_loans[i];

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

        /* Expected states. */
        int                  expected_long_1 = SAMPLE_LAST_OLD_VST + 1 + i;
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

    ret = dds_return_loan(g_reader, g_loans, ret);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    /* All samples should still be available. */
    ret = samples_cnt();
    CU_ASSERT_EQUAL_FATAL(ret, MAX_SAMPLES);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_read_mask_wl, not_new_view_state, .init=reader_init, .fini=reader_fini)
{
    uint32_t mask = DDS_ANY_SAMPLE_STATE | DDS_NOT_NEW_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
    dds_return_t ret;

    /* Read all samples that matches the mask. */
    ret = dds_read_mask_wl(g_reader, g_loans, g_info, MAX_SAMPLES, mask);
    CU_ASSERT_EQUAL_FATAL(ret, SAMPLE_LAST_OLD_VST + 1);
    for(int i = 0; i < ret; i++) {
        Space_Type1 *sample = (Space_Type1*)g_loans[i];

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

        /* Expected states. */
        int                  expected_long_1 = i;
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

    ret = dds_return_loan(g_reader, g_loans, ret);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    /* All samples should still be available. */
    ret = samples_cnt();
    CU_ASSERT_EQUAL_FATAL(ret, MAX_SAMPLES);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_read_mask_wl, alive_instance_state, .init=reader_init, .fini=reader_fini)
{
    uint32_t mask = DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ALIVE_INSTANCE_STATE;
    dds_return_t ret;

    /* Read all samples that matches the mask. */
    ret = dds_read_mask_wl(g_reader, g_loans, g_info, MAX_SAMPLES, mask);
    CU_ASSERT_EQUAL_FATAL(ret, 3);
    for(int i = 0; i < ret; i++) {
        Space_Type1 *sample = (Space_Type1*)g_loans[i];

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

        /* Expected states. */
        int                  expected_long_1 = i * 3;
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

    ret = dds_return_loan(g_reader, g_loans, ret);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    /* All samples should still be available. */
    ret = samples_cnt();
    CU_ASSERT_EQUAL_FATAL(ret, MAX_SAMPLES);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_read_mask_wl, not_alive_instance_state, .init=reader_init, .fini=reader_fini)
{
    uint32_t mask = DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_NOT_ALIVE_DISPOSED_INSTANCE_STATE | DDS_NOT_ALIVE_NO_WRITERS_INSTANCE_STATE;
    dds_return_t ret;

    /* Read all samples that matches the mask. */
    ret = dds_read_mask_wl(g_reader, g_loans, g_info, MAX_SAMPLES, mask);
    CU_ASSERT_EQUAL_FATAL(ret, 4);
    for(int i = 0; i < ret; i++) {
        Space_Type1 *sample = (Space_Type1*)g_loans[i];

        /*
         * | long_1 | long_2 | long_3 |    sst   | vst |    ist     |
         * ----------------------------------------------------------
         * |    0   |    0   |    0   |     read | old | alive      |
         * |    1   |    0   |    0   |     read | old | disposed   | <---
         * |    2   |    1   |    0   |     read | old | no_writers | <---
         * |    3   |    1   |    1   | not_read | old | alive      |
         * |    4   |    2   |    1   | not_read | new | disposed   | <---
         * |    5   |    2   |    1   | not_read | new | no_writers | <---
         * |    6   |    3   |    2   | not_read | new | alive      |
         */

        /* Expected states. */
        int                  expected_long_1 = (i <= 1) ? i + 1 : i + 2;
        dds_sample_state_t   expected_sst    = SAMPLE_SST(expected_long_1);
        dds_view_state_t     expected_vst    = SAMPLE_VST(expected_long_1);
        dds_instance_state_t expected_ist    = SAMPLE_IST(expected_long_1);

        /* Check data. */
        CU_ASSERT_EQUAL_FATAL(sample->long_1, expected_long_1  );
        CU_ASSERT_EQUAL_FATAL(sample->long_2, expected_long_1/2);
        CU_ASSERT_EQUAL_FATAL(sample->long_3, expected_long_1/3);

        /* Check states. */
        CU_ASSERT_EQUAL_FATAL(g_info[i].valid_data,      true);
        CU_ASSERT_EQUAL_FATAL(g_info[i].sample_state,    expected_sst);
        CU_ASSERT_EQUAL_FATAL(g_info[i].view_state,      expected_vst);
        CU_ASSERT_EQUAL_FATAL(g_info[i].instance_state,  expected_ist);
        CU_ASSERT_NOT_EQUAL_FATAL(g_info[i].instance_state, DDS_IST_ALIVE);
    }

    ret = dds_return_loan(g_reader, g_loans, ret);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    /* All samples should still be available. */
    ret = samples_cnt();
    CU_ASSERT_EQUAL_FATAL(ret, MAX_SAMPLES);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_read_mask_wl, disposed_instance_state, .init=reader_init, .fini=reader_fini)
{
    uint32_t mask = DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_NOT_ALIVE_DISPOSED_INSTANCE_STATE;
    dds_return_t ret;

    /* Read all samples that matches the mask. */
    ret = dds_read_mask_wl(g_reader, g_loans, g_info, MAX_SAMPLES, mask);
    CU_ASSERT_EQUAL_FATAL(ret, 2);
    for(int i = 0; i < ret; i++) {
        Space_Type1 *sample = (Space_Type1*)g_loans[i];

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

        /* Expected states. */
        int                  expected_long_1 = (i * 3) + 1;
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

    ret = dds_return_loan(g_reader, g_loans, ret);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    /* All samples should still be available. */
    ret = samples_cnt();
    CU_ASSERT_EQUAL_FATAL(ret, MAX_SAMPLES);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_read_mask_wl, no_writers_instance_state, .init=reader_init, .fini=reader_fini)
{
    uint32_t mask = DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_NOT_ALIVE_NO_WRITERS_INSTANCE_STATE;
    dds_return_t ret;

    /* Read all samples that matches the mask. */
    ret = dds_read_mask_wl(g_reader, g_loans, g_info, MAX_SAMPLES, mask);
    CU_ASSERT_EQUAL_FATAL(ret, 2);
    for(int i = 0; i < ret; i++) {
        Space_Type1 *sample = (Space_Type1*)g_loans[i];

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

        /* Expected states. */
        int                  expected_long_1 = (i * 3) + 2;
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

    ret = dds_return_loan(g_reader, g_loans, ret);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    /* All samples should still be available. */
    ret = samples_cnt();
    CU_ASSERT_EQUAL_FATAL(ret, MAX_SAMPLES);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_read_mask_wl, combination_of_states, .init=reader_init, .fini=reader_fini)
{
    uint32_t mask = DDS_NOT_READ_SAMPLE_STATE | DDS_NOT_NEW_VIEW_STATE | DDS_ALIVE_INSTANCE_STATE;
    dds_return_t ret;

    /* Read all samples that matches the mask. */
    ret = dds_read_mask_wl(g_reader, g_loans, g_info, MAX_SAMPLES, mask);
    CU_ASSERT_EQUAL_FATAL(ret, 1);
    for(int i = 0; i < ret; i++) {
        Space_Type1 *sample = (Space_Type1*)g_loans[i];

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

        /* Expected states. */
        int                  expected_long_1 = 3;
        dds_sample_state_t   expected_sst    = DDS_SST_NOT_READ;
        dds_view_state_t     expected_vst    = DDS_VST_OLD;
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

    ret = dds_return_loan(g_reader, g_loans, ret);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    /* All samples should still be available. */
    ret = samples_cnt();
    CU_ASSERT_EQUAL_FATAL(ret, MAX_SAMPLES);
}
/*************************************************************************************************/





/**************************************************************************************************
 *
 * These will check the take in various ways.
 *
 *************************************************************************************************/
/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_take, invalid_buffers) = {
        CU_DataPoints(void**,             g_samples, g_loans, (void**)0),
        CU_DataPoints(dds_sample_info_t*, g_info,    NULL,    NULL),
        CU_DataPoints(size_t,             0,         3,       MAX_SAMPLES),
        CU_DataPoints(uint32_t,           0,         3,       MAX_SAMPLES),
};
CU_Theory((void **buf, dds_sample_info_t *si, size_t bufsz, uint32_t maxs), ddsc_take, invalid_buffers, .init=reader_init, .fini=reader_fini)
{
    dds_return_t ret;
    /* The only valid permutation is when non of the buffer values are
     * invalid. So, don't test that. */
    CU_ASSERT_FATAL((buf != g_samples) || (si != g_info) || (bufsz == 0) || (maxs == 0) || (bufsz < maxs));
    /* TODO: CHAM-306, currently, a buffer is automatically 'promoted' to a loan when a buffer is
     * provided with NULL pointers. So, in fact, there's currently no real difference between calling
     * dds_take() dds_take_wl() (except for the provided bufsz). This will change, which means that
     * the given buffer should contain valid pointers, which again means that 'loan intended' buffer
     * should result in bad_parameter.
     * However, that's not the case yet. So don't test it. */
    if (buf != g_loans) {
        ret = dds_take(g_reader, buf, si, bufsz, maxs);
        CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
    } else {
        CU_PASS("Skipped");
    }
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_take, invalid_readers) = {
        CU_DataPoints(dds_entity_t, -2, -1, 0, INT_MAX, INT_MIN),
};
CU_Theory((dds_entity_t rdr), ddsc_take, invalid_readers, .init=reader_init, .fini=reader_fini)
{
    dds_return_t ret;

    ret = dds_take(rdr, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_take, non_readers) = {
        CU_DataPoints(dds_entity_t*, &g_participant, &g_topic, &g_writer, &g_subscriber, &g_waitset),
};
CU_Theory((dds_entity_t *rdr), ddsc_take, non_readers, .init=reader_init, .fini=reader_fini)
{
    dds_return_t ret;
    ret = dds_take(*rdr, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_ILLEGAL_OPERATION);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_take, already_deleted, .init=reader_init, .fini=reader_fini)
{
    dds_return_t ret;
    /* Try to take with a deleted reader. */
    dds_delete(g_reader);
    ret = dds_take(g_reader, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
    CU_ASSERT_EQUAL(ret, DDS_RETCODE_BAD_PARAMETER);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_take, valid, .init=reader_init, .fini=reader_fini)
{
    dds_return_t expected_cnt = MAX_SAMPLES;
    dds_return_t ret;

    ret = dds_take(g_reader, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
    CU_ASSERT_EQUAL_FATAL(ret, MAX_SAMPLES);
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

        /* Expected states. */
        int                  expected_long_1 = i;
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

    /* Only samples that weren't taken should be available. */
    ret = samples_cnt();
    CU_ASSERT_EQUAL_FATAL(ret, MAX_SAMPLES - expected_cnt);
}
/*************************************************************************************************/





/**************************************************************************************************
 *
 * These will check the read_wl in various ways.
 *
 *************************************************************************************************/
/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_take_wl, invalid_buffers) = {
        CU_DataPoints(void**,             g_loans, (void**)0),
        CU_DataPoints(dds_sample_info_t*, g_info,  (dds_sample_info_t*)0   ),
        CU_DataPoints(uint32_t,           0,        3,  MAX_SAMPLES   ),
};
CU_Theory((void **buf, dds_sample_info_t *si, uint32_t maxs), ddsc_take_wl, invalid_buffers, .init=reader_init, .fini=reader_fini)
{
    dds_return_t ret;
    /* The only valid permutation is when non of the buffer values are
     * invalid. So, don't test that. */
    CU_ASSERT_FATAL((buf != g_loans) || (si != g_info) || (maxs == 0));
    ret = dds_take_wl(g_reader, buf, si, maxs);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_take_wl, invalid_readers) = {
        CU_DataPoints(dds_entity_t, -2, -1, 0, INT_MAX, INT_MIN),
};
CU_Theory((dds_entity_t rdr), ddsc_take_wl, invalid_readers, .init=reader_init, .fini=reader_fini)
{
    dds_return_t ret;

    ret = dds_take_wl(rdr, g_loans, g_info, MAX_SAMPLES);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_take_wl, non_readers) = {
        CU_DataPoints(dds_entity_t*, &g_participant, &g_topic, &g_writer, &g_subscriber, &g_waitset),
};
CU_Theory((dds_entity_t *rdr), ddsc_take_wl, non_readers, .init=reader_init, .fini=reader_fini)
{
    dds_return_t ret;
    ret = dds_take_wl(*rdr, g_loans, g_info, MAX_SAMPLES);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_ILLEGAL_OPERATION);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_take_wl, already_deleted, .init=reader_init, .fini=reader_fini)
{
    dds_return_t ret;
    /* Try to read with a deleted reader. */
    dds_delete(g_reader);
    ret = dds_take_wl(g_reader, g_loans, g_info, MAX_SAMPLES);
    CU_ASSERT_EQUAL(ret, DDS_RETCODE_BAD_PARAMETER);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_take_wl, valid, .init=reader_init, .fini=reader_fini)
{
    dds_return_t expected_cnt = MAX_SAMPLES;
    dds_return_t ret;

    ret = dds_take_wl(g_reader, g_loans, g_info, MAX_SAMPLES);
    CU_ASSERT_EQUAL_FATAL(ret, MAX_SAMPLES);
    for(int i = 0; i < ret; i++) {
        Space_Type1 *sample = (Space_Type1*)g_loans[i];

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

        /* Expected states. */
        int                  expected_long_1 = i;
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

    ret = dds_return_loan(g_reader, g_loans, ret);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    /* Only samples that weren't taken should be available. */
    ret = samples_cnt();
    CU_ASSERT_EQUAL_FATAL(ret, MAX_SAMPLES - expected_cnt);
}
/*************************************************************************************************/





/**************************************************************************************************
 *
 * These will check the read_mask in various ways.
 *
 *************************************************************************************************/
/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_take_mask, invalid_buffers) = {
        CU_DataPoints(void**,             g_samples, g_loans, (void**)0),
        CU_DataPoints(dds_sample_info_t*, g_info,    NULL,    NULL),
        CU_DataPoints(size_t,             0,         3,       MAX_SAMPLES),
        CU_DataPoints(uint32_t,           0,         3,       MAX_SAMPLES),
};
CU_Theory((void **buf, dds_sample_info_t *si, size_t bufsz, uint32_t maxs), ddsc_take_mask, invalid_buffers, .init=reader_init, .fini=reader_fini)
{
    uint32_t mask = DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
    dds_return_t ret;
    /* The only valid permutation is when non of the buffer values are
     * invalid. So, don't test that. */
    CU_ASSERT_FATAL((buf != g_samples) || (si != g_info) || (bufsz == 0) || (maxs == 0) || (bufsz < maxs));
    /* TODO: CHAM-306, currently, a buffer is automatically 'promoted' to a loan when a buffer is
     * provided with NULL pointers. So, in fact, there's currently no real difference between calling
     * dds_take_mask() dds_take_mask_wl() (except for the provided bufsz). This will change, which means that
     * the given buffer should contain valid pointers, which again means that 'loan intended' buffer
     * should result in bad_parameter.
     * However, that's not the case yet. So don't test it. */
    if (buf != g_loans) {
        ret = dds_take_mask(g_reader, buf, si, bufsz, maxs, mask);
        CU_ASSERT_EQUAL(ret, DDS_RETCODE_BAD_PARAMETER);
    } else {
        CU_PASS("Skipped");
    }
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_take_mask, invalid_readers) = {
        CU_DataPoints(dds_entity_t, -2, -1, 0, INT_MAX, INT_MIN),
};
CU_Theory((dds_entity_t rdr), ddsc_take_mask, invalid_readers, .init=reader_init, .fini=reader_fini)
{
    uint32_t mask = DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
    dds_return_t ret;

    ret = dds_take_mask(rdr, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES, mask);
    CU_ASSERT_EQUAL(ret, DDS_RETCODE_BAD_PARAMETER);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_take_mask, non_readers) = {
        CU_DataPoints(dds_entity_t*, &g_participant, &g_topic, &g_writer, &g_subscriber, &g_waitset),
};
CU_Theory((dds_entity_t *rdr), ddsc_take_mask, non_readers, .init=reader_init, .fini=reader_fini)
{
    uint32_t mask = DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
    dds_return_t ret;
    ret = dds_take_mask(*rdr, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES, mask);
    CU_ASSERT_EQUAL(ret, DDS_RETCODE_ILLEGAL_OPERATION);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_take_mask, already_deleted, .init=reader_init, .fini=reader_fini)
{
    uint32_t mask = DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
    dds_return_t ret;
    /* Try to read with a deleted reader. */
    dds_delete(g_reader);
    ret = dds_take_mask(g_reader, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES, mask);
    CU_ASSERT_EQUAL(ret, DDS_RETCODE_BAD_PARAMETER);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_take_mask, any, .init=reader_init, .fini=reader_fini)
{
    uint32_t mask = DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
    dds_return_t expected_cnt = MAX_SAMPLES;
    dds_return_t ret;

    /* Read all samples that matches the mask. */
    ret = dds_take_mask(g_reader, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES, mask);
    CU_ASSERT_EQUAL_FATAL(ret, expected_cnt);
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

        /* Expected states. */
        int                  expected_long_1 = i;
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

    /* Only samples that weren't taken should be available. */
    ret = samples_cnt();
    CU_ASSERT_EQUAL_FATAL(ret, MAX_SAMPLES - expected_cnt);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_take_mask, not_read_sample_state, .init=reader_init, .fini=reader_fini)
{
    uint32_t mask = DDS_NOT_READ_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
    dds_return_t expected_cnt = 4;
    dds_return_t ret;

    /* Read all samples that matches the mask. */
    ret = dds_take_mask(g_reader, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES, mask);
    CU_ASSERT_EQUAL_FATAL(ret, expected_cnt);
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

        /* Expected states. */
        int                  expected_long_1 = SAMPLE_LAST_READ_SST + 1 + i;
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

    /* Only samples that weren't taken should be available. */
    ret = samples_cnt();
    CU_ASSERT_EQUAL_FATAL(ret, MAX_SAMPLES - expected_cnt);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_take_mask, read_sample_state, .init=reader_init, .fini=reader_fini)
{
    uint32_t mask = DDS_READ_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
    dds_return_t expected_cnt = 3;
    dds_return_t ret;

    /* Read all samples that matches the mask. */
    ret = dds_take_mask(g_reader, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES, mask);
    CU_ASSERT_EQUAL_FATAL(ret, expected_cnt);
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

        /* Expected states. */
        int                  expected_long_1 = i;
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

    /* Only samples that weren't taken should be available. */
    ret = samples_cnt();
    CU_ASSERT_EQUAL_FATAL(ret, MAX_SAMPLES - expected_cnt);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_take_mask, new_view_state, .init=reader_init, .fini=reader_fini)
{
    uint32_t mask = DDS_ANY_SAMPLE_STATE | DDS_NEW_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
    dds_return_t expected_cnt = 3;
    dds_return_t ret;

    /* Read all samples that matches the mask. */
    ret = dds_take_mask(g_reader, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES, mask);
    CU_ASSERT_EQUAL_FATAL(ret, expected_cnt);
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

        /* Expected states. */
        int                  expected_long_1 = SAMPLE_LAST_OLD_VST + 1 + i;
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

    /* Only samples that weren't taken should be available. */
    ret = samples_cnt();
    CU_ASSERT_EQUAL_FATAL(ret, MAX_SAMPLES - expected_cnt);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_take_mask, not_new_view_state, .init=reader_init, .fini=reader_fini)
{
    uint32_t mask = DDS_ANY_SAMPLE_STATE | DDS_NOT_NEW_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
    dds_return_t expected_cnt = 4;
    dds_return_t ret;

    /* Read all samples that matches the mask. */
    ret = dds_take_mask(g_reader, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES, mask);
    CU_ASSERT_EQUAL_FATAL(ret, expected_cnt);
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

        /* Expected states. */
        int                  expected_long_1 = i;
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

    /* Only samples that weren't taken should be available. */
    ret = samples_cnt();
    CU_ASSERT_EQUAL_FATAL(ret, MAX_SAMPLES - expected_cnt);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_take_mask, alive_instance_state, .init=reader_init, .fini=reader_fini)
{
    uint32_t mask = DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ALIVE_INSTANCE_STATE;
    dds_return_t expected_cnt = 3;
    dds_return_t ret;

    /* Read all samples that matches the mask. */
    ret = dds_take_mask(g_reader, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES, mask);
    CU_ASSERT_EQUAL_FATAL(ret, expected_cnt);
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

        /* Expected states. */
        int                  expected_long_1 = i * 3;
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

    /* Only samples that weren't taken should be available. */
    ret = samples_cnt();
    CU_ASSERT_EQUAL_FATAL(ret, MAX_SAMPLES - expected_cnt);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_take_mask, not_alive_instance_state, .init=reader_init, .fini=reader_fini)
{
    uint32_t mask = DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_NOT_ALIVE_DISPOSED_INSTANCE_STATE | DDS_NOT_ALIVE_NO_WRITERS_INSTANCE_STATE;
    dds_return_t expected_cnt = 4;
    dds_return_t ret;

    /* Read all samples that matches the mask. */
    ret = dds_take_mask(g_reader, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES, mask);
    CU_ASSERT_EQUAL_FATAL(ret, expected_cnt);
    for(int i = 0; i < ret; i++) {
        Space_Type1 *sample = (Space_Type1*)g_samples[i];

        /*
         * | long_1 | long_2 | long_3 |    sst   | vst |    ist     |
         * ----------------------------------------------------------
         * |    0   |    0   |    0   |     read | old | alive      |
         * |    1   |    0   |    0   |     read | old | disposed   | <---
         * |    2   |    1   |    0   |     read | old | no_writers | <---
         * |    3   |    1   |    1   | not_read | old | alive      |
         * |    4   |    2   |    1   | not_read | new | disposed   | <---
         * |    5   |    2   |    1   | not_read | new | no_writers | <---
         * |    6   |    3   |    2   | not_read | new | alive      |
         */

        /* Expected states. */
        int                  expected_long_1 = (i <= 1) ? i + 1 : i + 2;
        dds_sample_state_t   expected_sst    = SAMPLE_SST(expected_long_1);
        dds_view_state_t     expected_vst    = SAMPLE_VST(expected_long_1);
        dds_instance_state_t expected_ist    = SAMPLE_IST(expected_long_1);

        /* Check data. */
        CU_ASSERT_EQUAL_FATAL(sample->long_1, expected_long_1  );
        CU_ASSERT_EQUAL_FATAL(sample->long_2, expected_long_1/2);
        CU_ASSERT_EQUAL_FATAL(sample->long_3, expected_long_1/3);

        /* Check states. */
        CU_ASSERT_EQUAL_FATAL(g_info[i].valid_data,      true);
        CU_ASSERT_EQUAL_FATAL(g_info[i].sample_state,    expected_sst);
        CU_ASSERT_EQUAL_FATAL(g_info[i].view_state,      expected_vst);
        CU_ASSERT_EQUAL_FATAL(g_info[i].instance_state,  expected_ist);
        CU_ASSERT_NOT_EQUAL_FATAL(g_info[i].instance_state, DDS_IST_ALIVE);
    }

    /* Only samples that weren't taken should be available. */
    ret = samples_cnt();
    CU_ASSERT_EQUAL_FATAL(ret, MAX_SAMPLES - expected_cnt);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_take_mask, disposed_instance_state, .init=reader_init, .fini=reader_fini)
{
    uint32_t mask = DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_NOT_ALIVE_DISPOSED_INSTANCE_STATE;
    dds_return_t expected_cnt = 2;
    dds_return_t ret;

    /* Read all samples that matches the mask. */
    ret = dds_take_mask(g_reader, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES, mask);
    CU_ASSERT_EQUAL_FATAL(ret, expected_cnt);
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

        /* Expected states. */
        int                  expected_long_1 = (i * 3) + 1;
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

    /* Only samples that weren't taken should be available. */
    ret = samples_cnt();
    CU_ASSERT_EQUAL_FATAL(ret, MAX_SAMPLES - expected_cnt);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_take_mask, no_writers_instance_state, .init=reader_init, .fini=reader_fini)
{
    uint32_t mask = DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_NOT_ALIVE_NO_WRITERS_INSTANCE_STATE;
    dds_return_t expected_cnt = 2;
    dds_return_t ret;

    /* Read all samples that matches the mask. */
    ret = dds_take_mask(g_reader, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES, mask);
    CU_ASSERT_EQUAL_FATAL(ret, expected_cnt);
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

        /* Expected states. */
        int                  expected_long_1 = (i * 3) + 2;
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

    /* Only samples that weren't taken should be available. */
    ret = samples_cnt();
    CU_ASSERT_EQUAL_FATAL(ret, MAX_SAMPLES - expected_cnt);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_take_mask, combination_of_states, .init=reader_init, .fini=reader_fini)
{
    uint32_t mask = DDS_NOT_READ_SAMPLE_STATE | DDS_NOT_NEW_VIEW_STATE | DDS_ALIVE_INSTANCE_STATE;
    dds_return_t expected_cnt = 1;
    dds_return_t ret;

    /* Read all samples that matches the mask. */
    ret = dds_take_mask(g_reader, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES, mask);
    CU_ASSERT_EQUAL_FATAL(ret, expected_cnt);
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

        /* Expected states. */
        int                  expected_long_1 = 3;
        dds_sample_state_t   expected_sst    = DDS_SST_NOT_READ;
        dds_view_state_t     expected_vst    = DDS_VST_OLD;
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

    /* Only samples that weren't taken should be available. */
    ret = samples_cnt();
    CU_ASSERT_EQUAL_FATAL(ret, MAX_SAMPLES - expected_cnt);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_take_mask, take_instance_last_sample)
{
#define WOULD_CRASH
#ifdef WOULD_CRASH
    uint32_t mask = DDS_NOT_READ_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ALIVE_INSTANCE_STATE;
    dds_sample_state_t expected_sst = DDS_SST_NOT_READ;
    int expected_long_3 = 3;
#else
    uint32_t mask = DDS_READ_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ALIVE_INSTANCE_STATE;
    dds_sample_state_t expected_sst = DDS_SST_READ;
    int expected_long_3 = 2;
#endif
    dds_return_t expected_cnt = 1;
    Space_Type1 sample = { 0, 0, 0 };
    dds_attach_t triggered;
    dds_return_t ret;
    char name[100];

    /* We need other readers/writers/data to force the crash. */
    g_qos = dds_create_qos();
    dds_qset_history(g_qos, DDS_HISTORY_KEEP_ALL, DDS_LENGTH_UNLIMITED);
    g_participant = dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);
    CU_ASSERT_FATAL(g_participant > 0);
    g_waitset = dds_create_waitset(g_participant);
    CU_ASSERT_FATAL(g_waitset > 0);
    g_topic = dds_create_topic(g_participant, &Space_Type1_desc, create_unique_topic_name("ddsc_reader_test", name, 100), NULL, NULL);
    CU_ASSERT_FATAL(g_topic > 0);
    g_reader = dds_create_reader(g_participant, g_topic, g_qos, NULL);
    CU_ASSERT_FATAL(g_reader > 0);
    g_writer = dds_create_writer(g_participant, g_topic, NULL, NULL);
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

    /* Generate following data:
     *  | long_1 | long_2 | long_3 |    sst   | vst |    ist     |
     *  ----------------------------------------------------------
     *  |    0   |    1   |    2   |     read | old | alive      |
     *  |    0   |    1   |    3   | not_read | old | alive      |
     */
    sample.long_1 = 0;
    sample.long_2 = 1;
    sample.long_3 = 2;
    ret = dds_write(g_writer, &sample);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    ret = dds_read(g_reader, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
    CU_ASSERT_EQUAL_FATAL(ret, 1);
    sample.long_3 = 3;
    ret = dds_write(g_writer, &sample);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    /* Take just one sample of the instance (the last one). */
    ret = dds_take_mask(g_reader, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES, mask);
    CU_ASSERT_EQUAL_FATAL(ret, expected_cnt);
    for(int i = 0; i < ret; i++) {
        Space_Type1 *s = (Space_Type1*)g_samples[i];

        /*
         *  | long_1 | long_2 | long_3 |    sst   | vst |    ist     |
         *  ----------------------------------------------------------
         *  |    0   |    1   |    2   |     read | old | alive      | <--- no worries
         *  |    0   |    1   |    3   | not_read | old | alive      | <--- crashed
         */

        /* Check data. */
        CU_ASSERT_EQUAL_FATAL(s->long_1, 0);
        CU_ASSERT_EQUAL_FATAL(s->long_2, 1);
        CU_ASSERT_EQUAL_FATAL(s->long_3, expected_long_3);

        /* Check states. */
        CU_ASSERT_EQUAL_FATAL(g_info[i].valid_data,     true);
        CU_ASSERT_EQUAL_FATAL(g_info[i].sample_state,   expected_sst);
        CU_ASSERT_EQUAL_FATAL(g_info[i].view_state,     DDS_VST_OLD);
        CU_ASSERT_EQUAL_FATAL(g_info[i].instance_state, DDS_IST_ALIVE);
    }

    /* Only samples that weren't taken should be available. */
    ret = samples_cnt();
    CU_ASSERT_EQUAL_FATAL(ret, 1);

    /*
     * So far so good.
     * But now the problem appeared:
     * The reader crashed when deleting....
     */
    dds_delete(g_reader);

    /* Before the crash was fixed, we didn't come here. */
    dds_delete(g_writer);
    dds_delete(g_waitset);
    dds_delete(g_topic);
    dds_delete(g_participant);
    dds_delete_qos(g_qos);
}
/*************************************************************************************************/





/**************************************************************************************************
 *
 * These will check the read_mask_wl in various ways.
 *
 *************************************************************************************************/
/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_take_mask_wl, invalid_buffers) = {
        CU_DataPoints(void**,             g_loans, (void**)0),
        CU_DataPoints(dds_sample_info_t*, g_info,  (dds_sample_info_t*)0   ),
        CU_DataPoints(uint32_t,           0,        3,  MAX_SAMPLES   ),
};
CU_Theory((void **buf, dds_sample_info_t *si, uint32_t maxs), ddsc_take_mask_wl, invalid_buffers, .init=reader_init, .fini=reader_fini)
{
    uint32_t mask = DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
    dds_return_t ret;
    /* The only valid permutation is when non of the buffer values are
     * invalid. So, don't test that. */
    CU_ASSERT_FATAL((buf != g_loans) || (si != g_info) || (maxs == 0));
    ret = dds_take_mask_wl(g_reader, buf, si, maxs, mask);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_take_mask_wl, invalid_readers) = {
        CU_DataPoints(dds_entity_t, -2, -1, 0, INT_MAX, INT_MIN),
};
CU_Theory((dds_entity_t rdr), ddsc_take_mask_wl, invalid_readers, .init=reader_init, .fini=reader_fini)
{
    uint32_t mask = DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
    dds_return_t ret;

    ret = dds_take_mask_wl(rdr, g_loans, g_info, MAX_SAMPLES, mask);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_take_mask_wl, non_readers) = {
        CU_DataPoints(dds_entity_t*, &g_participant, &g_topic, &g_writer, &g_subscriber, &g_waitset),
};
CU_Theory((dds_entity_t *rdr), ddsc_take_mask_wl, non_readers, .init=reader_init, .fini=reader_fini)
{
    uint32_t mask = DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
    dds_return_t ret;
    ret = dds_take_mask_wl(*rdr, g_loans, g_info, MAX_SAMPLES, mask);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_ILLEGAL_OPERATION);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_take_mask_wl, already_deleted, .init=reader_init, .fini=reader_fini)
{
    uint32_t mask = DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
    dds_return_t ret;
    /* Try to read with a deleted reader. */
    dds_delete(g_reader);
    ret = dds_take_mask_wl(g_reader, g_loans, g_info, MAX_SAMPLES, mask);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_take_mask_wl, any, .init=reader_init, .fini=reader_fini)
{
    uint32_t mask = DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
    dds_return_t expected_cnt = MAX_SAMPLES;
    dds_return_t ret;

    /* Read all samples that matches the mask. */
    ret = dds_take_mask_wl(g_reader, g_loans, g_info, MAX_SAMPLES, mask);
    CU_ASSERT_EQUAL_FATAL(ret, expected_cnt);
    for(int i = 0; i < ret; i++) {
        Space_Type1 *sample = (Space_Type1*)g_loans[i];

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

        /* Expected states. */
        int                  expected_long_1 = i;
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

    ret = dds_return_loan(g_reader, g_loans, ret);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    /* Only samples that weren't taken should be available. */
    ret = samples_cnt();
    CU_ASSERT_EQUAL_FATAL(ret, MAX_SAMPLES - expected_cnt);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_take_mask_wl, not_read_sample_state, .init=reader_init, .fini=reader_fini)
{
    uint32_t mask = DDS_NOT_READ_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
    dds_return_t expected_cnt = 4;
    dds_return_t ret;

    /* Read all samples that matches the mask. */
    ret = dds_take_mask_wl(g_reader, g_loans, g_info, MAX_SAMPLES, mask);
    CU_ASSERT_EQUAL_FATAL(ret, expected_cnt);
    for(int i = 0; i < ret; i++) {
        Space_Type1 *sample = (Space_Type1*)g_loans[i];

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

        /* Expected states. */
        int                  expected_long_1 = SAMPLE_LAST_READ_SST + 1 + i;
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

    ret = dds_return_loan(g_reader, g_loans, ret);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    /* Only samples that weren't taken should be available. */
    ret = samples_cnt();
    CU_ASSERT_EQUAL_FATAL(ret, MAX_SAMPLES - expected_cnt);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_take_mask_wl, read_sample_state, .init=reader_init, .fini=reader_fini)
{
    uint32_t mask = DDS_READ_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
    dds_return_t expected_cnt = 3;
    dds_return_t ret;

    /* Read all samples that matches the mask. */
    ret = dds_take_mask_wl(g_reader, g_loans, g_info, MAX_SAMPLES, mask);
    CU_ASSERT_EQUAL_FATAL(ret, expected_cnt);
    for(int i = 0; i < ret; i++) {
        Space_Type1 *sample = (Space_Type1*)g_loans[i];

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

        /* Expected states. */
        int                  expected_long_1 = i;
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

    ret = dds_return_loan(g_reader, g_loans, ret);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    /* Only samples that weren't taken should be available. */
    ret = samples_cnt();
    CU_ASSERT_EQUAL_FATAL(ret, MAX_SAMPLES - expected_cnt);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_take_mask_wl, new_view_state, .init=reader_init, .fini=reader_fini)
{
    uint32_t mask = DDS_ANY_SAMPLE_STATE | DDS_NEW_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
    dds_return_t expected_cnt = 3;
    dds_return_t ret;

    /* Read all samples that matches the mask. */
    ret = dds_take_mask_wl(g_reader, g_loans, g_info, MAX_SAMPLES, mask);
    CU_ASSERT_EQUAL_FATAL(ret, expected_cnt);
    for(int i = 0; i < ret; i++) {
        Space_Type1 *sample = (Space_Type1*)g_loans[i];

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

        /* Expected states. */
        int                  expected_long_1 = SAMPLE_LAST_OLD_VST + 1 + i;
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

    ret = dds_return_loan(g_reader, g_loans, ret);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    /* Only samples that weren't taken should be available. */
    ret = samples_cnt();
    CU_ASSERT_EQUAL_FATAL(ret, MAX_SAMPLES - expected_cnt);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_take_mask_wl, not_new_view_state, .init=reader_init, .fini=reader_fini)
{
    uint32_t mask = DDS_ANY_SAMPLE_STATE | DDS_NOT_NEW_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
    dds_return_t expected_cnt = 4;
    dds_return_t ret;

    /* Read all samples that matches the mask. */
    ret = dds_take_mask_wl(g_reader, g_loans, g_info, MAX_SAMPLES, mask);
    CU_ASSERT_EQUAL_FATAL(ret, expected_cnt);
    for(int i = 0; i < ret; i++) {
        Space_Type1 *sample = (Space_Type1*)g_loans[i];

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

        /* Expected states. */
        int                  expected_long_1 = i;
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

    ret = dds_return_loan(g_reader, g_loans, ret);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    /* Only samples that weren't taken should be available. */
    ret = samples_cnt();
    CU_ASSERT_EQUAL_FATAL(ret, MAX_SAMPLES - expected_cnt);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_take_mask_wl, alive_instance_state, .init=reader_init, .fini=reader_fini)
{
    uint32_t mask = DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ALIVE_INSTANCE_STATE;
    dds_return_t expected_cnt = 3;
    dds_return_t ret;

    /* Read all samples that matches the mask. */
    ret = dds_take_mask_wl(g_reader, g_loans, g_info, MAX_SAMPLES, mask);
    CU_ASSERT_EQUAL_FATAL(ret, expected_cnt);
    for(int i = 0; i < ret; i++) {
        Space_Type1 *sample = (Space_Type1*)g_loans[i];

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

        /* Expected states. */
        int                  expected_long_1 = i * 3;
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

    ret = dds_return_loan(g_reader, g_loans, ret);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    /* Only samples that weren't taken should be available. */
    ret = samples_cnt();
    CU_ASSERT_EQUAL_FATAL(ret, MAX_SAMPLES - expected_cnt);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_take_mask_wl, not_alive_instance_state, .init=reader_init, .fini=reader_fini)
{
    uint32_t mask = DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_NOT_ALIVE_DISPOSED_INSTANCE_STATE | DDS_NOT_ALIVE_NO_WRITERS_INSTANCE_STATE;
    dds_return_t expected_cnt = 4;
    dds_return_t ret;

    /* Read all samples that matches the mask. */
    ret = dds_take_mask_wl(g_reader, g_loans, g_info, MAX_SAMPLES, mask);
    CU_ASSERT_EQUAL_FATAL(ret, expected_cnt);
    for(int i = 0; i < ret; i++) {
        Space_Type1 *sample = (Space_Type1*)g_loans[i];

        /*
         * | long_1 | long_2 | long_3 |    sst   | vst |    ist     |
         * ----------------------------------------------------------
         * |    0   |    0   |    0   |     read | old | alive      |
         * |    1   |    0   |    0   |     read | old | disposed   | <---
         * |    2   |    1   |    0   |     read | old | no_writers | <---
         * |    3   |    1   |    1   | not_read | old | alive      |
         * |    4   |    2   |    1   | not_read | new | disposed   | <---
         * |    5   |    2   |    1   | not_read | new | no_writers | <---
         * |    6   |    3   |    2   | not_read | new | alive      |
         */

        /* Expected states. */
        int                  expected_long_1 = (i <= 1) ? i + 1 : i + 2;
        dds_sample_state_t   expected_sst    = SAMPLE_SST(expected_long_1);
        dds_view_state_t     expected_vst    = SAMPLE_VST(expected_long_1);
        dds_instance_state_t expected_ist    = SAMPLE_IST(expected_long_1);

        /* Check data. */
        CU_ASSERT_EQUAL_FATAL(sample->long_1, expected_long_1  );
        CU_ASSERT_EQUAL_FATAL(sample->long_2, expected_long_1/2);
        CU_ASSERT_EQUAL_FATAL(sample->long_3, expected_long_1/3);

        /* Check states. */
        CU_ASSERT_EQUAL_FATAL(g_info[i].valid_data,      true);
        CU_ASSERT_EQUAL_FATAL(g_info[i].sample_state,    expected_sst);
        CU_ASSERT_EQUAL_FATAL(g_info[i].view_state,      expected_vst);
        CU_ASSERT_EQUAL_FATAL(g_info[i].instance_state,  expected_ist);
        CU_ASSERT_NOT_EQUAL_FATAL(g_info[i].instance_state, DDS_IST_ALIVE);
    }

    ret = dds_return_loan(g_reader, g_loans, ret);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    /* Only samples that weren't taken should be available. */
    ret = samples_cnt();
    CU_ASSERT_EQUAL_FATAL(ret, MAX_SAMPLES - expected_cnt);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_take_mask_wl, disposed_instance_state, .init=reader_init, .fini=reader_fini)
{
    uint32_t mask = DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_NOT_ALIVE_DISPOSED_INSTANCE_STATE;
    dds_return_t expected_cnt = 2;
    dds_return_t ret;

    /* Read all samples that matches the mask. */
    ret = dds_take_mask_wl(g_reader, g_loans, g_info, MAX_SAMPLES, mask);
    CU_ASSERT_EQUAL_FATAL(ret, expected_cnt);
    for(int i = 0; i < ret; i++) {
        Space_Type1 *sample = (Space_Type1*)g_loans[i];

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

        /* Expected states. */
        int                  expected_long_1 = (i * 3) + 1;
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

    ret = dds_return_loan(g_reader, g_loans, ret);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    /* Only samples that weren't taken should be available. */
    ret = samples_cnt();
    CU_ASSERT_EQUAL_FATAL(ret, MAX_SAMPLES - expected_cnt);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_take_mask_wl, no_writers_instance_state, .init=reader_init, .fini=reader_fini)
{
    uint32_t mask = DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_NOT_ALIVE_NO_WRITERS_INSTANCE_STATE;
    dds_return_t expected_cnt = 2;
    dds_return_t ret;

    /* Read all samples that matches the mask. */
    ret = dds_take_mask_wl(g_reader, g_loans, g_info, MAX_SAMPLES, mask);
    CU_ASSERT_EQUAL_FATAL(ret, expected_cnt);
    for(int i = 0; i < ret; i++) {
        Space_Type1 *sample = (Space_Type1*)g_loans[i];

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

        /* Expected states. */
        int                  expected_long_1 = (i * 3) + 2;
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

    ret = dds_return_loan(g_reader, g_loans, ret);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    /* Only samples that weren't taken should be available. */
    ret = samples_cnt();
    CU_ASSERT_EQUAL_FATAL(ret, MAX_SAMPLES - expected_cnt);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_take_mask_wl, combination_of_states, .init=reader_init, .fini=reader_fini)
{
    uint32_t mask = DDS_NOT_READ_SAMPLE_STATE | DDS_NOT_NEW_VIEW_STATE | DDS_ALIVE_INSTANCE_STATE;
    dds_return_t expected_cnt = 1;
    dds_return_t ret;

    /* Read all samples that matches the mask. */
    ret = dds_take_mask_wl(g_reader, g_loans, g_info, MAX_SAMPLES, mask);
    CU_ASSERT_EQUAL_FATAL(ret, expected_cnt);
    for(int i = 0; i < ret; i++) {
        Space_Type1 *sample = (Space_Type1*)g_loans[i];

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

        /* Expected states. */
        int                  expected_long_1 = 3;
        dds_sample_state_t   expected_sst    = DDS_SST_NOT_READ;
        dds_view_state_t     expected_vst    = DDS_VST_OLD;
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

    ret = dds_return_loan(g_reader, g_loans, ret);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    /* Only samples that weren't taken should be available. */
    ret = samples_cnt();
    CU_ASSERT_EQUAL_FATAL(ret, MAX_SAMPLES - expected_cnt);
}

/* This test verifies that readers are enabled by default */
CU_Test(ddsc_reader, enable_by_default) {
  dds_entity_t participant, subscriber, reader, topic;
  dds_return_t ret;
  dds_qos_t *rqos;
  dds_history_kind_t hist_kind;
  int32_t hist_depth;

  rqos = dds_create_qos();
  /* check that default autoenable setting of participant is true */
  participant = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
  CU_ASSERT_FATAL(participant > 0);
  /* check that default autoenable setting of subscriber is true */
  subscriber = dds_create_subscriber(participant, NULL, NULL);
  CU_ASSERT_FATAL(subscriber > 0);
  topic = dds_create_topic(participant, &Space_Type1_desc, "ddsc_reader_enable_defaults", NULL, NULL);
  CU_ASSERT_FATAL(topic > 0);
  reader = dds_create_reader(subscriber, topic, NULL, NULL);
  CU_ASSERT_FATAL(reader > 0);
  /* get the default history qos */
  ret = dds_get_qos(reader, rqos);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
  dds_qget_history(rqos, &hist_kind,&hist_depth);
  CU_ASSERT_EQUAL_FATAL(hist_kind, DDS_HISTORY_KEEP_LAST);
  CU_ASSERT_EQUAL_FATAL(hist_depth, 1);
  /* try to change the immutable immutable qos, this should fail */
  dds_qset_history(rqos, DDS_HISTORY_KEEP_ALL, 0);
  ret = dds_set_qos(reader, rqos);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_IMMUTABLE_POLICY);
  ret = dds_delete (participant);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
  dds_delete_qos(rqos);
}

/* This test validates various combinations of auto_enabled_created_entities
 * in combination with the enabled or disabled entities. The following
 * matrix shows the combinations for the subscriber
 *
 *                           autoenable
 *                         false   |   true
 *               --------------------------
 *               false|     1.        2.
 *  enabled           |
 *               true |     4.        3.
 *
 *  Case 1 is the case where the subscriber is disabled and has autoenable=false.
 *  In this case the reader that is created for this subscriber is disabled
 *  when calling dds_enable() on the subscriber followed by calling dds_enable()
 *  on the reader.
 *
 *  Case 2 is the case where the subscriber is disabled and has autoenable=true.
 *  In this case the reader that is created for this subscriber is disabled
 *  and will be enabled when calling dds_enable() on the subscriber.
 *
 *  Case 3 is the case where the subscriber is enabled but has autoenable=false.
 *  In this case the reader that is created for this subscriber is disabled, and
 *  will be enabled when calling dds_enable() on the reader.
 *
 *  Case 4 is the case where the subscriber is enabled and has autoenable=true.
 *  In this case a reader that is created for this subscriber must be enabled
 *  upon creation.
 *
 *  The test uses a single subscriber to demonstrate that starts disabled
 *  to test case 1 and 2, and then changes to enabled to test cases 3 and 4.
 *  The autoenable qos is a mutable qos that is changed throughout the test.
 */
CU_Test(ddsc_reader, enable_reader) {
  dds_qos_t *pqos, *sqos, *rqos;
  bool autoenable;
  bool status;
  dds_return_t ret;
  dds_history_kind_t hist_kind;
  int32_t hist_depth;
  dds_entity_t participant, subscriber1, subscriber2, topic, reader1, reader2, reader3, reader4;
  dds_presentation_access_scope_kind_t access_scope;
  bool coherent_access;
  bool ordered_access;

  /* create a participant with autoenable=false */
  pqos = dds_create_qos();
  sqos = dds_create_qos();
  rqos = dds_create_qos();
  dds_qset_entity_factory(pqos, false);
  participant = dds_create_participant (DDS_DOMAIN_DEFAULT, pqos, NULL);
  CU_ASSERT_FATAL(participant > 0);
  ret = dds_get_qos(participant, pqos);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
  status = dds_qget_entity_factory(pqos, &autoenable);
  CU_ASSERT_EQUAL_FATAL(status, true);
  CU_ASSERT_EQUAL_FATAL(autoenable, false);
  /* create topic */
  topic = dds_create_topic(participant, &Space_Type1_desc, "ddsc_reader_enable_disable", NULL, NULL);
  CU_ASSERT_FATAL(topic > 0);

  /* case 1: create a disabled subscriber with autoenable=false */
  dds_qset_entity_factory(sqos, false);
  subscriber1 = dds_create_subscriber(participant, sqos, NULL);
  CU_ASSERT_FATAL(subscriber1 > 0);
  ret = dds_get_qos(subscriber1, sqos);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
  status = dds_qget_entity_factory(sqos, &autoenable);
  CU_ASSERT_EQUAL_FATAL(status, true);
  CU_ASSERT_EQUAL_FATAL(autoenable, false);
  /* check that the subscriber is disabled by trying to set
   * an immutable qos. We use the presentation qos for that purpose */
  dds_qset_presentation(sqos, DDS_PRESENTATION_GROUP, true, true);
  ret = dds_set_qos(subscriber1, sqos);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
  ret = dds_get_qos(subscriber1, sqos);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
  dds_qget_presentation(sqos, &access_scope, &coherent_access, &ordered_access);
  CU_ASSERT_EQUAL_FATAL(access_scope, DDS_PRESENTATION_GROUP);
  CU_ASSERT_EQUAL_FATAL(coherent_access, true);
  CU_ASSERT_EQUAL_FATAL(coherent_access, true);
  /* create a reader for case 1 and check that the reader is
   * disabled by trying to set an immutable qos. We use the
   * history qos for that purpose.*/
  reader1 = dds_create_reader(subscriber1, topic, NULL, NULL);
  CU_ASSERT_FATAL(reader1 > 0);
  dds_qset_history(rqos, DDS_HISTORY_KEEP_ALL, 0);
  ret = dds_set_qos(reader1, rqos);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
  ret = dds_get_qos(reader1, rqos);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
  dds_qget_history(rqos, &hist_kind,&hist_depth);
  CU_ASSERT_EQUAL_FATAL(hist_kind, DDS_HISTORY_KEEP_ALL);
  CU_ASSERT_EQUAL_FATAL(hist_depth, 0);
  /* try to enable the reader, this should fail because
   * the subscriber is not enabled */
  ret = dds_enable(reader1);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_PRECONDITION_NOT_MET);

  /* Case 2: create a disabled subscriber with autoenable=true */
  dds_qset_entity_factory(sqos, true);
  ret = dds_set_qos(subscriber1, sqos);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
  ret = dds_get_qos(subscriber1, sqos);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
  status = dds_qget_entity_factory(sqos, &autoenable);
  CU_ASSERT_EQUAL_FATAL(status, true);
  CU_ASSERT_EQUAL_FATAL(autoenable, true);
  /* check that the subscriber is still disabled by trying to set
   * an immutable qos. We use the presentation qos for that purpose */
  dds_qset_presentation(sqos, DDS_PRESENTATION_INSTANCE, false, false);
  ret = dds_set_qos(subscriber1, sqos);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
  ret = dds_get_qos(subscriber1, sqos);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
  dds_qget_presentation(sqos, &access_scope, &coherent_access, &ordered_access);
  CU_ASSERT_EQUAL_FATAL(access_scope, DDS_PRESENTATION_INSTANCE);
  CU_ASSERT_EQUAL_FATAL(coherent_access, false);
  CU_ASSERT_EQUAL_FATAL(coherent_access, false);
  /* create a reader for case 2 and check that the reader is
   * disabled by trying to set an immutable qos. We use the
   * history qos for that purpose.*/
  reader2 = dds_create_reader(subscriber1, topic, NULL, NULL);
  CU_ASSERT_FATAL(reader2 > 0);
  dds_qset_history(rqos, DDS_HISTORY_KEEP_ALL, 0);
  ret = dds_set_qos(reader2, rqos);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
  ret = dds_get_qos(reader2, rqos);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
  dds_qget_history(rqos, &hist_kind,&hist_depth);
  CU_ASSERT_EQUAL_FATAL(hist_kind, DDS_HISTORY_KEEP_ALL);
  CU_ASSERT_EQUAL_FATAL(hist_depth, 0);
  /* try to enable the reader, this should fail because
   * the subscriber is not enabled */
  ret = dds_enable(reader2);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_PRECONDITION_NOT_MET);

  /* now enable the subscriber */
  ret = dds_enable(subscriber1);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
  /* check that the subscriber is still disabled by trying to set
   * an immutable qos. We use the presentation qos for that purpose */
  dds_qset_presentation(sqos, DDS_PRESENTATION_GROUP, true, false);
  ret = dds_set_qos(subscriber1, sqos);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_IMMUTABLE_POLICY);
  /* reader1 should remain disabled, check that reader1 can still
   * change one of its immutable qos settings */
  dds_qset_history(rqos, DDS_HISTORY_KEEP_ALL, 10);
  ret = dds_set_qos(reader1, rqos);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
  ret = dds_get_qos(reader1, rqos);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
  dds_qget_history(rqos, &hist_kind,&hist_depth);
  CU_ASSERT_EQUAL_FATAL(hist_kind, DDS_HISTORY_KEEP_ALL);
  CU_ASSERT_EQUAL_FATAL(hist_depth, 10);
  /* reader2 has become enabled when the subscriber was enabled.
   * Check that reader 2 cannot change its immutable qos setting */
  ret = dds_set_qos(reader2, rqos);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_IMMUTABLE_POLICY);
  /* now enable reader 1 */
  ret = dds_enable(reader1);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
  /* now check that we cannot change the immutable qos for reader1 any more */
  dds_qset_history(rqos, DDS_HISTORY_KEEP_ALL, 20);
  ret = dds_set_qos(reader1, rqos);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_IMMUTABLE_POLICY);

  /* check that re-enabling the subscriber, reader1 and reader2
   * return DDS_RETCODE_OK */
  ret = dds_enable(subscriber1);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
  ret = dds_enable(reader1);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
  ret = dds_enable(reader2);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

  /* Case 3: create an enabled subscriber with autoenable=false */
  dds_qset_entity_factory(sqos, false);
  subscriber2 = dds_create_subscriber(participant, sqos, NULL);
  ret = dds_set_qos(subscriber2, sqos);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
  ret = dds_get_qos(subscriber2, sqos);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
  status = dds_qget_entity_factory(sqos, &autoenable);
  CU_ASSERT_EQUAL_FATAL(status, true);
  CU_ASSERT_EQUAL_FATAL(autoenable, false);
  ret = dds_enable(subscriber2);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
  /* create a reader for case 3 and check that the reader is
   * disabled by trying to set an immutable qos. We use the
   * history qos for that purpose.*/
  reader3 = dds_create_reader(subscriber2, topic, NULL, NULL);
  CU_ASSERT_FATAL(reader3 > 0);
  /* check that reader can still change one of its immutable qos settings */
  dds_qset_history(rqos, DDS_HISTORY_KEEP_ALL, 30);
  ret = dds_set_qos(reader3, rqos);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
  ret = dds_get_qos(reader3, rqos);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
  dds_qget_history(rqos, &hist_kind,&hist_depth);
  CU_ASSERT_EQUAL_FATAL(hist_kind, DDS_HISTORY_KEEP_ALL);
  CU_ASSERT_EQUAL_FATAL(hist_depth, 30);

  /* Case 4: create a reader for an enabled subscriber with autoenable=true */
  ret = dds_get_qos(subscriber1, sqos);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
  status = dds_qget_entity_factory(sqos, &autoenable);
  CU_ASSERT_EQUAL_FATAL(status, true);
  CU_ASSERT_EQUAL_FATAL(autoenable, true);
  /* check that reader4 is automatically enabled by
   *  trying to set an immutable qos.  */
  reader4 = dds_create_reader(subscriber1, topic, NULL, NULL);
  CU_ASSERT_FATAL(reader4 > 0);
  dds_qset_history(rqos, DDS_HISTORY_KEEP_ALL, 40);
  ret = dds_set_qos(reader4, rqos);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_IMMUTABLE_POLICY);

  /* enable reader3 */
  ret = dds_enable(reader3);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
  dds_qset_history(rqos, DDS_HISTORY_KEEP_ALL, 50);
  ret = dds_set_qos(reader3, rqos);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_IMMUTABLE_POLICY);

  ret = dds_delete(participant);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
  dds_delete_qos(rqos);
  dds_delete_qos(sqos);
  dds_delete_qos(pqos);
}

/* The following test case tries to delete a reader that is disabled
 * by deleting the reader, its parent, and the parent of its parent */
CU_Test(ddsc_reader, delete_disabled_reader) {
  dds_entity_t participant, subscriber, reader, topic;
  dds_qos_t *sqos, *rqos;
  dds_return_t ret;
  dds_history_kind_t hist_kind;
  int32_t hist_depth;

  sqos = dds_create_qos();
  rqos = dds_create_qos();
  participant = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
  CU_ASSERT_FATAL(participant > 0);
  dds_qset_entity_factory(sqos, false);
  subscriber = dds_create_subscriber(participant, sqos, NULL);
  CU_ASSERT_FATAL(subscriber > 0);
  topic = dds_create_topic(participant, &Space_Type1_desc, "ddsc_reader_disabled", NULL, NULL);
  CU_ASSERT_FATAL(topic > 0);
  /* try to delete a disabled reader */
  reader = dds_create_reader(subscriber, topic, NULL, NULL);
  CU_ASSERT_FATAL(reader > 0);
  dds_qset_history(rqos, DDS_HISTORY_KEEP_ALL, 0);
  ret = dds_set_qos(reader, rqos);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
  dds_qget_history(rqos, &hist_kind,&hist_depth);
  CU_ASSERT_EQUAL_FATAL(hist_kind, DDS_HISTORY_KEEP_ALL);
  CU_ASSERT_EQUAL_FATAL(hist_depth, 0);
  ret = dds_delete(reader);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
  /* try to delete the parent of a disabled reader */
  reader = dds_create_reader(subscriber, topic, NULL, NULL);
  CU_ASSERT_FATAL(reader > 0);
  ret = dds_delete(subscriber);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
  /* try to delete the grandparent of a disabled reader */
  subscriber = dds_create_subscriber(participant, sqos, NULL);
  CU_ASSERT_FATAL(subscriber > 0);
  reader = dds_create_reader(subscriber, topic, NULL, NULL);
  CU_ASSERT_FATAL(reader > 0);
  ret = dds_delete(participant);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
  dds_delete_qos(sqos);
  dds_delete_qos(rqos);
}

/* The following test validates return codes for operations
 * on a disabled reader
 */
CU_Test(ddsc_reader, not_enabled) {
  dds_entity_t participant, subscriber, reader, topic;
  dds_qos_t *sqos, *rqos;
  dds_return_t ret;
  dds_history_kind_t hist_kind;
  int32_t hist_depth;
  void* samples[MAX_SAMPLES];
  static dds_sample_info_t  info[MAX_SAMPLES];
  uint32_t mask = DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
  struct ddsi_serdata *buf;
  dds_duration_t duration = DDS_SECS(1);

  sqos = dds_create_qos();
  rqos = dds_create_qos();
  participant = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
  CU_ASSERT_FATAL(participant > 0);
  dds_qset_entity_factory(sqos, false);
  subscriber = dds_create_subscriber(participant, sqos, NULL);
  CU_ASSERT_FATAL(subscriber > 0);
  topic = dds_create_topic(participant, &Space_Type1_desc, "ddsc_reader_disabled", NULL, NULL);
  CU_ASSERT_FATAL(topic > 0);
  /* create a disabled reader */
  reader = dds_create_reader(subscriber, topic, NULL, NULL);
  CU_ASSERT_FATAL(reader > 0);
  dds_qset_history(rqos, DDS_HISTORY_KEEP_ALL, 0);
  ret = dds_set_qos(reader, rqos);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
  dds_qget_history(rqos, &hist_kind,&hist_depth);
  CU_ASSERT_EQUAL_FATAL(hist_kind, DDS_HISTORY_KEEP_ALL);
  CU_ASSERT_EQUAL_FATAL(hist_depth, 0);
  /* dds_read */
  ret = dds_read(reader, samples, info, MAX_SAMPLES, MAX_SAMPLES);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_NOT_ENABLED);
  /* dds_read_wl */
  ret = dds_read_wl(reader, samples, info, MAX_SAMPLES);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_NOT_ENABLED);
  /* dds_read_mask */
  ret = dds_read_mask(reader, samples, info, MAX_SAMPLES, MAX_SAMPLES, mask);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_NOT_ENABLED);
  /* dds_read_mask_wl */
  ret = dds_read_mask_wl(reader, samples, info, MAX_SAMPLES, mask);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_NOT_ENABLED);
  /* dds_readcdr */
  ret = dds_readcdr(reader, &buf, MAX_SAMPLES, info, mask);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_NOT_ENABLED);
  /* TODO: dds_read_instance */
  /* TODO: dds_read_instance_wl */
  /* TODO: dds_read_instance_mask */
  /* TODO: dds_read_instance_mask_wl */
  /* TODO: dds_read_next */
  ret = dds_read_next(reader, samples, info);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_NOT_ENABLED);
  /* dds_read_next_wl */
  ret = dds_read_next_wl(reader, samples, info);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_NOT_ENABLED);
  /* dds_take */
  ret = dds_take(reader, samples, info, MAX_SAMPLES, MAX_SAMPLES);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_NOT_ENABLED);
  /* dds_take_wl */
  ret = dds_take_wl(reader, samples, info, MAX_SAMPLES);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_NOT_ENABLED);
  /* dds_take_mask */
  ret = dds_take_mask(reader, samples, info, MAX_SAMPLES, MAX_SAMPLES, mask);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_NOT_ENABLED);
  /* dds_take_mask_wl */
  ret = dds_read_mask_wl(reader, samples, info, MAX_SAMPLES, mask);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_NOT_ENABLED);
  /* dds_takecdr */
  ret = dds_takecdr(reader, &buf, MAX_SAMPLES, info, mask);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_NOT_ENABLED);
  /* TODO: dds_take_instance */
  /* TODO: dds_take_instance_wl */
  /* TODO: dds_take_instance_mask */
  /* TODO: dds_take_instance_mask_wl */
  /* YODO: dds_take_next */
  ret = dds_take_next(reader, samples, info);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_NOT_ENABLED);
  /* dds_take_next_wl */
  ret = dds_take_next_wl (reader, samples, info);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_NOT_ENABLED);
  /* dds_return_loan */
  /* dds_reader_wait_for_historical_data */
  ret = dds_reader_wait_for_historical_data (reader, duration);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_NOT_ENABLED);
  ret = dds_delete(participant);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
  dds_delete_qos(sqos);
  dds_delete_qos(rqos);
}
