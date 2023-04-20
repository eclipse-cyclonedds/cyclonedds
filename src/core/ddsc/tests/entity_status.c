// Copyright(c) 2006 to 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <limits.h>
#include <stdlib.h>

#include "dds/dds.h"
#include "dds/ddsrt/process.h"
#include "dds/ddsrt/threads.h"

#include "test_common.h"

/****************************************************************************
 * Test globals.
 ****************************************************************************/
static dds_entity_t    participant;
static dds_entity_t    subscriber;
static dds_entity_t    publisher;
static dds_entity_t    top;
static dds_entity_t    wri;
static dds_entity_t    rea;
static dds_entity_t    waitSetwr;
static dds_entity_t    waitSetrd;
static dds_return_t    ret;
static uint32_t        sta;

static dds_qos_t      *qos;
static dds_attach_t wsresults[1];
static dds_attach_t wsresults2[2];
static size_t wsresultsize = 1U;
static size_t wsresultsize2 = 2U;
static dds_time_t waitTimeout = DDS_SECS (2);
static dds_time_t shortTimeout = DDS_MSECS (10);
static dds_publication_matched_status_t publication_matched;
static dds_subscription_matched_status_t subscription_matched;

struct reslimits {
  int32_t max_samples;
  int32_t max_instances;
  int32_t max_samples_per_instance;
};

static struct reslimits resource_limits = {1,1,1};

static dds_instance_handle_t reader_i_hdl = 0;
static dds_instance_handle_t writer_i_hdl = 0;

/****************************************************************************
 * Test initializations and teardowns.
 ****************************************************************************/

static void
init_entity_status(void)
{
    char topicName[100];

    participant = dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);
    CU_ASSERT(participant > 0);

    top = dds_create_topic(participant, &RoundTripModule_DataType_desc, create_unique_topic_name("ddsc_status_test", topicName, 100), NULL, NULL);
    CU_ASSERT(top > 0);

    qos = dds_create_qos();
    CU_ASSERT_PTR_NOT_NULL_FATAL(qos);
    dds_qset_resource_limits (qos, resource_limits.max_samples, resource_limits.max_instances, resource_limits.max_samples_per_instance);
    dds_qset_reliability(qos, DDS_RELIABILITY_BEST_EFFORT, DDS_MSECS(100));
    dds_qset_history (qos, DDS_HISTORY_KEEP_ALL, 0);
    dds_qset_destination_order (qos, DDS_DESTINATIONORDER_BY_SOURCE_TIMESTAMP);

    subscriber = dds_create_subscriber(participant, qos, NULL);
    CU_ASSERT_FATAL(subscriber > 0);
    rea = dds_create_reader(subscriber, top, qos, NULL);
    CU_ASSERT_FATAL(rea > 0);
    publisher = dds_create_publisher(participant, qos, NULL);
    CU_ASSERT_FATAL(publisher > 0);
    wri = dds_create_writer(publisher, top, qos, NULL);
    CU_ASSERT_FATAL(wri > 0);

    waitSetwr = dds_create_waitset(participant);
    ret = dds_waitset_attach (waitSetwr, wri, wri);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    waitSetrd = dds_create_waitset(participant);
    ret = dds_waitset_attach (waitSetrd, rea, rea);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    /* Get reader/writer handles because they can be tested against. */
    ret = dds_get_instance_handle(rea, &reader_i_hdl);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    ret = dds_get_instance_handle(wri, &writer_i_hdl);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
}

static void
fini_entity_status(void)
{
    dds_waitset_detach(waitSetrd, rea);
    dds_waitset_detach(waitSetwr, wri);

    dds_delete(waitSetrd);
    dds_delete(waitSetwr);
    dds_delete(wri);
    dds_delete(publisher);
    dds_delete(rea);
    dds_delete_qos(qos);
    dds_delete(top);
    dds_delete(subscriber);
    dds_delete(participant);
}

/****************************************************************************
 * Triggering tests
 ****************************************************************************/
CU_Test(ddsc_entity_status, publication_matched, .init=init_entity_status, .fini=fini_entity_status)
{
    /* We're interested in publication matched status. */
    ret = dds_set_status_mask(wri, DDS_PUBLICATION_MATCHED_STATUS);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    /* Wait for publication matched status */
    ret = dds_waitset_wait(waitSetwr, wsresults, wsresultsize, waitTimeout);
    CU_ASSERT_EQUAL_FATAL(ret, (dds_return_t)wsresultsize);
    ret = dds_get_publication_matched_status(wri, &publication_matched);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    CU_ASSERT_EQUAL_FATAL(publication_matched.current_count,        1);
    CU_ASSERT_EQUAL_FATAL(publication_matched.current_count_change, 1);
    CU_ASSERT_EQUAL_FATAL(publication_matched.total_count,          1);
    CU_ASSERT_EQUAL_FATAL(publication_matched.total_count_change,   1);
    CU_ASSERT_EQUAL_FATAL(publication_matched.last_subscription_handle, reader_i_hdl);

    /* Second call should reset the changed count. */
    ret = dds_get_publication_matched_status(wri, &publication_matched);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    CU_ASSERT_EQUAL_FATAL(publication_matched.current_count,        1);
    CU_ASSERT_EQUAL_FATAL(publication_matched.current_count_change, 0);
    CU_ASSERT_EQUAL_FATAL(publication_matched.total_count,          1);
    CU_ASSERT_EQUAL_FATAL(publication_matched.total_count_change,   0);
    CU_ASSERT_EQUAL_FATAL(publication_matched.last_subscription_handle, reader_i_hdl);

    /* Getting the status should have reset the trigger,
     * meaning that the wait should timeout. */
    ret = dds_waitset_wait(waitSetwr, wsresults, wsresultsize, shortTimeout);
    CU_ASSERT_EQUAL_FATAL(ret, 0);

    /* Un-match the publication by deleting the reader. */
    dds_delete(rea);

    /* Wait for publication matched status */
    ret = dds_waitset_wait(waitSetwr, wsresults, wsresultsize, waitTimeout);
    CU_ASSERT_EQUAL_FATAL(ret, (dds_return_t)wsresultsize);
    ret = dds_get_publication_matched_status(wri, &publication_matched);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    CU_ASSERT_EQUAL_FATAL(publication_matched.current_count,         0);
    CU_ASSERT_EQUAL_FATAL(publication_matched.current_count_change, -1);
    CU_ASSERT_EQUAL_FATAL(publication_matched.total_count,           1);
    CU_ASSERT_EQUAL_FATAL(publication_matched.total_count_change,    0);
    CU_ASSERT_EQUAL_FATAL(publication_matched.last_subscription_handle, reader_i_hdl);

    /* Second call should reset the changed count. */
    ret = dds_get_publication_matched_status(wri, &publication_matched);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    CU_ASSERT_EQUAL_FATAL(publication_matched.current_count,        0);
    CU_ASSERT_EQUAL_FATAL(publication_matched.current_count_change, 0);
    CU_ASSERT_EQUAL_FATAL(publication_matched.total_count,          1);
    CU_ASSERT_EQUAL_FATAL(publication_matched.total_count_change,   0);
    CU_ASSERT_EQUAL_FATAL(publication_matched.last_subscription_handle, reader_i_hdl);
}

CU_Test(ddsc_entity_status, subscription_matched, .init=init_entity_status, .fini=fini_entity_status)
{
    /* We're interested in subscription matched status. */
    ret = dds_set_status_mask(rea, DDS_SUBSCRIPTION_MATCHED_STATUS);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    /* Wait for subscription  matched status */
    ret = dds_waitset_wait(waitSetrd, wsresults, wsresultsize, waitTimeout);
    CU_ASSERT_EQUAL_FATAL(ret, (dds_return_t)wsresultsize);
    ret = dds_get_subscription_matched_status(rea, &subscription_matched);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    CU_ASSERT_EQUAL_FATAL(subscription_matched.current_count,        1);
    CU_ASSERT_EQUAL_FATAL(subscription_matched.current_count_change, 1);
    CU_ASSERT_EQUAL_FATAL(subscription_matched.total_count,          1);
    CU_ASSERT_EQUAL_FATAL(subscription_matched.total_count_change,   1);
    CU_ASSERT_EQUAL_FATAL(subscription_matched.last_publication_handle, writer_i_hdl);

    /* Second call should reset the changed count. */
    ret = dds_get_subscription_matched_status(rea, &subscription_matched);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    CU_ASSERT_EQUAL_FATAL(subscription_matched.current_count,        1);
    CU_ASSERT_EQUAL_FATAL(subscription_matched.current_count_change, 0);
    CU_ASSERT_EQUAL_FATAL(subscription_matched.total_count,          1);
    CU_ASSERT_EQUAL_FATAL(subscription_matched.total_count_change,   0);
    CU_ASSERT_EQUAL_FATAL(subscription_matched.last_publication_handle, writer_i_hdl);

    /* Getting the status should have reset the trigger,
     * meaning that the wait should timeout. */
    ret = dds_waitset_wait(waitSetrd, wsresults, wsresultsize, shortTimeout);
    CU_ASSERT_EQUAL_FATAL(ret, 0);

    /* Un-match the subscription by deleting the writer. */
    dds_delete(wri);

    /* Wait for subscription  matched status */
    ret = dds_waitset_wait(waitSetrd, wsresults, wsresultsize, waitTimeout);
    CU_ASSERT_EQUAL_FATAL(ret, (dds_return_t)wsresultsize);
    ret = dds_get_subscription_matched_status(rea, &subscription_matched);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    CU_ASSERT_EQUAL_FATAL(subscription_matched.current_count,         0);
    CU_ASSERT_EQUAL_FATAL(subscription_matched.current_count_change, -1);
    CU_ASSERT_EQUAL_FATAL(subscription_matched.total_count,           1);
    CU_ASSERT_EQUAL_FATAL(subscription_matched.total_count_change,    0);
    CU_ASSERT_EQUAL_FATAL(subscription_matched.last_publication_handle, writer_i_hdl);

    /* Second call should reset the changed count. */
    ret = dds_get_subscription_matched_status(rea, &subscription_matched);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    CU_ASSERT_EQUAL_FATAL(subscription_matched.current_count,        0);
    CU_ASSERT_EQUAL_FATAL(subscription_matched.current_count_change, 0);
    CU_ASSERT_EQUAL_FATAL(subscription_matched.total_count,          1);
    CU_ASSERT_EQUAL_FATAL(subscription_matched.total_count_change,   0);
    CU_ASSERT_EQUAL_FATAL(subscription_matched.last_publication_handle, writer_i_hdl);
}

CU_Test(ddsc_entity, incompatible_qos, .init=init_entity_status, .fini=fini_entity_status)
{
    dds_entity_t reader2;
    dds_requested_incompatible_qos_status_t req_incompatible_qos;
    dds_offered_incompatible_qos_status_t off_incompatible_qos;
    memset (&req_incompatible_qos, 0, sizeof (req_incompatible_qos));
    memset (&off_incompatible_qos, 0, sizeof (off_incompatible_qos));
    dds_qset_durability (qos, DDS_DURABILITY_PERSISTENT);

    /* Create a reader with persistent durability */
    reader2 = dds_create_reader(participant, top, qos, NULL);
    CU_ASSERT_FATAL(reader2 > 0);
    ret = dds_waitset_attach (waitSetrd, reader2, reader2);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    /* Get reader and writer status conditions and attach to waitset */
    ret = dds_set_status_mask(rea, DDS_REQUESTED_INCOMPATIBLE_QOS_STATUS);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    ret = dds_set_status_mask(reader2, DDS_REQUESTED_INCOMPATIBLE_QOS_STATUS);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    ret = dds_set_status_mask(wri, DDS_OFFERED_INCOMPATIBLE_QOS_STATUS);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    /* Wait for subscription requested incompatible status, which should only be
     * triggered on reader2. */
    ret = dds_waitset_wait(waitSetrd, wsresults, wsresultsize, waitTimeout);
    CU_ASSERT_EQUAL_FATAL(ret, 1);
    CU_ASSERT_EQUAL_FATAL(reader2,  (dds_entity_t)(intptr_t)wsresults[0]);

    /* Get and check the status. */
    ret = dds_get_requested_incompatible_qos_status (reader2, &req_incompatible_qos);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    CU_ASSERT_EQUAL_FATAL(req_incompatible_qos.total_count,           1);
    CU_ASSERT_EQUAL_FATAL(req_incompatible_qos.total_count_change,    1);
    CU_ASSERT_EQUAL_FATAL(req_incompatible_qos.last_policy_id, DDS_DURABILITY_QOS_POLICY_ID);

    /* Second call should reset the changed count. */
    ret = dds_get_requested_incompatible_qos_status (reader2, &req_incompatible_qos);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    CU_ASSERT_EQUAL_FATAL(req_incompatible_qos.total_count,           1);
    CU_ASSERT_EQUAL_FATAL(req_incompatible_qos.total_count_change,    0);
    CU_ASSERT_EQUAL_FATAL(req_incompatible_qos.last_policy_id, DDS_DURABILITY_QOS_POLICY_ID);

    /*Getting the status should have reset the trigger, waitset should timeout */
    ret = dds_waitset_wait(waitSetrd, wsresults, wsresultsize, shortTimeout);
    CU_ASSERT_EQUAL_FATAL(ret, 0);

    /* Wait for offered incompatible QoS status */
    ret = dds_waitset_wait(waitSetwr, wsresults, wsresultsize, waitTimeout);
    CU_ASSERT_EQUAL_FATAL(ret, (dds_return_t)wsresultsize);
    ret = dds_get_offered_incompatible_qos_status (wri, &off_incompatible_qos);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    CU_ASSERT_EQUAL_FATAL(off_incompatible_qos.total_count,           1);
    CU_ASSERT_EQUAL_FATAL(off_incompatible_qos.total_count_change,    1);
    CU_ASSERT_EQUAL_FATAL(off_incompatible_qos.last_policy_id, DDS_DURABILITY_QOS_POLICY_ID);

    /* Second call should reset the changed count. */
    ret = dds_get_offered_incompatible_qos_status (wri, &off_incompatible_qos);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    CU_ASSERT_EQUAL_FATAL(off_incompatible_qos.total_count,           1);
    CU_ASSERT_EQUAL_FATAL(off_incompatible_qos.total_count_change,    0);
    CU_ASSERT_EQUAL_FATAL(off_incompatible_qos.last_policy_id, DDS_DURABILITY_QOS_POLICY_ID);

    /*Getting the status should have reset the trigger, waitset should timeout */
    ret = dds_waitset_wait(waitSetrd, wsresults, wsresultsize, shortTimeout);
    CU_ASSERT_EQUAL_FATAL(ret, 0);

    ret = dds_waitset_detach(waitSetrd, reader2);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    dds_delete(reader2);
}

CU_Test(ddsc_entity, liveliness_changed, .init=init_entity_status, .fini=fini_entity_status)
{
    uint32_t set_mask = 0;
    dds_liveliness_changed_status_t liveliness_changed;

    ret = dds_set_status_mask(rea, DDS_LIVELINESS_CHANGED_STATUS);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    /* Get the status set */
    ret = dds_get_status_mask (rea, &set_mask);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    CU_ASSERT_EQUAL_FATAL(set_mask, DDS_LIVELINESS_CHANGED_STATUS);

    /* wait for LIVELINESS_CHANGED status */
    ret = dds_waitset_wait(waitSetrd, wsresults, wsresultsize, waitTimeout);
    CU_ASSERT_EQUAL_FATAL(ret, (dds_return_t)wsresultsize);

    ret = dds_get_liveliness_changed_status (rea, &liveliness_changed);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    CU_ASSERT_EQUAL_FATAL(liveliness_changed.alive_count,           1);
    CU_ASSERT_EQUAL_FATAL(liveliness_changed.alive_count_change,    1);
    CU_ASSERT_EQUAL_FATAL(liveliness_changed.not_alive_count,       0);
    CU_ASSERT_EQUAL_FATAL(liveliness_changed.not_alive_count_change,0);
    CU_ASSERT_EQUAL_FATAL(liveliness_changed.last_publication_handle, writer_i_hdl);

    /* Second call should reset the changed count. */
    ret = dds_get_liveliness_changed_status (rea, &liveliness_changed);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    CU_ASSERT_EQUAL_FATAL(liveliness_changed.alive_count,           1);
    CU_ASSERT_EQUAL_FATAL(liveliness_changed.alive_count_change,    0);
    CU_ASSERT_EQUAL_FATAL(liveliness_changed.not_alive_count,       0);
    CU_ASSERT_EQUAL_FATAL(liveliness_changed.not_alive_count_change,0);
    CU_ASSERT_EQUAL_FATAL(liveliness_changed.last_publication_handle, writer_i_hdl);

    /*Getting the status should have reset the trigger, waitset should timeout */
    ret = dds_waitset_wait(waitSetrd, wsresults, wsresultsize, shortTimeout);
    CU_ASSERT_EQUAL_FATAL(ret, 0);

    /* Reset writer */
    ret = dds_waitset_detach(waitSetwr, wri);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    dds_delete(wri);

    /* wait for LIVELINESS_CHANGED when a writer is deleted */
    ret = dds_waitset_wait(waitSetrd, wsresults, wsresultsize, waitTimeout);
    CU_ASSERT_EQUAL_FATAL(ret, (dds_return_t)wsresultsize);

    ret = dds_get_liveliness_changed_status (rea, &liveliness_changed);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    CU_ASSERT_EQUAL_FATAL(liveliness_changed.alive_count,           0);
    CU_ASSERT_EQUAL_FATAL(liveliness_changed.alive_count_change,   -1);
    CU_ASSERT_EQUAL_FATAL(liveliness_changed.not_alive_count,       0);
    CU_ASSERT_EQUAL_FATAL(liveliness_changed.not_alive_count_change,0);
    CU_ASSERT_EQUAL_FATAL(liveliness_changed.last_publication_handle, writer_i_hdl);

    /* Second call should reset the changed count. */
    ret = dds_get_liveliness_changed_status (rea, &liveliness_changed);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    CU_ASSERT_EQUAL_FATAL(liveliness_changed.alive_count,           0);
    CU_ASSERT_EQUAL_FATAL(liveliness_changed.alive_count_change,    0);
    CU_ASSERT_EQUAL_FATAL(liveliness_changed.not_alive_count,       0);
    CU_ASSERT_EQUAL_FATAL(liveliness_changed.not_alive_count_change,0);
    CU_ASSERT_EQUAL_FATAL(liveliness_changed.last_publication_handle, writer_i_hdl);
}

CU_Test(ddsc_entity, sample_rejected, .init=init_entity_status, .fini=fini_entity_status)
{
    dds_sample_rejected_status_t sample_rejected;

    /* Topic instance */
    RoundTripModule_DataType sample;
    memset (&sample_rejected, 0, sizeof (sample_rejected));
    memset (&sample, 0, sizeof (sample));

    ret = dds_set_status_mask(wri, DDS_PUBLICATION_MATCHED_STATUS);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    ret = dds_set_status_mask(rea, DDS_SUBSCRIPTION_MATCHED_STATUS | DDS_SAMPLE_REJECTED_STATUS);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    /* Wait for subscription matched and publication matched */
    ret = dds_waitset_wait(waitSetwr, wsresults, wsresultsize, waitTimeout);
    CU_ASSERT_EQUAL_FATAL(ret, (dds_return_t)wsresultsize);
    ret = dds_waitset_wait(waitSetrd, wsresults, wsresultsize, waitTimeout);
    CU_ASSERT_EQUAL_FATAL(ret, (dds_return_t)wsresultsize);

    ret = dds_set_status_mask(rea, DDS_SAMPLE_REJECTED_STATUS);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    /* write data - write more than resource limits set by a data reader */
    for (int i = 0; i < 5; i++)
    {
        ret = dds_write (wri, &sample);
        CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    }

    /* wait for sample rejected status */
    ret = dds_waitset_wait(waitSetrd, wsresults, wsresultsize, waitTimeout);
    CU_ASSERT_EQUAL_FATAL(ret, (dds_return_t)wsresultsize);
    ret = dds_get_sample_rejected_status (rea, &sample_rejected);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    CU_ASSERT_EQUAL_FATAL(sample_rejected.total_count,        4);
    CU_ASSERT_EQUAL_FATAL(sample_rejected.total_count_change, 4);
    CU_ASSERT_EQUAL_FATAL(sample_rejected.last_reason, DDS_REJECTED_BY_SAMPLES_LIMIT);

    /* Second call should reset the changed count. */
    ret = dds_get_sample_rejected_status (rea, &sample_rejected);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    CU_ASSERT_EQUAL_FATAL(sample_rejected.total_count,        4);
    CU_ASSERT_EQUAL_FATAL(sample_rejected.total_count_change, 0);
    CU_ASSERT_EQUAL_FATAL(sample_rejected.last_reason, DDS_REJECTED_BY_SAMPLES_LIMIT);

    /*Getting the status should have reset the trigger, waitset should timeout */
    ret = dds_waitset_wait(waitSetrd, wsresults, wsresultsize, shortTimeout);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
}

#if 0
/* This is basically the same as the Lite test, but inconsistent topic is not triggered.
 * That is actually what I would expect, because the code doesn't seem to be the way
 * to go to test for inconsistent topic. */
Test(ddsc_entity, inconsistent_topic)
{
    dds_inconsistent_topic_status_t topic_status;

    top = dds_create_topic(participant, &RoundTripModule_DataType_desc, "RoundTrip1", NULL, NULL);
    CU_ASSERT_FATAL(top > 0);

    /*Set reader topic and writer topic statuses enabled*/
    ret = dds_set_status_mask(top, DDS_INCONSISTENT_TOPIC_STATUS);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    ret = dds_set_status_mask(top, DDS_INCONSISTENT_TOPIC_STATUS);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    /* Wait for pub inconsistent topic status callback */
    ret = dds_waitset_wait(waitSetwr, wsresults, wsresultsize, waitTimeout);
    CU_ASSERT_EQUAL_FATAL(ret, (dds_return_t)wsresultsize);
    ret = dds_get_inconsistent_topic_status (top, &topic_status);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    CU_ASSERT_FATAL(topic_status.total_count > 0);

    /*Getting the status should have reset the trigger, waitset should timeout */
    status = dds_waitset_wait(waitSetrd, wsresults, wsresultsize, shortTimeout);
    CU_ASSERT_EQUAL_FATAL(status, 0, "returned %d", status);

    /* Wait for sub inconsistent topic status callback */
    ret = dds_waitset_wait(waitSetrd, wsresults, wsresultsize, waitTimeout);
    CU_ASSERT_EQUAL_FATAL(status, wsresultsize);
    ret = dds_get_inconsistent_topic_status (top, &topic_status);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    CU_ASSERT_FATAL(topic_status.total_count > 0);

    /*Getting the status should have reset the trigger, waitset should timeout */
    status = dds_waitset_wait(waitSetrd, wsresults, wsresultsize, shortTimeout);
    CU_ASSERT_EQUAL_FATAL(status, 0);

    dds_delete(top);
}
#endif

CU_Test(ddsc_entity, sample_lost, .init=init_entity_status, .fini=fini_entity_status)
{

    dds_sample_lost_status_t sample_lost;
    dds_time_t time1;
    /* Topic instance */
    RoundTripModule_DataType sample;
    memset (&sample_lost, 0, sizeof (sample_lost));
    memset (&sample, 0, sizeof (sample));

    ret = dds_set_status_mask(wri, DDS_PUBLICATION_MATCHED_STATUS);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    ret = dds_set_status_mask(rea, DDS_SUBSCRIPTION_MATCHED_STATUS | DDS_SAMPLE_LOST_STATUS);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    /* Wait for subscription matched and publication matched */
    ret = dds_waitset_wait(waitSetwr, wsresults, wsresultsize, waitTimeout);
    CU_ASSERT_EQUAL_FATAL(ret, (dds_return_t)wsresultsize);
    ret = dds_waitset_wait(waitSetrd, wsresults, wsresultsize, waitTimeout);
    CU_ASSERT_EQUAL_FATAL(ret, (dds_return_t)wsresultsize);

    ret = dds_set_status_mask(rea, DDS_SAMPLE_LOST_STATUS);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    /* get current time - subtraction ensures that this is truly historic on all platforms. */
    time1 = dds_time () - 1000000;

    /* write a sample with current timestamp */
    ret = dds_write_ts (wri, &sample, dds_time ());
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    /* second sample with older timestamp */
    ret = dds_write_ts (wri, &sample, time1);

    /* wait for sample lost status */
    ret = dds_waitset_wait(waitSetrd, wsresults, wsresultsize, waitTimeout);
    CU_ASSERT_EQUAL_FATAL(ret, (dds_return_t)wsresultsize);
    ret = dds_get_sample_lost_status (rea, &sample_lost);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    CU_ASSERT_EQUAL_FATAL(sample_lost.total_count,        1);
    CU_ASSERT_EQUAL_FATAL(sample_lost.total_count_change, 1);

    /* Second call should reset the changed count. */
    ret = dds_get_sample_lost_status (rea, &sample_lost);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    CU_ASSERT_EQUAL_FATAL(sample_lost.total_count,        1);
    CU_ASSERT_EQUAL_FATAL(sample_lost.total_count_change, 0);

    /*Getting the status should have reset the trigger, waitset should timeout */
    ret = dds_waitset_wait(waitSetrd, wsresults, wsresultsize, shortTimeout);
    CU_ASSERT_EQUAL_FATAL(ret, 0);

}

CU_Test(ddsc_entity, data_available, .init=init_entity_status, .fini=fini_entity_status)
{
    RoundTripModule_DataType sample;
    memset (&sample, 0, sizeof (sample));

    ret = dds_set_status_mask(wri, DDS_PUBLICATION_MATCHED_STATUS);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    ret = dds_set_status_mask(rea, DDS_SUBSCRIPTION_MATCHED_STATUS | DDS_DATA_AVAILABLE_STATUS);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    /* Wait for subscription and publication matched status */
    ret = dds_waitset_wait(waitSetwr, wsresults, wsresultsize, waitTimeout);
    CU_ASSERT_EQUAL_FATAL(ret, (dds_return_t)wsresultsize);
    ret = dds_waitset_wait(waitSetrd, wsresults2, wsresultsize2, waitTimeout);
    CU_ASSERT_EQUAL_FATAL(ret, (dds_return_t)wsresultsize);

    /* Write the sample */
    ret = dds_write (wri, &sample);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    /* wait for data available */
    ret = dds_take_status(rea, &sta, DDS_SUBSCRIPTION_MATCHED_STATUS);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    CU_ASSERT_EQUAL_FATAL(sta, DDS_SUBSCRIPTION_MATCHED_STATUS);

    ret = dds_waitset_wait(waitSetrd, wsresults2, wsresultsize2, waitTimeout);
    CU_ASSERT_EQUAL_FATAL(ret, (dds_return_t)wsresultsize);

    ret = dds_get_status_changes (rea, &sta);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    ret = dds_waitset_detach(waitSetrd, rea);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    dds_delete(rea);

    /* Wait for reader to be deleted */
    ret = dds_waitset_wait(waitSetwr, wsresults, wsresultsize, waitTimeout);
    CU_ASSERT_NOT_EQUAL_FATAL(ret, 0);
}

CU_Test(ddsc_entity, all_data_available, .init=init_entity_status, .fini=fini_entity_status)
{
    dds_entity_t reader2;
    dds_entity_t waitSetrd2;
    dds_sample_info_t info;

    /* Topic instance */
    RoundTripModule_DataType p_sample;
    void * s_samples[1];
    RoundTripModule_DataType s_sample;

    memset (&p_sample, 0, sizeof (p_sample));
    memset (&s_sample, 0, sizeof (s_sample));
    s_samples[0] = &s_sample;

    reader2 = dds_create_reader(subscriber, top, NULL, NULL);
    CU_ASSERT_FATAL(reader2 > 0);

    ret = dds_set_status_mask(wri, DDS_PUBLICATION_MATCHED_STATUS);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    ret = dds_set_status_mask(rea, DDS_SUBSCRIPTION_MATCHED_STATUS | DDS_DATA_AVAILABLE_STATUS);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    ret = dds_set_status_mask(reader2, DDS_SUBSCRIPTION_MATCHED_STATUS | DDS_DATA_AVAILABLE_STATUS);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    waitSetrd2 = dds_create_waitset(participant);
    ret = dds_waitset_attach (waitSetrd2, reader2, reader2);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    /* Wait for publication matched status */
    ret = dds_waitset_wait(waitSetwr, wsresults, wsresultsize, waitTimeout);
    CU_ASSERT_EQUAL_FATAL(ret, (dds_return_t)wsresultsize);

    /* Wait for subscription matched status on both readers */
    ret = dds_waitset_wait(waitSetrd, wsresults2, wsresultsize2, waitTimeout);
    CU_ASSERT_EQUAL_FATAL(ret, (dds_return_t)wsresultsize);
    ret = dds_waitset_wait(waitSetrd2, wsresults2, wsresultsize2, waitTimeout);
    CU_ASSERT_EQUAL_FATAL(ret, (dds_return_t)wsresultsize);

    ret = dds_write (wri, &p_sample);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    /* Reset the publication and subscription matched status */
    ret = dds_get_publication_matched_status(wri, NULL);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    ret = dds_take_status (rea, &sta, DDS_SUBSCRIPTION_MATCHED_STATUS | DDS_LIVELINESS_CHANGED_STATUS);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    CU_ASSERT_EQUAL_FATAL(sta, DDS_SUBSCRIPTION_MATCHED_STATUS | DDS_LIVELINESS_CHANGED_STATUS);
    ret = dds_take_status (reader2, &sta, DDS_SUBSCRIPTION_MATCHED_STATUS | DDS_LIVELINESS_CHANGED_STATUS);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    CU_ASSERT_EQUAL_FATAL(sta, DDS_SUBSCRIPTION_MATCHED_STATUS | DDS_LIVELINESS_CHANGED_STATUS);

    /* wait for data */
    ret = dds_waitset_wait(waitSetrd, wsresults2, wsresultsize2, waitTimeout);
    CU_ASSERT_NOT_EQUAL_FATAL(ret, 0);

    ret = dds_waitset_wait(waitSetrd2, wsresults2, wsresultsize2, waitTimeout);
    CU_ASSERT_NOT_EQUAL_FATAL(ret, 0);

    ret = dds_waitset_detach(waitSetrd, rea);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    ret = dds_waitset_detach(waitSetrd2, reader2);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    ret = dds_delete(waitSetrd2);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    /* Force materialized DATA_ON_READERS */
    ret = dds_waitset_attach(dds_create_waitset(participant), subscriber, 0);
    CU_ASSERT_FATAL (ret == 0);

    /* Get DATA_ON_READERS status*/
    ret = dds_get_status_changes (subscriber, &sta);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    CU_ASSERT_EQUAL_FATAL(sta, DDS_DATA_ON_READERS_STATUS);

    /* Get DATA_AVAILABLE status */
    ret = dds_get_status_changes (rea, &sta);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    CU_ASSERT_EQUAL_FATAL(sta, DDS_DATA_AVAILABLE_STATUS);

    /* Get DATA_AVAILABLE status */
    ret = dds_get_status_changes (reader2, &sta);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    CU_ASSERT_EQUAL_FATAL(sta, DDS_DATA_AVAILABLE_STATUS);

    /* Read 1 data sample from reader1 */
    ret = dds_take (rea, s_samples, &info, 1, 1);
    CU_ASSERT_EQUAL_FATAL(ret, 1);

    /* status after taking the data should be reset */
    ret = dds_get_status_changes (rea, &sta);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    CU_ASSERT_NOT_EQUAL(sta, ~DDS_DATA_AVAILABLE_STATUS);

    /* status from reader2 */
    ret = dds_get_status_changes (reader2, &sta);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    CU_ASSERT_NOT_EQUAL(sta, ~DDS_DATA_AVAILABLE_STATUS);

    /* status from subscriber */
    ret = dds_get_status_changes (subscriber, &sta);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    CU_ASSERT_EQUAL_FATAL(sta, 0);

    RoundTripModule_DataType_free (&s_sample, DDS_FREE_CONTENTS);

    dds_delete(reader2);

    /* Wait for reader to be deleted */
    ret = dds_waitset_wait(waitSetwr, wsresults, wsresultsize, waitTimeout);
    CU_ASSERT_NOT_EQUAL_FATAL(ret, 0);
}

/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_get_enabled_status, bad_param) = {
        CU_DataPoints(dds_entity_t, -2, -1, 0, INT_MAX, INT_MIN),
};
CU_Theory((dds_entity_t e), ddsc_get_enabled_status, bad_param, .init=init_entity_status, .fini=fini_entity_status)
{
    uint32_t mask;

    ret = dds_get_status_mask(e, &mask);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
}

CU_Test(ddsc_get_enabled_status, deleted_reader, .init=init_entity_status, .fini=fini_entity_status)
{
    uint32_t mask;
    dds_delete(rea);
    ret = dds_get_status_mask(rea, &mask);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
}

CU_Test(ddsc_get_enabled_status, illegal, .init=init_entity_status, .fini=fini_entity_status)
{
    uint32_t mask;
    ret = dds_get_status_mask(waitSetrd, &mask);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_ILLEGAL_OPERATION);
}

CU_TheoryDataPoints(ddsc_get_enabled_status, status_ok) = {
        CU_DataPoints(dds_entity_t *,&rea, &wri, &participant, &top, &publisher, &subscriber),
};
CU_Theory((dds_entity_t *e), ddsc_get_enabled_status, status_ok, .init=init_entity_status, .fini=fini_entity_status)
{
    uint32_t mask;
    ret = dds_get_status_mask (*e, &mask);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_set_enabled_status, bad_param) = {
        CU_DataPoints(dds_entity_t, -2, -1, 0, INT_MAX, INT_MIN),
};
CU_Theory((dds_entity_t e), ddsc_set_enabled_status, bad_param, .init=init_entity_status, .fini=fini_entity_status)
{
    ret = dds_set_status_mask(e, 0 /*mask*/);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
}

CU_Test(ddsc_set_enabled_status, deleted_reader, .init=init_entity_status, .fini=fini_entity_status)
{
    dds_delete(rea);
    ret = dds_set_status_mask(rea, 0 /*mask*/);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
}

CU_Test(ddsc_set_enabled_status, illegal, .init=init_entity_status, .fini=fini_entity_status)
{
    ret = dds_set_status_mask(waitSetrd, 0);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_ILLEGAL_OPERATION);
}

CU_TheoryDataPoints(ddsc_set_enabled_status, status_ok) = {
        CU_DataPoints(dds_entity_t *,&rea, &wri, &participant, &top, &publisher, &subscriber),
};
CU_Theory((dds_entity_t *entity), ddsc_set_enabled_status, status_ok, .init=init_entity_status, .fini=fini_entity_status)
{
    ret = dds_set_status_mask (*entity, 0);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_read_status, bad_param) = {
        CU_DataPoints(dds_entity_t, -2, -1, 0, INT_MAX, INT_MIN),
};
CU_Theory((dds_entity_t e), ddsc_read_status, bad_param, .init=init_entity_status, .fini=fini_entity_status)
{
    uint32_t status;

    ret = dds_read_status(e, &status, 0 /*mask*/);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
}

CU_Test(ddsc_read_status, deleted_reader, .init=init_entity_status, .fini=fini_entity_status)
{
    uint32_t status;
    dds_delete(rea);
    ret = dds_read_status(rea, &status, 0 /*mask*/);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
}

CU_Test(ddsc_read_status, illegal, .init=init_entity_status, .fini=fini_entity_status)
{
    uint32_t status;
    ret = dds_read_status(waitSetrd, &status, 0);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_ILLEGAL_OPERATION);
}
CU_TheoryDataPoints(ddsc_read_status, status_ok) = {
        CU_DataPoints(dds_entity_t *,&rea, &wri, &participant, &top, &publisher, &subscriber),
};
CU_Theory((dds_entity_t *e), ddsc_read_status, status_ok, .init=init_entity_status, .fini=fini_entity_status)
{
    uint32_t status;
    ret = dds_read_status (*e, &status, 0);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
}

CU_Test(ddsc_read_status, invalid_status_on_reader, .init=init_entity_status, .fini=fini_entity_status)
{
    uint32_t status;
    ret = dds_read_status(rea, &status, DDS_PUBLICATION_MATCHED_STATUS);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
}

CU_Test(ddsc_read_status, invalid_status_on_writer, .init=init_entity_status, .fini=fini_entity_status)
{
    uint32_t status;
    ret = dds_read_status(wri, &status, DDS_SUBSCRIPTION_MATCHED_STATUS);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_take_status, bad_param) = {
        CU_DataPoints(dds_entity_t, -2, -1, 0, INT_MAX, INT_MIN),
};
CU_Theory((dds_entity_t e), ddsc_take_status, bad_param, .init=init_entity_status, .fini=fini_entity_status)
{
    uint32_t status;

    ret = dds_take_status(e, &status, 0 /*mask*/);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
}

CU_Test(ddsc_take_status, deleted_reader, .init=init_entity_status, .fini=fini_entity_status)
{
    uint32_t status;
    dds_delete(rea);
    ret = dds_take_status(rea, &status, 0 /*mask*/);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
}
CU_Test(ddsc_take_status, illegal, .init=init_entity_status, .fini=fini_entity_status)
{
    uint32_t status;
    ret = dds_take_status(waitSetrd, &status, 0);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_ILLEGAL_OPERATION);
}

CU_TheoryDataPoints(ddsc_take_status, status_ok) = {
        CU_DataPoints(dds_entity_t *,&rea, &wri, &participant, &top, &publisher, &subscriber),
};
CU_Theory((dds_entity_t *e), ddsc_take_status, status_ok, .init=init_entity_status, .fini=fini_entity_status)
{
    uint32_t status;
    ret = dds_take_status (*e, &status, 0 /*mask*/);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
}

/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_get_status_changes, bad_param) = {
        CU_DataPoints(dds_entity_t, -2, -1, 0, INT_MAX, INT_MIN),
};
CU_Theory((dds_entity_t e), ddsc_get_status_changes, bad_param, .init=init_entity_status, .fini=fini_entity_status)
{
    uint32_t status;

    ret = dds_get_status_changes(e, &status);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
}

CU_Test(ddsc_get_status_changes, deleted_reader, .init=init_entity_status, .fini=fini_entity_status)
{
    uint32_t status;
    dds_delete(rea);
    ret = dds_get_status_changes(rea, &status);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
}

CU_Test(ddsc_get_status_changes, illegal, .init=init_entity_status, .fini=fini_entity_status)
{
    uint32_t status;
    ret = dds_get_status_changes(waitSetrd, &status);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_ILLEGAL_OPERATION);
}

CU_TheoryDataPoints(ddsc_get_status_changes, status_ok) = {
        CU_DataPoints(dds_entity_t *,&rea, &wri, &participant, &top, &publisher, &subscriber),
};
CU_Theory((dds_entity_t *e), ddsc_get_status_changes, status_ok, .init=init_entity_status, .fini=fini_entity_status)
{
    uint32_t status;
    ret = dds_get_status_changes (*e, &status);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_triggered, bad_param) = {
        CU_DataPoints(dds_entity_t, -2, -1, 0, INT_MAX, INT_MIN),
};
CU_Theory((dds_entity_t e), ddsc_triggered, bad_param, .init=init_entity_status, .fini=fini_entity_status)
{
    ret = dds_triggered(e);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
}

CU_Test(ddsc_triggered, deleted_reader, .init=init_entity_status, .fini=fini_entity_status)
{
    dds_delete(rea);
    ret = dds_triggered(rea);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
}

CU_TheoryDataPoints(ddsc_triggered, status_ok) = {
        CU_DataPoints(dds_entity_t *,&rea, &wri, &participant, &top, &publisher, &subscriber, &waitSetrd),
};
CU_Theory((dds_entity_t *e), ddsc_triggered, status_ok, .init=init_entity_status, .fini=fini_entity_status)
{
    ret = dds_triggered (*e);
    CU_ASSERT_FATAL(ret >= 0);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_get_inconsistent_topic_status, inconsistent_topic_status, .init=init_entity_status, .fini=fini_entity_status)
{
    dds_inconsistent_topic_status_t inconsistent_topic_status;
    ret = dds_get_inconsistent_topic_status(top, &inconsistent_topic_status);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    CU_ASSERT_EQUAL_FATAL(inconsistent_topic_status.total_count,          0);
    CU_ASSERT_EQUAL_FATAL(inconsistent_topic_status.total_count_change,   0);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_get_inconsistent_topic_status, bad_params) = {
        CU_DataPoints(dds_entity_t, -2, -1, 0, INT_MAX, INT_MIN),
};
CU_Theory((dds_entity_t topic), ddsc_get_inconsistent_topic_status, bad_params, .init=init_entity_status, .fini=fini_entity_status)
{
    dds_inconsistent_topic_status_t topic_status;

    ret = dds_get_inconsistent_topic_status(topic, &topic_status);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_get_inconsistent_topic_status, null, .init=init_entity_status, .fini=fini_entity_status)
{
    dds_set_status_mask(top, 0);
    ret = dds_get_inconsistent_topic_status(top, NULL);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_get_inconsistent_topic_status, non_topics) = {
        CU_DataPoints(dds_entity_t*, &rea, &wri, &participant),
};
CU_Theory((dds_entity_t *topic), ddsc_get_inconsistent_topic_status, non_topics, .init=init_entity_status, .fini=fini_entity_status)
{
    ret = dds_get_inconsistent_topic_status(*topic, NULL);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_ILLEGAL_OPERATION);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_get_inconsistent_topic_status, deleted_topic, .init=init_entity_status, .fini=fini_entity_status)
{
    dds_delete(top);
    ret = dds_get_inconsistent_topic_status(top, NULL);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_get_publication_matched_status, bad_params) = {
        CU_DataPoints(dds_entity_t, -2, -1, 0, INT_MAX, INT_MIN),
};
CU_Theory((dds_entity_t writer), ddsc_get_publication_matched_status, bad_params, .init=init_entity_status, .fini=fini_entity_status)
{
    dds_publication_matched_status_t status;

    ret = dds_get_publication_matched_status(writer, &status);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_get_publication_matched_status, null, .init=init_entity_status, .fini=fini_entity_status)
{
    dds_set_status_mask(wri, 0);
    ret = dds_get_publication_matched_status(wri, NULL);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_get_publication_matched_status, non_writers) = {
        CU_DataPoints(dds_entity_t*, &rea, &top, &participant),
};
CU_Theory((dds_entity_t *writer), ddsc_get_publication_matched_status, non_writers, .init=init_entity_status, .fini=fini_entity_status)
{
    ret = dds_get_publication_matched_status(*writer, NULL);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_ILLEGAL_OPERATION);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_get_publication_matched_status, deleted_writer, .init=init_entity_status, .fini=fini_entity_status)
{
    dds_delete(wri);
    ret = dds_get_publication_matched_status(wri, NULL);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_get_liveliness_lost_status, liveliness_lost_status, .init=init_entity_status, .fini=fini_entity_status)
{
    dds_liveliness_lost_status_t liveliness_lost_status;
    ret = dds_get_liveliness_lost_status(wri, &liveliness_lost_status);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    CU_ASSERT_EQUAL_FATAL(liveliness_lost_status.total_count,        0);
    CU_ASSERT_EQUAL_FATAL(liveliness_lost_status.total_count_change, 0);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_get_liveliness_lost_status, bad_params) = {
        CU_DataPoints(dds_entity_t, -2, -1, 0, INT_MAX, INT_MIN),
};
CU_Theory((dds_entity_t writer), ddsc_get_liveliness_lost_status, bad_params, .init=init_entity_status, .fini=fini_entity_status)
{
    dds_liveliness_lost_status_t status;

    ret = dds_get_liveliness_lost_status(writer, &status);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_get_liveliness_lost_status, null, .init=init_entity_status, .fini=fini_entity_status)
{
    dds_set_status_mask(wri, 0);
    ret = dds_get_liveliness_lost_status(wri, NULL);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_get_liveliness_lost_status, non_writers) = {
        CU_DataPoints(dds_entity_t*, &rea, &top, &participant),
};
CU_Theory((dds_entity_t *writer), ddsc_get_liveliness_lost_status, non_writers, .init=init_entity_status, .fini=fini_entity_status)
{
    ret = dds_get_liveliness_lost_status(*writer, NULL);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_ILLEGAL_OPERATION);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_get_liveliness_lost_status, deleted_writer, .init=init_entity_status, .fini=fini_entity_status)
{
    dds_delete(wri);
    ret = dds_get_liveliness_lost_status(wri, NULL);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_get_offered_deadline_missed_status, offered_deadline_missed_status, .init=init_entity_status, .fini=fini_entity_status)
{
    dds_offered_deadline_missed_status_t offered_deadline_missed_status;
    ret = dds_get_offered_deadline_missed_status(wri, &offered_deadline_missed_status);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    CU_ASSERT_EQUAL_FATAL(offered_deadline_missed_status.total_count,            0);
    CU_ASSERT_EQUAL_FATAL(offered_deadline_missed_status.total_count_change,     0);
    CU_ASSERT_EQUAL_FATAL(offered_deadline_missed_status.last_instance_handle,   0);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_get_offered_deadline_missed_status, bad_params) = {
        CU_DataPoints(dds_entity_t, -2, -1, 0, INT_MAX, INT_MIN),
};
CU_Theory((dds_entity_t writer), ddsc_get_offered_deadline_missed_status, bad_params, .init=init_entity_status, .fini=fini_entity_status)
{
    dds_offered_deadline_missed_status_t status;

    ret = dds_get_offered_deadline_missed_status(writer, &status);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_get_offered_deadline_missed_status, null, .init=init_entity_status, .fini=fini_entity_status)
{
    dds_set_status_mask(wri, 0);
    ret = dds_get_offered_deadline_missed_status(wri, NULL);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_get_offered_deadline_missed_status, non_writers) = {
        CU_DataPoints(dds_entity_t*, &rea, &top, &participant),
};
CU_Theory((dds_entity_t *writer), ddsc_get_offered_deadline_missed_status, non_writers, .init=init_entity_status, .fini=fini_entity_status)
{
    ret = dds_get_offered_deadline_missed_status(*writer, NULL);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_ILLEGAL_OPERATION);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_get_offered_deadline_missed_status, deleted_writer, .init=init_entity_status, .fini=fini_entity_status)
{
    dds_delete(wri);
    ret = dds_get_offered_deadline_missed_status(wri, NULL);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_get_offered_incompatible_qos_status, bad_params) = {
        CU_DataPoints(dds_entity_t, -2, -1, 0, INT_MAX, INT_MIN),
};
CU_Theory((dds_entity_t writer), ddsc_get_offered_incompatible_qos_status, bad_params, .init=init_entity_status, .fini=fini_entity_status)
{
    dds_offered_incompatible_qos_status_t status;

    ret = dds_get_offered_incompatible_qos_status(writer, &status);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_get_offered_incompatible_qos_status, null, .init=init_entity_status, .fini=fini_entity_status)
{
    dds_set_status_mask(wri, 0);
    ret = dds_get_offered_incompatible_qos_status(wri, NULL);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_get_offered_incompatible_qos_status, non_writers) = {
        CU_DataPoints(dds_entity_t*, &rea, &top, &participant),
};
CU_Theory((dds_entity_t *writer), ddsc_get_offered_incompatible_qos_status, non_writers, .init=init_entity_status, .fini=fini_entity_status)
{
    ret = dds_get_offered_incompatible_qos_status(*writer, NULL);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_ILLEGAL_OPERATION);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_get_offered_incompatible_qos_status, deleted_writer, .init=init_entity_status, .fini=fini_entity_status)
{
    dds_delete(wri);
    ret = dds_get_offered_incompatible_qos_status(wri, NULL);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_get_subscription_matched_status, bad_params) = {
        CU_DataPoints(dds_entity_t, -2, -1, 0, INT_MAX, INT_MIN),
};
CU_Theory((dds_entity_t reader), ddsc_get_subscription_matched_status, bad_params, .init=init_entity_status, .fini=fini_entity_status)
{
    dds_subscription_matched_status_t status;

    ret = dds_get_subscription_matched_status(reader, &status);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_get_subscription_matched_status, null, .init=init_entity_status, .fini=fini_entity_status)
{
    dds_set_status_mask(rea, 0);
    ret = dds_get_subscription_matched_status(rea, NULL);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_get_subscription_matched_status, non_readers) = {
        CU_DataPoints(dds_entity_t*, &wri, &top, &participant),
};
CU_Theory((dds_entity_t *reader), ddsc_get_subscription_matched_status, non_readers, .init=init_entity_status, .fini=fini_entity_status)
{
    ret = dds_get_subscription_matched_status(*reader, NULL);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_ILLEGAL_OPERATION);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_get_subscription_matched_status, deleted_reader, .init=init_entity_status, .fini=fini_entity_status)
{
    dds_delete(rea);
    ret = dds_get_subscription_matched_status(rea, NULL);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_get_liveliness_changed_status, bad_params) = {
        CU_DataPoints(dds_entity_t, -2, -1, 0, INT_MAX, INT_MIN),
};
CU_Theory((dds_entity_t reader), ddsc_get_liveliness_changed_status, bad_params, .init=init_entity_status, .fini=fini_entity_status)
{
    dds_liveliness_changed_status_t status;

    ret = dds_get_liveliness_changed_status(reader, &status);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_get_liveliness_changed_status, null, .init=init_entity_status, .fini=fini_entity_status)
{
    dds_set_status_mask(rea, 0);
    ret = dds_get_liveliness_changed_status(rea, NULL);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_get_liveliness_changed_status, non_readers) = {
        CU_DataPoints(dds_entity_t*, &wri, &top, &participant),
};
CU_Theory((dds_entity_t *reader), ddsc_get_liveliness_changed_status, non_readers, .init=init_entity_status, .fini=fini_entity_status)
{
    ret = dds_get_liveliness_changed_status(*reader, NULL);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_ILLEGAL_OPERATION);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_get_liveliness_changed_status, deleted_reader, .init=init_entity_status, .fini=fini_entity_status)
{
    dds_delete(rea);
    ret = dds_get_liveliness_changed_status(rea, NULL);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_get_sample_rejected_status, bad_params) = {
        CU_DataPoints(dds_entity_t, -2, -1, 0, INT_MAX, INT_MIN),
};
CU_Theory((dds_entity_t reader), ddsc_get_sample_rejected_status, bad_params, .init=init_entity_status, .fini=fini_entity_status)
{
    dds_sample_rejected_status_t status;

    ret = dds_get_sample_rejected_status(reader, &status);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_get_sample_rejected_status, null, .init=init_entity_status, .fini=fini_entity_status)
{
    dds_set_status_mask(rea, 0);
    ret = dds_get_sample_rejected_status(rea, NULL);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_get_sample_rejected_status, non_readers) = {
        CU_DataPoints(dds_entity_t*, &wri, &top, &participant),
};
CU_Theory((dds_entity_t *reader), ddsc_get_sample_rejected_status, non_readers, .init=init_entity_status, .fini=fini_entity_status)
{
    ret = dds_get_sample_rejected_status(*reader, NULL);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_ILLEGAL_OPERATION);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_get_sample_rejected_status, deleted_reader, .init=init_entity_status, .fini=fini_entity_status)
{
    dds_delete(rea);
    ret = dds_get_sample_rejected_status(rea, NULL);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_get_sample_lost_status, bad_params) = {
        CU_DataPoints(dds_entity_t, -2, -1, 0, INT_MAX, INT_MIN),
};
CU_Theory((dds_entity_t reader), ddsc_get_sample_lost_status, bad_params, .init=init_entity_status, .fini=fini_entity_status)
{
    dds_sample_lost_status_t status;

    ret = dds_get_sample_lost_status(reader, &status);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_get_sample_lost_status, null, .init=init_entity_status, .fini=fini_entity_status)
{
    dds_set_status_mask(rea, 0);
    ret = dds_get_sample_lost_status(rea, NULL);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_get_sample_lost_status, non_readers) = {
        CU_DataPoints(dds_entity_t*, &wri, &top, &participant),
};
CU_Theory((dds_entity_t *reader), ddsc_get_sample_lost_status, non_readers, .init=init_entity_status, .fini=fini_entity_status)
{
    ret = dds_get_sample_lost_status(*reader, NULL);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_ILLEGAL_OPERATION);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_get_sample_lost_status, deleted_reader, .init=init_entity_status, .fini=fini_entity_status)
{
    dds_delete(rea);
    ret = dds_get_sample_lost_status(rea, NULL);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_get_requested_deadline_missed_status, requested_deadline_missed_status, .init=init_entity_status, .fini=fini_entity_status)
{
    dds_requested_deadline_missed_status_t requested_deadline_missed_status;
    ret = dds_get_requested_deadline_missed_status(rea, &requested_deadline_missed_status);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    CU_ASSERT_EQUAL_FATAL(requested_deadline_missed_status.total_count,           0);
    CU_ASSERT_EQUAL_FATAL(requested_deadline_missed_status.total_count_change,    0);
    CU_ASSERT_EQUAL_FATAL(requested_deadline_missed_status.last_instance_handle,  DDS_HANDLE_NIL);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_get_requested_deadline_missed_status, bad_params) = {
        CU_DataPoints(dds_entity_t, -2, -1, 0, INT_MAX, INT_MIN),
};
CU_Theory((dds_entity_t reader), ddsc_get_requested_deadline_missed_status, bad_params, .init=init_entity_status, .fini=fini_entity_status)
{
    dds_requested_deadline_missed_status_t status;

    ret = dds_get_requested_deadline_missed_status(reader, &status);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_get_requested_deadline_missed_status, null, .init=init_entity_status, .fini=fini_entity_status)
{
    dds_set_status_mask(rea, 0);
    ret = dds_get_requested_deadline_missed_status(rea, NULL);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_get_requested_deadline_missed_status, non_readers) = {
        CU_DataPoints(dds_entity_t*, &wri, &top, &participant),
};
CU_Theory((dds_entity_t *reader), ddsc_get_requested_deadline_missed_status, non_readers, .init=init_entity_status, .fini=fini_entity_status)
{
    ret = dds_get_requested_deadline_missed_status(*reader, NULL);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_ILLEGAL_OPERATION);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_get_requested_deadline_missed_status, deleted_reader, .init=init_entity_status, .fini=fini_entity_status)
{
    dds_delete(rea);
    ret = dds_get_requested_deadline_missed_status(rea, NULL);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_get_requested_incompatible_qos_status, bad_params) = {
        CU_DataPoints(dds_entity_t, -2, -1, 0, INT_MAX, INT_MIN),
};
CU_Theory((dds_entity_t reader), ddsc_get_requested_incompatible_qos_status, bad_params, .init=init_entity_status, .fini=fini_entity_status)
{
    dds_requested_incompatible_qos_status_t status;

    ret = dds_get_requested_incompatible_qos_status(reader, &status);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_get_requested_incompatible_qos_status, null, .init=init_entity_status, .fini=fini_entity_status)
{
    dds_set_status_mask(rea, 0);
    ret = dds_get_requested_incompatible_qos_status(rea, NULL);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_TheoryDataPoints(ddsc_get_requested_incompatible_qos_status, non_readers) = {
        CU_DataPoints(dds_entity_t*, &wri, &top, &participant),
};
CU_Theory((dds_entity_t *reader), ddsc_get_requested_incompatible_qos_status, non_readers, .init=init_entity_status, .fini=fini_entity_status)
{
    ret = dds_get_requested_incompatible_qos_status(*reader, NULL);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_ILLEGAL_OPERATION);
}
/*************************************************************************************************/

/*************************************************************************************************/
CU_Test(ddsc_get_requested_incompatible_qos_status, deleted_reader, .init=init_entity_status, .fini=fini_entity_status)
{
    dds_delete(rea);
    ret = dds_get_requested_incompatible_qos_status(rea, NULL);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_BAD_PARAMETER);
}
/*************************************************************************************************/

/*************************************************************************************************/
