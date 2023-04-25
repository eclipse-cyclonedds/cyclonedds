// Copyright(c) 2006 to 2020 ZettaScale Technology and others
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

#include "test_common.h"

static dds_entity_t e[8];

#define PAR (0) /* Participant */
#define TOP (1) /* Topic */
#define PUB (2) /* Publisher */
#define WRI (3) /* Writer */
#define SUB (4) /* Subscriber */
#define REA (5) /* Reader */
#define RCD (6) /* ReadCondition */
#define BAD (7) /* Bad (non-entity) */

struct index_result {
    unsigned index;
    dds_return_t exp_res;
};

static void
setup(void)
{
    e[PAR] = dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);
    CU_ASSERT_FATAL(e[PAR] > 0);
    e[TOP] = dds_create_topic(e[PAR], &RoundTripModule_DataType_desc, "RoundTrip", NULL, NULL);
    CU_ASSERT_FATAL(e[TOP] > 0);
    e[PUB] = dds_create_publisher(e[PAR], NULL, NULL);
    CU_ASSERT_FATAL(e[PUB] > 0);
    e[WRI] = dds_create_writer(e[PUB], e[TOP], NULL, NULL);
    CU_ASSERT_FATAL(e[WRI] > 0);
    e[SUB] = dds_create_subscriber(e[PAR], NULL, NULL);
    CU_ASSERT_FATAL(e[SUB] > 0);
    e[REA] = dds_create_reader(e[SUB], e[TOP], NULL, NULL);
    CU_ASSERT_FATAL(e[REA] > 0);
    e[RCD] = dds_create_readcondition(e[REA], DDS_ANY_STATE);
    CU_ASSERT_FATAL(e[RCD] > 0);
    e[BAD] = 314159265;
}

static void
teardown(void)
{
    for(unsigned i = (sizeof e / sizeof *e); i > 0; i--) {
        dds_delete(e[i - 1]);
    }
}

/*************************************************************************************************/



CU_Test(ddsc_unsupported, dds_begin_end_coherent, .init = setup, .fini = teardown)
{
    dds_return_t result;
    static struct index_result pars[] = {
        {PUB, DDS_RETCODE_UNSUPPORTED},
        {WRI, DDS_RETCODE_UNSUPPORTED},
        {SUB, DDS_RETCODE_UNSUPPORTED},
        {REA, DDS_RETCODE_UNSUPPORTED},
        {BAD, DDS_RETCODE_BAD_PARAMETER}
    };

    for (size_t i=0; i < sizeof (pars) / sizeof (pars[0]);i++) {
        result = dds_begin_coherent(e[pars[i].index]);
        CU_ASSERT_EQUAL(result, pars[i].exp_res);
        result = dds_end_coherent(e[pars[i].index]);
        CU_ASSERT_EQUAL(result, pars[i].exp_res);
    }
}

CU_Test(ddsc_unsupported, dds_suspend_resume, .init = setup, .fini = teardown)
{
    dds_return_t result;
    static struct index_result pars[] = {
        {PUB, DDS_RETCODE_UNSUPPORTED},
        {WRI, DDS_RETCODE_ILLEGAL_OPERATION},
        {BAD, DDS_RETCODE_BAD_PARAMETER}
    };

    for (size_t i=0; i < sizeof (pars) / sizeof (pars[0]);i++) {
        result = dds_suspend(e[pars[i].index]);
        CU_ASSERT_EQUAL(result, pars[i].exp_res);
        result = dds_resume(e[pars[i].index]);
        CU_ASSERT_EQUAL(result, pars[i].exp_res);
    }
}

CU_Test(ddsc_unsupported, dds_get_instance_handle, .init = setup, .fini = teardown)
{
    dds_return_t result;
    dds_instance_handle_t ih;
    static struct index_result pars[] = {
        {TOP, DDS_RETCODE_OK},
        {PUB, DDS_RETCODE_OK},
        {SUB, DDS_RETCODE_OK},
        {RCD, DDS_RETCODE_OK},
        {BAD, DDS_RETCODE_BAD_PARAMETER}
    };

    for (size_t i=0; i < sizeof (pars) / sizeof (pars[0]);i++) {
        result = dds_get_instance_handle(e[pars[i].index], &ih);
        CU_ASSERT_EQUAL(result, pars[i].exp_res);
        if (pars[i].exp_res == DDS_RETCODE_OK) {
          CU_ASSERT(ih > 0);
        }
    }
}

CU_Test(ddsc_unsupported, dds_set_qos, .init = setup, .fini = teardown)
{
    dds_return_t result;
    dds_qos_t *qos;
    static struct index_result pars[] = {
        {RCD, DDS_RETCODE_ILLEGAL_OPERATION},
        {BAD, DDS_RETCODE_BAD_PARAMETER}
    };

    qos = dds_create_qos();
    for (size_t i=0; i < sizeof (pars) / sizeof (pars[0]);i++) {
        result = dds_set_qos(e[pars[i].index], qos);
        CU_ASSERT_EQUAL(result, pars[i].exp_res);
    }
    dds_delete_qos(qos);
}
