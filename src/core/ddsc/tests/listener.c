// Copyright(c) 2006 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <stdlib.h>

#include "dds/dds.h"
#include "dds/ddsrt/misc.h"
#include "dds/ddsrt/sync.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/environ.h"
#include "test_common.h"
#include "test_oneliner.h"

#define DEFINE_STATUS_CALLBACK(name, NAME, kind) \
  static void name##_cb (dds_entity_t kind, const dds_##name##_status_t status, void *arg) \
  { \
    (void) arg; \
    (void) kind; \
    (void) status; \
  }

DEFINE_STATUS_CALLBACK (inconsistent_topic, INCONSISTENT_TOPIC, topic)
DEFINE_STATUS_CALLBACK (liveliness_changed, LIVELINESS_CHANGED, reader)
DEFINE_STATUS_CALLBACK (liveliness_lost, LIVELINESS_LOST, writer)
DEFINE_STATUS_CALLBACK (offered_deadline_missed, OFFERED_DEADLINE_MISSED, writer)
DEFINE_STATUS_CALLBACK (offered_incompatible_qos, OFFERED_INCOMPATIBLE_QOS, writer)
DEFINE_STATUS_CALLBACK (publication_matched, PUBLICATION_MATCHED, writer)
DEFINE_STATUS_CALLBACK (requested_deadline_missed, REQUESTED_DEADLINE_MISSED, reader)
DEFINE_STATUS_CALLBACK (requested_incompatible_qos, REQUESTED_INCOMPATIBLE_QOS, reader)
DEFINE_STATUS_CALLBACK (sample_lost, SAMPLE_LOST, reader)
DEFINE_STATUS_CALLBACK (sample_rejected, SAMPLE_REJECTED, reader)
DEFINE_STATUS_CALLBACK (subscription_matched, SUBSCRIPTION_MATCHED, reader)

static void data_on_readers_cb (dds_entity_t subscriber, void *arg)
{
  (void) subscriber;
  (void) arg;
}

static void data_available_cb (dds_entity_t reader, void *arg)
{
  (void) reader;
  (void) arg;
}

static void dummy_cb (void)
{
  // Used as a listener function in checking merging of listeners,
  // and for that purpose, casting it to whatever function type is
  // required is ok.  It is not supposed to ever be called.
  abort ();
}

#undef DEFINE_STATUS_CALLBACK

/**************************************************
 ****                                          ****
 ****  create/delete/get/set/copy/merge/reset  ****
 ****                                          ****
 **************************************************/

static void set_all_const (dds_listener_t *l, void (*c) (void))
{
  dds_lset_data_available (l, (dds_on_data_available_fn) c);
  dds_lset_data_on_readers (l, (dds_on_data_on_readers_fn) c);
  dds_lset_inconsistent_topic (l, (dds_on_inconsistent_topic_fn) c);
  dds_lset_liveliness_changed (l, (dds_on_liveliness_changed_fn) c);
  dds_lset_liveliness_lost (l, (dds_on_liveliness_lost_fn) c);
  dds_lset_offered_deadline_missed (l, (dds_on_offered_deadline_missed_fn) c);
  dds_lset_offered_incompatible_qos (l, (dds_on_offered_incompatible_qos_fn) c);
  dds_lset_publication_matched (l, (dds_on_publication_matched_fn) c);
  dds_lset_requested_deadline_missed (l, (dds_on_requested_deadline_missed_fn) c);
  dds_lset_requested_incompatible_qos (l, (dds_on_requested_incompatible_qos_fn) c);
  dds_lset_sample_lost (l, (dds_on_sample_lost_fn) c);
  dds_lset_sample_rejected (l, (dds_on_sample_rejected_fn) c);
  dds_lset_subscription_matched (l, (dds_on_subscription_matched_fn) c);
}

static void set_all (dds_listener_t *l)
{
  dds_lset_data_available (l, data_available_cb);
  dds_lset_data_on_readers (l, data_on_readers_cb);
  dds_lset_inconsistent_topic (l, inconsistent_topic_cb);
  dds_lset_liveliness_changed (l, liveliness_changed_cb);
  dds_lset_liveliness_lost (l, liveliness_lost_cb);
  dds_lset_offered_deadline_missed (l, offered_deadline_missed_cb);
  dds_lset_offered_incompatible_qos (l, offered_incompatible_qos_cb);
  dds_lset_publication_matched (l, publication_matched_cb);
  dds_lset_requested_deadline_missed (l, requested_deadline_missed_cb);
  dds_lset_requested_incompatible_qos (l, requested_incompatible_qos_cb);
  dds_lset_sample_lost (l, sample_lost_cb);
  dds_lset_sample_rejected (l, sample_rejected_cb);
  dds_lset_subscription_matched (l, subscription_matched_cb);
}

#define ASSERT_CALLBACK_EQUAL(fntype, listener, expected) \
  do { \
    dds_on_##fntype##_fn cb; \
    dds_lget_##fntype(listener, &cb); \
    CU_ASSERT_EQUAL(cb, expected); \
  } while (0)

static void check_all_const (const dds_listener_t *l, void (*c) (void))
{
  ASSERT_CALLBACK_EQUAL (data_available, l, (dds_on_data_available_fn) c);
  ASSERT_CALLBACK_EQUAL (data_on_readers, l, (dds_on_data_on_readers_fn) c);
  ASSERT_CALLBACK_EQUAL (inconsistent_topic, l, (dds_on_inconsistent_topic_fn) c);
  ASSERT_CALLBACK_EQUAL (liveliness_changed, l, (dds_on_liveliness_changed_fn) c);
  ASSERT_CALLBACK_EQUAL (liveliness_lost, l, (dds_on_liveliness_lost_fn) c);
  ASSERT_CALLBACK_EQUAL (offered_deadline_missed, l, (dds_on_offered_deadline_missed_fn) c);
  ASSERT_CALLBACK_EQUAL (offered_incompatible_qos, l, (dds_on_offered_incompatible_qos_fn) c);
  ASSERT_CALLBACK_EQUAL (publication_matched, l, (dds_on_publication_matched_fn) c);
  ASSERT_CALLBACK_EQUAL (requested_deadline_missed, l, (dds_on_requested_deadline_missed_fn) c);
  ASSERT_CALLBACK_EQUAL (requested_incompatible_qos, l, (dds_on_requested_incompatible_qos_fn) c);
  ASSERT_CALLBACK_EQUAL (sample_lost, l, (dds_on_sample_lost_fn) c);
  ASSERT_CALLBACK_EQUAL (sample_rejected, l, (dds_on_sample_rejected_fn) c);
  ASSERT_CALLBACK_EQUAL (subscription_matched, l, (dds_on_subscription_matched_fn) c);
}

static void check_all (const dds_listener_t *l)
{
  ASSERT_CALLBACK_EQUAL (data_available, l, data_available_cb);
  ASSERT_CALLBACK_EQUAL (data_on_readers, l, data_on_readers_cb);
  ASSERT_CALLBACK_EQUAL (inconsistent_topic, l, inconsistent_topic_cb);
  ASSERT_CALLBACK_EQUAL (liveliness_changed, l, liveliness_changed_cb);
  ASSERT_CALLBACK_EQUAL (liveliness_lost, l, liveliness_lost_cb);
  ASSERT_CALLBACK_EQUAL (offered_deadline_missed, l, offered_deadline_missed_cb);
  ASSERT_CALLBACK_EQUAL (offered_incompatible_qos, l, offered_incompatible_qos_cb);
  ASSERT_CALLBACK_EQUAL (publication_matched, l, publication_matched_cb);
  ASSERT_CALLBACK_EQUAL (requested_deadline_missed, l, requested_deadline_missed_cb);
  ASSERT_CALLBACK_EQUAL (requested_incompatible_qos, l, requested_incompatible_qos_cb);
  ASSERT_CALLBACK_EQUAL (sample_lost, l, sample_lost_cb);
  ASSERT_CALLBACK_EQUAL (sample_rejected, l, sample_rejected_cb);
  ASSERT_CALLBACK_EQUAL (subscription_matched, l, subscription_matched_cb);
}

CU_Test (ddsc_listener, create_and_delete)
{
  dds_listener_t *listener = dds_create_listener (NULL);
  CU_ASSERT_PTR_NOT_NULL_FATAL (listener);
  check_all_const (listener, 0);
  dds_delete_listener (listener);

  // check delete_listeners handles a null pointer gracefully
  dds_delete_listener (NULL);
}

CU_Test (ddsc_listener, reset)
{
  dds_listener_t *listener = dds_create_listener (NULL);
  CU_ASSERT_PTR_NOT_NULL_FATAL (listener);

  set_all (listener);

  // all callbacks should revert to default after reset
  dds_reset_listener (listener);
  check_all_const (listener, 0);
  dds_delete_listener (listener);

  // check reset_listeners handles a null pointer gracefully
  dds_reset_listener (NULL);
}

CU_Test (ddsc_listener, copy)
{
  dds_listener_t *listener1 = dds_create_listener (NULL);
  CU_ASSERT_PTR_NOT_NULL_FATAL (listener1);
  set_all (listener1);

  dds_listener_t *listener2 = dds_create_listener (NULL);
  CU_ASSERT_PTR_NOT_NULL_FATAL (listener2);
  dds_copy_listener (listener2, listener1);
  check_all (listener2);

  // Calling copy with NULL should not crash and be noops
  dds_copy_listener (listener2, NULL);
  dds_copy_listener (NULL, listener1);
  dds_copy_listener (NULL, NULL);

  dds_delete_listener (listener1);
  dds_delete_listener (listener2);
}

CU_Test (ddsc_listener, merge)
{
  dds_listener_t *listener1 = dds_create_listener (NULL);
  CU_ASSERT_PTR_NOT_NULL_FATAL (listener1);
  set_all (listener1);

  // Merging listener1 into empty listener2 be like a copy
  dds_listener_t *listener2 = dds_create_listener (NULL);
  CU_ASSERT_PTR_NOT_NULL_FATAL (listener2);
  dds_merge_listener (listener2, listener1);
  check_all (listener2);

  // Merging listener into a full listener2 should not overwrite anything
  set_all_const (listener2, dummy_cb);
  dds_merge_listener (listener2, listener1);
  check_all_const (listener2, dummy_cb);

  // Using NULLs shouldn't crash and be noops
  dds_merge_listener (listener2, NULL);
  dds_merge_listener (NULL, listener1);
  dds_merge_listener (NULL, NULL);

  dds_delete_listener (listener1);
  dds_delete_listener (listener2);
}

CU_Test(ddsc_listener, getters_setters)
{
  // test all individual cb get/set methods
  dds_listener_t *listener = dds_create_listener (NULL);
  CU_ASSERT_PTR_NOT_NULL_FATAL (listener);

#define TEST_GET_SET(listener, fntype, cb) \
  do { \
    dds_on_##fntype##_fn dummy = NULL; \
    /* Initially expect DDS_LUNSET on a newly created listener */ \
    ASSERT_CALLBACK_EQUAL (fntype, listener, 0); \
    /* Using listener or callback NULL, shouldn't crash and be noop */ \
    dds_lset_##fntype (NULL, NULL); \
    dds_lget_##fntype (NULL, NULL); \
    dds_lget_##fntype (listener, NULL); \
    dds_lget_##fntype (NULL, &dummy);  \
    CU_ASSERT_EQUAL_FATAL (dummy, NULL); \
    /* Set to NULL, get to confirm it succeeds */ \
    dds_lset_##fntype (listener, NULL); \
    ASSERT_CALLBACK_EQUAL (fntype, listener, NULL); \
    /* Set to a proper cb method, get to confirm it succeeds */ \
    dds_lset_##fntype (listener, cb); \
    ASSERT_CALLBACK_EQUAL (fntype, listener, cb); \
  } while (0)
  TEST_GET_SET (listener, data_available, data_available_cb);
  TEST_GET_SET (listener, data_on_readers, data_on_readers_cb);
  TEST_GET_SET (listener, inconsistent_topic, inconsistent_topic_cb);
  TEST_GET_SET (listener, liveliness_changed, liveliness_changed_cb);
  TEST_GET_SET (listener, liveliness_lost, liveliness_lost_cb);
  TEST_GET_SET (listener, offered_deadline_missed, offered_deadline_missed_cb);
  TEST_GET_SET (listener, offered_incompatible_qos, offered_incompatible_qos_cb);
  TEST_GET_SET (listener, publication_matched, publication_matched_cb);
  TEST_GET_SET (listener, requested_deadline_missed, requested_deadline_missed_cb);
  TEST_GET_SET (listener, requested_incompatible_qos, requested_incompatible_qos_cb);
  TEST_GET_SET (listener, sample_lost, sample_lost_cb);
  TEST_GET_SET (listener, sample_rejected, sample_rejected_cb);
  TEST_GET_SET (listener, subscription_matched, subscription_matched_cb);
#undef TEST_GET_SET

  dds_delete_listener (listener);
}

#undef ASSERT_CALLBACK_EQUAL

// Use no_shm variant because the use of shared memory may result in asynchronous delivery
// of data published by a local reader/writer and at least some of these tests are written
// on the assumption that it is always synchronous
#define dotest(ops) CU_ASSERT_FATAL (test_oneliner_no_shm (ops) > 0)

CU_Test (ddsc_listener, propagation)
{
  // data-on-readers set on a participant at creation time must not trigger for
  // the readers for DCPSPublication and DCPSSubscription: those events must be
  // invisible for the test logic to work reliably. Installing a dummy listener
  // for it on the reader should prevent that from happening
  dotest ("da dor lc sm P ; ?!dor ?!da ?!sm ?!lc");
  // writing data should trigger data-available unless data-on-readers is set
  dotest ("da lc sm P ; r ; wr w 0 ; ?da r ?sm r ?lc r");
  dotest ("da dor lc sm P ; r ; wr w 0 ; ?!da ; ?dor R ?sm r ?lc r");
  // setting listeners after entity creation should work, too
  dotest ("P W R ; dor P pm W sm R ; r w ; ?sm r ?pm w ; wr w 0 ; ?dor R ; ?!da");
}

CU_Test (ddsc_listener, matched)
{
  // publication & subscription matched must both trigger; note: reader/writer matching inside
  // a process is synchronous, no need to check everywhere
  dotest ("sm r pm w ?pm w ?sm r");
  // across the network it should work just as well (matching happens on different threads for
  // remote & local entity creation, so it is meaningfully different test)
  dotest ("sm r pm w' ?pm w' ?sm r");

  // Disconnect + reconnect; "deaf P" means the disconnect is asymmetrical: P no longer observes P'
  // but P' still observes P.  If r did not ack the data before losing connectivity, w' will hold
  // the data and it will be re-delivered after reconnecting, depending on QoS settings (the "..."
  // allows for extra samples) and whether the instance was taken or not
  //
  // If r did ack the data, w will drop it and it can't be delivered.  If there is another r'' that
  // did not ack, r will still not get the data because the writer determines that it was ack'd
  // already and it won't retransmit.
  // FIXME: this differs from what the spec says should happen, maybe it should be changed?
  // (It is a fall-out from changes to make sure a volatile reader doesn't get historical data, but
  // it could be handled differently.)
  //
  // Waiting for an acknowledgement therefore makes sense (and in the other runs, a 0.3s sleep
  // kind-a solves the problem of not known exactly how many events there will be: it means at
  // least one event has been observed, and behaviour of Cyclone in a simple case like this means
  // the full retransmit request will be replied to with a single packet, and that therefore the
  // likelihood of the retransmitted data arriving within a window of 0.3s is very high.  (Where
  // 0.3s is an attempt to pick a duration on the long side of what's needed and short enough not
  // to delay things much.)
  dotest ("sm da r pm w' ?sm r ?pm w' ;" // matched reader/writer pair
          " wr w' 1   ; ?da r take{(1,0,0)} r ?ack w' ;" // wait-for-acks => writer drops data
          " deaf! P   ; ?sm(1,0,0,-1,w') r ?da r take{d1} r ; wr w' 2 ;" // write lost on "wire"
          " hearing! P; ?sm(2,1,1,1,w') r  ?da r sleep 0.3 take{(2,0,0)} r ; ?!pm");
  dotest ("sm da r pm w' ; ?sm r ?pm w' ;"
          " r'' ?pm w' deaf! P'' ;" // with second reader: reader is deaf so won't ACK
          " wr w' 1   ; ?da r take{(1,0,0)} r ?ack(r) w' ;" // wait for ack from r' (not r'')
          " deaf! P   ; ?sm(1,0,0,-1,w') r ?da r take{d1} r ; wr w' 2 ;" // write lost on "wire"
          " hearing! P; ?sm(2,1,1,1,w') r  ?da r sleep 0.3 take{(2,0,0)} r ; ?!pm");
  // same without taking the "dispose" after disconnect
  // sample 1 will be delivered anew
  dotest ("sm da r pm w' ; ?sm r ?pm w' ; wr w' 1 ; ?da r take{(1,0,0)} r ;"
          " deaf! P ; ?sm(1,0,0,-1,w') r ?da r ; wr w' 2 ;"
          " hearing! P ; ?sm(2,1,1,1,w') r ?da r sleep 0.3 take{d1,(2,0,0)} r ; ?!pm");

  // if a volatile writer loses the reader temporarily, the data won't show up
  dotest ("sm da r pm w' ; ?sm r ?pm w' ; wr w' 1 ; ?da r read{(1,0,0)} r ;"
          " deaf! P' ; ?!sm ?!da ?pm(1,0,0,-1,r) w' ; wr w' 2 ;"
          " hearing! P' ; ?!sm ?pm(2,1,1,1,r) w' ?!da ; wr w' 3 ;"
          " ?da r sleep 0.3 read{s(1,0,0),f(3,0,0)} r");
  // if a transient-local writer loses the reader temporarily, what data
  // has been published during the disconnect must still show up; delete
  // writer, &c. checks nothing else showed up afterward
  // - first: durability service history depth 1: 2nd write of 2 pushes
  //   the 1st write of it out of the history and only 2 samples arrive
  // - second: d.s. keep-all: both writes are kept and 3 samples arrive
  dotest ("sm da r(d=tl) pm w'(d=tl,h=1,ds=0/1) ; ?sm r ?pm w' ;"
          " wr w' 1 ; ?da r read{(1,0,0)} r ;"
          " deaf! P' ; ?pm(1,0,0,-1,r) w' ; wr w' 2 wr w' 2 ;"
          " hearing! P' ; ?pm(2,1,1,1,r) w' ; wr w' 3 ;"
          " ?da(2) r read{s(1,0,0),f(2,0,0),f(3,0,0)} r ;"
          " -w' ?sm r ?da r read(3,3) r");
  dotest ("sm da r(d=tl) pm w'(d=tl,h=1,ds=0/all) ; ?sm r ?pm w' ;"
          " wr w' 1 ; ?da r read{(1,0,0)} r ;"
          " deaf! P' ; ?pm(1,0,0,-1,r) w' ; wr w' 2 wr w' 2 ;"
          " hearing! P' ; ?pm(2,1,1,1,r) w' ; wr w' 3 ;"
          " ?da(3) r read{s(1,0,0),f(2,0,0),f(2,0,0),f(3,0,0)} r ;"
          " -w' ?sm r ?da r read(4,3) r");
}

CU_Test (ddsc_listener, publication_matched)
{
  // regardless of order of creation, the writer should see one reader come & then go
  dotest ("sm r pm w ; ?pm(1,1,1,1,r) w ?sm r ; -r ; ?pm(1,0,0,-1,r) w");
  dotest ("pm w sm r ; ?pm(1,1,1,1,r) w ?sm r ; -r ; ?pm(1,0,0,-1,r) w");

  // regardless of order of creation, the writer should see one reader come & then go, also
  // when a second reader introduced
  dotest ("sm r pm w ; ?pm(1,1,1,1,r) w ?sm r ; t ?pm(2,1,2,1,t) w ; -r ; ?pm(2,0,1,-1,r) w");
  dotest ("pm w sm r ; ?pm(1,1,1,1,r) w ?sm r ; t ?pm(2,1,2,1,t) w ; -t ; ?pm(2,0,1,-1,t) w");

  // same with 2 domains
  dotest ("sm r pm w' ; ?pm(1,1,1,1,r) w' ?sm r ; -r ; ?pm(1,0,0,-1,r) w'");
  dotest ("pm w sm r' ; ?pm(1,1,1,1,r') w ?sm r' ; -r' ; ?pm(1,0,0,-1,r') w");

  dotest ("sm r pm w' ; ?pm(1,1,1,1,r) w' ?sm r ; t ?pm(2,1,2,1,t) w' ; -r ; ?pm(2,0,1,-1,r) w'");
  dotest ("pm w sm r' ; ?pm(1,1,1,1,r') w ?sm r' ; t ?pm(2,1,2,1,t) w ; -t ; ?pm(2,0,1,-1,t) w");
  dotest ("sm r pm w' ; ?pm(1,1,1,1,r) w' ?sm r ; t' ?pm(2,1,2,1,t') w' ; -r ; ?pm(2,0,1,-1,r) w'");
  dotest ("pm w sm r' ; ?pm(1,1,1,1,r') w ?sm r' ; t' ?pm(2,1,2,1,t') w ; -t' ; ?pm(2,0,1,-1,t') w");
}

CU_Test (ddsc_listener, subscription_matched)
{
  // regardless of order of creation, the reader should see one writer come & then go
  dotest ("sm r pm w ; ?pm w ?sm(1,1,1,1,w) r ; -w ; ?sm(1,0,0,-1,w) r");
  dotest ("pm w sm r ; ?pm w ?sm(1,1,1,1,w) r ; -w ; ?sm(1,0,0,-1,w) r");

  // regardless of order of creation, the reader should see one writer come & then go, also
  // when a second writer is introduced
  dotest ("sm r pm w ; ?pm w ?sm(1,1,1,1,w) r ; x ?sm(2,1,2,1,x) r ; -w ; ?sm(2,0,1,-1,w) r");
  dotest ("pm w sm r ; ?pm w ?sm(1,1,1,1,w) r ; x ?sm(2,1,2,1,x) r ; -x ; ?sm(2,0,1,-1,x) r");

  // same with 2 domains
  dotest ("sm r pm w' ; ?pm w' ?sm(1,1,1,1,w') r ; -w' ; ?sm(1,0,0,-1,w') r");
  dotest ("pm w sm r' ; ?pm w ?sm(1,1,1,1,w) r' ; -w ; ?sm(1,0,0,-1,w) r'");

  dotest ("sm r pm w' ; ?pm w' ?sm(1,1,1,1,w') r ; x ?sm(2,1,2,1,x) r ; -w' ; ?sm(2,0,1,-1,w') r");
  dotest ("pm w sm r' ; ?pm w ?sm(1,1,1,1,w) r' ; x ?sm(2,1,2,1,x) r' ; -x ; ?sm(2,0,1,-1,x) r'");
  dotest ("sm r pm w' ; ?pm w' ?sm(1,1,1,1,w') r ; x' ?sm(2,1,2,1,x') r ; -w' ; ?sm(2,0,1,-1,w') r");
  dotest ("pm w sm r' ; ?pm w ?sm(1,1,1,1,w) r' ; x' ?sm(2,1,2,1,x') r' ; -x' ; ?sm(2,0,1,-1,x') r'");
}

CU_Test (ddsc_listener, incompatible_qos)
{
  // best-effort writer & reliable reader: both must trigger incompatible QoS event
  dotest ("oiq w(r=be) riq r ; ?oiq(1,1,r) w ?riq(1,1,r) r");
  dotest ("riq r oiq w(r=be) ; ?oiq(1,1,r) w ?riq(1,1,r) r");
  dotest ("oiq w(o=x) riq r ; ?oiq(1,1,o) w ?riq(1,1,o) r");
  dotest ("riq r oiq w(o=x) ; ?oiq(1,1,o) w ?riq(1,1,o) r");
}

CU_Test (ddsc_listener, data_available)
{
  // data available on reader (+ absence of data-on-readers)
  dotest ("da sm r pm w ?pm w ?sm r wr w 0 ?da r ?!dor");
  // data available set on subscriber (+ absence of data-on-readers)
  dotest ("da R sm r pm w ?pm w ?sm r wr w 0 ?da r ?!dor");
  // data available set on participant (+ absence of data-on-readers)
  dotest ("da P sm r pm w ?pm w ?sm r wr w 0 ?da r ?!dor");

  // non-auto-dispose, transient-local: disconnect => no_writers, reconnect => alive (using invalid samples)
  // the invalid sample has the source time stamp of the latest update -- one wonders whether that is wise?
  dotest ("da r(d=tl) ?pm w'(d=tl,ad=n) ; wr w' (1,2,3)@1.1 ?da r read{fan(1,2,3)w'} r ;"
          " deaf! P ; ?da r read{suo(1,2,3)w'@1.1,fuo1w'@1.1} r ;"
          " hearing! P ; ?da r read{sao(1,2,3)w'@1.1,fao1w'@1.1} r");
}

CU_Test (ddsc_listener, data_available_delete_writer)
{
  // unmatching a writer that didn't read anything has no visible effect on RHC
  // subscription-matched event is generated synchronously, so "?sm r" doesn't
  // really add anything (it'd be different if there are two domain instances)
  dotest ("da sm r w ; -w ?sm r ?!da ; take(0,0) r");
  // after writing: auto-dispose should always trigger data available, an invalid
  // sample needs to show up if there isn't an unread sample to use instead
  dotest ("da r w ; wr w 0 ?da r ; -w ?da r ; take(1,0) r");
  dotest ("da r w ; wr w 0 ?da r ; read(1,0) r ; -w ?da r ; take(1,1) r");
  dotest ("da r w ; wr w 0 ?da r ; take(1,0) r ; -w ?da r ; take(0,1) r");
  // same with two writers (no point in doing this also with two domains)
  dotest ("da r w x ; -w ?!da -x ?!da ; take(0,0) r");
  dotest ("da r w x ; wr w 0 ?da r ; -x ?!da ; -w ?da r ; take(1,0) r");
  dotest ("da r w x ; wr w 0 ?da r ; -w ?da r ; take(1,0) r ; -x ?!da ; take(0,0) r");
  dotest ("da r w x ; wr w 0 wr x 0 ?da r ; -w ?!da ; take(2,0) r ; -x ?da r ; take(0,1) r");
  dotest ("da r w x ; wr w 0 wr x 0 ?da r ; read(2,0) r ; -w ?!da -x ?da r ; take(2,1) r");
  dotest ("da r w x ; wr w 0 wr x 0 ?da r ; read(2,0) r ; -x ?!da -w ?da r ; take(2,1) r");
  dotest ("da r w x ; wr w 0 read(1,0) r ; wr x 0 ?da r ; -w ?!da -x ?da r ; take(2,0) r");
  dotest ("da r w x ; wr w 0 read(1,0) r ; wr x 0 ?da r ; -x ?!da -w ?da r ; take(2,0) r");
  dotest ("da r w x ; wr w 0 read(1,0) r ; wr x 0 ?da r ; read(2,0) r ; -w ?!da -x ?da r ; take(2,1) r");
  dotest ("da r w x ; wr w 0 read(1,0) r ; wr x 0 ?da r ; read(2,0) r ; -x ?!da -w ?da r ; take(2,1) r");
  dotest ("da r w x ; wr w 0 wr x 0 ?da r ; take(2,0) r ; -w ?!da -x ?da r ; take(0,1) r");
  dotest ("da r w x ; wr w 0 wr x 0 ?da r ; take(2,0) r ; -x ?!da -w ?da r ; take(0,1) r");
}

CU_Test (ddsc_listener, data_available_delete_writer_disposed)
{
  // same as data_available_delete_writer, but now with the instance disposed first
  dotest ("da r w ; wr w 0 disp w 0 ?da r ; -w ?!da");
  dotest ("da r w ; wr w 0 disp w 0 ?da r ; read(1,0) r ; -w ?!da");
  dotest ("da r w ; wr w 0 disp w 0 ?da r ; take(1,0) r ; -w ?!da");

  dotest ("da r w x ; wr w 0 ?da r ; read(1,0) r ; disp w 0 ?da r ; read(1,1) r ; -w ?!da -x ?!da");
  dotest ("da r w x ; wr w 0 ?da r ; take(1,0) r ; disp w 0 ?da r ; take(0,1) r ; -w ?!da -x ?!da");
  dotest ("da r w x ; wr w 0 ?da r ; read(1,0) r ; disp w 0 ?da r ; read(1,1) r ; -x ?!da -w ?!da");
  dotest ("da r w x ; wr w 0 ?da r ; take(1,0) r ; disp w 0 ?da r ; take(0,1) r ; -x ?!da -w ?!da");

  dotest ("da r w x ; wr w 0 ?da r ; read(1,0) r ; disp x 0 ?da r ; read(1,1) r ; -w ?!da -x ?!da");
  dotest ("da r w x ; wr w 0 ?da r ; take(1,0) r ; disp x 0 ?da r ; take(0,1) r ; -w ?!da -x ?!da");
  dotest ("da r w x ; wr w 0 ?da r ; read(1,0) r ; disp x 0 ?da r ; read(1,1) r ; -x ?!da -w ?!da");
  dotest ("da r w x ; wr w 0 ?da r ; take(1,0) r ; disp x 0 ?da r ; take(0,1) r ; -x ?!da -w ?!da");
}

CU_Test (ddsc_listener, data_on_readers)
{
  // data on readers wins from data available
  dotest ("dor R da r ; wr w 0 ; ?dor R ?!da");
  dotest ("dor P da r ; wr w 0 ; ?dor R ?!da");
}

CU_Test (ddsc_listener, sample_lost)
{
  // FIXME: figure out what really constitutes a "lost sample"
  dotest ("sl r ; wr w 0@0 ?!sl ; wr w 0@-1 ?sl(1,1) r");
}

CU_Test (ddsc_listener, sample_rejected)
{
  // FIXME: rejection counts with retries?
  // reliable: expect timeout on the write when max samples has been reached
  // invalid samples don't count towards resource limits, so dispose should
  // not be blocked
  dotest ("sr r(rl=1) ; wr w 0 wrfail w 0 wrfail w 0 ; ?sr r");
  dotest ("sr r(rl=1) ; wr w 0 wrfail w 0 ; read(1,0) r ; disp w 0 ; read(1,1) r ; ?sr r");

  // best-effort: writes should succeed despite not delivering the data adding
  // the data in the RHC, also check number of samples rejected
  dotest ("sr r(rl=1,r=be) ; wr w(r=be) 0 wr w 0 wr w 0 ; ?sr(2,1,s) r");
  dotest ("sr r(rl=1,r=be) ; wr w(r=be) 0 wr w 0 ; read(1,0) r ; disp w 0 ; read(1,1) r ; ?sr(1,1,s) r");
}

CU_Test (ddsc_listener, liveliness_changed)
{
  // liveliness changed should trigger along with matching
  dotest ("pm w lc sm r ; ?pm w ?sm r ; ?lc(1,0,1,0,w) r ; -w ; ?lc(0,0,-1,0,w) r");
  dotest ("pm w lc sm r' ; ?pm w ?sm r' ; ?lc(1,0,1,0,w) r' ; -w ; ?lc(0,0,-1,0,w) r'");
}
