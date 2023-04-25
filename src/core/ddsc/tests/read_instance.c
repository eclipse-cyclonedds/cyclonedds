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
#define MAX_SAMPLES                 5
/*
 * By writing, disposing, unregistering, reading and re-writing, the following
 * data will be available in the reader history (but not in this order).
 *    | long_1 | long_2 | long_3 |    sst   | vst |    ist     |
 *    ----------------------------------------------------------
 *    |    0   |    0   |    0   |     read | old | alive      |
 *    |    0   |    1   |    2   | not_read | old | alive      |
 *    |    1   |    2   |    4   | not_read | new | alive      |
 *    |    2   |    3   |    6   | not_read | new | disposed   |
 *    |    3   |    4   |    8   | not_read | new | no_writers |
 */
#define SAMPLE_IST(idx)           ((idx <= 2) ? DDS_IST_ALIVE              : \
                                   (idx == 3) ? DDS_IST_NOT_ALIVE_DISPOSED : \
                                                DDS_IST_NOT_ALIVE_NO_WRITERS )
#define SAMPLE_VST(idx)           ((idx <= 1) ? DDS_VST_OLD  : DDS_VST_NEW)
#define SAMPLE_SST(idx)           ((idx == 0) ? DDS_SST_READ : DDS_SST_NOT_READ)

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

static bool
filter_mod2(const void * sample)
{
    const Space_Type1 *s = sample;
    return (s->long_2 % 2 == 0);
}

static void
read_instance_init(void)
{
    Space_Type1 sample = { 0, 0, 0 };
    dds_attach_t triggered;
    dds_return_t ret;
    char name[100];
    dds_qos_t *qos;

    qos = dds_create_qos();
    CU_ASSERT_PTR_NOT_NULL_FATAL(qos);

    g_participant = dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);
    CU_ASSERT_FATAL(g_participant > 0);

    g_subscriber = dds_create_subscriber(g_participant, NULL, NULL);
    CU_ASSERT_FATAL(g_subscriber > 0);

    g_publisher = dds_create_publisher(g_participant, NULL, NULL);
    CU_ASSERT_FATAL(g_publisher > 0);

    g_waitset = dds_create_waitset(g_participant);
    CU_ASSERT_FATAL(g_waitset > 0);

    g_topic = dds_create_topic(g_participant, &Space_Type1_desc, create_unique_topic_name("ddsc_read_instance_test", name, sizeof name), NULL, NULL);
    CU_ASSERT_FATAL(g_topic > 0);

    /* Create a writer that will not automatically dispose unregistered samples. */
    dds_qset_writer_data_lifecycle(qos, false);
    g_writer = dds_create_writer(g_publisher, g_topic, qos, NULL);
    CU_ASSERT_FATAL(g_writer > 0);

    /* Create a reader that keeps all samples when not taken. */
    dds_qset_history(qos, DDS_HISTORY_KEEP_ALL, DDS_LENGTH_UNLIMITED);
    g_reader = dds_create_reader(g_subscriber, g_topic, qos, NULL);
    CU_ASSERT_FATAL(g_reader > 0);

    /* Create a read condition that only reads not_read samples. */
    g_rcond = dds_create_readcondition(g_reader, DDS_NOT_READ_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE);
    CU_ASSERT_FATAL(g_rcond > 0);

    /* Create a query condition that only reads not_read samples of instances mod2. */
    g_qcond = dds_create_querycondition(g_reader, DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE, filter_mod2);
    CU_ASSERT_FATAL(g_qcond > 0);

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

    /* Write the sample that will become {sst(read), vst(old), ist(alive)}. */
    sample.long_1 = 0;
    sample.long_2 = 0;
    sample.long_3 = 0;
    ret = dds_write(g_writer, &sample);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    /*  | long_1 | long_2 | long_3 |    sst   | vst |    ist     |
     *  ----------------------------------------------------------
     *  |    0   |    0   |    0   | not_read | new | alive      |
     */

    /* Read sample that will become {sst(read), vst(old), ist(alive)}. */
    ret = dds_read(g_reader, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
    CU_ASSERT_EQUAL_FATAL(ret, 1);
    for(int i = 0; i < ret; i++) {
        Space_Type1 *s = (Space_Type1*)g_samples[i];
        (void)s;
    }
    /*  | long_1 | long_2 | long_3 |    sst   | vst |    ist     |
     *  ----------------------------------------------------------
     *  |    0   |    0   |    0   |     read | old | alive      |
     */

    /* Write the sample that will become {sst(not_read), vst(old), ist(alive)}. */
    sample.long_1 = 0;
    sample.long_2 = 1;
    sample.long_3 = 2;
    ret = dds_write(g_writer, &sample);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    /*  | long_1 | long_2 | long_3 |    sst   | vst |    ist     |
     *  ----------------------------------------------------------
     *  |    0   |    0   |    0   |     read | old | alive      |
     *  |    0   |    1   |    2   | not_read | old | alive      |
     */

    /* Write the samples that will become {sst(not_read), vst(new), ist(*)}. */
    for (int i = 2; i < MAX_SAMPLES; i++) {
        sample.long_1 = i - 1;
        sample.long_2 = i;
        sample.long_3 = i*2;
        ret = dds_write(g_writer, &sample);
        CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    }
    /*  | long_1 | long_2 | long_3 |    sst   | vst |    ist     |
     *  ----------------------------------------------------------
     *  |    0   |    0   |    0   |     read | old | alive      |
     *  |    0   |    1   |    2   | not_read | old | alive      |
     *  |    1   |    2   |    4   | not_read | new | alive      |
     *  |    2   |    3   |    6   | not_read | new | alive      |
     *  |    3   |    4   |    8   | not_read | new | alive      |
     */

    /* Dispose the sample that will become {sst(not_read), vst(new), ist(disposed)}. */
    sample.long_1 = 2;
    sample.long_2 = 3;
    sample.long_3 = 6;
    ret = dds_dispose(g_writer, &sample);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    /*  | long_1 | long_2 | long_3 |    sst   | vst |    ist     |
     *  ----------------------------------------------------------
     *  |    0   |    0   |    0   |     read | old | alive      |
     *  |    0   |    1   |    2   | not_read | old | alive      |
     *  |    1   |    2   |    4   | not_read | new | alive      |
     *  |    2   |    3   |    6   | not_read | new | disposed   |
     *  |    3   |    4   |    8   | not_read | new | alive      |
     */

    /* Unregister the sample that will become {sst(not_read), vst(new), ist(no_writers)}. */
    sample.long_1 = 3;
    sample.long_2 = 4;
    sample.long_3 = 8;
    ret = dds_unregister_instance(g_writer, &sample);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    /*  | long_1 | long_2 | long_3 |    sst   | vst |    ist     |
     *  ----------------------------------------------------------
     *  |    0   |    0   |    0   |     read | old | alive      |
     *  |    0   |    1   |    2   | not_read | old | alive      |
     *  |    1   |    2   |    4   | not_read | new | alive      |
     *  |    2   |    3   |    6   | not_read | new | disposed   |
     *  |    3   |    4   |    8   | not_read | new | no_writers |
     */

    /* Get valid instance handle. */
    sample.long_1 = 0;
    sample.long_2 = 0;
    sample.long_3 = 0;
    g_hdl_valid = dds_lookup_instance(g_reader, &sample);
    CU_ASSERT_NOT_EQUAL_FATAL(g_hdl_valid, DDS_HANDLE_NIL);

    dds_delete_qos(qos);
}

static void
read_instance_fini(void)
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
    CU_ASSERT_FATAL(ret >= 0);
    return ret;
}





/**************************************************************************************************
 *
 * These will check the read_instance_* functions with invalid parameters.
 *
 *************************************************************************************************/
/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_read_instance, invalid_params) = {
        CU_DataPoints(dds_entity_t*,      &g_reader,  &g_rcond, &g_qcond),
        CU_DataPoints(void**,              g_samples,  g_loans, (void**)0),
        CU_DataPoints(dds_sample_info_t*,  g_info,     NULL,     NULL),
        CU_DataPoints(size_t,              0,          2,        MAX_SAMPLES),
        CU_DataPoints(uint32_t,            0,          2,        MAX_SAMPLES),
};
CU_Theory((dds_entity_t *ent, void **buf, dds_sample_info_t *si, size_t bufsz, uint32_t maxs), ddsc_read_instance, invalid_params, .init=read_instance_init, .fini=read_instance_fini)
{
    dds_return_t ret;
    /* The only valid permutation is when non of the buffer values are
     * invalid and neither is the handle. So, don't test that. */
    CU_ASSERT_FATAL((buf != g_samples) || (si != g_info) || (bufsz == 0) || (maxs == 0) || (bufsz < maxs));
    /* TODO: CHAM-306, currently, a buffer is automatically 'promoted' to a loan when a buffer is
     * provided with NULL pointers. So, in fact, there's currently no real difference between calling
     * dds_read() dds_read_wl() (except for the provided bufsz). This will change, which means that
     * the given buffer should contain valid pointers, which again means that 'loan intended' buffer
     * should result in bad_parameter.
     * However, that's not the case yet. So don't test it. */
    if (buf != g_loans) {
        ret = dds_read_instance(*ent, buf, si, bufsz, maxs, g_hdl_valid);
        CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
    } else {
        CU_PASS("Skipped");
    }
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_read_instance_wl, invalid_params) = {
        CU_DataPoints(dds_entity_t *,      &g_reader,  &g_rcond, &g_qcond),
        CU_DataPoints(void **,              g_samples,  g_loans, (void**)0),
        CU_DataPoints(dds_sample_info_t *,  g_info,     NULL,     NULL),
        CU_DataPoints(uint32_t,             0,          2,        MAX_SAMPLES),
};
CU_Theory((dds_entity_t *ent, void **buf, dds_sample_info_t *si, uint32_t maxs), ddsc_read_instance_wl, invalid_params, .init=read_instance_init, .fini=read_instance_fini)
{
    dds_return_t ret;
    /* The only valid permutation is when non of the buffer values are
     * invalid and neither is the handle. So, don't test that. */
    if ((buf != g_loans) || (si != g_info) || (maxs == 0)) {
        ret = dds_read_instance_wl(*ent, buf, si, maxs, g_hdl_valid);
        CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
    } else {
        CU_PASS("Skipped");
    }
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_read_instance_mask, invalid_params) = {
        CU_DataPoints(dds_entity_t*,      &g_reader, &g_rcond, &g_qcond),
        CU_DataPoints(void**,              g_samples, g_loans, (void**)0),
        CU_DataPoints(dds_sample_info_t*,  g_info,    NULL,     NULL),
        CU_DataPoints(size_t,              0,         2,        MAX_SAMPLES),
        CU_DataPoints(uint32_t,            0,         2,        MAX_SAMPLES),
};
CU_Theory((dds_entity_t *ent, void **buf, dds_sample_info_t *si, size_t bufsz, uint32_t maxs), ddsc_read_instance_mask, invalid_params, .init=read_instance_init, .fini=read_instance_fini)
{
    uint32_t mask = DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
    dds_return_t ret;
    /* The only valid permutation is when non of the buffer values are
     * invalid and neither is the handle. So, don't test that. */
    CU_ASSERT_FATAL((buf != g_samples) || (si != g_info) || (bufsz == 0) || (maxs == 0) || (bufsz < maxs));
    /* TODO: CHAM-306, currently, a buffer is automatically 'promoted' to a loan when a buffer is
     * provided with NULL pointers. So, in fact, there's currently no real difference between calling
     * dds_read() dds_read_wl() (except for the provided bufsz). This will change, which means that
     * the given buffer should contain valid pointers, which again means that 'loan intended' buffer
     * should result in bad_parameter.
     * However, that's not the case yet. So don't test it. */
    if (buf != g_loans) {
        ret = dds_read_instance_mask(*ent, buf, si, bufsz, maxs, g_hdl_valid, mask);
        CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
    } else {
        CU_PASS("Skipped");
    }
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_read_instance_mask_wl, invalid_params) = {
        CU_DataPoints(dds_entity_t*,      &g_reader, &g_rcond,   &g_qcond),
        CU_DataPoints(void**,              g_loans,   (void**)0, (void**)0),
        CU_DataPoints(dds_sample_info_t*,  g_info,     NULL,      NULL),
        CU_DataPoints(uint32_t,            0,          2,         MAX_SAMPLES),
};
CU_Theory((dds_entity_t *ent, void **buf, dds_sample_info_t *si, uint32_t maxs), ddsc_read_instance_mask_wl, invalid_params, .init=read_instance_init, .fini=read_instance_fini)
{
    uint32_t mask = DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
    dds_return_t ret;
    /* The only valid permutation is when non of the buffer values are
     * invalid and neither is the handle. So, don't test that. */
    CU_ASSERT_FATAL((buf != g_loans) || (si != g_info) || (maxs == 0));
    ret = dds_read_instance_mask_wl(*ent, buf, si, maxs, g_hdl_valid, mask);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
}
/*************************************************************************************************/





/**************************************************************************************************
 *
 * These will check the read_instance_* functions with invalid handles.
 *
 *************************************************************************************************/
/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_read_instance, invalid_handles) = {
        CU_DataPoints(dds_entity_t*,         &g_reader, &g_rcond, &g_qcond),
        CU_DataPoints(dds_instance_handle_t, DDS_HANDLE_NIL, 0, 1, 100, UINT64_MAX),
};
CU_Theory((dds_entity_t *rdr, dds_instance_handle_t hdl), ddsc_read_instance, invalid_handles, .init=read_instance_init, .fini=read_instance_fini)
{
    dds_return_t ret;
    ret = dds_read_instance(*rdr, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES, hdl);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_PRECONDITION_NOT_MET);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_read_instance_wl, invalid_handles) = {
        CU_DataPoints(dds_entity_t*,         &g_reader, &g_rcond, &g_qcond),
        CU_DataPoints(dds_instance_handle_t, DDS_HANDLE_NIL, 0, 1, 100, UINT64_MAX),
};
CU_Theory((dds_entity_t *rdr, dds_instance_handle_t hdl), ddsc_read_instance_wl, invalid_handles, .init=read_instance_init, .fini=read_instance_fini)
{
    dds_return_t ret;
    ret = dds_read_instance_wl(*rdr, g_loans, g_info, MAX_SAMPLES, hdl);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_PRECONDITION_NOT_MET);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_read_instance_mask, invalid_handles) = {
        CU_DataPoints(dds_entity_t*,         &g_reader, &g_rcond, &g_qcond),
        CU_DataPoints(dds_instance_handle_t, DDS_HANDLE_NIL, 0, 1, 100, UINT64_MAX),
};
CU_Theory((dds_entity_t *rdr, dds_instance_handle_t hdl), ddsc_read_instance_mask, invalid_handles, .init=read_instance_init, .fini=read_instance_fini)
{
    uint32_t mask = DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
    dds_return_t ret;
    ret = dds_read_instance_mask(*rdr, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES, hdl, mask);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_PRECONDITION_NOT_MET);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_read_instance_mask_wl, invalid_handles) = {
        CU_DataPoints(dds_entity_t*,         &g_reader, &g_rcond, &g_qcond),
        CU_DataPoints(dds_instance_handle_t, DDS_HANDLE_NIL, 0, 1, 100, UINT64_MAX),
};
CU_Theory((dds_entity_t *rdr, dds_instance_handle_t hdl), ddsc_read_instance_mask_wl, invalid_handles, .init=read_instance_init, .fini=read_instance_fini)
{
    uint32_t mask = DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
    dds_return_t ret;
    ret = dds_read_instance_mask_wl(*rdr, g_loans, g_info, MAX_SAMPLES, hdl, mask);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_PRECONDITION_NOT_MET);
}
/*************************************************************************************************/





/**************************************************************************************************
 *
 * These will check the read_instance_* functions with invalid readers.
 *
 *************************************************************************************************/
/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_read_instance, invalid_readers) = {
        CU_DataPoints(dds_entity_t, -2, -1, 0, INT_MAX, INT_MIN),
};
CU_Theory((dds_entity_t rdr), ddsc_read_instance, invalid_readers, .init=read_instance_init, .fini=read_instance_fini)
{
    dds_return_t ret;

    ret = dds_read_instance(rdr, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES, g_hdl_valid);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_read_instance_wl, invalid_readers) = {
        CU_DataPoints(dds_entity_t, -2, -1, 0, INT_MAX, INT_MIN),
};
CU_Theory((dds_entity_t rdr), ddsc_read_instance_wl, invalid_readers, .init=read_instance_init, .fini=read_instance_fini)
{
    dds_return_t ret;

    ret = dds_read_instance_wl(rdr, g_loans, g_info, MAX_SAMPLES, g_hdl_valid);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_read_instance_mask, invalid_readers) = {
        CU_DataPoints(dds_entity_t, -2, -1, 0, INT_MAX, INT_MIN),
};
CU_Theory((dds_entity_t rdr), ddsc_read_instance_mask, invalid_readers, .init=read_instance_init, .fini=read_instance_fini)
{
    uint32_t mask = DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
    dds_return_t ret;

    ret = dds_read_instance_mask(rdr, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES, g_hdl_valid, mask);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_read_instance_mask_wl, invalid_readers) = {
        CU_DataPoints(dds_entity_t, -2, -1, 0, INT_MAX, INT_MIN),
};
CU_Theory((dds_entity_t rdr), ddsc_read_instance_mask_wl, invalid_readers, .init=read_instance_init, .fini=read_instance_fini)
{
    uint32_t mask = DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
    dds_return_t ret;

    ret = dds_read_instance_mask_wl(rdr, g_loans, g_info, MAX_SAMPLES, g_hdl_valid, mask);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
}
/*************************************************************************************************/






/**************************************************************************************************
 *
 * These will check the read_instance_* functions with non readers.
 *
 *************************************************************************************************/
/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_read_instance, non_readers) = {
        CU_DataPoints(dds_entity_t*, &g_participant, &g_topic, &g_writer, &g_subscriber, &g_publisher, &g_waitset),
};
CU_Theory((dds_entity_t *rdr), ddsc_read_instance, non_readers, .init=read_instance_init, .fini=read_instance_fini)
{
    dds_return_t ret;
    ret = dds_read_instance(*rdr, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES, g_hdl_valid);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_ILLEGAL_OPERATION);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_read_instance_wl, non_readers) = {
        CU_DataPoints(dds_entity_t*, &g_participant, &g_topic, &g_writer, &g_subscriber, &g_publisher, &g_waitset),
};
CU_Theory((dds_entity_t *rdr), ddsc_read_instance_wl, non_readers, .init=read_instance_init, .fini=read_instance_fini)
{
    dds_return_t ret;
    ret = dds_read_instance_wl(*rdr, g_loans, g_info, MAX_SAMPLES, g_hdl_valid);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_ILLEGAL_OPERATION);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_read_instance_mask, non_readers) = {
        CU_DataPoints(dds_entity_t*, &g_participant, &g_topic, &g_writer, &g_subscriber, &g_publisher, &g_waitset),
};
CU_Theory((dds_entity_t *rdr), ddsc_read_instance_mask, non_readers, .init=read_instance_init, .fini=read_instance_fini)
{
    uint32_t mask = DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
    dds_return_t ret;
    ret = dds_read_instance_mask(*rdr, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES, g_hdl_valid, mask);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_ILLEGAL_OPERATION);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_read_instance_mask_wl, non_readers) = {
        CU_DataPoints(dds_entity_t*, &g_participant, &g_topic, &g_writer, &g_subscriber, &g_publisher, &g_waitset),
};
CU_Theory((dds_entity_t *rdr), ddsc_read_instance_mask_wl, non_readers, .init=read_instance_init, .fini=read_instance_fini)
{
    uint32_t mask = DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
    dds_return_t ret;
    ret = dds_read_instance_mask_wl(*rdr, g_loans, g_info, MAX_SAMPLES, g_hdl_valid, mask);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_ILLEGAL_OPERATION);
}
/*************************************************************************************************/






/**************************************************************************************************
 *
 * These will check the read_instance_* functions with deleted readers.
 *
 *************************************************************************************************/
/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_read_instance, already_deleted) = {
        CU_DataPoints(dds_entity_t*, &g_rcond, &g_qcond, &g_reader),
};
CU_Theory((dds_entity_t *rdr), ddsc_read_instance, already_deleted, .init=read_instance_init, .fini=read_instance_fini)
{
    dds_return_t ret;
    ret = dds_delete(*rdr);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    ret = dds_read_instance(*rdr, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES, g_hdl_valid);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_read_instance_wl, already_deleted) = {
        CU_DataPoints(dds_entity_t*, &g_rcond, &g_qcond, &g_reader),
};
CU_Theory((dds_entity_t *rdr), ddsc_read_instance_wl, already_deleted, .init=read_instance_init, .fini=read_instance_fini)
{
    dds_return_t ret;
    ret = dds_delete(*rdr);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    ret = dds_read_instance_wl(*rdr, g_loans, g_info, MAX_SAMPLES, g_hdl_valid);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_read_instance_mask, already_deleted) = {
        CU_DataPoints(dds_entity_t*, &g_rcond, &g_qcond, &g_reader),
};
CU_Theory((dds_entity_t *rdr), ddsc_read_instance_mask, already_deleted, .init=read_instance_init, .fini=read_instance_fini)
{
    uint32_t mask = DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
    dds_return_t ret;
    ret = dds_delete(*rdr);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    ret = dds_read_instance_mask(*rdr, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES, g_hdl_valid, mask);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_read_instance_mask_wl, already_deleted) = {
        CU_DataPoints(dds_entity_t*, &g_rcond, &g_qcond, &g_reader),
};
CU_Theory((dds_entity_t *rdr), ddsc_read_instance_mask_wl, already_deleted, .init=read_instance_init, .fini=read_instance_fini)
{
    uint32_t mask = DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
    dds_return_t ret;
    ret = dds_delete(*rdr);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    ret = dds_read_instance_mask_wl(*rdr, g_loans, g_info, MAX_SAMPLES, g_hdl_valid, mask);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
}
/*************************************************************************************************/






/**************************************************************************************************
 *
 * These will check the read_instance_* functions with a valid reader.
 *
 *************************************************************************************************/
/*************************************************************************************************/
CU_Test(ddsc_read_instance, reader, .init=read_instance_init, .fini=read_instance_fini)
{
    dds_return_t expected_cnt = 2;
    dds_return_t ret;

    ret = dds_read_instance(g_reader, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES, g_hdl_valid);
    CU_ASSERT_EQUAL_FATAL(ret, expected_cnt);
    for(int i = 0; i < ret; i++) {
        Space_Type1 *sample = (Space_Type1*)g_samples[i];

        /*
         *    | long_1 | long_2 | long_3 |    sst   | vst |    ist     |
         *    ----------------------------------------------------------
         *    |    0   |    0   |    0   |     read | old | alive      | <---
         *    |    0   |    1   |    2   | not_read | old | alive      | <---
         *    |    1   |    2   |    4   | not_read | new | alive      |
         *    |    2   |    3   |    6   | not_read | new | disposed   |
         *    |    3   |    4   |    8   | not_read | new | no_writers |
         */

        /* Expected states. */
        int                  expected_long_1 = 0;
        int                  expected_long_2 = i;
        dds_sample_state_t   expected_sst    = SAMPLE_SST(expected_long_2);
        dds_view_state_t     expected_vst    = SAMPLE_VST(expected_long_2);
        dds_instance_state_t expected_ist    = SAMPLE_IST(expected_long_2);

        /* Check data. */
        CU_ASSERT_EQUAL_FATAL(sample->long_1, expected_long_1  );
        CU_ASSERT_EQUAL_FATAL(sample->long_2, expected_long_2  );
        CU_ASSERT_EQUAL_FATAL(sample->long_3, expected_long_2*2);

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
CU_Test(ddsc_read_instance_wl, reader, .init=read_instance_init, .fini=read_instance_fini)
{
    dds_return_t expected_cnt = 2;
    dds_return_t ret;

    ret = dds_read_instance_wl(g_reader, g_loans, g_info, MAX_SAMPLES, g_hdl_valid);
    CU_ASSERT_EQUAL_FATAL(ret, expected_cnt);
    for(int i = 0; i < ret; i++) {
        Space_Type1 *sample = (Space_Type1*)g_loans[i];

        /*
         *    | long_1 | long_2 | long_3 |    sst   | vst |    ist     |
         *    ----------------------------------------------------------
         *    |    0   |    0   |    0   |     read | old | alive      | <---
         *    |    0   |    1   |    2   | not_read | old | alive      | <---
         *    |    1   |    2   |    4   | not_read | new | alive      |
         *    |    2   |    3   |    6   | not_read | new | disposed   |
         *    |    3   |    4   |    8   | not_read | new | no_writers |
         */

        /* Expected states. */
        int                  expected_long_1 = 0;
        int                  expected_long_2 = i;
        dds_sample_state_t   expected_sst    = SAMPLE_SST(expected_long_2);
        dds_view_state_t     expected_vst    = SAMPLE_VST(expected_long_2);
        dds_instance_state_t expected_ist    = SAMPLE_IST(expected_long_2);

        /* Check data. */
        CU_ASSERT_EQUAL_FATAL(sample->long_1, expected_long_1  );
        CU_ASSERT_EQUAL_FATAL(sample->long_2, expected_long_2  );
        CU_ASSERT_EQUAL_FATAL(sample->long_3, expected_long_2*2);

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
CU_Test(ddsc_read_instance_mask, reader, .init=read_instance_init, .fini=read_instance_fini)
{
    uint32_t mask = DDS_NOT_READ_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
    dds_return_t expected_cnt = 1;
    dds_return_t ret;

    ret = dds_read_instance_mask(g_reader, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES, g_hdl_valid, mask);
    CU_ASSERT_EQUAL_FATAL(ret, expected_cnt);
    for(int i = 0; i < ret; i++) {
        Space_Type1 *sample = (Space_Type1*)g_samples[i];

        /*
         *    | long_1 | long_2 | long_3 |    sst   | vst |    ist     |
         *    ----------------------------------------------------------
         *    |    0   |    0   |    0   |     read | old | alive      |
         *    |    0   |    1   |    2   | not_read | old | alive      | <---
         *    |    1   |    2   |    4   | not_read | new | alive      |
         *    |    2   |    3   |    6   | not_read | new | disposed   |
         *    |    3   |    4   |    8   | not_read | new | no_writers |
         */

        /* Expected states. */
        int                  expected_long_1 = 0;
        int                  expected_long_2 = 1;
        dds_sample_state_t   expected_sst    = SAMPLE_SST(expected_long_2);
        dds_view_state_t     expected_vst    = SAMPLE_VST(expected_long_2);
        dds_instance_state_t expected_ist    = SAMPLE_IST(expected_long_2);

        /* Check data. */
        CU_ASSERT_EQUAL_FATAL(sample->long_1, expected_long_1  );
        CU_ASSERT_EQUAL_FATAL(sample->long_2, expected_long_2  );
        CU_ASSERT_EQUAL_FATAL(sample->long_3, expected_long_2*2);

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
CU_Test(ddsc_read_instance_mask_wl, reader, .init=read_instance_init, .fini=read_instance_fini)
{
    uint32_t mask = DDS_NOT_READ_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
    dds_return_t expected_cnt = 1;
    dds_return_t ret;

    ret = dds_read_instance_mask_wl(g_reader, g_loans, g_info, MAX_SAMPLES, g_hdl_valid, mask);
    CU_ASSERT_EQUAL_FATAL(ret, expected_cnt);
    for(int i = 0; i < ret; i++) {
        Space_Type1 *sample = (Space_Type1*)g_loans[i];

        /*
         *    | long_1 | long_2 | long_3 |    sst   | vst |    ist     |
         *    ----------------------------------------------------------
         *    |    0   |    0   |    0   |     read | old | alive      |
         *    |    0   |    1   |    2   | not_read | old | alive      | <---
         *    |    1   |    2   |    4   | not_read | new | alive      |
         *    |    2   |    3   |    6   | not_read | new | disposed   |
         *    |    3   |    4   |    8   | not_read | new | no_writers |
         */

        /* Expected states. */
        int                  expected_long_1 = 0;
        int                  expected_long_2 = 1;
        dds_sample_state_t   expected_sst    = SAMPLE_SST(expected_long_2);
        dds_view_state_t     expected_vst    = SAMPLE_VST(expected_long_2);
        dds_instance_state_t expected_ist    = SAMPLE_IST(expected_long_2);

        /* Check data. */
        CU_ASSERT_EQUAL_FATAL(sample->long_1, expected_long_1  );
        CU_ASSERT_EQUAL_FATAL(sample->long_2, expected_long_2  );
        CU_ASSERT_EQUAL_FATAL(sample->long_3, expected_long_2*2);

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
 * These will check the read_instance_* functions with a valid readcondition.
 *
 *************************************************************************************************/
/*************************************************************************************************/
CU_Test(ddsc_read_instance, readcondition, .init=read_instance_init, .fini=read_instance_fini)
{
    dds_return_t expected_cnt = 1;
    dds_return_t ret;

    ret = dds_read_instance(g_rcond, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES, g_hdl_valid);
    CU_ASSERT_EQUAL_FATAL(ret, expected_cnt);
    for(int i = 0; i < ret; i++) {
        Space_Type1 *sample = (Space_Type1*)g_samples[i];

        /*
         *    | long_1 | long_2 | long_3 |    sst   | vst |    ist     |
         *    ----------------------------------------------------------
         *    |    0   |    0   |    0   |     read | old | alive      |
         *    |    0   |    1   |    2   | not_read | old | alive      | <---
         *    |    1   |    2   |    4   | not_read | new | alive      |
         *    |    2   |    3   |    6   | not_read | new | disposed   |
         *    |    3   |    4   |    8   | not_read | new | no_writers |
         */

        /* Expected states. */
        int                  expected_long_1 = 0;
        int                  expected_long_2 = 1;
        dds_sample_state_t   expected_sst    = SAMPLE_SST(expected_long_2);
        dds_view_state_t     expected_vst    = SAMPLE_VST(expected_long_2);
        dds_instance_state_t expected_ist    = SAMPLE_IST(expected_long_2);

        /* Check data. */
        CU_ASSERT_EQUAL_FATAL(sample->long_1, expected_long_1  );
        CU_ASSERT_EQUAL_FATAL(sample->long_2, expected_long_2  );
        CU_ASSERT_EQUAL_FATAL(sample->long_3, expected_long_2*2);

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
CU_Test(ddsc_read_instance_wl, readcondition, .init=read_instance_init, .fini=read_instance_fini)
{
    dds_return_t expected_cnt = 1;
    dds_return_t ret;

    ret = dds_read_instance_wl(g_rcond, g_loans, g_info, MAX_SAMPLES, g_hdl_valid);
    CU_ASSERT_EQUAL_FATAL(ret, expected_cnt);
    for(int i = 0; i < ret; i++) {
        Space_Type1 *sample = (Space_Type1*)g_loans[i];

        /*
         *    | long_1 | long_2 | long_3 |    sst   | vst |    ist     |
         *    ----------------------------------------------------------
         *    |    0   |    0   |    0   |     read | old | alive      |
         *    |    0   |    1   |    2   | not_read | old | alive      | <---
         *    |    1   |    2   |    4   | not_read | new | alive      |
         *    |    2   |    3   |    6   | not_read | new | disposed   |
         *    |    3   |    4   |    8   | not_read | new | no_writers |
         */

        /* Expected states. */
        int                  expected_long_1 = 0;
        int                  expected_long_2 = 1;
        dds_sample_state_t   expected_sst    = SAMPLE_SST(expected_long_2);
        dds_view_state_t     expected_vst    = SAMPLE_VST(expected_long_2);
        dds_instance_state_t expected_ist    = SAMPLE_IST(expected_long_2);

        /* Check data. */
        CU_ASSERT_EQUAL_FATAL(sample->long_1, expected_long_1  );
        CU_ASSERT_EQUAL_FATAL(sample->long_2, expected_long_2  );
        CU_ASSERT_EQUAL_FATAL(sample->long_3, expected_long_2*2);

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
CU_Test(ddsc_read_instance_mask, readcondition, .init=read_instance_init, .fini=read_instance_fini)
{
    uint32_t mask = DDS_READ_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
    dds_return_t expected_cnt = 2;
    dds_return_t ret;

    ret = dds_read_instance_mask(g_rcond, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES, g_hdl_valid, mask);
    CU_ASSERT_EQUAL_FATAL(ret, expected_cnt);
    for(int i = 0; i < ret; i++) {
        Space_Type1 *sample = (Space_Type1*)g_samples[i];

        /*
         *    | long_1 | long_2 | long_3 |    sst   | vst |    ist     |
         *    ----------------------------------------------------------
         *    |    0   |    0   |    0   |     read | old | alive      | <---
         *    |    0   |    1   |    2   | not_read | old | alive      | <---
         *    |    1   |    2   |    4   | not_read | new | alive      |
         *    |    2   |    3   |    6   | not_read | new | disposed   |
         *    |    3   |    4   |    8   | not_read | new | no_writers |
         */

        /* Expected states. */
        int                  expected_long_1 = 0;
        int                  expected_long_2 = i;
        dds_sample_state_t   expected_sst    = SAMPLE_SST(expected_long_2);
        dds_view_state_t     expected_vst    = SAMPLE_VST(expected_long_2);
        dds_instance_state_t expected_ist    = SAMPLE_IST(expected_long_2);

        /* Check data. */
        CU_ASSERT_EQUAL_FATAL(sample->long_1, expected_long_1  );
        CU_ASSERT_EQUAL_FATAL(sample->long_2, expected_long_2  );
        CU_ASSERT_EQUAL_FATAL(sample->long_3, expected_long_2*2);

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
CU_Test(ddsc_read_instance_mask_wl, readcondition, .init=read_instance_init, .fini=read_instance_fini)
{
    uint32_t mask = DDS_NOT_READ_SAMPLE_STATE | DDS_NEW_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
    dds_return_t expected_cnt = 1;
    dds_return_t ret;

    ret = dds_read_instance_mask_wl(g_rcond, g_loans, g_info, MAX_SAMPLES, g_hdl_valid, mask);
    CU_ASSERT_EQUAL_FATAL(ret, expected_cnt);
    for(int i = 0; i < ret; i++) {
        Space_Type1 *sample = (Space_Type1*)g_loans[i];

        /*
         *    | long_1 | long_2 | long_3 |    sst   | vst |    ist     |
         *    ----------------------------------------------------------
         *    |    0   |    0   |    0   |     read | old | alive      |
         *    |    0   |    1   |    2   | not_read | old | alive      | <---
         *    |    1   |    2   |    4   | not_read | new | alive      |
         *    |    2   |    3   |    6   | not_read | new | disposed   |
         *    |    3   |    4   |    8   | not_read | new | no_writers |
         */

        /* Expected states. */
        int                  expected_long_1 = 0;
        int                  expected_long_2 = 1;
        dds_sample_state_t   expected_sst    = SAMPLE_SST(expected_long_2);
        dds_view_state_t     expected_vst    = SAMPLE_VST(expected_long_2);
        dds_instance_state_t expected_ist    = SAMPLE_IST(expected_long_2);

        /* Check data. */
        CU_ASSERT_EQUAL_FATAL(sample->long_1, expected_long_1  );
        CU_ASSERT_EQUAL_FATAL(sample->long_2, expected_long_2  );
        CU_ASSERT_EQUAL_FATAL(sample->long_3, expected_long_2*2);

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
 * These will check the read_instance_* functions with a valid querycondition.
 *
 *************************************************************************************************/
/*************************************************************************************************/
CU_Test(ddsc_read_instance, querycondition, .init=read_instance_init, .fini=read_instance_fini)
{
    dds_return_t expected_cnt = 1;
    dds_return_t ret;

    ret = dds_read_instance(g_qcond, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES, g_hdl_valid);
    CU_ASSERT_EQUAL_FATAL(ret, expected_cnt);
    for(int i = 0; i < ret; i++) {
        Space_Type1 *sample = (Space_Type1*)g_samples[i];

        /*
         *    | long_1 | long_2 | long_3 |    sst   | vst |    ist     |
         *    ----------------------------------------------------------
         *    |    0   |    0   |    0   |     read | old | alive      | <---
         *    |    0   |    1   |    2   | not_read | old | alive      |
         *    |    1   |    2   |    4   | not_read | new | alive      |
         *    |    2   |    3   |    6   | not_read | new | disposed   |
         *    |    3   |    4   |    8   | not_read | new | no_writers |
         */

        /* Expected states. */
        int                  expected_long_1 = 0;
        int                  expected_long_2 = 0;
        dds_sample_state_t   expected_sst    = SAMPLE_SST(expected_long_2);
        dds_view_state_t     expected_vst    = SAMPLE_VST(expected_long_2);
        dds_instance_state_t expected_ist    = SAMPLE_IST(expected_long_2);

        /* Check data. */
        CU_ASSERT_EQUAL_FATAL(sample->long_1, expected_long_1  );
        CU_ASSERT_EQUAL_FATAL(sample->long_2, expected_long_2  );
        CU_ASSERT_EQUAL_FATAL(sample->long_3, expected_long_2*2);

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
CU_Test(ddsc_read_instance_wl, querycondition, .init=read_instance_init, .fini=read_instance_fini)
{
    dds_return_t expected_cnt = 1;
    dds_return_t ret;

    ret = dds_read_instance_wl(g_qcond, g_loans, g_info, MAX_SAMPLES, g_hdl_valid);
    CU_ASSERT_EQUAL_FATAL(ret, expected_cnt);
    for(int i = 0; i < ret; i++) {
        Space_Type1 *sample = (Space_Type1*)g_loans[i];

        /*
         *    | long_1 | long_2 | long_3 |    sst   | vst |    ist     |
         *    ----------------------------------------------------------
         *    |    0   |    0   |    0   |     read | old | alive      | <---
         *    |    0   |    1   |    2   | not_read | old | alive      |
         *    |    1   |    2   |    4   | not_read | new | alive      |
         *    |    2   |    3   |    6   | not_read | new | disposed   |
         *    |    3   |    4   |    8   | not_read | new | no_writers |
         */

        /* Expected states. */
        int                  expected_long_1 = 0;
        int                  expected_long_2 = 0;
        dds_sample_state_t   expected_sst    = SAMPLE_SST(expected_long_2);
        dds_view_state_t     expected_vst    = SAMPLE_VST(expected_long_2);
        dds_instance_state_t expected_ist    = SAMPLE_IST(expected_long_2);

        /* Check data. */
        CU_ASSERT_EQUAL_FATAL(sample->long_1, expected_long_1  );
        CU_ASSERT_EQUAL_FATAL(sample->long_2, expected_long_2  );
        CU_ASSERT_EQUAL_FATAL(sample->long_3, expected_long_2*2);

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
CU_Test(ddsc_read_instance_mask, querycondition, .init=read_instance_init, .fini=read_instance_fini)
{
    uint32_t mask = DDS_READ_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
    dds_return_t expected_cnt = 1;
    dds_return_t ret;

    ret = dds_read_instance_mask(g_qcond, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES, g_hdl_valid, mask);
    CU_ASSERT_EQUAL_FATAL(ret, expected_cnt);
    for(int i = 0; i < ret; i++) {
        Space_Type1 *sample = (Space_Type1*)g_samples[i];

        /*
         *    | long_1 | long_2 | long_3 |    sst   | vst |    ist     |
         *    ----------------------------------------------------------
         *    |    0   |    0   |    0   |     read | old | alive      | <---
         *    |    0   |    1   |    2   | not_read | old | alive      |
         *    |    1   |    2   |    4   | not_read | new | alive      |
         *    |    2   |    3   |    6   | not_read | new | disposed   |
         *    |    3   |    4   |    8   | not_read | new | no_writers |
         */

        /* Expected states. */
        int                  expected_long_1 = 0;
        int                  expected_long_2 = 0;
        dds_sample_state_t   expected_sst    = SAMPLE_SST(expected_long_2);
        dds_view_state_t     expected_vst    = SAMPLE_VST(expected_long_2);
        dds_instance_state_t expected_ist    = SAMPLE_IST(expected_long_2);

        /* Check data. */
        CU_ASSERT_EQUAL_FATAL(sample->long_1, expected_long_1  );
        CU_ASSERT_EQUAL_FATAL(sample->long_2, expected_long_2  );
        CU_ASSERT_EQUAL_FATAL(sample->long_3, expected_long_2*2);

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
CU_Test(ddsc_read_instance_mask_wl, querycondition, .init=read_instance_init, .fini=read_instance_fini)
{
    uint32_t mask = DDS_NOT_READ_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
    dds_return_t expected_cnt = 1;
    dds_return_t ret;

    ret = dds_read_instance_mask_wl(g_qcond, g_loans, g_info, MAX_SAMPLES, g_hdl_valid, mask);
    CU_ASSERT_EQUAL_FATAL(ret, expected_cnt);
    for(int i = 0; i < ret; i++) {
        Space_Type1 *sample = (Space_Type1*)g_loans[i];

        /*
         *    | long_1 | long_2 | long_3 |    sst   | vst |    ist     |
         *    ----------------------------------------------------------
         *    |    0   |    0   |    0   |     read | old | alive      | <---
         *    |    0   |    1   |    2   | not_read | old | alive      |
         *    |    1   |    2   |    4   | not_read | new | alive      |
         *    |    2   |    3   |    6   | not_read | new | disposed   |
         *    |    3   |    4   |    8   | not_read | new | no_writers |
         */

        /* Expected states. */
        int                  expected_long_1 = 0;
        int                  expected_long_2 = 0;
        dds_sample_state_t   expected_sst    = SAMPLE_SST(expected_long_2);
        dds_view_state_t     expected_vst    = SAMPLE_VST(expected_long_2);
        dds_instance_state_t expected_ist    = SAMPLE_IST(expected_long_2);

        /* Check data. */
        CU_ASSERT_EQUAL_FATAL(sample->long_1, expected_long_1  );
        CU_ASSERT_EQUAL_FATAL(sample->long_2, expected_long_2  );
        CU_ASSERT_EQUAL_FATAL(sample->long_3, expected_long_2*2);

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
