// Copyright(c) 2006 to 2020 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <limits.h>

#include "dds/dds.h"
#include "dds/ddsrt/misc.h"
#include "dds/ddsrt/process.h"
#include "dds/ddsrt/threads.h"

#include "test_common.h"

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
    (void)sample;
    return true;
}

static void
hierarchy_init(void)
{
    uint32_t mask = DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE;
    char name[100];

    g_participant = dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);
    CU_ASSERT_FATAL(g_participant > 0 );

    g_topic = dds_create_topic(g_participant, &RoundTripModule_DataType_desc, create_unique_topic_name("ddsc_hierarchy_test", name, sizeof name), NULL, NULL);
    CU_ASSERT_FATAL(g_topic > 0);

    g_publisher = dds_create_publisher(g_participant, NULL, NULL);
    CU_ASSERT_FATAL(g_publisher > 0 );

    g_subscriber = dds_create_subscriber(g_participant, NULL, NULL);
    CU_ASSERT_FATAL(g_subscriber > 0 );

    g_writer = dds_create_writer(g_publisher, g_topic, NULL, NULL);
    CU_ASSERT_FATAL(g_writer > 0 );

    g_reader = dds_create_reader(g_subscriber, g_topic, NULL, NULL);
    CU_ASSERT_FATAL(g_reader > 0);

    g_readcond = dds_create_readcondition(g_reader, mask);
    CU_ASSERT_FATAL(g_readcond > 0);

    g_querycond = dds_create_querycondition(g_reader, mask, accept_all);
    CU_ASSERT_FATAL(g_querycond > 0);

    /* The deletion of the last participant will close down every thing. This
     * means that the API will react differently after that. Because the
     * testing we're doing here is quite generic, we'd like to not close down
     * everything when we delete our participant. For that, we create a second
     * participant, which will keep everything running.
     */
    g_keep = dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);
    CU_ASSERT_FATAL(g_keep > 0);
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
CU_Test(ddsc_entity_delete, recursive, .init=hierarchy_init, .fini=hierarchy_fini)
{
    dds_domainid_t id;
    dds_return_t ret;

    /* First be sure that 'dds_get_domainid' returns ok. */
    ret = dds_get_domainid(g_participant, &id);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    ret = dds_get_domainid(g_topic, &id);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    ret = dds_get_domainid(g_publisher, &id);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    ret = dds_get_domainid(g_subscriber, &id);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    ret = dds_get_domainid(g_writer, &id);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    ret = dds_get_domainid(g_reader, &id);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    ret = dds_get_domainid(g_readcond, &id);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    ret = dds_get_domainid(g_querycond, &id);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    /* Deleting the top dog (participant) should delete all children. */
    ret = dds_delete(g_participant);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    /* Check if all the entities are deleted now. */
    ret = dds_get_domainid(g_participant, &id);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
    ret = dds_get_domainid(g_topic, &id);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
    ret = dds_get_domainid(g_publisher, &id);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
    ret = dds_get_domainid(g_subscriber, &id);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
    ret = dds_get_domainid(g_writer, &id);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
    ret = dds_get_domainid(g_reader, &id);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
    ret = dds_get_domainid(g_readcond, &id);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
    ret = dds_get_domainid(g_querycond, &id);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_entity_delete, recursive_with_deleted_topic)
{
    dds_domainid_t id;
    dds_return_t ret;
    char name[100];

    /* Internal handling of topic is different from all the other entities.
     * It's very interesting if this recursive deletion still works and
     * doesn't crash when the topic is already deleted (CHAM-424). */

    /* First, create a topic and a writer with that topic. */
    g_participant = dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);
    CU_ASSERT_FATAL(g_participant > 0);
    g_topic = dds_create_topic(g_participant, &RoundTripModule_DataType_desc, create_unique_topic_name("ddsc_hierarchy_test", name, 100), NULL, NULL);
    CU_ASSERT_FATAL(g_topic > 0);
    g_writer = dds_create_writer(g_participant, g_topic, NULL, NULL);
    CU_ASSERT_FATAL(g_writer> 0);
    g_keep = dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);
    CU_ASSERT_FATAL(g_keep > 0);

    /* Second, delete the topic to make sure that the writer holds the last
     * reference to the topic and thus will delete it when it itself is
     * deleted. */
    ret = dds_delete(g_topic);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    /* Second call to delete a topic must fail */
    ret = dds_delete(g_topic);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_ALREADY_DELETED);

    /* Third, deleting the participant should delete all children of which
     * the writer with the last topic reference is one. */
    ret = dds_delete(g_participant);
    /* Before the CHAM-424 fix, we would not get here because of a crash,
     * or it (incidentally) continued but returned an error. */
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    /* Check if the entities are actually deleted. */
    ret = dds_get_domainid(g_participant, &id);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER );
    ret = dds_get_domainid(g_topic, &id);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
    ret = dds_get_domainid(g_writer, &id);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);

    dds_delete(g_keep);
}
/*************************************************************************************************/




/**************************************************************************************************
 *
 * These will check the dds_get_participant in various ways.
 *
 *************************************************************************************************/
/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_entity_get_participant, valid_entities) = {
        CU_DataPoints(dds_entity_t*, &g_readcond, &g_querycond, &g_reader, &g_subscriber, &g_writer, &g_publisher, &g_topic, &g_participant),
};
CU_Theory((dds_entity_t *entity), ddsc_entity_get_participant, valid_entities, .init=hierarchy_init, .fini=hierarchy_fini)
{
    dds_entity_t participant;
    participant = dds_get_participant(*entity);
    CU_ASSERT_EQUAL_FATAL(participant, g_participant);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_entity_get_participant, deleted_entities) = {
        CU_DataPoints(dds_entity_t*, &g_readcond, &g_querycond, &g_reader, &g_subscriber, &g_writer, &g_publisher, &g_topic, &g_participant),
};
CU_Theory((dds_entity_t *entity), ddsc_entity_get_participant, deleted_entities, .init=hierarchy_init, .fini=hierarchy_fini)
{
    dds_entity_t participant;
    dds_delete(*entity);
    participant = dds_get_participant(*entity);
    CU_ASSERT_EQUAL_FATAL(participant, DDS_RETCODE_BAD_PARAMETER);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_entity_get_participant, invalid_entities) = {
        CU_DataPoints(dds_entity_t, -2, -1, 0, INT_MAX, INT_MIN),
};
CU_Theory((dds_entity_t entity), ddsc_entity_get_participant, invalid_entities, .init=hierarchy_init, .fini=hierarchy_fini)
{
    dds_entity_t participant;

    participant = dds_get_participant(entity);
    CU_ASSERT_EQUAL_FATAL(participant, DDS_RETCODE_BAD_PARAMETER);
}
/*************************************************************************************************/





/**************************************************************************************************
 *
 * These will check the dds_get_parent in various ways.
 *
 *************************************************************************************************/
/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_entity_get_parent, conditions) = {
        CU_DataPoints(dds_entity_t*, &g_readcond, &g_querycond),
};
CU_Theory((dds_entity_t *entity), ddsc_entity_get_parent, conditions, .init=hierarchy_init, .fini=hierarchy_fini)
{
    dds_entity_t parent;
    parent = dds_get_parent(*entity);
    CU_ASSERT_EQUAL_FATAL(parent, g_reader);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_entity_get_parent, reader, .init=hierarchy_init, .fini=hierarchy_fini)
{
    dds_entity_t parent;
    parent = dds_get_parent(g_reader);
    CU_ASSERT_EQUAL_FATAL(parent, g_subscriber);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_entity_get_parent, writer, .init=hierarchy_init, .fini=hierarchy_fini)
{
    dds_entity_t parent;
    parent = dds_get_parent(g_writer);
    CU_ASSERT_EQUAL_FATAL(parent, g_publisher);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_entity_get_parent, pubsubtop) = {
        CU_DataPoints(dds_entity_t*, &g_publisher, &g_subscriber, &g_topic),
};
CU_Theory((dds_entity_t *entity), ddsc_entity_get_parent, pubsubtop, .init=hierarchy_init, .fini=hierarchy_fini)
{
    dds_entity_t parent;
    parent = dds_get_parent(*entity);
    CU_ASSERT_EQUAL_FATAL(parent, g_participant);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_entity_get_parent, participant, .init=hierarchy_init, .fini=hierarchy_fini)
{
    dds_entity_t parent;
    parent = dds_get_parent(g_participant);
    CU_ASSERT_NOT_EQUAL_FATAL(parent, DDS_ENTITY_NIL);
    parent = dds_get_parent(parent);
    CU_ASSERT_NOT_EQUAL_FATAL(parent, DDS_ENTITY_NIL);
    parent = dds_get_parent(parent);
    CU_ASSERT_NOT_EQUAL_FATAL(parent, DDS_CYCLONEDDS_HANDLE);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_entity_get_parent, deleted_entities) = {
        CU_DataPoints(dds_entity_t*, &g_readcond, &g_querycond, &g_reader, &g_subscriber, &g_writer, &g_publisher, &g_topic, &g_participant),
};
CU_Theory((dds_entity_t *entity), ddsc_entity_get_parent, deleted_entities, .init=hierarchy_init, .fini=hierarchy_fini)
{
    dds_entity_t parent;
    dds_delete(*entity);
    parent = dds_get_parent(*entity);
    CU_ASSERT_EQUAL_FATAL(parent, DDS_RETCODE_BAD_PARAMETER);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_entity_get_parent, invalid_entities) = {
        CU_DataPoints(dds_entity_t, -2, -1, 0, INT_MAX, INT_MIN),
};
CU_Theory((dds_entity_t entity), ddsc_entity_get_parent, invalid_entities, .init=hierarchy_init, .fini=hierarchy_fini)
{
    dds_entity_t parent;

    parent = dds_get_parent(entity);
    CU_ASSERT_EQUAL_FATAL(parent, DDS_RETCODE_BAD_PARAMETER);
}
/*************************************************************************************************/





/**************************************************************************************************
 *
 * These will check the dds_get_children in various ways.
 *
 *************************************************************************************************/
/*************************************************************************************************/
CU_Test(ddsc_entity_get_children, null, .init=hierarchy_init, .fini=hierarchy_fini)
{
    dds_return_t ret;
    ret = dds_get_children(g_participant, NULL, 0);
    CU_ASSERT_EQUAL_FATAL(ret, 3);
}
/*************************************************************************************************/

/*************************************************************************************************/
#if SIZE_MAX > INT32_MAX
CU_Test(ddsc_entity_get_children, invalid_size, .init=hierarchy_init, .fini=hierarchy_fini)
{
    dds_return_t ret;
    dds_entity_t child;
    ret = dds_get_children(g_participant, &child, SIZE_MAX);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
}
#endif
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_entity_get_children, too_small, .init=hierarchy_init, .fini=hierarchy_fini)
{
    dds_return_t ret;
    dds_entity_t children[2];
    ret = dds_get_children(g_participant, children, 2);
    CU_ASSERT_EQUAL_FATAL(ret, 3);
    CU_ASSERT_FATAL((children[0] == g_publisher) || (children[0] == g_subscriber)  || (children[0] == g_topic));
    CU_ASSERT_FATAL((children[1] == g_publisher) || (children[1] == g_subscriber)  || (children[1] == g_topic));
    CU_ASSERT_NOT_EQUAL_FATAL(children[0], children[1]);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_entity_get_children, participant, .init=hierarchy_init, .fini=hierarchy_fini)
{
    dds_return_t ret;
    dds_entity_t children[4];
    ret = dds_get_children(g_participant, children, 4);
    CU_ASSERT_EQUAL_FATAL(ret, 3);
    CU_ASSERT_FATAL((children[0] == g_publisher) || (children[0] == g_subscriber)  || (children[0] == g_topic));
    CU_ASSERT_FATAL((children[1] == g_publisher) || (children[1] == g_subscriber)  || (children[1] == g_topic));
    CU_ASSERT_FATAL((children[2] == g_publisher) || (children[2] == g_subscriber)  || (children[2] == g_topic));
    CU_ASSERT_NOT_EQUAL_FATAL(children[0], children[1]);
    CU_ASSERT_NOT_EQUAL_FATAL(children[0], children[2]);
    CU_ASSERT_NOT_EQUAL_FATAL(children[1], children[2]);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_entity_get_children, topic, .init=hierarchy_init, .fini=hierarchy_fini)
{
    dds_return_t ret;
    dds_entity_t child;
    ret = dds_get_children(g_topic, &child, 1);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_entity_get_children, publisher, .init=hierarchy_init, .fini=hierarchy_fini)
{
    dds_return_t ret;
    dds_entity_t child;
    ret = dds_get_children(g_publisher, &child, 1);
    CU_ASSERT_EQUAL_FATAL(ret, 1);
    CU_ASSERT_EQUAL_FATAL(child, g_writer);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_entity_get_children, subscriber, .init=hierarchy_init, .fini=hierarchy_fini)
{
    dds_return_t ret;
    dds_entity_t children[2];
    ret = dds_get_children(g_subscriber, children, 2);
    CU_ASSERT_EQUAL_FATAL(ret, 1);
    CU_ASSERT_EQUAL_FATAL(children[0], g_reader);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_entity_get_children, writer, .init=hierarchy_init, .fini=hierarchy_fini)
{
    dds_return_t ret;
    ret = dds_get_children(g_writer, NULL, 0);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_entity_get_children, reader, .init=hierarchy_init, .fini=hierarchy_fini)
{
    dds_return_t ret;
    dds_entity_t children[2];
    ret = dds_get_children(g_reader, children, 2);
    CU_ASSERT_EQUAL_FATAL(ret, 2);
    CU_ASSERT_FATAL((children[0] == g_readcond) || (children[0] == g_querycond));
    CU_ASSERT_FATAL((children[1] == g_readcond) || (children[1] == g_querycond));
    CU_ASSERT_NOT_EQUAL_FATAL(children[0], children[1]);
}
/*************************************************************************************************/
/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_entity_get_children, conditions) = {
        CU_DataPoints(dds_entity_t*, &g_readcond, &g_querycond),
};
CU_Theory((dds_entity_t *entity), ddsc_entity_get_children, conditions, .init=hierarchy_init, .fini=hierarchy_fini)
{
    dds_return_t ret;
    dds_entity_t child;
    ret = dds_get_children(*entity, &child, 1);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_entity_get_children, deleted_entities) = {
        CU_DataPoints(dds_entity_t*, &g_readcond, &g_querycond, &g_reader, &g_subscriber, &g_writer, &g_publisher, &g_topic, &g_participant),
};
CU_Theory((dds_entity_t *entity), ddsc_entity_get_children, deleted_entities, .init=hierarchy_init, .fini=hierarchy_fini)
{
    dds_return_t ret;
    dds_entity_t children[4];
    dds_delete(*entity);
    ret = dds_get_children(*entity, children, 4);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_entity_get_children, invalid_entities) = {
        CU_DataPoints(dds_entity_t, -2, -1, 0, INT_MAX, INT_MIN),
};
CU_Theory((dds_entity_t entity), ddsc_entity_get_children, invalid_entities, .init=hierarchy_init, .fini=hierarchy_fini)
{
    dds_entity_t children[4];
    dds_return_t ret;

    ret = dds_get_children(entity, children, 4);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
}
/*************************************************************************************************/





/**************************************************************************************************
 *
 * These will check the dds_get_topic in various ways.
 *
 *************************************************************************************************/
/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_entity_get_topic, data_entities) = {
        CU_DataPoints(dds_entity_t*, &g_readcond, &g_querycond, &g_reader, &g_writer),
};
CU_Theory((dds_entity_t *entity), ddsc_entity_get_topic, data_entities, .init=hierarchy_init, .fini=hierarchy_fini)
{
    dds_entity_t topic;
    topic = dds_get_topic(*entity);
    CU_ASSERT_EQUAL_FATAL(topic, g_topic );
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_entity_get_topic, deleted_entities) = {
        CU_DataPoints(dds_entity_t*, &g_readcond, &g_querycond, &g_reader, &g_writer),
};
CU_Theory((dds_entity_t *entity), ddsc_entity_get_topic, deleted_entities, .init=hierarchy_init, .fini=hierarchy_fini)
{
    dds_entity_t topic;
    dds_delete(*entity);
    topic = dds_get_topic(*entity);
    CU_ASSERT_EQUAL_FATAL(topic, DDS_RETCODE_BAD_PARAMETER);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_entity_get_topic, invalid_entities) = {
        CU_DataPoints(dds_entity_t, -2, -1, 0, INT_MAX, INT_MIN),
};
CU_Theory((dds_entity_t entity), ddsc_entity_get_topic, invalid_entities, .init=hierarchy_init, .fini=hierarchy_fini)
{
    dds_entity_t topic;

    topic = dds_get_topic(entity);
    CU_ASSERT_EQUAL_FATAL(topic, DDS_RETCODE_BAD_PARAMETER);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_entity_get_topic, non_data_entities) = {
        CU_DataPoints(dds_entity_t*, &g_subscriber, &g_publisher, &g_topic, &g_participant),
};
CU_Theory((dds_entity_t *entity), ddsc_entity_get_topic, non_data_entities, .init=hierarchy_init, .fini=hierarchy_fini)
{
    dds_entity_t topic;
    topic = dds_get_topic(*entity);
    CU_ASSERT_EQUAL_FATAL(topic, DDS_RETCODE_ILLEGAL_OPERATION);
}
/*************************************************************************************************/





/**************************************************************************************************
 *
 * These will check the dds_get_publisher in various ways.
 *
 *************************************************************************************************/
/*************************************************************************************************/
CU_Test(ddsc_entity_get_publisher, writer, .init=hierarchy_init, .fini=hierarchy_fini)
{
    dds_entity_t publisher;
    publisher = dds_get_publisher(g_writer);
    CU_ASSERT_EQUAL_FATAL(publisher, g_publisher);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_entity_get_publisher, deleted_writer, .init=hierarchy_init, .fini=hierarchy_fini)
{
    dds_entity_t publisher;
    dds_delete(g_writer);
    publisher = dds_get_publisher(g_writer);
    CU_ASSERT_EQUAL_FATAL(publisher, DDS_RETCODE_BAD_PARAMETER);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_entity_get_publisher, invalid_writers) = {
        CU_DataPoints(dds_entity_t, -2, -1, 0, INT_MAX, INT_MIN),
};
CU_Theory((dds_entity_t entity), ddsc_entity_get_publisher, invalid_writers, .init=hierarchy_init, .fini=hierarchy_fini)
{
    dds_entity_t publisher;

    publisher = dds_get_publisher(entity);
    CU_ASSERT_EQUAL_FATAL(publisher, DDS_RETCODE_BAD_PARAMETER);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_entity_get_publisher, non_writers) = {
        CU_DataPoints(dds_entity_t*, &g_publisher, &g_reader, &g_publisher, &g_topic, &g_participant),
};
CU_Theory((dds_entity_t *cond), ddsc_entity_get_publisher, non_writers, .init=hierarchy_init, .fini=hierarchy_fini)
{
    dds_entity_t publisher;
    publisher = dds_get_publisher(*cond);
    CU_ASSERT_EQUAL_FATAL(publisher, DDS_RETCODE_ILLEGAL_OPERATION);
}
/*************************************************************************************************/




/**************************************************************************************************
 *
 * These will check the dds_get_subscriber in various ways.
 *
 *************************************************************************************************/
/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_entity_get_subscriber, readers) = {
        CU_DataPoints(dds_entity_t*, &g_readcond, &g_querycond, &g_reader),
};
CU_Theory((dds_entity_t *entity), ddsc_entity_get_subscriber, readers, .init=hierarchy_init, .fini=hierarchy_fini)
{
    dds_entity_t subscriber;
    subscriber = dds_get_subscriber(*entity);
    CU_ASSERT_EQUAL_FATAL(subscriber, g_subscriber);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_entity_get_subscriber, deleted_readers) = {
        CU_DataPoints(dds_entity_t*, &g_readcond, &g_querycond, &g_reader),
};
CU_Theory((dds_entity_t *entity), ddsc_entity_get_subscriber, deleted_readers, .init=hierarchy_init, .fini=hierarchy_fini)
{
    dds_entity_t subscriber;
    dds_delete(*entity);
    subscriber = dds_get_subscriber(*entity);
    CU_ASSERT_EQUAL_FATAL(subscriber, DDS_RETCODE_BAD_PARAMETER);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_entity_get_subscriber, invalid_readers) = {
        CU_DataPoints(dds_entity_t, -2, -1, 0, INT_MAX, INT_MIN),
};
CU_Theory((dds_entity_t entity), ddsc_entity_get_subscriber, invalid_readers, .init=hierarchy_init, .fini=hierarchy_fini)
{
    dds_entity_t subscriber;

    subscriber = dds_get_subscriber(entity);
    CU_ASSERT_EQUAL_FATAL(subscriber, DDS_RETCODE_BAD_PARAMETER);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_entity_get_subscriber, non_readers) = {
        CU_DataPoints(dds_entity_t*, &g_subscriber, &g_writer, &g_publisher, &g_topic, &g_participant),
};
CU_Theory((dds_entity_t *cond), ddsc_entity_get_subscriber, non_readers, .init=hierarchy_init, .fini=hierarchy_fini)
{
    dds_entity_t subscriber;
    subscriber = dds_get_subscriber(*cond);
    CU_ASSERT_EQUAL_FATAL(subscriber, DDS_RETCODE_ILLEGAL_OPERATION);
}
/*************************************************************************************************/






/**************************************************************************************************
 *
 * These will check the dds_get_datareader in various ways.
 *
 *************************************************************************************************/
/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_entity_get_datareader, conditions) = {
        CU_DataPoints(dds_entity_t*, &g_readcond, &g_querycond),
};
CU_Theory((dds_entity_t *cond), ddsc_entity_get_datareader, conditions, .init=hierarchy_init, .fini=hierarchy_fini)
{
    dds_entity_t reader;
    reader = dds_get_datareader(*cond);
    CU_ASSERT_EQUAL_FATAL(reader, g_reader);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_entity_get_datareader, deleted_conds) = {
        CU_DataPoints(dds_entity_t*, &g_readcond, &g_querycond),
};
CU_Theory((dds_entity_t *cond), ddsc_entity_get_datareader, deleted_conds, .init=hierarchy_init, .fini=hierarchy_fini)
{
    dds_entity_t reader;
    dds_delete(*cond);
    reader = dds_get_datareader(*cond);
    CU_ASSERT_EQUAL_FATAL(reader, DDS_RETCODE_BAD_PARAMETER);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_entity_get_datareader, invalid_conds) = {
        CU_DataPoints(dds_entity_t, -2, -1, 0, INT_MAX, INT_MIN),
};
CU_Theory((dds_entity_t cond), ddsc_entity_get_datareader, invalid_conds, .init=hierarchy_init, .fini=hierarchy_fini)
{
    dds_entity_t reader;

    reader = dds_get_datareader(cond);
    CU_ASSERT_EQUAL_FATAL(reader, DDS_RETCODE_BAD_PARAMETER);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_entity_get_datareader, non_conds) = {
        CU_DataPoints(dds_entity_t*, &g_reader, &g_subscriber, &g_writer, &g_publisher, &g_topic, &g_participant),
};
CU_Theory((dds_entity_t *cond), ddsc_entity_get_datareader, non_conds, .init=hierarchy_init, .fini=hierarchy_fini)
{
    dds_entity_t reader;
    reader = dds_get_datareader(*cond);
    CU_ASSERT_EQUAL_FATAL(reader, DDS_RETCODE_ILLEGAL_OPERATION);
}

/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_entity_implicit_publisher, deleted)
{
    dds_entity_t participant;
    dds_entity_t writer;
    dds_entity_t topic;
    dds_return_t ret;
    char name[100];

    participant = dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);
    CU_ASSERT_FATAL(participant > 0);

    topic = dds_create_topic(participant, &RoundTripModule_DataType_desc, create_unique_topic_name("ddsc_entity_implicit_publisher_test", name, 100), NULL, NULL);
    CU_ASSERT_FATAL(topic > 0);

    writer = dds_create_writer(participant, topic, NULL, NULL);
    CU_ASSERT_FATAL(writer > 0);

    ret = dds_get_children(participant, NULL, 0);
    CU_ASSERT_EQUAL_FATAL(ret, 2);

    dds_delete(writer);

    ret = dds_get_children(participant, NULL, 0);
    CU_ASSERT_EQUAL_FATAL(ret, 1);

    dds_delete(topic);
    dds_delete(participant);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_entity_implicit_publisher, invalid_topic)
{
    dds_entity_t participant;
    dds_entity_t writer;

    participant = dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);
    CU_ASSERT_FATAL(participant > 0);

    /* Disable SAL warning on intentional misuse of the API */
    DDSRT_WARNING_MSVC_OFF(28020);
    writer = dds_create_writer(participant, 0, NULL, NULL);
    /* Disable SAL warning on intentional misuse of the API */
    DDSRT_WARNING_MSVC_ON(28020);
    CU_ASSERT_FATAL(writer < 0);

    dds_delete(writer);
    dds_delete(participant);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_entity_implicit_subscriber, deleted)
{
    dds_entity_t participant;
    dds_entity_t reader;
    dds_entity_t topic;
    dds_return_t ret;
    char name[100];

    participant = dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);
    CU_ASSERT_FATAL(participant > 0);

    topic = dds_create_topic(participant, &RoundTripModule_DataType_desc, create_unique_topic_name("ddsc_entity_implicit_subscriber_test", name, 100), NULL, NULL);
    CU_ASSERT_FATAL(topic > 0);

    reader = dds_create_reader(participant, topic, NULL, NULL);
    CU_ASSERT_FATAL(reader > 0);

    ret = dds_get_children(participant, NULL, 0);
    CU_ASSERT_EQUAL_FATAL(ret, 2);

    dds_delete(reader);

    ret = dds_get_children(participant, NULL, 0);
    CU_ASSERT_EQUAL_FATAL(ret, 1);

    dds_delete(topic);
    dds_delete(participant);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_entity_explicit_subscriber, invalid_topic)
{
    dds_entity_t participant;
    dds_entity_t reader;
    dds_entity_t subscriber;

    participant = dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);
    CU_ASSERT_FATAL(participant > 0);

    subscriber = dds_create_subscriber(participant, NULL,NULL);
    /* Disable SAL warning on intentional misuse of the API */
    DDSRT_WARNING_MSVC_OFF(28020);
    reader = dds_create_reader(subscriber, 0, NULL, NULL);
    DDSRT_WARNING_MSVC_ON(28020);
    CU_ASSERT_FATAL(reader < 0);

    dds_delete(reader);
    dds_delete(participant);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_entity_get_children, implicit_publisher)
{
    dds_entity_t participant;
    dds_entity_t publisher = 0;
    dds_entity_t writer;
    dds_entity_t topic;
    dds_entity_t child[2], child2[2];
    dds_return_t ret;
    char name[100];

    participant = dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);
    CU_ASSERT_FATAL(participant > 0);

    topic = dds_create_topic(participant, &RoundTripModule_DataType_desc, create_unique_topic_name("ddsc_entity_implicit_publisher_test", name, 100), NULL, NULL);
    CU_ASSERT_FATAL(topic > 0);

    writer = dds_create_writer(participant, topic, NULL, NULL);
    CU_ASSERT_FATAL(writer > 0);
    ret = dds_get_children(participant, child, 2);
    CU_ASSERT_EQUAL_FATAL(ret, 2);
    if(child[0] == topic){
      publisher = child[1];
    } else if(child[1] == topic){
        publisher = child[0];
    } else{
        CU_FAIL_FATAL("topic was not returned");
    }
    CU_ASSERT_NOT_EQUAL_FATAL(publisher, topic);

    CU_ASSERT_FATAL(publisher > 0);
    CU_ASSERT_NOT_EQUAL_FATAL(publisher, writer);

    dds_delete(writer);

    ret = dds_get_children(participant, child2, 2);
    CU_ASSERT_EQUAL_FATAL(ret, 1);
    CU_ASSERT_FATAL( (child2[0] == child[0]) || (child2[0] == child[1]) );

    dds_delete(topic);
    dds_delete(participant);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_entity_get_children, implicit_subscriber)
{
    dds_entity_t participant;
    dds_entity_t subscriber = 0;
    dds_entity_t reader;
    dds_entity_t topic;
    dds_entity_t child[2], child2[2];
    dds_return_t ret;
    char name[100];

    participant = dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);
    CU_ASSERT_FATAL(participant > 0);

    topic = dds_create_topic(participant, &RoundTripModule_DataType_desc, create_unique_topic_name("ddsc_entity_implicit_subscriber_test", name, 100), NULL, NULL);
    CU_ASSERT_FATAL(topic > 0);

    reader = dds_create_reader(participant, topic, NULL, NULL);
    CU_ASSERT_FATAL(reader > 0);
    ret = dds_get_children(participant, child, 2);
    CU_ASSERT_EQUAL_FATAL(ret, 2);
    if(child[0] == topic){
        subscriber = child[1];
    } else if(child[1] == topic){
        subscriber = child[0];
    } else{
        CU_FAIL_FATAL("topic was not returned");
    }
    CU_ASSERT_NOT_EQUAL_FATAL(subscriber, topic);

    CU_ASSERT_FATAL(subscriber > 0);
    CU_ASSERT_NOT_EQUAL_FATAL(subscriber, reader);

    dds_delete(reader);

    ret = dds_get_children(participant, child2, 2);
    CU_ASSERT_EQUAL_FATAL(ret, 1);
    CU_ASSERT_FATAL( (child2[0] == child[0]) || (child2[0] == child[1]) );

    dds_delete(topic);
    dds_delete(participant);

}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_entity_get_parent, implicit_publisher)
{
    dds_entity_t participant;
    dds_entity_t writer;
    dds_entity_t parent;
    dds_entity_t topic;
    dds_return_t ret;
    char name[100];

    participant = dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);
    CU_ASSERT_FATAL(participant > 0);

    topic = dds_create_topic(participant, &RoundTripModule_DataType_desc, create_unique_topic_name("ddsc_entity_implicit_publisher_promotion_test", name, 100), NULL, NULL);
    CU_ASSERT_FATAL(topic > 0);

    writer = dds_create_writer(participant, topic, NULL, NULL);
    CU_ASSERT_FATAL(writer > 0);

    parent = dds_get_parent(writer);
    CU_ASSERT_NOT_EQUAL_FATAL(parent, participant);
    CU_ASSERT_FATAL(parent > 0);

    dds_delete(writer);

    ret = dds_delete(parent);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
    dds_delete(participant);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_entity_get_parent, implicit_subscriber)
{
    dds_entity_t participant;
    dds_entity_t reader;
    dds_entity_t parent;
    dds_entity_t topic;
    dds_return_t ret;
    char name[100];

    participant = dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);
    CU_ASSERT_FATAL(participant > 0);

    topic = dds_create_topic(participant, &RoundTripModule_DataType_desc, create_unique_topic_name("ddsc_entity_implicit_subscriber_promotion_test", name, 100), NULL, NULL);
    CU_ASSERT_FATAL(topic > 0);

    reader = dds_create_reader(participant, topic, NULL, NULL);
    CU_ASSERT_FATAL(reader > 0);

    parent = dds_get_parent(reader);
    CU_ASSERT_NOT_EQUAL_FATAL(parent, participant);
    CU_ASSERT_FATAL(parent > 0);

    dds_delete(reader);

    ret = dds_delete(parent);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
    dds_delete(participant);

}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_entity_implicit, delete_publisher)
{
    dds_entity_t participant;
    dds_entity_t writer;
    dds_entity_t parent;
    dds_entity_t topic;
    dds_return_t ret;
    char name[100];

    participant = dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);
    CU_ASSERT_FATAL(participant > 0);

    topic = dds_create_topic(participant, &RoundTripModule_DataType_desc, create_unique_topic_name("ddsc_entity_implicit_delete_publisher", name, 100), NULL, NULL);
    CU_ASSERT_FATAL(topic > 0);

    writer = dds_create_writer(participant, topic, NULL, NULL);
    CU_ASSERT_FATAL(writer > 0);

    parent = dds_get_parent(writer);
    CU_ASSERT_FATAL(parent > 0);

    ret = dds_delete(parent);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    ret = dds_delete(writer);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);

    dds_delete(participant);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_entity_implicit, delete_subscriber)
{
    dds_entity_t participant;
    dds_entity_t reader;
    dds_entity_t parent;
    dds_entity_t topic;
    dds_return_t ret;
    char name[100];

    participant = dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);
    CU_ASSERT_FATAL(participant > 0);

    topic = dds_create_topic(participant, &RoundTripModule_DataType_desc, create_unique_topic_name("ddsc_entity_implicit_delete_subscriber", name, 100), NULL, NULL);
    CU_ASSERT_FATAL(topic > 0);

    reader = dds_create_reader(participant, topic, NULL, NULL);
    CU_ASSERT_FATAL(reader > 0);

    parent = dds_get_parent(reader);
    CU_ASSERT_FATAL(parent > 0);

    ret = dds_delete(parent);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    ret = dds_delete(reader);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);

    dds_delete(participant);
}
/*************************************************************************************************/

/*************************************************************************************************/

#endif
