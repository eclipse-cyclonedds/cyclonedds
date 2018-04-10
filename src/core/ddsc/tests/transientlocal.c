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
#include <stdio.h>
#include "ddsc/dds.h"
#include "Space.h"
#include <criterion/criterion.h>
#include <criterion/logging.h>

#define TRACE_SAMPLE(info, sample) cr_log_info("%s (%d, %d, %d)\n", info, sample.long_1, sample.long_2, sample.long_3);
#define MAX_SAMPLES  (7)
Test(ddsc_transient_local, late_joiner)
{
    Space_Type1 sample = { 0 };
    dds_return_t ret;
    dds_entity_t par;
    dds_entity_t pub;
    dds_entity_t sub;
    dds_entity_t top;
    dds_entity_t wrt;
    dds_entity_t rdr;
    dds_qos_t   *qos;
    static void*              samples[MAX_SAMPLES];
    static Space_Type1        data[MAX_SAMPLES];
    static dds_sample_info_t  info[MAX_SAMPLES];

    /* Initialize reading buffers. */
    memset (data, 0, sizeof (data));
    for (int i = 0; i < MAX_SAMPLES; i++) {
        samples[i] = &data[i];
    }

    /* Use transient local with reliable. */
    qos = dds_qos_create();
    dds_qset_durability(qos, DDS_DURABILITY_TRANSIENT_LOCAL);
    dds_qset_reliability(qos, DDS_RELIABILITY_RELIABLE, DDS_INFINITY);

    /* Create participant and topic. */
    par = dds_create_participant(DDS_DOMAIN_DEFAULT, qos, NULL);
    cr_assert_gt(par, 0, "Failed to create prerequisite par");
    top = dds_create_topic(par, &Space_Type1_desc, "ddsc_transient_local_happy_days", qos, NULL);
    cr_assert_gt(par, 0, "Failed to create prerequisite top");

    /* Create publishing entities. */
    cr_log_info("Create writer\n");
    pub = dds_create_publisher(par, qos, NULL);
    cr_assert_gt(pub, 0, "Failed to create prerequisite pub");
    wrt = dds_create_writer(pub, top, qos, NULL);
    cr_assert_gt(wrt, 0, "Failed to create prerequisite wrt");

    /* Write first set of samples. */
    sample.long_1 = 1;
    sample.long_2 = 1;
    sample.long_3 = 1;
    TRACE_SAMPLE("Write ", sample);
    ret = dds_write(wrt, &sample);
    cr_assert_eq(ret, DDS_RETCODE_OK, "Failed prerequisite write");
    sample.long_1 = 2;
    sample.long_2 = 2;
    sample.long_3 = 2;
    TRACE_SAMPLE("Write ", sample);
    ret = dds_write(wrt, &sample);
    cr_assert_eq(ret, DDS_RETCODE_OK, "Failed prerequisite write");

    /* Create subscribing entities. */
    cr_log_info("Create reader\n");
    sub = dds_create_subscriber(par, qos, NULL);
    cr_assert_gt(sub, 0, "Failed to create prerequisite sub");
    rdr = dds_create_reader(sub, top, qos, NULL);
    cr_assert_gt(rdr, 0, "Failed to create prerequisite g_reader");

    /* Write second set of samples. */
    sample.long_1 = 8;
    sample.long_2 = 8;
    sample.long_3 = 8;
    TRACE_SAMPLE("Write ", sample);
    ret = dds_write(wrt, &sample);
    cr_assert_eq(ret, DDS_RETCODE_OK, "Failed prerequisite write");
    sample.long_1 = 9;
    sample.long_2 = 9;
    sample.long_3 = 9;
    TRACE_SAMPLE("Write ", sample);
    ret = dds_write(wrt, &sample);
    cr_assert_eq(ret, DDS_RETCODE_OK, "Failed prerequisite write");

    /* Read samples, which should be all four. */
    ret = dds_read(rdr, samples, info, MAX_SAMPLES, MAX_SAMPLES);
    cr_log_info("Read cnt %d\n", ret);
    for(int i = 0; i < ret; i++) {
        Space_Type1 *sample = (Space_Type1*)samples[i];
        TRACE_SAMPLE("Read  ", (*sample));
    }
    cr_assert_eq(ret, 4, "# read %d, expected 4", ret);

    dds_delete(par);
    dds_qos_delete(qos);
}
