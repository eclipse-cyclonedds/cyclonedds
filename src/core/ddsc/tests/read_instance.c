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
read_instance_init(void)
{
    Space_Type1 sample = { 0, 0, 0 };
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

    g_topic = dds_create_topic(g_participant, &Space_Type1_desc, create_topic_name("ddsc_read_instance_test", name, sizeof name), NULL, NULL);
    cr_assert_gt(g_topic, 0, "Failed to create prerequisite g_topic");

    /* Create a writer that will not automatically dispose unregistered samples. */
    dds_qset_writer_data_lifecycle(qos, false);
    g_writer = dds_create_writer(g_publisher, g_topic, qos, NULL);
    cr_assert_gt(g_writer, 0, "Failed to create prerequisite g_writer");

    /* Create a reader that keeps all samples when not taken. */
    dds_qset_history(qos, DDS_HISTORY_KEEP_ALL, DDS_LENGTH_UNLIMITED);
    g_reader = dds_create_reader(g_subscriber, g_topic, qos, NULL);
    cr_assert_gt(g_reader, 0, "Failed to create prerequisite g_reader");

    /* Create a read condition that only reads not_read samples. */
    g_rcond = dds_create_readcondition(g_reader, DDS_NOT_READ_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE);
    cr_assert_gt(g_rcond, 0, "Failed to create prerequisite g_rcond");

    /* Create a query condition that only reads not_read samples of instances mod2. */
    g_qcond = dds_create_querycondition(g_reader, DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE, filter_mod2);
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

    /* Write the sample that will become {sst(read), vst(old), ist(alive)}. */
    sample.long_1 = 0;
    sample.long_2 = 0;
    sample.long_3 = 0;
    PRINT_SAMPLE("INIT: Write     ", sample);
    ret = dds_write(g_writer, &sample);
    cr_assert_eq(ret, DDS_RETCODE_OK, "Failed prerequisite write");
    /*  | long_1 | long_2 | long_3 |    sst   | vst |    ist     |
     *  ----------------------------------------------------------
     *  |    0   |    0   |    0   | not_read | new | alive      |
     */

    /* Read sample that will become {sst(read), vst(old), ist(alive)}. */
    ret = dds_read(g_reader, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
    cr_assert_eq(ret, 1, "Failed prerequisite read");
    for(int i = 0; i < ret; i++) {
        Space_Type1 *s = (Space_Type1*)g_samples[i];
        PRINT_SAMPLE("INIT: Read      ", (*s));
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
    PRINT_SAMPLE("INIT: Write     ", sample);
    ret = dds_write(g_writer, &sample);
    cr_assert_eq(ret, DDS_RETCODE_OK, "Failed prerequisite write");
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
        PRINT_SAMPLE("INIT: Write     ", sample);
        ret = dds_write(g_writer, &sample);
        cr_assert_eq(ret, DDS_RETCODE_OK, "Failed prerequisite write");
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
    PRINT_SAMPLE("INIT: Dispose   ", sample);
    ret = dds_dispose(g_writer, &sample);
    cr_assert_eq(ret, DDS_RETCODE_OK, "Failed prerequisite dispose");
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
    PRINT_SAMPLE("INIT: Unregister", sample);
    ret = dds_unregister_instance(g_writer, &sample);
    cr_assert_eq(ret, DDS_RETCODE_OK, "Failed prerequisite unregister");
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
    g_hdl_valid = dds_instance_lookup(g_reader, &sample);
    cr_assert_neq(g_hdl_valid, DDS_HANDLE_NIL, "Failed prerequisite dds_instance_lookup");

    dds_qos_delete(qos);
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
    cr_assert_geq(ret, 0, "Failed samples count read: %d", dds_err_nr(ret));
    return ret;
}





/**************************************************************************************************
 *
 * These will check the read_instance_* functions with invalid parameters.
 *
 *************************************************************************************************/
/*************************************************************************************************/
TheoryDataPoints(ddsc_read_instance, invalid_params) = {
        DataPoints(dds_entity_t*,               &g_reader, &g_rcond, &g_qcond),
        DataPoints(void**,                      g_samples, g_loans, (void**)0),
        DataPoints(dds_sample_info_t*,          g_info, (dds_sample_info_t*)0),
        DataPoints(size_t,                      0, 2, MAX_SAMPLES),
        DataPoints(uint32_t,                    0, 2, MAX_SAMPLES),
};
Theory((dds_entity_t *ent, void **buf, dds_sample_info_t *si, size_t bufsz, uint32_t maxs), ddsc_read_instance, invalid_params, .init=read_instance_init, .fini=read_instance_fini)
{
    dds_return_t ret;
    /* The only valid permutation is when non of the buffer values are
     * invalid and neither is the handle. So, don't test that. */
    cr_assume((buf != g_samples) || (si != g_info) || (bufsz == 0) || (maxs == 0) || (bufsz < maxs));
    /* TODO: CHAM-306, currently, a buffer is automatically 'promoted' to a loan when a buffer is
     * provided with NULL pointers. So, in fact, there's currently no real difference between calling
     * dds_read() dds_read_wl() (except for the provided bufsz). This will change, which means that
     * the given buffer should contain valid pointers, which again means that 'loan intended' buffer
     * should result in bad_parameter.
     * However, that's not the case yet. So don't test it. */
    cr_assume(buf != g_loans);
    ret = dds_read_instance(*ent, buf, si, bufsz, maxs, g_hdl_valid);
    cr_assert_eq(dds_err_nr(ret), DDS_RETCODE_BAD_PARAMETER, "returned %d", dds_err_nr(ret));
}
/*************************************************************************************************/

/*************************************************************************************************/
TheoryDataPoints(ddsc_read_instance_wl, invalid_params) = {
        DataPoints(dds_entity_t*,               &g_reader, &g_rcond, &g_qcond),
        DataPoints(void**,                      g_loans, (void**)0),
        DataPoints(dds_sample_info_t*,          g_info, (dds_sample_info_t*)0),
        DataPoints(size_t,                      0, 2, MAX_SAMPLES),
};
Theory((dds_entity_t *ent, void **buf, dds_sample_info_t *si, uint32_t maxs), ddsc_read_instance_wl, invalid_params, .init=read_instance_init, .fini=read_instance_fini)
{
    dds_return_t ret;
    /* The only valid permutation is when non of the buffer values are
     * invalid and neither is the handle. So, don't test that. */
    cr_assume((buf != g_loans) || (si != g_info) || (maxs == 0));
    ret = dds_read_instance_wl(*ent, buf, si, maxs, g_hdl_valid);
    cr_assert_eq(dds_err_nr(ret), DDS_RETCODE_BAD_PARAMETER, "returned %d", dds_err_nr(ret));
}
/*************************************************************************************************/

/*************************************************************************************************/
TheoryDataPoints(ddsc_read_instance_mask, invalid_params) = {
        DataPoints(dds_entity_t*,               &g_reader, &g_rcond, &g_qcond),
        DataPoints(void**,                      g_samples, g_loans, (void**)0),
        DataPoints(dds_sample_info_t*,          g_info, (dds_sample_info_t*)0),
        DataPoints(size_t,                      0, 2, MAX_SAMPLES),
        DataPoints(uint32_t,                    0, 2, MAX_SAMPLES),
};
Theory((dds_entity_t *ent, void **buf, dds_sample_info_t *si, size_t bufsz, uint32_t maxs), ddsc_read_instance_mask, invalid_params, .init=read_instance_init, .fini=read_instance_fini)
{
    uint32_t mask = DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
    dds_return_t ret;
    /* The only valid permutation is when non of the buffer values are
     * invalid and neither is the handle. So, don't test that. */
    cr_assume((buf != g_samples) || (si != g_info) || (bufsz == 0) || (maxs == 0) || (bufsz < maxs));
    /* TODO: CHAM-306, currently, a buffer is automatically 'promoted' to a loan when a buffer is
     * provided with NULL pointers. So, in fact, there's currently no real difference between calling
     * dds_read() dds_read_wl() (except for the provided bufsz). This will change, which means that
     * the given buffer should contain valid pointers, which again means that 'loan intended' buffer
     * should result in bad_parameter.
     * However, that's not the case yet. So don't test it. */
    cr_assume(buf != g_loans);
    ret = dds_read_instance_mask(*ent, buf, si, bufsz, maxs, g_hdl_valid, mask);
    cr_assert_eq(dds_err_nr(ret), DDS_RETCODE_BAD_PARAMETER, "returned %d", dds_err_nr(ret));
}
/*************************************************************************************************/

/*************************************************************************************************/
TheoryDataPoints(ddsc_read_instance_mask_wl, invalid_params) = {
        DataPoints(dds_entity_t*,               &g_reader, &g_rcond, &g_qcond),
        DataPoints(void**,                      g_loans, (void**)0),
        DataPoints(dds_sample_info_t*,          g_info, (dds_sample_info_t*)0),
        DataPoints(size_t,                      0, 2, MAX_SAMPLES),
};
Theory((dds_entity_t *ent, void **buf, dds_sample_info_t *si, uint32_t maxs), ddsc_read_instance_mask_wl, invalid_params, .init=read_instance_init, .fini=read_instance_fini)
{
    uint32_t mask = DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
    dds_return_t ret;
    /* The only valid permutation is when non of the buffer values are
     * invalid and neither is the handle. So, don't test that. */
    cr_assume((buf != g_loans) || (si != g_info) || (maxs == 0));
    ret = dds_read_instance_mask_wl(*ent, buf, si, maxs, g_hdl_valid, mask);
    cr_assert_eq(dds_err_nr(ret), DDS_RETCODE_BAD_PARAMETER, "returned %d", dds_err_nr(ret));
}
/*************************************************************************************************/





/**************************************************************************************************
 *
 * These will check the read_instance_* functions with invalid handles.
 *
 *************************************************************************************************/
/*************************************************************************************************/
TheoryDataPoints(ddsc_read_instance, invalid_handles) = {
        DataPoints(dds_entity_t*,         &g_reader, &g_rcond, &g_qcond),
        DataPoints(dds_instance_handle_t, DDS_HANDLE_NIL, 0, 1, 100, UINT64_MAX),
};
Theory((dds_entity_t *rdr, dds_instance_handle_t hdl), ddsc_read_instance, invalid_handles, .init=read_instance_init, .fini=read_instance_fini)
{
    dds_return_t ret;
    ret = dds_read_instance(*rdr, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES, hdl);
    cr_assert_eq(dds_err_nr(ret), DDS_RETCODE_PRECONDITION_NOT_MET, "returned %d", dds_err_nr(ret));
}
/*************************************************************************************************/

/*************************************************************************************************/
TheoryDataPoints(ddsc_read_instance_wl, invalid_handles) = {
        DataPoints(dds_entity_t*,         &g_reader, &g_rcond, &g_qcond),
        DataPoints(dds_instance_handle_t, DDS_HANDLE_NIL, 0, 1, 100, UINT64_MAX),
};
Theory((dds_entity_t *rdr, dds_instance_handle_t hdl), ddsc_read_instance_wl, invalid_handles, .init=read_instance_init, .fini=read_instance_fini)
{
    dds_return_t ret;
    ret = dds_read_instance_wl(*rdr, g_loans, g_info, MAX_SAMPLES, hdl);
    cr_assert_eq(dds_err_nr(ret), DDS_RETCODE_PRECONDITION_NOT_MET, "returned %d", dds_err_nr(ret));
}
/*************************************************************************************************/

/*************************************************************************************************/
TheoryDataPoints(ddsc_read_instance_mask, invalid_handles) = {
        DataPoints(dds_entity_t*,         &g_reader, &g_rcond, &g_qcond),
        DataPoints(dds_instance_handle_t, DDS_HANDLE_NIL, 0, 1, 100, UINT64_MAX),
};
Theory((dds_entity_t *rdr, dds_instance_handle_t hdl), ddsc_read_instance_mask, invalid_handles, .init=read_instance_init, .fini=read_instance_fini)
{
    uint32_t mask = DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
    dds_return_t ret;
    ret = dds_read_instance_mask(*rdr, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES, hdl, mask);
    cr_assert_eq(dds_err_nr(ret), DDS_RETCODE_PRECONDITION_NOT_MET, "returned %d", dds_err_nr(ret));
}
/*************************************************************************************************/

/*************************************************************************************************/
TheoryDataPoints(ddsc_read_instance_mask_wl, invalid_handles) = {
        DataPoints(dds_entity_t*,         &g_reader, &g_rcond, &g_qcond),
        DataPoints(dds_instance_handle_t, DDS_HANDLE_NIL, 0, 1, 100, UINT64_MAX),
};
Theory((dds_entity_t *rdr, dds_instance_handle_t hdl), ddsc_read_instance_mask_wl, invalid_handles, .init=read_instance_init, .fini=read_instance_fini)
{
    uint32_t mask = DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
    dds_return_t ret;
    ret = dds_read_instance_mask_wl(*rdr, g_loans, g_info, MAX_SAMPLES, hdl, mask);
    cr_assert_eq(dds_err_nr(ret), DDS_RETCODE_PRECONDITION_NOT_MET, "returned %d", dds_err_nr(ret));
}
/*************************************************************************************************/





/**************************************************************************************************
 *
 * These will check the read_instance_* functions with invalid readers.
 *
 *************************************************************************************************/
/*************************************************************************************************/
TheoryDataPoints(ddsc_read_instance, invalid_readers) = {
        DataPoints(dds_entity_t, -2, -1, 0, 1, 100, INT_MAX, INT_MIN),
};
Theory((dds_entity_t rdr), ddsc_read_instance, invalid_readers, .init=read_instance_init, .fini=read_instance_fini)
{
    dds_entity_t exp = DDS_RETCODE_BAD_PARAMETER * -1;
    dds_return_t ret;

    ret = dds_read_instance(rdr, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES, g_hdl_valid);
    cr_assert_eq(dds_err_nr(ret), dds_err_nr(exp), "returned %d != expected %d", dds_err_nr(ret), dds_err_nr(exp));
}
/*************************************************************************************************/

/*************************************************************************************************/
TheoryDataPoints(ddsc_read_instance_wl, invalid_readers) = {
        DataPoints(dds_entity_t, -2, -1, 0, 1, 100, INT_MAX, INT_MIN),
};
Theory((dds_entity_t rdr), ddsc_read_instance_wl, invalid_readers, .init=read_instance_init, .fini=read_instance_fini)
{
    dds_entity_t exp = DDS_RETCODE_BAD_PARAMETER * -1;
    dds_return_t ret;

    ret = dds_read_instance_wl(rdr, g_loans, g_info, MAX_SAMPLES, g_hdl_valid);
    cr_assert_eq(dds_err_nr(ret), dds_err_nr(exp), "returned %d != expected %d", dds_err_nr(ret), dds_err_nr(exp));
}
/*************************************************************************************************/

/*************************************************************************************************/
TheoryDataPoints(ddsc_read_instance_mask, invalid_readers) = {
        DataPoints(dds_entity_t, -2, -1, 0, 1, 100, INT_MAX, INT_MIN),
};
Theory((dds_entity_t rdr), ddsc_read_instance_mask, invalid_readers, .init=read_instance_init, .fini=read_instance_fini)
{
    uint32_t mask = DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
    dds_entity_t exp = DDS_RETCODE_BAD_PARAMETER * -1;
    dds_return_t ret;

    ret = dds_read_instance_mask(rdr, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES, g_hdl_valid, mask);
    cr_assert_eq(dds_err_nr(ret), dds_err_nr(exp), "returned %d != expected %d", dds_err_nr(ret), dds_err_nr(exp));
}
/*************************************************************************************************/

/*************************************************************************************************/
TheoryDataPoints(ddsc_read_instance_mask_wl, invalid_readers) = {
        DataPoints(dds_entity_t, -2, -1, 0, 1, 100, INT_MAX, INT_MIN),
};
Theory((dds_entity_t rdr), ddsc_read_instance_mask_wl, invalid_readers, .init=read_instance_init, .fini=read_instance_fini)
{
    uint32_t mask = DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
    dds_entity_t exp = DDS_RETCODE_BAD_PARAMETER * -1;
    dds_return_t ret;

    ret = dds_read_instance_mask_wl(rdr, g_loans, g_info, MAX_SAMPLES, g_hdl_valid, mask);
    cr_assert_eq(dds_err_nr(ret), dds_err_nr(exp), "returned %d != expected %d", dds_err_nr(ret), dds_err_nr(exp));
}
/*************************************************************************************************/






/**************************************************************************************************
 *
 * These will check the read_instance_* functions with non readers.
 *
 *************************************************************************************************/
/*************************************************************************************************/
TheoryDataPoints(ddsc_read_instance, non_readers) = {
        DataPoints(dds_entity_t*, &g_participant, &g_topic, &g_writer, &g_subscriber, &g_publisher, &g_waitset),
};
Theory((dds_entity_t *rdr), ddsc_read_instance, non_readers, .init=read_instance_init, .fini=read_instance_fini)
{
    dds_return_t ret;
    ret = dds_read_instance(*rdr, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES, g_hdl_valid);
    cr_assert_eq(dds_err_nr(ret), DDS_RETCODE_ILLEGAL_OPERATION, "returned %d", dds_err_nr(ret));
}
/*************************************************************************************************/

/*************************************************************************************************/
TheoryDataPoints(ddsc_read_instance_wl, non_readers) = {
        DataPoints(dds_entity_t*, &g_participant, &g_topic, &g_writer, &g_subscriber, &g_publisher, &g_waitset),
};
Theory((dds_entity_t *rdr), ddsc_read_instance_wl, non_readers, .init=read_instance_init, .fini=read_instance_fini)
{
    dds_return_t ret;
    ret = dds_read_instance_wl(*rdr, g_loans, g_info, MAX_SAMPLES, g_hdl_valid);
    cr_assert_eq(dds_err_nr(ret), DDS_RETCODE_ILLEGAL_OPERATION, "returned %d", dds_err_nr(ret));
}
/*************************************************************************************************/

/*************************************************************************************************/
TheoryDataPoints(ddsc_read_instance_mask, non_readers) = {
        DataPoints(dds_entity_t*, &g_participant, &g_topic, &g_writer, &g_subscriber, &g_publisher, &g_waitset),
};
Theory((dds_entity_t *rdr), ddsc_read_instance_mask, non_readers, .init=read_instance_init, .fini=read_instance_fini)
{
    uint32_t mask = DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
    dds_return_t ret;
    ret = dds_read_instance_mask(*rdr, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES, g_hdl_valid, mask);
    cr_assert_eq(dds_err_nr(ret), DDS_RETCODE_ILLEGAL_OPERATION, "returned %d", dds_err_nr(ret));
}
/*************************************************************************************************/

/*************************************************************************************************/
TheoryDataPoints(ddsc_read_instance_mask_wl, non_readers) = {
        DataPoints(dds_entity_t*, &g_participant, &g_topic, &g_writer, &g_subscriber, &g_publisher, &g_waitset),
};
Theory((dds_entity_t *rdr), ddsc_read_instance_mask_wl, non_readers, .init=read_instance_init, .fini=read_instance_fini)
{
    uint32_t mask = DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
    dds_return_t ret;
    ret = dds_read_instance_mask_wl(*rdr, g_loans, g_info, MAX_SAMPLES, g_hdl_valid, mask);
    cr_assert_eq(dds_err_nr(ret), DDS_RETCODE_ILLEGAL_OPERATION, "returned %d", dds_err_nr(ret));
}
/*************************************************************************************************/






/**************************************************************************************************
 *
 * These will check the read_instance_* functions with deleted readers.
 *
 *************************************************************************************************/
/*************************************************************************************************/
TheoryDataPoints(ddsc_read_instance, already_deleted) = {
        DataPoints(dds_entity_t*, &g_rcond, &g_qcond, &g_reader),
};
Theory((dds_entity_t *rdr), ddsc_read_instance, already_deleted, .init=read_instance_init, .fini=read_instance_fini)
{
    dds_return_t ret;
    ret = dds_delete(*rdr);
    cr_assert_eq(ret, DDS_RETCODE_OK, "prerequisite delete failed: %d", dds_err_nr(ret));
    ret = dds_read_instance(*rdr, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES, g_hdl_valid);
    cr_assert_eq(dds_err_nr(ret), DDS_RETCODE_ALREADY_DELETED, "returned %d", dds_err_nr(ret));
}
/*************************************************************************************************/

/*************************************************************************************************/
TheoryDataPoints(ddsc_read_instance_wl, already_deleted) = {
        DataPoints(dds_entity_t*, &g_rcond, &g_qcond, &g_reader),
};
Theory((dds_entity_t *rdr), ddsc_read_instance_wl, already_deleted, .init=read_instance_init, .fini=read_instance_fini)
{
    dds_return_t ret;
    ret = dds_delete(*rdr);
    cr_assert_eq(ret, DDS_RETCODE_OK, "prerequisite delete failed: %d", dds_err_nr(ret));
    ret = dds_read_instance_wl(*rdr, g_loans, g_info, MAX_SAMPLES, g_hdl_valid);
    cr_assert_eq(dds_err_nr(ret), DDS_RETCODE_ALREADY_DELETED, "returned %d", dds_err_nr(ret));
}
/*************************************************************************************************/

/*************************************************************************************************/
TheoryDataPoints(ddsc_read_instance_mask, already_deleted) = {
        DataPoints(dds_entity_t*, &g_rcond, &g_qcond, &g_reader),
};
Theory((dds_entity_t *rdr), ddsc_read_instance_mask, already_deleted, .init=read_instance_init, .fini=read_instance_fini)
{
    uint32_t mask = DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
    dds_return_t ret;
    ret = dds_delete(*rdr);
    cr_assert_eq(ret, DDS_RETCODE_OK, "prerequisite delete failed: %d", dds_err_nr(ret));
    ret = dds_read_instance_mask(*rdr, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES, g_hdl_valid, mask);
    cr_assert_eq(dds_err_nr(ret), DDS_RETCODE_ALREADY_DELETED, "returned %d", dds_err_nr(ret));
}
/*************************************************************************************************/

/*************************************************************************************************/
TheoryDataPoints(ddsc_read_instance_mask_wl, already_deleted) = {
        DataPoints(dds_entity_t*, &g_rcond, &g_qcond, &g_reader),
};
Theory((dds_entity_t *rdr), ddsc_read_instance_mask_wl, already_deleted, .init=read_instance_init, .fini=read_instance_fini)
{
    uint32_t mask = DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
    dds_return_t ret;
    ret = dds_delete(*rdr);
    cr_assert_eq(ret, DDS_RETCODE_OK, "prerequisite delete failed: %d", dds_err_nr(ret));
    ret = dds_read_instance_mask_wl(*rdr, g_loans, g_info, MAX_SAMPLES, g_hdl_valid, mask);
    cr_assert_eq(dds_err_nr(ret), DDS_RETCODE_ALREADY_DELETED, "returned %d", dds_err_nr(ret));
}
/*************************************************************************************************/






/**************************************************************************************************
 *
 * These will check the read_instance_* functions with a valid reader.
 *
 *************************************************************************************************/
/*************************************************************************************************/
Test(ddsc_read_instance, reader, .init=read_instance_init, .fini=read_instance_fini)
{
    dds_return_t expected_cnt = 2;
    dds_return_t ret;

    ret = dds_read_instance(g_reader, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES, g_hdl_valid);
    cr_assert_eq(ret, expected_cnt, "# read %d, expected %d", ret, expected_cnt);
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
        PRINT_SAMPLE("ddsc_read_instance::reader: Read", (*sample));

        /* Expected states. */
        int                  expected_long_1 = 0;
        int                  expected_long_2 = i;
        dds_sample_state_t   expected_sst    = SAMPLE_SST(expected_long_2);
        dds_view_state_t     expected_vst    = SAMPLE_VST(expected_long_2);
        dds_instance_state_t expected_ist    = SAMPLE_IST(expected_long_2);

        /* Check data. */
        cr_assert_eq(sample->long_1, expected_long_1  );
        cr_assert_eq(sample->long_2, expected_long_2  );
        cr_assert_eq(sample->long_3, expected_long_2*2);

        /* Check states. */
        cr_assert_eq(g_info[i].valid_data,     true);
        cr_assert_eq(g_info[i].sample_state,   expected_sst);
        cr_assert_eq(g_info[i].view_state,     expected_vst);
        cr_assert_eq(g_info[i].instance_state, expected_ist);
    }

    /* All samples should still be available. */
    ret = samples_cnt();
    cr_assert_eq(ret, MAX_SAMPLES, "# samples %d, expected %d", ret, MAX_SAMPLES);
}
/*************************************************************************************************/

/*************************************************************************************************/
Test(ddsc_read_instance_wl, reader, .init=read_instance_init, .fini=read_instance_fini)
{
    dds_return_t expected_cnt = 2;
    dds_return_t ret;

    ret = dds_read_instance_wl(g_reader, g_loans, g_info, MAX_SAMPLES, g_hdl_valid);
    cr_assert_eq(ret, expected_cnt, "# read %d, expected %d", ret, expected_cnt);
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
        PRINT_SAMPLE("ddsc_read_instance_wl::reader: Read", (*sample));

        /* Expected states. */
        int                  expected_long_1 = 0;
        int                  expected_long_2 = i;
        dds_sample_state_t   expected_sst    = SAMPLE_SST(expected_long_2);
        dds_view_state_t     expected_vst    = SAMPLE_VST(expected_long_2);
        dds_instance_state_t expected_ist    = SAMPLE_IST(expected_long_2);

        /* Check data. */
        cr_assert_eq(sample->long_1, expected_long_1  );
        cr_assert_eq(sample->long_2, expected_long_2  );
        cr_assert_eq(sample->long_3, expected_long_2*2);

        /* Check states. */
        cr_assert_eq(g_info[i].valid_data,     true);
        cr_assert_eq(g_info[i].sample_state,   expected_sst);
        cr_assert_eq(g_info[i].view_state,     expected_vst);
        cr_assert_eq(g_info[i].instance_state, expected_ist);
    }

    ret = dds_return_loan(g_reader, g_loans, ret);
    cr_assert_eq (ret, DDS_RETCODE_OK);

    /* All samples should still be available. */
    ret = samples_cnt();
    cr_assert_eq(ret, MAX_SAMPLES, "# samples %d, expected %d", ret, MAX_SAMPLES);
}
/*************************************************************************************************/

/*************************************************************************************************/
Test(ddsc_read_instance_mask, reader, .init=read_instance_init, .fini=read_instance_fini)
{
    uint32_t mask = DDS_NOT_READ_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
    dds_return_t expected_cnt = 1;
    dds_return_t ret;

    ret = dds_read_instance_mask(g_reader, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES, g_hdl_valid, mask);
    cr_assert_eq(ret, expected_cnt, "# read %d, expected %d", ret, expected_cnt);
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
        PRINT_SAMPLE("ddsc_read_instance_mask::reader: Read", (*sample));

        /* Expected states. */
        int                  expected_long_1 = 0;
        int                  expected_long_2 = 1;
        dds_sample_state_t   expected_sst    = SAMPLE_SST(expected_long_2);
        dds_view_state_t     expected_vst    = SAMPLE_VST(expected_long_2);
        dds_instance_state_t expected_ist    = SAMPLE_IST(expected_long_2);

        /* Check data. */
        cr_assert_eq(sample->long_1, expected_long_1  );
        cr_assert_eq(sample->long_2, expected_long_2  );
        cr_assert_eq(sample->long_3, expected_long_2*2);

        /* Check states. */
        cr_assert_eq(g_info[i].valid_data,     true);
        cr_assert_eq(g_info[i].sample_state,   expected_sst);
        cr_assert_eq(g_info[i].view_state,     expected_vst);
        cr_assert_eq(g_info[i].instance_state, expected_ist);
    }

    /* All samples should still be available. */
    ret = samples_cnt();
    cr_assert_eq(ret, MAX_SAMPLES, "# samples %d, expected %d", ret, MAX_SAMPLES);
}
/*************************************************************************************************/

/*************************************************************************************************/
Test(ddsc_read_instance_mask_wl, reader, .init=read_instance_init, .fini=read_instance_fini)
{
    uint32_t mask = DDS_NOT_READ_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
    dds_return_t expected_cnt = 1;
    dds_return_t ret;

    ret = dds_read_instance_mask_wl(g_reader, g_loans, g_info, MAX_SAMPLES, g_hdl_valid, mask);
    cr_assert_eq(ret, expected_cnt, "# read %d, expected %d", ret, expected_cnt);
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
        PRINT_SAMPLE("ddsc_read_instance_mask_wl::reader: Read", (*sample));

        /* Expected states. */
        int                  expected_long_1 = 0;
        int                  expected_long_2 = 1;
        dds_sample_state_t   expected_sst    = SAMPLE_SST(expected_long_2);
        dds_view_state_t     expected_vst    = SAMPLE_VST(expected_long_2);
        dds_instance_state_t expected_ist    = SAMPLE_IST(expected_long_2);

        /* Check data. */
        cr_assert_eq(sample->long_1, expected_long_1  );
        cr_assert_eq(sample->long_2, expected_long_2  );
        cr_assert_eq(sample->long_3, expected_long_2*2);

        /* Check states. */
        cr_assert_eq(g_info[i].valid_data,     true);
        cr_assert_eq(g_info[i].sample_state,   expected_sst);
        cr_assert_eq(g_info[i].view_state,     expected_vst);
        cr_assert_eq(g_info[i].instance_state, expected_ist);
    }

    ret = dds_return_loan(g_reader, g_loans, ret);
    cr_assert_eq (ret, DDS_RETCODE_OK);

    /* All samples should still be available. */
    ret = samples_cnt();
    cr_assert_eq(ret, MAX_SAMPLES, "# samples %d, expected %d", ret, MAX_SAMPLES);
}
/*************************************************************************************************/






/**************************************************************************************************
 *
 * These will check the read_instance_* functions with a valid readcondition.
 *
 *************************************************************************************************/
/*************************************************************************************************/
Test(ddsc_read_instance, readcondition, .init=read_instance_init, .fini=read_instance_fini)
{
    dds_return_t expected_cnt = 1;
    dds_return_t ret;

    ret = dds_read_instance(g_rcond, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES, g_hdl_valid);
    cr_assert_eq(ret, expected_cnt, "# read %d, expected %d", ret, expected_cnt);
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
        PRINT_SAMPLE("ddsc_read_instance::readcondition: Read", (*sample));

        /* Expected states. */
        int                  expected_long_1 = 0;
        int                  expected_long_2 = 1;
        dds_sample_state_t   expected_sst    = SAMPLE_SST(expected_long_2);
        dds_view_state_t     expected_vst    = SAMPLE_VST(expected_long_2);
        dds_instance_state_t expected_ist    = SAMPLE_IST(expected_long_2);

        /* Check data. */
        cr_assert_eq(sample->long_1, expected_long_1  );
        cr_assert_eq(sample->long_2, expected_long_2  );
        cr_assert_eq(sample->long_3, expected_long_2*2);

        /* Check states. */
        cr_assert_eq(g_info[i].valid_data,     true);
        cr_assert_eq(g_info[i].sample_state,   expected_sst);
        cr_assert_eq(g_info[i].view_state,     expected_vst);
        cr_assert_eq(g_info[i].instance_state, expected_ist);
    }

    /* All samples should still be available. */
    ret = samples_cnt();
    cr_assert_eq(ret, MAX_SAMPLES, "# samples %d, expected %d", ret, MAX_SAMPLES);
}
/*************************************************************************************************/

/*************************************************************************************************/
Test(ddsc_read_instance_wl, readcondition, .init=read_instance_init, .fini=read_instance_fini)
{
    dds_return_t expected_cnt = 1;
    dds_return_t ret;

    ret = dds_read_instance_wl(g_rcond, g_loans, g_info, MAX_SAMPLES, g_hdl_valid);
    cr_assert_eq(ret, expected_cnt, "# read %d, expected %d", ret, expected_cnt);
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
        PRINT_SAMPLE("ddsc_read_instance_wl::readcondition: Read", (*sample));

        /* Expected states. */
        int                  expected_long_1 = 0;
        int                  expected_long_2 = 1;
        dds_sample_state_t   expected_sst    = SAMPLE_SST(expected_long_2);
        dds_view_state_t     expected_vst    = SAMPLE_VST(expected_long_2);
        dds_instance_state_t expected_ist    = SAMPLE_IST(expected_long_2);

        /* Check data. */
        cr_assert_eq(sample->long_1, expected_long_1  );
        cr_assert_eq(sample->long_2, expected_long_2  );
        cr_assert_eq(sample->long_3, expected_long_2*2);

        /* Check states. */
        cr_assert_eq(g_info[i].valid_data,     true);
        cr_assert_eq(g_info[i].sample_state,   expected_sst);
        cr_assert_eq(g_info[i].view_state,     expected_vst);
        cr_assert_eq(g_info[i].instance_state, expected_ist);
    }

    ret = dds_return_loan(g_reader, g_loans, ret);
    cr_assert_eq (ret, DDS_RETCODE_OK);

    /* All samples should still be available. */
    ret = samples_cnt();
    cr_assert_eq(ret, MAX_SAMPLES, "# samples %d, expected %d", ret, MAX_SAMPLES);
}
/*************************************************************************************************/

/*************************************************************************************************/
Test(ddsc_read_instance_mask, readcondition, .init=read_instance_init, .fini=read_instance_fini)
{
    uint32_t mask = DDS_READ_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
    dds_return_t expected_cnt = 2;
    dds_return_t ret;

    ret = dds_read_instance_mask(g_rcond, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES, g_hdl_valid, mask);
    cr_assert_eq(ret, expected_cnt, "# read %d, expected %d", ret, expected_cnt);
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
        PRINT_SAMPLE("ddsc_read_instance_mask::readcondition: Read", (*sample));

        /* Expected states. */
        int                  expected_long_1 = 0;
        int                  expected_long_2 = i;
        dds_sample_state_t   expected_sst    = SAMPLE_SST(expected_long_2);
        dds_view_state_t     expected_vst    = SAMPLE_VST(expected_long_2);
        dds_instance_state_t expected_ist    = SAMPLE_IST(expected_long_2);

        /* Check data. */
        cr_assert_eq(sample->long_1, expected_long_1  );
        cr_assert_eq(sample->long_2, expected_long_2  );
        cr_assert_eq(sample->long_3, expected_long_2*2);

        /* Check states. */
        cr_assert_eq(g_info[i].valid_data,     true);
        cr_assert_eq(g_info[i].sample_state,   expected_sst);
        cr_assert_eq(g_info[i].view_state,     expected_vst);
        cr_assert_eq(g_info[i].instance_state, expected_ist);
    }

    /* All samples should still be available. */
    ret = samples_cnt();
    cr_assert_eq(ret, MAX_SAMPLES, "# samples %d, expected %d", ret, MAX_SAMPLES);
}
/*************************************************************************************************/

/*************************************************************************************************/
Test(ddsc_read_instance_mask_wl, readcondition, .init=read_instance_init, .fini=read_instance_fini)
{
    uint32_t mask = DDS_NOT_READ_SAMPLE_STATE | DDS_NEW_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
    dds_return_t expected_cnt = 1;
    dds_return_t ret;

    ret = dds_read_instance_mask_wl(g_rcond, g_loans, g_info, MAX_SAMPLES, g_hdl_valid, mask);
    cr_assert_eq(ret, expected_cnt, "# read %d, expected %d", ret, expected_cnt);
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
        PRINT_SAMPLE("ddsc_read_instance_mask_wl::readcondition: Read", (*sample));

        /* Expected states. */
        int                  expected_long_1 = 0;
        int                  expected_long_2 = 1;
        dds_sample_state_t   expected_sst    = SAMPLE_SST(expected_long_2);
        dds_view_state_t     expected_vst    = SAMPLE_VST(expected_long_2);
        dds_instance_state_t expected_ist    = SAMPLE_IST(expected_long_2);

        /* Check data. */
        cr_assert_eq(sample->long_1, expected_long_1  );
        cr_assert_eq(sample->long_2, expected_long_2  );
        cr_assert_eq(sample->long_3, expected_long_2*2);

        /* Check states. */
        cr_assert_eq(g_info[i].valid_data,     true);
        cr_assert_eq(g_info[i].sample_state,   expected_sst);
        cr_assert_eq(g_info[i].view_state,     expected_vst);
        cr_assert_eq(g_info[i].instance_state, expected_ist);
    }

    ret = dds_return_loan(g_reader, g_loans, ret);
    cr_assert_eq (ret, DDS_RETCODE_OK);

    /* All samples should still be available. */
    ret = samples_cnt();
    cr_assert_eq(ret, MAX_SAMPLES, "# samples %d, expected %d", ret, MAX_SAMPLES);
}
/*************************************************************************************************/






/**************************************************************************************************
 *
 * These will check the read_instance_* functions with a valid querycondition.
 *
 *************************************************************************************************/
/*************************************************************************************************/
Test(ddsc_read_instance, querycondition, .init=read_instance_init, .fini=read_instance_fini)
{
    dds_return_t expected_cnt = 1;
    dds_return_t ret;

    ret = dds_read_instance(g_qcond, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES, g_hdl_valid);
    cr_assert_eq(ret, expected_cnt, "# read %d, expected %d", ret, expected_cnt);
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
        PRINT_SAMPLE("ddsc_read_instance::querycondition: Read", (*sample));

        /* Expected states. */
        int                  expected_long_1 = 0;
        int                  expected_long_2 = 0;
        dds_sample_state_t   expected_sst    = SAMPLE_SST(expected_long_2);
        dds_view_state_t     expected_vst    = SAMPLE_VST(expected_long_2);
        dds_instance_state_t expected_ist    = SAMPLE_IST(expected_long_2);

        /* Check data. */
        cr_assert_eq(sample->long_1, expected_long_1  );
        cr_assert_eq(sample->long_2, expected_long_2  );
        cr_assert_eq(sample->long_3, expected_long_2*2);

        /* Check states. */
        cr_assert_eq(g_info[i].valid_data,     true);
        cr_assert_eq(g_info[i].sample_state,   expected_sst);
        cr_assert_eq(g_info[i].view_state,     expected_vst);
        cr_assert_eq(g_info[i].instance_state, expected_ist);
    }

    /* All samples should still be available. */
    ret = samples_cnt();
    cr_assert_eq(ret, MAX_SAMPLES, "# samples %d, expected %d", ret, MAX_SAMPLES);
}
/*************************************************************************************************/

/*************************************************************************************************/
Test(ddsc_read_instance_wl, querycondition, .init=read_instance_init, .fini=read_instance_fini)
{
    dds_return_t expected_cnt = 1;
    dds_return_t ret;

    ret = dds_read_instance_wl(g_qcond, g_loans, g_info, MAX_SAMPLES, g_hdl_valid);
    cr_assert_eq(ret, expected_cnt, "# read %d, expected %d", ret, expected_cnt);
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
        PRINT_SAMPLE("ddsc_read_instance_wl::querycondition: Read", (*sample));

        /* Expected states. */
        int                  expected_long_1 = 0;
        int                  expected_long_2 = 0;
        dds_sample_state_t   expected_sst    = SAMPLE_SST(expected_long_2);
        dds_view_state_t     expected_vst    = SAMPLE_VST(expected_long_2);
        dds_instance_state_t expected_ist    = SAMPLE_IST(expected_long_2);

        /* Check data. */
        cr_assert_eq(sample->long_1, expected_long_1  );
        cr_assert_eq(sample->long_2, expected_long_2  );
        cr_assert_eq(sample->long_3, expected_long_2*2);

        /* Check states. */
        cr_assert_eq(g_info[i].valid_data,     true);
        cr_assert_eq(g_info[i].sample_state,   expected_sst);
        cr_assert_eq(g_info[i].view_state,     expected_vst);
        cr_assert_eq(g_info[i].instance_state, expected_ist);
    }

    ret = dds_return_loan(g_reader, g_loans, ret);
    cr_assert_eq (ret, DDS_RETCODE_OK);

    /* All samples should still be available. */
    ret = samples_cnt();
    cr_assert_eq(ret, MAX_SAMPLES, "# samples %d, expected %d", ret, MAX_SAMPLES);
}
/*************************************************************************************************/

/*************************************************************************************************/
Test(ddsc_read_instance_mask, querycondition, .init=read_instance_init, .fini=read_instance_fini)
{
    uint32_t mask = DDS_READ_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
    dds_return_t expected_cnt = 1;
    dds_return_t ret;

    ret = dds_read_instance_mask(g_qcond, g_samples, g_info, MAX_SAMPLES, MAX_SAMPLES, g_hdl_valid, mask);
    cr_assert_eq(ret, expected_cnt, "# read %d, expected %d", ret, expected_cnt);
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
        PRINT_SAMPLE("ddsc_read_instance_mask::querycondition: Read", (*sample));

        /* Expected states. */
        int                  expected_long_1 = 0;
        int                  expected_long_2 = 0;
        dds_sample_state_t   expected_sst    = SAMPLE_SST(expected_long_2);
        dds_view_state_t     expected_vst    = SAMPLE_VST(expected_long_2);
        dds_instance_state_t expected_ist    = SAMPLE_IST(expected_long_2);

        /* Check data. */
        cr_assert_eq(sample->long_1, expected_long_1  );
        cr_assert_eq(sample->long_2, expected_long_2  );
        cr_assert_eq(sample->long_3, expected_long_2*2);

        /* Check states. */
        cr_assert_eq(g_info[i].valid_data,     true);
        cr_assert_eq(g_info[i].sample_state,   expected_sst);
        cr_assert_eq(g_info[i].view_state,     expected_vst);
        cr_assert_eq(g_info[i].instance_state, expected_ist);
    }

    /* All samples should still be available. */
    ret = samples_cnt();
    cr_assert_eq(ret, MAX_SAMPLES, "# samples %d, expected %d", ret, MAX_SAMPLES);
}
/*************************************************************************************************/

/*************************************************************************************************/
Test(ddsc_read_instance_mask_wl, querycondition, .init=read_instance_init, .fini=read_instance_fini)
{
    uint32_t mask = DDS_NOT_READ_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
    dds_return_t expected_cnt = 1;
    dds_return_t ret;

    ret = dds_read_instance_mask_wl(g_qcond, g_loans, g_info, MAX_SAMPLES, g_hdl_valid, mask);
    cr_assert_eq(ret, expected_cnt, "# read %d, expected %d", ret, expected_cnt);
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
        PRINT_SAMPLE("ddsc_read_instance_mask_wl::querycondition: Read", (*sample));

        /* Expected states. */
        int                  expected_long_1 = 0;
        int                  expected_long_2 = 0;
        dds_sample_state_t   expected_sst    = SAMPLE_SST(expected_long_2);
        dds_view_state_t     expected_vst    = SAMPLE_VST(expected_long_2);
        dds_instance_state_t expected_ist    = SAMPLE_IST(expected_long_2);

        /* Check data. */
        cr_assert_eq(sample->long_1, expected_long_1  );
        cr_assert_eq(sample->long_2, expected_long_2  );
        cr_assert_eq(sample->long_3, expected_long_2*2);

        /* Check states. */
        cr_assert_eq(g_info[i].valid_data,     true);
        cr_assert_eq(g_info[i].sample_state,   expected_sst);
        cr_assert_eq(g_info[i].view_state,     expected_vst);
        cr_assert_eq(g_info[i].instance_state, expected_ist);
    }

    ret = dds_return_loan(g_reader, g_loans, ret);
    cr_assert_eq (ret, DDS_RETCODE_OK);

    /* All samples should still be available. */
    ret = samples_cnt();
    cr_assert_eq(ret, MAX_SAMPLES, "# samples %d, expected %d", ret, MAX_SAMPLES);
}
/*************************************************************************************************/
