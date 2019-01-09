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
#include "RoundTrip.h"
#include "CUnit/Test.h"

/****************************************************************************
 * TODO: (CHAM-279) Add DDS_INCONSISTENT_TOPIC_STATUS test
 * TODO: (CHAM-277) Add DDS_OFFERED/REQUESTED_DEADLINE_MISSED_STATUS test
 * TODO: (CHAM-278) Add DDS_LIVELINESS_LOST_STATUS test
 * TODO: Check DDS_REQUESTED_INCOMPATIBLE_QOS_STATUS intermittent fail (total_count != 1)
 ****************************************************************************/



/****************************************************************************
 * Convenience test macros.
 ****************************************************************************/
#define ASSERT_CALLBACK_EQUAL(fntype, listener, expected) \
    do { \
        dds_on_##fntype##_fn cb; \
        dds_lget_##fntype(listener, &cb); \
        CU_ASSERT_EQUAL(cb, expected); \
    } while (0)

#define STR(fntype) #fntype##_cb

#define TEST_GET_SET(listener, fntype, cb) \
    do { \
        dds_on_##fntype##_fn dummy = NULL; \
        /* Initially expect DDS_LUNSET on a newly created listener */ \
        ASSERT_CALLBACK_EQUAL(fntype, listener, DDS_LUNSET); \
        /* Using listener or callback NULL, shouldn't crash and be noop */ \
        dds_lset_##fntype(NULL, NULL); \
        dds_lget_##fntype(NULL, NULL); \
        dds_lget_##fntype(listener, NULL); \
        dds_lget_##fntype(NULL, &dummy);  \
        CU_ASSERT_EQUAL_FATAL(dummy, NULL); \
        /* Set to NULL, get to confirm it succeeds */ \
        dds_lset_##fntype(listener, NULL); \
        ASSERT_CALLBACK_EQUAL(fntype, listener, NULL); \
        /* Set to a proper cb method, get to confirm it succeeds */ \
        dds_lset_##fntype(listener, cb); \
        ASSERT_CALLBACK_EQUAL(fntype, listener, cb); \
    } while (0)



/****************************************************************************
 * Test globals.
 ****************************************************************************/
static dds_entity_t    g_participant = 0;
static dds_entity_t    g_subscriber  = 0;
static dds_entity_t    g_publisher   = 0;
static dds_entity_t    g_topic       = 0;
static dds_entity_t    g_writer      = 0;
static dds_entity_t    g_reader      = 0;

static dds_listener_t *g_listener = NULL;
static dds_qos_t      *g_qos = NULL;
static os_mutex        g_mutex;
static os_cond         g_cond;



/****************************************************************************
 * Callback stuff.
 ****************************************************************************/
static uint32_t        cb_called     = 0;
static dds_entity_t    cb_topic      = 0;
static dds_entity_t    cb_writer     = 0;
static dds_entity_t    cb_reader     = 0;
static dds_entity_t    cb_subscriber = 0;

static dds_inconsistent_topic_status_t          cb_inconsistent_topic_status;
static dds_liveliness_lost_status_t             cb_liveliness_lost_status;
static dds_offered_deadline_missed_status_t     cb_offered_deadline_missed_status;
static dds_offered_incompatible_qos_status_t    cb_offered_incompatible_qos_status;
static dds_sample_lost_status_t                 cb_sample_lost_status;
static dds_sample_rejected_status_t             cb_sample_rejected_status;
static dds_liveliness_changed_status_t          cb_liveliness_changed_status;
static dds_requested_deadline_missed_status_t   cb_requested_deadline_missed_status;
static dds_requested_incompatible_qos_status_t  cb_requested_incompatible_qos_status;
static dds_publication_matched_status_t         cb_publication_matched_status;
static dds_subscription_matched_status_t        cb_subscription_matched_status;


static void
inconsistent_topic_cb(
        dds_entity_t topic,
        const dds_inconsistent_topic_status_t status, void* arg)
{
    (void)arg;
    os_mutexLock(&g_mutex);
    cb_topic = topic;
    cb_inconsistent_topic_status = status;
    cb_called |= DDS_INCONSISTENT_TOPIC_STATUS;
    os_condBroadcast(&g_cond);
    os_mutexUnlock(&g_mutex);
}

static void
liveliness_lost_cb(
        dds_entity_t writer,
        const dds_liveliness_lost_status_t status,
        void* arg)
{
    (void)arg;
    os_mutexLock(&g_mutex);
    cb_writer = writer;
    cb_liveliness_lost_status = status;
    cb_called |= DDS_LIVELINESS_LOST_STATUS;
    os_condBroadcast(&g_cond);
    os_mutexUnlock(&g_mutex);
}

static void
offered_deadline_missed_cb(
        dds_entity_t writer,
        const dds_offered_deadline_missed_status_t status,
        void* arg)
{
    (void)arg;
    os_mutexLock(&g_mutex);
    cb_writer = writer;
    cb_offered_deadline_missed_status = status;
    cb_called |= DDS_OFFERED_DEADLINE_MISSED_STATUS;
    os_condBroadcast(&g_cond);
    os_mutexUnlock(&g_mutex);
}

static void
offered_incompatible_qos_cb(
        dds_entity_t writer,
        const dds_offered_incompatible_qos_status_t status,
        void* arg)
{
    (void)arg;
    os_mutexLock(&g_mutex);
    cb_writer = writer;
    cb_offered_incompatible_qos_status = status;
    cb_called |= DDS_OFFERED_INCOMPATIBLE_QOS_STATUS;
    os_condBroadcast(&g_cond);
    os_mutexUnlock(&g_mutex);
}

static void
data_on_readers_cb(
        dds_entity_t subscriber,
        void* arg)
{
    (void)arg;
    os_mutexLock(&g_mutex);
    cb_subscriber = subscriber;
    cb_called |= DDS_DATA_ON_READERS_STATUS;
    os_condBroadcast(&g_cond);
    os_mutexUnlock(&g_mutex);
}

static void
sample_lost_cb(
        dds_entity_t reader,
        const dds_sample_lost_status_t status,
        void* arg)
{
    (void)arg;
    os_mutexLock(&g_mutex);
    cb_reader = reader;
    cb_sample_lost_status = status;
    cb_called |= DDS_SAMPLE_LOST_STATUS;
    os_condBroadcast(&g_cond);
    os_mutexUnlock(&g_mutex);
}

static void
data_available_cb(
        dds_entity_t reader,
        void* arg)
{
    (void)arg;
    os_mutexLock(&g_mutex);
    cb_reader = reader;
    cb_called |= DDS_DATA_AVAILABLE_STATUS;
    os_condBroadcast(&g_cond);
    os_mutexUnlock(&g_mutex);
}

static void
sample_rejected_cb(
        dds_entity_t reader,
        const dds_sample_rejected_status_t status,
        void* arg)
{
    (void)arg;
    os_mutexLock(&g_mutex);
    cb_reader = reader;
    cb_sample_rejected_status = status;
    cb_called |= DDS_SAMPLE_REJECTED_STATUS;
    os_condBroadcast(&g_cond);
    os_mutexUnlock(&g_mutex);
}

static void
liveliness_changed_cb(
        dds_entity_t reader,
        const dds_liveliness_changed_status_t status,
        void* arg)
{
    (void)arg;
    os_mutexLock(&g_mutex);
    cb_reader = reader;
    cb_liveliness_changed_status = status;
    cb_called |= DDS_LIVELINESS_CHANGED_STATUS;
    os_condBroadcast(&g_cond);
    os_mutexUnlock(&g_mutex);
}

static void
requested_deadline_missed_cb(
        dds_entity_t reader,
        const dds_requested_deadline_missed_status_t status,
        void* arg)
{
    (void)arg;
    os_mutexLock(&g_mutex);
    cb_reader = reader;
    cb_requested_deadline_missed_status = status;
    cb_called |= DDS_REQUESTED_DEADLINE_MISSED_STATUS;
    os_condBroadcast(&g_cond);
    os_mutexUnlock(&g_mutex);
}

static void
requested_incompatible_qos_cb(
        dds_entity_t reader,
        const dds_requested_incompatible_qos_status_t status,
        void* arg)
{
    (void)arg;
    os_mutexLock(&g_mutex);
    cb_reader = reader;
    cb_requested_incompatible_qos_status = status;
    cb_called |= DDS_REQUESTED_INCOMPATIBLE_QOS_STATUS;
    os_condBroadcast(&g_cond);
    os_mutexUnlock(&g_mutex);
}

static void
publication_matched_cb(
        dds_entity_t writer,
        const dds_publication_matched_status_t status,
        void* arg)
{
    (void)arg;
    os_mutexLock(&g_mutex);
    cb_writer = writer;
    cb_publication_matched_status = status;
    cb_called |= DDS_PUBLICATION_MATCHED_STATUS;
    os_condBroadcast(&g_cond);
    os_mutexUnlock(&g_mutex);
}

static void
subscription_matched_cb(
        dds_entity_t reader,
        const dds_subscription_matched_status_t status,
        void* arg)
{
    (void)arg;
    os_mutexLock(&g_mutex);
    cb_reader = reader;
    cb_subscription_matched_status = status;
    cb_called |= DDS_SUBSCRIPTION_MATCHED_STATUS;
    os_condBroadcast(&g_cond);
    os_mutexUnlock(&g_mutex);
}

static void
callback_dummy(void)
{
}

static uint32_t
waitfor_cb(uint32_t expected)
{
    os_time timeout = { 5, 0 };
    os_result osr = os_resultSuccess;
    os_mutexLock(&g_mutex);
    while (((cb_called & expected) != expected) && (osr == os_resultSuccess)) {
        osr = os_condTimedWait(&g_cond, &g_mutex, &timeout);
    }
    os_mutexUnlock(&g_mutex);
    return cb_called;
}



/****************************************************************************
 * Test initializations and teardowns.
 ****************************************************************************/
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
init_triggering_base(void)
{
    char name[100];

    os_osInit();

    os_mutexInit(&g_mutex);
    os_condInit(&g_cond, &g_mutex);

    g_participant = dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);
    CU_ASSERT_FATAL(g_participant > 0);

    g_subscriber = dds_create_subscriber(g_participant, NULL, NULL);
    CU_ASSERT_FATAL(g_subscriber > 0);

    g_publisher = dds_create_publisher(g_participant, NULL, NULL);
    CU_ASSERT_FATAL(g_publisher > 0);

    g_topic = dds_create_topic(g_participant, &RoundTripModule_DataType_desc, create_topic_name("ddsc_listener_test", name, 100), NULL, NULL);
    CU_ASSERT_FATAL(g_topic > 0);

    g_listener = dds_create_listener(NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(g_listener);

    g_qos = dds_create_qos();
    CU_ASSERT_PTR_NOT_NULL_FATAL(g_qos);
    dds_qset_reliability(g_qos, DDS_RELIABILITY_RELIABLE, DDS_SECS(1));
    dds_qset_history(g_qos, DDS_HISTORY_KEEP_ALL, 0);
}

static void
init_triggering_test(void)
{
    uint32_t triggered;

    /* Initialize base. */
    init_triggering_base();

    /* Set QoS Policies that'll help us test various status callbacks. */
    dds_qset_destination_order(g_qos, DDS_DESTINATIONORDER_BY_SOURCE_TIMESTAMP);
    dds_qset_reliability(g_qos, DDS_RELIABILITY_BEST_EFFORT, DDS_MSECS(100));
    dds_qset_resource_limits(g_qos, 1, 1, 1);

    /* Use these to be sure reader and writer know each other. */
    dds_lset_publication_matched(g_listener, publication_matched_cb);
    dds_lset_subscription_matched(g_listener, subscription_matched_cb);
    dds_lset_liveliness_changed(g_listener, liveliness_changed_cb);

    /* Create reader and writer with proper listeners. */
    g_writer = dds_create_writer(g_publisher, g_topic, g_qos, g_listener);
    CU_ASSERT(g_writer > 0);
    g_reader = dds_create_reader(g_subscriber, g_topic, g_qos, g_listener);
    CU_ASSERT(g_reader > 0);

    /* Sync. */
    triggered = waitfor_cb(DDS_PUBLICATION_MATCHED_STATUS | DDS_SUBSCRIPTION_MATCHED_STATUS | DDS_LIVELINESS_CHANGED_STATUS);
    CU_ASSERT_EQUAL_FATAL(triggered & DDS_LIVELINESS_CHANGED_STATUS,    DDS_LIVELINESS_CHANGED_STATUS);
    CU_ASSERT_EQUAL_FATAL(triggered & DDS_PUBLICATION_MATCHED_STATUS,   DDS_PUBLICATION_MATCHED_STATUS);
    CU_ASSERT_EQUAL_FATAL(triggered & DDS_SUBSCRIPTION_MATCHED_STATUS,  DDS_SUBSCRIPTION_MATCHED_STATUS);
}

static void
fini_triggering_base(void)
{
    dds_delete_qos(g_qos);
    dds_delete_listener(g_listener);
    dds_delete(g_participant);
    os_condDestroy(&g_cond);
    os_mutexDestroy(&g_mutex);
    os_osExit();
}

static void
fini_triggering_test(void)
{
    dds_delete(g_reader);
    dds_delete(g_writer);
    fini_triggering_base();
}


/****************************************************************************
 * API tests
 ****************************************************************************/
CU_Test(ddsc_listener, create_and_delete)
{
    /* Verify create doesn't return null */
    dds_listener_t *listener;
    listener = dds_create_listener(NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(listener);

    /* Check default cb's are set */
    ASSERT_CALLBACK_EQUAL(inconsistent_topic, listener, DDS_LUNSET);
    ASSERT_CALLBACK_EQUAL(liveliness_lost, listener, DDS_LUNSET);
    ASSERT_CALLBACK_EQUAL(offered_deadline_missed, listener, DDS_LUNSET);
    ASSERT_CALLBACK_EQUAL(offered_incompatible_qos, listener, DDS_LUNSET);
    ASSERT_CALLBACK_EQUAL(data_on_readers, listener, DDS_LUNSET);
    ASSERT_CALLBACK_EQUAL(sample_lost, listener, DDS_LUNSET);
    ASSERT_CALLBACK_EQUAL(sample_rejected, listener, DDS_LUNSET);
    ASSERT_CALLBACK_EQUAL(liveliness_changed, listener, DDS_LUNSET);
    ASSERT_CALLBACK_EQUAL(requested_deadline_missed, listener, DDS_LUNSET);
    ASSERT_CALLBACK_EQUAL(requested_incompatible_qos, listener, DDS_LUNSET);
    ASSERT_CALLBACK_EQUAL(publication_matched, listener, DDS_LUNSET);
    ASSERT_CALLBACK_EQUAL(subscription_matched, listener, DDS_LUNSET);
    ASSERT_CALLBACK_EQUAL(data_available, listener, DDS_LUNSET);

    dds_delete_listener(listener);
    OS_WARNING_MSVC_OFF(6387); /* Disable SAL warning on intentional misuse of the API */
    dds_delete_listener(NULL);
    OS_WARNING_MSVC_ON(6387);
}

CU_Test(ddsc_listener, reset)
{
    dds_listener_t *listener;
    listener = dds_create_listener(NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(listener);

    /* Set a listener cb to a non-default value */
    dds_lset_data_available(listener, NULL);
    ASSERT_CALLBACK_EQUAL(data_available, listener, NULL);

    /* Listener cb should revert to default after reset */
    dds_reset_listener(listener);
    ASSERT_CALLBACK_EQUAL(data_available, listener, DDS_LUNSET);

    /* Resetting a NULL listener should not crash */
    dds_reset_listener(NULL);

    dds_delete_listener(listener);
}

CU_Test(ddsc_listener, copy)
{
    dds_listener_t *listener1 = NULL, *listener2 = NULL;
    listener1 = dds_create_listener(NULL);
    listener2 = dds_create_listener(NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(listener1);
    CU_ASSERT_PTR_NOT_NULL_FATAL(listener2);

    /* Set some listener1 callbacks to non-default values */
    dds_lset_data_available(listener1, NULL);
    dds_lset_sample_lost(listener1, sample_lost_cb);
    ASSERT_CALLBACK_EQUAL(data_available, listener1, NULL);
    ASSERT_CALLBACK_EQUAL(sample_lost, listener1, sample_lost_cb);
    ASSERT_CALLBACK_EQUAL(data_available, listener2, DDS_LUNSET);
    ASSERT_CALLBACK_EQUAL(sample_lost, listener2, DDS_LUNSET);

    /* Cb's should be copied to listener2 */
    dds_copy_listener(listener2, listener1);
    ASSERT_CALLBACK_EQUAL(data_available, listener1, NULL);
    ASSERT_CALLBACK_EQUAL(data_available, listener2, NULL);
    ASSERT_CALLBACK_EQUAL(sample_lost, listener1, sample_lost_cb);
    ASSERT_CALLBACK_EQUAL(sample_lost, listener2, sample_lost_cb);

    /* Calling copy with NULL should not crash and be noops. */
    OS_WARNING_MSVC_OFF(6387); /* Disable SAL warning on intentional misuse of the API */
    dds_copy_listener(listener2, NULL);
    dds_copy_listener(NULL, listener1);
    dds_copy_listener(NULL, NULL);
    OS_WARNING_MSVC_ON(6387);

    dds_delete_listener(listener1);
    dds_delete_listener(listener2);
}

CU_Test(ddsc_listener, merge)
{
    dds_listener_t *listener1 = NULL, *listener2 = NULL;
    listener1 = dds_create_listener(NULL);
    listener2 = dds_create_listener(NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(listener1);
    CU_ASSERT_PTR_NOT_NULL_FATAL(listener2);

    /* Set all listener1 callbacks to non-default values */
    dds_lset_inconsistent_topic         (listener1, inconsistent_topic_cb);
    dds_lset_liveliness_lost            (listener1, liveliness_lost_cb);
    dds_lset_offered_deadline_missed    (listener1, offered_deadline_missed_cb);
    dds_lset_offered_incompatible_qos   (listener1, offered_incompatible_qos_cb);
    dds_lset_data_on_readers            (listener1, data_on_readers_cb);
    dds_lset_sample_lost                (listener1, sample_lost_cb);
    dds_lset_data_available             (listener1, data_available_cb);
    dds_lset_sample_rejected            (listener1, sample_rejected_cb);
    dds_lset_liveliness_changed         (listener1, liveliness_changed_cb);
    dds_lset_requested_deadline_missed  (listener1, requested_deadline_missed_cb);
    dds_lset_requested_incompatible_qos (listener1, requested_incompatible_qos_cb);
    dds_lset_publication_matched        (listener1, publication_matched_cb);
    dds_lset_subscription_matched       (listener1, subscription_matched_cb);

    /* Merging listener1 into empty listener2 should act a bit like a copy. */
    dds_merge_listener(listener2, listener1);
    ASSERT_CALLBACK_EQUAL(inconsistent_topic,           listener2, inconsistent_topic_cb);
    ASSERT_CALLBACK_EQUAL(liveliness_lost,              listener2, liveliness_lost_cb);
    ASSERT_CALLBACK_EQUAL(offered_deadline_missed,      listener2, offered_deadline_missed_cb);
    ASSERT_CALLBACK_EQUAL(offered_incompatible_qos,     listener2, offered_incompatible_qos_cb);
    ASSERT_CALLBACK_EQUAL(data_on_readers,              listener2, data_on_readers_cb);
    ASSERT_CALLBACK_EQUAL(sample_lost,                  listener2, sample_lost_cb);
    ASSERT_CALLBACK_EQUAL(data_available,               listener2, data_available_cb);
    ASSERT_CALLBACK_EQUAL(sample_rejected,              listener2, sample_rejected_cb);
    ASSERT_CALLBACK_EQUAL(liveliness_changed,           listener2, liveliness_changed_cb);
    ASSERT_CALLBACK_EQUAL(requested_deadline_missed,    listener2, requested_deadline_missed_cb);
    ASSERT_CALLBACK_EQUAL(requested_incompatible_qos,   listener2, requested_incompatible_qos_cb);
    ASSERT_CALLBACK_EQUAL(publication_matched,          listener2, publication_matched_cb);
    ASSERT_CALLBACK_EQUAL(subscription_matched,         listener2, subscription_matched_cb);

    /* Merging listener into a full listener2 should act as a noop. */
    dds_lset_inconsistent_topic         (listener2, (dds_on_inconsistent_topic_fn)callback_dummy);
    dds_lset_liveliness_lost            (listener2, (dds_on_liveliness_lost_fn)callback_dummy);
    dds_lset_offered_deadline_missed    (listener2, (dds_on_offered_deadline_missed_fn)callback_dummy);
    dds_lset_offered_incompatible_qos   (listener2, (dds_on_offered_incompatible_qos_fn)callback_dummy);
    dds_lset_data_on_readers            (listener2, (dds_on_data_on_readers_fn)callback_dummy);
    dds_lset_sample_lost                (listener2, (dds_on_sample_lost_fn)callback_dummy);
    dds_lset_data_available             (listener2, (dds_on_data_available_fn)callback_dummy);
    dds_lset_sample_rejected            (listener2, (dds_on_sample_rejected_fn)callback_dummy);
    dds_lset_liveliness_changed         (listener2, (dds_on_liveliness_changed_fn)callback_dummy);
    dds_lset_requested_deadline_missed  (listener2, (dds_on_requested_deadline_missed_fn)callback_dummy);
    dds_lset_requested_incompatible_qos (listener2, (dds_on_requested_incompatible_qos_fn)callback_dummy);
    dds_lset_publication_matched        (listener2, (dds_on_publication_matched_fn)callback_dummy);
    dds_lset_subscription_matched       (listener2, (dds_on_subscription_matched_fn)callback_dummy);
    dds_merge_listener(listener2, listener1);
    ASSERT_CALLBACK_EQUAL(inconsistent_topic,           listener2, (dds_on_inconsistent_topic_fn)callback_dummy);
    ASSERT_CALLBACK_EQUAL(liveliness_lost,              listener2, (dds_on_liveliness_lost_fn)callback_dummy);
    ASSERT_CALLBACK_EQUAL(offered_deadline_missed,      listener2, (dds_on_offered_deadline_missed_fn)callback_dummy);
    ASSERT_CALLBACK_EQUAL(offered_incompatible_qos,     listener2, (dds_on_offered_incompatible_qos_fn)callback_dummy);
    ASSERT_CALLBACK_EQUAL(data_on_readers,              listener2, (dds_on_data_on_readers_fn)callback_dummy);
    ASSERT_CALLBACK_EQUAL(sample_lost,                  listener2, (dds_on_sample_lost_fn)callback_dummy);
    ASSERT_CALLBACK_EQUAL(data_available,               listener2, (dds_on_data_available_fn)callback_dummy);
    ASSERT_CALLBACK_EQUAL(sample_rejected,              listener2, (dds_on_sample_rejected_fn)callback_dummy);
    ASSERT_CALLBACK_EQUAL(liveliness_changed,           listener2, (dds_on_liveliness_changed_fn)callback_dummy);
    ASSERT_CALLBACK_EQUAL(requested_deadline_missed,    listener2, (dds_on_requested_deadline_missed_fn)callback_dummy);
    ASSERT_CALLBACK_EQUAL(requested_incompatible_qos,   listener2, (dds_on_requested_incompatible_qos_fn)callback_dummy);
    ASSERT_CALLBACK_EQUAL(publication_matched,          listener2, (dds_on_publication_matched_fn)callback_dummy);
    ASSERT_CALLBACK_EQUAL(subscription_matched,         listener2, (dds_on_subscription_matched_fn)callback_dummy);

    /* Using NULLs shouldn't crash and be noops. */
    dds_merge_listener(listener2, NULL);
    dds_merge_listener(NULL, listener1);
    dds_merge_listener(NULL, NULL);

    dds_delete_listener(listener1);
    dds_delete_listener(listener2);
}

CU_Test(ddsc_listener, getters_setters)
{
    /* test all individual cb get/set methods */
    dds_listener_t *listener = dds_create_listener(NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(listener);

    OS_WARNING_MSVC_OFF(6387); /* Disable SAL warning on intentional misuse of the API */ \
    TEST_GET_SET(listener, inconsistent_topic, inconsistent_topic_cb);
    TEST_GET_SET(listener, liveliness_lost, liveliness_lost_cb);
    TEST_GET_SET(listener, offered_deadline_missed, offered_deadline_missed_cb);
    TEST_GET_SET(listener, offered_incompatible_qos, offered_incompatible_qos_cb);
    TEST_GET_SET(listener, data_on_readers, data_on_readers_cb);
    TEST_GET_SET(listener, sample_lost, sample_lost_cb);
    TEST_GET_SET(listener, sample_rejected, sample_rejected_cb);
    TEST_GET_SET(listener, liveliness_changed, liveliness_changed_cb);
    TEST_GET_SET(listener, requested_deadline_missed, requested_deadline_missed_cb);
    TEST_GET_SET(listener, requested_incompatible_qos, requested_incompatible_qos_cb);
    TEST_GET_SET(listener, publication_matched, publication_matched_cb);
    TEST_GET_SET(listener, subscription_matched, subscription_matched_cb);
    TEST_GET_SET(listener, data_available, data_available_cb);
    OS_WARNING_MSVC_ON(6387);

    dds_delete_listener(listener);
}



/****************************************************************************
 * Triggering tests
 ****************************************************************************/
CU_Test(ddsc_listener, propagation, .init=init_triggering_base, .fini=fini_triggering_base)
{
    dds_listener_t *listener_par = NULL;
    dds_listener_t *listener_pub = NULL;
    dds_listener_t *listener_sub = NULL;
    uint32_t triggered;
    dds_return_t ret;
    RoundTripModule_DataType sample;
    memset (&sample, 0, sizeof (sample));

    /* Let participant be interested in data. */
    listener_par = dds_create_listener(NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(listener_par);
    dds_lset_data_on_readers(listener_par, data_on_readers_cb);
    ret = dds_set_listener(g_participant, listener_par);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    dds_delete_listener(listener_par);

    /* Let publisher be interested in publication matched. */
    listener_pub = dds_create_listener(NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(listener_pub);
    dds_lset_publication_matched(listener_pub, publication_matched_cb);
    ret = dds_set_listener(g_publisher, listener_pub);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    dds_delete_listener(listener_pub);

    /* Let subscriber be interested in subscription matched. */
    listener_sub = dds_create_listener(NULL);
    CU_ASSERT_PTR_NOT_NULL_FATAL(listener_pub);
    dds_lset_subscription_matched(listener_sub, subscription_matched_cb);
    ret = dds_set_listener(g_subscriber, listener_sub);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    dds_delete_listener(listener_sub);

    /* Create reader and writer without listeners. */
    g_reader = dds_create_reader(g_subscriber, g_topic, g_qos, NULL);
    CU_ASSERT_FATAL(g_reader > 0);
    g_writer = dds_create_writer(g_publisher, g_topic, g_qos, NULL);
    CU_ASSERT_FATAL(g_writer > 0);

    /* Publication and Subscription should be matched. */
    triggered = waitfor_cb(DDS_PUBLICATION_MATCHED_STATUS | DDS_SUBSCRIPTION_MATCHED_STATUS);
    CU_ASSERT_EQUAL_FATAL(triggered & DDS_SUBSCRIPTION_MATCHED_STATUS, DDS_SUBSCRIPTION_MATCHED_STATUS);
    CU_ASSERT_EQUAL_FATAL(triggered & DDS_PUBLICATION_MATCHED_STATUS,  DDS_PUBLICATION_MATCHED_STATUS);
    CU_ASSERT_EQUAL_FATAL(cb_writer, g_writer);
    CU_ASSERT_EQUAL_FATAL(cb_reader, g_reader);

    /* Write sample. */
    ret = dds_write(g_writer, &sample);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    /* Data on readers should be triggered with the right status. */
    triggered = waitfor_cb(DDS_DATA_ON_READERS_STATUS);
    CU_ASSERT_EQUAL_FATAL(triggered & DDS_DATA_ON_READERS_STATUS, DDS_DATA_ON_READERS_STATUS);
    CU_ASSERT_EQUAL_FATAL(cb_subscriber, g_subscriber);
    CU_ASSERT_NOT_EQUAL_FATAL(triggered & DDS_DATA_AVAILABLE_STATUS, DDS_DATA_AVAILABLE_STATUS);

    dds_delete(g_writer);
    dds_delete(g_reader);
}


CU_Test(ddsc_listener, matched, .init=init_triggering_base, .fini=fini_triggering_base)
{
    uint32_t triggered;

    /* We will basically do the same as the 'normal' init_triggering_test() and
     * fini_triggering_test() calls. It's just that we do it in a different
     * order and use the participant iso subscriber and publisher. */

    /* We are interested in matched notifications. */
    dds_lset_publication_matched(g_listener, publication_matched_cb);
    dds_lset_subscription_matched(g_listener, subscription_matched_cb);

    /* Create reader and writer with proper listeners.
     * The creation order is deliberately different from publication_matched and subscription_matched. */
    g_reader = dds_create_reader(g_participant, g_topic, g_qos, g_listener);
    CU_ASSERT_FATAL(g_reader > 0);
    g_writer = dds_create_writer(g_participant, g_topic, g_qos, g_listener);
    CU_ASSERT_FATAL(g_writer > 0);

    /* Both matched should be triggered on the right entities. */
    triggered = waitfor_cb(DDS_PUBLICATION_MATCHED_STATUS | DDS_SUBSCRIPTION_MATCHED_STATUS);
    CU_ASSERT_EQUAL_FATAL(triggered & DDS_SUBSCRIPTION_MATCHED_STATUS, DDS_SUBSCRIPTION_MATCHED_STATUS);
    CU_ASSERT_EQUAL_FATAL(triggered & DDS_PUBLICATION_MATCHED_STATUS,  DDS_PUBLICATION_MATCHED_STATUS);
    CU_ASSERT_EQUAL_FATAL(cb_writer, g_writer);
    CU_ASSERT_EQUAL_FATAL(cb_reader, g_reader);

    dds_delete(g_writer);
    dds_delete(g_reader);
}

CU_Test(ddsc_listener, publication_matched, .init=init_triggering_test, .fini=fini_triggering_test)
{
    dds_instance_handle_t reader_hdl;
    dds_return_t ret;
    uint32_t triggered;
    uint32_t status;

    /* Get reader handle that should be part of the status. */
    ret = dds_get_instance_handle(g_reader, &reader_hdl);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    /* Publication matched should be triggered with the right status. */
    triggered = waitfor_cb(DDS_PUBLICATION_MATCHED_STATUS);
    CU_ASSERT_EQUAL_FATAL(triggered & DDS_PUBLICATION_MATCHED_STATUS, DDS_PUBLICATION_MATCHED_STATUS);
    CU_ASSERT_EQUAL_FATAL(cb_writer, g_writer);
    CU_ASSERT_EQUAL_FATAL(cb_publication_matched_status.current_count, 1);
    CU_ASSERT_EQUAL_FATAL(cb_publication_matched_status.current_count_change, 1);
    CU_ASSERT_EQUAL_FATAL(cb_publication_matched_status.total_count, 1);
    CU_ASSERT_EQUAL_FATAL(cb_publication_matched_status.total_count_change, 1);
    CU_ASSERT_EQUAL_FATAL(cb_publication_matched_status.last_subscription_handle, reader_hdl);

    /* The listener should have swallowed the status. */
    ret = dds_read_status(g_writer, &status, DDS_PUBLICATION_MATCHED_STATUS);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    CU_ASSERT_EQUAL_FATAL(status, 0);

    /* Reset the trigger flags. */
    cb_called = 0;

    /* Un-match the publication by deleting the reader. */
    dds_delete(g_reader);

    /* Publication matched should be triggered with the right status. */
    triggered = waitfor_cb(DDS_PUBLICATION_MATCHED_STATUS);
    CU_ASSERT_EQUAL_FATAL(triggered & DDS_PUBLICATION_MATCHED_STATUS, DDS_PUBLICATION_MATCHED_STATUS);
    CU_ASSERT_EQUAL_FATAL(cb_writer, g_writer);
    CU_ASSERT_EQUAL_FATAL(cb_publication_matched_status.current_count, 0);
    CU_ASSERT_EQUAL_FATAL(cb_publication_matched_status.current_count_change, -1);
    CU_ASSERT_EQUAL_FATAL(cb_publication_matched_status.total_count, 1);
    CU_ASSERT_EQUAL_FATAL(cb_publication_matched_status.total_count_change, 0);
    CU_ASSERT_EQUAL_FATAL(cb_publication_matched_status.last_subscription_handle, reader_hdl);
}

CU_Test(ddsc_listener, subscription_matched, .init=init_triggering_test, .fini=fini_triggering_test)
{
    dds_instance_handle_t writer_hdl;
    dds_return_t ret;
    uint32_t triggered;
    uint32_t status;

    /* Get writer handle that should be part of the status. */
    ret = dds_get_instance_handle(g_writer, &writer_hdl);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    /* Subscription matched should be triggered with the right status. */
    triggered = waitfor_cb(DDS_SUBSCRIPTION_MATCHED_STATUS);
    CU_ASSERT_EQUAL_FATAL(triggered & DDS_SUBSCRIPTION_MATCHED_STATUS, DDS_SUBSCRIPTION_MATCHED_STATUS);
    CU_ASSERT_EQUAL_FATAL(cb_reader, g_reader);
    CU_ASSERT_EQUAL_FATAL(cb_subscription_matched_status.current_count, 1);
    CU_ASSERT_EQUAL_FATAL(cb_subscription_matched_status.current_count_change, 1);
    CU_ASSERT_EQUAL_FATAL(cb_subscription_matched_status.total_count, 1);
    CU_ASSERT_EQUAL_FATAL(cb_subscription_matched_status.total_count_change, 1);
    CU_ASSERT_EQUAL_FATAL(cb_subscription_matched_status.last_publication_handle, writer_hdl);

    /* The listener should have swallowed the status. */
    ret = dds_read_status(g_reader, &status, DDS_SUBSCRIPTION_MATCHED_STATUS);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    CU_ASSERT_EQUAL_FATAL(status, 0);

    /* Reset the trigger flags. */
    cb_called = 0;

    /* Un-match the subscription by deleting the writer. */
    dds_delete(g_writer);

    /* Subscription matched should be triggered with the right status. */
    triggered = waitfor_cb(DDS_SUBSCRIPTION_MATCHED_STATUS);
    CU_ASSERT_EQUAL_FATAL(triggered & DDS_SUBSCRIPTION_MATCHED_STATUS, DDS_SUBSCRIPTION_MATCHED_STATUS);
    CU_ASSERT_EQUAL_FATAL(cb_reader, g_reader);
    CU_ASSERT_EQUAL_FATAL(cb_subscription_matched_status.current_count, 0);
    CU_ASSERT_EQUAL_FATAL(cb_subscription_matched_status.current_count_change, -1);
    CU_ASSERT_EQUAL_FATAL(cb_subscription_matched_status.total_count, 1);
    CU_ASSERT_EQUAL_FATAL(cb_subscription_matched_status.total_count_change, 0);
    CU_ASSERT_EQUAL_FATAL(cb_subscription_matched_status.last_publication_handle, writer_hdl);
}

CU_Test(ddsc_listener, incompatible_qos, .init=init_triggering_base, .fini=fini_triggering_base)
{
    dds_return_t ret;
    uint32_t triggered;
    uint32_t status;

    /* We are interested in incompatible qos notifications. */
    dds_lset_offered_incompatible_qos(g_listener, offered_incompatible_qos_cb);
    dds_lset_requested_incompatible_qos(g_listener, requested_incompatible_qos_cb);

    /* Create reader and writer with proper listeners.
     * But create reader with persistent durability to get incompatible qos. */
    g_writer = dds_create_writer(g_participant, g_topic, g_qos, g_listener);
    CU_ASSERT_FATAL(g_writer > 0);
    dds_qset_durability (g_qos, DDS_DURABILITY_PERSISTENT);
    g_reader = dds_create_reader(g_participant, g_topic, g_qos, g_listener);
    CU_ASSERT_FATAL(g_reader > 0);

    /* Incompatible QoS should be triggered with the right status. */
    triggered = waitfor_cb(DDS_OFFERED_INCOMPATIBLE_QOS_STATUS | DDS_REQUESTED_INCOMPATIBLE_QOS_STATUS);
    CU_ASSERT_EQUAL_FATAL(triggered & DDS_OFFERED_INCOMPATIBLE_QOS_STATUS, DDS_OFFERED_INCOMPATIBLE_QOS_STATUS);
    CU_ASSERT_EQUAL_FATAL(triggered & DDS_REQUESTED_INCOMPATIBLE_QOS_STATUS, DDS_REQUESTED_INCOMPATIBLE_QOS_STATUS);
    CU_ASSERT_EQUAL_FATAL(cb_reader, g_reader);
    CU_ASSERT_EQUAL_FATAL(cb_writer, g_writer);
    CU_ASSERT_EQUAL_FATAL(cb_offered_incompatible_qos_status.total_count, 1);
    CU_ASSERT_EQUAL_FATAL(cb_offered_incompatible_qos_status.total_count_change, 1);
    CU_ASSERT_EQUAL_FATAL(cb_offered_incompatible_qos_status.last_policy_id, DDS_DURABILITY_QOS_POLICY_ID);
    CU_ASSERT_EQUAL_FATAL(cb_requested_incompatible_qos_status.total_count, 1);
    CU_ASSERT_EQUAL_FATAL(cb_requested_incompatible_qos_status.total_count_change, 1);
    CU_ASSERT_EQUAL_FATAL(cb_requested_incompatible_qos_status.last_policy_id, DDS_DURABILITY_QOS_POLICY_ID);

    /* The listener should have swallowed the status. */
    ret = dds_read_status(g_writer, &status, DDS_OFFERED_INCOMPATIBLE_QOS_STATUS);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    CU_ASSERT_EQUAL_FATAL(status, 0);
    ret = dds_read_status(g_reader, &status, DDS_REQUESTED_INCOMPATIBLE_QOS_STATUS);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    CU_ASSERT_EQUAL_FATAL(status, 0);

    dds_delete(g_writer);
    dds_delete(g_reader);
}

CU_Test(ddsc_listener, data_available, .init=init_triggering_test, .fini=fini_triggering_test)
{
    dds_return_t ret;
    uint32_t triggered;
    uint32_t status;
    RoundTripModule_DataType sample;
    memset (&sample, 0, sizeof (sample));

    /* We are interested in data available notifications. */
    dds_lset_data_available(g_listener, data_available_cb);
    ret = dds_set_listener(g_reader, g_listener);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    /* Write sample. */
    ret = dds_write(g_writer, &sample);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    /* Data available should be triggered with the right status. */
    triggered = waitfor_cb(DDS_DATA_AVAILABLE_STATUS);
    CU_ASSERT_EQUAL_FATAL(triggered & DDS_DATA_AVAILABLE_STATUS, DDS_DATA_AVAILABLE_STATUS);
    CU_ASSERT_EQUAL_FATAL(cb_reader, g_reader);

    /* The listener should have swallowed the status. */
    ret = dds_read_status(g_subscriber, &status, DDS_DATA_ON_READERS_STATUS);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    CU_ASSERT_EQUAL_FATAL(status, 0);
    ret = dds_read_status(g_reader, &status, DDS_DATA_AVAILABLE_STATUS);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    CU_ASSERT_EQUAL_FATAL(status, 0);
}

CU_Test(ddsc_listener, data_on_readers, .init=init_triggering_test, .fini=fini_triggering_test)
{
    dds_return_t ret;
    uint32_t triggered;
    uint32_t status;
    RoundTripModule_DataType sample;
    memset (&sample, 0, sizeof (sample));

    /* We are interested in data available notifications. */
    dds_lset_data_on_readers(g_listener, data_on_readers_cb);
    ret = dds_set_listener(g_subscriber, g_listener);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    /* Setting data available notifications should not 'sabotage' the on_readers call. */
    dds_lset_data_available(g_listener, data_available_cb);
    ret = dds_set_listener(g_reader, g_listener);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    /* Write sample. */
    ret = dds_write(g_writer, &sample);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    /* Data on readers should be triggered with the right status. */
    triggered = waitfor_cb(DDS_DATA_ON_READERS_STATUS);
    CU_ASSERT_EQUAL_FATAL(triggered & DDS_DATA_ON_READERS_STATUS, DDS_DATA_ON_READERS_STATUS);
    CU_ASSERT_EQUAL_FATAL(cb_subscriber, g_subscriber);
    CU_ASSERT_NOT_EQUAL_FATAL(triggered & DDS_DATA_AVAILABLE_STATUS, DDS_DATA_AVAILABLE_STATUS);

    /* The listener should have swallowed the status. */
    ret = dds_read_status(g_subscriber, &status, DDS_DATA_ON_READERS_STATUS);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    CU_ASSERT_EQUAL_FATAL(status, 0);
    ret = dds_read_status(g_reader, &status, DDS_DATA_AVAILABLE_STATUS);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    CU_ASSERT_EQUAL_FATAL(status, 0);
}


CU_Test(ddsc_listener, sample_lost, .init=init_triggering_test, .fini=fini_triggering_test)
{
    dds_return_t ret;
    uint32_t triggered;
    dds_time_t the_past;
    uint32_t status;
    RoundTripModule_DataType sample;
    memset (&sample, 0, sizeof (sample));

    /* Get a time that should be historic on all platforms.*/
    the_past = dds_time() - 1000000;

    /* We are interested in sample lost notifications. */
    dds_lset_sample_lost(g_listener, sample_lost_cb);
    ret = dds_set_listener(g_reader, g_listener);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    /* Write first sample with current timestamp. */
    ret = dds_write_ts(g_writer, &sample, dds_time());
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    /* Write second sample with older timestamp. */
    ret = dds_write_ts(g_writer, &sample, the_past);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    /* Sample lost should be triggered with the right status. */
    triggered = waitfor_cb(DDS_SAMPLE_LOST_STATUS);
    CU_ASSERT_EQUAL_FATAL(triggered & DDS_SAMPLE_LOST_STATUS, DDS_SAMPLE_LOST_STATUS);
    CU_ASSERT_EQUAL_FATAL(cb_reader, g_reader);
    CU_ASSERT_EQUAL_FATAL(cb_sample_lost_status.total_count, 1);
    CU_ASSERT_EQUAL_FATAL(cb_sample_lost_status.total_count_change, 1);

    /* The listener should have swallowed the status. */
    ret = dds_read_status(g_reader, &status, DDS_SAMPLE_LOST_STATUS);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    CU_ASSERT_EQUAL_FATAL(status, 0);
}

CU_Test(ddsc_listener, sample_rejected, .init=init_triggering_test, .fini=fini_triggering_test)
{
    dds_return_t ret;
    uint32_t triggered;
    uint32_t status;
    RoundTripModule_DataType sample;
    memset (&sample, 0, sizeof (sample));

    /* We are interested in sample rejected notifications. */
    dds_lset_sample_rejected(g_listener, sample_rejected_cb);
    ret = dds_set_listener(g_reader, g_listener);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    /* Write more than resource limits set by the reader. */
    ret = dds_write(g_writer, &sample);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    ret = dds_write(g_writer, &sample);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    ret = dds_write(g_writer, &sample);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    /* Sample lost should be triggered with the right status. */
    triggered = waitfor_cb(DDS_SAMPLE_REJECTED_STATUS);
    CU_ASSERT_EQUAL_FATAL(triggered & DDS_SAMPLE_REJECTED_STATUS, DDS_SAMPLE_REJECTED_STATUS);
    CU_ASSERT_EQUAL_FATAL(cb_reader, g_reader);
    CU_ASSERT_EQUAL_FATAL(cb_sample_rejected_status.total_count, 2);
    CU_ASSERT_EQUAL_FATAL(cb_sample_rejected_status.total_count_change, 1);

    /* The listener should have swallowed the status. */
    ret = dds_read_status(g_reader, &status, DDS_SAMPLE_REJECTED_STATUS);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    CU_ASSERT_EQUAL_FATAL(status, 0);
}

CU_Test(ddsc_listener, liveliness_changed, .init=init_triggering_test, .fini=fini_triggering_base)
{
    dds_instance_handle_t writer_hdl;
    dds_return_t ret;
    uint32_t triggered;
    uint32_t status;

    /* The init_triggering_test_byliveliness set our interest in liveliness. */

    /* Get writer handle that should be part of the status. */
    ret = dds_get_instance_handle(g_writer, &writer_hdl);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    /* Liveliness changed should be triggered with the right status. */
    triggered = waitfor_cb(DDS_LIVELINESS_CHANGED_STATUS);
    CU_ASSERT_EQUAL_FATAL(triggered & DDS_LIVELINESS_CHANGED_STATUS,  DDS_LIVELINESS_CHANGED_STATUS);
    CU_ASSERT_EQUAL_FATAL(cb_reader, g_reader);
    CU_ASSERT_EQUAL_FATAL(cb_liveliness_changed_status.alive_count, 1);
    CU_ASSERT_EQUAL_FATAL(cb_liveliness_changed_status.alive_count_change, 1);
    CU_ASSERT_EQUAL_FATAL(cb_liveliness_changed_status.not_alive_count, 0);
    CU_ASSERT_EQUAL_FATAL(cb_liveliness_changed_status.not_alive_count_change, 0);
    CU_ASSERT_EQUAL_FATAL(cb_liveliness_changed_status.last_publication_handle, writer_hdl);

    /* The listener should have swallowed the status. */
    ret = dds_read_status(g_reader, &status, DDS_LIVELINESS_CHANGED_STATUS);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    CU_ASSERT_EQUAL_FATAL(status, 0);

    /* Reset the trigger flags. */
    cb_called = 0;

    /* Change liveliness again by deleting the writer. */
    dds_delete(g_writer);

    /* Liveliness changed should be triggered with the right status. */
    triggered = waitfor_cb(DDS_LIVELINESS_CHANGED_STATUS);
    CU_ASSERT_EQUAL_FATAL(triggered & DDS_LIVELINESS_CHANGED_STATUS,  DDS_LIVELINESS_CHANGED_STATUS);
    CU_ASSERT_EQUAL_FATAL(cb_reader, g_reader);
    CU_ASSERT_EQUAL_FATAL(cb_liveliness_changed_status.alive_count, 0);
    CU_ASSERT_EQUAL_FATAL(cb_liveliness_changed_status.alive_count_change, 0);
    CU_ASSERT_EQUAL_FATAL(cb_liveliness_changed_status.not_alive_count, 1);
    CU_ASSERT_EQUAL_FATAL(cb_liveliness_changed_status.not_alive_count_change, 1);
    CU_ASSERT_EQUAL_FATAL(cb_liveliness_changed_status.last_publication_handle, writer_hdl);
}

#if 0
/* This is basically the same as the Lite test, but inconsistent topic is not triggered.
 * That is actually what I would expect, because the code doesn't seem to be the way
 * to go to test for inconsistent topic. */
Test(ddsc_listener, inconsistent_topic, .init=init_triggering_base, .fini=fini_triggering_base)
{
    dds_entity_t wr_topic;
    dds_entity_t rd_topic;
    dds_entity_t writer;
    dds_entity_t reader;
    uint32_t triggered;

    os_osInit();

    os_mutexInit(&g_mutex);
    os_condInit(&g_cond, &g_mutex);

    g_qos = dds_create_qos();
    cr_assert_not_null(g_qos, "Failed to create prerequisite g_qos");

    g_listener = dds_create_listener(NULL);
    cr_assert_not_null(g_listener, "Failed to create prerequisite g_listener");

    g_participant = dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);
    cr_assert_gt(g_participant, 0, "Failed to create prerequisite g_participant");

    /* We are interested in inconsistent topics. */
    dds_lset_inconsistent_topic(g_listener, inconsistent_topic_cb);

    wr_topic = dds_create_topic(g_participant, &RoundTripModule_DataType_desc, "WRITER_TOPIC", NULL, g_listener);
    cr_assert_gt(g_topic, 0, "Failed to create prerequisite wr_topic");

    rd_topic = dds_create_topic(g_participant, &RoundTripModule_DataType_desc, "READER_TOPIC", NULL, g_listener);
    cr_assert_gt(g_topic, 0, "Failed to create prerequisite rd_topic");

    /* Create reader and writer. */
    writer = dds_create_writer(g_participant, g_topic, NULL, NULL);
    cr_assert_gt(writer, 0, "Failed to create prerequisite writer");
    dds_qset_reliability (g_qos, DDS_RELIABILITY_RELIABLE, DDS_SECS (1));
    dds_qset_history (g_qos, DDS_HISTORY_KEEP_ALL, 0);
    reader = dds_create_reader(g_subscriber, g_topic, g_qos, NULL);
    cr_assert_gt(reader, 0, "Failed to create prerequisite reader");

    /* Inconsistent topic should be triggered with the right status. */
    triggered = waitfor_cb(DDS_INCONSISTENT_TOPIC_STATUS);
    cr_assert_eq(triggered & DDS_INCONSISTENT_TOPIC_STATUS,  DDS_INCONSISTENT_TOPIC_STATUS, "DDS_INCONSISTENT_TOPIC_STATUS not triggered");

    dds_delete(reader);
    dds_delete(writer);
    dds_delete(rd_topic);
    dds_delete(wr_topic);
    dds_delete(g_participant);

    dds_delete_listener(g_listener);
    dds_delete_qos(g_qos);
}
#endif

