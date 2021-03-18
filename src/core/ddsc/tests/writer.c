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
#include <stdio.h>

#include "CUnit/Test.h"
#include "dds/dds.h"
#include "RoundTrip.h"
#include "dds/ddsrt/misc.h"

static dds_entity_t participant = 0;
static dds_entity_t topic = 0;
static dds_entity_t publisher = 0;
static dds_entity_t writer = 0;

static void
setup(void)
{
    participant = dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);
    CU_ASSERT_FATAL(participant > 0);
    topic = dds_create_topic(participant, &RoundTripModule_DataType_desc, "RoundTrip", NULL, NULL);
    CU_ASSERT_FATAL(topic > 0);
    publisher = dds_create_publisher(participant, NULL, NULL);
    CU_ASSERT_FATAL(publisher > 0);
}

static void
teardown(void)
{
    dds_delete(writer);
    dds_delete(publisher);
    dds_delete(topic);
    dds_delete(participant);
}

CU_Test(ddsc_create_writer, basic, .init = setup, .fini = teardown)
{
    dds_return_t result;

    writer = dds_create_writer(participant, topic, NULL, NULL);
    CU_ASSERT_FATAL(writer > 0);
    result = dds_delete(writer);
    CU_ASSERT_EQUAL_FATAL(result, DDS_RETCODE_OK);

}

CU_Test(ddsc_create_writer, null_parent, .init = setup, .fini = teardown)
{
    DDSRT_WARNING_MSVC_OFF(28020); /* Disable SAL warning on intentional misuse of the API */
    writer = dds_create_writer(0, topic, NULL, NULL);
    DDSRT_WARNING_MSVC_ON(28020);
    CU_ASSERT_EQUAL_FATAL(writer, DDS_RETCODE_BAD_PARAMETER);
}

CU_Test(ddsc_create_writer, bad_parent, .init = setup, .fini = teardown)
{
    writer = dds_create_writer(topic, topic, NULL, NULL);
    CU_ASSERT_EQUAL_FATAL(writer, DDS_RETCODE_ILLEGAL_OPERATION);
}

CU_Test(ddsc_create_writer, participant, .init = setup, .fini = teardown)
{
    writer = dds_create_writer(participant, topic, NULL, NULL);
    CU_ASSERT_FATAL(writer > 0);
}

CU_Test(ddsc_create_writer, wrong_participant, .init = setup, .fini = teardown)
{
    dds_entity_t participant2 = dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);
    CU_ASSERT_FATAL(participant2 > 0);
    writer = dds_create_writer(participant2, topic, NULL, NULL);
    CU_ASSERT_EQUAL_FATAL(writer, DDS_RETCODE_BAD_PARAMETER);
    dds_delete(participant2);
}

CU_Test(ddsc_create_writer, publisher, .init = setup, .fini = teardown)
{
    writer = dds_create_writer(publisher, topic, NULL, NULL);
    CU_ASSERT_FATAL(writer > 0);
}

CU_Test(ddsc_create_writer, deleted_publisher, .init = setup, .fini = teardown)
{
    dds_delete(publisher);

    writer = dds_create_writer(publisher, topic, NULL, NULL);
    CU_ASSERT_EQUAL_FATAL(writer, DDS_RETCODE_BAD_PARAMETER);
}

CU_Test(ddsc_create_writer, null_topic, .init = setup, .fini = teardown)
{
    DDSRT_WARNING_MSVC_OFF(28020); /* Disable SAL warning on intentional misuse of the API */
    writer = dds_create_writer(publisher, 0, NULL, NULL);
    DDSRT_WARNING_MSVC_ON(28020);
    CU_ASSERT_EQUAL_FATAL(writer, DDS_RETCODE_BAD_PARAMETER);
}

CU_Test(ddsc_create_writer, bad_topic, .init = setup, .fini = teardown)
{
    writer = dds_create_writer(publisher, publisher, NULL, NULL);
    CU_ASSERT_EQUAL_FATAL(writer, DDS_RETCODE_ILLEGAL_OPERATION);
}

CU_Test(ddsc_create_writer, deleted_topic, .init = setup, .fini = teardown)
{
    dds_delete(topic);

    writer = dds_create_writer(publisher, topic, NULL, NULL);
    CU_ASSERT_EQUAL_FATAL(writer, DDS_RETCODE_BAD_PARAMETER);
}


CU_Test(ddsc_create_writer, participant_mismatch, .init = setup, .fini = teardown)
{
    dds_entity_t l_par = 0;
    dds_entity_t l_pub = 0;

    /* The call to setup() created the global topic. */

    /* Create publisher on local participant. */
    l_par = dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);
    CU_ASSERT_FATAL(l_par > 0);
    l_pub = dds_create_publisher(l_par, NULL, NULL);
    CU_ASSERT_FATAL(l_pub > 0);

    /* Create writer with local publisher and global topic. */
    writer = dds_create_writer(l_pub, topic, NULL, NULL);

    /* Expect the creation to have failed. */
    CU_ASSERT_FATAL(writer <= 0);

    dds_delete(l_pub);
    dds_delete(l_par);
}

CU_Test(ddsc_writer, enable_by_default) {
  dds_return_t status, status1;
  dds_qos_t *pqos, *wqos;
  bool autoenable;

  /* check that default autoenable setting of participant is true */
  participant = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
  CU_ASSERT_FATAL(participant > 0);
  pqos = dds_create_qos();
  status = dds_get_qos(participant, pqos);
  CU_ASSERT_EQUAL_FATAL(status, DDS_RETCODE_OK);
  status = dds_qget_entity_factory(pqos, &autoenable);
  CU_ASSERT_EQUAL_FATAL(status, true);
  CU_ASSERT_EQUAL_FATAL(autoenable, true);
  /* check that default autoenable setting of publisher is true */
  publisher = dds_create_publisher(participant, NULL, NULL);
  CU_ASSERT_FATAL(publisher > 0);
  topic = dds_create_topic(participant, &RoundTripModule_DataType_desc, "ddsc_writer_enable", NULL, NULL);
  CU_ASSERT_FATAL(topic > 0);
  writer = dds_create_writer(publisher, topic, NULL, NULL);
  CU_ASSERT_FATAL(writer > 0);
  wqos = dds_create_qos();
  status = dds_get_qos(writer, wqos);
  CU_ASSERT_EQUAL_FATAL(status, DDS_RETCODE_OK);
  status = dds_qget_entity_factory(wqos, &autoenable);
  CU_ASSERT_EQUAL_FATAL(status, true);
  CU_ASSERT_EQUAL_FATAL(autoenable, true);

  /* enabling an already enabled entity is a noop */
  status1 = dds_enable (writer);
  CU_ASSERT_EQUAL_FATAL(status1, DDS_RETCODE_OK);

  /* we check that the writer is really enabled
   * by trying to set a qos that cannot be changed once
   * the reader is enabled. We use the history qos
   * for that purpose */
  dds_qset_history(wqos, DDS_HISTORY_KEEP_ALL, 0);
  status = dds_set_qos(writer, wqos);
  CU_ASSERT_EQUAL_FATAL(status, DDS_RETCODE_IMMUTABLE_POLICY);
  status = dds_delete (participant);
  CU_ASSERT_EQUAL_FATAL(status, DDS_RETCODE_OK);
  dds_delete_qos(pqos);
  dds_delete_qos(wqos);
}

CU_Test(ddsc_writer, disable_writer_enable_later) {
  dds_qos_t *pqos, *wqos;
  bool autoenable;
  bool status;
  dds_return_t ret;
  dds_history_kind_t hist_kind;
  int32_t hist_depth;

  pqos = dds_create_qos();
  dds_qset_entity_factory(pqos, false);
  /* create a participant with autoenable=false */
  participant = dds_create_participant (DDS_DOMAIN_DEFAULT, pqos, NULL);
  CU_ASSERT_FATAL(participant > 0);
  /* create a default publisher that should be disabled */
  publisher = dds_create_publisher(participant, NULL, NULL);
  CU_ASSERT_FATAL(publisher > 0);
  topic = dds_create_topic(participant, &RoundTripModule_DataType_desc, "ddsc_writer_enable", NULL, NULL);
  CU_ASSERT_FATAL(topic > 0);
  writer = dds_create_writer(publisher, topic, NULL, NULL);
  CU_ASSERT_FATAL(writer > 0);
  /* get the autoenable value for this writer */
  wqos = dds_create_qos();
  ret = dds_get_qos(writer, wqos);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
  status = dds_qget_entity_factory(wqos, &autoenable);
  CU_ASSERT_EQUAL_FATAL(status, true);
  CU_ASSERT_EQUAL_FATAL(autoenable, true);
  /* the autoenable is true, but the writer should
   * not be enabled because the publisher was not enabled
   * Because there is no explicit call to find out if an entity
   * is enabled we do this by trying to set an immutable qos
   * on the writer. This should succeed if the writer
   * is not yet enabled, and fail if it is. We use the history
   * qos for that purpose */
  dds_qset_history(wqos, DDS_HISTORY_KEEP_ALL, 0);
  ret = dds_set_qos(writer, wqos);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
  /* check that qos is really changed */
  ret = dds_get_qos(writer, wqos);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
  dds_qget_history(wqos, &hist_kind,&hist_depth);
  CU_ASSERT_EQUAL_FATAL(hist_kind, DDS_HISTORY_KEEP_ALL);
  CU_ASSERT_EQUAL_FATAL(hist_depth, 0);
  /* now try to enable the writer and set the immutable
   * reader qos again. We should not be able to enable the
   * writer because the publisher is still disabled.
   * The writer remains disabled and we should still be able
   * to modify its immutable qos */
  ret = dds_enable(writer);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_PRECONDITION_NOT_MET);
  dds_qset_history(wqos, DDS_HISTORY_KEEP_LAST, 20);
  ret = dds_set_qos(writer, wqos);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
  /* check that qos is really changed */
  ret = dds_get_qos(writer, wqos);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
  dds_qget_history(wqos, &hist_kind,&hist_depth);
  CU_ASSERT_EQUAL_FATAL(hist_kind, DDS_HISTORY_KEEP_LAST);
  CU_ASSERT_EQUAL_FATAL(hist_depth, 20);
  /* now enable the publisher and the writer, and try
   * to set an immutable qos. This then should fail */
  ret = dds_enable(publisher);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
  ret = dds_enable(writer);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
  dds_qset_history(wqos, DDS_HISTORY_KEEP_LAST, 30);
  ret = dds_set_qos(writer, wqos);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_IMMUTABLE_POLICY);
  /* check that qos has not changed */
  ret = dds_get_qos(writer, wqos);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
  dds_qget_history(wqos, &hist_kind,&hist_depth);
  CU_ASSERT_EQUAL_FATAL(hist_kind, DDS_HISTORY_KEEP_LAST);
  CU_ASSERT_EQUAL_FATAL(hist_depth, 20);
  ret = dds_delete (participant);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
  dds_delete_qos(wqos);
  dds_delete_qos(pqos);
}

CU_Test(ddsc_writer, delete_disabled_writer) {
  dds_qos_t *pubqos, *wqos;
  dds_return_t ret;
  dds_history_kind_t hist_kind;
  int32_t hist_depth;

  /* create a participant */
  participant = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
  CU_ASSERT_FATAL(participant > 0);
  /* create a publisher with autoenable=false */
  pubqos = dds_create_qos();
  dds_qset_entity_factory(pubqos, false);
  publisher = dds_create_publisher(participant, pubqos, NULL);
  CU_ASSERT_FATAL(publisher > 0);
  /* create a writer */
  topic = dds_create_topic(participant, &RoundTripModule_DataType_desc, "ddsc_writer_enable", NULL, NULL);
  CU_ASSERT_FATAL(topic > 0);
  writer = dds_create_writer(publisher, topic, NULL, NULL);
  CU_ASSERT_FATAL(writer > 0);
  /* check that the writer is disabled by
   * trying to set an immutable qos. This should succeed
   * First check that current value is KEEPLAST-1 */
  wqos = dds_create_qos();
  ret = dds_get_qos(writer, wqos);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
  dds_qget_history(wqos, &hist_kind,&hist_depth);
  CU_ASSERT_EQUAL_FATAL(hist_kind, DDS_HISTORY_KEEP_LAST);
  CU_ASSERT_EQUAL_FATAL(hist_depth, 1);
  /* Now change the qos */
  dds_qset_history(wqos, DDS_HISTORY_KEEP_LAST, 10);
  ret = dds_set_qos(writer, wqos);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
  /* check that qos is really changed */
  ret = dds_get_qos(writer, wqos);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
  dds_qget_history(wqos, &hist_kind,&hist_depth);
  CU_ASSERT_EQUAL_FATAL(hist_kind, DDS_HISTORY_KEEP_LAST);
  CU_ASSERT_EQUAL_FATAL(hist_depth, 10);
  /* Now we know that the writer is disabled.
   * Let's now delete the participant */
  ret = dds_delete(writer);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
 ret = dds_delete(participant);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
  dds_delete_qos(wqos);
  dds_delete_qos(pubqos);
}

CU_Test(ddsc_writer, delete_parent_of_disabled_reader) {
  dds_qos_t *pubqos;
  dds_return_t ret;

  /* create a participant */
  participant = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
  CU_ASSERT_FATAL(participant > 0);
  /* create a publisher with autoenable=false */
  pubqos = dds_create_qos();
  dds_qset_entity_factory(pubqos, false);
  publisher = dds_create_publisher(participant, pubqos, NULL);
  CU_ASSERT_FATAL(publisher > 0);
  /* create a writer */
  topic = dds_create_topic(participant, &RoundTripModule_DataType_desc, "ddsc_writer_disabled", NULL, NULL);
  CU_ASSERT_FATAL(topic > 0);
  writer = dds_create_writer(publisher, topic, NULL, NULL);
  CU_ASSERT_FATAL(writer > 0);
  ret = dds_delete(participant);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
  dds_delete_qos(pubqos);
}

CU_Test(ddsc_writer, autoenable_disabled_writer) {
  dds_qos_t *pqos, *pubqos, *wqos;
  bool autoenable;
  bool status;
  dds_return_t ret;
  dds_history_kind_t hist_kind;
  int32_t hist_depth;

  pqos = dds_create_qos();
  dds_qset_entity_factory(pqos, false);
  /* create a participant with autoenable=false */
  participant = dds_create_participant (DDS_DOMAIN_DEFAULT, pqos, NULL);
  CU_ASSERT_FATAL(participant > 0);
  ret = dds_get_qos(participant, pqos);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
  status = dds_qget_entity_factory(pqos, &autoenable);
  CU_ASSERT_EQUAL_FATAL(status, true);
  CU_ASSERT_EQUAL_FATAL(autoenable, false);
  /* create a disabled publisher */
  publisher = dds_create_publisher(participant, NULL, NULL);
  CU_ASSERT_FATAL(publisher > 0);
  /* check that autoenable of the publisher is set to true */
  pubqos = dds_create_qos();
  ret = dds_get_qos(publisher, pubqos);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
  status = dds_qget_entity_factory(pubqos, &autoenable);
  CU_ASSERT_EQUAL_FATAL(status, true);
  CU_ASSERT_EQUAL_FATAL(autoenable, true);
  /* create topic */
  topic = dds_create_topic(participant, &RoundTripModule_DataType_desc, "ddsc_writer_disabled", NULL, NULL);
  CU_ASSERT_FATAL(topic > 0);
  /* create a writer */
  writer = dds_create_writer(publisher, topic, NULL, NULL);
  CU_ASSERT_FATAL(writer > 0);
  /* the writer should be disabled because the publisher
   * is disabled. To check that the writer is really
   * disabled try to set an immutable qos. We use the history
   * qos for that purpose. Setting it should succeed. */
  wqos = dds_create_qos();
  dds_qset_history(wqos, DDS_HISTORY_KEEP_ALL, 0);
  ret = dds_set_qos(writer, wqos);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
  /* check that qos is really changed */
  ret = dds_get_qos(writer, wqos);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
  dds_qget_history(wqos, &hist_kind,&hist_depth);
  CU_ASSERT_EQUAL_FATAL(hist_kind, DDS_HISTORY_KEEP_ALL);
  CU_ASSERT_EQUAL_FATAL(hist_depth, 0);
  /* enable the writer, this should fail
   * because the publisher is not yet enabled */
  ret = dds_enable(writer);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_PRECONDITION_NOT_MET);
  /* Check that the writer is still disabled
   * by trying to set an immutable qos */
  dds_qset_history(wqos, DDS_HISTORY_KEEP_ALL, 10);
  ret = dds_set_qos(writer, wqos);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
  /* enable the publisher */
  ret = dds_enable(publisher);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
  /* to check that the publisher is enabled
  * we try to set an immutable qos, this should
  * fail */
  dds_qset_presentation(pubqos, DDS_PRESENTATION_GROUP, true, true);
  ret = dds_set_qos(publisher, pubqos);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_IMMUTABLE_POLICY);
  /* the writer must be enabled because
   * the writer was created using a publisher
   * with autoenabled=true  and the publisher
   * has been enabled. To check whether the writer
   * is enabled we try setting an immutable qos,
   * this should fail */
  dds_qset_history(wqos, DDS_HISTORY_KEEP_ALL, 20);
  ret = dds_set_qos(writer, wqos);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_IMMUTABLE_POLICY);
  /* now enable the writer, this should be a noop */
  ret = dds_enable(writer);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
  /* check that the writer is enabled by trying to set an immutable qos */
  dds_qset_history(wqos, DDS_HISTORY_KEEP_ALL, 30);
  ret = dds_set_qos(writer, wqos);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_IMMUTABLE_POLICY);
  ret = dds_delete(participant);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
  dds_delete_qos(wqos);
  dds_delete_qos(pubqos);
  dds_delete_qos(pqos);
}
