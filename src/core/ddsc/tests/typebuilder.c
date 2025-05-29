// Copyright(c) 2006 to 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <assert.h>

#include "dds/dds.h"
#include "config_env.h"

#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsi/ddsi_typelib.h"
#include "dds/ddsi/ddsi_typebuilder.h"
#include "ddsi__xt_impl.h"
#include "dds__types.h"
#include "dds__topic.h"
#include "TypeBuilderTypes.h"
#include "CUnit/Test.h"
#include "test_common.h"

static dds_entity_t g_participant = 0;

static void typebuilder_init (void)
{
  g_participant = dds_create_participant (0, NULL, NULL);
  CU_ASSERT_FATAL (g_participant > 0);
}

static void typebuilder_fini (void)
{
  dds_delete (g_participant);
}

static void topic_type_ref (dds_entity_t topic, struct ddsi_type **type)
{
  dds_topic *t;
  dds_return_t ret = dds_topic_pin (topic, &t);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);
  struct ddsi_sertype *sertype = t->m_stype;
  ret = ddsi_type_ref_local (&t->m_entity.m_domain->gv, type, sertype, DDSI_TYPEID_KIND_COMPLETE);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);
  CU_ASSERT_FATAL (type != NULL);
  CU_ASSERT_FATAL (*type != NULL);
  dds_topic_unpin (t);
}

static void topic_type_unref (dds_entity_t topic, struct ddsi_type *type)
{
  dds_topic *t;
  dds_return_t ret = dds_topic_pin (topic, &t);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);
  ddsi_type_unref (&t->m_entity.m_domain->gv, type);
  dds_topic_unpin (t);
}

static struct ddsi_domaingv *gv_from_topic (dds_entity_t topic)
{
  dds_topic *t;
  dds_return_t ret = dds_topic_pin (topic, &t);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);
  struct ddsi_domaingv *gv = &t->m_entity.m_domain->gv;
  dds_topic_unpin (t);
  return gv;
}

static bool ti_to_pairs_equal (dds_sequence_DDS_XTypes_TypeIdentifierTypeObjectPair *a, dds_sequence_DDS_XTypes_TypeIdentifierTypeObjectPair *b)
{
  if (a->_length != b->_length)
    return false;
  for (uint32_t n = 0; n < a->_length; n++)
  {
    struct DDS_XTypes_TypeObject *to_b = NULL;
    uint32_t m;
    for (m = 0; !to_b && m < b->_length; m++)
    {
      if (!ddsi_typeid_compare_impl (&a->_buffer[n].type_identifier, &b->_buffer[m].type_identifier))
        to_b = &b->_buffer[m].type_object;
    }
    if (!to_b)
      return false;

    dds_ostreamLE_t to_a_ser = { .x = { NULL, 0, 0, DDSI_RTPS_CDR_ENC_VERSION_2 } };
    xcdr2_ser (&a->_buffer[n].type_object, &DDS_XTypes_TypeObject_desc, &to_a_ser);
    dds_ostreamLE_t to_b_ser = { .x = { NULL, 0, 0, DDSI_RTPS_CDR_ENC_VERSION_2 } };
    xcdr2_ser (to_b, &DDS_XTypes_TypeObject_desc, &to_b_ser);

    if (to_a_ser.x.m_index != to_b_ser.x.m_index)
      return false;
    if (memcmp (to_a_ser.x.m_buffer, to_b_ser.x.m_buffer, to_a_ser.x.m_index))
      return false;

    dds_ostreamLE_fini (&to_a_ser, &dds_cdrstream_default_allocator);
    dds_ostreamLE_fini (&to_b_ser, &dds_cdrstream_default_allocator);
  }
  return true;
}

static bool ti_pairs_equal (dds_sequence_DDS_XTypes_TypeIdentifierPair *a, dds_sequence_DDS_XTypes_TypeIdentifierPair *b)
{
    if (a->_length != b->_length)
    return false;
  for (uint32_t n = 0; n < a->_length; n++)
  {
    bool found = false;
    for (uint32_t m = 0; !found && m < b->_length; m++)
    {
      if (!ddsi_typeid_compare_impl (&a->_buffer[n].type_identifier1, &b->_buffer[m].type_identifier1))
      {
        if (ddsi_typeid_compare_impl (&a->_buffer[n].type_identifier2, &b->_buffer[m].type_identifier2))
          return false;
        found = true;
      }
    }
    if (!found)
      return false;
  }
  return true;
}

static bool tmap_equal (ddsi_typemap_t *a, ddsi_typemap_t *b)
{
  return ti_to_pairs_equal (&a->x.identifier_object_pair_minimal, &b->x.identifier_object_pair_minimal)
      && ti_to_pairs_equal (&a->x.identifier_object_pair_complete, &b->x.identifier_object_pair_complete)
      && ti_pairs_equal (&a->x.identifier_complete_minimal, &b->x.identifier_complete_minimal);
}

#define D(n) TypeBuilderTypes_ ## n ## _desc
CU_TheoryDataPoints (ddsc_typebuilder, topic_desc) = {
  CU_DataPoints (const dds_topic_descriptor_t *, &D(t1), &D(t2), &D(t3), &D(t4), &D(t5), &D(t6), &D(t7), &D(t8),
                                                 &D(t9), &D(t10), &D(t11), &D(t12), &D(t13), &D(t14), &D(t15), &D(t16),
                                                 &D(t17), &D(t18), &D(t19), &D(t20), &D(t21), &D(t22), &D(t23), &D(t24),
                                                 &D(t25), &D(t26), &D(t27), &D(t28), &D(t29), &D(t30), &D(t31), &D(t32),
                                                 &D(t33), &D(t34), &D(t35), &D(t36), &D(t37), &D(t38), /* TODO &D(t39), */
                                                 &D(t40), &D(t41), &D(t42), &D(t43), &D(t44), &D(t45), &D(t46), &D(t47),
                                                 &D(t48), &D(t49) ),
};
#undef D

CU_Theory((const dds_topic_descriptor_t *desc), ddsc_typebuilder, topic_desc, .init = typebuilder_init, .fini = typebuilder_fini)
{
  char topic_name[100];
  dds_return_t ret;
  dds_entity_t topic;
  struct ddsi_type *type;
  dds_topic_descriptor_t *generated_desc;

  printf ("Testing %s\n", desc->m_typename);

  create_unique_topic_name ("ddsc_typebuilder", topic_name, sizeof (topic_name));
  topic = dds_create_topic (g_participant, desc, topic_name, NULL, NULL);
  CU_ASSERT_FATAL (topic > 0);

  // generate a topic descriptor
  topic_type_ref (topic, &type);
  generated_desc = dds_alloc (sizeof (*generated_desc));
  ret = ddsi_topic_descriptor_from_type (gv_from_topic (topic), generated_desc, type);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);

  // check
  printf ("size: %u (%u)\n", generated_desc->m_size, desc->m_size);
  CU_ASSERT_EQUAL_FATAL (desc->m_size, generated_desc->m_size);
  printf ("align: %u (%u)\n", generated_desc->m_align, desc->m_align);
  CU_ASSERT_EQUAL_FATAL (desc->m_align, generated_desc->m_align);
  printf ("flagset: %x (%x)\n", generated_desc->m_flagset, desc->m_flagset);
  CU_ASSERT_EQUAL_FATAL (desc->m_flagset, generated_desc->m_flagset);
  printf ("nkeys: %u (%u)\n", generated_desc->m_nkeys, desc->m_nkeys);
  CU_ASSERT_EQUAL_FATAL (desc->m_nkeys, generated_desc->m_nkeys);
  for (uint32_t n = 0; n < desc->m_nkeys; n++)
  {
    printf("key[%u] name: %s (%s)\n", n, generated_desc->m_keys[n].m_name, desc->m_keys[n].m_name);
    CU_ASSERT_EQUAL_FATAL (strcmp (desc->m_keys[n].m_name, generated_desc->m_keys[n].m_name), 0);
    printf("  offset: %u (%u)\n", generated_desc->m_keys[n].m_offset, desc->m_keys[n].m_offset);
    CU_ASSERT_EQUAL_FATAL (desc->m_keys[n].m_offset, generated_desc->m_keys[n].m_offset);
    printf("  index: %u (%u)\n", generated_desc->m_keys[n].m_idx, desc->m_keys[n].m_idx);
    CU_ASSERT_EQUAL_FATAL (desc->m_keys[n].m_idx, generated_desc->m_keys[n].m_idx);
  }
  printf ("typename: %s (%s)\n", generated_desc->m_typename, desc->m_typename);
  CU_ASSERT_EQUAL_FATAL (strcmp (desc->m_typename, generated_desc->m_typename), 0);
  printf ("nops: %u (%u)\n", generated_desc->m_nops, desc->m_nops);
  CU_ASSERT_EQUAL_FATAL (desc->m_nops, generated_desc->m_nops);

  uint32_t ops_cnt_gen = dds_stream_countops (generated_desc->m_ops, generated_desc->m_nkeys, generated_desc->m_keys);
  uint32_t ops_cnt = dds_stream_countops (desc->m_ops, desc->m_nkeys, desc->m_keys);
  printf ("ops count: %u (%u)\n", ops_cnt_gen, ops_cnt);
  CU_ASSERT_EQUAL_FATAL (ops_cnt_gen, ops_cnt);
  for (uint32_t n = 0; n < ops_cnt; n++)
  {
    if (desc->m_ops[n] != generated_desc->m_ops[n])
    {
      printf ("incorrect op at index %u: 0x%08x (0x%08x)\n", n, generated_desc->m_ops[n], desc->m_ops[n]);
      CU_FAIL_FATAL ("different ops");
    }
  }

  printf ("typeinfo: %u (%u)\n", generated_desc->type_information.sz, desc->type_information.sz);
  ddsi_typeinfo_t *tinfo = ddsi_typeinfo_deser (desc->type_information.data, desc->type_information.sz);
  ddsi_typeinfo_t *gen_tinfo = ddsi_typeinfo_deser (generated_desc->type_information.data, generated_desc->type_information.sz);
  CU_ASSERT_FATAL (ddsi_typeinfo_equal (tinfo, gen_tinfo, DDSI_TYPE_INCLUDE_DEPS));
  ddsi_typeinfo_fini (tinfo);
  ddsrt_free (tinfo);
  ddsi_typeinfo_fini (gen_tinfo);
  ddsrt_free (gen_tinfo);

  printf ("typemap: %u (%u)\n", generated_desc->type_mapping.sz, desc->type_mapping.sz);
  ddsi_typemap_t *tmap = ddsi_typemap_deser (desc->type_mapping.data, desc->type_mapping.sz);
  ddsi_typemap_t *gen_tmap = ddsi_typemap_deser (generated_desc->type_mapping.data, generated_desc->type_mapping.sz);
  CU_ASSERT_FATAL (tmap_equal (tmap, gen_tmap));
  ddsi_typemap_fini (tmap);
  ddsrt_free (tmap);
  ddsi_typemap_fini (gen_tmap);
  ddsrt_free (gen_tmap);

  // we don't check restrict_data_representation, this information is not in the type meta-data

  // cleanup
  ddsi_topic_descriptor_fini (generated_desc);
  ddsrt_free (generated_desc);
  topic_type_unref (topic, type);
  printf ("\n");
}

CU_Test(ddsc_typebuilder, invalid_toplevel, .init = typebuilder_init, .fini = typebuilder_fini)
{
  char topic_name[100];
  dds_return_t ret;
  dds_entity_t topic;
  struct ddsi_type *type;
  dds_topic_descriptor_t *generated_desc;

  create_unique_topic_name ("ddsc_typebuilder", topic_name, sizeof (topic_name));
  topic = dds_create_topic (g_participant, &TypeBuilderTypes_t2_desc, topic_name, NULL, NULL);
  CU_ASSERT_FATAL (topic > 0);

  // generate a topic descriptor
  topic_type_ref (topic, &type);
  generated_desc = dds_alloc (sizeof (*generated_desc));
  assert (generated_desc);
  for (uint32_t n = 0; n < type->xt._u.structure.members.length; n++)
  {
    ret = ddsi_topic_descriptor_from_type (gv_from_topic (topic), generated_desc, type->xt._u.structure.members.seq[n].type);
    CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_BAD_PARAMETER);
  }

  // cleanup
  ddsrt_free (generated_desc);
  topic_type_unref (topic, type);
}

CU_Test(ddsc_typebuilder, alias_toplevel, .init = typebuilder_init, .fini = typebuilder_fini)
{
  char topic_name[100];
  dds_return_t ret;
  dds_entity_t topic;
  struct ddsi_type *type;
  dds_topic_descriptor_t *generated_desc;

  create_unique_topic_name ("ddsc_typebuilder", topic_name, sizeof (topic_name));
  topic = dds_create_topic (g_participant, &TypeBuilderTypes_t48_desc, topic_name, NULL, NULL);
  CU_ASSERT_FATAL (topic > 0);

  // generate a topic descriptor
  topic_type_ref (topic, &type);
  generated_desc = dds_alloc (sizeof (*generated_desc));
  assert (generated_desc);
  assert (type->xt._u.structure.members.length == 1);
  assert (type->xt._u.structure.members.seq[0].type->xt._d == DDS_XTypes_TK_ALIAS);
  ret = ddsi_topic_descriptor_from_type (gv_from_topic (topic), generated_desc, type->xt._u.structure.members.seq[0].type);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);

  // should be able to create a topic
  char topic_name2[100];
  create_unique_topic_name ("ddsc_typebuilder", topic_name2, sizeof (topic_name2));
  const dds_entity_t topic2 = dds_create_topic (g_participant, generated_desc, topic_name2, NULL, NULL);
  CU_ASSERT_FATAL (topic2 > 0);

  // verify its type really is the alias
  struct ddsi_type *type2;
  topic_type_ref (topic2, &type2);
  CU_ASSERT_EQUAL (type2->xt._d, DDS_XTypes_TK_ALIAS);
  topic_type_unref (topic2, type2);

#if 0
  const dds_entity_t wr = dds_create_writer (g_participant, topic2, NULL, NULL);
  CU_ASSERT_FATAL (wr > 0);
  while (true)
  {
    dds_write (wr, &(TypeBuilderTypes_t48){ .t1 = { .n1 = 33 } });
    dds_sleepfor (DDS_SECS (1));
  }
#endif

  // cleanup
  ddsi_topic_descriptor_fini (generated_desc);
  ddsrt_free (generated_desc);
  topic_type_unref (topic, type);
}
