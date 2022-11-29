/*
 * Copyright(c) 2019 to 2022 ZettaScale Technology and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */

#include <inttypes.h>

#include "dds/dds.h"

#include "test_common.h"

#include "Space.h"

static int32_t seq = 0;
static dds_entity_t pp = 0;
static dds_entity_t rd = 0;
static dds_entity_t wr = 0;

static void setup(const char *topic_name, dds_duration_t sep, dds_destination_order_kind_t dok)
{
    pp = dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);
    CU_ASSERT_FATAL(pp > 0);

    dds_entity_t tp = dds_create_topic(pp, &Space_Type1_desc, topic_name, NULL, NULL);
    CU_ASSERT_FATAL(tp > 0);

    //qos
    dds_qos_t qos;
    ddsi_xqos_init_empty (&qos);
    qos.present |= DDSI_QP_HISTORY | DDSI_QP_DESTINATION_ORDER | DDSI_QP_TIME_BASED_FILTER;
    qos.history.kind = DDS_HISTORY_KEEP_LAST;
    qos.history.depth = 1;
    qos.destination_order.kind = dok;
    qos.time_based_filter.minimum_separation = sep;

    rd = dds_create_reader (pp, tp, &qos, NULL);
    CU_ASSERT_FATAL(rd > 0);

    wr = dds_create_writer(pp, tp, &qos, NULL);
    CU_ASSERT_FATAL(wr > 0);

    dds_return_t rc = dds_set_status_mask(wr, DDS_PUBLICATION_MATCHED_STATUS);
    CU_ASSERT_EQUAL_FATAL(rc, DDS_RETCODE_OK);

    uint32_t status = 0;
    while(!(status & DDS_PUBLICATION_MATCHED_STATUS))
    {
        rc = dds_get_status_changes (wr, &status);
        CU_ASSERT_EQUAL_FATAL(rc, DDS_RETCODE_OK);

        /* Polling sleep. */
        dds_sleepfor (DDS_MSECS (20));
    }
}

static void teardown()
{
    CU_ASSERT_EQUAL_FATAL (dds_delete(pp), DDS_RETCODE_OK);
}

static void test_f(dds_duration_t ts, uint32_t total_count, int32_t total_count_change)
{
    Space_Type1 msg = { 123, ++seq, 0};

    dds_return_t ret = dds_write_ts(wr, &msg, ts);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    dds_sample_lost_status_t sl_status;
    ret = dds_get_sample_lost_status (rd, &sl_status);
    CU_ASSERT_EQUAL(ret, DDS_RETCODE_OK);
    CU_ASSERT_EQUAL(sl_status.total_count, total_count);
    CU_ASSERT_EQUAL(sl_status.total_count_change, total_count_change);
}

CU_Test(ddsc_time_based_filter, no_sep)
{
    dds_duration_t sep = DDS_MSECS(0);
    dds_destination_order_kind_t dok = DDS_DESTINATIONORDER_BY_SOURCE_TIMESTAMP;

    setup("ddsc_time_based_filter_no_sep", sep, dok);

    test_f(DDS_MSECS(1), 0, 0);
    test_f(DDS_MSECS(0), 1, 1);
    test_f(DDS_MSECS(1), 1, 0);

    teardown();
}

CU_Test(ddsc_time_based_filter, sep)
{
    dds_duration_t sep = DDS_MSECS(100);
    dds_destination_order_kind_t dok = DDS_DESTINATIONORDER_BY_SOURCE_TIMESTAMP;

    setup("ddsc_time_based_filter_sep", sep, dok);

    test_f(DDS_MSECS(1), 0, 0);
    test_f(DDS_MSECS(0), 1, 1);
    test_f(DDS_MSECS(1), 2, 1);
    test_f(DDS_MSECS(101), 2, 0);

    teardown();
}
