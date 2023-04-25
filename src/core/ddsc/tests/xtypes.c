// Copyright(c) 2006 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <assert.h>
#include <limits.h>

#include "dds/ddsrt/cdtors.h"
#include "dds/ddsrt/misc.h"
#include "dds/ddsrt/process.h"
#include "dds/ddsrt/threads.h"
#include "dds/ddsrt/environ.h"
#include "dds/ddsrt/atomics.h"
#include "dds/ddsrt/time.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsi/ddsi_entity.h"
#include "dds/ddsi/ddsi_entity_index.h"
#include "dds/ddsi/ddsi_typelib.h"
#include "dds/ddsi/ddsi_xt_typelookup.h"
#include "ddsi__xt_impl.h"
#include "ddsi__addrset.h"
#include "ddsi__endpoint_match.h"
#include "ddsi__proxy_endpoint.h"
#include "ddsi__proxy_participant.h"
#include "ddsi__typelookup.h"
#include "ddsi__typewrap.h"
#include "ddsi__vendor.h"
#include "dds/cdr/dds_cdrstream.h"
#include "dds/dds.h"
#include "dds/version.h"
#include "dds__entity.h"
#include "config_env.h"
#include "test_common.h"

#include "XSpace.h"
#include "XSpaceEnum.h"
#include "XSpaceMustUnderstand.h"
#include "XSpaceTypeConsistencyEnforcement.h"
#include "XSpaceNoTypeInfo.h"

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
typedef void (*sample_check) (void *s1, void *s2);
typedef void (*typeobj_modify) (dds_sequence_DDS_XTypes_TypeIdentifierTypeObjectPair *type_id_obj_seq, uint32_t kind);

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

static void do_test (const dds_topic_descriptor_t *rd_desc, const dds_qos_t *add_rd_qos, const dds_topic_descriptor_t *wr_desc, const dds_qos_t *add_wr_qos, bool assignable, sample_init fn_init, bool read_sample, sample_check fn_check)
{
  dds_return_t ret;
  char topic_name[100];
  create_unique_topic_name ("ddsc_xtypes", topic_name, sizeof (topic_name));
  dds_entity_t topic_wr = dds_create_topic (g_participant1, wr_desc, topic_name, NULL, NULL);
  CU_ASSERT_FATAL (topic_wr > 0);
  dds_entity_t topic_rd = dds_create_topic (g_participant2, rd_desc, topic_name, NULL, NULL);
  CU_ASSERT_FATAL (topic_rd > 0);

  dds_qos_t *qos = dds_create_qos (), *wrqos = dds_create_qos (), *rdqos = dds_create_qos ();
  CU_ASSERT_FATAL (qos != NULL);
  dds_qset_reliability (qos, DDS_RELIABILITY_RELIABLE, DDS_SECS (10));
  dds_qset_history (qos, DDS_HISTORY_KEEP_ALL, 0);
  dds_qset_data_representation (qos, 1, (dds_data_representation_id_t[]) { DDS_DATA_REPRESENTATION_XCDR2 });
  dds_copy_qos (wrqos, qos);
  dds_copy_qos (rdqos, qos);

  if (add_wr_qos)
    dds_merge_qos (wrqos, add_wr_qos);
  dds_entity_t writer = dds_create_writer (g_participant1, topic_wr, wrqos, NULL);
  CU_ASSERT_FATAL (writer > 0);

  if (add_rd_qos)
    dds_merge_qos (rdqos, add_rd_qos);
  dds_entity_t reader = dds_create_reader (g_participant2, topic_rd, rdqos, NULL);
  CU_ASSERT_FATAL (reader > 0);

  dds_delete_qos (qos);
  dds_delete_qos (wrqos);
  dds_delete_qos (rdqos);

  if (assignable)
  {
    sync_reader_writer (g_participant2, reader, g_participant1, writer);
    ret = dds_set_status_mask (reader, DDS_DATA_AVAILABLE_STATUS);
    CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);

    if (fn_init)
    {
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
        ret = dds_take (reader, rd_samples, &info, 1, 1);
        CU_ASSERT_EQUAL_FATAL (ret, 1);
        if (fn_check)
          fn_check (wr_sample, rd_sample);
      }
      dds_sample_free (wr_sample, wr_desc, DDS_FREE_ALL);
      dds_sample_free (rd_sample, rd_desc, DDS_FREE_ALL);
    }
  }
  else
  {
    no_sync_reader_writer (g_participant2, reader, g_participant1, writer, DDS_MSECS (200));
  }
}

/* Some basic tests */
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
static void sample_check_XType1_1a (void *ptr1, void *ptr2)
{
  XSpace_XType1 *s_wr = (XSpace_XType1 *) ptr1;
  XSpace_XType1a *s_rd = (XSpace_XType1a *) ptr2;
  CU_ASSERT_EQUAL_FATAL (s_rd->long_1, s_wr->long_1);
  CU_ASSERT_EQUAL_FATAL (s_rd->long_2, s_wr->long_2);
  CU_ASSERT_EQUAL_FATAL (s_rd->bm_3, s_wr->bm_3);
}
static void sample_check_XType1a_1 (void *ptr1, void *ptr2)
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
static void sample_check_XType2_2a (void *ptr1, void *ptr2)
{
  XSpace_XType2 *s_wr = (XSpace_XType2 *) ptr1;
  XSpace_XType2a *s_rd = (XSpace_XType2a *) ptr2;
  CU_ASSERT_EQUAL_FATAL (s_rd->long_1, s_wr->long_1);
  CU_ASSERT_EQUAL_FATAL (s_rd->long_2, s_wr->long_2);
  CU_ASSERT_EQUAL_FATAL (s_rd->long_3, 0);
}
static void sample_check_XType2a_2 (void *ptr1, void *ptr2)
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
static void sample_check_XType3_3a (void *ptr1, void *ptr2)
{
  XSpace_XType3 *s_wr = (XSpace_XType3 *) ptr1;
  XSpace_XType3a *s_rd = (XSpace_XType3a *) ptr2;
  CU_ASSERT_EQUAL_FATAL (s_rd->long_1, 0);
  CU_ASSERT_EQUAL_FATAL (s_rd->long_2, s_wr->long_2);
  CU_ASSERT_EQUAL_FATAL (s_rd->struct_3.long_4, s_wr->struct_3.long_4);
}
static void sample_check_XType3a_3 (void *ptr1, void *ptr2)
{
  XSpace_XType3a *s_wr = (XSpace_XType3a *) ptr1;
  XSpace_XType3 *s_rd = (XSpace_XType3 *) ptr2;
  CU_ASSERT_EQUAL_FATAL (s_rd->long_2, s_wr->long_2);
  CU_ASSERT_EQUAL_FATAL (s_rd->struct_3.long_4, s_wr->struct_3.long_4);
  CU_ASSERT_EQUAL_FATAL (s_rd->struct_3.long_5, 0);
}

static void sample_init_XType4 (void *ptr)
{
  XSpace_XType4 *sample = (XSpace_XType4 *) ptr;
  uint32_t *s1 = ddsrt_malloc (999 * sizeof (*s1));
  for (uint32_t n = 0; n < 999; n++) s1[n] = n;
  uint32_t *s2 = ddsrt_malloc (10 * sizeof (*s2));
  for (uint32_t n = 0; n < 10; n++) s2[n] = n;
  sample->seq_1 = (dds_sequence_uint32) { ._length = 999, ._maximum = 999, ._buffer = s1, ._release = true };
  sample->seq_2 = (dds_sequence_uint32) { ._length = 10, ._maximum = 10, ._buffer = s2, ._release = true };
}
static void sample_init_XType4a (void *ptr)
{
  XSpace_XType4a *sample = (XSpace_XType4a *) ptr;
  uint32_t *s1 = ddsrt_malloc (999 * sizeof (*s1));
  for (uint32_t n = 0; n < 999; n++) s1[n] = n;
  uint32_t *s2 = ddsrt_malloc (5 * sizeof (*s2));
  for (uint32_t n = 0; n < 5; n++) s2[n] = n;
  sample->seq_1 = (dds_sequence_uint32) { ._length = 999, ._maximum = 999, ._buffer = s1, ._release = true };
  sample->seq_2 = (dds_sequence_uint32) { ._length = 5, ._maximum = 5, ._buffer = s2, ._release = true };
}
static void sample_check_XType4a_4 (void *ptr1, void *ptr2)
{
  XSpace_XType4a *s_wr = (XSpace_XType4a *) ptr1;
  XSpace_XType4 *s_rd = (XSpace_XType4 *) ptr2;
  CU_ASSERT_FATAL (s_rd->seq_1._length == s_wr->seq_1._length && s_rd->seq_1._length == 999);
  CU_ASSERT_FATAL (s_rd->seq_2._length == s_wr->seq_2._length && s_rd->seq_2._length == 5);
  for (uint32_t n = 0; n < 999; n++)
    CU_ASSERT_FATAL (s_rd->seq_1._buffer[n] == s_wr->seq_1._buffer[n] && s_rd->seq_1._buffer[n] == n);
  for (uint32_t n = 0; n < 5; n++)
    CU_ASSERT_FATAL (s_rd->seq_2._buffer[n] == s_wr->seq_2._buffer[n] && s_rd->seq_2._buffer[n] == n);
}

static void sample_init_XType5a (void *ptr)
{
  XSpace_XType5a *sample = (XSpace_XType5a *) ptr;
  for (uint32_t n = 0; n < 999; n++) sample->str_1[n] = 'a';
  for (uint32_t n = 0; n < 5; n++) sample->str_2[n] = 'a';
  sample->str_1[999] = '\0';
  sample->str_2[5] = '\0';
}
static void sample_check_XType5a_5 (void *ptr1, void *ptr2)
{
  XSpace_XType5a *s_wr = (XSpace_XType5a *) ptr1;
  XSpace_XType5 *s_rd = (XSpace_XType5 *) ptr2;
  CU_ASSERT_FATAL (strlen (s_rd->str_1) == strlen (s_wr->str_1) && strlen (s_rd->str_1) == 999);
  CU_ASSERT_FATAL (strlen (s_rd->str_2) == strlen (s_wr->str_2) && strlen (s_rd->str_2) == 5);
  CU_ASSERT_FATAL (!strcmp (s_rd->str_1, s_wr->str_1));
  CU_ASSERT_FATAL (!strcmp (s_rd->str_2, s_wr->str_2));
}

#define D(n) XSpace_ ## n ## _desc
#define I(n) sample_init_ ## n
#define C(n) sample_check_ ## n
CU_TheoryDataPoints (ddsc_xtypes, basic) = {
  CU_DataPoints (const char *,                   "mutable_bitmask",
  /*                                             |                      */"appendable_field",
  /*                                             |                       |                       */"appendable_nested",
  /*                                             |                       |                        |              */"mutable_seq",
  /*                                             |                       |                        |               |              */"strlen_keys"      ),
  CU_DataPoints (const dds_topic_descriptor_t *, &D(XType1),             &D(XType2),              &D(XType3),     &D(XType4),     &D(XType5),         ),
  CU_DataPoints (const dds_topic_descriptor_t *, &D(XType1a),            &D(XType2a),             &D(XType3a),    &D(XType4a),    &D(XType5a),        ),
  CU_DataPoints (sample_init,                    I(XType1),              I(XType2),               I(XType3),      I(XType4),      NULL,               ),
  CU_DataPoints (sample_init,                    I(XType1a),             I(XType2a),              I(XType3a),     I(XType4a),     I(XType5a),         ),
  CU_DataPoints (sample_check,                   C(XType1_1a),           C(XType2_2a),            C(XType3_3a),   NULL,           NULL,               ),
  CU_DataPoints (sample_check,                   C(XType1a_1),           C(XType2a_2),            C(XType3a_3),   C(XType4a_4),   C(XType5a_5),       ),
};

CU_Theory ((const char *descr, const dds_topic_descriptor_t *desc1, const dds_topic_descriptor_t *desc2, sample_init fn_init1, sample_init fn_init2, sample_check fn_check1, sample_check fn_check2),
    ddsc_xtypes, basic, .init = xtypes_init, .fini = xtypes_fini)
{
  for (int t = 0; t <= 1; t++)
  {
    printf ("Running test xtypes_basic: %s (run %d/2)\n", descr, t + 1);
    sample_init i = t ? fn_init2 : fn_init1;
    sample_check c = t ? fn_check2 : fn_check1;
    do_test (t ? desc1 : desc2, NULL, t ? desc2 : desc1, NULL, i != NULL, i, c != NULL, c);
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

CU_Theory ((const dds_topic_descriptor_t *rd_desc, const dds_topic_descriptor_t *wr_desc, bool assignable, sample_init fn_init, bool read_sample),
    ddsc_xtypes, must_understand, .init = xtypes_init, .fini = xtypes_fini)
{
  printf ("Running test xtypes_must_understand: %s %s\n", wr_desc->m_typename, rd_desc->m_typename);
  do_test (rd_desc, NULL, wr_desc, NULL, assignable, fn_init, read_sample, 0);
}
#undef D
#undef I

/* Type consistency enforcement policy test cases (ignore seq/str bounds, prevent type widening, allow type coercion) */
#define D(n) (&XSpaceTypeConsistencyEnforcement_ ## n ## _desc)
#define I(n) sample_init_tce_ ## n
#define C(n) sample_check_tce_ ## n
#define ALLOW   DDS_TYPE_CONSISTENCY_ALLOW_TYPE_COERCION
#define DISALW  DDS_TYPE_CONSISTENCY_DISALLOW_TYPE_COERCION
#define DT true
#define DF false
#define DDS_TD_T const dds_topic_descriptor_t *
#define DDS_TCE_T dds_type_consistency_kind_t

CU_TheoryDataPoints (ddsc_xtypes, type_consistency_enforcement) = {
  CU_DataPoints (const char *,
                            "wr seq bound > rd seq bound, but ignore_seq_bounds",
                                     "wr seq bound > rd seq bound, !ignore_seq_bounds",
                                              "wr seq unbounded > rd seq bound, !ignore_seq_bounds",
                                                       "wr seq bound < rd seq unbound, !ignore_seq_bounds",
                                                                "disallow coercion, same type",
                                                                         "disallow coercion, different (assignable) type",
                                                                                  "wr str bound > rd str bound, but ignore_str_bounds",
                                                                                           "wr str bound > rd str bound, !ignore_str_bounds",
                                                                                                    "wr str unbounded > rd str bound, !ignore_str_bounds",
                                                                                                             "wr str bound < rd str unbound, !ignore_str_bounds",
                                                                                                                      "member names different, !ignore_member_names",
                                                                                                                               "member names different, ignore_member_names",
                                                                                                                                        "union member names different, !ignore_member_names",
                                                                                                                                                 "union member names different, !ignore_member_names",
                                                                                                                                                          "widen type, !prevent_type_widening",
                                                                                                                                                                   "widen type, prevent_type_widening",
                                                                                                                                                                            "same members, prevent_type_widening",
                                                                                                                                                                                     "widen with optional member, prevent_type_widening",
                                                                                                                                                                                              "widen mutable type, !prevent_type_widening",
                                                                                                                                                                                                       "widen mutable type, prevent_type_widening",
                                                                                                                                                                                                                "widen union type, !prevent_type_widening",
                                                                                                                                                                                                                         "widen union type, prevent_type_widening"),
  CU_DataPoints (DDS_TD_T,  D(t1_1), D(t1_1), D(t1_1), D(t1_3), D(t1_1), D(t1_1), D(t2_1), D(t2_1), D(t2_1), D(t2_3), D(t3_1), D(t3_1), D(t4_1), D(t4_1), D(t5_1), D(t5_1), D(t5_1), D(t5_1), D(t6_1), D(t6_1), D(t7_1), D(t7_1)    ),
  CU_DataPoints (DDS_TD_T,  D(t1_2), D(t1_2), D(t1_3), D(t1_1), D(t1_1), D(t1_3), D(t2_2), D(t2_2), D(t2_3), D(t2_1), D(t3_2), D(t3_2), D(t4_2), D(t4_2), D(t5_2), D(t5_2), D(t5_3), D(t5_4), D(t6_2), D(t6_2), D(t7_2), D(t7_2)    ),

  CU_DataPoints (DDS_TCE_T, ALLOW  , ALLOW  , ALLOW  , ALLOW  , DISALW , DISALW , ALLOW  , ALLOW  , ALLOW  , ALLOW  , ALLOW  , ALLOW  , ALLOW  , ALLOW  , ALLOW  , ALLOW  , ALLOW  , ALLOW  , ALLOW  , ALLOW  , ALLOW  , ALLOW      ),  // allow/disallow type coercion
  CU_DataPoints (bool,      DF     , DF     , DF     , DF     , DF     , DF     , DF     , DF     , DF     , DF     , DF     , DF     , DF     , DF     , false  , true   , true   , true   , false  , true   , false  , true       ),  // prevent_type_widening
  CU_DataPoints (bool,      true   , false  , false  , false  , DT     , DT     , DT     , DT     , DT     , DT     , DT     , DT     , DT     , DT     , DT     , DT     , DT     , DT     , DT     , DT     , DT     , DT         ),  // ignore_seq_bounds
  CU_DataPoints (bool,      DT     , DT     , DT     , DT     , DT     , DT     , true   , false  , false  , false  , DT     , DT     , DT     , DT     , DT     , DT     , DT     , DT     , DT     , DT     , DT     , DT         ),  // ignore_str_bounds
  CU_DataPoints (bool,      DF     , DF     , DF     , DF     , DF     , DF     , DF     , DF     , DF     , DF     , false  , true   , false  , true   , DF     , DF     , DF     , DF     , DF     , DF     , DF     , DF         ),  // ignore_member_names
  CU_DataPoints (bool,      DF     , DF     , DF     , DF     , DF     , DF     , DF     , DF     , DF     , DF     , DF     , DF     , DF     , DF     , DF     , DF     , DF     , DF     , DF     , DF     , DF     , DF         ),  // force_type_validation

  CU_DataPoints (bool,      true   , false  , false  , true   , true   , false  , true   , false  , false  , true   , false  , true   , false  , true   , true   , false  , false  , true   , true   , false  , true   , false      ),  // expect match
};

CU_Theory ((const char *test, const dds_topic_descriptor_t *rd_desc, const dds_topic_descriptor_t *wr_desc, dds_type_consistency_kind_t kind,
    bool prevent_type_widening, bool ignore_seq_bounds, bool ignore_str_bounds, bool ignore_member_names, bool force_type_validation, bool assignable),
  ddsc_xtypes, type_consistency_enforcement, .init = xtypes_init, .fini = xtypes_fini)
{
  printf ("Running test xtypes_type_consistency_enforcement: %s wr %s rd %s\n", test, wr_desc->m_typename, rd_desc->m_typename);
  dds_qos_t *rd_qos = dds_create_qos ();
  dds_qset_type_consistency (rd_qos, kind, ignore_seq_bounds, ignore_str_bounds, ignore_member_names, prevent_type_widening, force_type_validation);
  do_test (rd_desc, rd_qos, wr_desc, NULL, assignable, 0, false, 0);
  dds_delete_qos (rd_qos);
}
#undef D
#undef I
#undef C
#undef ALLOW
#undef DISALLOW
#undef DT
#undef DF
#undef DDS_TD_T
#undef DDS_TCE_T

/* Type consistency enforcement policy test case for force_type_validation */
CU_Test (ddsc_xtypes, type_consistency_enforcement_force_validation, .init = xtypes_init, .fini = xtypes_fini)
{
  for (uint32_t n = 0; n <= 1; n++)
  {
    bool force_type_validation = (n == 1);
    printf ("Running test type_consistency_enforcement_force_validation: force_type_validation = %s\n", force_type_validation ? "true" : "false");
    dds_qos_t *rd_qos = dds_create_qos ();
    dds_qset_type_consistency (rd_qos, DDS_TYPE_CONSISTENCY_ALLOW_TYPE_COERCION, true, true, false, false, force_type_validation);
    do_test (&XSpaceNoTypeInfo_t1_desc, rd_qos, &XSpaceNoTypeInfo_t1_desc, NULL, !force_type_validation, 0, false, 0);
    dds_delete_qos (rd_qos);
  }
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

CU_Theory ((const dds_topic_descriptor_t *rd_desc, const dds_topic_descriptor_t *wr_desc, bool assignable, sample_init fn_init, bool read_sample),
    ddsc_xtypes, enum_extensibility, .init = xtypes_init, .fini = xtypes_fini)
{
  printf ("Running test xtypes_enum: %s %s\n", wr_desc->m_typename, rd_desc->m_typename);
  do_test (rd_desc, NULL, wr_desc, NULL, assignable, fn_init, read_sample, 0);
}

#undef D
#undef DP
#undef DM
#undef DL
#undef I


static void typeinfo_ser (struct dds_type_meta_ser *ser, DDS_XTypes_TypeInformation *ti)
{
  dds_ostream_t os = { .m_buffer = NULL, .m_index = 0, .m_size = 0, .m_xcdr_version = DDSI_RTPS_CDR_ENC_VERSION_2 };
  xcdr2_ser (ti, &DDS_XTypes_TypeInformation_desc, &os);
  ser->data = os.m_buffer;
  ser->sz = os.m_index;
}

static void typeinfo_deser (DDS_XTypes_TypeInformation **ti, const struct dds_type_meta_ser *ser)
{
  xcdr2_deser (ser->data, ser->sz, (void **) ti, &DDS_XTypes_TypeInformation_desc);
}

static void typemap_ser (struct dds_type_meta_ser *ser, DDS_XTypes_TypeMapping *tmap)
{
  dds_ostream_t os = { .m_buffer = NULL, .m_index = 0, .m_size = 0, .m_xcdr_version = DDSI_RTPS_CDR_ENC_VERSION_2 };
  xcdr2_ser (tmap, &DDS_XTypes_TypeMapping_desc, &os);
  ser->data = os.m_buffer;
  ser->sz = os.m_index;
}

static void typemap_deser (DDS_XTypes_TypeMapping **tmap, const struct dds_type_meta_ser *ser)
{
  xcdr2_deser (ser->data, ser->sz, (void **) tmap, &DDS_XTypes_TypeMapping_desc);
}

static void test_proxy_rd_create (struct ddsi_domaingv *gv, const char *topic_name, DDS_XTypes_TypeInformation *ti, dds_return_t exp_ret, const ddsi_guid_t *pp_guid, const ddsi_guid_t *rd_guid)
{
  ddsi_plist_t *plist = ddsrt_calloc (1, sizeof (*plist));
  plist->qos.present |= DDSI_QP_TOPIC_NAME | DDSI_QP_TYPE_NAME | DDSI_QP_TYPE_INFORMATION | DDSI_QP_DATA_REPRESENTATION | DDSI_QP_LIVELINESS;
  plist->qos.topic_name = ddsrt_strdup (topic_name);
  plist->qos.type_name = ddsrt_strdup ("dummy");
  plist->qos.type_information = ddsi_typeinfo_dup ((struct ddsi_typeinfo *) ti);
  plist->qos.data_representation.value.n = 1;
  plist->qos.data_representation.value.ids = ddsrt_calloc (1, sizeof (*plist->qos.data_representation.value.ids));
  plist->qos.data_representation.value.ids[0] = DDS_DATA_REPRESENTATION_XCDR2;
  plist->qos.liveliness.kind = DDS_LIVELINESS_AUTOMATIC;
  plist->qos.liveliness.lease_duration = DDS_INFINITY;

  struct ddsi_thread_state * const thrst = ddsi_lookup_thread_state ();
  ddsi_thread_state_awake (thrst, gv);
  struct ddsi_addrset *as = ddsi_new_addrset ();
  ddsi_add_locator_to_addrset (gv, as, &gv->loc_default_uc);
  ddsi_ref_addrset (as); // increase refc to 2, new_proxy_participant does not add a ref
  int rc = ddsi_new_proxy_participant (gv, pp_guid, 0, NULL, as, as, plist, DDS_INFINITY, DDSI_VENDORID_ECLIPSE, 0, ddsrt_time_wallclock (), 1);
  CU_ASSERT_FATAL (rc);

  ddsi_xqos_mergein_missing (&plist->qos, &ddsi_default_qos_reader, ~(uint64_t)0);
#ifdef DDS_HAS_SSM
  rc = ddsi_new_proxy_reader (gv, pp_guid, rd_guid, as, plist, ddsrt_time_wallclock (), 1, 0);
#else
  rc = ddsi_new_proxy_reader (gv, pp_guid, rd_guid, as, plist, ddsrt_time_wallclock (), 1);
#endif
  CU_ASSERT_EQUAL_FATAL (rc, exp_ret);
  ddsi_plist_fini (plist);
  ddsrt_free (plist);
  ddsi_thread_state_asleep (thrst);
}

static void test_proxy_rd_matches (dds_entity_t wr, bool exp_match)
{
  struct dds_entity *x;
  dds_return_t rc = dds_entity_pin (wr, &x);
  CU_ASSERT_EQUAL_FATAL (rc, DDS_RETCODE_OK);
  struct dds_writer *dds_wr = (struct dds_writer *) x;
  CU_ASSERT_EQUAL_FATAL (dds_wr->m_wr->num_readers, exp_match ? 1 : 0);
  dds_entity_unpin (x);
}

static void test_proxy_rd_fini (const ddsi_guid_t *pp_guid, const ddsi_guid_t *rd_guid)
{
  struct ddsi_domaingv *gv = get_domaingv (g_participant1);
  struct ddsi_thread_state * const thrst = ddsi_lookup_thread_state ();
  ddsi_thread_state_awake (thrst, gv);
  ddsi_delete_proxy_reader (gv, rd_guid, ddsrt_time_wallclock (), false);
  ddsi_delete_proxy_participant_by_guid (gv, pp_guid, ddsrt_time_wallclock (), false);
  ddsi_thread_state_asleep (thrst);
}

/* Invalid hashed type (with valid hash type id) as top-level type */
CU_Test (ddsc_xtypes, invalid_top_level_local_hash, .init = xtypes_init, .fini = xtypes_fini)
{
  char topic_name[100];
  dds_topic_descriptor_t desc;
  DDS_XTypes_TypeInformation *ti;

  for (uint32_t n = 0; n < 6; n++)
  {
    // coverity[store_writes_const_field]
    memcpy (&desc, &XSpace_to_toplevel_desc, sizeof (desc));
    typeinfo_deser (&ti, &desc.type_information);
    if (n % 2)
    {
      ddsi_typeid_fini_impl (&ti->minimal.typeid_with_size.type_id);
      ddsi_typeid_copy_impl (&ti->minimal.typeid_with_size.type_id, &ti->minimal.dependent_typeids._buffer[n / 2].type_id);
    }
    else
    {
      ddsi_typeid_fini_impl (&ti->complete.typeid_with_size.type_id);
      ddsi_typeid_copy_impl (&ti->complete.typeid_with_size.type_id, &ti->complete.dependent_typeids._buffer[n / 2].type_id);
    }
    typeinfo_ser (&desc.type_information, ti);

    create_unique_topic_name ("ddsc_xtypes", topic_name, sizeof (topic_name));
    dds_entity_t topic = dds_create_topic (g_participant1, &desc, topic_name, NULL, NULL);
    CU_ASSERT_FATAL (topic < 0);

    ddsi_typeinfo_fini ((ddsi_typeinfo_t *) ti);
    ddsrt_free (ti);
    ddsrt_free ((void *) desc.type_information.data);
  }
}

/* Non-hashed type (with valid hash type id) as top-level type */
CU_Test (ddsc_xtypes, invalid_top_level_local_non_hash, .init = xtypes_init, .fini = xtypes_fini)
{
  char topic_name[100];

  dds_topic_descriptor_t desc;
  // coverity[store_writes_const_field]
  memcpy (&desc, &XSpace_to_toplevel_desc, sizeof (desc));

  DDS_XTypes_TypeInformation *ti;
  typeinfo_deser (&ti, &desc.type_information);

  ddsi_typeid_fini_impl (&ti->minimal.typeid_with_size.type_id);
  ti->minimal.typeid_with_size.type_id._d = DDS_XTypes_TK_UINT32;
  typeinfo_ser (&desc.type_information, ti);

  create_unique_topic_name ("ddsc_xtypes", topic_name, sizeof (topic_name));
  dds_entity_t topic = dds_create_topic (g_participant1, &desc, topic_name, NULL, NULL);
  CU_ASSERT_FATAL (topic < 0);

  ddsi_typeinfo_fini ((ddsi_typeinfo_t *) ti);
  ddsrt_free (ti);
  ddsrt_free ((void *) desc.type_information.data);
}

static void mod_toplevel (dds_sequence_DDS_XTypes_TypeIdentifierTypeObjectPair *type_id_obj_seq, uint32_t kind)
{
  assert (kind == DDS_XTypes_EK_MINIMAL);
  (void) kind;
  assert (type_id_obj_seq->_buffer[0].type_object._u.minimal._d == DDS_XTypes_TK_STRUCTURE);
  type_id_obj_seq->_buffer[0].type_object._u.minimal._u.struct_type.member_seq._buffer[0].common.member_flags = 0x7f;
}

static void mod_inherit (dds_sequence_DDS_XTypes_TypeIdentifierTypeObjectPair *type_id_obj_seq, uint32_t kind)
{
  assert (kind == DDS_XTypes_EK_MINIMAL);
  (void) kind;
  assert (type_id_obj_seq->_buffer[0].type_object._u.minimal._d == DDS_XTypes_TK_STRUCTURE);
  ddsi_typeid_fini_impl (&type_id_obj_seq->_buffer[0].type_object._u.minimal._u.struct_type.header.base_type);
  ddsi_typeid_copy_impl (&type_id_obj_seq->_buffer[0].type_object._u.minimal._u.struct_type.header.base_type,
      &type_id_obj_seq->_buffer[0].type_object._u.minimal._u.struct_type.member_seq._buffer[0].common.member_type_id);
}

static void mod_uniondisc (dds_sequence_DDS_XTypes_TypeIdentifierTypeObjectPair *type_id_obj_seq, uint32_t kind)
{
  assert (kind == DDS_XTypes_EK_MINIMAL);
  (void) kind;
  assert (type_id_obj_seq->_buffer[0].type_object._u.minimal._d == DDS_XTypes_TK_UNION);
  type_id_obj_seq->_buffer[0].type_object._u.minimal._u.union_type.discriminator.common.type_id._d = DDS_XTypes_TK_FLOAT32;
}

static void mod_unionmembers (dds_sequence_DDS_XTypes_TypeIdentifierTypeObjectPair *type_id_obj_seq, uint32_t kind)
{
  assert (kind == DDS_XTypes_EK_MINIMAL);
  (void) kind;
  assert (type_id_obj_seq->_buffer[0].type_object._u.minimal._d == DDS_XTypes_TK_UNION);
  assert (type_id_obj_seq->_buffer[0].type_object._u.minimal._u.union_type.member_seq._length == 2);
  type_id_obj_seq->_buffer[0].type_object._u.minimal._u.union_type.member_seq._buffer[0].common.member_flags |= DDS_XTypes_IS_DEFAULT;
}

static void mod_arraybound (dds_sequence_DDS_XTypes_TypeIdentifierTypeObjectPair *type_id_obj_seq, uint32_t kind)
{
  assert (kind == DDS_XTypes_EK_MINIMAL);
  (void) kind;
  assert (type_id_obj_seq->_buffer[0].type_object._u.minimal._d == DDS_XTypes_TK_STRUCTURE);
  assert (type_id_obj_seq->_buffer[0].type_object._u.minimal._u.struct_type.member_seq._buffer[0].common.member_type_id._d == DDS_XTypes_TI_PLAIN_ARRAY_SMALL);
  type_id_obj_seq->_buffer[0].type_object._u.minimal._u.struct_type.member_seq._buffer[0].common.member_type_id._u.array_sdefn.array_bound_seq._buffer[0] = 5;
}

static void modify_type_meta (dds_topic_descriptor_t *dst_desc, const dds_topic_descriptor_t *src_desc, typeobj_modify mod, bool update_typeinfo, uint32_t kind)
{
  // coverity[store_writes_const_field]
  memcpy (dst_desc, src_desc, sizeof (*dst_desc));

  DDS_XTypes_TypeInformation *ti = NULL;
  if (update_typeinfo)
    typeinfo_deser (&ti, &dst_desc->type_information);

  DDS_XTypes_TypeMapping *tmap = NULL;
  typemap_deser (&tmap, &dst_desc->type_mapping);

  if (update_typeinfo)
  {
    assert (ti);
    // confirm that top-level type is the first in type map
    if (kind == DDS_XTypes_EK_MINIMAL || kind == DDS_XTypes_EK_BOTH)
    {
      assert (!ddsi_typeid_compare_impl (&ti->minimal.typeid_with_size.type_id, &tmap->identifier_object_pair_minimal._buffer[0].type_identifier));
      ddsi_typeid_fini_impl (&ti->minimal.typeid_with_size.type_id);
      for (uint32_t n = 0; n < tmap->identifier_object_pair_minimal._length; n++)
        ddsi_typeid_fini_impl (&tmap->identifier_object_pair_minimal._buffer[n].type_identifier);
    }
    if (kind == DDS_XTypes_EK_COMPLETE || kind == DDS_XTypes_EK_BOTH)
    {
      assert (!ddsi_typeid_compare_impl (&ti->complete.typeid_with_size.type_id, &tmap->identifier_object_pair_complete._buffer[0].type_identifier));
      ddsi_typeid_fini_impl (&ti->complete.typeid_with_size.type_id);
      for (uint32_t n = 0; n < tmap->identifier_object_pair_complete._length; n++)
        ddsi_typeid_fini_impl (&tmap->identifier_object_pair_complete._buffer[n].type_identifier);
    }
  }

  // modify the specified object in the type mapping
  if (kind == DDS_XTypes_EK_MINIMAL || kind == DDS_XTypes_EK_BOTH)
    mod (&tmap->identifier_object_pair_minimal, DDS_XTypes_EK_MINIMAL);
  if (kind == DDS_XTypes_EK_COMPLETE || kind == DDS_XTypes_EK_BOTH)
    mod (&tmap->identifier_object_pair_complete, DDS_XTypes_EK_COMPLETE);

  if (update_typeinfo)
  {
    // get hash-id for modified type and store in type map and replace top-level type id
    if (kind == DDS_XTypes_EK_MINIMAL || kind == DDS_XTypes_EK_BOTH)
    {
      for (uint32_t n = 0; n < tmap->identifier_object_pair_minimal._length; n++)
      {
        ddsi_typeid_t type_id;
        ddsi_typeobj_get_hash_id (&tmap->identifier_object_pair_minimal._buffer[n].type_object, &type_id);
        ddsi_typeid_copy_impl (&tmap->identifier_object_pair_minimal._buffer[n].type_identifier, &type_id.x);
        ddsi_typeid_fini (&type_id);
      }
      ddsi_typeid_copy_impl (&ti->minimal.typeid_with_size.type_id, &tmap->identifier_object_pair_minimal._buffer[0].type_identifier);
    }
    if (kind == DDS_XTypes_EK_COMPLETE || kind == DDS_XTypes_EK_BOTH)
    {
      for (uint32_t n = 0; n < tmap->identifier_object_pair_complete._length; n++)
      {
        ddsi_typeid_t type_id;
        ddsi_typeobj_get_hash_id (&tmap->identifier_object_pair_complete._buffer[n].type_object, &type_id);
        ddsi_typeid_copy_impl (&tmap->identifier_object_pair_complete._buffer[n].type_identifier, &type_id.x);
        ddsi_typeid_fini (&type_id);
      }
      ddsi_typeid_copy_impl (&ti->complete.typeid_with_size.type_id, &tmap->identifier_object_pair_complete._buffer[0].type_identifier);
    }
  }

  // replace the type map and type info in the topic descriptor with updated ones
  if (update_typeinfo)
    typeinfo_ser (&dst_desc->type_information, ti);
  typemap_ser (&dst_desc->type_mapping, tmap);

  // clean up
  ddsi_typemap_fini ((ddsi_typemap_t *) tmap);
  ddsrt_free (tmap);

  if (update_typeinfo)
  {
    ddsi_typeinfo_fini ((ddsi_typeinfo_t *) ti);
    ddsrt_free (ti);
  }
}

#define D(n) XSpace_ ## n ## _desc
CU_TheoryDataPoints (ddsc_xtypes, invalid_type_object_local) = {
  CU_DataPoints (const char *,                    "invalid flag, non-matching typeid",
  /*                                              |               */"invalid flag, matching typeid",
  /*                                              |                |               */"invalid inheritance",
  /*                                              |                |                |              */"invalid union discr",
  /*                                              |                |                |               |                */"union multiple default",
  /*                                              |                |                |               |                 |                   */"array bound overflow"),
  CU_DataPoints (const dds_topic_descriptor_t *,  &D(to_toplevel), &D(to_toplevel), &D(to_inherit), &D(to_uniondisc), &D(to_unionmembers), &D(to_arraybound) ),
  CU_DataPoints (typeobj_modify,                  mod_toplevel,    mod_toplevel,    mod_inherit,    mod_uniondisc,    mod_unionmembers,    mod_arraybound    ),
  CU_DataPoints (bool,                            false,           true,            true,           true,             true,                true              ),
};
#undef D

CU_Theory ((const char *test_descr, const dds_topic_descriptor_t *topic_desc, typeobj_modify mod, bool matching_typeinfo), ddsc_xtypes, invalid_type_object_local, .init = xtypes_init, .fini = xtypes_fini)
{
  char topic_name[100];
  printf("Test invalid_type_object_local: %s\n", test_descr);

  dds_topic_descriptor_t desc;
  modify_type_meta (&desc, topic_desc, mod, matching_typeinfo, DDS_XTypes_EK_MINIMAL);

  // test that topic creation fails
  create_unique_topic_name ("ddsc_xtypes", topic_name, sizeof (topic_name));
  dds_entity_t topic = dds_create_topic (g_participant1, &desc, topic_name, NULL, NULL);
  CU_ASSERT_FATAL (topic < 0);

  if (matching_typeinfo)
    ddsrt_free ((void *) desc.type_information.data);
  ddsrt_free ((void *) desc.type_mapping.data);
}

/* Invalid hashed type (with valid hash type id) as top-level type for proxy endpoint */
CU_Test (ddsc_xtypes, invalid_top_level_remote_hash, .init = xtypes_init, .fini = xtypes_fini)
{
  dds_topic_descriptor_t desc;
  DDS_XTypes_TypeInformation *ti;
  struct ddsi_domaingv *gv = get_domaingv (g_participant1);
  char topic_name[100];
  create_unique_topic_name ("ddsc_xtypes", topic_name, sizeof (topic_name));

  // create local topic so that types are in type lib and resolved
  dds_entity_t topic = dds_create_topic (g_participant1, &XSpace_to_toplevel_desc, topic_name, NULL, NULL);
  CU_ASSERT_FATAL (topic > 0);

  // create type id with invalid top-level
  // coverity[store_writes_const_field]
  memcpy (&desc, &XSpace_to_toplevel_desc, sizeof (desc));
  typeinfo_deser (&ti, &desc.type_information);
  ddsi_typeid_fini_impl (&ti->minimal.typeid_with_size.type_id);
  ddsi_typeid_copy_impl (&ti->minimal.typeid_with_size.type_id, &ti->minimal.dependent_typeids._buffer[0].type_id);

  // create proxy reader with modified type
  struct ddsi_guid pp_guid, rd_guid;
  gen_test_guid (gv, &pp_guid, DDSI_ENTITYID_PARTICIPANT);
  gen_test_guid (gv, &rd_guid, DDSI_ENTITYID_KIND_READER_NO_KEY);
  test_proxy_rd_create (gv, topic_name, ti, DDS_RETCODE_BAD_PARAMETER, &pp_guid, &rd_guid);

  // clean up
  test_proxy_rd_fini (&pp_guid, &rd_guid);
  ddsi_typeinfo_fini ((ddsi_typeinfo_t *) ti);
  ddsrt_free (ti);
}


/* Invalid type object for proxy endpoint */
#define D(n) XSpace_ ## n ## _desc
CU_TheoryDataPoints (ddsc_xtypes, invalid_type_object_remote) = {
  CU_DataPoints (const char *,
  /*                                             */"invalid flag",
  /*                                              |               */"invalid inheritance",
  /*                                              |                |              */"invalid union discr"),
  CU_DataPoints (const dds_topic_descriptor_t *,  &D(to_toplevel), &D(to_inherit), &D(to_uniondisc) ),
  CU_DataPoints (typeobj_modify,                  mod_toplevel,    mod_inherit,    mod_uniondisc    )
};
#undef D

CU_Theory ((const char *test_descr, const dds_topic_descriptor_t *topic_desc, typeobj_modify mod), ddsc_xtypes, invalid_type_object_remote, .init = xtypes_init, .fini = xtypes_fini)
{
  struct ddsi_domaingv *gv = get_domaingv (g_participant1);
  printf("Test invalid_type_object_remote: %s\n", test_descr);

  char topic_name[100];
  create_unique_topic_name ("ddsc_xtypes", topic_name, sizeof (topic_name));

  // local writer
  dds_entity_t topic = dds_create_topic (g_participant1, topic_desc, topic_name, NULL, NULL);
  CU_ASSERT_FATAL (topic > 0);
  dds_entity_t wr = dds_create_writer (g_participant1, topic, NULL, NULL);
  CU_ASSERT_FATAL (wr > 0);

  dds_topic_descriptor_t desc;
  modify_type_meta (&desc, topic_desc, mod, true, DDS_XTypes_EK_MINIMAL);

  DDS_XTypes_TypeInformation *ti;
  typeinfo_deser (&ti, &desc.type_information);
  struct ddsi_guid pp_guid, rd_guid;
  gen_test_guid (gv, &pp_guid, DDSI_ENTITYID_PARTICIPANT);
  gen_test_guid (gv, &rd_guid, DDSI_ENTITYID_KIND_READER_NO_KEY);
  test_proxy_rd_create (gv, topic_name, ti, DDS_RETCODE_OK, &pp_guid, &rd_guid);
  test_proxy_rd_matches (wr, false);

  struct ddsi_generic_proxy_endpoint **gpe_match_upd = NULL;
  uint32_t n_match_upd = 0;

  DDS_XTypes_TypeMapping *tmap;
  typemap_deser (&tmap, &desc.type_mapping);
  DDS_Builtin_TypeLookup_Reply reply = {
    .header = { .remoteEx = DDS_RPC_REMOTE_EX_OK, .relatedRequestId = { .sequence_number = { .low = 1, .high = 0 }, .writer_guid = { .guidPrefix = { 0 }, .entityId = { .entityKind = DDSI_EK_WRITER, .entityKey = { 0 } } } } },
    .return_data = { ._d = DDS_Builtin_TypeLookup_getTypes_HashId, ._u = { .getType = { ._d = DDS_RETCODE_OK, ._u = { .result =
      { .types = { ._length = tmap->identifier_object_pair_minimal._length, ._maximum = tmap->identifier_object_pair_minimal._maximum, ._release = false, ._buffer = tmap->identifier_object_pair_minimal._buffer } } } } } }
    };
  ddsi_tl_add_types (gv, &reply, &gpe_match_upd, &n_match_upd);

  // expect no match because of invalid types
  CU_ASSERT_EQUAL_FATAL (n_match_upd, 0);
  ddsrt_free (gpe_match_upd);

  struct ddsi_type *type = ddsi_type_lookup (gv, (ddsi_typeid_t *) &tmap->identifier_object_pair_minimal._buffer[0].type_identifier);
  CU_ASSERT_PTR_NOT_NULL_FATAL (type);
  assert (type);
  CU_ASSERT_EQUAL_FATAL (type->state, DDSI_TYPE_INVALID);

  // clean up
  test_proxy_rd_fini (&pp_guid, &rd_guid);
  ddsi_typeinfo_fini ((ddsi_typeinfo_t *) ti);
  ddsrt_free (ti);
  ddsi_typemap_fini ((ddsi_typemap_t *) tmap);
  ddsrt_free (tmap);
  ddsrt_free ((void *) desc.type_information.data);
  ddsrt_free ((void *) desc.type_mapping.data);
}

static void mod_dep_test (dds_sequence_DDS_XTypes_TypeIdentifierTypeObjectPair *type_id_obj_seq, uint32_t kind)
{
  // Remove member n2 from dep_test_nested
  if (kind == DDS_XTypes_EK_MINIMAL)
  {
    assert (type_id_obj_seq->_buffer[1].type_object._u.minimal._d == DDS_XTypes_TK_STRUCTURE);
    assert (type_id_obj_seq->_buffer[1].type_object._u.minimal._u.struct_type.member_seq._length == 2);
    type_id_obj_seq->_buffer[1].type_object._u.minimal._u.struct_type.member_seq._length = 1;
  }
  else
  {
    assert (type_id_obj_seq->_buffer[1].type_object._u.complete._d == DDS_XTypes_TK_STRUCTURE);
    assert (type_id_obj_seq->_buffer[1].type_object._u.complete._u.struct_type.member_seq._length == 2);
    type_id_obj_seq->_buffer[1].type_object._u.complete._u.struct_type.member_seq._length = 1;
  }

  // Recalculate type ids for dep_test_nested and replace dep_test.f1 member type id
  ddsi_typeid_t type_id;
  if (kind == DDS_XTypes_EK_MINIMAL)
  {
    assert (type_id_obj_seq->_buffer[0].type_object._u.minimal._d == DDS_XTypes_TK_STRUCTURE);
    ddsi_typeid_fini_impl (&type_id_obj_seq->_buffer[0].type_object._u.minimal._u.struct_type.member_seq._buffer[0].common.member_type_id);
    ddsi_typeobj_get_hash_id (&type_id_obj_seq->_buffer[1].type_object, &type_id);
    ddsi_typeid_copy_impl (&type_id_obj_seq->_buffer[0].type_object._u.minimal._u.struct_type.member_seq._buffer[0].common.member_type_id, &type_id.x);
  }
  else
  {
    assert (type_id_obj_seq->_buffer[0].type_object._u.complete._d == DDS_XTypes_TK_STRUCTURE);
    ddsi_typeid_fini_impl (&type_id_obj_seq->_buffer[0].type_object._u.complete._u.struct_type.member_seq._buffer[0].common.member_type_id);
    ddsi_typeobj_get_hash_id (&type_id_obj_seq->_buffer[1].type_object, &type_id);
    ddsi_typeid_copy_impl (&type_id_obj_seq->_buffer[0].type_object._u.complete._u.struct_type.member_seq._buffer[0].common.member_type_id, &type_id.x);
  }
}

CU_Test (ddsc_xtypes, resolve_dep_type, .init = xtypes_init, .fini = xtypes_fini)
{
  struct ddsi_domaingv *gv = get_domaingv (g_participant1);
  char topic_name[100];
  create_unique_topic_name ("ddsc_xtypes", topic_name, sizeof (topic_name));

  // local writer
  dds_entity_t topic = dds_create_topic (g_participant1, &XSpace_dep_test_desc, topic_name, NULL, NULL);
  CU_ASSERT_FATAL (topic > 0);
  dds_entity_t wr = dds_create_writer (g_participant1, topic, NULL, NULL);
  CU_ASSERT_FATAL (wr > 0);

  dds_topic_descriptor_t desc;
  modify_type_meta (&desc, &XSpace_dep_test_desc, mod_dep_test, true, DDS_XTypes_EK_BOTH);

  DDS_XTypes_TypeInformation *ti;
  typeinfo_deser (&ti, &desc.type_information);
  struct ddsi_guid pp_guid, rd_guid;
  gen_test_guid (gv, &pp_guid, DDSI_ENTITYID_PARTICIPANT);
  gen_test_guid (gv, &rd_guid, DDSI_ENTITYID_KIND_READER_NO_KEY);
  test_proxy_rd_create (gv, topic_name, ti, DDS_RETCODE_OK, &pp_guid, &rd_guid);
  test_proxy_rd_matches (wr, false);

  struct ddsi_generic_proxy_endpoint **gpe_match_upd = NULL;
  uint32_t n_match_upd = 0;
  DDS_Builtin_TypeLookup_Reply reply;
  DDS_XTypes_TypeMapping *tmap;
  typemap_deser (&tmap, &desc.type_mapping);

  // add proxy reader's top-level type
  reply = (DDS_Builtin_TypeLookup_Reply) {
    .header = { .remoteEx = DDS_RPC_REMOTE_EX_OK, .relatedRequestId = { .sequence_number = { .low = 1, .high = 0 }, .writer_guid = { .guidPrefix = { 0 }, .entityId = { .entityKind = DDSI_EK_WRITER, .entityKey = { 0 } } } } },
    .return_data = { ._d = DDS_Builtin_TypeLookup_getTypes_HashId, ._u = { .getType = { ._d = DDS_RETCODE_OK, ._u = { .result =
      { .types = { ._length = 1, ._maximum = 1, ._release = false, ._buffer = &tmap->identifier_object_pair_minimal._buffer[0] } } } } } }
    };
  ddsi_tl_add_types (gv, &reply, &gpe_match_upd, &n_match_upd);
  // FIXME expect matching triggered (but matching would fail because deps are unresolved), this needs to be fixed in ddsi_tl_handle_reply
  CU_ASSERT_EQUAL_FATAL (n_match_upd, 1);
  ddsrt_free (gpe_match_upd);

  // add nested type and expect match
  gpe_match_upd = NULL;
  n_match_upd = 0;
  reply = (DDS_Builtin_TypeLookup_Reply) {
    .header = { .remoteEx = DDS_RPC_REMOTE_EX_OK, .relatedRequestId = { .sequence_number = { .low = 1, .high = 0 }, .writer_guid = { .guidPrefix = { 0 }, .entityId = { .entityKind = DDSI_EK_WRITER, .entityKey = { 0 } } } } },
    .return_data = { ._d = DDS_Builtin_TypeLookup_getTypes_HashId, ._u = { .getType = { ._d = DDS_RETCODE_OK, ._u = { .result =
      { .types = { ._length = 1, ._maximum = 1, ._release = false, ._buffer = &tmap->identifier_object_pair_minimal._buffer[1] } } } } } }
    };
  ddsi_tl_add_types (gv, &reply, &gpe_match_upd, &n_match_upd);
  CU_ASSERT_EQUAL_FATAL (n_match_upd, 1);

  assert (gpe_match_upd);
  for (uint32_t e = 0; e < n_match_upd; e++)
    ddsi_update_proxy_endpoint_matching (gv, gpe_match_upd[e]);
  ddsrt_free (gpe_match_upd);

  test_proxy_rd_matches (wr, true);

  // clean up
  test_proxy_rd_fini (&pp_guid, &rd_guid);
  ddsi_typeinfo_fini ((ddsi_typeinfo_t *) ti);
  ddsrt_free (ti);
  ddsi_typemap_fini ((ddsi_typemap_t *) tmap);
  ddsrt_free (tmap);
  ddsrt_free ((void *) desc.type_information.data);
  ddsrt_free ((void *) desc.type_mapping.data);
}

CU_Test (ddsc_xtypes, get_type_info, .init = xtypes_init, .fini = xtypes_fini)
{
  char topic_name[100];
  create_unique_topic_name ("ddsc_xtypes", topic_name, sizeof (topic_name));

  dds_entity_t topic = dds_create_topic (g_participant1, &XSpace_XType1_desc, topic_name, NULL, NULL);
  CU_ASSERT_FATAL (topic > 0);
  dds_entity_t wr = dds_create_writer (g_participant1, topic, NULL, NULL);
  CU_ASSERT_FATAL (wr > 0);
  dds_entity_t rd = dds_create_reader (g_participant1, topic, NULL, NULL);
  CU_ASSERT_FATAL (rd > 0);

  dds_typeinfo_t *type_info_tp, *type_info_wr, *type_info_rd;
  dds_return_t ret;
  ret = dds_get_typeinfo (topic, &type_info_tp);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);
  ret = dds_get_typeinfo (wr, &type_info_wr);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);
  ret = dds_get_typeinfo (rd, &type_info_rd);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);
  CU_ASSERT_FATAL (ddsi_typeinfo_equal (type_info_tp, type_info_wr, DDSI_TYPE_INCLUDE_DEPS));
  CU_ASSERT_FATAL (ddsi_typeinfo_equal (type_info_tp, type_info_rd, DDSI_TYPE_INCLUDE_DEPS));

  dds_free_typeinfo (type_info_tp);
  dds_free_typeinfo (type_info_wr);
  dds_free_typeinfo (type_info_rd);
}
