// Copyright(c) 2019 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <inttypes.h>

#include "dds/dds.h"
#include "dds__types.h"
#include "dds__entity.h"
#include "dds__topic.h"
#include "ddsi__thread.h"
#include "dds/ddsi/ddsi_serdata.h"
#include "dds/ddsi/ddsi_tkmap.h"
#include "dds/ddsc/dds_rhc.h"

#include "test_common.h"

#include "Space.h"

static dds_entity_t pp = 0;
static dds_entity_t tp = 0;
static dds_entity_t rd = 0;
static dds_entity_t wr = 0;

static Space_Type1 msg = { 123, 0, 0};


static void setup(dds_duration_t sep, dds_destination_order_kind_t dok)
{
    pp = dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);
    CU_ASSERT_FATAL(pp > 0);

    char name[100];
    create_unique_topic_name("ddsc_time_based_filter", name, sizeof name);
    tp = dds_create_topic(pp, &Space_Type1_desc, name, NULL, NULL);
    CU_ASSERT_FATAL(tp > 0);

    //qos
    dds_qos_t *qos = dds_create_qos();
    CU_ASSERT_PTR_NOT_NULL_FATAL(qos);

    dds_qset_history(qos, DDS_HISTORY_KEEP_LAST, 1);
    dds_qset_destination_order(qos, dok);
    dds_qset_time_based_filter(qos, sep);

    rd = dds_create_reader (pp, tp, qos, NULL);

    wr = dds_create_writer(pp, tp, qos, NULL);

    dds_delete_qos(qos);

    CU_ASSERT_FATAL(rd > 0);
    CU_ASSERT_FATAL(wr > 0);

    dds_return_t rc = dds_set_status_mask(wr, DDS_PUBLICATION_MATCHED_STATUS);
    CU_ASSERT_EQUAL_FATAL(rc, DDS_RETCODE_OK);

    uint32_t status = 0;
    while(!(status & DDS_PUBLICATION_MATCHED_STATUS))
    {
        rc = dds_get_status_changes (wr, &status);
        CU_ASSERT_EQUAL_FATAL(rc, DDS_RETCODE_OK);

        /* Polling sleep. */
        dds_sleepfor (DDS_MSECS (20));
    }
}

static void teardown(void)
{
    dds_return_t ret = dds_delete(pp);
    CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);
}

static void test_write(dds_duration_t source_time, uint32_t total_count, int32_t total_count_change)
{
    msg.long_2++;

    dds_return_t ret = dds_write_ts(wr, &msg, source_time);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

    dds_sample_lost_status_t sl_status;
    ret = dds_get_sample_lost_status (rd, &sl_status);
    CU_ASSERT_EQUAL(ret, DDS_RETCODE_OK);
    CU_ASSERT_EQUAL(sl_status.total_count, total_count);
    CU_ASSERT_EQUAL(sl_status.total_count_change, total_count_change);
}

static struct ddsi_domaingv *get_gv (dds_entity_t e)
{
  struct ddsi_domaingv *gv;
  dds_entity *x;
  if (dds_entity_pin (e, &x) < 0)
    abort ();
  gv = &x->m_domain->gv;
  dds_entity_unpin (x);
  return gv;
}

static void test_insert(void)
{
    dds_entity *e_ptr = NULL;
    struct dds_topic *x = NULL;
    struct ddsi_sertype *st = NULL;
    struct ddsi_serdata *sd = NULL;

    dds_return_t ret = dds_entity_pin(rd, &e_ptr);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    CU_ASSERT_PTR_NOT_NULL_FATAL(e_ptr);
    if (!e_ptr)
      goto fail;

    /*implementation TBD*/
    ddsi_thread_state_awake_domain_ok (ddsi_lookup_thread_state ());

    ret = dds_topic_pin (tp, &x);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    CU_ASSERT_PTR_NOT_NULL_FATAL(x);
    if (!x)
      goto fail;

    st = ddsi_sertype_ref (x->m_stype);
    CU_ASSERT_PTR_NOT_NULL_FATAL(st);
    if (!st)
      goto fail;

    msg.long_2++;
    sd = ddsi_serdata_from_sample (st, SDK_DATA, &msg);
    CU_ASSERT_PTR_NOT_NULL_FATAL(sd);
    if (!sd)
      goto fail;

    sd->twrite.v = DDS_TIME_INVALID;

  struct ddsi_writer_info wi = {
    .guid = { .prefix = { .u = {1,1,1} }, .entityid = { .u = 0x102 } },
    .auto_dispose = false,
    .ownership_strength = 0,
    .iid = 1
#ifdef DDS_HAS_LIFESPAN
    , .lifespan_exp = { DDS_INFINITY }
#endif
    };
    const struct ddsi_domaingv *gv = get_gv (pp);
    CU_ASSERT_PTR_NOT_NULL_FATAL(gv);
    if (!gv)
      goto fail;

    struct ddsi_tkmap_instance *ti = ddsi_tkmap_lookup_instance_ref(gv->m_tkmap, sd);
    CU_ASSERT_PTR_NOT_NULL_FATAL(ti);
    if (!ti)
      goto fail;

    bool store_result = dds_rhc_store(((struct dds_reader*)e_ptr)->m_rhc, &wi, sd, ti);
    CU_ASSERT(store_result);

fail:
    if (sd)
      ddsi_serdata_unref(sd);
    if (st)
      ddsi_sertype_unref(st);
    if (x)
      dds_topic_unpin (x);
    if (e_ptr)
      dds_entity_unpin(e_ptr);

    ddsi_thread_state_asleep (ddsi_lookup_thread_state ());
}

CU_Test(ddsc_time_based_filter, filter_reception_no_separation)
{
    setup(DDS_MSECS(0), DDS_DESTINATIONORDER_BY_RECEPTION_TIMESTAMP);

    test_write(DDS_MSECS(1), 0, 0);
    test_write(DDS_MSECS(0), 0, 0);
    test_write(DDS_MSECS(1), 0, 0);

    teardown();
}

CU_Test(ddsc_time_based_filter, filter_reception_separation)
{
    setup(DDS_MSECS(100), DDS_DESTINATIONORDER_BY_RECEPTION_TIMESTAMP);

    test_write(DDS_MSECS(1), 0, 0);
    test_write(DDS_MSECS(0), 1, 1);
    test_write(DDS_MSECS(1), 2, 1);
    test_write(DDS_MSECS(101), 2, 0);

    teardown();
}

CU_Test(ddsc_time_based_filter, filter_source_no_separation)
{
    setup(DDS_MSECS(0), DDS_DESTINATIONORDER_BY_SOURCE_TIMESTAMP);

    test_write(DDS_MSECS(1), 0, 0);
    test_write(DDS_MSECS(0), 1, 1);
    test_write(DDS_MSECS(1), 1, 0);

    teardown();
}

CU_Test(ddsc_time_based_filter, filter_source_separation)
{
    setup(DDS_MSECS(100), DDS_DESTINATIONORDER_BY_SOURCE_TIMESTAMP);

    test_write(DDS_MSECS(1), 0, 0);
    test_write(DDS_MSECS(0), 1, 1);
    test_write(DDS_MSECS(1), 2, 1);
    test_write(DDS_MSECS(101), 2, 0);

    teardown();
}

CU_Test(ddsc_time_based_filter, filter_reception_invalid_timestamp)
{
    setup(DDS_MSECS(100), DDS_DESTINATIONORDER_BY_RECEPTION_TIMESTAMP);

    test_write(DDS_MSECS(0), 0, 0);
    test_insert();

    teardown();
}

CU_Test(ddsc_time_based_filter, filter_source_invalid_timestamp)
{
    setup(DDS_MSECS(100), DDS_DESTINATIONORDER_BY_SOURCE_TIMESTAMP);

    test_write(DDS_MSECS(0), 0, 0);
    test_insert();

    teardown();
}
