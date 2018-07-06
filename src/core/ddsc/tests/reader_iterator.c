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
#include "Space.h"
#include "RoundTrip.h"
#include <criterion/criterion.h>
#include <criterion/logging.h>
#include <criterion/theories.h>

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

/*
 * By writing, disposing, unregistering, reading and re-writing, the following
 * data will be available in the reader history (but not in this order).
 *    | long_1 | long_2 | long_3 |    sst   | vst |    ist     |
 *    ----------------------------------------------------------
 *    |    0   |    0   |    0   | not_read | new | alive      |
 *    |    0   |    1   |    2   | not_read | new | alive      |
 *    |    0   |    2   |    4   | not_read | new | alive      |
 *    |    1   |    3   |    6   |     read | old | alive      |
 *    |    1   |    4   |    8   |     read | old | alive      |
 *    |    1   |    5   |   10   |     read | old | alive      |
 *    |    2   |    6   |   12   | not_read | old | alive      |
 *    |    2   |    7   |   14   | not_read | old | alive      |
 *    |    2   |    8   |   16   |     read | old | alive      |
 *    |    3   |    9   |   18   | not_read | old | alive      |
 *    |    3   |   10   |   20   |     read | old | alive      |
 *    |    3   |   11   |   22   | not_read | old | alive      |
 *    |    4   |   12   |   24   |     read | old | alive      |
 *    |    4   |   13   |   26   | not_read | old | alive      |
 *    |    4   |   14   |   28   | not_read | old | alive      |
 *    |    5   |   15   |   30   |     read | old | disposed   |
 *    |    5   |   16   |   32   | not_read | old | disposed   |
 *    |    5   |   17   |   34   |     read | old | disposed   |
 *    |    6   |   18   |   36   |     read | old | no_writers |
 *    |    6   |   19   |   38   | not_read | old | no_writers |
 *    |    6   |   20   |   40   |     read | old | no_writers |
 *
 */
#define MAX_SAMPLES                 21

#define RDR_NOT_READ_CNT            11
#define RDR_INV_READ_CNT             1
int rdr_expected_long_2[RDR_NOT_READ_CNT] = { 0, 1, 2, 6, 7, 9, 11, 13, 14, 16, 19 };

/* Because we only read one sample at a time, only the first sample of an instance
 * can be new. This turns out to be only the very first sample.  */
#define SAMPLE_VST(long_2)           ((long_2 == 0) ? DDS_VST_NEW : DDS_VST_OLD)

#define SAMPLE_IST(long_1)           ((long_1 == 5) ? DDS_IST_NOT_ALIVE_DISPOSED   : \
                                      (long_1 == 6) ? DDS_IST_NOT_ALIVE_NO_WRITERS : \
                                                      DDS_IST_ALIVE                )

static dds_entity_t g_participant = 0;
static dds_entity_t g_subscriber  = 0;
static dds_entity_t g_publisher   = 0;
static dds_entity_t g_topic       = 0;
static dds_entity_t g_reader      = 0;
static dds_entity_t g_writer      = 0;
static dds_entity_t g_waitset     = 0;
static dds_entity_t g_rcond       = 0;
static dds_entity_t g_qcond       = 0;

static void*              g_loans[MAX_SAMPLES];
static void*              g_samples[MAX_SAMPLES];
static Space_Type1        g_data[MAX_SAMPLES];
static dds_sample_info_t  g_info[MAX_SAMPLES];

static dds_instance_handle_t   g_hdl_valid;
static dds_instance_handle_t   g_hdl_nil = DDS_HANDLE_NIL;

static char*
create_topic_name(const char *prefix, char *name, size_t size)
{
    /* Get semi random g_topic name. */
    os_procId pid = os_procIdSelf();
    uintmax_t tid = os_threadIdToInteger(os_threadIdSelf());
    (void) snprintf(name, size, "%s_pid%"PRIprocId"_tid%"PRIuMAX"", prefix, pid, tid);
    return name;
}

static bool
filter_init(const void * sample)
{
    const Space_Type1 *s = sample;
    return ((s->long_2 ==  3) ||
            (s->long_2 ==  4) ||
            (s->long_2 ==  5) ||
            (s->long_2 ==  8) ||
            (s->long_2 == 10) ||
            (s->long_2 == 12) ||
            (s->long_2 == 15) ||
            (s->long_2 == 17) ||
            (s->long_2 == 18) ||
            (s->long_2 == 20));
}

static bool
filter_mod2(const void * sample)
{
    const Space_Type1 *s = sample;
    return (s->long_2 % 2 == 0);
}

static void
reader_iterator_init(void)
{
    Space_Type1 sample = { 0 };
    dds_attach_t triggered;
    dds_return_t ret;
    char name[100];
    dds_qos_t *qos;

    qos = dds_qos_create();
    cr_assert_not_null(qos, "Failed to create prerequisite qos");

    g_participant = dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);
    cr_assert_gt(g_participant, 0, "Failed to create prerequisite g_participant");

    g_subscriber = dds_create_subscriber(g_participant, NULL, NULL);
    cr_assert_gt(g_subscriber, 0, "Failed to create prerequisite g_subscriber");

    g_publisher = dds_create_publisher(g_participant, NULL, NULL);
    cr_assert_gt(g_publisher, 0, "Failed to create prerequisite g_publisher");

    g_waitset = dds_create_waitset(g_participant);
    cr_assert_gt(g_waitset, 0, "Failed to create g_waitset");

    g_topic = dds_create_topic(g_participant, &Space_Type1_desc, create_topic_name("ddsc_read_iterator_test", name, sizeof name), NULL, NULL);
    cr_assert_gt(g_topic, 0, "Failed to create prerequisite g_topic");

    /* Create a writer that will not automatically dispose unregistered samples. */
    dds_qset_writer_data_lifecycle(qos, false);
    g_writer = dds_create_writer(g_publisher, g_topic, qos, NULL);
    cr_assert_gt(g_writer, 0, "Failed to create prerequisite g_writer");

    /* Create a reader that keeps all samples when not taken. */
    dds_qset_history(qos, DDS_HISTORY_KEEP_ALL, DDS_LENGTH_UNLIMITED);
    g_reader = dds_create_reader(g_subscriber, g_topic, qos, NULL);
    cr_assert_gt(g_reader, 0, "Failed to create prerequisite g_reader");

    /* Create a read condition that only reads old samples. */
    g_rcond = dds_create_readcondition(g_reader, DDS_NOT_READ_SAMPLE_STATE | DDS_NOT_NEW_VIEW_STATE | DDS_ANY_INSTANCE_STATE);
    cr_assert_gt(g_rcond, 0, "Failed to create prerequisite g_rcond");

    /* Create a query condition that only reads of instances mod2. */
    g_qcond = dds_create_querycondition(g_reader, DDS_NOT_READ_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE, filter_mod2);
    cr_assert_gt(g_qcond, 0, "Failed to create prerequisite g_qcond");

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
    for (int i = 0; i < MAX_SAMPLES; i++) {
        g_loans[i] = NULL;
    }


    /* Write the samples. */
    for (int i = 0; i < MAX_SAMPLES; i++) {
        sample.long_1 = i/3;
        sample.long_2 = i;
        sample.long_3 = i*2;
        ret = dds_write(g_writer, &sample);
        cr_assert_eq(ret, DDS_RETCODE_OK, "Failed prerequisite write");
    }
    /*    | long_1 | long_2 | long_3 |    sst   | vst | ist   |
     *    -----------------------------------------------------
     *    |    0   |    0   |    0   | not_read | new | alive |
     *    |    0   |    1   |    2   | not_read | new | alive |
     *    |    0   |    2   |    4   | not_read | new | alive |
     *    |    1   |    3   |    6   | not_read | new | alive |
     *    |    1   |    4   |    8   | not_read | new | alive |
     *    |    1   |    5   |   10   | not_read | new | alive |
     *    |    2   |    6   |   12   | not_read | new | alive |
     *    |    2   |    7   |   14   | not_read | new | alive |
     *    |    2   |    8   |   16   | not_read | new | alive |
     *    |    3   |    9   |   18   | not_read | new | alive |
     *    |    3   |   10   |   20   | not_read | new | alive |
     *    |    3   |   11   |   22   | not_read | new | alive |
     *    |    4   |   12   |   24   | not_read | new | alive |
     *    |    4   |   13   |   26   | not_read | new | alive |
     *    |    4   |   14   |   28   | not_read | new | alive |
     *    |    5   |   15   |   30   | not_read | new | alive |
     *    |    5   |   16   |   32   | not_read | new | alive |
     *    |    5   |   17   |   34   | not_read | new | alive |
     *    |    6   |   18   |   36   | not_read | new | alive |
     *    |    6   |   19   |   38   | not_read | new | alive |
     *    |    6   |   20   |   40   | not_read | new | alive |
     */

    /* Set the sst to read for the proper samples by using a query
     * condition that filters for these specific samples. */
    {
        dds_entity_t qcond = 0;

        /* Create a query condition that reads the specific sample to get a set of 'read' samples after init. */
        qcond = dds_create_querycondition(g_reader, DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE, filter_init);
        cr_assert_gt(g_qcond, 0, "Failed to create prerequisite qcond");

        ret = dds_read(qcond, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
        cr_assert_gt(ret, 0, "Failed prerequisite read: %d", dds_err_nr(ret));

        dds_delete(qcond);
    }
    /*    | long_1 | long_2 | long_3 |    sst   | vst |  ist  |
     *    -----------------------------------------------------
     *    |    0   |    0   |    0   | not_read | new | alive |
     *    |    0   |    1   |    2   | not_read | new | alive |
     *    |    0   |    2   |    4   | not_read | new | alive |
     *    |    1   |    3   |    6   |     read | old | alive |
     *    |    1   |    4   |    8   |     read | old | alive |
     *    |    1   |    5   |   10   |     read | old | alive |
     *    |    2   |    6   |   12   | not_read | old | alive |
     *    |    2   |    7   |   14   | not_read | old | alive |
     *    |    2   |    8   |   16   |     read | old | alive |
     *    |    3   |    9   |   18   | not_read | old | alive |
     *    |    3   |   10   |   20   |     read | old | alive |
     *    |    3   |   11   |   22   | not_read | old | alive |
     *    |    4   |   12   |   24   |     read | old | alive |
     *    |    4   |   13   |   26   | not_read | old | alive |
     *    |    4   |   14   |   28   | not_read | old | alive |
     *    |    5   |   15   |   30   |     read | old | alive |
     *    |    5   |   16   |   32   | not_read | old | alive |
     *    |    5   |   17   |   34   |     read | old | alive |
     *    |    6   |   18   |   36   |     read | old | alive |
     *    |    6   |   19   |   38   | not_read | old | alive |
     *    |    6   |   20   |   40   |     read | old | alive |
     */


    /* Dispose and unregister the last two samples. */
    sample.long_1 = 5;
    sample.long_2 = 15;
    sample.long_3 = 30;
    ret = dds_dispose(g_writer, &sample);
    cr_assert_eq(ret, DDS_RETCODE_OK, "Failed prerequisite dispose");
    sample.long_1 = 6;
    sample.long_2 = 16;
    sample.long_3 = 32;
    ret = dds_unregister_instance(g_writer, &sample);
    cr_assert_eq(ret, DDS_RETCODE_OK, "Failed prerequisite unregister");
    /*    | long_1 | long_2 | long_3 |    sst   | vst |    ist     |
     *    ----------------------------------------------------------
     *    |    0   |    0   |    0   | not_read | new | alive      |
     *    |    0   |    1   |    2   | not_read | new | alive      |
     *    |    0   |    2   |    4   | not_read | new | alive      |
     *    |    1   |    3   |    6   |     read | old | alive      |
     *    |    1   |    4   |    8   |     read | old | alive      |
     *    |    1   |    5   |   10   |     read | old | alive      |
     *    |    2   |    6   |   12   | not_read | old | alive      |
     *    |    2   |    7   |   14   | not_read | old | alive      |
     *    |    2   |    8   |   16   |     read | old | alive      |
     *    |    3   |    9   |   18   | not_read | old | alive      |
     *    |    3   |   10   |   20   |     read | old | alive      |
     *    |    3   |   11   |   22   | not_read | old | alive      |
     *    |    4   |   12   |   24   |     read | old | alive      |
     *    |    4   |   13   |   26   | not_read | old | alive      |
     *    |    4   |   14   |   28   | not_read | old | alive      |
     *    |    5   |   15   |   30   |     read | old | disposed   |
     *    |    5   |   16   |   32   | not_read | old | disposed   |
     *    |    5   |   17   |   34   |     read | old | disposed   |
     *    |    6   |   18   |   36   |     read | old | no_writers |
     *    |    6   |   19   |   38   | not_read | old | no_writers |
     *    |    6   |   20   |   40   |     read | old | no_writers |
     */

    dds_qos_delete(qos);
}

static void
reader_iterator_fini(void)
{
    dds_delete(g_rcond);
    dds_delete(g_qcond);
    dds_delete(g_reader);
    dds_delete(g_writer);
    dds_delete(g_subscriber);
    dds_delete(g_publisher);
    dds_delete(g_waitset);
    dds_delete(g_topic);
    dds_delete(g_participant);
}

static dds_return_t
samples_cnt(void)
{
    dds_return_t ret;
    ret = dds_read(g_reader, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
    cr_assert_geq(ret, 0, "Failed samples count read: %d", dds_err_nr(ret));
    return ret;
}

/**************************************************************************************************
 *
 * These will check the dds_read_next() in various ways.
 *
 *************************************************************************************************/
/*************************************************************************************************/
Test(ddsc_read_next, reader, .init=reader_iterator_init, .fini=reader_iterator_fini)
{
    dds_return_t cnt = 0, cntinv = 0;
    dds_return_t ret = 1;

    while (ret == 1){
      ret = dds_read_next(g_reader, g_samples, g_info);
      cr_assert_geq(ret, 0 , "# read %d", ret);
      if(ret == 1 && g_info[0].valid_data){
        Space_Type1 *sample = (Space_Type1*)g_samples[0];
        PRINT_SAMPLE("ddsc_read_next::reader: Read", (*sample));

        /* Expected states. */
        int                  expected_long_2 = rdr_expected_long_2[cnt];
        int                  expected_long_1 = expected_long_2/3;
        int                  expected_long_3 = expected_long_2*2;
        dds_sample_state_t   expected_sst    = DDS_SST_NOT_READ;
        dds_view_state_t     expected_vst    = SAMPLE_VST(expected_long_2);
        dds_instance_state_t expected_ist    = SAMPLE_IST(expected_long_1);

        /* Check data. */
        cr_assert_eq(sample->long_1, expected_long_1 );
        cr_assert_eq(sample->long_2, expected_long_2   );
        cr_assert_eq(sample->long_3, expected_long_2 *2);

        /* Check states. */
        cr_assert_eq(g_info[0].valid_data,     true);
        cr_assert_eq(g_info[0].sample_state,   expected_sst);
        cr_assert_eq(g_info[0].view_state,     expected_vst);
        cr_assert_eq(g_info[0].instance_state, expected_ist);
        cnt ++;
      } else if (ret == 1 && !g_info[0].valid_data) {
        cntinv ++;
      }
    }

    cr_assert_eq(cnt, RDR_NOT_READ_CNT);
    cr_assert_eq(cntinv, RDR_INV_READ_CNT);

    /* All samples should still be available. */
    ret = samples_cnt();
    cr_assert_eq(ret, MAX_SAMPLES, "# samples %d, expected %d", ret, MAX_SAMPLES);

}
/*************************************************************************************************/

/*************************************************************************************************/
TheoryDataPoints(ddsc_read_next, invalid_readers) = {
        DataPoints(dds_entity_t, -2, -1, 0, 1, 100, INT_MAX, INT_MIN),
};
Theory((dds_entity_t rdr), ddsc_read_next, invalid_readers, .init=reader_iterator_init, .fini=reader_iterator_fini)
{
    dds_entity_t exp = DDS_RETCODE_BAD_PARAMETER * -1;
    dds_return_t ret;

    ret = dds_read_next(rdr, g_samples, g_info);
    cr_assert_eq(dds_err_nr(ret), dds_err_nr(exp), "returned %d != expected %d", dds_err_str(ret), dds_err_nr(exp));
}
/*************************************************************************************************/

/*************************************************************************************************/
TheoryDataPoints(ddsc_read_next, non_readers) = {
        DataPoints(dds_entity_t*, &g_participant, &g_topic, &g_writer, &g_subscriber, &g_publisher, &g_waitset, &g_rcond, &g_qcond),
};
Theory((dds_entity_t *rdr), ddsc_read_next, non_readers, .init=reader_iterator_init, .fini=reader_iterator_fini)
{
    dds_return_t ret;
    ret = dds_read_next(*rdr, g_samples, g_info);
    cr_assert_eq(dds_err_nr(ret), DDS_RETCODE_ILLEGAL_OPERATION, "returned %d", dds_err_nr(ret));
}
/*************************************************************************************************/

/*************************************************************************************************/
TheoryDataPoints(ddsc_read_next, already_deleted) = {
        DataPoints(dds_entity_t*, &g_rcond, &g_qcond, &g_reader),
};
Theory((dds_entity_t *rdr), ddsc_read_next, already_deleted, .init=reader_iterator_init, .fini=reader_iterator_fini)
{
    dds_return_t ret;
    ret = dds_delete(*rdr);
    cr_assert_eq(ret, DDS_RETCODE_OK, "prerequisite delete failed: %d", dds_err_nr(ret));
    ret = dds_read_next(*rdr, g_samples, g_info);
    cr_assert_eq(dds_err_nr(ret), DDS_RETCODE_ALREADY_DELETED, "returned %d", dds_err_nr(ret));
}

/*************************************************************************************************/

/*************************************************************************************************/
TheoryDataPoints(ddsc_read_next, invalid_buffers) = {
        DataPoints(void**,             g_samples, g_loans, (void**)0),
        DataPoints(dds_sample_info_t*, g_info,    (dds_sample_info_t*)0   ),
};
Theory((void **buf, dds_sample_info_t *si), ddsc_read_next, invalid_buffers, .init=reader_iterator_init, .fini=reader_iterator_fini)
{
    dds_return_t ret;
    cr_assume((buf != g_samples) || (si != g_info));
    cr_assume(buf != g_loans);
    ret = dds_read_next(g_reader, buf, si);
    cr_assert_eq(dds_err_nr(ret), DDS_RETCODE_BAD_PARAMETER, "returned %d", dds_err_nr(ret));
}
/*************************************************************************************************/





/**************************************************************************************************
 *
 * These will check the dds_read_next_wl() in various ways.
 *
 *************************************************************************************************/
/*************************************************************************************************/
Test(ddsc_read_next_wl, reader, .init=reader_iterator_init, .fini=reader_iterator_fini)
{
    dds_return_t cnt = 0, cntinv = 0;
    dds_return_t ret = 1;

    while (ret == 1){
      ret = dds_read_next_wl(g_reader, g_loans, g_info);
      cr_assert_geq(ret, 0 , "# read %d", ret);
      if(ret == 1 && g_info[0].valid_data){
        Space_Type1 *sample = (Space_Type1*)g_loans[0];
        PRINT_SAMPLE("ddsc_read_next_wl::reader: Read", (*sample));

        /* Expected states. */
        int                  expected_long_2 = rdr_expected_long_2[cnt];
        int                  expected_long_1 = expected_long_2/3;
        int                  expected_long_3 = expected_long_2*2;
        dds_sample_state_t   expected_sst    = DDS_SST_NOT_READ;
        dds_view_state_t     expected_vst    = SAMPLE_VST(expected_long_2);
        dds_instance_state_t expected_ist    = SAMPLE_IST(expected_long_1);

        /* Check data. */
        cr_assert_eq(sample->long_1, expected_long_2/3 );
        cr_assert_eq(sample->long_2, expected_long_2   );
        cr_assert_eq(sample->long_3, expected_long_2 *2);

        /* Check states. */
        cr_assert_eq(g_info[0].valid_data,     true);
        cr_assert_eq(g_info[0].sample_state,   expected_sst);
        cr_assert_eq(g_info[0].view_state,     expected_vst);
        cr_assert_eq(g_info[0].instance_state, expected_ist);
        cnt ++;
      } else if (ret == 1 && !g_info[0].valid_data) {
        cntinv ++;
      }
    }

    cr_assert_eq(cnt, RDR_NOT_READ_CNT);
    cr_assert_eq(cntinv, RDR_INV_READ_CNT);

    ret = dds_return_loan(g_reader, g_loans, ret);
    cr_assert_eq (ret, DDS_RETCODE_OK);

    /* All samples should still be available. */
    ret = samples_cnt();
    cr_assert_eq(ret, MAX_SAMPLES, "# samples %d, expected %d", ret, MAX_SAMPLES);

}
/*************************************************************************************************/

/*************************************************************************************************/
TheoryDataPoints(ddsc_read_next_wl, invalid_readers) = {
        DataPoints(dds_entity_t, -2, -1, 0, 1, 100, INT_MAX, INT_MIN),
};
Theory((dds_entity_t rdr), ddsc_read_next_wl, invalid_readers, .init=reader_iterator_init, .fini=reader_iterator_fini)
{
    dds_entity_t exp = DDS_RETCODE_BAD_PARAMETER * -1;
    dds_return_t ret;

    ret = dds_read_next_wl(rdr, g_loans, g_info);
    cr_assert_eq(dds_err_nr(ret), dds_err_nr(exp), "returned %d != expected %d", dds_err_nr(ret), dds_err_nr(exp));
}
/*************************************************************************************************/

/*************************************************************************************************/
TheoryDataPoints(ddsc_read_next_wl, non_readers) = {
        DataPoints(dds_entity_t*, &g_participant, &g_topic, &g_writer, &g_subscriber, &g_publisher, &g_waitset, &g_rcond, &g_qcond),
};
Theory((dds_entity_t *rdr), ddsc_read_next_wl, non_readers, .init=reader_iterator_init, .fini=reader_iterator_fini)
{
    dds_return_t ret;
    ret = dds_read_next_wl(*rdr, g_loans, g_info);
    cr_assert_eq(dds_err_nr(ret), DDS_RETCODE_ILLEGAL_OPERATION, "returned %d", dds_err_nr(ret));
}
/*************************************************************************************************/

/*************************************************************************************************/
TheoryDataPoints(ddsc_read_next_wl, already_deleted) = {
        DataPoints(dds_entity_t*, &g_rcond, &g_qcond, &g_reader),
};
Theory((dds_entity_t *rdr), ddsc_read_next_wl, already_deleted, .init=reader_iterator_init, .fini=reader_iterator_fini)
{
    dds_return_t ret;
    ret = dds_delete(*rdr);
    cr_assert_eq(ret, DDS_RETCODE_OK, "prerequisite delete failed: %d", dds_err_nr(ret));
    ret = dds_read_next_wl(*rdr, g_loans, g_info);
    cr_assert_eq(dds_err_nr(ret), DDS_RETCODE_ALREADY_DELETED, "returned %d", dds_err_nr(ret));
}

/*************************************************************************************************/

/*************************************************************************************************/
TheoryDataPoints(ddsc_read_next_wl, invalid_buffers) = {
        DataPoints(void**,             g_loans, (void**)0),
        DataPoints(dds_sample_info_t*, g_info,    (dds_sample_info_t*)0   ),
};
Theory((void **buf, dds_sample_info_t *si), ddsc_read_next_wl, invalid_buffers, .init=reader_iterator_init, .fini=reader_iterator_fini)
{
    dds_return_t ret;
    cr_assume((buf != g_loans) || (si != g_info));
    ret = dds_read_next_wl(g_reader, buf, si);
    cr_assert_eq(dds_err_nr(ret), DDS_RETCODE_BAD_PARAMETER, "returned %d", dds_err_nr(ret));
}
/*************************************************************************************************/





/**************************************************************************************************
 *
 * These will check the dds_take_next() in various ways.
 *
 *************************************************************************************************/
/*************************************************************************************************/
Test(ddsc_take_next, reader, .init=reader_iterator_init, .fini=reader_iterator_fini)
{
    dds_return_t cnt = 0, cntinv = 0;
    dds_return_t ret = 1;

    while (ret == 1){
      ret = dds_take_next(g_reader, g_samples, g_info);
      cr_assert_geq(ret, 0 , "# read %d", ret);
      if(ret == 1 && g_info[0].valid_data){
        Space_Type1 *sample = (Space_Type1*)g_samples[0];
        PRINT_SAMPLE("ddsc_take_next::reader: Read", (*sample));

        /* Expected states. */
        int                  expected_long_2 = rdr_expected_long_2[cnt];
        int                  expected_long_1 = expected_long_2/3;
        int                  expected_long_3 = expected_long_2*2;
        dds_sample_state_t   expected_sst    = DDS_SST_NOT_READ;
        dds_view_state_t     expected_vst    = SAMPLE_VST(expected_long_2);
        dds_instance_state_t expected_ist    = SAMPLE_IST(expected_long_1);

        /* Check data. */
        cr_assert_eq(sample->long_1, expected_long_1 );
        cr_assert_eq(sample->long_2, expected_long_2   );
        cr_assert_eq(sample->long_3, expected_long_2 *2);

        /* Check states. */
        cr_assert_eq(g_info[0].valid_data,     true);
        cr_assert_eq(g_info[0].sample_state,   expected_sst);
        cr_assert_eq(g_info[0].view_state,     expected_vst);
        cr_assert_eq(g_info[0].instance_state, expected_ist);
        cnt ++;
      } else if (ret == 1 && !g_info[0].valid_data) {
        cntinv ++;
      }
    }

    cr_assert_eq(cnt, RDR_NOT_READ_CNT);
    cr_assert_eq(cntinv, RDR_INV_READ_CNT);

    /* All samples should still be available. */
    ret = samples_cnt();
    cr_assert_eq(ret, (MAX_SAMPLES - RDR_NOT_READ_CNT), "# samples %d, expected %d", ret, MAX_SAMPLES);

}
/*************************************************************************************************/
TheoryDataPoints(ddsc_take_next, invalid_readers) = {
        DataPoints(dds_entity_t, -2, -1, 0, 1, 100, INT_MAX, INT_MIN),
};
Theory((dds_entity_t rdr), ddsc_take_next, invalid_readers, .init=reader_iterator_init, .fini=reader_iterator_fini)
{
    dds_entity_t exp = DDS_RETCODE_BAD_PARAMETER * -1;
    dds_return_t ret;

    ret = dds_take_next(rdr, g_samples, g_info);
    cr_assert_eq(dds_err_nr(ret), dds_err_nr(exp), "returned %d != expected %d", dds_err_nr(ret), dds_err_nr(exp));
}
/*************************************************************************************************/

/*************************************************************************************************/
TheoryDataPoints(ddsc_take_next, non_readers) = {
        DataPoints(dds_entity_t*, &g_participant, &g_topic, &g_writer, &g_subscriber, &g_publisher, &g_waitset, &g_rcond, &g_qcond),
};
Theory((dds_entity_t *rdr), ddsc_take_next, non_readers, .init=reader_iterator_init, .fini=reader_iterator_fini)
{
    dds_return_t ret;
    ret = dds_take_next(*rdr, g_samples, g_info);
    cr_assert_eq(dds_err_nr(ret), DDS_RETCODE_ILLEGAL_OPERATION, "returned %d", dds_err_nr(ret));
}
/*************************************************************************************************/

/*************************************************************************************************/
TheoryDataPoints(ddsc_take_next, already_deleted) = {
        DataPoints(dds_entity_t*, &g_rcond, &g_qcond, &g_reader),
};
Theory((dds_entity_t *rdr), ddsc_take_next, already_deleted, .init=reader_iterator_init, .fini=reader_iterator_fini)
{
    dds_return_t ret;
    ret = dds_delete(*rdr);
    cr_assert_eq(ret, DDS_RETCODE_OK, "prerequisite delete failed: %d", dds_err_nr(ret));
    ret = dds_take_next(*rdr, g_samples, g_info);
    cr_assert_eq(dds_err_nr(ret), DDS_RETCODE_ALREADY_DELETED, "returned %d", dds_err_nr(ret));
}

/*************************************************************************************************/

/*************************************************************************************************/
TheoryDataPoints(ddsc_take_next, invalid_buffers) = {
        DataPoints(void**,             g_samples, g_loans, (void**)0),
        DataPoints(dds_sample_info_t*, g_info,    (dds_sample_info_t*)0   ),
};
Theory((void **buf, dds_sample_info_t *si), ddsc_take_next, invalid_buffers, .init=reader_iterator_init, .fini=reader_iterator_fini)
{
    dds_return_t ret;
    cr_assume((buf != g_samples) || (si != g_info));
    cr_assume(buf != g_loans);
    ret = dds_take_next(g_reader, buf, si);
    cr_assert_eq(dds_err_nr(ret), DDS_RETCODE_BAD_PARAMETER, "returned %d", dds_err_nr(ret));
}
/*************************************************************************************************/





/**************************************************************************************************
 *
 * These will check the dds_take_next_wl() in various ways.
 *
 *************************************************************************************************/
/*************************************************************************************************/
Test(ddsc_take_next_wl, reader, .init=reader_iterator_init, .fini=reader_iterator_fini)
{
    dds_return_t cnt = 0, cntinv = 0;
    dds_return_t ret = 1;

    while (ret == 1){
      ret = dds_take_next_wl(g_reader, g_loans, g_info);
      cr_assert_geq(ret, 0 , "# read %d", ret);
      if(ret == 1 && g_info[0].valid_data){
        Space_Type1 *sample = (Space_Type1*)g_loans[0];
        PRINT_SAMPLE("ddsc_read_next_wl::reader: Read", (*sample));

        /* Expected states. */
        int                  expected_long_2 = rdr_expected_long_2[cnt];
        int                  expected_long_1 = expected_long_2/3;
        int                  expected_long_3 = expected_long_2*2;
        dds_sample_state_t   expected_sst    = DDS_SST_NOT_READ;
        dds_view_state_t     expected_vst    = SAMPLE_VST(expected_long_2);
        dds_instance_state_t expected_ist    = SAMPLE_IST(expected_long_1);

        /* Check data. */
        cr_assert_eq(sample->long_1, expected_long_2/3 );
        cr_assert_eq(sample->long_2, expected_long_2   );
        cr_assert_eq(sample->long_3, expected_long_2 *2);

        /* Check states. */
        cr_assert_eq(g_info[0].valid_data,     true);
        cr_assert_eq(g_info[0].sample_state,   expected_sst);
        cr_assert_eq(g_info[0].view_state,     expected_vst);
        cr_assert_eq(g_info[0].instance_state, expected_ist);
        cnt ++;
      } else if (ret == 1 && !g_info[0].valid_data) {
        cntinv ++;
      }
    }

    cr_assert_eq(cnt, RDR_NOT_READ_CNT);
    cr_assert_eq(cntinv, RDR_INV_READ_CNT);

    ret = dds_return_loan(g_reader, g_loans, ret);
    cr_assert_eq (ret, DDS_RETCODE_OK);

    /* All samples should still be available. */
    ret = samples_cnt();
    cr_assert_eq(ret, (MAX_SAMPLES - RDR_NOT_READ_CNT), "# samples %d, expected %d", ret, MAX_SAMPLES);

}
/*************************************************************************************************/
TheoryDataPoints(ddsc_take_next_wl, invalid_readers) = {
        DataPoints(dds_entity_t, -2, -1, 0, 1, 100, INT_MAX, INT_MIN),
};
Theory((dds_entity_t rdr), ddsc_take_next_wl, invalid_readers, .init=reader_iterator_init, .fini=reader_iterator_fini)
{
    dds_entity_t exp = DDS_RETCODE_BAD_PARAMETER * -1;
    dds_return_t ret;

    ret = dds_take_next_wl(rdr, g_loans, g_info);
    cr_assert_eq(dds_err_nr(ret), dds_err_nr(exp), "returned %d != expected %d", dds_err_nr(ret), dds_err_nr(exp));
}
/*************************************************************************************************/

/*************************************************************************************************/
TheoryDataPoints(ddsc_take_next_wl, non_readers) = {
        DataPoints(dds_entity_t*, &g_participant, &g_topic, &g_writer, &g_subscriber, &g_publisher, &g_waitset, &g_rcond, &g_qcond),
};
Theory((dds_entity_t *rdr), ddsc_take_next_wl, non_readers, .init=reader_iterator_init, .fini=reader_iterator_fini)
{
    dds_return_t ret;
    ret = dds_take_next_wl(*rdr, g_loans, g_info);
    cr_assert_eq(dds_err_nr(ret), DDS_RETCODE_ILLEGAL_OPERATION, "returned %d", dds_err_nr(ret));
}
/*************************************************************************************************/

/*************************************************************************************************/
TheoryDataPoints(ddsc_take_next_wl, already_deleted) = {
        DataPoints(dds_entity_t*, &g_rcond, &g_qcond, &g_reader),
};
Theory((dds_entity_t *rdr), ddsc_take_next_wl, already_deleted, .init=reader_iterator_init, .fini=reader_iterator_fini)
{
    dds_return_t ret;
    ret = dds_delete(*rdr);
    cr_assert_eq(ret, DDS_RETCODE_OK, "prerequisite delete failed: %d", dds_err_nr(ret));
    ret = dds_take_next_wl(*rdr, g_loans, g_info);
    cr_assert_eq(dds_err_nr(ret), DDS_RETCODE_ALREADY_DELETED, "returned %d", dds_err_nr(ret));
}
/*************************************************************************************************/

/*************************************************************************************************/
TheoryDataPoints(ddsc_take_next_wl, invalid_buffers) = {
        DataPoints(void**,             g_loans, (void**)0),
        DataPoints(dds_sample_info_t*, g_info,    (dds_sample_info_t*)0   ),
};
Theory((void **buf, dds_sample_info_t *si), ddsc_take_next_wl, invalid_buffers, .init=reader_iterator_init, .fini=reader_iterator_fini)
{
    dds_return_t ret;
    cr_assume((buf != g_loans) || (si != g_info));
    ret = dds_take_next_wl(g_reader, buf, si);
    cr_assert_eq(dds_err_nr(ret), DDS_RETCODE_BAD_PARAMETER, "returned %d", dds_err_nr(ret));
}
/*************************************************************************************************/

