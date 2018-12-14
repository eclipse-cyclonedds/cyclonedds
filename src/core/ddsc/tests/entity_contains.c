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
static dds_entity_t waitset = DDS_ENTITY_NIL;
static dds_entity_t publisher2 = DDS_ENTITY_NIL;
static dds_entity_t subscriber2 =DDS_ENTITY_NIL;
static dds_entity_t writer2 = DDS_ENTITY_NIL;
static dds_entity_t reader2 = DDS_ENTITY_NIL;
static dds_entity_t readcond = DDS_ENTITY_NIL;
static dds_entity_t querycond = DDS_ENTITY_NIL;

/* Dummy query condition callback. */
static bool
accept_all(const void * sample)
{
    (void)sample;
    return true;
}

static dds_entity_t result;

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
    CU_ASSERT_FATAL(reader > 0);
    uint32_t mask = DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
    readcond = dds_create_readcondition(reader, mask);
    CU_ASSERT_FATAL(readcond > 0);
    querycond = dds_create_querycondition(reader, mask, accept_all);
    CU_ASSERT_FATAL(querycond > 0);
    publisher2 = dds_create_publisher(participant, NULL, NULL);
    CU_ASSERT_FATAL(publisher2 > 0);
    subscriber2 = dds_create_subscriber(participant, NULL, NULL);
    CU_ASSERT_FATAL(subscriber2 > 0);
    writer2 = dds_create_writer(publisher2, topic, NULL, NULL);
    CU_ASSERT_FATAL(writer2 > 0);
    reader2 = dds_create_reader(subscriber2, topic, NULL, NULL);
    CU_ASSERT_FATAL(reader2 > 0);
    waitset = dds_create_waitset(participant);
    CU_ASSERT_FATAL(waitset > 0);
    result = dds_waitset_attach(waitset, subscriber2, subscriber2);
    CU_ASSERT_EQUAL_FATAL(result, DDS_RETCODE_OK)
    result = dds_waitset_attach(waitset, reader2, reader2);
    CU_ASSERT_EQUAL_FATAL(result, DDS_RETCODE_OK);
    result = dds_waitset_attach(waitset, publisher2, publisher2);
    CU_ASSERT_EQUAL_FATAL(result, DDS_RETCODE_OK)
    result = dds_waitset_attach(waitset, writer2, writer2);
    CU_ASSERT_EQUAL_FATAL(result, DDS_RETCODE_OK);
}

static void
teardown(void)
{
    dds_delete(reader);
    dds_delete(reader2);
    dds_delete(writer);
    dds_delete(writer2);
    dds_delete(publisher);
    dds_delete(subscriber);
    dds_delete(subscriber2);
    dds_delete(topic);
    dds_delete(waitset);
    dds_delete(participant);
}

CU_Test(ddsc_Contains, bad_parent)
{
    dds_entity_t participant, publisher;
    dds_entity_t entity;
    participant = dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);
    CU_ASSERT_FATAL (participant > 0);
    publisher = dds_create_publisher (participant, NULL, NULL);
    CU_ASSERT_FATAL (publisher > 0);

    entity = dds_contains(-DDS_RETCODE_ERROR, publisher);
    CU_ASSERT(entity < 0);
    CU_ASSERT_EQUAL(dds_err_nr(entity), DDS_RETCODE_BAD_PARAMETER);
    entity = dds_contains(DDS_ENTITY_NIL, publisher);
    CU_ASSERT(entity < 0);
    CU_ASSERT_EQUAL(dds_err_nr(entity), DDS_RETCODE_BAD_PARAMETER);

    dds_delete(publisher);
    dds_delete(participant);
}

CU_Test(ddsc_Contains, bad_child, .init=setup, .fini= teardown)
{
    dds_entity_t participant, publisher;
    dds_entity_t entity;
    participant = dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);
    CU_ASSERT_FATAL (participant > 0);
    publisher = dds_create_publisher (participant, NULL, NULL);
    CU_ASSERT_FATAL (publisher > 0);

    entity = dds_contains (participant, -DDS_RETCODE_ERROR);
    CU_ASSERT(entity < 0);
    CU_ASSERT_EQUAL (dds_err_nr(entity), DDS_RETCODE_BAD_PARAMETER);
    entity = dds_contains (participant, DDS_ENTITY_NIL);
    CU_ASSERT (entity < 0);
    CU_ASSERT_EQUAL (dds_err_nr(entity), DDS_RETCODE_BAD_PARAMETER);
    entity = dds_contains (participant, participant);
    CU_ASSERT (entity < 0);
    CU_ASSERT_EQUAL (dds_err_nr(entity), DDS_RETCODE_ILLEGAL_OPERATION);
}

CU_Test(ddsc_Contains, deleted_parent)
{
    dds_entity_t entity;
    dds_entity_t participant, publisher;
    participant = dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);
    CU_ASSERT_FATAL (participant > 0);
    publisher = dds_create_publisher(participant, NULL, NULL);
    CU_ASSERT_FATAL (participant > 0);

    dds_delete (participant);

    entity = dds_contains (participant, publisher);
    CU_ASSERT_FATAL (entity < 0);
    CU_ASSERT_EQUAL_FATAL (dds_err_nr (entity), DDS_RETCODE_BAD_PARAMETER);

    dds_delete (publisher);
}

CU_Test(ddsc_Contains, deleted_child)
{
    dds_entity_t entity;
    dds_entity_t participant, publisher;
    participant = dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);
    CU_ASSERT_FATAL (participant > 0);
    publisher = dds_create_publisher(participant, NULL, NULL);
    CU_ASSERT_FATAL (participant > 0);

    dds_delete (publisher);

    entity = dds_contains (participant, publisher);
    CU_ASSERT_FATAL (entity < 0);
    CU_ASSERT_EQUAL_FATAL (dds_err_nr (entity), DDS_RETCODE_ALREADY_DELETED);

    dds_delete (participant);
}

CU_TheoryDataPoints(ddsc_Contains, illegal_condition_combinations) = {
    CU_DataPoints(dds_entity_t*, &readcond, &readcond, &querycond, &querycond, &readcond, &readcond, &readcond, &readcond, &readcond, &readcond, &querycond, &querycond, &querycond, &querycond, &querycond, &querycond, &publisher, &subscriber, &publisher, &subscriber, &reader, &writer, &subscriber, &publisher, &topic, &participant,),
    CU_DataPoints(dds_entity_t*, &readcond, &querycond, &querycond, &readcond, &reader, &writer, &subscriber, &publisher, &topic, &participant, &reader, &writer, &subscriber, &publisher, &topic, &participant, &reader, &writer, &reader2, &writer2, &participant, &participant, &participant, &participant, &participant, &participant)
};
CU_Theory((dds_entity_t *parent_entity, dds_entity_t *child_entity), ddsc_Contains, illegal_condition_combinations, .init=setup, .fini=teardown)
{
    dds_entity_t res_entity = dds_contains(*parent_entity, *child_entity);
    CU_ASSERT_EQUAL_FATAL(dds_err_nr(res_entity), DDS_RETCODE_ILLEGAL_OPERATION);
}

CU_TheoryDataPoints(ddsc_Contains, basic_cases) = {
    CU_DataPoints(dds_entity_t*, &participant, &participant, &participant, &participant, &participant, &participant, &subscriber, &subscriber, &subscriber, &publisher, &waitset, &waitset, &waitset, &waitset),
    CU_DataPoints(dds_entity_t*, &subscriber, &publisher, &writer, &reader, &readcond, &querycond, &reader, &readcond, &querycond, &writer, &reader2, &writer2, &subscriber2, &publisher2)
};
CU_Theory((dds_entity_t *parent_entity, dds_entity_t *child_entity), ddsc_Contains, basic_cases, .init=setup, .fini= teardown)
{
    dds_entity_t res_entity = dds_contains(*parent_entity, *child_entity);
    CU_ASSERT_EQUAL_FATAL(res_entity, *child_entity);
}

CU_TheoryDataPoints(ddsc_Contains, wrong_cases) = {
    CU_DataPoints(dds_entity_t*, &waitset, &waitset, &waitset, &waitset, &subscriber, &subscriber2, &publisher2, &publisher, &subscriber2, &reader2),
    CU_DataPoints(dds_entity_t*, &reader, &writer, &publisher, &subscriber, &reader2, &reader, &writer, &writer2, &readcond, &readcond)
};
CU_Theory((dds_entity_t *parent_entity, dds_entity_t *child_entity), ddsc_Contains, wrong_cases, .init=setup, .fini= teardown)
{
    dds_entity_t res_entity = dds_contains(*parent_entity, *child_entity);
    CU_ASSERT_EQUAL_FATAL(res_entity, DDS_ENTITY_NIL);
}
