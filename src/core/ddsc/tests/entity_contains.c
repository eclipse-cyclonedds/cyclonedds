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
#include "CUnit/Theory.h"

#include "RoundTrip.h"

CU_Test(ddsc_contains, bad_parent)
{
    dds_entity_t participant, publisher;
    dds_entity_t entity;
    participant = dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);
    CU_ASSERT_FATAL(participant > 0);
    publisher = dds_create_publisher(participant, NULL, NULL);
    CU_ASSERT_FATAL(publisher > 0);

    entity = dds_contains(-DDS_RETCODE_ERROR, publisher);
    CU_ASSERT(entity < 0);
    CU_ASSERT_EQUAL(dds_err_nr(entity), DDS_RETCODE_BAD_PARAMETER);
    entity = dds_contains(DDS_ENTITY_NIL, publisher);
    CU_ASSERT(entity < 0);
    CU_ASSERT_EQUAL(dds_err_nr(entity), DDS_RETCODE_BAD_PARAMETER);

    dds_delete(publisher);
    dds_delete(participant);
}

CU_Test(ddsc_contains, bad_child)
{
    dds_entity_t participant, publisher;
    dds_entity_t entity;
    participant = dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);
    CU_ASSERT_FATAL(participant > 0);
    publisher = dds_create_publisher(participant, NULL, NULL);
    CU_ASSERT_FATAL(publisher > 0);

    entity = dds_contains(participant, -DDS_RETCODE_ERROR);
    CU_ASSERT(entity < 0);
    CU_ASSERT_EQUAL(dds_err_nr(entity), DDS_RETCODE_BAD_PARAMETER);
    entity = dds_contains(participant, DDS_ENTITY_NIL);
    CU_ASSERT (entity < 0);
    CU_ASSERT_EQUAL(dds_err_nr(entity), DDS_RETCODE_BAD_PARAMETER);
    entity = dds_contains(participant, participant);
    CU_ASSERT (entity < 0);
    CU_ASSERT_EQUAL(dds_err_nr(entity), DDS_RETCODE_ILLEGAL_OPERATION);
}

CU_Test(ddsc_contains, already_deleted)
{
    dds_entity_t entity, participant, publisher, subscriber, waitset;
    dds_return_t result;

    participant = dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);
    CU_ASSERT_FATAL(participant > 0);
    publisher = dds_create_publisher(participant, NULL, NULL);
    CU_ASSERT_FATAL(publisher > 0);
    subscriber = dds_create_subscriber(participant, NULL, NULL);
    CU_ASSERT_FATAL(subscriber > 0);
    waitset = dds_create_waitset(participant);
    CU_ASSERT_FATAL(waitset > 0);

    /* Test with a deleted child entity first to verify already deleted is
       returned when only the child entity is deleted. */
    result = dds_delete(publisher);
    CU_ASSERT_EQUAL_FATAL(result, DDS_RETCODE_OK);

    entity = dds_contains(participant, publisher);
    CU_ASSERT(entity < 0);
    CU_ASSERT_EQUAL(dds_err_nr(entity), DDS_RETCODE_ALREADY_DELETED);

    /* Test with a deleted parent entity, but not a direct parent, to verify
       already deleted is returned when the parent entity is deleted. */
    result = dds_delete(waitset);
    CU_ASSERT_EQUAL_FATAL(result, DDS_RETCODE_OK);

    entity = dds_contains(waitset, subscriber);
    CU_ASSERT(entity < 0);
    CU_ASSERT_EQUAL(dds_err_nr(entity), DDS_RETCODE_ALREADY_DELETED);

    dds_delete(subscriber);
    dds_delete(participant);
}

/* Dummy query condition callback. */
static bool
accept_all(const void * sample)
{
    (void)sample;
    return true;
}

static uint32_t mask = DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE;

static dds_entity_t par = DDS_ENTITY_NIL;
static dds_entity_t top = DDS_ENTITY_NIL;
static dds_entity_t pub = DDS_ENTITY_NIL;
static dds_entity_t pub2 = DDS_ENTITY_NIL;
static dds_entity_t wtr = DDS_ENTITY_NIL;
static dds_entity_t wtr2 = DDS_ENTITY_NIL;
static dds_entity_t sub = DDS_ENTITY_NIL;
static dds_entity_t sub2 = DDS_ENTITY_NIL;
static dds_entity_t rdr = DDS_ENTITY_NIL;
static dds_entity_t rdr2 = DDS_ENTITY_NIL;
static dds_entity_t ws = DDS_ENTITY_NIL;
static dds_entity_t rc = DDS_ENTITY_NIL;
static dds_entity_t qc = DDS_ENTITY_NIL;

/**
 * The tree of types created will look like this.
 *
 *  - participant
 *     - publisher
 *        - writer
 *     - publisher2
 *        - writer2
 *     - subscriber
 *        - reader
 *           - read condition
 *           - query condition
 *     - subscriber2
 *        - reader2
 *     - waitset
 */

static void
setup(void)
{
    dds_return_t ret;

    par = dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);
    CU_ASSERT_FATAL(par > 0);
    top = dds_create_topic(par, &RoundTripModule_DataType_desc, "RoundTrip", NULL, NULL);
    CU_ASSERT_FATAL(top > 0);

    pub = dds_create_publisher(par, NULL, NULL);
    CU_ASSERT_FATAL(pub > 0);
    pub2 = dds_create_publisher(par, NULL, NULL);
    CU_ASSERT_FATAL(pub2 > 0);
    wtr = dds_create_writer(pub, top, NULL, NULL);
    CU_ASSERT_FATAL(wtr > 0);
    wtr2 = dds_create_writer(pub2, top, NULL, NULL);
    CU_ASSERT_FATAL(wtr2 > 0);

    sub = dds_create_subscriber(par, NULL, NULL);
    CU_ASSERT_FATAL(sub > 0);
    sub2 = dds_create_subscriber(par, NULL, NULL);
    CU_ASSERT_FATAL(sub2 > 0);
    rdr = dds_create_reader(sub, top, NULL, NULL);
    CU_ASSERT_FATAL(rdr > 0);
    rdr2 = dds_create_reader(sub2, top, NULL, NULL);
    CU_ASSERT_FATAL(rdr2 > 0);
    rc = dds_create_readcondition(rdr, mask);
    CU_ASSERT_FATAL(rc > 0);
    qc = dds_create_querycondition(rdr, mask, accept_all);
    CU_ASSERT_FATAL(qc > 0);

    ws = dds_create_waitset(par);
    CU_ASSERT_FATAL(ws > 0);
    ret = dds_waitset_attach(ws, pub, pub);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    ret = dds_waitset_attach(ws, wtr, wtr);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    ret = dds_waitset_attach(ws, sub, sub);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    ret = dds_waitset_attach(ws, rdr, rdr);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    ret = dds_waitset_attach(ws, rc, rc);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    ret = dds_waitset_attach(ws, qc, qc);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
}

static void
teardown(void)
{
    dds_waitset_detach(ws, qc);
    dds_waitset_detach(ws, rc);
    dds_waitset_detach(ws, rdr);
    dds_waitset_detach(ws, sub);
    dds_waitset_detach(ws, wtr);
    dds_waitset_detach(ws, pub);
    dds_delete(par);
}

/* Verify the function returns illegal operation for all invalid combinations
   of parent and child entity. */
CU_TheoryDataPoints(ddsc_contains, bad_combos) = {
    CU_DataPoints(dds_entity_t*, &rc,  &qc,   &qc,   &rc,  &wtr, &pub,
                                 &rdr, &rdr2, &wtr, &pub,
                                 &sub, &sub2, &pub,
                                 &wtr, &wtr2, &rdr, &sub,
                                 &pub, &pub2, &sub),
    CU_DataPoints(dds_entity_t*, &rc,  &rc,   &qc,   &qc,  &rc,   &rc,
                                 &rdr, &rdr,  &rdr, &rdr,
                                 &sub, &sub,  &sub,
                                 &wtr, &wtr,  &wtr, &wtr,
                                 &pub, &pub,  &pub)
};

CU_Theory((dds_entity_t *parent, dds_entity_t *entity), ddsc_contains, bad_combos, .init=setup, .fini=teardown)
{
    dds_entity_t result = dds_contains(*parent, *entity);
    CU_ASSERT(result < 0);
    CU_ASSERT_EQUAL(dds_err_nr(result), DDS_RETCODE_ILLEGAL_OPERATION);
}

/* Verify an entity can be found throughout the entire tree. e.g. a read
   condition can be found in the reader, subscriber and participant. */
CU_TheoryDataPoints(ddsc_contains, happy_days) = {
    CU_DataPoints(dds_entity_t*, &rdr, &sub, &par, &ws,
                                 &sub, &par,
                                 &par,
                                 &pub, &par,
                                 &par),
    CU_DataPoints(dds_entity_t*, &rc,  &rc,  &rc, &rc,
                                 &rdr, &rdr,
                                 &sub,
                                 &wtr, &wtr,
                                 &pub)
};

CU_Theory((dds_entity_t *parent, dds_entity_t *entity), ddsc_contains, happy_days, .init=setup, .fini= teardown)
{
    dds_entity_t result = dds_contains(*parent, *entity);
    CU_ASSERT_EQUAL(result, *entity);
}

/* Verify that when an entity cannot be found DDS_ENTITY_NIL is returned when
   the input entities and their combination is valid. */
CU_TheoryDataPoints(ddsc_contains, unhappy_days) = {
    CU_DataPoints(dds_entity_t*, &rdr2, &rdr2, &sub2, &pub2, &ws),
    CU_DataPoints(dds_entity_t*, &rc,   &qc,   &rdr,  &wtr,  &rdr2)
};

CU_Theory((dds_entity_t *parent, dds_entity_t *entity), ddsc_contains, unhappy_days, .init=setup, .fini= teardown)
{
    dds_entity_t result = dds_contains(*parent, *entity);
    CU_ASSERT_EQUAL(result, DDS_ENTITY_NIL);
}
