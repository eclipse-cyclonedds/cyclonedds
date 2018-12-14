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
#include "CUnit/Test.h"
#include "CUnit/Theory.h"
#include "os/os.h"

#include "RoundTrip.h"

static dds_entity_t participant = DDS_ENTITY_NIL;
static dds_entity_t topic = DDS_ENTITY_NIL;
static dds_entity_t publisher = DDS_ENTITY_NIL;
static dds_entity_t subscriber =DDS_ENTITY_NIL;
static dds_entity_t writer = DDS_ENTITY_NIL;
static dds_entity_t reader = DDS_ENTITY_NIL;

static dds_return_t result;

static void
setup(void)
{
    participant = dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);
    CU_ASSERT_FATAL(participant > 0);
    topic = dds_create_topic(participant, &RoundTripModule_DataType_desc, "RoundTrip", NULL, NULL);
    CU_ASSERT_FATAL(topic > 0);
    publisher = dds_create_publisher(participant, NULL, NULL);
    CU_ASSERT_FATAL(publisher > 0);
    subscriber = dds_create_subscriber(participant, NULL, NULL);
    CU_ASSERT_FATAL(subscriber > 0);
    writer = dds_create_writer(publisher, topic, NULL, NULL);
    CU_ASSERT_FATAL(writer > 0);
    reader = dds_create_reader(subscriber, topic, NULL, NULL);
    CU_ASSERT_FATAL(writer > 0);
}

static void
teardown(void)
{
    dds_delete(reader);
    dds_delete(writer);
    dds_delete(publisher);
    dds_delete(subscriber);
    dds_delete(topic);
    dds_delete(participant);
}

CU_Test(ddsc_Contains, no_child, .init = setup, .fini = teardown )
{
    result = dds_contains(participant, DDS_ENTITY_NIL);
    CU_ASSERT_EQUAL_FATAL(dds_err_nr(result), DDS_RETCODE_BAD_PARAMETER);
}

CU_Test(ddsc_Contains, invalid_child, .init = setup, .fini = teardown )
{
    result = dds_contains(participant, -1);
    CU_ASSERT_EQUAL_FATAL(dds_err_nr(result), DDS_RETCODE_BAD_PARAMETER);
}

CU_Test(ddsc_Contains, no_parent, .init = setup, .fini = teardown )
{
    result = dds_contains(DDS_ENTITY_NIL, publisher);
    CU_ASSERT_EQUAL_FATAL(dds_err_nr(result), DDS_RETCODE_BAD_PARAMETER);
}

CU_Test(ddsc_Contains, invalid_parent, .init = setup, .fini = teardown )
{
    result = dds_contains(-1, publisher);
    CU_ASSERT_EQUAL_FATAL(dds_err_nr(result), DDS_RETCODE_BAD_PARAMETER);
}

CU_TheoryDataPoints(ddsc_Contains, true_cases) =
{
        CU_DataPoints(dds_entity_t*, &participant, &participant, &participant, &participant, &participant, &subscriber, &publisher),
        CU_DataPoints(dds_entity_t*, &subscriber, &publisher, &writer, &reader, &topic, &reader, &writer),
};
CU_Theory((dds_entity_t *parent, dds_entity_t *child), ddsc_Contains, true_cases, .init=setup, .fini=teardown)
{
    result = dds_contains(*parent, *child);
    CU_ASSERT_EQUAL_FATAL(result, *child);
}

CU_TheoryDataPoints(ddsc_Contains, false_cases) =
{
        CU_DataPoints(dds_entity_t*, &subscriber, &publisher, &writer, &reader, &topic, &reader, &reader, &reader, &reader, &writer, &writer, &writer, &writer, &writer, &publisher, &subscriber, &topic, &topic, &topic, &topic, &participant, &reader, &publisher, &subscriber, &reader, &topic),
        CU_DataPoints(dds_entity_t*, &participant, &participant, &participant, &participant, &participant, &topic, &subscriber, &publisher, &writer, &participant, &topic, &subscriber, &publisher, &reader, &reader, &writer, &publisher, &subscriber, &writer, &reader, &participant, &reader, &publisher, &subscriber, &reader, &topic),
};
CU_Theory((dds_entity_t *parent, dds_entity_t *child), ddsc_Contains, false_cases, .init=setup, .fini=teardown)
{
    result = dds_contains(*parent, *child);
    CU_ASSERT_EQUAL_FATAL(result, DDS_ENTITY_NIL);
}
