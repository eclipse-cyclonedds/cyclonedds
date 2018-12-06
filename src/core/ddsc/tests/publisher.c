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
#include "CUnit/Test.h"

/* We are deliberately testing some bad arguments that SAL will complain about.
 * So, silence SAL regarding these issues. */
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 28020)
#endif

#define cu_assert_status_eq(s1, s2) CU_ASSERT_EQUAL_FATAL(dds_err_nr(s1), s2)

/* Dummy callback */
static void data_available_cb(dds_entity_t reader, void* arg)
{
  (void)reader;
  (void)arg;
}


CU_Test(ddsc_publisher, create)
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
  CU_ASSERT_EQUAL_FATAL(dds_err_nr(publisher), DDS_RETCODE_BAD_PARAMETER);

  participant = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
  CU_ASSERT_FATAL(participant >  0);

  /* Use non-null participant */
  publisher = dds_create_publisher(participant, NULL, NULL);
  CU_ASSERT_FATAL(publisher > 0);

  /* Use entity that is not a participant */
  publisher1 = dds_create_publisher(publisher, NULL, NULL);
  CU_ASSERT_EQUAL_FATAL(dds_err_nr(publisher1), DDS_RETCODE_ILLEGAL_OPERATION);
  dds_delete(publisher);

  /* Create a non-null qos */
  qos = dds_create_qos();
  CU_ASSERT_NOT_EQUAL_FATAL(qos, NULL);

  /* Use qos without partition; in that case the default partition should be used */
  publisher = dds_create_publisher(participant, qos, NULL);
  CU_ASSERT_FATAL(publisher > 0);
  dds_delete(publisher);

/* Somehow, the compiler thinks the char arrays might not be zero-terminated... */
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 6054)
#endif

  /* Use qos with single partition */
  dds_qset_partition (qos, 1, singlePartitions);
  publisher = dds_create_publisher(participant, qos, NULL);
  CU_ASSERT_FATAL(publisher > 0);
  dds_delete(publisher);

  /* Use qos with multiple partitions */
  dds_qset_partition (qos, 2, multiplePartitions);
  publisher = dds_create_publisher(participant, qos, NULL);
  CU_ASSERT_FATAL(publisher > 0);
  dds_delete(publisher);

  /* Use qos with multiple partitions */
  dds_qset_partition (qos, 2, duplicatePartitions);
  publisher = dds_create_publisher(participant, qos, NULL);
  CU_ASSERT_FATAL(publisher > 0);
  dds_delete(publisher);

#ifdef _MSC_VER
#pragma warning(pop)
#endif

  /* Use listener(NULL) */
  listener = dds_create_listener(NULL);
  CU_ASSERT_NOT_EQUAL_FATAL(listener, NULL);
  publisher = dds_create_publisher(participant, NULL, listener);
  CU_ASSERT_FATAL(publisher > 0);
  dds_delete(publisher);

  dds_reset_listener(listener);

  /* Use listener for data_available */
  dds_lset_data_available(listener, NULL);
  publisher = dds_create_publisher(participant, NULL, listener);
  CU_ASSERT_FATAL(publisher > 0);
  dds_delete(publisher);

  dds_reset_listener(listener);

  /* Use DDS_LUNSET for data_available */
  dds_lset_data_available(listener, DDS_LUNSET);
  publisher = dds_create_publisher(participant, NULL, listener);
  CU_ASSERT_FATAL(publisher > 0);
  dds_delete(publisher);

  dds_reset_listener(listener);

  /* Use callback for data_available */
  dds_lset_data_available(listener, data_available_cb);
  publisher = dds_create_publisher(participant, NULL, listener);
  CU_ASSERT_FATAL(publisher > 0);
  dds_delete(publisher);

  /* Use both qos setting and callback listener */
  dds_lset_data_available(listener, data_available_cb);
  publisher = dds_create_publisher(participant, qos, listener);
  CU_ASSERT(publisher > 0);
  dds_delete(publisher);

  dds_delete_listener(listener);
  dds_delete_qos(qos);
  dds_delete (participant);
}

CU_Test(ddsc_publisher, suspend_resume)
{

  dds_entity_t participant, publisher;
  dds_return_t status;

  /* Suspend a 0 publisher */
  status = dds_suspend(0);
  cu_assert_status_eq(status, DDS_RETCODE_BAD_PARAMETER);

  /* Resume a 0 publisher */
  status = dds_resume(0);
  cu_assert_status_eq(status, DDS_RETCODE_BAD_PARAMETER);

  /* Uae dds_suspend on something else than a publisher */
  participant = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
  CU_ASSERT_FATAL(participant > 0);
  status = dds_suspend(participant);
  cu_assert_status_eq(status, DDS_RETCODE_BAD_PARAMETER);

  /* Use dds_resume on something else than a publisher */
  status = dds_resume(participant);
  cu_assert_status_eq(status, DDS_RETCODE_BAD_PARAMETER);

  /* Use dds_resume without calling dds_suspend */
  publisher = dds_create_publisher(participant, NULL, NULL);
  CU_ASSERT_FATAL(publisher > 0);
  status = dds_resume(publisher); /* Should be precondition not met? */
  cu_assert_status_eq(status, DDS_RETCODE_UNSUPPORTED);

  /* Use dds_suspend on non-null publisher */
  status = dds_suspend(publisher);
  cu_assert_status_eq(status, DDS_RETCODE_UNSUPPORTED);

  /* Use dds_resume on non-null publisher */
  status = dds_resume(publisher);
  cu_assert_status_eq(status, DDS_RETCODE_UNSUPPORTED);

  dds_delete(publisher);
  dds_delete(participant);

  return;
}

CU_Test(ddsc_publisher, wait_for_acks)
{
  dds_entity_t participant, publisher;
  dds_return_t status;
  dds_duration_t zeroSec = ((dds_duration_t)DDS_SECS(0));
  dds_duration_t oneSec = ((dds_duration_t)DDS_SECS(1));
  dds_duration_t minusOneSec = ((dds_duration_t)DDS_SECS(-1));

  /* Wait_for_acks on 0 publisher or writer and minusOneSec timeout */
  status = dds_wait_for_acks(0, minusOneSec);
  cu_assert_status_eq(status, DDS_RETCODE_BAD_PARAMETER);

  /* Wait_for_acks on NULL publisher or writer and zeroSec timeout */
  status = dds_wait_for_acks(0, zeroSec);
  cu_assert_status_eq(status, DDS_RETCODE_BAD_PARAMETER);

  /* wait_for_acks on NULL publisher or writer and oneSec timeout */
  status = dds_wait_for_acks(0, oneSec);
  cu_assert_status_eq(status, DDS_RETCODE_BAD_PARAMETER);

  /* wait_for_acks on NULL publisher or writer and DDS_INFINITE timeout */
  status = dds_wait_for_acks(0, DDS_INFINITY);
  cu_assert_status_eq(status, DDS_RETCODE_BAD_PARAMETER);

  participant = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
  CU_ASSERT_FATAL(participant > 0);

  /* Wait_for_acks on participant and minusOneSec timeout */
  status = dds_wait_for_acks(participant, minusOneSec);
  cu_assert_status_eq(status, DDS_RETCODE_BAD_PARAMETER);

  /* Wait_for_acks on participant and zeroSec timeout */
  status = dds_wait_for_acks(participant, zeroSec);
  cu_assert_status_eq(status, DDS_RETCODE_BAD_PARAMETER);

  /* Wait_for_acks on participant and oneSec timeout */
  status = dds_wait_for_acks(participant, oneSec);
  cu_assert_status_eq(status, DDS_RETCODE_BAD_PARAMETER);

  /* Wait_for_acks on participant and DDS_INFINITE timeout */
  status = dds_wait_for_acks(participant, DDS_INFINITY);
  cu_assert_status_eq(status, DDS_RETCODE_BAD_PARAMETER);

  publisher = dds_create_publisher(participant, NULL, NULL);
  CU_ASSERT_FATAL(publisher > 0);

  /* Wait_for_acks on publisher and minusOneSec timeout */
  status = dds_wait_for_acks(publisher, minusOneSec);
  cu_assert_status_eq(status, DDS_RETCODE_UNSUPPORTED);

  /* Wait_for_acks on publisher and zeroSec timeout */
  status = dds_wait_for_acks(publisher, zeroSec);
  cu_assert_status_eq(status, DDS_RETCODE_UNSUPPORTED);

  /* Wait_for_acks on publisher and oneSec timeout */
  status = dds_wait_for_acks(publisher, oneSec);
  cu_assert_status_eq(status, DDS_RETCODE_UNSUPPORTED);

  /* Wait_for_acks on publisher and DDS_INFINITE timeout */
  status = dds_wait_for_acks(publisher, DDS_INFINITY);
  cu_assert_status_eq(status, DDS_RETCODE_UNSUPPORTED);

  /* TODO: create tests by calling dds_qwait_for_acks on writers */

  status = dds_suspend(publisher);
  cu_assert_status_eq(status, DDS_RETCODE_UNSUPPORTED);

  /* Wait_for_acks on suspended publisher and minusOneSec timeout */
  status = dds_wait_for_acks(publisher, minusOneSec);
  cu_assert_status_eq(status, DDS_RETCODE_UNSUPPORTED);

  /* Wait_for_acks on suspended publisher and zeroSec timeout */
  status = dds_wait_for_acks(publisher, zeroSec);
  cu_assert_status_eq(status, DDS_RETCODE_UNSUPPORTED);

  /* Wait_for_acks on suspended publisher and oneSec timeout */
  status = dds_wait_for_acks(publisher, oneSec);
  cu_assert_status_eq(status, DDS_RETCODE_UNSUPPORTED);

  /* Wait_for_acks on suspended publisher and DDS_INFINITE timeout */
  status = dds_wait_for_acks(publisher, DDS_INFINITY);
  cu_assert_status_eq(status, DDS_RETCODE_UNSUPPORTED);

  dds_delete(publisher);
  dds_delete(participant);

  return;
}

CU_Test(ddsc_publisher, coherency)
{
  return;
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif
