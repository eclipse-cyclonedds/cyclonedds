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
#include "CUnit/Test.h"
#include "CUnit/Theory.h"
#include "RoundTrip.h"

/**************************************************************************************************
 *
 * Some thread related convenience stuff.
 *
 *************************************************************************************************/

typedef enum thread_state_t {
    STARTING,
    WAITING,
    STOPPED
} thread_state_t;

typedef struct thread_arg_t {
    os_threadId    tid;
    thread_state_t state;
    dds_entity_t   expected;
} thread_arg_t;

static void         waiting_thread_start(struct thread_arg_t *arg, dds_entity_t expected);
static dds_return_t waiting_thread_expect_exit(struct thread_arg_t *arg);




/**************************************************************************************************
 *
 * Test fixtures
 *
 *************************************************************************************************/

static dds_entity_t participant = 0;
static dds_entity_t topic       = 0;
static dds_entity_t writer      = 0;
static dds_entity_t reader      = 0;
static dds_entity_t waitset     = 0;
static dds_entity_t publisher   = 0;
static dds_entity_t subscriber  = 0;
static dds_entity_t readcond    = 0;


static char*
create_topic_name(const char *prefix, char *name, size_t size)
{
    /* Get semi random g_topic name. */
    os_procId pid = os_getpid();
    uintmax_t tid = os_threadIdToInteger(os_threadIdSelf());
    (void) snprintf(name, size, "%s_pid%"PRIprocId"_tid%"PRIuMAX"", prefix, pid, tid);
    return name;
}


static void
ddsc_waitset_basic_init(void)
{
    participant = dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);
    CU_ASSERT_FATAL(participant > 0);

    waitset = dds_create_waitset(participant);
    CU_ASSERT_FATAL(waitset > 0);
}

static void
ddsc_waitset_basic_fini(void)
{
    /* It shouldn't matter if any of these entities were deleted previously.
     * dds_delete will just return an error, which we don't check. */
    dds_delete(waitset);
    dds_delete(participant);
}

static void
ddsc_waitset_init(void)
{
    uint32_t mask = DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
    char name[100];

    os_osInit();

    ddsc_waitset_basic_init();

    publisher = dds_create_publisher(participant, NULL, NULL);
    CU_ASSERT_FATAL(publisher > 0);

    subscriber = dds_create_subscriber(participant, NULL, NULL);
    CU_ASSERT_FATAL(subscriber >  0);

    topic = dds_create_topic(participant, &RoundTripModule_DataType_desc, create_topic_name("ddsc_waitset_test", name, sizeof name), NULL, NULL);
    CU_ASSERT_FATAL(topic >  0);

    reader = dds_create_reader(subscriber, topic, NULL, NULL);
    CU_ASSERT_FATAL(reader >  0);

    writer = dds_create_writer(publisher, topic, NULL, NULL);
    CU_ASSERT_FATAL(writer >  0);

    readcond = dds_create_readcondition(reader, mask);
    CU_ASSERT_FATAL(readcond >  0);
}

static void
ddsc_waitset_fini(void)
{
    /* It shouldn't matter if any of these entities were deleted previously.
     * dds_delete will just return an error, which we don't check. */
    dds_delete(readcond);
    dds_delete(writer);
    dds_delete(reader);
    dds_delete(topic);
    dds_delete(publisher);
    dds_delete(subscriber);
    ddsc_waitset_basic_fini();
    os_osExit();
}

static void
ddsc_waitset_attached_init(void)
{
    dds_return_t ret;

    ddsc_waitset_init();

    /* Start with interest in nothing. */
    ret = dds_set_status_mask(participant, 0);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    ret = dds_set_status_mask(writer, 0);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    ret = dds_set_status_mask(reader, 0);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    ret = dds_set_status_mask(topic, 0);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    ret = dds_set_status_mask(publisher, 0);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    ret = dds_set_status_mask(subscriber, 0);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    /* Attach all entities to the waitset. */
    ret = dds_waitset_attach(waitset, participant, participant);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    ret = dds_waitset_attach(waitset, waitset, waitset);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    ret = dds_waitset_attach(waitset, writer, writer);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    ret = dds_waitset_attach(waitset, reader, reader);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    ret = dds_waitset_attach(waitset, topic, topic);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    ret = dds_waitset_attach(waitset, publisher, publisher);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    ret = dds_waitset_attach(waitset, subscriber, subscriber);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

}

static void
ddsc_waitset_attached_fini(void)
{
    /* Detach all entities to the waitset. */
    dds_waitset_detach(waitset, participant);
    dds_waitset_detach(waitset, topic);
    dds_waitset_detach(waitset, publisher);
    dds_waitset_detach(waitset, subscriber);
    dds_waitset_detach(waitset, waitset);
    dds_waitset_detach(waitset, writer);
    dds_waitset_detach(waitset, reader);

    ddsc_waitset_fini();
}

#if 0
#else
/**************************************************************************************************
 *
 * These will check the waitset creation in various ways.
 *
 *************************************************************************************************/
/*************************************************************************************************/
CU_Test(ddsc_waitset_create, second, .init=ddsc_waitset_basic_init, .fini=ddsc_waitset_basic_fini)
{
    dds_entity_t ws;
    dds_return_t ret;

    /* Basically, ddsc_waitset_basic_init() already tested the creation of a waitset. But
     * just see if we can create a second one. */
    ws = dds_create_waitset(participant);
    CU_ASSERT_FATAL(ws > 0);

    /* Also, we should be able to delete this second one. */
    ret = dds_delete(ws);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    /* And, of course, be able to delete the first one (return code isn't checked in the test fixtures). */
    ret = dds_delete(waitset);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_waitset_create, deleted_participant, .init=ddsc_waitset_basic_init, .fini=ddsc_waitset_basic_fini)
{
    dds_entity_t ws;
    dds_entity_t deleted;
    deleted = dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);
    dds_delete(deleted);
    ws = dds_create_waitset(deleted);
    CU_ASSERT_EQUAL_FATAL(dds_err_nr(ws), DDS_RETCODE_ALREADY_DELETED);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_waitset_create, invalid_params) = {
        CU_DataPoints(dds_entity_t, -2, -1, 0, 1, 100, INT_MAX, INT_MIN),
};
CU_Theory((dds_entity_t par), ddsc_waitset_create, invalid_params, .init=ddsc_waitset_basic_init, .fini=ddsc_waitset_basic_fini)
{
    dds_entity_t exp = DDS_RETCODE_BAD_PARAMETER * -1;
    dds_entity_t ws;

    ws = dds_create_waitset(par);
    CU_ASSERT_EQUAL_FATAL(dds_err_nr(ws), dds_err_nr(exp));
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_waitset_create, non_participants) = {
        CU_DataPoints(dds_entity_t*, &topic, &writer, &reader, &waitset, &publisher, &subscriber, &readcond),
};
CU_Theory((dds_entity_t *par), ddsc_waitset_create, non_participants, .init=ddsc_waitset_init, .fini=ddsc_waitset_fini)
{
    dds_entity_t ws;
    ws = dds_create_waitset(*par);
    CU_ASSERT_EQUAL_FATAL(dds_err_nr(ws), DDS_RETCODE_ILLEGAL_OPERATION);
}
/*************************************************************************************************/



/**************************************************************************************************
 *
 * These will check the waitset attach in various invalid ways.
 *   - Valid waitset but try to attach all kinds of invalid entities.
 *   - Try to attach a valid participant to all kinds of invalid entities.
 *   - Try attaching valid entities to valid (non-waitset) entities
 *   - Try attaching a valid entity a second time.
 *
 *************************************************************************************************/
/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_waitset_attach, invalid_params) = {
        CU_DataPoints(dds_entity_t, -2, -1, 0, 1, 100, INT_MAX, INT_MIN),
        CU_DataPoints(dds_attach_t,  (dds_attach_t)NULL, (dds_attach_t)&reader, (dds_attach_t)3, (dds_attach_t)0, (dds_attach_t)0, (dds_attach_t)0, (dds_attach_t)0),
};
CU_Theory((dds_entity_t e, dds_attach_t a), ddsc_waitset_attach, invalid_params, .init=ddsc_waitset_basic_init, .fini=ddsc_waitset_basic_fini)
{
    dds_return_t ret;
    ret = dds_waitset_attach(waitset, e, a);
    CU_ASSERT_EQUAL_FATAL(dds_err_nr(ret), DDS_RETCODE_BAD_PARAMETER);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_waitset_attach, invalid_waitsets) = {
        CU_DataPoints(dds_entity_t, -2, -1, 0, 1, 100, INT_MAX, INT_MIN),
        CU_DataPoints(dds_attach_t,  (dds_attach_t)NULL, (dds_attach_t)&reader, (dds_attach_t)3, (dds_attach_t)0, (dds_attach_t)0, (dds_attach_t)0, (dds_attach_t)0),
};
CU_Theory((dds_entity_t ws, dds_attach_t a), ddsc_waitset_attach, invalid_waitsets, .init=ddsc_waitset_basic_init, .fini=ddsc_waitset_basic_fini)
{
    dds_return_t exp = DDS_RETCODE_BAD_PARAMETER * -1;
    dds_return_t ret;

    ret = dds_waitset_attach(ws, participant, a);
    CU_ASSERT_EQUAL_FATAL(dds_err_nr(ret), dds_err_nr(exp));
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_waitset_attach, non_waitsets) = {
        CU_DataPoints(dds_entity_t*, &participant, &topic, &writer, &reader,           &publisher, &subscriber, &readcond),
        CU_DataPoints(dds_entity_t*, &participant, &topic, &writer, &reader, &waitset, &publisher, &subscriber, &readcond),
        CU_DataPoints(dds_attach_t,  (dds_attach_t)NULL, (dds_attach_t)&reader, (dds_attach_t)3, (dds_attach_t)0, (dds_attach_t)0, (dds_attach_t)0, (dds_attach_t)0),
};
CU_Theory((dds_entity_t *ws, dds_entity_t *e, dds_attach_t a), ddsc_waitset_attach, non_waitsets, .init=ddsc_waitset_init, .fini=ddsc_waitset_fini)
{
    dds_return_t ret;
    ret = dds_waitset_attach(*ws, *e, a);
    CU_ASSERT_EQUAL_FATAL(dds_err_nr(ret), DDS_RETCODE_ILLEGAL_OPERATION);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_waitset_attach, deleted_waitset, .init=ddsc_waitset_basic_init, .fini=ddsc_waitset_basic_fini)
{
    dds_return_t ret;
    dds_delete(waitset);
    ret = dds_waitset_attach(waitset, participant, 0);
    CU_ASSERT_EQUAL_FATAL(dds_err_nr(ret), DDS_RETCODE_ALREADY_DELETED);
}
/*************************************************************************************************/



/**************************************************************************************************
 *
 * These will check the waitset attach and detach with valid entities (which should always work).
 *
 *************************************************************************************************/
/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_waitset_attach_detach, valid_entities) = {
        CU_DataPoints(dds_entity_t*, &participant, &topic, &writer, &reader, &waitset, &publisher, &subscriber, &readcond),
        CU_DataPoints(dds_entity_t*, &participant, &topic, &writer, &reader, &waitset, &publisher, &subscriber, &readcond),
        CU_DataPoints(dds_attach_t,  (dds_attach_t)NULL, (dds_attach_t)&reader, (dds_attach_t)3, (dds_attach_t)3, (dds_attach_t)3, (dds_attach_t)3, (dds_attach_t)3, (dds_attach_t)3),
};
CU_Theory((dds_entity_t *ws, dds_entity_t *e, dds_attach_t a), ddsc_waitset_attach_detach, valid_entities, .init=ddsc_waitset_init, .fini=ddsc_waitset_fini)
{
    dds_return_t exp;
    dds_return_t ret;

    if (*ws == waitset) {
        /* Attaching to the waitset should work. */
        exp = DDS_RETCODE_OK;
    } else {
        /* Attaching to every other entity should fail. */
        exp = DDS_RETCODE_ILLEGAL_OPERATION;
    }

    /* Try to attach. */
    ret = dds_waitset_attach(*ws, *e, a);
    CU_ASSERT_EQUAL_FATAL(dds_err_nr(ret), exp);

    /* Detach when needed. */
    if (ret == DDS_RETCODE_OK) {
        ret = dds_waitset_detach(*ws, *e);
        CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    }
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_waitset_attach_detach, second, .init=ddsc_waitset_basic_init, .fini=ddsc_waitset_basic_fini)
{
    dds_return_t ret;

    ret = dds_waitset_attach(waitset, waitset, 0);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    ret = dds_waitset_attach(waitset, waitset, 0);
    CU_ASSERT_EQUAL_FATAL(dds_err_nr(ret), DDS_RETCODE_PRECONDITION_NOT_MET);

    ret = dds_waitset_detach(waitset, waitset);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    ret = dds_waitset_detach(waitset, waitset);
    CU_ASSERT_EQUAL_FATAL(dds_err_nr(ret), DDS_RETCODE_PRECONDITION_NOT_MET);
}
/*************************************************************************************************/



/**************************************************************************************************
 *
 * These will check the waitset detach in various ways.
 *   - Valid waitset but try to detach all kinds of invalid entities.
 *   - Try to detach a valid participant from all kinds of invalid entities.
 *   - Try detach valid entities from valid entities
 *          -- detach to entities that are not waitsets.
 *          -- detach to a waitset.
 *
 * Note: everything is expected to fail for varous reasons. The 'happy day' detach is tested on
 *       the fly in ddsc_waitset_attach_detach::valid_entities
 *
 *************************************************************************************************/
/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_waitset_detach, invalid_params) = {
        CU_DataPoints(dds_entity_t, -2, -1, 0, 1, 100, INT_MAX, INT_MIN),
};
CU_Theory((dds_entity_t e), ddsc_waitset_detach, invalid_params, .init=ddsc_waitset_basic_init, .fini=ddsc_waitset_basic_fini)
{
    dds_return_t ret;
    ret = dds_waitset_detach(waitset, e);
    CU_ASSERT_EQUAL_FATAL(dds_err_nr(ret), DDS_RETCODE_BAD_PARAMETER);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_waitset_detach, invalid_waitsets) = {
        CU_DataPoints(dds_entity_t, -2, -1, 0, 1, 100, INT_MAX, INT_MIN),
};
CU_Theory((dds_entity_t ws), ddsc_waitset_detach, invalid_waitsets, .init=ddsc_waitset_basic_init, .fini=ddsc_waitset_basic_fini)
{
    dds_return_t exp = DDS_RETCODE_BAD_PARAMETER * -1;
    dds_return_t ret;

    ret = dds_waitset_detach(ws, participant);
    CU_ASSERT_EQUAL_FATAL(dds_err_nr(ret), dds_err_nr(exp));
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_waitset_detach, valid_entities) = {
        CU_DataPoints(dds_entity_t*, &participant, &topic, &writer, &reader, &waitset, &publisher, &subscriber, &readcond),
        CU_DataPoints(dds_entity_t*, &participant, &topic, &writer, &reader, &waitset, &publisher, &subscriber, &readcond),
};
CU_Theory((dds_entity_t *ws, dds_entity_t *e), ddsc_waitset_detach, valid_entities, .init=ddsc_waitset_init, .fini=ddsc_waitset_fini)
{
    dds_return_t exp;
    dds_return_t ret;

    if (*ws == waitset) {
        /* Detaching from an empty waitset should yield 'precondition not met'. */
        exp = DDS_RETCODE_PRECONDITION_NOT_MET;
    } else {
        /* Attaching to every other entity should yield 'illegal operation'. */
        exp = DDS_RETCODE_ILLEGAL_OPERATION;
    }

    ret = dds_waitset_detach(*ws, *e);
    CU_ASSERT_EQUAL_FATAL(dds_err_nr(ret), exp);
}
/*************************************************************************************************/




/**************************************************************************************************
 *
 * These will check the waitset attach/detach in various ways. We will use NULL as attach argument
 * because it will be properly used in 'waitset behaviour' tests anyway.
 *
 *************************************************************************************************/
/*************************************************************************************************/
CU_Test(ddsc_waitset_attach_detach, itself, .init=ddsc_waitset_basic_init, .fini=ddsc_waitset_basic_fini)
{
    dds_return_t ret;

    ret = dds_waitset_attach(waitset, waitset, 0);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    ret = dds_waitset_detach(waitset, waitset);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_waitset_attach_detach, participant, .init=ddsc_waitset_basic_init, .fini=ddsc_waitset_basic_fini)
{
    dds_return_t ret;

    ret = dds_waitset_attach(waitset, participant, 0);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    ret = dds_waitset_detach(waitset, participant);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_waitset_attach_detach, reader, .init=ddsc_waitset_init, .fini=ddsc_waitset_fini)
{
    dds_return_t ret;

    ret = dds_waitset_attach(waitset, reader, 0);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    ret = dds_waitset_detach(waitset, reader);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_waitset_attach_detach, readcondition, .init=ddsc_waitset_init, .fini=ddsc_waitset_fini)
{
    dds_return_t ret;

    ret = dds_waitset_attach(waitset, readcond, 0);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    ret = dds_waitset_detach(waitset, readcond);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
}
/*************************************************************************************************/




/**************************************************************************************************
 *
 * These will check entities can be deleted while attached to the waitset. We will use NULL as
 * attach argument because it will be properly used in 'waitset behaviour' tests anyway.
 * 1) Test if the waitset can be deleted when attached to itself.
 * 2) Test if the waitset parent can be deleted when attached.
 * 3) Test if an 'ordinary' entity can be deleted when attached.
 * 4) Test if an condition can be deleted when attached.
 *
 *************************************************************************************************/
/*************************************************************************************************/
CU_Test(ddsc_waitset_delete_attached, self, .init=ddsc_waitset_basic_init, .fini=ddsc_waitset_basic_fini)
{
    dds_return_t ret;

    ret = dds_waitset_attach(waitset, waitset, 0);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    ret = dds_delete(waitset);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_waitset_delete_attached, participant, .init=ddsc_waitset_basic_init, .fini=ddsc_waitset_basic_fini)
{
    dds_return_t ret;

    ret = dds_waitset_attach(waitset, participant, 0);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    ret = dds_delete(participant);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_waitset_delete_attached, reader, .init=ddsc_waitset_init, .fini=ddsc_waitset_fini)
{
    dds_return_t ret;

    ret = dds_waitset_attach(waitset, reader, 0);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    ret = dds_delete(reader);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_waitset_delete_attached, readcondition, .init=ddsc_waitset_init, .fini=ddsc_waitset_fini)
{
    dds_return_t ret;

    ret = dds_waitset_attach(waitset, readcond, 0);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    ret = dds_delete(readcond);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
}
/*************************************************************************************************/



/**************************************************************************************************
 *
 * These will check the waitset triggering in various invalid ways.
 * The happy days scenario is tested by ddsc_waitset_triggering::on_self.
 *
 *************************************************************************************************/
/*************************************************************************************************/
CU_Test(ddsc_waitset_set_trigger, deleted_waitset, .init=ddsc_waitset_basic_init, .fini=ddsc_waitset_basic_fini)
{
    dds_return_t ret;
    dds_delete(waitset);
    ret = dds_waitset_set_trigger(waitset, true);
    CU_ASSERT_EQUAL_FATAL(dds_err_nr(ret), DDS_RETCODE_ALREADY_DELETED);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_waitset_set_trigger, invalid_params) = {
        CU_DataPoints(dds_entity_t, -2, -1, 0, 1, 100, INT_MAX, INT_MIN),
};
CU_Theory((dds_entity_t ws), ddsc_waitset_set_trigger, invalid_params, .init=ddsc_waitset_basic_init, .fini=ddsc_waitset_basic_fini)
{
    dds_return_t exp = DDS_RETCODE_BAD_PARAMETER * -1;
    dds_return_t ret;

    ret = dds_waitset_set_trigger(ws, true);
    CU_ASSERT_EQUAL_FATAL(dds_err_nr(ret), dds_err_nr(exp));
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_waitset_set_trigger, non_waitsets) = {
        CU_DataPoints(dds_entity_t*, &participant, &topic, &writer, &reader, &publisher, &subscriber, &readcond),
};
CU_Theory((dds_entity_t *ws), ddsc_waitset_set_trigger, non_waitsets, .init=ddsc_waitset_init, .fini=ddsc_waitset_fini)
{
    dds_return_t ret;
    ret = dds_waitset_set_trigger(*ws, true);
    CU_ASSERT_EQUAL_FATAL(dds_err_nr(ret), DDS_RETCODE_ILLEGAL_OPERATION);
}
/*************************************************************************************************/



/**************************************************************************************************
 *
 * These will check the waitset wait in various invalid ways.
 * The happy days scenario is tested by ddsc_waitset_triggering::*.
 *
 *************************************************************************************************/
/*************************************************************************************************/
CU_Test(ddsc_waitset_wait, deleted_waitset, .init=ddsc_waitset_attached_init, .fini=ddsc_waitset_attached_fini)
{
    dds_attach_t triggered;
    dds_return_t ret;
    dds_delete(waitset);
    ret = dds_waitset_wait(waitset, &triggered, 1, DDS_SECS(1));
    CU_ASSERT_EQUAL_FATAL(dds_err_nr(ret), DDS_RETCODE_ALREADY_DELETED);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_waitset_wait, invalid_waitsets) = {
        CU_DataPoints(dds_entity_t, -2, -1, 0, 1, 100, INT_MAX, INT_MIN),
};
CU_Theory((dds_entity_t ws), ddsc_waitset_wait, invalid_waitsets, .init=ddsc_waitset_basic_init, .fini=ddsc_waitset_basic_fini)
{
    dds_attach_t triggered;
    dds_return_t exp = DDS_RETCODE_BAD_PARAMETER * -1;
    dds_return_t ret;

    ret = dds_waitset_wait(ws, &triggered, 1, DDS_SECS(1));
    CU_ASSERT_EQUAL_FATAL(dds_err_nr(ret), dds_err_nr(exp));
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_waitset_wait, non_waitsets) = {
        CU_DataPoints(dds_entity_t*, &participant, &topic, &writer, &reader, &publisher, &subscriber, &readcond),
};
CU_Theory((dds_entity_t *ws), ddsc_waitset_wait, non_waitsets, .init=ddsc_waitset_init, .fini=ddsc_waitset_fini)
{
    dds_attach_t triggered;
    dds_return_t ret;
    ret = dds_waitset_wait(*ws, &triggered, 1, DDS_SECS(1));
    CU_ASSERT_EQUAL_FATAL(dds_err_nr(ret), DDS_RETCODE_ILLEGAL_OPERATION);
}
/*************************************************************************************************/

/*************************************************************************************************/
static dds_attach_t attachment;
CU_TheoryDataPoints(ddsc_waitset_wait, invalid_params) = {
        CU_DataPoints(dds_attach_t *, &attachment, NULL),
        CU_DataPoints(size_t, 0, 1, 100),
        CU_DataPoints(int, -1, 0, 1),
};
CU_Theory((dds_attach_t *a, size_t size, int msec), ddsc_waitset_wait, invalid_params, .init=ddsc_waitset_basic_init, .fini=ddsc_waitset_basic_fini)
{
    dds_return_t ret;

    /* Only test when the combination of parameters is actually invalid. */
    CU_ASSERT_FATAL(((a == NULL) && (size != 0)) || ((a != NULL) && (size == 0)) || (msec < 0));

    ret = dds_waitset_wait(waitset, a, size, DDS_MSECS(msec));
    CU_ASSERT_EQUAL_FATAL(dds_err_nr(ret), DDS_RETCODE_BAD_PARAMETER);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_waitset_wait_until, deleted_waitset, .init=ddsc_waitset_attached_init, .fini=ddsc_waitset_attached_fini)
{
    dds_attach_t triggered;
    dds_return_t ret;
    dds_delete(waitset);
    ret = dds_waitset_wait_until(waitset, &triggered, 1, dds_time());
    CU_ASSERT_EQUAL_FATAL(dds_err_nr(ret), DDS_RETCODE_ALREADY_DELETED);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_waitset_wait_until, invalid_waitsets) = {
        CU_DataPoints(dds_entity_t, -2, -1, 0, 1, 100, INT_MAX, INT_MIN),
};
CU_Theory((dds_entity_t ws), ddsc_waitset_wait_until, invalid_waitsets, .init=ddsc_waitset_basic_init, .fini=ddsc_waitset_basic_fini)
{
    dds_attach_t triggered;
    dds_return_t exp = DDS_RETCODE_BAD_PARAMETER * -1;
    dds_return_t ret;

    ret = dds_waitset_wait_until(ws, &triggered, 1, dds_time());
    CU_ASSERT_EQUAL_FATAL(dds_err_nr(ret), dds_err_nr(exp));
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_waitset_wait_until, non_waitsets) = {
        CU_DataPoints(dds_entity_t*, &participant, &topic, &writer, &reader, &publisher, &subscriber, &readcond),
};
CU_Theory((dds_entity_t *ws), ddsc_waitset_wait_until, non_waitsets, .init=ddsc_waitset_init, .fini=ddsc_waitset_fini)
{
    dds_attach_t triggered;
    dds_return_t ret;
    ret = dds_waitset_wait_until(*ws, &triggered, 1, dds_time());
    CU_ASSERT_EQUAL_FATAL(dds_err_nr(ret), DDS_RETCODE_ILLEGAL_OPERATION);
}
/*************************************************************************************************/

/*************************************************************************************************/
static dds_attach_t attachment;
CU_TheoryDataPoints(ddsc_waitset_wait_until, invalid_params) = {
        CU_DataPoints(dds_attach_t *, &attachment, NULL),
        CU_DataPoints(size_t, 0, 1, 100)
};
CU_Theory((dds_attach_t *a, size_t size), ddsc_waitset_wait_until, invalid_params, .init=ddsc_waitset_basic_init, .fini=ddsc_waitset_basic_fini)
{
    dds_return_t ret;

    /* Only test when the combination of parameters is actually invalid. */
    CU_ASSERT_FATAL(((a == NULL) && (size != 0)) || ((a != NULL) && (size == 0)));

    ret = dds_waitset_wait_until(waitset, a, size, dds_time());
    CU_ASSERT_EQUAL_FATAL(dds_err_nr(ret), DDS_RETCODE_BAD_PARAMETER);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_waitset_wait_until, past, .init=ddsc_waitset_attached_init, .fini=ddsc_waitset_attached_fini)
{
    dds_attach_t triggered;
    dds_return_t ret;

    ret = dds_waitset_wait_until(waitset, &triggered, 1, dds_time() - 100000);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
}
/*************************************************************************************************/



/**************************************************************************************************
 *
 * These will check the waitset getting the attached entities.
 *
 *************************************************************************************************/
#define MAX_ENTITIES_CNT    (10)
#define FOUND_PARTICIPANT   (0x0001)
#define FOUND_PUBLISHER     (0x0002)
#define FOUND_SUBSCRIBER    (0x0004)
#define FOUND_WAITSET       (0x0008)
#define FOUND_TOPIC         (0x0010)
#define FOUND_READER        (0x0020)
#define FOUND_WRITER        (0x0040)
#define FOUND_ALL           (0x007F)

static uint32_t NumberOfSetBits(uint32_t i)
{
    /* https://stackoverflow.com/questions/109023/how-to-count-the-number-of-set-bits-in-a-32-bit-integer */
    // Java: use >>> instead of >>
    // C or C++: use uint32_t
    i = i - ((i >> 1) & 0x55555555);
    i = (i & 0x33333333) + ((i >> 2) & 0x33333333);
    return (((i + (i >> 4)) & 0x0F0F0F0F) * 0x01010101) >> 24;
}

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_waitset_get_entities, array_sizes) = {
        CU_DataPoints(size_t, 0, 1, 7, MAX_ENTITIES_CNT),
};
CU_Theory((size_t size), ddsc_waitset_get_entities, array_sizes, .init=ddsc_waitset_attached_init, .fini=ddsc_waitset_attached_fini)
{
    uint32_t found = 0;
    dds_return_t i;
    dds_return_t ret;
    dds_entity_t entities[MAX_ENTITIES_CNT];

    /* Make sure at least one entity is in the waitsets' internal triggered list. */
    ret = dds_waitset_set_trigger(waitset, true);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    /* Get the actual attached entities. */
    ret = dds_waitset_get_entities(waitset, entities, size);

    /* ddsc_waitset_attached_init() attached 7 entities. */
    CU_ASSERT_EQUAL_FATAL(ret, 7);

    /* Found entities should be only present once. */
    ret = ((dds_return_t)size < ret) ? (dds_return_t)size : ret;
    for (i = 0; i < ret; i++) {
        if (entities[i] == participant) {
            CU_ASSERT_FATAL(!(found & FOUND_PARTICIPANT));
            found |= FOUND_PARTICIPANT;
        } else if (entities[i] == publisher) {
            CU_ASSERT_FATAL(!(found & FOUND_PUBLISHER));
            found |= FOUND_PUBLISHER;
        } else if (entities[i] == subscriber) {
            CU_ASSERT_FATAL(!(found & FOUND_SUBSCRIBER));
            found |= FOUND_SUBSCRIBER;
        } else if (entities[i] == waitset) {
            CU_ASSERT_FATAL(!(found & FOUND_WAITSET));
            found |= FOUND_WAITSET;
        } else if (entities[i] == topic) {
            CU_ASSERT_FATAL(!(found & FOUND_TOPIC));
            found |= FOUND_TOPIC;
        } else if (entities[i] == reader) {
            CU_ASSERT_FATAL(!(found & FOUND_READER));
            found |= FOUND_READER;
        } else if (entities[i] == writer) {
            CU_ASSERT_FATAL(!(found & FOUND_WRITER));
            found |= FOUND_WRITER;
        }
    }

    /* Every found entity should be a known one. */
    CU_ASSERT_EQUAL_FATAL((dds_return_t)NumberOfSetBits(found), ret);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_waitset_get_entities, no_array, .init=ddsc_waitset_attached_init, .fini=ddsc_waitset_attached_fini)
{
    dds_return_t ret;
    OS_WARNING_MSVC_OFF(6387); /* Disable SAL warning on intentional misuse of the API */
    ret = dds_waitset_get_entities(waitset, NULL, 1);
    OS_WARNING_MSVC_ON(6387);
    /* ddsc_waitset_attached_init attached 7 entities. */
    CU_ASSERT_EQUAL_FATAL(ret, 7);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_waitset_get_entities, deleted_waitset, .init=ddsc_waitset_attached_init, .fini=ddsc_waitset_attached_fini)
{
    dds_return_t ret;
    dds_entity_t entities[MAX_ENTITIES_CNT];
    dds_delete(waitset);
    ret = dds_waitset_get_entities(waitset, entities, MAX_ENTITIES_CNT);
    CU_ASSERT_EQUAL_FATAL(dds_err_nr(ret), DDS_RETCODE_ALREADY_DELETED);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_waitset_get_entities, invalid_params) = {
        CU_DataPoints(dds_entity_t, -2, -1, 0, 1, 100, INT_MAX, INT_MIN),
};
CU_Theory((dds_entity_t ws), ddsc_waitset_get_entities, invalid_params, .init=ddsc_waitset_attached_init, .fini=ddsc_waitset_attached_fini)
{
    dds_return_t exp = DDS_RETCODE_BAD_PARAMETER * -1;
    dds_entity_t entities[MAX_ENTITIES_CNT];
    dds_return_t ret;

    ret = dds_waitset_get_entities(ws, entities, MAX_ENTITIES_CNT);
    CU_ASSERT_EQUAL_FATAL(dds_err_nr(ret), dds_err_nr(exp));
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_waitset_get_entities, non_waitsets) = {
        CU_DataPoints(dds_entity_t*, &participant, &topic, &writer, &reader, &publisher, &subscriber, &readcond),
};
CU_Theory((dds_entity_t *ws), ddsc_waitset_get_entities, non_waitsets, .init=ddsc_waitset_attached_init, .fini=ddsc_waitset_attached_fini)
{
    dds_entity_t entities[MAX_ENTITIES_CNT];
    dds_return_t ret;
    ret = dds_waitset_get_entities(*ws, entities, MAX_ENTITIES_CNT);
    CU_ASSERT_EQUAL_FATAL(dds_err_nr(ret), DDS_RETCODE_ILLEGAL_OPERATION);
}
/*************************************************************************************************/





/**************************************************************************************************
 *
 * This will check if waitset will wake up when it is attached to itself and triggered.
 *
 * In short:
 * 1) Attach the waitset to itself
 * 2) A different thread will call dds_waitset_wait. This should block because the waitset
 *    hasn't been triggered yet. We also want it to block to know for sure that it'll wake up.
 * 3) Trigger the waitset. This should unblock the other thread that was waiting on the waitset.
 * 4) A new dds_waitset_wait should return immediately because the trigger value hasn't been
 *    reset (dds_waitset_set_trigger(waitset, false)) after the waitset woke up.
 *
 *************************************************************************************************/
/*************************************************************************************************/
CU_Test(ddsc_waitset_triggering, on_self, .init=ddsc_waitset_attached_init, .fini=ddsc_waitset_attached_fini)
{
    dds_attach_t triggered;
    thread_arg_t arg;
    dds_return_t ret;

    /* The waitset should not have been triggered. */
    ret = dds_triggered(waitset);
    CU_ASSERT_EQUAL_FATAL(ret, 0);

    /* Start a thread that'll wait because no entity attached to the waitset
     * has been triggered (for instance by a status change). */
    waiting_thread_start(&arg, waitset);

    /* Triggering of the waitset should unblock the thread. */
    ret = dds_waitset_set_trigger(waitset, true);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    ret = waiting_thread_expect_exit(&arg);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    /* Now the trigger state should be true. */
    ret = dds_triggered(waitset);
    CU_ASSERT_FATAL(ret > 0);

    /* Waitset shouldn't wait, but immediately return our waitset. */
    ret = dds_waitset_wait(waitset, &triggered, 1, DDS_SECS(1));
    CU_ASSERT_EQUAL_FATAL(ret, 1);
    CU_ASSERT_EQUAL_FATAL(waitset, (dds_entity_t)(intptr_t)triggered);

    /* Reset waitset trigger. */
    ret = dds_waitset_set_trigger(waitset, false);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    ret = dds_triggered(waitset);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
}
/*************************************************************************************************/




/**************************************************************************************************
 *
 * This will check if waitset will wake up when data is written related to an attached reader.
 *
 * In short:
 * 1) Attach the reader to the waitset
 * 2) A different thread will call dds_waitset_wait. This should block because the reader
 *    hasn't got data yet. We also want it to block to know for sure that it'll wake up.
 * 3) Write data. This should unblock the other thread that was waiting on the waitset.
 * 4) A new dds_waitset_wait should return immediately because the status of the reader hasn't
 *    changed after the first trigger (it didn't read the data).
 *
 *************************************************************************************************/
/*************************************************************************************************/
CU_Test(ddsc_waitset_triggering, on_reader, .init=ddsc_waitset_attached_init, .fini=ddsc_waitset_attached_fini)
{
    RoundTripModule_DataType sample;
    dds_attach_t triggered;
    thread_arg_t arg;
    dds_return_t ret;

    memset(&sample, 0, sizeof(RoundTripModule_DataType));

    /* Only interested in data_available for this test. */
    ret = dds_set_status_mask(reader, DDS_DATA_AVAILABLE_STATUS);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    /* The reader should not have been triggered. */
    ret = dds_triggered(reader);
    CU_ASSERT_EQUAL_FATAL(ret, 0);

    /* Start a thread that'll wait because no entity attached to the waitset
     * has been triggered (for instance by a status change). */
    waiting_thread_start(&arg, reader);

    /* Writing data should unblock the thread. */
    ret = dds_write(writer, &sample);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    ret = waiting_thread_expect_exit(&arg);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    /* Now the trigger state should be true. */
    ret = dds_triggered(reader);
    CU_ASSERT_FATAL(ret >  0);

    /* Waitset shouldn't wait, but immediately return our reader. */
    ret = dds_waitset_wait(waitset, &triggered, 1, DDS_SECS(1));
    CU_ASSERT_EQUAL_FATAL(ret, 1);
    CU_ASSERT_EQUAL_FATAL(reader, (dds_entity_t)(intptr_t)triggered);
}
/*************************************************************************************************/




/**************************************************************************************************
 *
 * This will check if waitset will wake up when data is written related to an attached condition.
 *
 * In short:
 * 1) Attach the readcondition to the waitset
 * 2) A different thread will call dds_waitset_wait. This should block because the related reader
 *    hasn't got data yet. We also want it to block to know for sure that it'll wake up.
 * 3) Write data. This should unblock the other thread that was waiting on the readcondition.
 * 4) A new dds_waitset_wait should return immediately because the status of the related reader
 *    (and thus the readcondition) hasn't changed after the first trigger (it didn't read the data).
 *
 *************************************************************************************************/
/*************************************************************************************************/
CU_Test(ddsc_waitset_triggering, on_readcondition, .init=ddsc_waitset_attached_init, .fini=ddsc_waitset_attached_fini)
{
    RoundTripModule_DataType sample;
    dds_attach_t triggered;
    thread_arg_t arg;
    dds_return_t ret;

    memset(&sample, 0, sizeof(RoundTripModule_DataType));

    /* Make sure that we start un-triggered. */
    ret = dds_triggered(readcond);
    CU_ASSERT_EQUAL_FATAL(ret, 0);

    /* Attach condition to the waitset. */
    ret = dds_waitset_attach(waitset, readcond, readcond);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    /* Start a thread that'll wait because no entity attached to the waitset
     * has been triggered (for instance by a status change). */
    waiting_thread_start(&arg, readcond);

    /* Writing data should unblock the thread. */
    ret = dds_write(writer, &sample);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    ret = waiting_thread_expect_exit(&arg);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    /* Now the trigger state should be true. */
    ret = dds_triggered(readcond);
    CU_ASSERT(ret > 0);

    /* Waitset shouldn't wait, but immediately return our reader. */
    ret = dds_waitset_wait(waitset, &triggered, 1, DDS_SECS(1));
    CU_ASSERT_EQUAL_FATAL(ret, 1);
    CU_ASSERT_EQUAL_FATAL(readcond, (dds_entity_t)(intptr_t)triggered);

    /* Detach condition. */
    ret = dds_waitset_detach(waitset, readcond);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
}
/*************************************************************************************************/





#endif

/**************************************************************************************************
 *
 * Convenience support functions.
 *
 *************************************************************************************************/

static uint32_t
waiting_thread(void *a)
{
    thread_arg_t *arg = (thread_arg_t*)a;
    dds_attach_t triggered;
    dds_return_t ret;

    arg->state = WAITING;
    /* This should block until the main test released all claims. */
    ret = dds_waitset_wait(waitset, &triggered, 1, DDS_SECS(1000));
    CU_ASSERT_EQUAL_FATAL(ret, 1);
    CU_ASSERT_EQUAL_FATAL(arg->expected, (dds_entity_t)(intptr_t)triggered);
    arg->state = STOPPED;

    return 0;
}

static os_result
thread_reached_state(thread_state_t *actual, thread_state_t expected, int32_t msec)
{
    /* Convenience function. */
    os_time msec10 = { 0, 10000000 };
    while ((msec > 0) && (*actual != expected)) {
        os_nanoSleep(msec10);
        msec -= 10;
    }
    return (*actual == expected) ? os_resultSuccess : os_resultTimeout;
}

static void
waiting_thread_start(struct thread_arg_t *arg, dds_entity_t expected)
{
    os_threadId   thread_id;
    os_threadAttr thread_attr;
    os_result     osr;

    assert(arg);

    /* Create an other thread that will blocking wait on the waitset. */
    arg->expected = expected;
    arg->state   = STARTING;
    os_threadAttrInit(&thread_attr);
    osr = os_threadCreate(&thread_id, "waiting_thread", &thread_attr, waiting_thread, arg);
    CU_ASSERT_EQUAL_FATAL(osr, os_resultSuccess);

    /* The thread should reach 'waiting' state. */
    osr = thread_reached_state(&(arg->state), WAITING, 1000);
    CU_ASSERT_EQUAL_FATAL(osr, os_resultSuccess);

    /* But thread should block and thus NOT reach 'stopped' state. */
    osr = thread_reached_state(&(arg->state), STOPPED, 100);
    CU_ASSERT_EQUAL_FATAL(osr, os_resultTimeout);

    arg->tid = thread_id;
}

static dds_return_t
waiting_thread_expect_exit(struct thread_arg_t *arg)
{
    os_result osr;
    assert(arg);
    osr = thread_reached_state(&(arg->state), STOPPED, 5000);
    if (osr == os_resultSuccess) {
        os_threadWaitExit(arg->tid, NULL);
        return DDS_RETCODE_OK;
    }
    return DDS_RETCODE_TIMEOUT;
}

