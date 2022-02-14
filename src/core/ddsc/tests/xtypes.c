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
#include <assert.h>
#include <limits.h>

#include "dds/dds.h"
#include "config_env.h"

#include "dds/version.h"
#include "dds__entity.h"
#include "dds/ddsi/q_entity.h"
#include "dds/ddsi/ddsi_entity_index.h"
#include "dds/ddsi/ddsi_typelib.h"
#include "dds/ddsrt/cdtors.h"
#include "dds/ddsrt/misc.h"
#include "dds/ddsrt/process.h"
#include "dds/ddsrt/threads.h"
#include "dds/ddsrt/environ.h"
#include "dds/ddsrt/atomics.h"
#include "dds/ddsrt/time.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/string.h"
#include "test_common.h"

#include "XSpace.h"
#include "XSpaceEnum.h"
#include "XSpaceMustUnderstand.h"

#define DDS_DOMAINID_PUB 0
#define DDS_DOMAINID_SUB 1
#define DDS_CONFIG "${CYCLONEDDS_URI}${CYCLONEDDS_URI:+,}<Discovery><ExternalDomainId>0</ExternalDomainId></Discovery>"

static dds_entity_t g_domain1 = 0;
static dds_entity_t g_participant1 = 0;
static dds_entity_t g_publisher1 = 0;

static dds_entity_t g_domain2 = 0;
static dds_entity_t g_participant2 = 0;
static dds_entity_t g_subscriber2 = 0;

typedef void (*sample_init) (void *s);
typedef void (*sample_cmp) (void *s1, void *s2);

static void xtypes_init (void)
{
  /* Domains for pub and sub use a different internal domain id, but the external
   * domain id in configuration is 0, so that both domains will map to the same port number.
   * This allows to create two domains in a single test process. */
  char *conf1 = ddsrt_expand_envvars (DDS_CONFIG, DDS_DOMAINID_PUB);
  char *conf2 = ddsrt_expand_envvars (DDS_CONFIG, DDS_DOMAINID_SUB);
  g_domain1 = dds_create_domain (DDS_DOMAINID_PUB, conf1);
  g_domain2 = dds_create_domain (DDS_DOMAINID_SUB, conf2);
  dds_free (conf1);
  dds_free (conf2);

  g_participant1 = dds_create_participant (DDS_DOMAINID_PUB, NULL, NULL);
  CU_ASSERT_FATAL (g_participant1 > 0);
  g_participant2 = dds_create_participant (DDS_DOMAINID_SUB, NULL, NULL);
  CU_ASSERT_FATAL (g_participant2 > 0);

  g_publisher1 = dds_create_publisher (g_participant1, NULL, NULL);
  CU_ASSERT_FATAL (g_publisher1 > 0);
  g_subscriber2 = dds_create_subscriber (g_participant2, NULL, NULL);
  CU_ASSERT_FATAL (g_subscriber2 > 0);
}

static void xtypes_fini (void)
{
  dds_delete (g_domain2);
  dds_delete (g_domain1);
}

static bool reader_wait_for_data (dds_entity_t pp, dds_entity_t rd, dds_duration_t dur)
{
  dds_attach_t triggered;
  dds_entity_t ws = dds_create_waitset (pp);
  CU_ASSERT_FATAL (ws > 0);
  dds_return_t ret = dds_waitset_attach (ws, rd, rd);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);
  ret = dds_waitset_wait (ws, &triggered, 1, dur);
  if (ret > 0)
    CU_ASSERT_EQUAL_FATAL (rd, (dds_entity_t)(intptr_t) triggered);
  dds_delete (ws);
  return ret > 0;
}

static void check_assignable (const dds_topic_descriptor_t *rd_desc, const dds_topic_descriptor_t *wr_desc, bool assignable, sample_init fn_init, bool read_sample, sample_cmp fn_cmp)
{
  char topic_name[100];
  dds_return_t ret;

  create_unique_topic_name ("ddsc_xtypes", topic_name, sizeof (topic_name));
  dds_entity_t topic_wr = dds_create_topic (g_participant1, wr_desc, topic_name, NULL, NULL);
  CU_ASSERT_FATAL (topic_wr > 0);
  dds_entity_t topic_rd = dds_create_topic (g_participant2, rd_desc, topic_name, NULL, NULL);
  CU_ASSERT_FATAL (topic_rd > 0);

  dds_qos_t *qos = dds_create_qos ();
  dds_qset_reliability (qos, DDS_RELIABILITY_RELIABLE, DDS_SECS (10));
  dds_qset_history (qos, DDS_HISTORY_KEEP_ALL, 0);
  dds_qset_data_representation (qos, 1, (dds_data_representation_id_t[]) { DDS_DATA_REPRESENTATION_XCDR2 });
  CU_ASSERT_FATAL (qos != NULL);

  dds_entity_t writer = dds_create_writer (g_participant1, topic_wr, qos, NULL);
  CU_ASSERT_FATAL (writer > 0);
  dds_entity_t reader = dds_create_reader (g_participant2, topic_rd, qos, NULL);
  CU_ASSERT_FATAL (reader > 0);
  dds_delete_qos (qos);

  if (assignable)
  {
    sync_reader_writer (g_participant2, reader, g_participant1, writer);
    ret = dds_set_status_mask (reader, DDS_DATA_AVAILABLE_STATUS);
    CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);

    void * wr_sample = dds_alloc (wr_desc->m_size);
    fn_init (wr_sample);
    ret = dds_write (writer, wr_sample);
    CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);

    void * rd_sample = dds_alloc (rd_desc->m_size);
    void * rd_samples[1];
    rd_samples[0] = rd_sample;
    dds_sample_info_t info;
    bool data = reader_wait_for_data (g_participant2, reader, DDS_MSECS (500));
    CU_ASSERT_FATAL (data == read_sample);
    if (data)
    {
      CU_ASSERT_FATAL (data);
      ret = dds_take (reader, rd_samples, &info, 1, 1);
      CU_ASSERT_EQUAL_FATAL (ret, 1);
      if (fn_cmp)
        fn_cmp (wr_sample, rd_sample);
    }
    dds_sample_free (wr_sample, wr_desc, DDS_FREE_ALL);
    dds_sample_free (rd_sample, rd_desc, DDS_FREE_ALL);
  }
  else
  {
    no_sync_reader_writer (g_participant2, reader, g_participant1, writer, DDS_MSECS (200));
  }
}


/* Basic assignability test cases */
static void sample_init_XType1 (void *ptr)
{
  XSpace_XType1 *sample = (XSpace_XType1 *) ptr;
  sample->long_1 = 1;
  sample->long_2 = 2;
  sample->bm_3 = XSpace_flag0 | XSpace_flag1;
}
static void sample_init_XType1a (void *ptr)
{
  XSpace_XType1a *sample = (XSpace_XType1a *) ptr;
  sample->long_1 = 1;
  sample->long_2 = 2;
  sample->bm_3 = 3;
}
static void sample_cmp_XType1_1a (void *ptr1, void *ptr2)
{
  XSpace_XType1 *s_wr = (XSpace_XType1 *) ptr1;
  XSpace_XType1a *s_rd = (XSpace_XType1a *) ptr2;
  CU_ASSERT_EQUAL_FATAL (s_rd->long_1, s_wr->long_1);
  CU_ASSERT_EQUAL_FATAL (s_rd->long_2, s_wr->long_2);
  CU_ASSERT_EQUAL_FATAL (s_rd->bm_3, s_wr->bm_3);
}
static void sample_cmp_XType1a_1 (void *ptr1, void *ptr2)
{
  XSpace_XType1a *s_wr = (XSpace_XType1a *) ptr1;
  XSpace_XType1 *s_rd = (XSpace_XType1 *) ptr2;
  CU_ASSERT_EQUAL_FATAL (s_rd->long_1, s_wr->long_1);
  CU_ASSERT_EQUAL_FATAL (s_rd->long_2, s_wr->long_2);
  CU_ASSERT_EQUAL_FATAL (s_rd->bm_3, s_wr->bm_3);
}

static void sample_init_XType2 (void *ptr)
{
  XSpace_XType2 *sample = (XSpace_XType2 *) ptr;
  sample->long_1 = 1;
  sample->long_2 = 2;
}
static void sample_init_XType2a (void *ptr)
{
  XSpace_XType2a *sample = (XSpace_XType2a *) ptr;
  sample->long_1 = 1;
  sample->long_2 = 2;
  sample->long_3 = 3;
}
static void sample_cmp_XType2_2a (void *ptr1, void *ptr2)
{
  XSpace_XType2 *s_wr = (XSpace_XType2 *) ptr1;
  XSpace_XType2a *s_rd = (XSpace_XType2a *) ptr2;
  CU_ASSERT_EQUAL_FATAL (s_rd->long_1, s_wr->long_1);
  CU_ASSERT_EQUAL_FATAL (s_rd->long_2, s_wr->long_2);
  CU_ASSERT_EQUAL_FATAL (s_rd->long_3, 0);
}
static void sample_cmp_XType2a_2 (void *ptr1, void *ptr2)
{
  XSpace_XType2a *s_wr = (XSpace_XType2a *) ptr1;
  XSpace_XType2 *s_rd = (XSpace_XType2 *) ptr2;
  CU_ASSERT_EQUAL_FATAL (s_rd->long_1, s_wr->long_1);
  CU_ASSERT_EQUAL_FATAL (s_rd->long_2, s_wr->long_2);
}

static void sample_init_XType3 (void *ptr)
{
  XSpace_XType3 *sample = (XSpace_XType3 *) ptr;
  sample->long_2 = 2;
  sample->struct_3.long_4 = 4;
  sample->struct_3.long_5 = 5;
}
static void sample_init_XType3a (void *ptr)
{
  XSpace_XType3a *sample = (XSpace_XType3a *) ptr;
  sample->long_1 = 1;
  sample->long_2 = 2;
  sample->struct_3.long_4 = 4;
}
static void sample_cmp_XType3_3a (void *ptr1, void *ptr2)
{
  XSpace_XType3 *s_wr = (XSpace_XType3 *) ptr1;
  XSpace_XType3a *s_rd = (XSpace_XType3a *) ptr2;
  CU_ASSERT_EQUAL_FATAL (s_rd->long_1, 0);
  CU_ASSERT_EQUAL_FATAL (s_rd->long_2, s_wr->long_2);
  CU_ASSERT_EQUAL_FATAL (s_rd->struct_3.long_4, s_wr->struct_3.long_4);
}
static void sample_cmp_XType3a_3 (void *ptr1, void *ptr2)
{
  XSpace_XType3a *s_wr = (XSpace_XType3a *) ptr1;
  XSpace_XType3 *s_rd = (XSpace_XType3 *) ptr2;
  CU_ASSERT_EQUAL_FATAL (s_rd->long_2, s_wr->long_2);
  CU_ASSERT_EQUAL_FATAL (s_rd->struct_3.long_4, s_wr->struct_3.long_4);
  CU_ASSERT_EQUAL_FATAL (s_rd->struct_3.long_5, 0);
}

#define D(n) XSpace_ ## n ## _desc
#define I(n) sample_init_ ## n
#define C(n) sample_cmp_ ## n
CU_TheoryDataPoints (ddsc_xtypes, basic) = {
  CU_DataPoints (const char *,                   "mutable_bitmask",
  /*                                             |                      */"appendable_field",
  /*                                             |                       |                       */"appendable_nested"),
  CU_DataPoints (const dds_topic_descriptor_t *, &D(XType1),             &D(XType2),              &D(XType3),             ),
  CU_DataPoints (const dds_topic_descriptor_t *, &D(XType1a),            &D(XType2a),             &D(XType3a),            ),
  CU_DataPoints (sample_init,                    I(XType1),              I(XType2),               I(XType3),              ),
  CU_DataPoints (sample_init,                    I(XType1a),             I(XType2a),              I(XType3a),             ),
  CU_DataPoints (sample_cmp,                     C(XType1_1a),           C(XType2_2a),            C(XType3_3a),           ),
  CU_DataPoints (sample_cmp,                     C(XType1a_1),           C(XType2a_2),            C(XType3a_3),           ),
};

CU_Theory ((const char *descr, const dds_topic_descriptor_t *desc1, const dds_topic_descriptor_t *desc2, sample_init fn_init1, sample_init fn_init2, sample_cmp fn_cmp1, sample_cmp fn_cmp2),
    ddsc_xtypes, basic, .init = xtypes_init, .fini = xtypes_fini)
{
  for (int t = 0; t <= 1; t++)
  {
    printf ("Running test xtypes_basic: %s (run %d/2)\n", descr, t + 1);

    const dds_topic_descriptor_t *wr_desc = t ? desc2 : desc1,
                                 *rd_desc = t ? desc1 : desc2;
    sample_init fn_init = t ? fn_init2 : fn_init1;
    sample_cmp fn_cmp = t ? fn_cmp2 : fn_cmp1;
    check_assignable (rd_desc, wr_desc, true, fn_init, true, fn_cmp);
  }
}
#undef D
#undef I
#undef C


/* Must-understand test cases */
static void sample_init_mu_wr1_2 (void *ptr)
{
  XSpaceMustUnderstand_wr1_2 *sample = (XSpaceMustUnderstand_wr1_2 *) ptr;
  sample->f1 = 1;
}
static void sample_init_mu_wr1_3a (void *ptr)
{
  XSpaceMustUnderstand_wr1_3 *sample = (XSpaceMustUnderstand_wr1_3 *) ptr;
  sample->f1 = 1;
  sample->f2 = NULL;
}
static void sample_init_mu_wr1_3b (void *ptr)
{
  XSpaceMustUnderstand_wr1_3 *sample = (XSpaceMustUnderstand_wr1_3 *) ptr;
  sample->f1 = 1;
  sample->f2 = dds_alloc (sizeof (*sample->f2));
  *(sample->f2) = 1;
}
static void sample_init_mu_wr1_4a (void *ptr)
{
  XSpaceMustUnderstand_wr1_4 *sample = (XSpaceMustUnderstand_wr1_4 *) ptr;
  sample->f1 = dds_alloc (sizeof (*sample->f1));
  *(sample->f1) = 1;
}
static void sample_init_mu_wr1_4b (void *ptr)
{
  XSpaceMustUnderstand_wr1_4 *sample = (XSpaceMustUnderstand_wr1_4 *) ptr;
  sample->f1 = NULL;
}

static void sample_init_mu_wr2_1a (void *ptr)
{
  XSpaceMustUnderstand_wr2_1 *sample = (XSpaceMustUnderstand_wr2_1 *) ptr;
  sample->f1.f1 = 1;
  sample->f1.f2 = NULL;
}
static void sample_init_mu_wr2_1b (void *ptr)
{
  XSpaceMustUnderstand_wr2_1 *sample = (XSpaceMustUnderstand_wr2_1 *) ptr;
  sample->f1.f1 = 1;
  sample->f1.f2 = dds_alloc (sizeof (*sample->f1.f2));
  *(sample->f1.f2) = 1;
}

#define D(n) XSpaceMustUnderstand_ ## n ## _desc
#define I(n) sample_init_ ## n
CU_TheoryDataPoints (ddsc_xtypes, must_understand) = {
  CU_DataPoints (const dds_topic_descriptor_t *,  &D(rd1),   &D(rd1),     &D(rd1),      &D(rd1),      &D(rd1),      &D(rd1),      &D(rd2),      &D(rd2)      ),
  CU_DataPoints (const dds_topic_descriptor_t *,  &D(wr1_1), &D(wr1_2),   &D(wr1_3),    &D(wr1_3),    &D(wr1_4),    &D(wr1_4),    &D(wr2_1),    &D(wr2_1)    ),
  CU_DataPoints (bool,                            false,     true,        true,         true,         true,         true,         true,         true         ),
  CU_DataPoints (sample_init,                     0,         I(mu_wr1_2), I(mu_wr1_3a), I(mu_wr1_3b), I(mu_wr1_4a), I(mu_wr1_4b), I(mu_wr2_1a), I(mu_wr2_1b) ),
  CU_DataPoints (bool,                            false,     true,        true,         false,        true,         true,         true,         false        )
};
#undef D
#undef I

CU_Theory ((const dds_topic_descriptor_t *rd_desc, const dds_topic_descriptor_t *wr_desc, bool assignable, sample_init fn_init, bool read_sample),
    ddsc_xtypes, must_understand, .init = xtypes_init, .fini = xtypes_fini)
{
  printf ("Running test xtypes_must_understand: %s %s\n", wr_desc->m_typename, rd_desc->m_typename);
  check_assignable (rd_desc, wr_desc, assignable, fn_init, read_sample, 0);
}


/* Enum extensibility test cases */
static void sample_init_en_wr1_1 (void *ptr)
{
  XSpaceEnum_wr1_1 *sample = (XSpaceEnum_wr1_1 *) ptr;
  sample->f1 = XSpaceEnum_F1;
}
static void sample_init_en_wr2_1 (void *ptr)
{
  XSpaceEnum_wr2_1 *sample = (XSpaceEnum_wr2_1 *) ptr;
  sample->f1 = XSpaceEnum_A1;
}
static void sample_init_en_wr2_3 (void *ptr)
{
  XSpaceEnumPlus_wr2_3 *sample = (XSpaceEnumPlus_wr2_3 *) ptr;
  sample->f1 = XSpaceEnumPlus_A1;
}
static void sample_init_en_wr2_4 (void *ptr)
{
  XSpaceEnumMin_wr2_4 *sample = (XSpaceEnumMin_wr2_4 *) ptr;
  sample->f1 = XSpaceEnumMin_A1;
}

#define D(n) XSpaceEnum_ ## n ## _desc
#define DP(n) XSpaceEnumPlus_ ## n ## _desc
#define DM(n) XSpaceEnumMin_ ## n ## _desc
#define DL(n) XSpaceEnumLabel_ ## n ## _desc
#define I(n) sample_init_ ## n
CU_TheoryDataPoints (ddsc_xtypes, enum_extensibility) = {
  CU_DataPoints (const dds_topic_descriptor_t *,  &D(rd1),     &D(rd1),   &D(rd1),    &D(rd1),    &D(rd1),    &D(rd2),     &D(rd2),   &D(rd2),     &D(rd2),     &D(rd2)    ),
  CU_DataPoints (const dds_topic_descriptor_t *,  &D(wr1_1),   &D(wr1_2), &DP(wr1_3), &DM(wr1_4), &DL(wr1_5), &D(wr2_1),   &D(wr2_2), &DP(wr2_3),  &DM(wr2_4),  &DL(wr2_5) ),
  CU_DataPoints (bool,                            true,        false,     false,      false,      false,      true,        false,     true,        true,        false      ),
  CU_DataPoints (sample_init,                     I(en_wr1_1), 0,         0,          0,          0,          I(en_wr2_1), 0,         I(en_wr2_3), I(en_wr2_4), 0          ),
  CU_DataPoints (bool,                            true,        false,     false,      false,      false,      true,        false,     true,        true,        false      )
};
#undef D
#undef DP
#undef DM
#undef DL
#undef I

CU_Theory ((const dds_topic_descriptor_t *rd_desc, const dds_topic_descriptor_t *wr_desc, bool assignable, sample_init fn_init, bool read_sample),
    ddsc_xtypes, enum_extensibility, .init = xtypes_init, .fini = xtypes_fini)
{
  printf ("Running test xtypes_enum: %s %s\n", wr_desc->m_typename, rd_desc->m_typename);
  check_assignable (rd_desc, wr_desc, assignable, fn_init, read_sample, 0);
}
