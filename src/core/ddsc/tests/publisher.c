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
#include "CUnit/Test.h"

/* We are deliberately testing some bad arguments that SAL will complain about.
 * So, silence SAL regarding these issues. */
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 28020)
#endif

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
  CU_ASSERT_EQUAL_FATAL(publisher, DDS_RETCODE_PRECONDITION_NOT_MET);

  participant = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
  CU_ASSERT_FATAL(participant >  0);

  /* Use non-null participant */
  publisher = dds_create_publisher(participant, NULL, NULL);
  CU_ASSERT_FATAL(publisher > 0);

  /* Use entity that is not a participant */
  publisher1 = dds_create_publisher(publisher, NULL, NULL);
  CU_ASSERT_EQUAL_FATAL(publisher1, DDS_RETCODE_ILLEGAL_OPERATION);
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
  CU_ASSERT_EQUAL_FATAL(status, DDS_RETCODE_PRECONDITION_NOT_MET);

  /* Resume a 0 publisher */
  status = dds_resume(0);
  CU_ASSERT_EQUAL_FATAL(status, DDS_RETCODE_PRECONDITION_NOT_MET);

  /* Uae dds_suspend on something else than a publisher */
  participant = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
  CU_ASSERT_FATAL(participant > 0);
  status = dds_suspend(participant);
  CU_ASSERT_EQUAL_FATAL(status, DDS_RETCODE_ILLEGAL_OPERATION);

  /* Use dds_resume on something else than a publisher */
  status = dds_resume(participant);
  CU_ASSERT_EQUAL_FATAL(status, DDS_RETCODE_ILLEGAL_OPERATION);

  /* Use dds_resume without calling dds_suspend */
  publisher = dds_create_publisher(participant, NULL, NULL);
  CU_ASSERT_FATAL(publisher > 0);
  status = dds_resume(publisher); /* Should be precondition not met? */
  CU_ASSERT_EQUAL_FATAL(status, DDS_RETCODE_UNSUPPORTED);

  /* Use dds_suspend on non-null publisher */
  status = dds_suspend(publisher);
  CU_ASSERT_EQUAL_FATAL(status, DDS_RETCODE_UNSUPPORTED);

  /* Use dds_resume on non-null publisher */
  status = dds_resume(publisher);
  CU_ASSERT_EQUAL_FATAL(status, DDS_RETCODE_UNSUPPORTED);

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
  CU_ASSERT_EQUAL_FATAL(status, DDS_RETCODE_BAD_PARAMETER);

  /* Wait_for_acks on NULL publisher or writer and zeroSec timeout */
  status = dds_wait_for_acks(0, zeroSec);
  CU_ASSERT_EQUAL_FATAL(status, DDS_RETCODE_PRECONDITION_NOT_MET);

  /* wait_for_acks on NULL publisher or writer and oneSec timeout */
  status = dds_wait_for_acks(0, oneSec);
  CU_ASSERT_EQUAL_FATAL(status, DDS_RETCODE_PRECONDITION_NOT_MET);

  /* wait_for_acks on NULL publisher or writer and DDS_INFINITE timeout */
  status = dds_wait_for_acks(0, DDS_INFINITY);
  CU_ASSERT_EQUAL_FATAL(status, DDS_RETCODE_PRECONDITION_NOT_MET);

  participant = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
  CU_ASSERT_FATAL(participant > 0);

  /* Wait_for_acks on participant and minusOneSec timeout */
  status = dds_wait_for_acks(participant, minusOneSec);
  CU_ASSERT_EQUAL_FATAL(status, DDS_RETCODE_BAD_PARAMETER);

  /* Wait_for_acks on participant and zeroSec timeout */
  status = dds_wait_for_acks(participant, zeroSec);
  CU_ASSERT_EQUAL_FATAL(status, DDS_RETCODE_ILLEGAL_OPERATION);

  /* Wait_for_acks on participant and oneSec timeout */
  status = dds_wait_for_acks(participant, oneSec);
  CU_ASSERT_EQUAL_FATAL(status, DDS_RETCODE_ILLEGAL_OPERATION);

  /* Wait_for_acks on participant and DDS_INFINITE timeout */
  status = dds_wait_for_acks(participant, DDS_INFINITY);
  CU_ASSERT_EQUAL_FATAL(status, DDS_RETCODE_ILLEGAL_OPERATION);

  publisher = dds_create_publisher(participant, NULL, NULL);
  CU_ASSERT_FATAL(publisher > 0);

  /* Wait_for_acks on publisher and minusOneSec timeout --
     either BAD_PARAMETER or UNSUPPORTED would be both be ok, really */
  status = dds_wait_for_acks(publisher, minusOneSec);
  CU_ASSERT_EQUAL_FATAL(status, DDS_RETCODE_BAD_PARAMETER);

  /* Wait_for_acks on publisher and zeroSec timeout */
  status = dds_wait_for_acks(publisher, zeroSec);
  CU_ASSERT_EQUAL_FATAL(status, DDS_RETCODE_UNSUPPORTED);

  /* Wait_for_acks on publisher and oneSec timeout */
  status = dds_wait_for_acks(publisher, oneSec);
  CU_ASSERT_EQUAL_FATAL(status, DDS_RETCODE_UNSUPPORTED);

  /* Wait_for_acks on publisher and DDS_INFINITE timeout */
  status = dds_wait_for_acks(publisher, DDS_INFINITY);
  CU_ASSERT_EQUAL_FATAL(status, DDS_RETCODE_UNSUPPORTED);

  /* TODO: create tests by calling dds_qwait_for_acks on writers */

  status = dds_suspend(publisher);
  CU_ASSERT_EQUAL_FATAL(status, DDS_RETCODE_UNSUPPORTED);

  /* Wait_for_acks on suspended publisher and minusOneSec timeout */
  status = dds_wait_for_acks(publisher, minusOneSec);
  CU_ASSERT_EQUAL_FATAL(status, DDS_RETCODE_BAD_PARAMETER);

  /* Wait_for_acks on suspended publisher and zeroSec timeout */
  status = dds_wait_for_acks(publisher, zeroSec);
  CU_ASSERT_EQUAL_FATAL(status, DDS_RETCODE_UNSUPPORTED);

  /* Wait_for_acks on suspended publisher and oneSec timeout */
  status = dds_wait_for_acks(publisher, oneSec);
  CU_ASSERT_EQUAL_FATAL(status, DDS_RETCODE_UNSUPPORTED);

  /* Wait_for_acks on suspended publisher and DDS_INFINITE timeout */
  status = dds_wait_for_acks(publisher, DDS_INFINITY);
  CU_ASSERT_EQUAL_FATAL(status, DDS_RETCODE_UNSUPPORTED);

  dds_delete(publisher);
  dds_delete(participant);

  return;
}

CU_Test(ddsc_publisher, enable_by_default)
{
  dds_entity_t participant, publisher;
  dds_return_t status, status1;
  dds_qos_t *pqos, *pubqos;
  bool autoenable;

  /* create a default publisher and check that autoenable=true */
  participant = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
  CU_ASSERT_FATAL(participant > 0);
  pqos = dds_create_qos();
  status = dds_get_qos(participant, pqos);
  CU_ASSERT_EQUAL_FATAL(status, DDS_RETCODE_OK);
  status = dds_qget_entity_factory(pqos, &autoenable);
  CU_ASSERT_EQUAL_FATAL(status, true);
  CU_ASSERT_EQUAL_FATAL(autoenable, true);
  publisher = dds_create_publisher(participant, NULL, NULL);
  CU_ASSERT_FATAL(publisher > 0);
  pubqos = dds_create_qos();
  status = dds_get_qos(publisher, pubqos);
  CU_ASSERT_EQUAL_FATAL(status, DDS_RETCODE_OK);
  status = dds_qget_entity_factory(pubqos, &autoenable);
  CU_ASSERT_EQUAL_FATAL(status, true);
  CU_ASSERT_EQUAL_FATAL(autoenable, true);
  /* enabling an already enabled entity is a noop */
  status1 = dds_enable (publisher);
  CU_ASSERT_EQUAL_FATAL(status1, DDS_RETCODE_OK);
  /* check that the publisher is really enabled
   * by trying to set a qos that cannot be changed once
   * the subscriber is enabled. We use the presentation qos
   * for that purpose */
  dds_qset_presentation(pubqos, DDS_PRESENTATION_TOPIC, true, true);
  status = dds_set_qos(publisher, pubqos);
  CU_ASSERT_EQUAL_FATAL(status, DDS_RETCODE_IMMUTABLE_POLICY);
  status = dds_delete (participant);
  CU_ASSERT_EQUAL_FATAL(status, DDS_RETCODE_OK);
  dds_delete_qos(pubqos);
  dds_delete_qos(pqos);

  /* create a participant with autoenable=false
   * check that a default publisher is disabled and has autoenable=true */
  pqos = dds_create_qos();
  dds_qset_entity_factory(pqos, false);
  participant = dds_create_participant (DDS_DOMAIN_DEFAULT, pqos, NULL);
  CU_ASSERT_FATAL(participant > 0);
  status = dds_get_qos(participant, pqos);
  CU_ASSERT_EQUAL_FATAL(status, DDS_RETCODE_OK);
  status = dds_qget_entity_factory(pqos, &autoenable);
  CU_ASSERT_EQUAL_FATAL(status, true);
  CU_ASSERT_EQUAL_FATAL(autoenable, false);
  publisher = dds_create_publisher(participant, NULL, NULL);
  CU_ASSERT_FATAL(publisher > 0);
  pubqos = dds_create_qos();
  status = dds_get_qos(publisher, pubqos);
  CU_ASSERT_EQUAL_FATAL(status, DDS_RETCODE_OK);
  status = dds_qget_entity_factory(pubqos, &autoenable);
  CU_ASSERT_EQUAL_FATAL(status, true);
  CU_ASSERT_EQUAL_FATAL(autoenable, true);
  /* the publisher should be disabled because the participant
   * has autoenable=false. To check that the publisher is really
   * disabled we try to set an immutable qos. We use the presentation
   * qos for that purpose. Setting it should succeed. */
  dds_qset_presentation(pubqos, DDS_PRESENTATION_TOPIC, true, true);
  status = dds_set_qos(publisher, pubqos);
  CU_ASSERT_EQUAL_FATAL(status, DDS_RETCODE_OK);
  status = dds_delete (participant);
  CU_ASSERT_EQUAL_FATAL(status, DDS_RETCODE_OK);
  dds_delete_qos(pubqos);
  dds_delete_qos(pqos);
}

CU_Test(ddsc_publisher, disabled_publisher_enable_later)
{
  dds_entity_t participant, publisher;
  dds_qos_t *pqos, *pubqos;
  bool autoenable;
  bool status;
  dds_return_t ret;
  bool c_access, o_access;
  dds_presentation_access_scope_kind_t pr_kind;

  pqos = dds_create_qos();
  dds_qset_entity_factory(pqos, false);
  /* create a participant with autoenable=false */
  participant = dds_create_participant (DDS_DOMAIN_DEFAULT, pqos, NULL);
  CU_ASSERT_FATAL(participant > 0);
  /* create a default publisher that should be disabled */
  publisher = dds_create_publisher(participant, NULL, NULL);
  CU_ASSERT_FATAL(publisher > 0);
  /* get the autoenable value for this publisher */
  pubqos = dds_create_qos();
  ret = dds_get_qos(publisher, pubqos);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
  status = dds_qget_entity_factory(pubqos, &autoenable);
  CU_ASSERT_EQUAL_FATAL(status, true);
  CU_ASSERT_EQUAL_FATAL(autoenable, true);
  /* the autoenable is true, but the publisher should
   * not be enabled because the participant was not enabled
   * Because there is no explicit call to find out if an entity
   * is enabled we do this by trying to set an immutable qos
   * on the publisher. This should succeed if the publisher
   * is not yet enabled, and fail if it is */
  dds_qset_presentation(pubqos, DDS_PRESENTATION_GROUP, true, true);
  ret = dds_set_qos(publisher, pubqos);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
  /* check that qos is really changed */
  dds_qget_presentation(pubqos, &pr_kind,&c_access, &o_access);
  CU_ASSERT_EQUAL_FATAL(pr_kind, DDS_PRESENTATION_GROUP);
  CU_ASSERT_EQUAL_FATAL(c_access, true);
  CU_ASSERT_EQUAL_FATAL(o_access, true);
  /* now enable the publisher and try again to set the
   * presentation qos. This should now result in IMMUTABLE_POLICY
   * because the publisher is already enabled */
  ret = dds_enable(publisher);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
  dds_qset_presentation(pubqos, DDS_PRESENTATION_INSTANCE, false, false);
  ret = dds_set_qos(publisher, pubqos);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_IMMUTABLE_POLICY);
  /* TODO:
   * the following operations should all be available on a
   * disabled entity: set_qos, get_qos, get_status_condition,
   * factory operations, get_status_changes, lookup operations
   */
  ret = dds_delete (participant);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
  dds_delete_qos(pubqos);
  dds_delete_qos(pqos);
}

CU_Test(ddsc_publisher, delete_disabled_publisher)
{
  dds_entity_t participant, publisher;
  dds_qos_t *pqos;
  dds_return_t ret;

  pqos = dds_create_qos();
  dds_qset_entity_factory(pqos, false);
  /* create a participant with autoenable=false */
  participant = dds_create_participant (DDS_DOMAIN_DEFAULT, pqos, NULL);
  CU_ASSERT_FATAL(participant > 0);
  /* create a default publisher that should be disabled */
  publisher = dds_create_publisher(participant, NULL, NULL);
  CU_ASSERT_FATAL(publisher > 0);
  /* delete the participant */
  ret = dds_delete(publisher);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
  ret = dds_delete(participant);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
  dds_delete_qos(pqos);
}

CU_Test(ddsc_publisher, coherency)
{
  return;
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif
