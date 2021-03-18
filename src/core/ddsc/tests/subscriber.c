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
#include "dds/dds.h"
#include "dds/ddsrt/misc.h"

#include <stdio.h>
#include "CUnit/Test.h"
#include "test_common.h"

/* We are deliberately testing some bad arguments that SAL will complain about.
 * So, silence SAL regarding these issues. */
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 6387 28020)
#endif

static void on_data_available(dds_entity_t reader, void* arg)
{
  (void)reader;
  (void)arg;
}

static void on_publication_matched(dds_entity_t writer, const dds_publication_matched_status_t status, void* arg)
{
  (void)writer;
  (void)status;
  (void)arg;
}

CU_Test(ddsc_subscriber, notify_readers) {
  dds_entity_t participant;
  dds_entity_t subscriber;
  dds_return_t ret;

  participant = dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);
  CU_ASSERT_FATAL(participant > 0);


  subscriber = dds_create_subscriber(participant, NULL, NULL);
  CU_ASSERT_FATAL(subscriber > 0);

  /* todo implement tests */
  ret = dds_notify_readers(subscriber);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_UNSUPPORTED);

  dds_delete(subscriber);
  dds_delete(participant);
}

CU_Test(ddsc_subscriber, create) {

  dds_entity_t participant;
  dds_entity_t subscriber;
  dds_listener_t *listener;
  dds_qos_t *sqos;

  participant = dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);
  CU_ASSERT_FATAL(participant > 0);

  /*** Verify participant parameter ***/

  subscriber = dds_create_subscriber(0, NULL, NULL);
  CU_ASSERT_EQUAL_FATAL(subscriber, DDS_RETCODE_BAD_PARAMETER);

  subscriber = dds_create_subscriber(participant, NULL, NULL);
  CU_ASSERT_FATAL(subscriber > 0);
  dds_delete(subscriber);

  /*** Verify qos parameter ***/

  sqos = dds_create_qos(); /* Use defaults (no user-defined policies) */
  subscriber = dds_create_subscriber(participant, sqos, NULL);
  CU_ASSERT_FATAL(subscriber > 0);
  dds_delete(subscriber);
  dds_delete_qos(sqos);

  sqos = dds_create_qos();
  DDSRT_WARNING_CLANG_OFF(assign-enum);
  dds_qset_destination_order(sqos, 3); /* Set invalid dest. order (ignored, not applicable for subscriber) */
  DDSRT_WARNING_CLANG_ON(assign-enum);
  subscriber = dds_create_subscriber(participant, sqos, NULL);
  CU_ASSERT_FATAL(subscriber > 0);
  dds_delete(subscriber);
  dds_delete_qos(sqos);

  sqos = dds_create_qos();
  DDSRT_WARNING_CLANG_OFF(assign-enum);
  dds_qset_presentation(sqos, 123, 1, 1); /* Set invalid presentation policy */
  DDSRT_WARNING_CLANG_ON(assign-enum);
  subscriber = dds_create_subscriber(participant, sqos, NULL);
  CU_ASSERT_EQUAL_FATAL(subscriber, DDS_RETCODE_BAD_PARAMETER);
  dds_delete_qos(sqos);

  /*** Verify listener parameter ***/

  listener = dds_create_listener(NULL); /* Use defaults (all listeners unset) */
  subscriber = dds_create_subscriber(participant, NULL, listener);
  CU_ASSERT_FATAL(subscriber > 0);
  dds_delete(subscriber);
  dds_delete_listener(listener);

  listener = dds_create_listener(NULL);
  dds_lset_data_available(listener, &on_data_available); /* Set on_data_available listener */
  subscriber = dds_create_subscriber(participant, NULL, listener);
  CU_ASSERT_FATAL(subscriber > 0);
  dds_delete(subscriber);
  dds_delete_listener(listener);

  listener = dds_create_listener(NULL);
  dds_lset_publication_matched(listener, &on_publication_matched); /* Set on_publication_matched listener (ignored, not applicable for subscriber) */
  subscriber = dds_create_subscriber(participant, NULL, listener);
  CU_ASSERT_FATAL(subscriber > 0);
  dds_delete(subscriber);
  dds_delete_listener(listener);

  dds_delete(participant);
}

/* This test verifies that subscribers are enabled by default */
CU_Test(ddsc_subscriber, enable_by_default) {
  dds_entity_t participant, subscriber;
  dds_return_t status, status1;
  dds_qos_t *pqos, *sqos;
  bool autoenable;

  /* create a default subscriber and check that autoenable=true */
  participant = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
  CU_ASSERT_FATAL(participant > 0);
  pqos = dds_create_qos();
  status = dds_get_qos(participant, pqos);
  CU_ASSERT_EQUAL_FATAL(status, DDS_RETCODE_OK);
  status = dds_qget_entity_factory(pqos, &autoenable);
  CU_ASSERT_EQUAL_FATAL(status, true);
  CU_ASSERT_EQUAL_FATAL(autoenable, true);
  subscriber = dds_create_subscriber(participant, NULL, NULL);
  CU_ASSERT_FATAL(subscriber > 0);
  sqos = dds_create_qos();
  status = dds_get_qos(subscriber, sqos);
  CU_ASSERT_EQUAL_FATAL(status, DDS_RETCODE_OK);
  status = dds_qget_entity_factory(sqos, &autoenable);
  CU_ASSERT_EQUAL_FATAL(status, true);
  CU_ASSERT_EQUAL_FATAL(autoenable, true);
  status1 = dds_enable (subscriber);
  CU_ASSERT_EQUAL_FATAL(status1, DDS_RETCODE_OK);
  /* check that the subscriber is really enabled
   * by trying to set an immutable qos */
  dds_qset_presentation(sqos, DDS_PRESENTATION_TOPIC, true, true);
  status = dds_set_qos(subscriber, sqos);
  CU_ASSERT_EQUAL_FATAL(status, DDS_RETCODE_IMMUTABLE_POLICY);
  status = dds_delete (participant);
  CU_ASSERT_EQUAL_FATAL(status, DDS_RETCODE_OK);
  dds_delete_qos(sqos);
  dds_delete_qos(pqos);

  /* create a participant with autoenable=false
   * check that a default subscriber is disabled and has autoenable=true */
  pqos = dds_create_qos();
  dds_qset_entity_factory(pqos, false);
  participant = dds_create_participant (DDS_DOMAIN_DEFAULT, pqos, NULL);
  CU_ASSERT_FATAL(participant > 0);
  status = dds_get_qos(participant, pqos);
  CU_ASSERT_EQUAL_FATAL(status, DDS_RETCODE_OK);
  status = dds_qget_entity_factory(pqos, &autoenable);
  CU_ASSERT_EQUAL_FATAL(status, true);
  CU_ASSERT_EQUAL_FATAL(autoenable, false);
  subscriber = dds_create_subscriber(participant, NULL, NULL);
  CU_ASSERT_FATAL(subscriber > 0);
  sqos = dds_create_qos();
  status = dds_get_qos(subscriber, sqos);
  CU_ASSERT_EQUAL_FATAL(status, DDS_RETCODE_OK);
  status = dds_qget_entity_factory(sqos, &autoenable);
  CU_ASSERT_EQUAL_FATAL(status, true);
  CU_ASSERT_EQUAL_FATAL(autoenable, true);
  /* the subscriber should be disabled because the participant
   * has autoenable=false. To check that the subscriber is really
   * disabled we try to set an immutable qos. We use the presentation
   * qos for that purpose. Setting it should succeed. */
  dds_qset_presentation(sqos, DDS_PRESENTATION_TOPIC, true, true);
  status = dds_set_qos(subscriber, sqos);
  CU_ASSERT_EQUAL_FATAL(status, DDS_RETCODE_OK);
  status = dds_delete (participant);
  CU_ASSERT_EQUAL_FATAL(status, DDS_RETCODE_OK);
  dds_delete_qos(sqos);
  dds_delete_qos(pqos);
}

/* In this test a subscriber is created in a disabled state
 * In this state immutable qos settings should still be changable.
 * After the subsciber has been enabled these qos settings cannot
 * change anymore */
CU_Test(ddsc_subscriber, disabled_subscriber_enable_later) {
  dds_entity_t participant, subscriber;
  dds_qos_t *pqos, *sqos;
  bool autoenable;
  bool status;
  dds_return_t ret;
  bool c_access, o_access;
  dds_presentation_access_scope_kind_t pr_kind;
  uint32_t mask;

  pqos = dds_create_qos();
  dds_qset_entity_factory(pqos, false);
  /* create a participant with autoenable=false */
  participant = dds_create_participant (DDS_DOMAIN_DEFAULT, pqos, NULL);
  CU_ASSERT_FATAL(participant > 0);
  /* create a default subscriber that should be disabled
   * and has autoenable=true */
  subscriber = dds_create_subscriber(participant, NULL, NULL);
  CU_ASSERT_FATAL(subscriber > 0);
  sqos = dds_create_qos();
  ret = dds_get_qos(subscriber, sqos);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
  status = dds_qget_entity_factory(sqos, &autoenable);
  CU_ASSERT_EQUAL_FATAL(status, true);
  CU_ASSERT_EQUAL_FATAL(autoenable, true);
  /* the subscriber should be disabled because the participant
   * has autoenable=false. To check that the subscriber is really
   * disabled try to set an immutable qos. We use the presentation qos
   * for that purpose. Setting it should succeed. */
  dds_qset_presentation(sqos, DDS_PRESENTATION_GROUP, true, true);
  ret = dds_set_qos(subscriber, sqos);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
  /* check that change is qos is really accepted */
  ret = dds_get_qos(subscriber, sqos);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
  dds_qget_presentation(sqos, &pr_kind,&c_access, &o_access);
  CU_ASSERT_EQUAL_FATAL(pr_kind, DDS_PRESENTATION_GROUP);
  CU_ASSERT_EQUAL_FATAL(c_access, true);
  CU_ASSERT_EQUAL_FATAL(o_access, true);
  /* the following operations should all be available on a
   * disabled entity: set_qos, get_qos, get_status_condition,
   * factory operations, get_status_changes, lookup operations
   * We already checked the dds_set_qos(), dds_get_qos() and
   * getting/setting factory settings, so let's
   * check get_status_changes() now
   */
  ret = dds_get_status_changes(subscriber, &mask);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
  /* now enable the subscriber and try again to set the
   * presentation qos. This should now result in IMMUTABLE_POLICY
   * because the subscriber is already enabled */
  ret = dds_enable(subscriber);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
  dds_qset_presentation(sqos, DDS_PRESENTATION_INSTANCE, false, false);
  ret = dds_set_qos(subscriber, sqos);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_IMMUTABLE_POLICY);
  ret = dds_delete (participant);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
  dds_delete_qos(sqos);
  dds_delete_qos(pqos);
}

CU_Test(ddsc_subscriber, delete_disabled_subscriber) {
  dds_entity_t participant, subscriber;
  dds_qos_t *pqos;
  dds_return_t ret;

  pqos = dds_create_qos();
  dds_qset_entity_factory(pqos, false);
  /* create a participant with autoenable=false */
  participant = dds_create_participant (DDS_DOMAIN_DEFAULT, pqos, NULL);
  CU_ASSERT_FATAL(participant > 0);
  /* create a default subscriber that should be disabled */
  subscriber = dds_create_subscriber(participant, NULL, NULL);
  CU_ASSERT_FATAL(subscriber > 0);
  /* delete the participant */
  ret = dds_delete(subscriber);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
  ret = dds_delete(participant);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
  dds_delete_qos(pqos);
}

CU_Test(ddsc_subscriber, not_enabled) {
  dds_entity_t participant, subscriber, reader, topic;
  dds_qos_t *pqos, *sqos, *rqos;
  dds_return_t ret;
  dds_history_kind_t hist_kind;
  int32_t hist_depth;

  pqos = dds_create_qos();
  sqos = dds_create_qos();
  rqos = dds_create_qos();
  dds_qset_entity_factory(pqos, false);
  /* create a participant with autoenable=false */
  participant = dds_create_participant (DDS_DOMAIN_DEFAULT, pqos, NULL);
  CU_ASSERT_FATAL(participant > 0);
  /* create a default subscriber that should be disabled */
  subscriber = dds_create_subscriber(participant, NULL, NULL);
  CU_ASSERT_FATAL(subscriber > 0);
  /* dds_get_qos */
  ret = dds_get_qos(subscriber, sqos);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
  dds_qset_presentation(sqos, DDS_PRESENTATION_GROUP, true, true);
  /* dds_set_qos */
  ret = dds_set_qos(subscriber, sqos);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
  /* dds_create_topic */
  topic = dds_create_topic(participant, &Space_Type1_desc, "ddsc_participant_disabled", NULL, NULL);
  CU_ASSERT_FATAL(topic > 0);
  /* dds_create_reader */
  reader = dds_create_reader(participant, topic, NULL, NULL);
  CU_ASSERT_FATAL(reader > 0);
  dds_qset_history(rqos, DDS_HISTORY_KEEP_ALL, 0);
  ret = dds_set_qos(reader, rqos);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
  dds_qget_history(rqos, &hist_kind,&hist_depth);
  CU_ASSERT_EQUAL_FATAL(hist_kind, DDS_HISTORY_KEEP_ALL);
  CU_ASSERT_EQUAL_FATAL(hist_depth, 0);
  /* dds_delete_reader */
  ret = dds_delete(reader);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
  /* dds_begin_coherent --> currently not implemented,
   * but should return DDS_RETCODE_NOT_ENABLED once implemented */
  ret = dds_begin_coherent(subscriber);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_UNSUPPORTED);
  /* dds_end_coherent --> currently not implemented,
   * but should return DDS_RETCODE_NOT_ENABLED once implemented */
  ret = dds_end_coherent(subscriber);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_UNSUPPORTED);
  /* dds_triggered */
  ret = dds_triggered(subscriber);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_NOT_ENABLED);
  /* dds_notify_readers --> currently unsupported, but once
   * supported DDS_RETCODE_NOT_ENABLED should be returned */
  ret =  dds_notify_readers(subscriber);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_UNSUPPORTED);
  /* dds_enable */
  ret = dds_enable(subscriber);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
  /* dds_delete */
  ret = dds_delete (subscriber);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
  ret = dds_delete (participant);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
  dds_delete_qos(pqos);
  dds_delete_qos(sqos);
  dds_delete_qos(rqos);
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif
