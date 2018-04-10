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

#include <stdio.h>
#include <criterion/criterion.h>
#include <criterion/logging.h>

/* We are deliberately testing some bad arguments that SAL will complain about.
 * So, silence SAL regarding these issues. */
#pragma warning(push)
#pragma warning(disable: 6387 28020)

static void on_data_available(dds_entity_t reader, void* arg) {}
static void on_publication_matched(dds_entity_t writer, const dds_publication_matched_status_t status, void* arg) {}

Test(ddsc_subscriber, notify_readers) {
  dds_entity_t participant;
  dds_entity_t subscriber;
  dds_return_t ret;

  participant = dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);
  cr_assert_gt(participant, 0, "Failed to create prerequisite participant");


  subscriber = dds_create_subscriber(participant, NULL, NULL);
  cr_assert_gt(subscriber, 0, "Failed to create prerequisite subscriber");

  /* todo implement tests */
  ret = dds_notify_readers(subscriber);
  cr_expect_eq(dds_err_nr(ret), DDS_RETCODE_UNSUPPORTED, "Invalid return code %d", ret);

  dds_delete(subscriber);
  dds_delete(participant);
}

Test(ddsc_subscriber, create) {

  dds_entity_t participant;
  dds_entity_t subscriber;
  dds_listener_t *listener;
  dds_qos_t *sqos;

  participant = dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);
  cr_assert_gt(participant, 0, "Failed to create prerequisite participant");

  /*** Verify participant parameter ***/

  subscriber = dds_create_subscriber(0, NULL, NULL);
  cr_assert_eq(dds_err_nr(subscriber), DDS_RETCODE_BAD_PARAMETER, "dds_create_subscriber: invalid participant parameter");

  subscriber = dds_create_subscriber(participant, NULL, NULL);
  cr_assert_gt(subscriber, 0, "dds_create_subscriber: valid participant parameter");
  dds_delete(subscriber);

  /*** Verify qos parameter ***/

  sqos = dds_qos_create(); /* Use defaults (no user-defined policies) */
  subscriber = dds_create_subscriber(participant, sqos, NULL);
  cr_assert_gt(subscriber, 0, "dds_create_subscriber: default QoS parameter");
  dds_delete(subscriber);
  dds_qos_delete(sqos);

  sqos = dds_qos_create();
  dds_qset_destination_order(sqos, 3); /* Set invalid dest. order (ignored, not applicable for subscriber) */
  subscriber = dds_create_subscriber(participant, sqos, NULL);
  cr_assert_gt(subscriber, 0, "dds_create_subscriber: invalid non-applicable QoS parameter");
  dds_delete(subscriber);
  dds_qos_delete(sqos);

  sqos = dds_qos_create();
  dds_qset_presentation(sqos, 123, 1, 1); /* Set invalid presentation policy */
  subscriber = dds_create_subscriber(participant, sqos, NULL);
  cr_assert_eq(dds_err_nr(subscriber), DDS_RETCODE_INCONSISTENT_POLICY, "dds_create_subscriber: invalid presentation access_scope QoS parameter");
  dds_qos_delete(sqos);

  /*** Verify listener parameter ***/

  listener = dds_listener_create(NULL); /* Use defaults (all listeners unset) */
  subscriber = dds_create_subscriber(participant, NULL, listener);
  cr_assert_gt(subscriber, 0, "dds_create_subscriber: unset listeners");
  dds_delete(subscriber);
  dds_listener_delete(listener);

  listener = dds_listener_create(NULL);
  dds_lset_data_available(listener, &on_data_available); /* Set on_data_available listener */
  subscriber = dds_create_subscriber(participant, NULL, listener);
  cr_assert_gt(subscriber, 0, "dds_create_subscriber: on_data_available listener");
  dds_delete(subscriber);
  dds_listener_delete(listener);

  listener = dds_listener_create(NULL);
  dds_lset_publication_matched(listener, &on_publication_matched); /* Set on_publication_matched listener (ignored, not applicable for subscriber) */
  subscriber = dds_create_subscriber(participant, NULL, listener);
  cr_assert_gt(subscriber, 0, "dds_create_subscriber: on_publication_matched listener");
  dds_delete(subscriber);
  dds_listener_delete(listener);

  dds_delete(participant);
}

#pragma warning(pop)
