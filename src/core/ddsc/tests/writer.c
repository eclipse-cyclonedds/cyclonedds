// Copyright(c) 2006 to 2019 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

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
