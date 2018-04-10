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
#include "RoundTrip.h"
#include "os/os.h"

#include <stdio.h>
#include <criterion/criterion.h>
#include <criterion/logging.h>

dds_entity_t participant = 0, topic = 0, reader = 0, read_condition = 0;

void create_entities(void)
{
    participant = dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);
    cr_assert_gt(participant, 0);

    topic = dds_create_topic(participant, &RoundTripModule_DataType_desc, "ddsc_reader_return_loan_RoundTrip", NULL, NULL);
    cr_assert_gt(topic, 0);

    reader = dds_create_reader(participant, topic, NULL, NULL);
    cr_assert_gt(reader, 0);

    read_condition = dds_create_readcondition(reader, DDS_ANY_STATE);
    cr_assert_gt(read_condition, 0);
}

void delete_entities(void)
{
    dds_return_t result;
    result = dds_delete(participant);
    cr_assert_eq(dds_err_nr(result), DDS_RETCODE_OK, "Recursively delete entities, Expected(%s) Returned(%s)", DDS_TO_STRING(DDS_RETCODE_OK), dds_err_str(result));
    dds_delete(read_condition);
}

static void** create_loan_buf(size_t sz, bool empty)
{
    size_t i;
    void **buf = NULL;
    buf = dds_alloc(sz * sizeof(*buf));
    for (i = 0; i < sz; i++) {
        buf[i] = dds_alloc(sizeof(RoundTripModule_DataType));
        if (empty) {
            memset(buf[i], 0, sizeof(RoundTripModule_DataType));
        } else {
            RoundTripModule_DataType *s = buf[i];
            s->payload._maximum = 0;
            s->payload._length = 25;
            s->payload._buffer = dds_alloc(25);
            memset(s->payload._buffer, 'z', 25);
            s->payload._release = true;
        }
    }
    return buf;
}

static void delete_loan_buf(void **buf, size_t sz, bool empty)
{
    size_t i;
    for (i = 0; i < sz; i++) {
        RoundTripModule_DataType *s = buf[i];
        if (!empty) {
            cr_expect_gt(s->payload._length, 0, "Expected allocated 'payload'-sequence in sample-contents of loan");
            if (s->payload._length > 0) {
                /* Freed by a successful dds_return_loan */
                dds_free(s->payload._buffer);
            }
        }
        /* dds_return_loan only free's sample contents */
        dds_free(s);
    }
    dds_free(buf);
}

/* Verify DDS_RETCODE_BAD_PARAMETER is returned */
Test(ddsc_reader, return_loan_bad_params, .init = create_entities, .fini = delete_entities)
{
    dds_return_t result;
    void **buf = NULL;

    result = dds_return_loan(reader, NULL, 0);
    cr_expect_eq(dds_err_nr(result), DDS_RETCODE_BAD_PARAMETER, "Invalid buffer(null), Expected(%s) Returned(%s)",
        DDS_TO_STRING(DDS_RETCODE_BAD_PARAMETER),
        dds_err_str(result));

#pragma warning(push)
#pragma warning(disable: 6387)
    result = dds_return_loan(reader, buf, 10);
#pragma warning(pop)
    cr_expect_eq(dds_err_nr(result), DDS_RETCODE_BAD_PARAMETER, "Invalid buffer size, Expected(%s) Returned(%s)",
        DDS_TO_STRING(DDS_RETCODE_BAD_PARAMETER),
        dds_err_str(result));

    buf = create_loan_buf(10, false);
#pragma warning(push)
#pragma warning(disable: 28020)
    result = dds_return_loan(0, buf, 10);
#pragma warning(pop)
    cr_expect_eq(dds_err_nr(result), DDS_RETCODE_BAD_PARAMETER, "Invalid entity, Expected(%s) Returned(%s)",
        DDS_TO_STRING(DDS_RETCODE_BAD_PARAMETER),
        dds_err_str(result));

    result = dds_return_loan(participant, buf, 0);
    cr_expect_eq(dds_err_nr(result), DDS_RETCODE_ILLEGAL_OPERATION, "Invalid entity-kind, Expected(%s) Returned(%s)",
        DDS_TO_STRING(DDS_RETCODE_ILLEGAL_OPERATION),
        dds_err_str(result));

    delete_loan_buf(buf, 10, false);
}

/* Verify DDS_RETCODE_OK is returned */
Test(ddsc_reader, return_loan_success, .init = create_entities, .fini = delete_entities)
{
    void **buf;
    void *buf2 = NULL;
    dds_return_t result;

    buf = create_loan_buf(10, false);
    result = dds_return_loan(reader, buf, 10);
    cr_expect_eq(dds_err_nr(result), DDS_RETCODE_OK, "Return loan of size 10 via reader entity, Expected(%s) Returned(%s)",
        DDS_TO_STRING(DDS_RETCODE_OK),
        dds_err_str(result));

    result = dds_return_loan(reader, &buf2, 0);
    cr_expect_eq(dds_err_nr(result), DDS_RETCODE_OK, "Return empty loan via reader entity, Expected(%s) Returned(%s)",
        DDS_TO_STRING(DDS_RETCODE_OK),
        dds_err_str(result));
    delete_loan_buf(buf, 10, true);

    buf = create_loan_buf(10, false);
    result = dds_return_loan(read_condition, buf, 10);
    cr_expect_eq(dds_err_nr(result), DDS_RETCODE_OK, "Return loan of size 10 via read-condition entity, Expected(%s) Returned(%s)",
        DDS_TO_STRING(DDS_RETCODE_OK),
        dds_err_str(result));

    result = dds_return_loan(read_condition, &buf2, 0);
    cr_expect_eq(dds_err_nr(result), DDS_RETCODE_OK, "Return empty loan via read-condition entity, Expected(%s) Returned(%s)",
        DDS_TO_STRING(DDS_RETCODE_OK),
        dds_err_str(result));
    delete_loan_buf(buf, 10, true);
}
