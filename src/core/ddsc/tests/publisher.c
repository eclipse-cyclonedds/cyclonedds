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
#include <criterion/criterion.h>
#include <criterion/logging.h>

/* We are deliberately testing some bad arguments that SAL will complain about.
 * So, silence SAL regarding these issues. */
#pragma warning(push)
#pragma warning(disable: 28020)


#define cr_assert_status_eq(s1, s2, ...) cr_assert_eq(dds_err_nr(s1), s2, __VA_ARGS__)

/* Dummy callback */
static void data_available_cb(dds_entity_t reader, void* arg) {}


Test(ddsc_publisher, create)
{
  const char *singlePartitions[] = { "partition" };
  const char *multiplePartitions[] = { "partition1", "partition2" };
  const char *duplicatePartitions[] = { "partition", "partition" };

  dds_entity_t participant;
  dds_entity_t publisher, publisher1;
  dds_listener_t *listener;
  dds_qos_t *qos;

  /* Use NULL participant */
  publisher = dds_create_publisher(0, NULL, NULL);
  cr_assert_eq(dds_err_nr(publisher), DDS_RETCODE_BAD_PARAMETER, "dds_create_publisher(NULL,NULL,NULL)");

  participant = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
  cr_assert_gt(participant, 0, "dds_create_participant(DDS_DOMAIN_DEFAULT,NULL,NULL)");

  /* Use non-null participant */
  publisher = dds_create_publisher(participant, NULL, NULL);
  cr_assert_gt(publisher, 0, "dds_create_publisher(participant,NULL,NULL)");

  /* Use entity that is not a participant */
  publisher1 = dds_create_publisher(publisher, NULL, NULL);
  cr_assert_eq(dds_err_nr(publisher1), DDS_RETCODE_ILLEGAL_OPERATION, "dds_create_publisher(publisher,NULL,NULL)");
  dds_delete(publisher);

  /* Create a non-null qos */
  qos = dds_qos_create();
  cr_assert_neq(qos, NULL, "dds_qos_create()");

  /* Use qos without partition; in that case the default partition should be used */
  publisher = dds_create_publisher(participant, qos, NULL);
  cr_assert_gt(publisher, 0, "dds_create_publisher(participant,qos,NULL) where qos with default partition");
  dds_delete(publisher);

/* Somehow, the compiler thinks the char arrays might not be zero-terminated... */
#pragma warning(push)
#pragma warning(disable: 6054)

  /* Use qos with single partition */
  dds_qset_partition (qos, 1, singlePartitions);
  publisher = dds_create_publisher(participant, qos, NULL);
  cr_assert_gt(publisher, 0, "dds_create_publisher(participant,qos,NULL) where qos with single partition");
  dds_delete(publisher);

  /* Use qos with multiple partitions */
  dds_qset_partition (qos, 2, multiplePartitions);
  publisher = dds_create_publisher(participant, qos, NULL);
  cr_assert_gt(publisher, 0, "dds_create_publisher(participant,qos,NULL) where qos with multiple partitions");
  dds_delete(publisher);

  /* Use qos with multiple partitions */
  dds_qset_partition (qos, 2, duplicatePartitions);
  publisher = dds_create_publisher(participant, qos, NULL);
  cr_assert_gt(publisher, 0, "dds_create_publisher(participant,qos,NULL) where qos with duplicate partitions");
  dds_delete(publisher);

#pragma warning(pop)

  /* Use listener(NULL) */
  listener = dds_listener_create(NULL);
  cr_assert_neq(listener, NULL, "dds_listener_create(NULL)");
  publisher = dds_create_publisher(participant, NULL, listener);
  cr_assert_gt(publisher, 0, "dds_create_publisher(participant,NULL,listener(NULL))");
  dds_delete(publisher);

  dds_listener_reset(listener);

  /* Use listener for data_available */
  dds_lset_data_available(listener, NULL);
  publisher = dds_create_publisher(participant, NULL, listener);
  cr_assert_gt(publisher, 0, "dds_create_publisher(participant,NULL,listener) with dds_lset_data_available(listener, NULL)");
  dds_delete(publisher);

  dds_listener_reset(listener);

  /* Use DDS_LUNSET for data_available */
  dds_lset_data_available(listener, DDS_LUNSET);
  publisher = dds_create_publisher(participant, NULL, listener);
  cr_assert_gt(publisher, 0, "dds_create_publisher(participant,NULL,listener) with dds_lset_data_available(listener, DDS_LUNSET)");
  dds_delete(publisher);

  dds_listener_reset(listener);

  /* Use callback for data_available */
  dds_lset_data_available(listener, data_available_cb);
  publisher = dds_create_publisher(participant, NULL, listener);
  cr_assert_gt(publisher, 0, "dds_create_publisher(participant,NULL,listener) with dds_lset_data_available(listener, data_available_cb)");
  dds_delete(publisher);

  /* Use both qos setting and callback listener */
  dds_lset_data_available(listener, data_available_cb);
  publisher = dds_create_publisher(participant, qos, listener);
  cr_assert_gt(publisher, 0, "dds_create_publisher(participant,qos,listener) with dds_lset_data_available(listener, data_available_cb)");
  dds_delete(publisher);

  dds_listener_delete(listener);
  dds_qos_delete(qos);
  dds_delete (participant);
}

Test(ddsc_publisher, suspend_resume)
{

  dds_entity_t participant, publisher;
  dds_return_t status;

  /* Suspend a 0 publisher */
  status = dds_suspend(0);
  cr_assert_status_eq(status, DDS_RETCODE_BAD_PARAMETER, "dds_suspend(NULL)");

  /* Resume a 0 publisher */
  status = dds_resume(0);
  cr_assert_status_eq(status, DDS_RETCODE_BAD_PARAMETER, "dds_resume(NULL)");

  /* Uae dds_suspend on something else than a publisher */
  participant = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
  cr_assert_gt(participant, 0, "dds_create_participant(DDS_DOMAIN_DEFAULT,NULL,NULL)");
  status = dds_suspend(participant);
  cr_assert_status_eq(status, DDS_RETCODE_BAD_PARAMETER, "dds_suspend(participant)");

  /* Use dds_resume on something else than a publisher */
  status = dds_resume(participant);
  cr_assert_status_eq(status, DDS_RETCODE_BAD_PARAMETER, "dds_resume(participant)");

  /* Use dds_resume without calling dds_suspend */
  publisher = dds_create_publisher(participant, NULL, NULL);
  cr_assert_gt(publisher, 0, "dds_create_publisher(participant,NULL,NULL)");
  status = dds_resume(publisher); /* Should be precondition not met? */
  cr_assert_status_eq(status, DDS_RETCODE_UNSUPPORTED, "dds_resume(publisher) without prior suspend");

  /* Use dds_suspend on non-null publisher */
  status = dds_suspend(publisher);
  cr_assert_status_eq(status, DDS_RETCODE_UNSUPPORTED, "dds_suspend(publisher)");

  /* Use dds_resume on non-null publisher */
  status = dds_resume(publisher);
  cr_assert_status_eq(status, DDS_RETCODE_UNSUPPORTED, "dds_resume(publisher)");

  dds_delete(publisher);
  dds_delete(participant);

  return;
}

Test(ddsc_publisher, wait_for_acks)
{
  dds_entity_t participant, publisher;
  dds_return_t status;
  dds_duration_t zeroSec = ((dds_duration_t)DDS_SECS(0));
  dds_duration_t oneSec = ((dds_duration_t)DDS_SECS(1));
  dds_duration_t minusOneSec = ((dds_duration_t)DDS_SECS(-1));

  /* Wait_for_acks on 0 publisher or writer and minusOneSec timeout */
  status = dds_wait_for_acks(0, minusOneSec);
  cr_assert_status_eq(status, DDS_RETCODE_BAD_PARAMETER, "dds_wait_for_acks(NULL,-1)");

  /* Wait_for_acks on NULL publisher or writer and zeroSec timeout */
  status = dds_wait_for_acks(0, zeroSec);
  cr_assert_status_eq(status, DDS_RETCODE_BAD_PARAMETER, "dds_wait_for_acks(NULL,0)");

  /* wait_for_acks on NULL publisher or writer and oneSec timeout */
  status = dds_wait_for_acks(0, oneSec);
  cr_assert_status_eq(status, DDS_RETCODE_BAD_PARAMETER, "dds_wait_for_acks(NULL,1)");

  /* wait_for_acks on NULL publisher or writer and DDS_INFINITE timeout */
  status = dds_wait_for_acks(0, DDS_INFINITY);
  cr_assert_status_eq(status, DDS_RETCODE_BAD_PARAMETER, "dds_wait_for_acks(NULL,DDS_INFINITY)");

  participant = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
  cr_assert_gt(participant, 0, "dds_create_participant(DDS_DOMAIN_DEFAULT,NULL,NULL)");

  /* Wait_for_acks on participant and minusOneSec timeout */
  status = dds_wait_for_acks(participant, minusOneSec);
  cr_assert_status_eq(status, DDS_RETCODE_BAD_PARAMETER, "dds_wait_for_acks(participant,-1)");

  /* Wait_for_acks on participant and zeroSec timeout */
  status = dds_wait_for_acks(participant, zeroSec);
  cr_assert_status_eq(status, DDS_RETCODE_BAD_PARAMETER, "dds_wait_for_acks(participant,0)");

  /* Wait_for_acks on participant and oneSec timeout */
  status = dds_wait_for_acks(participant, oneSec);
  cr_assert_status_eq(status, DDS_RETCODE_BAD_PARAMETER, "dds_wait_for_acks(participant,1)");

  /* Wait_for_acks on participant and DDS_INFINITE timeout */
  status = dds_wait_for_acks(participant, DDS_INFINITY);
  cr_assert_status_eq(status, DDS_RETCODE_BAD_PARAMETER, "dds_wait_for_acks(participant,DDS_INFINITY)");

  publisher = dds_create_publisher(participant, NULL, NULL);
  cr_assert_gt(publisher, 0, "dds_create_publisher(participant,NULL,NULL)");

  /* Wait_for_acks on publisher and minusOneSec timeout */
  status = dds_wait_for_acks(publisher, minusOneSec);
  cr_assert_status_eq(status, DDS_RETCODE_UNSUPPORTED, "dds_wait_for_acks(publisher,-1)");

  /* Wait_for_acks on publisher and zeroSec timeout */
  status = dds_wait_for_acks(publisher, zeroSec);
  cr_assert_status_eq(status, DDS_RETCODE_UNSUPPORTED, "dds_wait_for_acks(publisher,0)");

  /* Wait_for_acks on publisher and oneSec timeout */
  status = dds_wait_for_acks(publisher, oneSec);
  cr_assert_status_eq(status, DDS_RETCODE_UNSUPPORTED, "dds_wait_for_acks(publisher,1)");

  /* Wait_for_acks on publisher and DDS_INFINITE timeout */
  status = dds_wait_for_acks(publisher, DDS_INFINITY);
  cr_assert_status_eq(status, DDS_RETCODE_UNSUPPORTED, "dds_wait_for_acks(publisher,DDS_INFINITY)");

  /* TODO: create tests by calling dds_qwait_for_acks on writers */

  status = dds_suspend(publisher);
  cr_assert_status_eq(status, DDS_RETCODE_UNSUPPORTED, "dds_suspend(publisher)");

  /* Wait_for_acks on suspended publisher and minusOneSec timeout */
  status = dds_wait_for_acks(publisher, minusOneSec);
  cr_assert_status_eq(status, DDS_RETCODE_UNSUPPORTED, "dds_wait_for_acks(suspended_publisher,-1)");

  /* Wait_for_acks on suspended publisher and zeroSec timeout */
  status = dds_wait_for_acks(publisher, zeroSec);
  cr_assert_status_eq(status, DDS_RETCODE_UNSUPPORTED, "dds_wait_for_acks(suspended_publisher,0)");

  /* Wait_for_acks on suspended publisher and oneSec timeout */
  status = dds_wait_for_acks(publisher, oneSec);
  cr_assert_status_eq(status, DDS_RETCODE_UNSUPPORTED, "dds_wait_for_acks(suspended_publisher,1)");

  /* Wait_for_acks on suspended publisher and DDS_INFINITE timeout */
  status = dds_wait_for_acks(publisher, DDS_INFINITY);
  cr_assert_status_eq(status, DDS_RETCODE_UNSUPPORTED, "dds_wait_for_acks(suspended_publisher,DDS_INFINITY)");

  dds_delete(publisher);
  dds_delete(participant);

  return;
}

Test(ddsc_publisher, coherency)
{
  return;
}

#pragma warning(pop)
