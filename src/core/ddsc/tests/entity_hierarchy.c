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
#include "os/os.h"
#include <criterion/criterion.h>
#include <criterion/logging.h>
#include <criterion/theories.h>
#include "RoundTrip.h"

/* Add --verbose command line argument to get the cr_log_info traces (if there are any). */



/**************************************************************************************************
 *
 * Test fixtures
 *
 *************************************************************************************************/

static dds_entity_t g_keep        = 0;
static dds_entity_t g_participant = 0;
static dds_entity_t g_topic       = 0;
static dds_entity_t g_subscriber  = 0;
static dds_entity_t g_publisher   = 0;
static dds_entity_t g_reader      = 0;
static dds_entity_t g_writer      = 0;
static dds_entity_t g_readcond    = 0;
static dds_entity_t g_querycond   = 0;

/* Dummy query condition callback. */
static bool
accept_all(const void * sample)
{
    return true;
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
hierarchy_init(void)
{
    uint32_t mask = DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
    char name[100];

    g_participant = dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);
    cr_assert_gt(g_participant, 0, "Failed to create prerequisite g_participant");

    g_topic = dds_create_topic(g_participant, &RoundTripModule_DataType_desc, create_topic_name("ddsc_hierarchy_test", name, sizeof name), NULL, NULL);
    cr_assert_gt(g_topic, 0, "Failed to create prerequisite g_topic");

    g_publisher = dds_create_publisher(g_participant, NULL, NULL);
    cr_assert_gt(g_publisher, 0, "Failed to create prerequisite g_publisher");

    g_subscriber = dds_create_subscriber(g_participant, NULL, NULL);
    cr_assert_gt(g_subscriber, 0, "Failed to create prerequisite g_subscriber");

    g_writer = dds_create_writer(g_publisher, g_topic, NULL, NULL);
    cr_assert_gt(g_writer, 0, "Failed to create prerequisite g_writer");

    g_reader = dds_create_reader(g_subscriber, g_topic, NULL, NULL);
    cr_assert_gt(g_reader, 0, "Failed to create prerequisite g_reader");

    g_readcond = dds_create_readcondition(g_reader, mask);
    cr_assert_gt(g_readcond, 0, "Failed to create prerequisite g_readcond");

    g_querycond = dds_create_querycondition(g_reader, mask, accept_all);
    cr_assert_gt(g_querycond, 0, "Failed to create prerequisite g_querycond");

    /* The deletion of the last participant will close down every thing. This
     * means that the API will react differently after that. Because the
     * testing we're doing here is quite generic, we'd like to not close down
     * everything when we delete our participant. For that, we create a second
     * participant, which will keep everything running.
     */
    g_keep = dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);
    cr_assert_gt(g_keep, 0, "Failed to create prerequisite g_keep");
}

static void
hierarchy_fini(void)
{
    dds_delete(g_querycond);
    dds_delete(g_readcond);
    dds_delete(g_reader);
    dds_delete(g_writer);
    dds_delete(g_subscriber);
    dds_delete(g_publisher);
    dds_delete(g_topic);
    dds_delete(g_participant);
    dds_delete(g_keep);
}


#if 0
#else

/**************************************************************************************************
 *
 * These will check the recursive deletion.
 *
 *************************************************************************************************/
/*************************************************************************************************/
Test(ddsc_entity_delete, recursive, .init=hierarchy_init, .fini=hierarchy_fini)
{
    dds_domainid_t id;
    dds_return_t ret;

    /* First be sure that 'dds_get_domainid' returns ok. */
    ret = dds_get_domainid(g_participant, &id);
    cr_assert_eq(dds_err_nr(ret), DDS_RETCODE_OK);
    ret = dds_get_domainid(g_topic, &id);
    cr_assert_eq(dds_err_nr(ret), DDS_RETCODE_OK);
    ret = dds_get_domainid(g_publisher, &id);
    cr_assert_eq(dds_err_nr(ret), DDS_RETCODE_OK);
    ret = dds_get_domainid(g_subscriber, &id);
    cr_assert_eq(dds_err_nr(ret), DDS_RETCODE_OK);
    ret = dds_get_domainid(g_writer, &id);
    cr_assert_eq(dds_err_nr(ret), DDS_RETCODE_OK);
    ret = dds_get_domainid(g_reader, &id);
    cr_assert_eq(dds_err_nr(ret), DDS_RETCODE_OK);
    ret = dds_get_domainid(g_readcond, &id);
    cr_assert_eq(dds_err_nr(ret), DDS_RETCODE_OK);
    ret = dds_get_domainid(g_querycond, &id);
    cr_assert_eq(dds_err_nr(ret), DDS_RETCODE_OK);

    /* Deleting the top dog (participant) should delete all children. */
    ret = dds_delete(g_participant);
    cr_assert_eq(dds_err_nr(ret), DDS_RETCODE_OK);

    /* Check if all the entities are deleted now. */
    ret = dds_get_domainid(g_participant, &id);
    cr_assert_eq(dds_err_nr(ret), DDS_RETCODE_ALREADY_DELETED);
    ret = dds_get_domainid(g_topic, &id);
    cr_assert_eq(dds_err_nr(ret), DDS_RETCODE_ALREADY_DELETED);
    ret = dds_get_domainid(g_publisher, &id);
    cr_assert_eq(dds_err_nr(ret), DDS_RETCODE_ALREADY_DELETED);
    ret = dds_get_domainid(g_subscriber, &id);
    cr_assert_eq(dds_err_nr(ret), DDS_RETCODE_ALREADY_DELETED);
    ret = dds_get_domainid(g_writer, &id);
    cr_assert_eq(dds_err_nr(ret), DDS_RETCODE_ALREADY_DELETED);
    ret = dds_get_domainid(g_reader, &id);
    cr_assert_eq(dds_err_nr(ret), DDS_RETCODE_ALREADY_DELETED);
    ret = dds_get_domainid(g_readcond, &id);
    cr_assert_eq(dds_err_nr(ret), DDS_RETCODE_ALREADY_DELETED);
    ret = dds_get_domainid(g_querycond, &id);
    cr_assert_eq(dds_err_nr(ret), DDS_RETCODE_ALREADY_DELETED);
}
/*************************************************************************************************/

/*************************************************************************************************/
Test(ddsc_entity_delete, recursive_with_deleted_topic)
{
    dds_domainid_t id;
    dds_return_t ret;
    char name[100];

    /* Internal handling of topic is different from all the other entities.
     * It's very interesting if this recursive deletion still works and
     * doesn't crash when the topic is already deleted (CHAM-424). */

    /* First, create a topic and a writer with that topic. */
    g_participant = dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);
    cr_assert_gt(g_participant, 0, "Failed to create prerequisite g_participant");
    g_topic = dds_create_topic(g_participant, &RoundTripModule_DataType_desc, create_topic_name("ddsc_hierarchy_test", name, 100), NULL, NULL);
    cr_assert_gt(g_topic, 0, "Failed to create prerequisite g_topic");
    g_writer = dds_create_writer(g_participant, g_topic, NULL, NULL);
    cr_assert_gt(g_writer, 0, "Failed to create prerequisite g_writer");
    g_keep = dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);
    cr_assert_gt(g_keep, 0, "Failed to create prerequisite g_keep");

    /* Second, delete the topic to make sure that the writer holds the last
     * reference to the topic and thus will delete it when it itself is
     * deleted. */
    ret = dds_delete(g_topic);
    cr_assert_eq(dds_err_nr(ret), DDS_RETCODE_OK);

    /* Third, deleting the participant should delete all children of which
     * the writer with the last topic reference is one. */
    ret = dds_delete(g_participant);
    /* Before the CHAM-424 fix, we would not get here because of a crash,
     * or it (incidentally) continued but returned an error. */
    cr_assert_eq(dds_err_nr(ret), DDS_RETCODE_OK);

    /* Check if the entities are actually deleted. */
    ret = dds_get_domainid(g_participant, &id);
    cr_assert_eq(dds_err_nr(ret), DDS_RETCODE_ALREADY_DELETED, "%s", dds_err_str(ret));
    ret = dds_get_domainid(g_topic, &id);
    cr_assert_eq(dds_err_nr(ret), DDS_RETCODE_ALREADY_DELETED);
    ret = dds_get_domainid(g_writer, &id);
    cr_assert_eq(dds_err_nr(ret), DDS_RETCODE_ALREADY_DELETED);

    dds_delete(g_keep);
}
/*************************************************************************************************/




/**************************************************************************************************
 *
 * These will check the dds_get_participant in various ways.
 *
 *************************************************************************************************/
/*************************************************************************************************/
TheoryDataPoints(ddsc_entity_get_participant, valid_entities) = {
        DataPoints(dds_entity_t*, &g_readcond, &g_querycond, &g_reader, &g_subscriber, &g_writer, &g_publisher, &g_topic, &g_participant),
};
Theory((dds_entity_t *entity), ddsc_entity_get_participant, valid_entities, .init=hierarchy_init, .fini=hierarchy_fini)
{
    dds_entity_t participant;
    participant = dds_get_participant(*entity);
    cr_assert_eq(participant, g_participant);
}
/*************************************************************************************************/

/*************************************************************************************************/
TheoryDataPoints(ddsc_entity_get_participant, deleted_entities) = {
        DataPoints(dds_entity_t*, &g_readcond, &g_querycond, &g_reader, &g_subscriber, &g_writer, &g_publisher, &g_topic, &g_participant),
};
Theory((dds_entity_t *entity), ddsc_entity_get_participant, deleted_entities, .init=hierarchy_init, .fini=hierarchy_fini)
{
    dds_entity_t participant;
    dds_delete(*entity);
    participant = dds_get_participant(*entity);
    cr_assert_eq(dds_err_nr(participant), DDS_RETCODE_ALREADY_DELETED, "returned %d", dds_err_nr(participant));
}
/*************************************************************************************************/

/*************************************************************************************************/
TheoryDataPoints(ddsc_entity_get_participant, invalid_entities) = {
        DataPoints(dds_entity_t, -2, -1, 0, 1, 100, INT_MAX, INT_MIN),
};
Theory((dds_entity_t entity), ddsc_entity_get_participant, invalid_entities, .init=hierarchy_init, .fini=hierarchy_fini)
{
    dds_entity_t exp = DDS_RETCODE_BAD_PARAMETER * -1;
    dds_entity_t participant;

    participant = dds_get_participant(entity);
    cr_assert_eq(dds_err_nr(participant), dds_err_nr(exp), "returned %d != expected %d", dds_err_nr(participant), dds_err_nr(exp));
}
/*************************************************************************************************/





/**************************************************************************************************
 *
 * These will check the dds_get_parent in various ways.
 *
 *************************************************************************************************/
/*************************************************************************************************/
TheoryDataPoints(ddsc_entity_get_parent, conditions) = {
        DataPoints(dds_entity_t*, &g_readcond, &g_querycond),
};
Theory((dds_entity_t *entity), ddsc_entity_get_parent, conditions, .init=hierarchy_init, .fini=hierarchy_fini)
{
    dds_entity_t parent;
    parent = dds_get_parent(*entity);
    cr_assert_eq(parent, g_reader);
}
/*************************************************************************************************/

/*************************************************************************************************/
Test(ddsc_entity_get_parent, reader, .init=hierarchy_init, .fini=hierarchy_fini)
{
    dds_entity_t parent;
    parent = dds_get_parent(g_reader);
    cr_assert_eq(parent, g_subscriber);
}
/*************************************************************************************************/

/*************************************************************************************************/
Test(ddsc_entity_get_parent, writer, .init=hierarchy_init, .fini=hierarchy_fini)
{
    dds_entity_t parent;
    parent = dds_get_parent(g_writer);
    cr_assert_eq(parent, g_publisher);
}
/*************************************************************************************************/

/*************************************************************************************************/
TheoryDataPoints(ddsc_entity_get_parent, pubsubtop) = {
        DataPoints(dds_entity_t*, &g_publisher, &g_subscriber, &g_topic),
};
Theory((dds_entity_t *entity), ddsc_entity_get_parent, pubsubtop, .init=hierarchy_init, .fini=hierarchy_fini)
{
    dds_entity_t parent;
    parent = dds_get_parent(*entity);
    cr_assert_eq(parent, g_participant);
}
/*************************************************************************************************/

/*************************************************************************************************/
Test(ddsc_entity_get_parent, participant, .init=hierarchy_init, .fini=hierarchy_fini)
{
    dds_entity_t parent;
    parent = dds_get_parent(g_participant);
    cr_assert_eq(dds_err_nr(parent), DDS_ENTITY_NIL, "returned %d", dds_err_nr(parent));
}
/*************************************************************************************************/

/*************************************************************************************************/
TheoryDataPoints(ddsc_entity_get_parent, deleted_entities) = {
        DataPoints(dds_entity_t*, &g_readcond, &g_querycond, &g_reader, &g_subscriber, &g_writer, &g_publisher, &g_topic, &g_participant),
};
Theory((dds_entity_t *entity), ddsc_entity_get_parent, deleted_entities, .init=hierarchy_init, .fini=hierarchy_fini)
{
    dds_entity_t parent;
    dds_delete(*entity);
    parent = dds_get_parent(*entity);
    cr_assert_eq(dds_err_nr(parent), DDS_RETCODE_ALREADY_DELETED);
}
/*************************************************************************************************/

/*************************************************************************************************/
TheoryDataPoints(ddsc_entity_get_parent, invalid_entities) = {
        DataPoints(dds_entity_t, -2, -1, 0, 1, 100, INT_MAX, INT_MIN),
};
Theory((dds_entity_t entity), ddsc_entity_get_parent, invalid_entities, .init=hierarchy_init, .fini=hierarchy_fini)
{
    dds_entity_t exp = DDS_RETCODE_BAD_PARAMETER * -1;
    dds_entity_t parent;

    parent = dds_get_parent(entity);
    cr_assert_eq(dds_err_nr(parent), dds_err_nr(exp), "returned %d != expected %d", dds_err_nr(parent), dds_err_nr(exp));
}
/*************************************************************************************************/





/**************************************************************************************************
 *
 * These will check the dds_get_children in various ways.
 *
 *************************************************************************************************/
/*************************************************************************************************/
Test(ddsc_entity_get_children, null, .init=hierarchy_init, .fini=hierarchy_fini)
{
    dds_return_t ret;
    ret = dds_get_children(g_participant, NULL, 0);
    cr_assert_eq(ret, 3);
}
/*************************************************************************************************/

/*************************************************************************************************/
Test(ddsc_entity_get_children, invalid_size, .init=hierarchy_init, .fini=hierarchy_fini)
{
    dds_return_t ret;
    dds_entity_t child;
    ret = dds_get_children(g_participant, &child, INT32_MAX);
    cr_assert_eq(dds_err_nr(ret), DDS_RETCODE_BAD_PARAMETER);
}
/*************************************************************************************************/

/*************************************************************************************************/
Test(ddsc_entity_get_children, too_small, .init=hierarchy_init, .fini=hierarchy_fini)
{
    dds_return_t ret;
    dds_entity_t children[2];
    ret = dds_get_children(g_participant, children, 2);
    cr_assert_eq(ret, 3);
    cr_assert((children[0] == g_publisher) || (children[0] == g_subscriber)  || (children[0] == g_topic));
    cr_assert((children[1] == g_publisher) || (children[1] == g_subscriber)  || (children[1] == g_topic));
    cr_assert_neq(children[0], children[1]);
}
/*************************************************************************************************/

/*************************************************************************************************/
Test(ddsc_entity_get_children, participant, .init=hierarchy_init, .fini=hierarchy_fini)
{
    dds_return_t ret;
    dds_entity_t children[4];
    ret = dds_get_children(g_participant, children, 4);
    cr_assert_eq(ret, 3);
    cr_assert((children[0] == g_publisher) || (children[0] == g_subscriber)  || (children[0] == g_topic));
    cr_assert((children[1] == g_publisher) || (children[1] == g_subscriber)  || (children[1] == g_topic));
    cr_assert((children[2] == g_publisher) || (children[2] == g_subscriber)  || (children[2] == g_topic));
    cr_assert_neq(children[0], children[1]);
    cr_assert_neq(children[0], children[2]);
    cr_assert_neq(children[1], children[2]);
}
/*************************************************************************************************/

/*************************************************************************************************/
Test(ddsc_entity_get_children, topic, .init=hierarchy_init, .fini=hierarchy_fini)
{
    dds_return_t ret;
    dds_entity_t child;
    ret = dds_get_children(g_topic, &child, 1);
    cr_assert_eq(ret, 0);
}
/*************************************************************************************************/

/*************************************************************************************************/
Test(ddsc_entity_get_children, publisher, .init=hierarchy_init, .fini=hierarchy_fini)
{
    dds_return_t ret;
    dds_entity_t child;
    ret = dds_get_children(g_publisher, &child, 1);
    cr_assert_eq(ret, 1);
    cr_assert_eq(child, g_writer);
}
/*************************************************************************************************/

/*************************************************************************************************/
Test(ddsc_entity_get_children, subscriber, .init=hierarchy_init, .fini=hierarchy_fini)
{
    dds_return_t ret;
    dds_entity_t children[2];
    ret = dds_get_children(g_subscriber, children, 2);
    cr_assert_eq(ret, 1);
    cr_assert_eq(children[0], g_reader);
}
/*************************************************************************************************/

/*************************************************************************************************/
Test(ddsc_entity_get_children, writer, .init=hierarchy_init, .fini=hierarchy_fini)
{
    dds_return_t ret;
    ret = dds_get_children(g_writer, NULL, 0);
    cr_assert_eq(ret, 0);
}
/*************************************************************************************************/

/*************************************************************************************************/
Test(ddsc_entity_get_children, reader, .init=hierarchy_init, .fini=hierarchy_fini)
{
    dds_return_t ret;
    dds_entity_t children[2];
    ret = dds_get_children(g_reader, children, 2);
    cr_assert_eq(ret, 2);
    cr_assert((children[0] == g_readcond) || (children[0] == g_querycond));
    cr_assert((children[1] == g_readcond) || (children[1] == g_querycond));
    cr_assert_neq(children[0], children[1]);
}
/*************************************************************************************************/
/*************************************************************************************************/
TheoryDataPoints(ddsc_entity_get_children, conditions) = {
        DataPoints(dds_entity_t*, &g_readcond, &g_querycond),
};
Theory((dds_entity_t *entity), ddsc_entity_get_children, conditions, .init=hierarchy_init, .fini=hierarchy_fini)
{
    dds_return_t ret;
    dds_entity_t child;
    ret = dds_get_children(*entity, &child, 1);
    cr_assert_eq(ret, 0);
}
/*************************************************************************************************/

/*************************************************************************************************/
TheoryDataPoints(ddsc_entity_get_children, deleted_entities) = {
        DataPoints(dds_entity_t*, &g_readcond, &g_querycond, &g_reader, &g_subscriber, &g_writer, &g_publisher, &g_topic, &g_participant),
};
Theory((dds_entity_t *entity), ddsc_entity_get_children, deleted_entities, .init=hierarchy_init, .fini=hierarchy_fini)
{
    dds_return_t ret;
    dds_entity_t children[4];
    dds_delete(*entity);
    ret = dds_get_children(*entity, children, 4);
    cr_assert_eq(dds_err_nr(ret), DDS_RETCODE_ALREADY_DELETED);
}
/*************************************************************************************************/

/*************************************************************************************************/
TheoryDataPoints(ddsc_entity_get_children, invalid_entities) = {
        DataPoints(dds_entity_t, -2, -1, 0, 1, 100, INT_MAX, INT_MIN),
};
Theory((dds_entity_t entity), ddsc_entity_get_children, invalid_entities, .init=hierarchy_init, .fini=hierarchy_fini)
{
    dds_entity_t exp = DDS_RETCODE_BAD_PARAMETER * -1;
    dds_entity_t children[4];
    dds_return_t ret;

    ret = dds_get_children(entity, children, 4);
    cr_assert_eq(dds_err_nr(ret), dds_err_nr(exp), "returned %d != expected %d", dds_err_nr(ret), dds_err_nr(exp));
}
/*************************************************************************************************/





/**************************************************************************************************
 *
 * These will check the dds_get_topic in various ways.
 *
 *************************************************************************************************/
/*************************************************************************************************/
TheoryDataPoints(ddsc_entity_get_topic, data_entities) = {
        DataPoints(dds_entity_t*, &g_readcond, &g_querycond, &g_reader, &g_writer),
};
Theory((dds_entity_t *entity), ddsc_entity_get_topic, data_entities, .init=hierarchy_init, .fini=hierarchy_fini)
{
    dds_entity_t topic;
    topic = dds_get_topic(*entity);
    cr_assert_eq(topic, g_topic );
}
/*************************************************************************************************/

/*************************************************************************************************/
TheoryDataPoints(ddsc_entity_get_topic, deleted_entities) = {
        DataPoints(dds_entity_t*, &g_readcond, &g_querycond, &g_reader, &g_writer),
};
Theory((dds_entity_t *entity), ddsc_entity_get_topic, deleted_entities, .init=hierarchy_init, .fini=hierarchy_fini)
{
    dds_entity_t topic;
    dds_delete(*entity);
    topic = dds_get_topic(*entity);
    cr_assert_eq(dds_err_nr(topic), DDS_RETCODE_ALREADY_DELETED);
}
/*************************************************************************************************/

/*************************************************************************************************/
TheoryDataPoints(ddsc_entity_get_topic, invalid_entities) = {
        DataPoints(dds_entity_t, -2, -1, 0, 1, 100, INT_MAX, INT_MIN),
};
Theory((dds_entity_t entity), ddsc_entity_get_topic, invalid_entities, .init=hierarchy_init, .fini=hierarchy_fini)
{
    dds_entity_t exp = DDS_RETCODE_BAD_PARAMETER * -1;
    dds_entity_t topic;

    topic = dds_get_topic(entity);
    cr_assert_eq(dds_err_nr(topic), dds_err_nr(exp), "returned %d != expected %d", dds_err_nr(topic), dds_err_nr(exp));
}
/*************************************************************************************************/

/*************************************************************************************************/
TheoryDataPoints(ddsc_entity_get_topic, non_data_entities) = {
        DataPoints(dds_entity_t*, &g_subscriber, &g_publisher, &g_topic, &g_participant),
};
Theory((dds_entity_t *entity), ddsc_entity_get_topic, non_data_entities, .init=hierarchy_init, .fini=hierarchy_fini)
{
    dds_entity_t topic;
    topic = dds_get_topic(*entity);
    cr_assert_eq(dds_err_nr(topic), DDS_RETCODE_ILLEGAL_OPERATION, "returned %d", dds_err_nr(topic));
}
/*************************************************************************************************/





/**************************************************************************************************
 *
 * These will check the dds_get_publisher in various ways.
 *
 *************************************************************************************************/
/*************************************************************************************************/
Test(ddsc_entity_get_publisher, writer, .init=hierarchy_init, .fini=hierarchy_fini)
{
    dds_entity_t publisher;
    publisher = dds_get_publisher(g_writer);
    cr_assert_eq(publisher, g_publisher);
}
/*************************************************************************************************/

/*************************************************************************************************/
Test(ddsc_entity_get_publisher, deleted_writer, .init=hierarchy_init, .fini=hierarchy_fini)
{
    dds_entity_t publisher;
    dds_delete(g_writer);
    publisher = dds_get_publisher(g_writer);
    cr_assert_eq(dds_err_nr(publisher), DDS_RETCODE_ALREADY_DELETED);
}
/*************************************************************************************************/

/*************************************************************************************************/
TheoryDataPoints(ddsc_entity_get_publisher, invalid_writers) = {
        DataPoints(dds_entity_t, -2, -1, 0, 1, 100, INT_MAX, INT_MIN),
};
Theory((dds_entity_t entity), ddsc_entity_get_publisher, invalid_writers, .init=hierarchy_init, .fini=hierarchy_fini)
{
    dds_entity_t exp = DDS_RETCODE_BAD_PARAMETER * -1;
    dds_entity_t publisher;

    publisher = dds_get_publisher(entity);
    cr_assert_eq(dds_err_nr(publisher), dds_err_nr(exp), "returned %d != expected %d", dds_err_nr(publisher), dds_err_nr(exp));
}
/*************************************************************************************************/

/*************************************************************************************************/
TheoryDataPoints(ddsc_entity_get_publisher, non_writers) = {
        DataPoints(dds_entity_t*, &g_publisher, &g_reader, &g_publisher, &g_topic, &g_participant),
};
Theory((dds_entity_t *cond), ddsc_entity_get_publisher, non_writers, .init=hierarchy_init, .fini=hierarchy_fini)
{
    dds_entity_t publisher;
    publisher = dds_get_publisher(*cond);
    cr_assert_eq(dds_err_nr(publisher), DDS_RETCODE_ILLEGAL_OPERATION, "returned %d", dds_err_nr(publisher));
}
/*************************************************************************************************/




/**************************************************************************************************
 *
 * These will check the dds_get_subscriber in various ways.
 *
 *************************************************************************************************/
/*************************************************************************************************/
TheoryDataPoints(ddsc_entity_get_subscriber, readers) = {
        DataPoints(dds_entity_t*, &g_readcond, &g_querycond, &g_reader),
};
Theory((dds_entity_t *entity), ddsc_entity_get_subscriber, readers, .init=hierarchy_init, .fini=hierarchy_fini)
{
    dds_entity_t subscriber;
    subscriber = dds_get_subscriber(*entity);
    cr_assert_eq(subscriber, g_subscriber);
}
/*************************************************************************************************/

/*************************************************************************************************/
TheoryDataPoints(ddsc_entity_get_subscriber, deleted_readers) = {
        DataPoints(dds_entity_t*, &g_readcond, &g_querycond, &g_reader),
};
Theory((dds_entity_t *entity), ddsc_entity_get_subscriber, deleted_readers, .init=hierarchy_init, .fini=hierarchy_fini)
{
    dds_entity_t subscriber;
    dds_delete(*entity);
    subscriber = dds_get_subscriber(*entity);
    cr_assert_eq(dds_err_nr(subscriber), DDS_RETCODE_ALREADY_DELETED);
}
/*************************************************************************************************/

/*************************************************************************************************/
TheoryDataPoints(ddsc_entity_get_subscriber, invalid_readers) = {
        DataPoints(dds_entity_t, -2, -1, 0, 1, 100, INT_MAX, INT_MIN),
};
Theory((dds_entity_t entity), ddsc_entity_get_subscriber, invalid_readers, .init=hierarchy_init, .fini=hierarchy_fini)
{
    dds_entity_t exp = DDS_RETCODE_BAD_PARAMETER * -1;
    dds_entity_t subscriber;

    subscriber = dds_get_subscriber(entity);
    cr_assert_eq(dds_err_nr(subscriber), dds_err_nr(exp), "returned %d != expected %d", dds_err_nr(subscriber), dds_err_nr(exp));
}
/*************************************************************************************************/

/*************************************************************************************************/
TheoryDataPoints(ddsc_entity_get_subscriber, non_readers) = {
        DataPoints(dds_entity_t*, &g_subscriber, &g_writer, &g_publisher, &g_topic, &g_participant),
};
Theory((dds_entity_t *cond), ddsc_entity_get_subscriber, non_readers, .init=hierarchy_init, .fini=hierarchy_fini)
{
    dds_entity_t subscriber;
    subscriber = dds_get_subscriber(*cond);
    cr_assert_eq(dds_err_nr(subscriber), DDS_RETCODE_ILLEGAL_OPERATION, "returned %d", dds_err_nr(subscriber));
}
/*************************************************************************************************/






/**************************************************************************************************
 *
 * These will check the dds_get_datareader in various ways.
 *
 *************************************************************************************************/
/*************************************************************************************************/
TheoryDataPoints(ddsc_entity_get_datareader, conditions) = {
        DataPoints(dds_entity_t*, &g_readcond, &g_querycond),
};
Theory((dds_entity_t *cond), ddsc_entity_get_datareader, conditions, .init=hierarchy_init, .fini=hierarchy_fini)
{
    dds_entity_t reader;
    reader = dds_get_datareader(*cond);
    cr_assert_eq(reader, g_reader);
}
/*************************************************************************************************/

/*************************************************************************************************/
TheoryDataPoints(ddsc_entity_get_datareader, deleted_conds) = {
        DataPoints(dds_entity_t*, &g_readcond, &g_querycond),
};
Theory((dds_entity_t *cond), ddsc_entity_get_datareader, deleted_conds, .init=hierarchy_init, .fini=hierarchy_fini)
{
    dds_entity_t reader;
    dds_delete(*cond);
    reader = dds_get_datareader(*cond);
    cr_assert_eq(dds_err_nr(reader), DDS_RETCODE_ALREADY_DELETED);
}
/*************************************************************************************************/

/*************************************************************************************************/
TheoryDataPoints(ddsc_entity_get_datareader, invalid_conds) = {
        DataPoints(dds_entity_t, -2, -1, 0, 1, 100, INT_MAX, INT_MIN),
};
Theory((dds_entity_t cond), ddsc_entity_get_datareader, invalid_conds, .init=hierarchy_init, .fini=hierarchy_fini)
{
    dds_entity_t exp = DDS_RETCODE_BAD_PARAMETER * -1;
    dds_entity_t reader;

    reader = dds_get_datareader(cond);
    cr_assert_eq(dds_err_nr(reader), dds_err_nr(exp), "returned %d != expected %d", dds_err_nr(reader), dds_err_nr(exp));
}
/*************************************************************************************************/

/*************************************************************************************************/
TheoryDataPoints(ddsc_entity_get_datareader, non_conds) = {
        DataPoints(dds_entity_t*, &g_reader, &g_subscriber, &g_writer, &g_publisher, &g_topic, &g_participant),
};
Theory((dds_entity_t *cond), ddsc_entity_get_datareader, non_conds, .init=hierarchy_init, .fini=hierarchy_fini)
{
    dds_entity_t reader;
    reader = dds_get_datareader(*cond);
    cr_assert_eq(dds_err_nr(reader), DDS_RETCODE_ILLEGAL_OPERATION, "returned %d", dds_err_nr(reader));
}

/*************************************************************************************************/

/*************************************************************************************************/
Test(ddsc_entity_implicit_publisher, deleted)
{
    dds_entity_t participant;
    dds_entity_t writer;
    dds_entity_t topic;
    dds_return_t ret;
    char name[100];

    participant = dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);
    cr_assert_gt(participant, 0);

    topic = dds_create_topic(participant, &RoundTripModule_DataType_desc, create_topic_name("ddsc_entity_implicit_publisher_test", name, 100), NULL, NULL);
    cr_assert_gt(topic, 0);

    writer = dds_create_writer(participant, topic, NULL, NULL);
    cr_assert_gt(writer, 0);

    ret = dds_get_children(participant, NULL, 0);
    cr_assert_eq(ret, 2);

    dds_delete(writer);

    ret = dds_get_children(participant, NULL, 0);
    cr_assert_eq(ret, 1);

    dds_delete(topic);
    dds_delete(participant);
}
/*************************************************************************************************/

/*************************************************************************************************/
Test(ddsc_entity_implicit_publisher, invalid_topic)
{
    dds_entity_t participant;
    dds_entity_t writer;

    participant = dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);
    cr_assert_gt(participant, 0);

    /* Disable SAL warning on intentional misuse of the API */
    OS_WARNING_MSVC_OFF(28020);
    writer = dds_create_writer(participant, 0, NULL, NULL);
    /* Disable SAL warning on intentional misuse of the API */
    OS_WARNING_MSVC_ON(28020);
    cr_assert_lt(writer, 0);

    dds_delete(writer);
    dds_delete(participant);
}
/*************************************************************************************************/

/*************************************************************************************************/
Test(ddsc_entity_implicit_subscriber, deleted)
{
    dds_entity_t participant;
    dds_entity_t reader;
    dds_entity_t topic;
    dds_return_t ret;
    char name[100];

    participant = dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);
    cr_assert_gt(participant, 0);

    topic = dds_create_topic(participant, &RoundTripModule_DataType_desc, create_topic_name("ddsc_entity_implicit_subscriber_test", name, 100), NULL, NULL);
    cr_assert_gt(topic, 0);

    reader = dds_create_reader(participant, topic, NULL, NULL);
    cr_assert_gt(reader, 0);

    ret = dds_get_children(participant, NULL, 0);
    cr_assert_eq(ret, 2);

    dds_delete(reader);

    ret = dds_get_children(participant, NULL, 0);
    cr_assert_eq(ret, 1);

    dds_delete(topic);
    dds_delete(participant);
}
/*************************************************************************************************/

/*************************************************************************************************/
Test(ddsc_entity_explicit_subscriber, invalid_topic)
{
    dds_entity_t participant;
    dds_entity_t reader;
    dds_entity_t subscriber;

    participant = dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);
    cr_assert_gt(participant, 0);

    subscriber = dds_create_subscriber(participant, NULL,NULL);
    /* Disable SAL warning on intentional misuse of the API */
    OS_WARNING_MSVC_OFF(28020);
    reader = dds_create_reader(subscriber, 0, NULL, NULL);
    OS_WARNING_MSVC_ON(28020);
    cr_assert_lt(reader, 0);

    dds_delete(reader);
    dds_delete(participant);
}
/*************************************************************************************************/

/*************************************************************************************************/
Test(ddsc_entity_get_children, implicit_publisher)
{
    dds_entity_t participant;
    dds_entity_t publisher;
    dds_entity_t writer;
    dds_entity_t topic;
    dds_entity_t child[2], child2[2];
    dds_return_t ret;
    char name[100];

    participant = dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);
    cr_assert_gt(participant, 0);

    topic = dds_create_topic(participant, &RoundTripModule_DataType_desc, create_topic_name("ddsc_entity_implicit_publisher_test", name, 100), NULL, NULL);
    cr_assert_gt(topic, 0);

    writer = dds_create_writer(participant, topic, NULL, NULL);
    cr_assert_gt(writer, 0);
    ret = dds_get_children(participant, child, 2);
    cr_assert_eq(ret, 2);
    if(child[0] == topic){
      publisher = child[1];
    } else if(child[1] == topic){
        publisher = child[0];
    } else{
       cr_assert(false, "topic was not returned");
    }
    cr_assert_neq(publisher, topic);

    cr_assert_gt(publisher, 0);
    cr_assert_neq(publisher, writer);

    dds_delete(writer);

    ret = dds_get_children(participant, child2, 2);
    cr_assert_eq(ret, 2);
    cr_assert( (child2[0] == child[0]) || (child2[0] == child[1]) );
    cr_assert( (child2[1] == child[0]) || (child2[1] == child[1]) );

    dds_delete(topic);
    dds_delete(participant);
}
/*************************************************************************************************/

/*************************************************************************************************/
Test(ddsc_entity_get_children, implicit_subscriber)
{
    dds_entity_t participant;
    dds_entity_t subscriber;
    dds_entity_t reader;
    dds_entity_t topic;
    dds_entity_t child[2], child2[2];
    dds_return_t ret;
    char name[100];

    participant = dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);
    cr_assert_gt(participant, 0);

    topic = dds_create_topic(participant, &RoundTripModule_DataType_desc, create_topic_name("ddsc_entity_implicit_subscriber_test", name, 100), NULL, NULL);
    cr_assert_gt(topic, 0);

    reader = dds_create_reader(participant, topic, NULL, NULL);
    cr_assert_gt(reader, 0);
    ret = dds_get_children(participant, child, 2);
    cr_assert_eq(ret, 2);
    if(child[0] == topic){
        subscriber = child[1];
    } else if(child[1] == topic){
        subscriber = child[0];
    } else{
        cr_assert(false, "topic was not returned");
    }
    cr_assert_neq(subscriber, topic);

    cr_assert_gt(subscriber, 0);
    cr_assert_neq(subscriber, reader);

    dds_delete(reader);

    ret = dds_get_children(participant, child2, 2);
    cr_assert_eq(ret, 2);
    cr_assert( (child2[0] == child[0]) || (child2[0] == child[1]) );
    cr_assert( (child2[1] == child[0]) || (child2[1] == child[1]) );

    dds_delete(topic);
    dds_delete(participant);

}
/*************************************************************************************************/

/*************************************************************************************************/
Test(ddsc_entity_get_parent, implicit_publisher)
{
    dds_entity_t participant;
    dds_entity_t writer;
    dds_entity_t parent;
    dds_entity_t topic;
    dds_return_t ret;
    char name[100];

    participant = dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);
    cr_assert_gt(participant, 0);

    topic = dds_create_topic(participant, &RoundTripModule_DataType_desc, create_topic_name("ddsc_entity_implicit_publisher_promotion_test", name, 100), NULL, NULL);
    cr_assert_gt(topic, 0);

    writer = dds_create_writer(participant, topic, NULL, NULL);
    cr_assert_gt(writer, 0);

    parent = dds_get_parent(writer);
    cr_assert_neq(parent, participant);
    cr_assert_gt(parent, 0);

    dds_delete(writer);

    ret = dds_delete(parent);
    cr_assert_eq(dds_err_nr(ret), DDS_RETCODE_OK);
    dds_delete(participant);
}
/*************************************************************************************************/

/*************************************************************************************************/
Test(ddsc_entity_get_parent, implicit_subscriber)
{
    dds_entity_t participant;
    dds_entity_t reader;
    dds_entity_t parent;
    dds_entity_t topic;
    dds_return_t ret;
    char name[100];

    participant = dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);
    cr_assert_gt(participant, 0);

    topic = dds_create_topic(participant, &RoundTripModule_DataType_desc, create_topic_name("ddsc_entity_implicit_subscriber_promotion_test", name, 100), NULL, NULL);
    cr_assert_gt(topic, 0);

    reader = dds_create_reader(participant, topic, NULL, NULL);
    cr_assert_gt(reader, 0);

    parent = dds_get_parent(reader);
    cr_assert_neq(parent, participant);
    cr_assert_gt(parent, 0);

    dds_delete(reader);

    ret = dds_delete(parent);
    cr_assert_eq(dds_err_nr(ret), DDS_RETCODE_OK);
    dds_delete(participant);

}
/*************************************************************************************************/

/*************************************************************************************************/

#endif
