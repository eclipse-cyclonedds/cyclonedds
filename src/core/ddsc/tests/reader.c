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
#include "dds/ddsrt/misc.h"
#include "dds/ddsrt/process.h"
#include "dds/ddsrt/threads.h"

#include "dds__entity.h"
#include "test_common.h"

// Because all the _wl variants are deprecated
DDSRT_WARNING_DEPRECATED_OFF

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

CU_Test(ddsc_reader_create, topic_lifespan)
{
  dds_return_t rc;
  dds_entity_t pp = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
  CU_ASSERT_FATAL (pp > 0);
  char name[100];
  dds_qos_t *qos = dds_create_qos ();
  CU_ASSERT_FATAL (qos != NULL);
  dds_qset_lifespan (qos, DDS_SECS (3)); // value doesn't matter
  create_unique_topic_name ("ddsc_reader_create_topic_lifespan", name, sizeof name);
  dds_entity_t tp = dds_create_topic (pp, &Space_Type1_desc, name, qos, NULL);
  dds_delete_qos (qos);
  CU_ASSERT_FATAL (tp > 0);
  dds_entity_t rd = dds_create_reader (pp, tp, NULL, NULL);
  CU_ASSERT_FATAL (rd > 0);

  // need to look at the QoS object as it is actually stored: we don't want to
  // overlook the LIFESPAN making it into the reader and then not observing it
  // because get_qos does something funny
  struct dds_entity *x;
  rc = dds_entity_pin (rd, &x);
  CU_ASSERT_FATAL (rc == 0);
  CU_ASSERT_FATAL (x->m_qos != NULL);
  CU_ASSERT (!dds_qget_lifespan (x->m_qos, NULL));
  dds_entity_unpin (x);
  rc = dds_delete (pp);
  CU_ASSERT_FATAL (rc == 0);
}



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
/*************************************************************************************************/

static bool test_even_long_2_and_long_3_neq_0 (const void *vsample)
{
  const Space_Type1 *sample = vsample;
  return (sample->long_2 % 2) == 0 && sample->long_3 != 0;
}

static bool test_long_2_eq_2 (const void *vsample)
{
  const Space_Type1 *sample = vsample;
  return (sample->long_2 == 2);
}

static void do_readtake_sample_rank (const char *opname_past_tense, dds_return_t (*op) (dds_entity_t reader_or_condition, void **buf, dds_sample_info_t *si, size_t bufsz, uint32_t maxs, uint32_t mask))
{
  const dds_entity_t dp = dds_create_participant (0, NULL, NULL);
  CU_ASSERT_FATAL (dp > 0);
  char topicname[100];
  dds_qos_t *qos = dds_create_qos ();
  dds_qset_history (qos, DDS_HISTORY_KEEP_ALL, 0);
  create_unique_topic_name ("readtake_sample_rank", topicname, sizeof (topicname));
  const dds_entity_t tp = dds_create_topic (dp, &Space_Type1_desc, topicname, qos, NULL);
  CU_ASSERT_FATAL (tp > 0);
  dds_delete_qos (qos);

  dds_return_t rc;
  int nsamp[4] = { 1, 0, 0, 0};
  Space_Type1 xs[13];
  void *buf[sizeof (xs) / sizeof (xs[0])];
  dds_sample_info_t si[sizeof (xs) / sizeof (xs[0])];
  for (size_t i = 0; i < sizeof (xs) / sizeof (xs[0]); i++)
    buf[i] = &xs[i];
  while (nsamp[3] == 0)
  {
    int totsamp = 0;
    printf ("samples/instance: ");
    for (int i = 0; nsamp[i] > 0; i++) {
      totsamp += nsamp[i];
      printf (" %d", nsamp[i]);
    }
    printf ("\n");
    fflush (stdout);

    for (int skip_even_long_2 = 0; skip_even_long_2 <= 1; skip_even_long_2++)
    {
      printf ("  skip_even_long_2 %d\n", skip_even_long_2);
      int totsamp_after_maybe_skipping_even = 0;
      if (!skip_even_long_2)
        totsamp_after_maybe_skipping_even = totsamp;
      else
      {
        for (int i = 0; nsamp[i] > 0; i++)
          totsamp_after_maybe_skipping_even += nsamp[i] / 2;
      }

      for (int rdlimit = 1; rdlimit < totsamp + 1; rdlimit++)
      {
        const dds_entity_t rd = dds_create_reader (dp, tp, NULL, NULL);
        CU_ASSERT_FATAL (rd > 0);
        const dds_entity_t wr = dds_create_writer (dp, tp, NULL, NULL);
        CU_ASSERT_FATAL (wr > 0);

        // write as many samples as planned for each instance,
        // add an invalid one iff # == 4
        //
        // constructing an invalid sample requires reading the
        // already published data before disposing it
        for (int i = 0; nsamp[i] > 0; i++)
        {
          const int nwrites = (nsamp[i] <= 3) ? nsamp[i] : 3;
          for (int s = 0; s < nwrites; s++)
          {
            // Need invalid samples are also processed by the query condition (for better
            // or for worse, because it doesn't get told whether it is valid).  For the
            // test, use long_3 to not match an invalid sample.
            rc = dds_write (wr, &(Space_Type1){ .long_1 = i, .long_2 = s, .long_3 = 1 });
            CU_ASSERT_FATAL (rc == 0);
          }
          if (nsamp[i] > nwrites)
          {
            // Cyclone reader/writer instance handles are the same, so this'll work for reading
            // only the samples we just wrote.  The obvious simpler alternative would be to first
            // do all the writes, then read everything once, then do the disposes.  But why would
            // one ever want to always do things the simple way?
            dds_instance_handle_t ih;
            rc = dds_register_instance (wr, &ih, &(Space_Type1){ .long_1 = i, .long_2 = 0, .long_3 = 0 });
            CU_ASSERT_FATAL (rc == 0);
            assert (nwrites == 3);
            const dds_entity_t qc = dds_create_querycondition (rd, 0, test_long_2_eq_2);
            rc = dds_read_instance (qc, buf, si, 1, 1, ih);
            CU_ASSERT_FATAL (rc == 1);
            CU_ASSERT_FATAL (xs[0].long_1 == i && xs[0].long_2 == 2 && xs[0].long_3 == 1);
            rc = dds_delete (qc);
            CU_ASSERT_FATAL (rc == 0);
            rc = dds_dispose (wr, &(Space_Type1){ .long_1 = i, .long_2 = 0, .long_3 = 0 });
            CU_ASSERT_FATAL (rc == 0);
          }
        }

        // Skipping everything with an even value for long_2 in the read where check the sample_rank
        // - contents of each instance is a prefix of:
        //     (unread,   valid, .long_2 = 0, .long_3 = 1)
        //     (unread,   valid, .long_2 = 1, .long_3 = 1)
        //     (   X  ,   valid, .long_2 = 2, .long_3 = 1) -- X is unread if nwrites < 3 else read
        //     (unread, invalid, .long_2 = 0, .long_3 = 0)
        //   where long_2 = 0 on the invalid sample is because it is an attribute
        // - reading all samples with even long 2 & long_3 != 0 changes this to
        //     (  read,   valid, .long_2 = 0, .long_3 = 1)
        //     (unread,   valid, .long_2 = 1, .long_3 = 1)
        //     (  read,   valid, .long_2 = 2, .long_3 = 1)
        //     (unread, invalid, .long_2 = 0, .long_3 = 0)
        if (skip_even_long_2)
        {
          const dds_entity_t qc = dds_create_querycondition (rd, 0, test_even_long_2_and_long_3_neq_0);
          rc = dds_read (qc, buf, si, sizeof (xs) / sizeof (xs[0]), (uint32_t) (sizeof (xs) / sizeof (xs[0])));
          CU_ASSERT_FATAL (rc >= 0);
          for (int i = 0; i < rc; i++)
            CU_ASSERT_FATAL ((xs[i].long_2 % 2) == 0);
          rc = dds_delete (qc);
          CU_ASSERT_FATAL (rc == 0);
        }

        assert (rdlimit < (int) (sizeof (xs) / sizeof (xs[0])));
        int n = (int) op (rd, buf, si, (size_t) rdlimit, (uint32_t) rdlimit, (skip_even_long_2 ? DDS_NOT_READ_SAMPLE_STATE : 0));
        printf ("  -- %s %d (<= %d), ranks:", opname_past_tense, n, rdlimit);
        for (int i = 0; i < n; i++)
          printf (" %"PRIu32, si[i].sample_rank);
        printf ("\n");
        CU_ASSERT_FATAL (n == ((rdlimit <= totsamp_after_maybe_skipping_even) ? rdlimit : totsamp_after_maybe_skipping_even));

        {
          int i = 0;
          while (i < n)
          {
            CU_ASSERT_FATAL (0 <= xs[i].long_1 && xs[i].long_1 <= 2);
            CU_ASSERT_FATAL (!skip_even_long_2 || (xs[i].long_2 % 2) != 0);
            const int nsamp_after_maybe_skipping_even = skip_even_long_2 ? nsamp[xs[i].long_1] / 2 : nsamp[xs[i].long_1];
            const int nsamp_of_inst = (i + nsamp_after_maybe_skipping_even < n) ? nsamp_after_maybe_skipping_even : n - i;
            //printf ("i = %d nsamp_of_inst = %d\n", i, nsamp_of_inst);
            assert (nsamp_of_inst > 0);
            CU_ASSERT_FATAL (si[i].sample_rank == (uint32_t) (nsamp_of_inst - 1));
            CU_ASSERT_FATAL ((!si[i].valid_data && xs[i].long_3 == 0) || (si[i].valid_data && xs[i].long_3 == 1));
            CU_ASSERT_FATAL ((int) si[i].sample_rank < nsamp_of_inst);
            const int first_of_inst = i;
            for (int j = 0; j < nsamp_of_inst; j++)
            {
              CU_ASSERT_FATAL (si[i + j].instance_handle == si[first_of_inst].instance_handle);
              CU_ASSERT_FATAL (si[i + j].sample_rank == (uint32_t) (nsamp_of_inst - j - 1));
              CU_ASSERT_FATAL (xs[i + j].long_2 == (skip_even_long_2) ? 2*j+1 : j);
            }
            i += nsamp_of_inst;
          }
        }

        rc = dds_delete (rd);
        CU_ASSERT_FATAL (rc == 0);
        rc = dds_delete (wr);
        CU_ASSERT_FATAL (rc == 0);
      }
    }

    // increment # samples per instance
    { int i = 0; while (++nsamp[i] == 5) nsamp[i++] = 1; }
  }

  rc = dds_delete (dp);
  CU_ASSERT_FATAL (rc == 0);
}

CU_Test (ddsc_read, sample_rank)
{
  do_readtake_sample_rank ("read", dds_read_mask);
}

CU_Test (ddsc_take, sample_rank)
{
  do_readtake_sample_rank ("took", dds_take_mask);
}

static void ddsc_peek_setup (dds_entity_t *dp, dds_entity_t *rd, dds_instance_handle_t *ih)
{
  *dp = dds_create_participant (0, NULL, NULL);
  CU_ASSERT_FATAL (*dp > 0);
  char topicname[100];
  create_unique_topic_name ("ddsc_peek", topicname, sizeof (topicname));
  const dds_entity_t tp = dds_create_topic (*dp, &Space_Type1_desc, topicname, NULL, NULL);
  CU_ASSERT_FATAL (tp > 0);
  *rd = dds_create_reader (*dp, tp, NULL, NULL);
  CU_ASSERT_FATAL (*rd > 0);
  const dds_entity_t wr = dds_create_writer (*dp, tp, NULL, NULL);
  CU_ASSERT_FATAL (wr > 0);
  dds_return_t rc;
  rc = dds_write (wr, &(Space_Type1){ 2,2,2 });
  CU_ASSERT_FATAL (rc == 0);
  rc = dds_write (wr, &(Space_Type1){ 3,3,3 });
  CU_ASSERT_FATAL (rc == 0);
  rc = dds_write (wr, &(Space_Type1){ 5,5,5 });
  CU_ASSERT_FATAL (rc == 0);
  *ih = dds_lookup_instance (*rd, &(Space_Type1){ 3,3,3 });
  CU_ASSERT_FATAL (*ih != 0);
}

CU_Test (ddsc_peek, plain)
{
  dds_entity_t dp, rd;
  dds_instance_handle_t ih;
  dds_entity_t rc;
  ddsc_peek_setup (&dp, &rd, &ih);
  dds_sample_info_t si[3];
  Space_Type1 xs[3];
  void *ptrs[] = { &xs[0], &xs[1], &xs[2] };
  for (int k = 0; k < 2; k++)
  {
    uint32_t seen = 0;
    rc = dds_peek (rd, ptrs, si, 3, 3);
    CU_ASSERT_FATAL (rc == 3);
    for (int32_t i = 0; i < rc; i++)
    {
      CU_ASSERT_FATAL (xs[i].long_1 == xs[i].long_2 && xs[i].long_2 == xs[i].long_3);
      CU_ASSERT_FATAL (xs[i].long_1 <= 31);
      seen |= 1u << xs[i].long_1;
      CU_ASSERT_FATAL (si[i].view_state == DDS_NEW_VIEW_STATE);
      CU_ASSERT_FATAL (si[i].sample_state == DDS_NOT_READ_SAMPLE_STATE);
    }
    CU_ASSERT_FATAL (seen == 44u);
  }
  rc = dds_delete (dp);
  CU_ASSERT_FATAL (rc == 0);
}

CU_Test (ddsc_peek, mask)
{
  dds_entity_t dp, rd;
  dds_instance_handle_t ih;
  dds_entity_t rc;
  ddsc_peek_setup (&dp, &rd, &ih);
  dds_sample_info_t si[3];
  Space_Type1 xs[3];
  void *ptrs[] = { &xs[0], &xs[1], &xs[2] };
  rc = dds_read (rd, ptrs, si, 1, 1);
  CU_ASSERT_FATAL (rc == 1 && xs[0].long_1 <= 31);
  const uint32_t not_visible = 1u << xs[0].long_1;
  // dds_read gest tested elsewhere, no need to redo that here
  for (int k = 0; k < 2; k++)
  {
    uint32_t seen = 0;
    rc = dds_peek_mask (rd, ptrs, si, 3, 3, DDS_NOT_READ_SAMPLE_STATE);
    CU_ASSERT_FATAL (rc == 2);
    for (int32_t i = 0; i < rc; i++)
    {
      CU_ASSERT_FATAL (xs[i].long_1 == xs[i].long_2 && xs[i].long_2 == xs[i].long_3);
      CU_ASSERT_FATAL (xs[i].long_1 <= 31);
      seen |= 1u << xs[i].long_1;
      CU_ASSERT_FATAL (si[i].view_state == DDS_NEW_VIEW_STATE);
      CU_ASSERT_FATAL (si[i].sample_state == DDS_NOT_READ_SAMPLE_STATE);
    }
    CU_ASSERT_FATAL (seen == (44u & ~not_visible));
  }
  rc = dds_delete (dp);
  CU_ASSERT_FATAL (rc == 0);
}

CU_Test (ddsc_peek, instance)
{
  dds_entity_t dp, rd;
  dds_instance_handle_t ih;
  dds_entity_t rc;
  ddsc_peek_setup (&dp, &rd, &ih);
  dds_sample_info_t si[3];
  Space_Type1 xs[3];
  void *ptrs[] = { &xs[0], &xs[1], &xs[2] };
  rc = dds_peek_instance (rd, ptrs, si, 3, 3, ih);
  CU_ASSERT_FATAL (rc == 1);
  CU_ASSERT_FATAL (xs[0].long_1 == xs[0].long_2 && xs[0].long_2 == xs[0].long_3);
  CU_ASSERT_FATAL (xs[0].long_1 <= 31);
  const int32_t expect = xs[0].long_1;
  CU_ASSERT_FATAL (si[0].view_state == DDS_NEW_VIEW_STATE);
  CU_ASSERT_FATAL (si[0].sample_state == DDS_NOT_READ_SAMPLE_STATE);
  rc = dds_peek_instance (rd, ptrs, si, 3, 3, ih);
  CU_ASSERT_FATAL (rc == 1);
  CU_ASSERT_FATAL (xs[0].long_1 == xs[0].long_2 && xs[0].long_2 == xs[0].long_3);
  CU_ASSERT_FATAL (xs[0].long_1 == expect);
  CU_ASSERT_FATAL (si[0].view_state == DDS_NEW_VIEW_STATE);
  CU_ASSERT_FATAL (si[0].sample_state == DDS_NOT_READ_SAMPLE_STATE);
  rc = dds_delete (dp);
  CU_ASSERT_FATAL (rc == 0);
}

CU_Test (ddsc_peek, instance_mask)
{
  dds_entity_t dp, rd;
  dds_instance_handle_t ih;
  dds_entity_t rc;
  ddsc_peek_setup (&dp, &rd, &ih);
  dds_sample_info_t si[3];
  Space_Type1 xs[3];
  void *ptrs[] = { &xs[0], &xs[1], &xs[2] };
  rc = dds_read_instance (rd, ptrs, si, 1, 1, ih);
  CU_ASSERT_FATAL (rc == 1 && xs[0].long_1 <= 31);
  const int32_t expect = xs[0].long_1;
  rc = dds_peek_instance_mask (rd, ptrs, si, 3, 3, ih, DDS_NOT_READ_SAMPLE_STATE);
  CU_ASSERT_FATAL (rc == 0);
  rc = dds_peek_instance_mask (rd, ptrs, si, 3, 3, ih, DDS_READ_SAMPLE_STATE);
  CU_ASSERT_FATAL (rc == 1);
  CU_ASSERT_FATAL (xs[0].long_1 == xs[0].long_2 && xs[0].long_2 == xs[0].long_3);
  CU_ASSERT_FATAL (xs[0].long_1 == expect);
  CU_ASSERT_FATAL (si[0].view_state == DDS_NOT_NEW_VIEW_STATE);
  CU_ASSERT_FATAL (si[0].sample_state == DDS_READ_SAMPLE_STATE);
  rc = dds_delete (dp);
  CU_ASSERT_FATAL (rc == 0);
}

CU_Test (ddsc_peek, next)
{
  dds_entity_t dp, rd;
  dds_instance_handle_t ih;
  dds_entity_t rc;
  ddsc_peek_setup (&dp, &rd, &ih);
  dds_sample_info_t si[3];
  Space_Type1 xs[3];
  void *ptrs[] = { &xs[0], &xs[1], &xs[2] };
  // not much of an expectation yet, but at least it should return something
  // unread even if we try it a couple of times
  for (int i = 0; i < 4; i++)
  {
    rc = dds_peek_next (rd, ptrs, si);
    CU_ASSERT_FATAL (rc == 1);
    CU_ASSERT_FATAL (xs[0].long_1 == xs[0].long_2 && xs[0].long_2 == xs[0].long_3);
    CU_ASSERT_FATAL (si[0].view_state == DDS_NEW_VIEW_STATE);
    CU_ASSERT_FATAL (si[0].sample_state == DDS_NOT_READ_SAMPLE_STATE);
  }
  rc = dds_delete (dp);
  CU_ASSERT_FATAL (rc == 0);
}
