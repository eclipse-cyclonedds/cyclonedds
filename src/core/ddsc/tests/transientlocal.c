// Copyright(c) 2006 to 2019 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <stdio.h>
#include "dds/dds.h"
#include "Space.h"
#include "CUnit/Test.h"

#define MAX_SAMPLES  (7)
CU_Test(ddsc_transient_local, late_joiner)
{
    Space_Type1 sample = { 0, 0, 0 };
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
    qos = dds_create_qos();
    dds_qset_durability(qos, DDS_DURABILITY_TRANSIENT_LOCAL);
    dds_qset_reliability(qos, DDS_RELIABILITY_RELIABLE, DDS_INFINITY);

    /* Create participant and topic. */
    par = dds_create_participant(DDS_DOMAIN_DEFAULT, qos, NULL);
    CU_ASSERT_FATAL(par > 0);
    top = dds_create_topic(par, &Space_Type1_desc, "ddsc_transient_local_happy_days", qos, NULL);
    CU_ASSERT_FATAL(par > 0);

    /* Create publishing entities. */
    pub = dds_create_publisher(par, qos, NULL);
    CU_ASSERT_FATAL(pub > 0);
    wrt = dds_create_writer(pub, top, qos, NULL);
    CU_ASSERT_FATAL(wrt > 0);

    /* Write first set of samples. */
    sample.long_1 = 1;
    sample.long_2 = 1;
    sample.long_3 = 1;
    ret = dds_write(wrt, &sample);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    sample.long_1 = 2;
    sample.long_2 = 2;
    sample.long_3 = 2;
    ret = dds_write(wrt, &sample);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    /* Create subscribing entities. */
    sub = dds_create_subscriber(par, qos, NULL);
    CU_ASSERT_FATAL(sub > 0);
    rdr = dds_create_reader(sub, top, qos, NULL);
    CU_ASSERT_FATAL(rdr > 0);

    /* Write second set of samples. */
    sample.long_1 = 8;
    sample.long_2 = 8;
    sample.long_3 = 8;
    ret = dds_write(wrt, &sample);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    sample.long_1 = 9;
    sample.long_2 = 9;
    sample.long_3 = 9;
    ret = dds_write(wrt, &sample);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    /* Read samples, which should be all four. */
    ret = dds_read(rdr, samples, info, MAX_SAMPLES, MAX_SAMPLES);
#if 0
    for(int i = 0; i < ret; i++) {
        Space_Type1 *sample = (Space_Type1*)samples[i];
    }
#endif
    CU_ASSERT_EQUAL_FATAL(ret, 4);

    dds_delete(par);
    dds_delete_qos(qos);
}
